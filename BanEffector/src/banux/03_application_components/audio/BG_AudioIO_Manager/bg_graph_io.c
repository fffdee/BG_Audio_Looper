/**
 * @file bg_graph_io.c
 * @brief Effect Graph source/sink I/O callbacks (ADC/USB/BT/DAC).
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_internal.h"
#include "product_def.h"
#include "debug.h"
#include "bg_audio_detection.h"

#include "audio_adc.h"
#include "dac_interface.h"
#include "usb_audio_api.h"
#include "bg_low_power.h"

uint16_t ADC0_GetAvailableData(EffectNode_t *node)
{
	(void)node;
	return AudioADC_DataLenGet(ADC0_MODULE);
}

/**
 * ADC1 可用数据量查询 - 麦克风输入
 */
uint16_t ADC1_GetAvailableData(EffectNode_t *node)
{
	(void)node;
	return AudioADC_DataLenGet(ADC1_MODULE);
}

/**
 * USB 可用数据量查询 - USB音频输入
 */
uint16_t USB_GetAvailableData(EffectNode_t *node)
{
	(void)node;
	if (!usb_speaker_enable) {
		return 0;
	}
	/* 【模式互斥】Loop 模式下不取 USB 音频（USB 等时传输与 Looper 争抢带宽） */
	if (!BG_AudioUSBAllowed()) {
		return 0;
	}
	return UsbAudioSpeakerDataLenGet();
}

// ==================== 源节点数据读取回调 ====================

/**
 * Guitar ADC Source 回调 - 从 ADC0 读取吉他输入数据
 * 
 * 【关键】在 BT 模式下，AudioLoopWithGraph 已经等待 ADC 有足够数据，
 *        所以这里直接读取 max_len 样本，不再检查 available。
 *        这保证了所有源节点返回相同长度。
 */
uint16_t ADC0_ReadGuitarData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t samples_to_read;

	(void)node;

	/* 限制最大长度 */
	samples_to_read = max_len;
	if (samples_to_read > 640) {
		samples_to_read = 640;
	}

	/* 读取ADC数据（32位=L/R两个16位声道打包） */
	if (samples_to_read > 0) {
		AudioADC_DataGet(ADC0_MODULE, out_buf, samples_to_read);

		/* 【按声道插入检测】每个声道管自己的检测：
		 * 未插入的声道数据置 0，防止串音/浮空噪声污染 DSP 链路与 loop 录音。
		 * 样本格式: bit31..16 = R(右声道/Line2), bit15..0 = L(左声道/Line1) */
		{
			uint32_t ch_mask = (BG_AudioDetection_Line1IsPlugged() ? 0x0000FFFFu : 0u)
			                 | (BG_AudioDetection_Line2IsPlugged() ? 0xFFFF0000u : 0u);
			if (ch_mask != 0xFFFFFFFFu) {
				uint16_t i;
				for (i = 0; i < samples_to_read; i++) {
					out_buf[i] &= ch_mask;
				}
			}
		}

		/* 同步到共享缓冲区供 Looper 按源选择时直接访问 */
		memcpy(BG_AudioManager.Audio_data.guitar_buf_in, out_buf, samples_to_read * sizeof(uint32_t));
		/* 低功耗：检测吉他输入信号是否超过门限 */
		LowPower_CheckADCSignal(out_buf, samples_to_read);
	}
	return samples_to_read;
}

/**
 * Mic ADC Source 回调 - 从 ADC1 读取麦克风数据
 *
 * 【关键】在 BT 模式下，AudioLoopWithGraph 已经等待 ADC 有足够数据，
 *        所以这里直接读取 max_len 样本，不再检查 available。
 *        这保证了所有源节点返回相同长度。
 */
uint16_t ADC1_ReadMicData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t samples_to_read;

	(void)node;

	/* 限制最大长度 */
	samples_to_read = max_len;
	if (samples_to_read > 640) {
		samples_to_read = 640;
	}

	/* 读取ADC数据（32位=L/R两个16位声道打包） */
	if (samples_to_read > 0) {
		AudioADC_DataGet(ADC1_MODULE, out_buf, samples_to_read);
		/* 同步到共享缓冲区供 Looper 按源选择时直接访问 */
		memcpy(BG_AudioManager.Audio_data.mic_buf_in, out_buf, samples_to_read * sizeof(uint32_t));
		/* 低功耗：检测麦克风输入信号是否超过门限 */
		LowPower_CheckADCSignal(out_buf, samples_to_read);
		/* MIC 插入检测 + 稳定期：未插入或插入未满 1 秒时静音，
		 * 防止插入瞬态 pop / 直流漂移进入 DSP 与 Looper 录音。
		 * （仍读取以消耗 FIFO，仅不向下游放行） */
		if (!BG_AudioDetection_MicReady()) {
			memset(out_buf, 0, samples_to_read * sizeof(uint32_t));
			memset(BG_AudioManager.Audio_data.mic_buf_in, 0, samples_to_read * sizeof(uint32_t));
		}
	}
	return samples_to_read;
}

/**
 * DAC Sink 回调 - 写入数据到 DAC0 扬声器输出
 */
void DAC0_WriteSpeakerData(EffectNode_t *node, uint32_t *in_buf, uint16_t len)
{
	uint16_t free_space;
	uint16_t samples_to_write;

	(void)node;

	// 获取 DAC FIFO 可用空间
	free_space = AudioDAC_DataSpaceLenGet(DAC0);
	samples_to_write = (len < free_space) ? len : free_space;
	
	if (samples_to_write > 640) {
		samples_to_write = 640;
	}
	
	if (samples_to_write > 0) {
		// 直接写入数据，无需类型转换
		AudioDAC_DataSet(DAC0, in_buf, samples_to_write);
	}
}

/**
 * USB Audio Source 回调 - 从 USB 读取音频数据
 * 参考老方案 BuildFinalOutput: 数据不足时填零，保证输出长度一致
 * 应用 usb_max_volume 增益映射（Q8定点数乘法）
 */
uint16_t USB_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t available;
	uint16_t i;
	uint16_t ret_len;
	
	(void)node;
	
	/* 检查 USB 音频是否启用（含模式互斥：Loop 模式下不取 USB 音频） */
	if (!usb_speaker_enable || !BG_AudioUSBAllowed()) {
		/* USB 未启用 / Loop 模式，填零 */
		for (i = 0; i < max_len && i < 640; i++) {
			out_buf[i] = 0;
		}
		return max_len > 640 ? 640 : max_len;
	}
	
	/* 限制最大长度 */
	if (max_len > 640) {
		max_len = 640;
	}
	
	/* 从 USB 音频接口读取数据 */
	available = UsbAudioSpeakerDataLenGet();
	
	if (available >= max_len) {
		/* USB 数据充足，直接读取，无需类型转换 */
		UsbAudioSpeakerDataGet(out_buf, max_len);
		ret_len = max_len;
	}
	else if (available > 0) {
		/* USB 数据不足，读取可用数据，剩余填零 */
		UsbAudioSpeakerDataGet(out_buf, available);
		for (i = available; i < max_len; i++) {
			out_buf[i] = 0;
		}
		ret_len = max_len;
	}
	else {
		/* USB 无数据，全部填零避免噪声 */
		for (i = 0; i < max_len; i++) {
			out_buf[i] = 0;
		}
		ret_len = max_len;
	}
	
	/* 应用USB音乐增益映射 (Q8定点数乘法) */
	if (s_usb_gain_q8 != 256) {
		int16_t *samples = (int16_t *)out_buf;
		for (i = 0; i < ret_len * 2; i++) {
			int32_t s = (int32_t)samples[i] * s_usb_gain_q8 >> 8;
			if (s > 32767) s = 32767;
			else if (s < -32768) s = -32768;
			samples[i] = (int16_t)s;
		}
	}
	
	return ret_len;
}

/**
 * USB Audio Sink 回调 - 写入音频数据到 USB
 * 应用 usb_out_volume 增益和 usb_out_mute 静音控制
 */
void USB_WriteAudioData(EffectNode_t *node, uint32_t *in_buf, uint16_t len)
{
	uint16_t samples_to_write;
	uint16_t i;
	
	(void)node;
	
	// 检查 USB 麦克风是否启用（含模式互斥：Loop 模式下不上行 USB 音频）
	if (!usb_mic_enable || !BG_AudioUSBAllowed()) {
		return;
	}
	
	samples_to_write = len;
	if (samples_to_write > 640) {
		samples_to_write = 640;
	}
	
	/* USB输出静音：发送零数据 */
	if (s_usb_out_mute) {
		uint32_t zero_buf[640];
		memset(zero_buf, 0, sizeof(uint32_t) * samples_to_write);
		UsbAudioMicDataSet(zero_buf, samples_to_write);
		return;
	}
	
	/* 应用USB输出增益 (Q8定点数乘法) */
	if (s_usb_out_gain_q8 != 256) {
		int16_t *samples = (int16_t *)in_buf;
		for (i = 0; i < samples_to_write * 2; i++) {
			int32_t s = (int32_t)samples[i] * s_usb_out_gain_q8 >> 8;
			if (s > 32767) s = 32767;
			else if (s < -32768) s = -32768;
			samples[i] = (int16_t)s;
		}
	}
	
	// 写入数据
	UsbAudioMicDataSet(in_buf, samples_to_write);
}
