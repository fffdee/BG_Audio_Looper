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
	
	/* ct==NULL 时必须旁通：Apply 遇 ct==NULL 不写 out_buf → ADC 链静音 */
	if (gCtrlVars.mic_expander_unit.enable && gCtrlVars.mic_expander_unit.ct != NULL) {
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

	/* ct==NULL 时必须旁通：DRC 已挂在 Final_Mixer 之后，
	 * Apply 不写 out_buf 会导致整机（Mic/吉他/USB/BT/提示音）无声。 */
	#if CFG_AUDIO_EFFECT_MIC_DRC_EN
	if (gCtrlVars.mic_drc_unit.enable && gCtrlVars.mic_drc_unit.ct != NULL) {
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

/**
 * 延迟处理回调 - 系统自带 PCM Delay 效果器
 * 调用 SDK AudioEffectPcmDelayApply。
 * 数据格式: 32位双声道包 [L|R]，PcmDelay 按立体声交织数据处理。
 *   out = 干声(100%) + 湿声(wet%) * 延迟信号
 * SDK PcmDelay 为单抽头纯延迟，不支持内部反馈；feedback 字段保留扩展。
 */
void Delay_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	uint16_t i;
	uint16_t n;
	int16_t *in16;
	int16_t *out16;
	int32_t wet;
	int32_t delay_samples;
	PcmDelayUnit *unit = &gCtrlVars.music_delay_unit;

	if (in_count < 1 || !in_bufs[0]) {
		/* 无输入，清零输出，避免残留数据被播放 */
		if (out_buf) {
			for (i = 0; i < len; i++) out_buf[i] = 0;
		}
		return;
	}

	if (len > EFFECT_GRAPH_BUFFER_SIZE) len = EFFECT_GRAPH_BUFFER_SIZE;
	n    = len;                 /* 每通道样本数 (frames) */
	in16  = (int16_t *)in_bufs[0];
	out16 = (int16_t *)out_buf;

	wet = node->params.delay.wet_dry;   /* 0-100 */
	if (wet < 0)   wet = 0;
	if (wet > 100) wet = 100;

#if CFG_AUDIO_EFFECT_MUSIC_DELAY_EN
	if (unit->enable && unit->ct != NULL && wet > 0) {
		/* 计算并限制延迟样本数 (不超过分配的最大延迟) */
		delay_samples = (int32_t)node->params.delay.delay_ms * (int32_t)gCtrlVars.sample_rate / 1000;
		if (delay_samples > (int32_t)unit->max_delay_samples) delay_samples = (int32_t)unit->max_delay_samples;
		if (delay_samples < 0) delay_samples = 0;
		unit->delay_samples = delay_samples;

		/* pcm_delay_apply 输出 = 纯延迟信号 (delayed in) */
		AudioEffectPcmDelayApply(unit, in16, out16, n);

		/* 干声(100%) + 湿声(wet%) 混合，饱和限制到 16 位 */
		for (i = 0; i < (uint16_t)(n * 2); i++) {
			int32_t v = (int32_t)in16[i] + ((int32_t)out16[i] * wet) / 100;
			if (v >  32767) v =  32767;
			if (v < -32768) v = -32768;
			out16[i] = (int16_t)v;
		}
		return;
	}
#endif
	/* 旁路：直接复制 */
	for (i = 0; i < len; i++) {
		out_buf[i] = in_bufs[0][i];
	}
}

/**
 * 失真处理回调 - 可切换类型 (定点 DSP 终极优化)
 *   类型: 0=SOFT 软削波/蓝调过载, 1=HARD 硬限幅/摇滚金属, 2=FUZZ 法兹/管味
 *   流程: 按类型预增益 -> 软膝/硬限幅 -> tone 一阶低通
 *   - drive 失真量, asym 非对称, tone 亮度, level 输出电平
 *   - 纯加减乘除 + 移位, 无浮点/无查表, BP1048 执行"如丝般顺滑"
 *   数据格式: 32位双声道包 [L|R], len = frames
 */
typedef struct {
	int32_t y_prev_l;   /* L 声道 tone 一阶低通上一输出 */
	int32_t y_prev_r;   /* R 声道 tone 一阶低通上一输出 */
	int32_t fb_l;       /* L 声道反馈状态(上一输出样本) */
	int32_t fb_r;       /* R 声道反馈状态 */
} DistortionState_t;

void Distortion_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	uint16_t i;
	uint16_t n;
	int16_t *in16;
	int16_t *out16;
	static DistortionState_t s_dist;   /* 单失真节点状态 (tone 低通), 静态零初始化 */

	if (in_count < 1 || !in_bufs[0]) {
		if (out_buf) { for (i = 0; i < len; i++) out_buf[i] = 0; }
		return;
	}
	if (len > EFFECT_GRAPH_BUFFER_SIZE) len = EFFECT_GRAPH_BUFFER_SIZE;
	n    = len;                 /* frames */
	in16  = (int16_t *)in_bufs[0];
	out16 = (int16_t *)out_buf;

	/* 节点禁用 / 旁路: 直接复制 */
	if (!node->enabled) {
		for (i = 0; i < len; i++) out_buf[i] = in_bufs[0][i];
		return;
	}

	/* 参数读取与限制 */
	int32_t type  = node->params.distortion.type;
	int32_t drive = node->params.distortion.drive;
	int32_t asym  = node->params.distortion.asym;
	int32_t level = node->params.distortion.level;
	int32_t tone  = node->params.distortion.tone;
	int32_t fb    = node->params.distortion.feedback;
	if (type  < 0) type  = 0; else if (type  > 2) type  = 2;
	if (drive < 0) drive = 0; else if (drive > 100) drive = 100;
	if (asym  < 0) asym  = 0; else if (asym  > 100) asym  = 100;
	if (level < 0) level = 0; else if (level > 100) level = 100;
	if (tone  < 0) tone  = 0; else if (tone  > 100) tone  = 100;
	if (fb    < 0) fb    = 0; else if (fb    > 100) fb    = 100;

	/* 输出电平 (Q10 缩放): 0..32767 */
	int32_t lvl = (level * 32767) / 100;

	/* 按类型预计算常量 (循环外) */
	int32_t gain   = 1024;     /* HARD/FUZZ 预放大 (Q10) */
	int32_t th_pos = 1024;     /* 硬限幅阈值 (Q10) */
	int32_t th_neg = 1024;
	int32_t kp = 0;            /* SOFT 软削波正半周系数 (Q10) */
	int32_t kn = 0;            /* SOFT 软削波负半周系数 (Q10) */
	switch (type) {
	case DIST_TYPE_SOFT:
		/* 软削波系数 k (Q10): drive 0..100 -> 0..0.6 (避免高增益塌陷) */
		kp = (drive * 614) / 100;
		kn = (kp * (100 - asym)) / 100;   /* 非对称 */
		break;
	case DIST_TYPE_HARD:
		gain   = 1024 + (drive * 7  * 1024) / 100;   /* 1..8x */
		th_pos = 3072 + (asym - 50) * 40;
		th_neg = 3072 - (asym - 50) * 40;
		break;
	case DIST_TYPE_FUZZ:
		gain   = 1024 + (drive * 15 * 1024) / 100;   /* 1..16x 极强 */
		th_pos = 4096 + (asym - 50) * 120;           /* 强非对称 -> 法兹偏移 */
		th_neg = 4096 - (asym - 50) * 120;
		break;
	default:
		break;
	}

	for (i = 0; i < n; i++) {
		/* ---- 左声道 ---- */
		int32_t x   = in16[2 * i] + (fb * s_dist.fb_l) / 100;   /* +反馈(自我削波) */
		int32_t x10 = x >> 5;                         /* Q10 归一化 ±1024 */
		int32_t y10;
		if (type == DIST_TYPE_SOFT) {
			int32_t x3 = (x10 * x10) >> 10;           /* Q10 平方 */
			x3 = (x3 * x10) >> 10;                    /* Q10 立方 */
			int32_t kk = (x >= 0) ? kp : kn;
			y10 = x10 - ((kk * x3) >> 10);            /* 软削波 */
		} else {
			int32_t xg = (x10 * gain) >> 10;          /* 预放大 */
			if (xg >= th_pos)            y10 = th_pos;                 /* 硬限幅(正) */
			else if (xg <= th_neg)       y10 = th_neg;                 /* 硬限幅(负) */
			else if (xg > 1024)          y10 = 1024 + ((xg - 1024) * 3) / 4;  /* 软膝 */
			else if (xg < -1024)         y10 = -1024 + ((xg + 1024) * 3) / 4;
			else                         y10 = xg;
		}
		int32_t y   = (y10 * lvl) >> 10;             /* 输出电平 */
		if (y >  32767) y =  32767;
		if (y < -32768) y = -32768;
		s_dist.y_prev_l += ((y - s_dist.y_prev_l) * tone) / 100;   /* tone 一阶低通 */
		out16[2 * i] = (int16_t)s_dist.y_prev_l;
		s_dist.fb_l = y;   /* 更新反馈状态 */

		/* ---- 右声道 ---- */
		x   = in16[2 * i + 1] + (fb * s_dist.fb_r) / 100;   /* +反馈 */
		x10 = x >> 5;
		if (type == DIST_TYPE_SOFT) {
			int32_t x3 = (x10 * x10) >> 10;
			x3 = (x3 * x10) >> 10;
			int32_t kk = (x >= 0) ? kp : kn;
			y10 = x10 - ((kk * x3) >> 10);
		} else {
			int32_t xg = (x10 * gain) >> 10;
			if (xg >= th_pos)            y10 = th_pos;
			else if (xg <= th_neg)       y10 = th_neg;
			else if (xg > 1024)          y10 = 1024 + ((xg - 1024) * 3) / 4;
			else if (xg < -1024)         y10 = -1024 + ((xg + 1024) * 3) / 4;
			else                         y10 = xg;
		}
		y   = (y10 * lvl) >> 10;
		if (y >  32767) y =  32767;
		if (y < -32768) y = -32768;
		s_dist.y_prev_r += ((y - s_dist.y_prev_r) * tone) / 100;
		out16[2 * i + 1] = (int16_t)s_dist.y_prev_r;
		s_dist.fb_r = y;   /* 更新反馈状态 */
	}
}

/**
 * 合唱处理回调 - 系统自带 Chorus 效果器
 * 调用 SDK AudioEffectChorusApply。Chorus 算法仅支持单声道，
 * 这里 L/R 各跑一路实例，分别处理后打包回双声道。
 *   chorus_apply(ct, in, out, n, feedback, dry, wet, mod_rate) 内部完成干湿混合。
 */
void Chorus_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	uint16_t i;
	uint16_t n;
	int16_t *in16;
	int16_t *out16;
	ChorusUnit *cu  = &gCtrlVars.chorus_unit;     /* 左声道实例 */
	ChorusUnit *cuR = &gCtrlVars.chorus_unit_r;   /* 右声道实例 */
	/* 仅在 delay_length / mod_depth 变化时重新初始化 SDK (会清空内部延迟线) */
	static uint8_t last_dl = 0xff;
	static uint8_t last_md = 0xff;
	/* 单声道拆分临时缓冲 */
	static int16_t s_chorus_l[EFFECT_GRAPH_BUFFER_SIZE];
	static int16_t s_chorus_r[EFFECT_GRAPH_BUFFER_SIZE];

	if (in_count < 1 || !in_bufs[0]) {
		if (out_buf) {
			for (i = 0; i < len; i++) out_buf[i] = 0;
		}
		return;
	}

	if (len > EFFECT_GRAPH_BUFFER_SIZE) len = EFFECT_GRAPH_BUFFER_SIZE;
	n    = len;                 /* mono 样本数 = frames */
	in16  = (int16_t *)in_bufs[0];
	out16 = (int16_t *)out_buf;

	/* 同步 apply-time 参数 (每帧生效)；同时把 SDK init 用参数 clamp 到安全范围 */
	uint8_t dl = node->params.chorus.delay_length;
	uint8_t md = node->params.chorus.mod_depth;
	if (dl < 1) dl = 1; else if (dl > 25) dl = 25;            /* SDK 范围 1~25ms */
	if (md > (uint8_t)(dl - 1)) md = (dl > 1) ? (uint8_t)(dl - 1) : 0;  /* mod_depth < delay_length */

	cu->feedback = node->params.chorus.feedback;
	cu->dry      = node->params.chorus.dry;
	cu->wet      = node->params.chorus.wet;
	cu->mod_rate = node->params.chorus.mod_rate;
	cuR->feedback = node->params.chorus.feedback;
	cuR->dry      = node->params.chorus.dry;
	cuR->wet      = node->params.chorus.wet;
	cuR->mod_rate = node->params.chorus.mod_rate;

#if CFG_AUDIO_EFFECT_CHORUS_EN
	if (cu->enable && cu->ct != NULL && cuR->enable && cuR->ct != NULL) {
		/* delay_length / mod_depth 变化 -> 重新初始化两个实例 */
		if (dl != last_dl || md != last_md) {
			cu->delay_length  = dl;
			cu->mod_depth     = md;
			cuR->delay_length = dl;
			cuR->mod_depth    = md;
			AudioEffectChorusInit(cu,  1, gCtrlVars.sample_rate);
			AudioEffectChorusInit(cuR, 1, gCtrlVars.sample_rate);
			last_dl = dl;
			last_md = md;
		}

		/* L/R 拆分 */
		for (i = 0; i < n; i++) {
			s_chorus_l[i] = in16[2 * i];
			s_chorus_r[i] = in16[2 * i + 1];
		}
		/* 各声道独立跑一路实例 (SDK chorus 仅支持 mono) */
		AudioEffectChorusApply(cu,  s_chorus_l, s_chorus_l, n);
		AudioEffectChorusApply(cuR, s_chorus_r, s_chorus_r, n);
		/* 打包回双声道 */
		for (i = 0; i < n; i++) {
			out16[2 * i]     = s_chorus_l[i];
			out16[2 * i + 1] = s_chorus_r[i];
		}
		return;
	}
#endif
	/* 旁路：直接复制 */
	for (i = 0; i < len; i++) {
		out_buf[i] = in_bufs[0][i];
	}
}

/*******************************************************************************
 * 无限延音 (Infinite Sustain) 处理回调
 * 算法三步:
 *   1) 起点检测(Onset): 持续监听输入能量, 检测到能量突增(新音符被弹响)时触发抓取
 *   2) 音高/周期提取(AMDF): 等待攻击(transient)平息后, 用平均幅度差函数
 *      (Average Magnitude Difference Function) 提取一个完整周期的"基因"
 *   3) 相位对齐循环: 以恰好一个周期的波形无缝循环(相位天然对齐),
 *      形成听不出爆音的"无限延音"
 * 进阶处理(让延音更生动):
 *   包络塑形(Slow 模式缓慢淡入) / 幅度调制(缓慢 Tremolo) /
 *   自然衰减滤波(一阶低通) / 分层叠加(第二层基因)
 * 自包含定点 DSP, 无 SDK 依赖, 适合 BP1048 等嵌入式平台。
 * 数据格式: 32位双声道包 [L|R], len = frames (每个 uint32 打包一对 int16)
 ******************************************************************************/
#define SUSTAIN_MIN_PERIOD   32     /* 最小周期(样本), ~1.5kHz @48k */
#define SUSTAIN_MAX_PERIOD   1024   /* 最大周期(样本), ~47Hz @48k (覆盖吉他最低音) */
#define SUSTAIN_AMDF_WIN     256    /* AMDF 比较窗口 */
#define SUSTAIN_CAP_NEED     (SUSTAIN_MAX_PERIOD + SUSTAIN_AMDF_WIN)  /* 1280 */
#define SUSTAIN_Q15          32768
#define SUSTAIN_SR           48000  /* 系统采样率(用于颤音相位推进) */

typedef struct {
	uint8_t  active;        /* 是否已锁定延音 */
	uint8_t  capturing;     /* 正在抓取(等待平息/采集) */
	uint8_t  cap_phase;     /* 0=等待攻击平息 1=采集稳定段 */
	uint8_t  layer_on;      /* 第二层(分层)是否已激活 */
	uint8_t  prev_enable;   /* 上一帧 enable, 用于上升沿复位 */
	uint16_t settle;        /* 攻击平息剩余帧 */
	uint16_t cap_len;       /* 已采集样本数 */

	int16_t  cap[SUSTAIN_CAP_NEED];   /* 采集缓冲(单声道, 纯稳定段) */
	int16_t  cyc[SUSTAIN_MAX_PERIOD];  /* 主层单周期基因 */
	uint16_t cyc_len;
	uint16_t cyc_pos;
	int16_t  cyc2[SUSTAIN_MAX_PERIOD]; /* 第二层单周期基因 */
	uint16_t cyc_len2;
	uint16_t cyc_pos2;

	int32_t  env;           /* 输出包络 Q15 (0~32768) */
	uint16_t tre_phase;     /* 颤音 LFO 相位(0~65535) */
	int32_t  lp;            /* 低通状态 Q0 */
	int32_t  energy_avg;    /* 能量慢速均值(平均绝对值 EMA) */
} SustainState_t;

static SustainState_t s_sustain;   /* 全局单一 sustain 节点状态, 零初始化 */

/**
 * @brief 在采集缓冲上运行 AMDF, 找出最佳周期, 提取一个周期到目标缓冲
 * @note  提取"恰好一个周期"的连续波形并循环, 相位天然对齐(循环点无断裂)
 */
static void Sustain_AnalyzeAndLock(SustainState_t *s, int16_t *dst,
                                   uint16_t *dst_len, uint16_t *dst_pos)
{
	int32_t best_T   = SUSTAIN_MIN_PERIOD;
	int32_t best_val = 0x7fffffff;
	int32_t T, k;

	for (T = SUSTAIN_MIN_PERIOD; T <= SUSTAIN_MAX_PERIOD; T++) {
		int32_t sum = 0;
		const int16_t *a = s->cap;
		for (k = 0; k < SUSTAIN_AMDF_WIN; k++) {
			int32_t d = (int32_t)a[k] - (int32_t)a[k + T];
			if (d < 0) d = -d;
			sum += d;
		}
		if (sum < best_val) {
			best_val = sum;
			best_T   = T;
		}
	}

	/* 提取一个完整周期(相位对齐: 恰好一个周期循环即无缝) */
	{
		uint16_t Tn = (uint16_t)best_T;
		uint16_t j;
		for (j = 0; j < Tn; j++) {
			dst[j] = s->cap[j];
		}
		*dst_len = Tn;
		*dst_pos = 0;
	}
}

void Sustain_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count,
                     uint32_t *out_buf, uint16_t len)
{
	uint16_t i, n;
	int16_t *in16;
	int16_t *out16;
	SustainState_t *s = &s_sustain;

	if (in_count < 1 || !in_bufs[0]) {
		if (out_buf) { for (i = 0; i < len; i++) out_buf[i] = 0; }
		return;
	}
	if (len > EFFECT_GRAPH_BUFFER_SIZE) len = EFFECT_GRAPH_BUFFER_SIZE;
	n    = len;
	in16  = (int16_t *)in_bufs[0];
	out16 = (int16_t *)out_buf;

	/* ---- 参数(实时读取) ---- */
	uint8_t enable      = node->params.sustain.enable;
	uint8_t mode        = node->params.sustain.mode;
	uint8_t sens        = node->params.sustain.sensitivity;
	uint8_t thresh      = node->params.sustain.threshold;
	uint8_t attack_wait = node->params.sustain.attack_wait;
	uint8_t retrigger   = node->params.sustain.retrigger;
	uint8_t layer       = node->params.sustain.layer;
	uint8_t tremolo     = node->params.sustain.tremolo;
	uint8_t trate       = node->params.sustain.tremolo_rate;
	uint8_t filt        = node->params.sustain.filter;
	uint8_t level       = node->params.sustain.level;
	uint8_t mix         = node->params.sustain.mix;

	if (mix > 100) mix = 100;

	/* 节点禁用 或 引擎关闭 -> 直通干声, 不抓取不循环 */
	if (!node->enabled || !enable) {
		for (i = 0; i < len; i++) out_buf[i] = in_bufs[0][i];
		return;
	}

	/* enable 上升沿: 复位捕获/延音状态, 等待新的起音 */
	if (enable && !s->prev_enable) {
		s->active    = 0;
		s->capturing = 0;
		s->layer_on  = 0;
		s->env       = 0;
	}
	s->prev_enable = enable;

	/* 起音检测阈值: 相对能量倍数(Q8) 与 绝对门限 */
	int32_t mult    = 400 + (100 - (int32_t)sens) * 6;   /* 400(1.56x)~1000(3.9x) */
	int32_t abs_min = (int32_t)thresh * 60;              /* 0~6000 */

	/* 颤音 LFO 相位增量(每样本) */
	uint16_t tre_inc = (uint16_t)(((uint32_t)65536 * trate) / 10 / SUSTAIN_SR);
	if (tre_inc == 0) tre_inc = 1;   /* 避免 0 速率导致静止 */

	/* 低通系数(Q14): filt=0 直通, filt=100 强衰减(模拟自然高频衰减) */
	int32_t lp_a = (int32_t)(32767 - (int32_t)filt * 300) >> 1;
	if (lp_a < 1)      lp_a = 1;
	if (lp_a > 16383)  lp_a = 16383;

	/* 输出电平(Q15) */
	int32_t lvl = ((int32_t)level * 32767) / 100;

	/* 包络淡入步长: 瞬态短, 慢速(Slow)长 */
	int32_t fade_steps = (mode == 1) ? 24000 : 1500;

	/* 每帧: 若正在等待攻击平息, 递减 settle 计数 */
	if (s->capturing && s->cap_phase == 0) {
		if (s->settle > 0) s->settle--;
		else { s->cap_phase = 1; s->cap_len = 0; }
	}

	for (i = 0; i < n; i++) {
		int32_t L = in16[2 * i];
		int32_t R = in16[2 * i + 1];
		int32_t mono = (L + R) >> 1;
		int32_t a = (mono < 0) ? -mono : mono;

		/* 能量慢速均值 (EMA, alpha≈1/128) */
		s->energy_avg += (a - s->energy_avg) / 128;

		/* 采集稳定段到 cap */
		if (s->capturing && s->cap_phase == 1 && s->cap_len < SUSTAIN_CAP_NEED) {
			s->cap[s->cap_len++] = (int16_t)mono;
			if (s->cap_len >= SUSTAIN_CAP_NEED) {
				/* 分析并锁定周期 */
				if (s->active && layer) {
					Sustain_AnalyzeAndLock(s, s->cyc2, &s->cyc_len2, &s->cyc_pos2);
					s->layer_on = 1;
				} else if (s->active && retrigger) {
					Sustain_AnalyzeAndLock(s, s->cyc, &s->cyc_len, &s->cyc_pos);
					s->env = 0;   /* 重抓淡入 */
				} else {
					Sustain_AnalyzeAndLock(s, s->cyc, &s->cyc_len, &s->cyc_pos);
					s->active = 1;
					s->env = 0;    /* 首次淡入 */
				}
				s->capturing = 0;
			}
		}

		/* 起音检测(每样本): 触发新一轮抓取
		 * 条件: 未抓取中, 且(未锁定 / 已锁定且允许重抓 / 已锁定且允许分层) */
		if (!s->capturing &&
		    (s->active == 0 || (retrigger && s->active) || (layer && s->active))) {
			int32_t req = (s->energy_avg * mult) >> 8;
			if (a > req && a > abs_min) {
				s->capturing = 1;
				s->cap_phase = 0;
				s->settle    = attack_wait;
				s->cap_len   = 0;
			}
		}

		/* ---- 输出 ---- */
		if (s->capturing) {
			/* 抓取期间: 直通干声, 避免爆音/突变 */
			out16[2 * i]     = (int16_t)L;
			out16[2 * i + 1] = (int16_t)R;
		} else if (s->active && s->cyc_len > 0) {
			int32_t dryv = mono;
			int32_t sus = (int32_t)s->cyc[s->cyc_pos];
			s->cyc_pos = (s->cyc_pos + 1) % s->cyc_len;

			/* 分层叠加(第二层基因) */
			if (s->layer_on && s->cyc_len2 > 0) {
				int32_t v2 = (int32_t)s->cyc2[s->cyc_pos2];
				s->cyc_pos2 = (s->cyc_pos2 + 1) % s->cyc_len2;
				sus += (v2 >> 1);
				if (sus >  32767) sus =  32767;
				if (sus < -32768) sus = -32768;
			}

			/* 自然衰减低通(一阶) — 模拟真实乐器的自然高频衰减 */
			s->lp += ((sus - s->lp) * lp_a) / 16384;
			sus = s->lp;

			/* 颤音(Tremolo): 三角波 LFO 模拟长时 sustain 的音量微动 */
			s->tre_phase += tre_inc;
			{
				uint16_t ph = s->tre_phase;
				int32_t tri = (ph & 0x8000) ? (int32_t)(65535 - ph) : (int32_t)ph; /* 0~32767 */
				int32_t tf  = SUSTAIN_Q15 - ((int32_t)tremolo * tri) / 100;        /* Q15 */
				sus = (sus * tf) >> 15;
			}

			/* 包络淡入(Slow 模式更长, 避免突兀开始) */
			if (s->env < SUSTAIN_Q15) {
				s->env += (SUSTAIN_Q15 - s->env) / fade_steps;
				if (s->env > SUSTAIN_Q15) s->env = SUSTAIN_Q15;
			}
			sus = (sus * s->env) >> 15;

			/* 输出电平 */
			sus = (sus * lvl) >> 15;

			/* 干湿混合(mix=100 纯延音, mix<100 可叠加实时演奏) */
			{
				int32_t outv = (dryv * (100 - (int32_t)mix) + sus * (int32_t)mix) / 100;
				if (outv >  32767) outv =  32767;
				if (outv < -32768) outv = -32768;
				out16[2 * i]     = (int16_t)outv;
				out16[2 * i + 1] = (int16_t)outv;
			}
		} else {
			/* 未锁定且无抓取: 直通干声 */
			out16[2 * i]     = (int16_t)L;
			out16[2 * i + 1] = (int16_t)R;
		}
	}
}
