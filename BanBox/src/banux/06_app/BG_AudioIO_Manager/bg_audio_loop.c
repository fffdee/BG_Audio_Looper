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

/* BT 驱动模式忙等循环的超时计数上限。
 * 正常时 DAC/ADC FIFO 会很快满足条件；若数据流被异常打断（如拔插 line in），
 * 超过此次数即放弃本帧返回，避免主循环死锁导致看门狗复位。 */
#define BT_DRIVE_WAIT_TIMEOUT  100000UL

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
	
	/* A2DP 是否在推流；本帧是否用 BT 驱动帧长（SBC 有数据时） */
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

	if (bt_streaming &&
	    (bt_has_decoded_data || mv_msize(&SBC_MemHandle) > SBC_DECODER_FIFO_MIN)) {
		/* ========== 蓝牙驱动模式 ========== */
		frame_size = BT_GetAvailableData(NULL);
		if (frame_size > 0) {
			/* 忙等加超时保护：拔插 line in 等异常可能打断 ADC/DAC 数据流，
			 * 若无限等待会卡死主循环导致看门狗复位。超时后放弃本帧。 */
			uint32_t wait_cnt = BT_DRIVE_WAIT_TIMEOUT;
			while (AudioDAC_DataSpaceLenGet(DAC0) < frame_size) {
				if (--wait_cnt == 0) {
					return;
				}
			}
			wait_cnt = BT_DRIVE_WAIT_TIMEOUT;
			while (AudioADC_DataLenGet(ADC0_MODULE) < frame_size ||
			       AudioADC_DataLenGet(ADC1_MODULE) < frame_size) {
				if (--wait_cnt == 0) {
					return;
				}
			}
			graph->drive_mode = DRIVE_MODE_BT;
		} else {
			/* 推流中但本帧无解码输出：回退 ADC/USB，避免整路饿死 */
			bt_streaming = false;
		}
	} else if (bt_streaming) {
		/* 推流中但 SBC 暂空：回退 ADC/USB，保持 USB 音乐可播 */
		bt_streaming = false;
	}

	if (!bt_streaming) {
		/* ========== ADC/USB 驱动模式 ========== */
		/* 仅在 A2DP 真正结束时清预解码并恢复采样率 */
		if (GetA2dpState() != BT_A2DP_STATE_STREAMING) {
			if (bt_has_decoded_data) {
				bt_has_decoded_data = false;
				bt_decoded_len = 0;
			}
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
		}

		adc0_avail = AudioADC_DataLenGet(ADC0_MODULE);
		adc1_avail = AudioADC_DataLenGet(ADC1_MODULE);
		frame_size = (adc0_avail < adc1_avail) ? adc0_avail : adc1_avail;
		if (frame_size < MIN_FRAME) {
			return;
		}
		if (frame_size > MAX_FRAME) {
			frame_size = MAX_FRAME;
		}
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

		/* Wake only for actual playback data. Merely enumerating USB audio or
		 * connecting Bluetooth must not keep the system permanently awake. */
		if (usb_speaker_enable && UsbAudioSpeakerDataLenGet() > 0) {
			lp_activity |= LP_ACT_USB_AUDIO;
		}
		if (GetA2dpState() == BT_A2DP_STATE_STREAMING &&
		    (bt_has_decoded_data ||
		     mv_msize(&SBC_MemHandle) > SBC_DECODER_FIFO_MIN)) {
			lp_activity |= LP_ACT_BT_AUDIO;
		}
		if (OTG_DeviceCDC_GetRxCount() > 0) {
			lp_activity |= LP_ACT_CDC_COMM;
		}
		if (ShellIOManager_HasIncomingData()) {
			lp_activity |= LP_ACT_BLE_COMM;
		}
		if (AudioLooper.IsPlaying() || AudioLooper.IsRecording()) {
			lp_activity |= LP_ACT_LOOPER;
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
