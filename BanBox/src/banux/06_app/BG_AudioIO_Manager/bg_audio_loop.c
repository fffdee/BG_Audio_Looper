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
#include "audio_adc.h"
#include "dac_interface.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_cdc.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "bg_event.h"
#include "bg_low_power.h"
#include "bt_manager.h"
#include "audio_decoder_api.h"
#include "bt_manager.h"
#include "audio_looper.h"
#include "effect_graph.h"
#include "sys_param.h"
#include "bg_shell.h"
#include "shell_io_manager.h"
#include "dac.h"
#include "bt_stack_service.h"

void SetVolume(void)
{
	uint16_t DC_Data;
	uint32_t wheel_pct;  /* 0~16383, 即 wheel_pct = DC_Data */
#if HW_VOLUME_ADC_EN
	GPIO_RegOneBitClear(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
	GPIO_RegOneBitSet(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
	DC_Data = ADC_SingleModeDataGet(HW_VOLUME_ADC_CHANNEL) * 4;
#else
	/* BANBOX_II: 无音量旋钮，固定最大音量 */
	DC_Data = 0x3FFF;
#endif
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


uint8_t flag_on=1;
void AudioLoopWithGraph(void)
{
	uint16_t frame_size = 0;
	uint16_t processed_samples;
	uint16_t adc0_avail, adc1_avail;
	bool bt_streaming;
	EffectGraphRuntime_t *graph;
	const uint16_t MIN_FRAME = 48;
	const uint16_t MAX_FRAME = BT_DECODED_BUFFER_SIZE;  /* 与缓冲区大小对齐 */
	static bool last_bt_streaming = false;  /* 上一帧的蓝牙状态 */
	static uint16_t s_gpio_div_graph = 0;   /* GPIO 检测降频计数器 */

	// if(flag_on){
	// 	const char *cmd = "sb -t 60 20 3000\r";
	// 	Shell_InputData((uint8_t *)cmd, strlen(cmd));
	// 	flag_on = 0;
	// }
	/* 获取图实例 */
	graph = EffectGraph_GetInstance();
	if (!graph) {
		return;
	}
	
	/* 设置帧长限制 */
	graph->min_frame_size = MIN_FRAME;
	graph->max_frame_size = MAX_FRAME;
	
	/* 【修正】只看 A2DP 流状态，不依赖预解码数据状态 */
	/* 这样即使 SBC 缓冲区暂时数据不足，也保持蓝牙模式 */
	bt_streaming = (GetA2dpState() == BT_A2DP_STATE_STREAMING);
	
	/* 检测模式切换，只在真正切换时打印 */
	if (bt_streaming != last_bt_streaming) {
		if (bt_streaming) {
			DBG("[Audio] Switched to BT streaming mode\n");
		} else {
			DBG("[Audio] Switched to ADC mode\n");
		}
		last_bt_streaming = bt_streaming;
	}
	
	if (bt_streaming) {
		/* ========== 蓝牙驱动模式 (严格参考 AudioLoopWithBT) ========== */
		
		/* 1. 【关键】检查 SBC 缓冲区是否有数据（与老方案一致） */
		if (mv_msize(&SBC_MemHandle) <= SBC_DECODER_FIFO_MIN && !bt_has_decoded_data) {
			/* SBC 数据不足且无预解码数据，等待数据到来，不切换到 ADC 模式 */
			return;
		}
		
		/* 2. 预解码获取实际帧长（与老方案一致：先解码才知道 n） */
		frame_size = BT_GetAvailableData(NULL);
		if (frame_size == 0) {
			return;  /* 蓝牙真的没数据，等待 */
		}
		
		/* frame_size 现在是 BT 解码后的实际帧长 */
		
		/* 3. 【关键】阻塞等待 DAC 有足够空间 (与老方案一致!) */
		while (AudioDAC_DataSpaceLenGet(DAC0) < frame_size) {
			/* 忙等待 */
		}
		
		/* 4. 【关键】阻塞等待 ADC 有足够数据 (与老方案一致!) */
		while (AudioADC_DataLenGet(ADC0_MODULE) < frame_size || AudioADC_DataLenGet(ADC1_MODULE) < frame_size) {
			/* 忙等待 */
		}
		
		/* 5. 设置 BT 驱动模式（不打印日志避免刷屏） */
		graph->drive_mode = DRIVE_MODE_BT;
		
	} else {
		/* ========== ADC 驱动模式 (参考 AudioLoopMinimal) ========== */
		/* 清除可能残留的蓝牙预解码数据 */
		if (bt_has_decoded_data) {
			bt_has_decoded_data = false;
			bt_decoded_len = 0;
		}
		
		/* 【关键】蓝牙断开后恢复默认采样率 */
		if (bt_current_sample_rate != 0 && 
		    BG_AudioManager.Audio_data.SampleRate != system_default_sample_rate) {
			DBG("[Audio] BT disconnected, restoring sample rate to %ld Hz\n", 
			    (long)system_default_sample_rate);
			
			AudioDAC_SampleRateChange(DAC0, system_default_sample_rate);
			AudioDAC_SampleRateChange(DAC1, system_default_sample_rate);
			AudioADC_SampleRateSet(ADC0_MODULE, system_default_sample_rate);
			AudioADC_SampleRateSet(ADC1_MODULE, system_default_sample_rate);
			
			BG_AudioManager.Audio_data.SampleRate = system_default_sample_rate;
			bt_current_sample_rate = 0;
		}
		
		/* 获取 ADC 数据可用量 */
		adc0_avail = AudioADC_DataLenGet(ADC0_MODULE);
		adc1_avail = AudioADC_DataLenGet(ADC1_MODULE);
		
		/* 取最小值 */
		frame_size = (adc0_avail < adc1_avail) ? adc0_avail : adc1_avail;
		
		/* 帧大小限制 */
		if (frame_size < MIN_FRAME) {
			return; /* 数据不足 */
		}
		if (frame_size > MAX_FRAME) {
			frame_size = MAX_FRAME;
		}
		
		/* 设置 ADC 驱动模式（不打印日志避免刷屏） */
		graph->drive_mode = DRIVE_MODE_ADC;
	}
	
	/* 6. Looper 录制/播放时强制 frame_size=48
	 * 原因：Looper 每帧处理 48 个采样，凑满 64 采样(256字节)写一页 PSRAM。
	 * 若 frame_size>48，录制端只保存前 48 个采样（丢弃剩余），
	 * 导致音频时间压缩 → 播放加速。
	 * 强制 frame_size=48 使录制和播放速率匹配
	 */
	if (AudioLooper.IsRecording() || AudioLooper.IsPlaying()) {
		if (frame_size > 48) {
			frame_size = 48;
		}
	}

	/* 【内存保护】frame_size 绝不能超过 EFFECT_GRAPH_BUFFER_SIZE，否则节点缓冲区溢出 */
	if (frame_size > EFFECT_GRAPH_BUFFER_SIZE) {
		frame_size = EFFECT_GRAPH_BUFFER_SIZE;
	}

	/* 调用 Effect Graph 处理 */
	processed_samples = EffectGraph_Process(frame_size);
	
	if (processed_samples > 0) {
		/* 更新 GPIO 检测状态（每 50 帧一次，避免高频 GPIO 切换耦合噪声） */
		if (++s_gpio_div_graph >= 50)
		{
			s_gpio_div_graph = 0;
			ProcessGuitarOutput();
			ProcessMicOutput();
			ProcessSpeakerSwitch();
		}
		
		BG_AudioManager.Audio_data.guitar_count++;
		BG_AudioManager.Audio_data.mic_count++;
	}

#if LOOPER_IO_BUFFER_ENABLE
	/* Effect Graph已处理完毕（含 DAC 输出），现在安全执行Flash IO */
	if (processed_samples > 0) {
		looper_flush_io();
	}
#endif
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
#if USE_EFFECT_GRAPH_MODE
	{
		uint8_t lp_activity = 0;

		BtStackServiceRun();
		SetVolume();
		OTG_DeviceRequestProcess();
		OTG_DeviceCDC_Task();
		USB_HotplugCheck();

		if (usb_speaker_enable || usb_mic_enable) {
			lp_activity |= LP_ACT_USB_AUDIO;
		}
		if (GetA2dpState() == BT_A2DP_STATE_STREAMING) {
			lp_activity |= LP_ACT_BT_AUDIO;
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
			Looper_TimedOps_Process();
		}
		ShellIOManager_Process();
	}
#else
	static uint32_t bt_audio_buffer[640] = {0};

	BtStackServiceRun();
	SetVolume();
	OTG_DeviceRequestProcess();
	OTG_DeviceCDC_Task();
	USB_HotplugCheck();

	if (GetA2dpState() == BT_A2DP_STATE_STREAMING) {
		if (mv_msize(&SBC_MemHandle) > SBC_DECODER_FIFO_MIN) {
			AudioLoopWithBT(bt_audio_buffer);
		}
	} else {
		AudioLoopMinimal(bt_audio_buffer);
	}

	Looper_TimedOps_Process();
	ShellIOManager_Process();
#endif
}
