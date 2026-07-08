/**
 * @file bg_graph_setup.c
 * @brief Effect Graph callback registration and public audio IO helpers.
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_internal.h"
#include "product_def.h"
#include "debug.h"

#include "effect_graph.h"
#include "effect_graph_config.h"
#include "ctrlvars.h"
#include "rtos_api.h"
#if BANGTSYNTH_EN
#include "bangtsynth_node.h"
#endif

void BG_AudioIO_SetUsbOutVolume(uint8_t vol, uint8_t mute)
{
	s_usb_out_gain_q8 = (uint16_t)((uint32_t)vol * 256 / 100);
	s_usb_out_mute = mute ? 1 : 0;
	DBG("[Audio] USB out: vol=%d%% gain_q8=%d mute=%d\n", vol, s_usb_out_gain_q8, s_usb_out_mute);
}

/**
 * @brief 关机前释放大内存效果器（混响），为提示音 pvPortMalloc 腾出堆空间
 * @note  必须在 RemindSound_Start("off") 之前调用。
 *        指针清零并关闭 enable，ISR 中的 ReverbApply 会安全跳过。
 */
void BG_AudioIO_PrepareForShutdown(void)
{
    if (gCtrlVars.reverb_unit.ct != NULL) {
        gCtrlVars.reverb_unit.enable = 0;
        osPortFree(gCtrlVars.reverb_unit.ct);
        gCtrlVars.reverb_unit.ct = NULL;
        DBG("[Audio] Reverb freed for shutdown sound\n");
    }
}

void BG_AudioIO_SetupEffectGraphCallbacks(void)
{
	EffectGraphRuntime_t *graph = EffectGraph_GetInstance();
	EffectNode_t *node;
	uint8_t i;
	uint8_t adc_mixer_found = 0;  /* 第一个 mixer 被当作 adc_mixer */
	
	DBG("[Audio] Setting up Effect Graph callbacks (by type)...\n");
	
	if (!graph) {
		DBG("[Audio] ERROR: No graph instance!\n");
		return;
	}
	
	/* 遍历所有节点，基于 type 注册回调，不依赖 name */
	for (i = 0; i < graph->node_count; i++) {
		node = &graph->nodes[i];
		
		switch (node->type) {
		/* ===== 源节点 ===== */
		case EFFECT_NODE_TYPE_SOURCE_ADC0:
			node->func.source = ADC0_ReadGuitarData;
			node->avail_func = ADC0_GetAvailableData;
			DBG("[Audio] [%d] %s -> ADC0 guitar\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_SOURCE_ADC1:
			node->func.source = ADC1_ReadMicData;
			node->avail_func = ADC1_GetAvailableData;
			DBG("[Audio] [%d] %s -> ADC1 mic\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_SOURCE_USB_IN:
			node->func.source = USB_ReadAudioData;
			node->avail_func = USB_GetAvailableData;
			DBG("[Audio] [%d] %s -> USB in\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_SOURCE_BT_IN:
			node->func.source = BT_ReadAudioData;
			node->avail_func = BT_GetAvailableData;
			DBG("[Audio] [%d] %s -> BT in\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_SOURCE_METRONOME:
			node->func.source = Metronome_SourceCallback;
			node->avail_func = Metronome_GetAvailCallback;
			DBG("[Audio] [%d] %s -> Metronome\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_SOURCE_REMIND:
			node->func.source = Remind_SourceCallback;
			node->avail_func = Remind_GetAvailCallback;
			DBG("[Audio] [%d] %s -> Remind\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY:
			node->func.source = LooperPlay_SourceCallback;
			node->avail_func = LooperPlay_GetAvailCallback;
			DBG("[Audio] [%d] %s -> Looper play\n", i, node->name);
			break;
			
#if BANGTSYNTH_EN
		case EFFECT_NODE_TYPE_SOURCE_SYNTH:
			node->func.source = BanGTsynth_SourceCallback;
			node->avail_func = BanGTsynth_GetAvailCallback;
			DBG("[Audio] [%d] %s -> Synth\n", i, node->name);
			break;
#endif
			
		/* ===== 输出节点 ===== */
		case EFFECT_NODE_TYPE_SINK_DAC0:
			node->func.sink = DAC0_WriteSpeakerData;
			DBG("[Audio] [%d] %s -> DAC0 out\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_SINK_USB_OUT:
			node->func.sink = USB_WriteAudioData;
			DBG("[Audio] [%d] %s -> USB out\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_SINK_LOOPER_RECORD:
			node->func.sink = LooperRecord_SinkCallback;
			DBG("[Audio] [%d] %s -> Looper record\n", i, node->name);
			break;
			
		/* ===== 效果器节点 ===== */
		case EFFECT_NODE_TYPE_EFFECT_EQ:
			node->func.process = EQ_Process;
			DBG("[Audio] [%d] %s -> EQ\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_EFFECT_DRC:
			node->func.process = DRC_Process;
			DBG("[Audio] [%d] %s -> DRC\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_EFFECT_REVERB:
			node->func.process = Reverb_Process;
			DBG("[Audio] [%d] %s -> Reverb\n", i, node->name);
			break;
			
		case EFFECT_NODE_TYPE_EFFECT_EXPANDER:
			node->func.process = Expander_Process;
			DBG("[Audio] [%d] %s -> Expander\n", i, node->name);
			break;
			
		/* ===== 混音器节点 ===== */
		case EFFECT_NODE_TYPE_MIXER:
			/* 第一个 mixer 如果有 4 个输入则用 ADC_Mixer_Process（合并4个单声道EQ到双声道）
			 * 其余 mixer 用通用 Mixer_Process */
			if (!adc_mixer_found && node->input_count >= 4) {
				node->func.process = ADC_Mixer_Process;
				adc_mixer_found = 1;
				DBG("[Audio] [%d] %s -> ADC Mixer (4-ch)\n", i, node->name);
			} else {
				node->func.process = Mixer_Process;
				DBG("[Audio] [%d] %s -> Mixer\n", i, node->name);
			}
			break;
			
		/* ===== 其他效果器节点 ===== */
		case EFFECT_NODE_TYPE_EFFECT_HOWLING:
		case EFFECT_NODE_TYPE_EFFECT_NOISE_GATE:
		case EFFECT_NODE_TYPE_EFFECT_GAIN:
		case EFFECT_NODE_TYPE_EFFECT_DELAY:
		case EFFECT_NODE_TYPE_EFFECT_CHORUS:
		case EFFECT_NODE_TYPE_LOOPER:
			node->func.process = Passthrough_Process;
			DBG("[Audio] [%d] %s -> Passthrough\n", i, node->name);
			break;
			
		default:
			DBG("[Audio] [%d] %s -> Unknown type %d\n", i, node->name, node->type);
			break;
		}
	}
	
	DBG("[Audio] All %d node callbacks registered by type\n", graph->node_count);
}
