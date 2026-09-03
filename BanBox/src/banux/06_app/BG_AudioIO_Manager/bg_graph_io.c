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
#include "audio_decoder_api.h"
#include "bt_manager.h"
#include "bg_low_power.h"
#include "remind_sound.h"
#include "pitch_shift.h"

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
	return UsbAudioSpeakerDataLenGet();
}

/**
 * BT 可用数据量查询 - 蓝牙音频输入
 *
 * 【修正 v3】严格参考老方案：
 *   老方案先解码，获取实际帧长 n，然后用 n 等待 DAC/ADC
 *   所以这里必须预解码来获取准确帧长！
 *
 * 预解码数据缓存到 bt_decoded_buffer，供 BT_ReadAudioData 使用
 * 采样率变化时同步 DAC/ADC
 * 
 * 【关键修正】pcm_addr 是 int32_t*，不是 int16_t*！
 *   每个样本是 32-bit（左右声道各 16-bit 打包）
 */
uint16_t BT_GetAvailableData(EffectNode_t *node)
{
	uint16_t i;
	uint32_t *pcm_data;  /* 【修正】SDK 中 pcm_addr 是 uint32_t*，每个样本 32-bit */
	uint16_t pcm_len;
	uint32_t bt_sample_rate;
	
	(void)node;
	
	/* 如果已有预解码数据，直接返回其长度（避免重复解码） */
	if (bt_has_decoded_data && bt_decoded_len > 0) {
		return bt_decoded_len;
	}
	
	/* 检查蓝牙是否处于流式传输状态 */
	if (GetA2dpState() != BT_A2DP_STATE_STREAMING) {
		return 0;
	}

	/* 提示音占用全局解码器时，暂停 A2DP 解码，仅缓冲 SBC */
	if (RemindSound_IsPlaying()) {
		return 0;
	}
	
	/* 检查SBC缓冲区是否有足够数据（与老方案一致） */
	if (mv_msize(&SBC_MemHandle) <= SBC_DECODER_FIFO_MIN) {
		return 0;
	}
	
	/* 检查解码器是否可以继续（与老方案一致） */
	if (audio_decoder_can_continue() != RT_SUCCESS) {
		return 0;
	}
	
	/* 解码一帧（与老方案一致：必须先解码才知道帧长） */
	if (audio_decoder_decode() != RT_SUCCESS) {
		return 0;
	}
	
	/* 获取解码后的数据 */
	if (!audio_decoder || !audio_decoder->song_info) {
		return 0;
	}
	
	pcm_data = audio_decoder->song_info->pcm_addr;  /* int32_t* */
	pcm_len = audio_decoder->song_info->pcm_data_length;
	bt_sample_rate = audio_decoder->song_info->sampling_rate;
	
	/* 【关键】检查采样率变化，同步 DAC/ADC（与老方案一致） */
	if (bt_current_sample_rate != bt_sample_rate) {
		bt_current_sample_rate = bt_sample_rate;
		BG_AudioManager.Audio_data.SampleRate = bt_sample_rate;
		DBG("[BT] Sample rate: %ld Hz, syncing DAC/ADC...\n", (long)bt_sample_rate);
		
		/* 同步 DAC 采样率 */
		AudioDAC_SampleRateChange(DAC0, bt_sample_rate);
		AudioDAC_SampleRateChange(DAC1, bt_sample_rate);
		
		/* 同步 ADC 采样率 */
		AudioADC_SampleRateSet(ADC0_MODULE, bt_sample_rate);
		AudioADC_SampleRateSet(ADC1_MODULE, bt_sample_rate);
	}
	
	/* 限制帧长到 BT_DECODED_BUFFER_SIZE (与效果图缓冲区对齐) */
	if (pcm_len > BT_DECODED_BUFFER_SIZE) {
		pcm_len = BT_DECODED_BUFFER_SIZE;
	}
	
	/* 缓存预解码数据（与老方案完全一致：直接复制 int32 到 uint32） */
	/* 老方案: bt_audio_buffer[i] = (uint32_t)audio_decoder->song_info->pcm_addr[i]; */
	for (i = 0; i < pcm_len; i++) {
		bt_decoded_buffer[i] = (uint32_t)pcm_data[i];
	}
	bt_decoded_len = pcm_len;
	bt_has_decoded_data = true;
	
	/* 返回实际解码的帧长（这个值将用于等待 DAC/ADC） */
	return pcm_len;
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
		/* 提示音播放期间，ADC数据不输出给DAC（仍读取以消耗FIFO） */
		if (RemindSound_IsPlaying()) {
			memset(out_buf, 0, samples_to_read * sizeof(uint32_t));
		}
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
		/* 提示音播放期间，ADC数据不输出给DAC（仍读取以消耗FIFO） */
		if (RemindSound_IsPlaying()) {
			memset(out_buf, 0, samples_to_read * sizeof(uint32_t));
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
#if BG_PITCH_SHIFT_TEST_EN
		/* 试听：不进音效图，在最终输出处临时做半音移调 */
		PitchShift_ProcessPackedStereo(PitchShift_GetDefault(),
		                               in_buf,
		                               (int32_t)samples_to_write);
#endif
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
	
	/* 检查 USB 音频是否启用 */
	if (!usb_speaker_enable) {
		/* USB 未启用，填零 */
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
	
	// 检查 USB 麦克风是否启用
	if (!usb_mic_enable) {
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

/**
 * 蓝牙 Audio Source 回调 - 从蓝牙解码器读取音频数据
 * 
 * 【修正 v3】严格参考老方案：
 *   数据已在 BT_GetAvailableData 中预解码并缓存到 bt_decoded_buffer
 *   这里直接使用缓存的数据，不再重新解码
 *   
 *   老方案流程：解码 → 转换到 bt_audio_buffer → 使用
 *   新方案流程：BT_GetAvailableData 解码并缓存 → BT_ReadAudioData 直接使用缓存
 * 
 * 【关键修正】:
 *   1. 不再重复检查蓝牙状态（BT_GetAvailableData 已检查过）
 *   2. 如果没有预解码数据，填零保证长度一致（与 USB_ReadAudioData 一致）
 *   3. 返回的长度与 max_len 一致，保证所有节点帧长同步
 */
uint16_t BT_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t i;
	uint16_t pcm_len;
	
	(void)node;

	/* 限制最大长度 */
	if (max_len > 640) {
		max_len = 640;
	}
	
	/* 使用预解码数据（来自 BT_GetAvailableData） */
	if (bt_has_decoded_data && bt_decoded_len > 0) {
		pcm_len = bt_decoded_len;
		
		/* 【关键】使用预解码数据长度，不使用 max_len */
		/* 因为 max_len 就是 BT_GetAvailableData 返回的 bt_decoded_len */
		
		/* 复制缓存数据到输出缓冲区，同时应用BT音乐增益映射 */
		if (s_bt_gain_q8 == 256) {
			/* 增益1.0x，直接复制 */
			for (i = 0; i < pcm_len; i++) {
				out_buf[i] = bt_decoded_buffer[i];
			}
		} else {
			/* 应用Q8增益 */
			int16_t *dst = (int16_t *)out_buf;
			int16_t *src = (int16_t *)bt_decoded_buffer;
			for (i = 0; i < pcm_len * 2; i++) {
				int32_t s = (int32_t)src[i] * s_bt_gain_q8 >> 8;
				if (s > 32767) s = 32767;
				else if (s < -32768) s = -32768;
				dst[i] = (int16_t)s;
			}
		}
		
		/* 清除预解码标志（数据已被消费） */
		bt_has_decoded_data = false;
		bt_decoded_len = 0;
		
		/* 返回实际帧长（应该与 max_len 相同） */
		return pcm_len;
	}
	
	/* 没有预解码数据，填零（与 USB_ReadAudioData 一致，保证长度） */
	for (i = 0; i < max_len; i++) {
		out_buf[i] = 0;
	}
	return max_len;
}
