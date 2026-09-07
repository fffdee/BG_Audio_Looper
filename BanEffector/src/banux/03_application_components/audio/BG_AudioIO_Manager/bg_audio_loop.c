/**
 * @file bg_audio_loop.c
 * @brief Main audio loop and Effect-Graph frame scheduler.
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_internal.h"
#include "product_def.h"
#include "debug.h"

#include "gpio.h"
#include "app_config.h"
#include "adc.h"

#include "FreeRTOS.h"
#include "task.h"
#include "audio_adc.h"
#include "dac_interface.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_cdc.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "bg_event.h"
#include "bg_low_power.h"
#include "effect_graph.h"
#include "sys_param.h"
#include "bg_shell.h"
#include "shell_io_manager.h"
#include "dac.h"
#include "bt_stack_service.h"

/* 音量旋钮采样降频与滞回门限
 *
 * 原实现每轮主循环都执行：GPIO 翻转 + 阻塞式 ADC 采样 + 2 次 DAC 寄存器写。
 * Looper 工作时主循环被压到 48 采样(1ms)小帧，调度频率很高，这部分固定
 * 开销占比可观；且每帧 GPIO 翻转会形成约 1kHz 方波，可能耦合到音频 ADC
 * 造成底噪（与检测 GPIO 的 50 帧降频同理）。
 *
 * 优化：
 *   1) 每 SETVOLUME_SAMPLE_DIV 轮才真正采样一次
 *   2) 采样值变化超过 SETVOLUME_DEADBAND 才写 DAC 并重算增益（滞回，滤除旋钮抖动）
 */
#define SETVOLUME_SAMPLE_DIV   16U   /* 采样降频分频（主循环轮数） */
#define SETVOLUME_DEADBAND     64U   /* 滞回门限（0~16383 刻度，对应 ADC 原始值 16） */

void SetVolume(void)
{
	static uint16_t s_div = 0U;
	static uint16_t s_last_dc = 0xFFFFU;
	uint16_t DC_Data;
	uint32_t wheel_pct;  /* 0~16383, 即 wheel_pct = DC_Data */

	/* 降频：未到采样窗口直接返回，避免每轮做一次阻塞式 ADC 采样 */
	if (++s_div < SETVOLUME_SAMPLE_DIV) {
		return;
	}
	s_div = 0U;

#if HW_VOLUME_ADC_EN
	GPIO_RegOneBitClear(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
	GPIO_RegOneBitSet(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
	DC_Data = ADC_SingleModeDataGet(HW_VOLUME_ADC_CHANNEL) * 4;
#else
	/* BANBOX_II: 无音量旋钮，固定最大音量 */
	DC_Data = 0x3FFF;
#endif

	/* 滞回：变化未越过门限则跳过 DAC 写入与增益重算 */
	if (s_last_dc != 0xFFFFU) {
		uint16_t diff = (DC_Data > s_last_dc) ? (uint16_t)(DC_Data - s_last_dc)
		                                      : (uint16_t)(s_last_dc - DC_Data);
		if (diff <= SETVOLUME_DEADBAND) {
			return;
		}
	}
	s_last_dc = DC_Data;

	AudioDAC_VolSet(DAC0, DC_Data, DC_Data);
	AudioDAC_VolSet(DAC1, DC_Data, 0);

	/* 计算BT/USB增益映射 */
	wheel_pct = DC_Data;  /* 0~16383 */
	/* bt_gain_q8 = wheel_pct * bt_max_volume / 16383 * 256 / 100
	 *            = wheel_pct * bt_max_volume * 256 / (16383 * 100)
	 * 简化: 先算 wheel_pct * 256 / 16383 得到旋钮Q8，再乘 bt_max_volume / 100 */
	s_bt_gain_q8  = (uint16_t)((uint32_t)wheel_pct * g_sys_param.volume.bt_max_volume  * 256 / (16383 * 100));
	s_usb_gain_q8 = (uint16_t)((uint32_t)wheel_pct * g_sys_param.volume.usb_max_volume * 256 / (16383 * 100));
}

/* USB 音频始终允许（Looper 已移除，无模式互斥） */
uint8_t BG_AudioUSBAllowed(void)
{
	return 1U;
}


void AudioLoopWithGraph(void)
{
	uint16_t frame_size;
	uint16_t processed_samples;
	uint16_t adc0_avail, adc1_avail;
	EffectGraphRuntime_t *graph;
	const uint16_t MIN_FRAME = 48;
	static uint16_t s_gpio_div_graph = 0;

	graph = EffectGraph_GetInstance();
	if (!graph) {
		return;
	}
	graph->min_frame_size = MIN_FRAME;
	graph->max_frame_size = EFFECT_GRAPH_BUFFER_SIZE;

	adc0_avail = AudioADC_DataLenGet(ADC0_MODULE);
	adc1_avail = AudioADC_DataLenGet(ADC1_MODULE);
	frame_size = (adc0_avail < adc1_avail) ? adc0_avail : adc1_avail;
	if (frame_size < MIN_FRAME) {
		return;
	}
	if (frame_size > EFFECT_GRAPH_BUFFER_SIZE) {
		frame_size = EFFECT_GRAPH_BUFFER_SIZE;
	}
	graph->drive_mode = DRIVE_MODE_ADC;

	processed_samples = EffectGraph_Process(frame_size);
	if (processed_samples > 0) {
		if (++s_gpio_div_graph >= 50) {
			s_gpio_div_graph = 0;
			ProcessGuitarOutput();
			ProcessMicOutput();
			ProcessSpeakerSwitch();
		}
		BG_AudioManager.Audio_data.guitar_count++;
		BG_AudioManager.Audio_data.mic_count++;
	}
}

/**
 * 音频主循环处理函数
 */
/**
 * @brief USB 热拔插检测
 *
 * 检测 USB 线缆连接/断开状态变化:
 *   - 插入: UsbDeviceEnable() 使 USB 设备生效，发布 EVT_SYS_USB_CONNECT
 *   - 拔出: UsbDeviceDisable() 关闭 USB 设备，发布 EVT_SYS_USB_DISCONNECT
 */
void USB_HotplugCheck(void)
{
	bool now_connected = OTG_PortDeviceIsLink();

	if (now_connected != s_usb_connected) {
		if (now_connected) {
			DBG("[USB] Cable connected, enabling device\n");
			UsbDeviceEnable();
			BG_EVT_PUB(EVT_SYS_USB_CONNECT);
		} else {
			DBG("[USB] Cable disconnected, disabling device\n");
			UsbDeviceDisable();
			BG_EVT_PUB(EVT_SYS_USB_DISCONNECT);
		}
		s_usb_connected = now_connected;
	}
}

void Audio_loop(void)
{
	uint8_t lp_activity = 0;

	BtStackServiceRun();
	SetVolume();
	OTG_DeviceRequestProcess();
	OTG_DeviceCDC_Task();
	USB_HotplugCheck();

	if (usb_speaker_enable && UsbAudioSpeakerDataLenGet() > 0) {
		lp_activity |= LP_ACT_USB_AUDIO;
	}
	if (OTG_DeviceCDC_GetRxCount() > 0) {
		lp_activity |= LP_ACT_CDC_COMM;
	}
	if (ShellIOManager_HasIncomingData()) {
		lp_activity |= LP_ACT_BLE_COMM;
	}
	if (lp_activity) {
		LowPower_FeedActivity(lp_activity);
	}
	LowPower_Tick();

	if (!LowPower_IsLowPower()) {
		AudioLoopWithGraph();
	}
	ShellIOManager_Process();
}
