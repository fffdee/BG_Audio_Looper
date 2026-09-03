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

/* ==================== 工作模式互斥（Loop 模式 / 自由模式） ==================== */

/* 退出 Loop 模式的确认延迟(ms)：
 * Looper 停止后延迟恢复 BT/USB 音频，避免"停止→恢复→再启动"
 * 造成 BT/USB 反复开关（抖动切换比卡顿体验更差）。 */
#define LOOP_MODE_EXIT_DELAY_MS  300U

static uint32_t        s_looper_active_tick = 0U;            /* 最近一次 Looper 工作的时刻(ms) */
static AudioWorkMode_t s_work_mode = AUDIO_WORK_MODE_FREE;   /* 当前生效的工作模式 */

/* Looper 是否正在工作（录音或播放中）：1=工作中 */
uint8_t BG_AudioLooperIsActive(void)
{
	return (AudioLooper.IsRecording() || AudioLooper.IsPlaying()) ? 1U : 0U;
}

/* 更新工作模式状态机（主循环每轮调用一次） */
void BG_AudioWorkModeUpdate(void)
{
	uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

	if (BG_AudioLooperIsActive()) {
		/* Looper 一开始工作就立即切 Loop 模式，确保 Loop 流畅 */
		s_looper_active_tick = now_ms;
		if (s_work_mode != AUDIO_WORK_MODE_LOOP) {
			s_work_mode = AUDIO_WORK_MODE_LOOP;
			/* 注意：这里不清空 SBC_MemHandle。它可能被 BT 协议栈在其它上下文
			 * 写入（mv_mwrite），主循环侧重置 mem_len/p 会与其产生竞态。
			 * 积压数据在退出 Loop 时由解码侧自然丢弃即可。 */
			DBG("[Mode] -> LOOP: BT/USB audio disabled\n");
		}
		return;
	}

	/* Looper 空闲：延迟确认后才切回自由模式 */
	if (s_work_mode != AUDIO_WORK_MODE_FREE) {
		if ((now_ms - s_looper_active_tick) >= LOOP_MODE_EXIT_DELAY_MS) {
			s_work_mode = AUDIO_WORK_MODE_FREE;
			DBG("[Mode] -> FREE: BT/USB audio enabled\n");
		}
	}
}

AudioWorkMode_t BG_AudioGetWorkMode(void)
{
	return s_work_mode;
}

uint8_t BG_AudioBTAllowed(void)
{
	return (s_work_mode == AUDIO_WORK_MODE_FREE) ? 1U : 0U;
}

uint8_t BG_AudioUSBAllowed(void)
{
	return (s_work_mode == AUDIO_WORK_MODE_FREE) ? 1U : 0U;
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

	/* 【模式互斥】Loop 模式禁止 BT 音频驱动本帧。
	 * Looper 工作时强制 48 采样小帧，BT 大帧会与之争抢主循环时间；
	 * 且 BT(44.1k) 与 Looper 采样率冲突，二者同时跑必然导致 Loop 卡顿。 */
	if (!BG_AudioBTAllowed()) {
		bt_streaming = false;
	}

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
		/* 清预解码并恢复采样率。
		 *
		 * 原条件只看 A2DP 是否真正结束。但 Loop 模式互斥会强制 BT 不驱动本帧，
		 * 此时 A2DP 可能仍处于 STREAMING，采样率就会一直卡在 44.1k；
		 * 等蓝牙真正断开时才一次性切回 48k，ADC/DAC 重新同步极易失败，
		 * 表现为"蓝牙断开后 ADC 输入无声，而 LED 仍停在播放态"。
		 * 因此只要 BT 不驱动本帧（含 Loop 模式）就恢复系统默认采样率。 */
		if (GetA2dpState() != BT_A2DP_STATE_STREAMING ||
		    BG_AudioGetWorkMode() == AUDIO_WORK_MODE_LOOP) {
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

		/* 工作模式状态机更新（Loop 模式 / 自由模式 互斥） */
		BG_AudioWorkModeUpdate();

		/* Wake only for actual playback data. Merely enumerating USB audio or
		 * connecting Bluetooth must not keep the system permanently awake.
		 *
		 * 【模式互斥】Loop 模式下不把 BT/USB 音频计入唤醒活动：
		 * 否则它们会持续参与调度并与 Looper 争抢主循环时间，导致 Loop 卡顿。 */
		if (BG_AudioUSBAllowed()) {
			if (usb_speaker_enable && UsbAudioSpeakerDataLenGet() > 0) {
				lp_activity |= LP_ACT_USB_AUDIO;
			}
		}
		if (BG_AudioBTAllowed()) {
			if (GetA2dpState() == BT_A2DP_STATE_STREAMING &&
			    (bt_has_decoded_data ||
			     mv_msize(&SBC_MemHandle) > SBC_DECODER_FIFO_MIN)) {
				lp_activity |= LP_ACT_BT_AUDIO;
			}
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
		/* Loop 模式期间强制保持唤醒（含 300ms 退出延迟窗口）。
		 *
		 * 关键：Looper_TimedOps_Process() 位于低功耗判断的内部，而它负责
		 * 通过 BLE 向 App 回包（衔接/接入/停止/同步录制等）。一旦系统休眠，
		 * 回包发不出去，App 就会判定"指令失败"。
		 * 本轮把 BT/USB 唤醒源门控掉之后，系统更容易进入低功耗，
		 * 因此这里必须显式保活，确保 BLE 指令链路始终可用。 */
		if (BG_AudioGetWorkMode() == AUDIO_WORK_MODE_LOOP) {
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
		/* 说明：Looper_TimedOps_Process() 保持在低功耗判断之内。
		 * 它内部会触发 Flash I/O，在低功耗状态下执行有风险；
		 * 而 Loop 模式已通过 lp_activity 保活（见上方），
		 * 不会进入低功耗，回包链路不受影响。 */
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
