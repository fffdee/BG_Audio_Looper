/**
 * @file bg_graph_nodes.c
 * @brief Effect Graph auxiliary source/sink nodes (metronome/remind/looper).
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_internal.h"
#include "product_def.h"
#include "debug.h"

#include "metronome.h"
#include "remind_sound.h"
#include "audio_looper.h"

uint16_t Metronome_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t i;
	uint16_t generated = 0;
	
	(void)node;
	
	/* 限制最大长度 */
	if (max_len > 640) {
		max_len = 640;
	}
	
	/* 先清零缓冲区 */
	for (i = 0; i < max_len; i++) {
		out_buf[i] = 0;
	}
	
	/* 如果节拍器未启用，返回静音 */
	if (!MetronomeModule.IsEnabled()) {
		return max_len;
	}
	
	/* 生成节拍器音频（直接写入，不混音） */
	generated = MetronomeModule.GenerateAudio(out_buf, max_len);
	
	return (generated > 0) ? generated : max_len;
}

/**
 * 节拍器可用数据量查询回调
 */
uint16_t Metronome_GetAvailCallback(EffectNode_t *node)
{
	(void)node;
	/* 节拍器始终可以生成数据 */
	return 48;
}

/**
 * 提示音源节点回调 - 解码提示音 PCM 数据
 * 非阻塞：每次调用解码一小帧，混入 USB_BT_MIXER
 */
uint16_t Remind_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	(void)node;
	return RemindSound_GenerateAudio(out_buf, max_len);
}

/**
 * 提示音可用数据量查询回调
 */
uint16_t Remind_GetAvailCallback(EffectNode_t *node)
{
	(void)node;
	return RemindSound_GetAvailableData();
}

/**
 * Looper播放源节点回调 - 从Flash读取录制的音频
 * 支持任意长度请求，通过循环读取多页来填充
 */
uint16_t LooperPlay_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t i;
	uint16_t samples_filled = 0;
	const uint16_t samples_per_page = 48;  /* 每页固定 48 个样本，必须与录音保持一致 */
	
	(void)node;
	
	/* 先清零缓冲区（samples_per_page 大小，不能超） */
	for (i = 0; i < max_len && i < samples_per_page; i++) {
		out_buf[i] = 0;
	}
	
	/* 如果Looper正在播放，每次只读取固定的 samples_per_page(48) 个样本
	 * 播放速率必须与录音速率一致：每次回调 48 个样本，不能多。 */
	if (AudioLooper.IsPlaying()) {
		samples_filled = (max_len < samples_per_page) ? max_len : samples_per_page;
		
		/* 读取一页数据（固定 48 样本），填充到 out_buf */
		AudioLooper.ProcessPlayback32(out_buf, looper_flash_buffer, samples_filled);
	} else {
		/* 未播放时返回 0，pre_reverb_mixer 中 looper 贡献为静音（正确行为） */
		samples_filled = 0;
	}
	
	return samples_filled;
}

/**
 * Looper播放可用数据量查询回调
 */
uint16_t LooperPlay_GetAvailCallback(EffectNode_t *node)
{
	(void)node;
	/* Looper始终可以提供数据（播放或静音） */
	return 48;
}

/**
 * Looper录制输出节点回调 - 将音频写入Flash
 */
void LooperRecord_SinkCallback(EffectNode_t *node, uint32_t *in_buf, uint16_t len)
{
	(void)node;
	
	/* 安全检查：确保输入缓冲区有效 */
	if (!in_buf || len == 0) {
		return;
	}
	
	/* 限制最大长度，避免缓冲区溢出 (每页固定48采样) */
	if (len > 48) {
		len = 48;
	}
	
	/* 设置各录制源缓冲区指针 (Effect Graph 路径) */
	g_looper_src_mic    = BG_AudioManager.Audio_data.mic_buf_in;
	g_looper_src_linein = BG_AudioManager.Audio_data.guitar_buf_in;

	/* 如果Looper正在录制，写入数据 */
	if (AudioLooper.IsRecording()) {
		AudioLooper.ProcessRecording32(in_buf, looper_flash_buffer, len);
	}
}
