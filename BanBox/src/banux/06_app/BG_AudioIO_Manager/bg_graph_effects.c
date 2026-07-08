/**
 * @file bg_graph_effects.c
 * @brief Effect Graph DSP node processors (EQ/DRC/Reverb/Mixer...).
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_internal.h"
#include "product_def.h"
#include "debug.h"

#include "audio_effect.h"
#include "ctrlvars.h"
#include "reverb.h"
#include "effect_graph_config.h"

void ADC_Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	uint16_t i;
	int16_t *out_16 = (int16_t *)out_buf;
	
	(void)node;
	
	/* 安全检查 */
	if (!out_buf || len == 0 || in_count == 0) {
		return;
	}
	
	/* 副音箱模式：2个输入（ADC0立体声, ADC1立体声）*/
	if (in_count == 2) {
		/* 直接混合两个立体声输入 */
		for (i = 0; i < len; i++) {
			int32_t *in0_32 = (int32_t *)in_bufs[0];
			int32_t *in1_32 = (int32_t *)in_bufs[1];
			int16_t *in0_16 = (int16_t *)&in0_32[i];
			int16_t *in1_16 = (int16_t *)&in1_32[i];
			
			int32_t left_sum = 0;
			int32_t right_sum = 0;
			
			/* 混合左声道 */
			if (in_bufs[0]) {
				left_sum += in0_16[0];
			}
			if (in_bufs[1]) {
				left_sum += in1_16[0];
			}
			
			/* 混合右声道 */
			if (in_bufs[0]) {
				right_sum += in0_16[1];
			}
			if (in_bufs[1]) {
				right_sum += in1_16[1];
			}
			
			/* 饱和限制到16位 */
			if (left_sum > 32767) left_sum = 32767;
			if (left_sum < -32768) left_sum = -32768;
			if (right_sum > 32767) right_sum = 32767;
			if (right_sum < -32768) right_sum = -32768;
			
			/* 打包成32位: [低16位=L | 高16位=R] */
			out_16[i * 2] = (int16_t)left_sum;
			out_16[i * 2 + 1] = (int16_t)right_sum;
		}
		return;
	}
	
	/* 主音箱模式：4个输入（guitar_L, guitar_R, mic_L, mic_R）*/
	if (in_count != 4) {
		DBG("[ADC_Mixer] Warning: Expected 2 or 4 inputs, got %d\n", in_count);
		return;
	}
	
	/* 合并: 所有输入混成单声道后输出到 L/R 双声道（居中声像）
	 * 原因: 吉他通常接单声道线（TRS/TS），只有 L 声道有信号，R≈0。
	 * 若分别输出 L/R，则实时监听时 R 耳无声，而 Looper 回放是
	 * 单声道复制到双声道，导致回放感知响度约 2-4x 强于实时监听。
	 * 将所有输入混合为单声道后同时送到 L 和 R，与 Looper 回放行为一致。
	 */
	for (i = 0; i < len; i++) {
		int32_t mono_sum = 0;

		/* 累加全部4路单声道信号：guitar_L + guitar_R + mic_L + mic_R */
		if (in_bufs[0]) mono_sum += ((int16_t *)in_bufs[0])[i];
		if (in_bufs[1]) mono_sum += ((int16_t *)in_bufs[1])[i];
		if (in_bufs[2]) mono_sum += ((int16_t *)in_bufs[2])[i];
		if (in_bufs[3]) mono_sum += ((int16_t *)in_bufs[3])[i];

		/* 饱和限制到16位 */
		if (mono_sum > 32767)  mono_sum =  32767;
		if (mono_sum < -32768) mono_sum = -32768;

		/* 同时送到 L 和 R，保持与 Looper 单声道回放的响度一致 */
		out_16[i * 2]     = (int16_t)mono_sum;
		out_16[i * 2 + 1] = (int16_t)mono_sum;
	}
}

/**
 * 混音器处理回调 - 将多路输入混合为一路输出
 */
void Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	uint16_t i;
	uint8_t j;
	
	(void)node; /* 未使用，消除警告 */
	
	/* 安全检查 */
	if (!out_buf || len == 0 || in_count == 0) {
		return;
	}
	
	/* 限制最大长度，避免缓冲区溢出 */
	if (len > 640) {
		len = 640;
	}
	
	/* 清零输出缓冲区 */
	for (i = 0; i < len; i++) {
		out_buf[i] = 0;
	}
	
	/* 【修复】逐声道饱和累加，避免 uint32_t 直接相加时 LOW16 溢出的进位污染 HIGH16
	 * 格式约定（与 ADC_Mixer_Process 一致）:
	 *   bits[15: 0] = LEFT  声道 (int16_t)
	 *   bits[31:16] = RIGHT 声道 (int16_t)
	 */
	for (j = 0; j < in_count; j++) {
		if (in_bufs[j]) {
			for (i = 0; i < len; i++) {
				int32_t acc_left  = (int16_t)(out_buf[i] & 0xFFFF)
				                  + (int16_t)(in_bufs[j][i] & 0xFFFF);
				int32_t acc_right = (int16_t)((out_buf[i] >> 16) & 0xFFFF)
				                  + (int16_t)((in_bufs[j][i] >> 16) & 0xFFFF);
				/* 饱和到 16 位有符号范围 */
				if (acc_left  >  32767) acc_left  =  32767;
				if (acc_left  < -32768) acc_left  = -32768;
				if (acc_right >  32767) acc_right =  32767;
				if (acc_right < -32768) acc_right = -32768;
				out_buf[i] = ((uint32_t)(uint16_t)(int16_t)acc_right << 16)
				           | ((uint16_t)(int16_t)acc_left & 0xFFFF);
			}
		}
	}
}
void Expander_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	(void)node;
	
	if (in_count < 1 || !in_bufs[0]) {
		return;
	}
	
	/* 调用 SDK 扩展器效果 */
	if (gCtrlVars.mic_expander_unit.enable) {
		AudioEffectExpanderApply(&gCtrlVars.mic_expander_unit,
		                         (int16_t *)in_bufs[0],
		                         (int16_t *)out_buf,
		                         len);
	} else {
		/* 旁路：直接复制 */
		uint16_t i;
		for (i = 0; i < len; i++) {
			out_buf[i] = in_bufs[0][i];
		}
	}
}

/**
 * DRC 处理回调 - 动态范围压缩
 * 调用 SDK AudioEffectDRCApply
 */
void DRC_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	if (in_count < 1 || !in_bufs[0]) {
		return;
	}

	/* 同步EffectGraph参数到全局DRC单元，只在参数实际变化时才重新配置 SDK
	 * （避免每帧都调用 AudioEffectDRCConfig 导致 CPU 超载 / 帧丢失 / 升调） */
	{
		static int8_t  last_threshold = 0x7f;
		static uint8_t last_ratio     = 0xff;
		static uint8_t last_attack    = 0xff;
		static uint8_t last_release   = 0xff;
		int8_t  cur_thr = node->params.drc.threshold;
		uint8_t cur_rat = node->params.drc.ratio;
		uint8_t cur_atk = node->params.drc.attack;
		uint8_t cur_rel = node->params.drc.release;
		if (cur_thr != last_threshold || cur_rat != last_ratio ||
		    cur_atk != last_attack    || cur_rel != last_release) {
			gCtrlVars.mic_drc_unit.threshold[0] = cur_thr;
			gCtrlVars.mic_drc_unit.ratio[0]     = cur_rat;
			gCtrlVars.mic_drc_unit.attack_tc[0] = cur_atk;
			gCtrlVars.mic_drc_unit.release_tc[0]= cur_rel;
			#if CFG_AUDIO_EFFECT_MIC_DRC_EN
			AudioEffectDRCConfig(&gCtrlVars.mic_drc_unit, 2, 44100);
			#endif
			last_threshold = cur_thr;
			last_ratio     = cur_rat;
			last_attack    = cur_atk;
			last_release   = cur_rel;
		}
	}

	#if CFG_AUDIO_EFFECT_MIC_DRC_EN
	if (gCtrlVars.mic_drc_unit.enable) {
		AudioEffectDRCApply(&gCtrlVars.mic_drc_unit,
		                    (int16_t *)in_bufs[0],
		                    (int16_t *)out_buf,
		                    len);
	} else
	#endif
	{
		/* 旁路：直接复制 */
		uint16_t i;
		for (i = 0; i < len; i++) {
			out_buf[i] = in_bufs[0][i];
		}
	}
}

/**
 * EQ 处理回调 - 均衡器
 * 调用 SDK AudioEffectEQApply
 * 
 * ADC EQ节点(eq_guitar_l/r, eq_mic_l/r):
 *   - 输入: 32位双声道数据(高16位=R, 低16位=L)
 *   - 根据edge的src_port提取对应声道: 0=L, 1=R
 *   - 处理: 单声道16位EQ
 *   - 输出: 单声道16位数据(存储在32位buffer的低16位)
 * 
 * USB/BT EQ节点(usb_bt_eq):
 *   - 输入/输出: 32位双声道数据
 *   - 处理: 立体声16位EQ
 */
void EQ_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	EQUnit *target_eq;
	uint16_t i;
	int16_t *temp_buf_mono;
	uint8_t src_port;
	
	if (in_count < 1 || !in_bufs[0]) {
		return;
	}

	/* 根据节点ID选择对应的独立EQ单元 */
	switch (node->id) {
		case NODE_ID_EQ_GUITAR_L:
			target_eq = &gCtrlVars.eq_guitar_l_unit;
			src_port = 0;  /* L声道 */
			break;
		case NODE_ID_EQ_GUITAR_R:
			target_eq = &gCtrlVars.eq_guitar_r_unit;
			src_port = 1;  /* R声道 */
			break;
		case NODE_ID_EQ_MIC_L:
			target_eq = &gCtrlVars.eq_mic_l_unit;
			src_port = 0;  /* L声道 */
			break;
		case NODE_ID_EQ_MIC_R:
			target_eq = &gCtrlVars.eq_mic_r_unit;
			src_port = 1;  /* R声道 */
			break;
		case NODE_ID_USB_BT_EQ:
			target_eq = &gCtrlVars.music_out_eq_unit;
			src_port = 255;  /* 双声道标记 */
			break;
		default:
			/* 未知节点，使用默认EQ避免崩溃 */
			target_eq = &gCtrlVars.eq_guitar_l_unit;
			src_port = 0;
			//DBG("[EQ_Process] WARNING: Unknown node ID %d, using default EQ\n", node->id);
			break;
	}

	/* 调试输出 */
	// eq_debug_counter++;
	// if ((eq_debug_counter & 0x1FFF) == 0) {
	// 	DBG("[EQ_Process] node_id=%d name=%s port=%d | target_eq: en=%d fc=%d ch=%d ct=%p\n", 
	// 	    node->id, node->name, src_port, target_eq->enable, target_eq->filter_count, 
	// 	    target_eq->channel, target_eq->ct);
	// }

	/* 根据target_eq的enable标志决定是否应用EQ处理 */
	#if CFG_AUDIO_EFFECT_MIC_OUT_EQ_EN
	if (target_eq->enable && target_eq->filter_count > 0 && target_eq->ct != NULL) {
		if (src_port == 255) {
			/* USB/BT EQ: 双声道处理 */
			AudioEffectEQApply(target_eq,
			                   (int16_t *)in_bufs[0],
			                   (int16_t *)out_buf,
			                   len,
			                   2);  /* 2 = 立体声 */
		} else {
			/* ADC EQ: 单声道处理
			 * 从32位双声道数据中提取对应声道(L或R) */
			temp_buf_mono = (int16_t *)in_bufs[0];  /* 重解释为16位数组 */
			
			/* 提取单声道数据到out_buf (每个32位样本提取一个16位样本) */
			for (i = 0; i < len; i++) {
				/* src_port=0: 提取低16位(L), src_port=1: 提取高16位(R) */
				int16_t mono_sample = temp_buf_mono[i * 2 + src_port];
				/* 暂存到out_buf的低16位 */
				((int16_t *)out_buf)[i] = mono_sample;
			}
			
			/* 单声道EQ处理 */
			AudioEffectEQApply(target_eq,
			                   (int16_t *)out_buf,
			                   (int16_t *)out_buf,
			                   len,
			                   1);  /* 1 = 单声道 */
			
			/* 处理后的单声道数据已经在out_buf的低16位，保持不变 */
		}
	} else
	#endif
	{
		/* 旁路：根据节点类型正确提取数据 */
		uint16_t i;
		if (src_port == 255) {
			/* USB/BT EQ 旁路：直接复制立体声输入 */
			for (i = 0; i < len; i++) {
				out_buf[i] = in_bufs[0][i];
			}
		} else {
			/* ADC 单声道 EQ 旁路：必须正确提取对应声道，
			 * 否则 ADC_Mixer_Process 读到的是交错的 L/R 样本而非单声道。
			 * in_bufs[0] 是 uint32_t 立体声包 [L|R]，
			 * src_port=0 → L 声道 (偶数 int16 位置)
			 * src_port=1 → R 声道 (奇数 int16 位置) */
			for (i = 0; i < len; i++) {
				int16_t mono_sample =
					((int16_t *)in_bufs[0])[i * 2 + src_port];
				((int16_t *)out_buf)[i] = mono_sample;
			}
		}
	}
}

/**
 * 直通处理回调 - 直接复制输入到输出，不做任何处理
 * 用于 USB/BT 路径，保持与老方案一致（BT 音频不经过效果处理）
 */
void Passthrough_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	uint16_t i;
	
	(void)node;
	
	if (in_count < 1 || !in_bufs[0]) {
		/* 无输入，清零输出 */
		for (i = 0; i < len; i++) {
			out_buf[i] = 0;
		}
		return;
	}
	
	/* 直接复制，不做任何效果处理 */
	for (i = 0; i < len; i++) {
		out_buf[i] = in_bufs[0][i];
	}
}

/**
 * 混响处理回调 - 添加空间感
 * 调用 SDK AudioEffectReverbApply
 */
void Reverb_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	if (in_count < 1 || !in_bufs[0]) {
		return;
	}

	/* 只在参数实际变化时才重新配置 Reverb，避免每帧调用 reverb_configure 导致 CPU 超载 */
	{
		static uint8_t last_room_size = 0xff;
		static uint8_t last_damping   = 0xff;
		static uint8_t last_wet_dry   = 0xff;
		uint8_t cur_rs  = node->params.reverb.room_size;
		uint8_t cur_dmp = node->params.reverb.damping;
		uint8_t cur_wet = node->params.reverb.wet_dry;
		if (cur_rs != last_room_size || cur_dmp != last_damping || cur_wet != last_wet_dry) {
			gCtrlVars.reverb_unit.dry_scale      = 100;
			gCtrlVars.reverb_unit.wet_scale      = cur_wet;
			gCtrlVars.reverb_unit.roomsize_scale = cur_rs;
			gCtrlVars.reverb_unit.damping_scale  = cur_dmp;
			gCtrlVars.reverb_unit.width_scale    = 50;
			if (gCtrlVars.reverb_unit.ct) {
				reverb_configure(gCtrlVars.reverb_unit.ct,
				                gCtrlVars.reverb_unit.dry_scale,
				                gCtrlVars.reverb_unit.wet_scale,
				                gCtrlVars.reverb_unit.width_scale,
				                gCtrlVars.reverb_unit.roomsize_scale,
				                gCtrlVars.reverb_unit.damping_scale);
			}
			last_room_size = cur_rs;
			last_damping   = cur_dmp;
			last_wet_dry   = cur_wet;
		}
	}

	/* ct==NULL 时无论 enable 状态如何都必须旁通：
	 * ChainGraph 恢复参数后 enable 可能被重新置 1，但内存申请已经失败(ct=NULL)，
	 * AudioEffectReverbApply 遇到 ct==NULL 会直接 return 而不写 out_buf，
	 * 导致 out_buf 全零 → ADC 信号路径静音。 */
	if (gCtrlVars.reverb_unit.enable && gCtrlVars.reverb_unit.wet_scale > 0
	    && gCtrlVars.reverb_unit.ct != NULL) {
		AudioEffectReverbApply(&gCtrlVars.reverb_unit,
		                       (int16_t *)in_bufs[0],
		                       (int16_t *)out_buf,
		                       len);
	} else {
		/* 旁路：直接复制（含 ct==NULL / enable==0 / wet==0 三种情况） */
		uint16_t i;
		for (i = 0; i < len; i++) {
			out_buf[i] = in_bufs[0][i];
		}
	}
}
