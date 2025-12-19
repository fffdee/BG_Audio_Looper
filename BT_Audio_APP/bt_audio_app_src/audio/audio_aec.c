/**
 **************************************************************************************
 * @file    audio_aec.c
 * @brief
 *
 * @author  Sam
 * @version V1.0.0
 *
 * $Created: 2021-3-12 15:42:47$
 *
 * @Copyright (C) 2021, Shanghai Mountain View Silicon Co.,Ltd. All rights reserved.
 **************************************************************************************
 */
#include "string.h"
#include "type.h"
#include "app_config.h"
#include "audio_aec.h"
#include "ctrlvars.h"
#include "blue_ns_core.h"
#include "rtos_api.h"
#include "mvstdio.h"

/*******************************************************************************
 * AEC
 * 用于进行AEC的缓存远端发送数据
 * uint:sample
 ******************************************************************************/
void Audio_AECEffectInit(uint32_t echoLevel, uint32_t noiseLevel)
{
	gCtrlVars.mic_aec_unit.enable				 = 1;
	gCtrlVars.mic_aec_unit.es_level    			 = echoLevel;//BT_HFP_AEC_ECHO_LEVEL;
	gCtrlVars.mic_aec_unit.ns_level     		 = noiseLevel;//BT_HFP_AEC_NOISE_LEVEL;
	AudioEffectAecInit(&gCtrlVars.mic_aec_unit, 1, 16000);//固定为16K采样率

	// aec expander init
	AudioEffectExpanderInit(&gCtrlVars.expander_for_aec_unit, 2, 16000);

	// aec eq init
	AudioEffectEQInit(&gCtrlVars.eq_for_aec_unit, 2, 16000);
	
	// aec drc init
	gCtrlVars.drc_for_aec_unit.channel = 1;
	AudioEffectDRCInit(&gCtrlVars.drc_for_aec_unit, 1, 16000);
}

bool Audio_AECInit(AudioAECContext* AecCt)
{
	if(!AecCt->AecDelayBuf)
		AecCt->AecDelayBuf = (uint8_t*)osPortMalloc(BLK_LEN*LEN_PER_SAMPLE*MAX_DELAY_BLOCK);  //64*2*32

	if(AecCt->AecDelayBuf == NULL)
	{
		return FALSE;
	}
	memset(AecCt->AecDelayBuf, 0, (BLK_LEN*LEN_PER_SAMPLE*MAX_DELAY_BLOCK));

	AecCt->AecDelayRingBuf.addr = AecCt->AecDelayBuf;
	AecCt->AecDelayRingBuf.mem_capacity = (BLK_LEN*LEN_PER_SAMPLE*MAX_DELAY_BLOCK);
	AecCt->AecDelayRingBuf.mem_len = (BLK_LEN*LEN_PER_SAMPLE*DEFAULT_DELAY_BLK);
	AecCt->AecDelayRingBuf.p = 0;
	AecCt->AecDelayRingBuf.cb = NULL;
	AecCt->SourceBuf_Aec = (uint16_t*)osPortMalloc(CFG_BTHF_PARA_SAMPLES_PER_FRAME * 2);
	if(AecCt->SourceBuf_Aec == NULL)
	{
		return FALSE;
	}
	memset(AecCt->SourceBuf_Aec, 0, CFG_BTHF_PARA_SAMPLES_PER_FRAME * 2);
	return TRUE;
}

void Audio_AECReset(AudioAECContext* AecCt, uint32_t DelayBlock)
{
	memset(AecCt->AecDelayBuf, 0, (BLK_LEN*LEN_PER_SAMPLE*MAX_DELAY_BLOCK));

	AecCt->AecDelayRingBuf.addr = AecCt->AecDelayBuf;

	AecCt->AecDelayRingBuf.mem_capacity = (BLK_LEN*LEN_PER_SAMPLE*MAX_DELAY_BLOCK);
	AecCt->AecDelayRingBuf.mem_len = BLK_LEN*LEN_PER_SAMPLE*DelayBlock;

	AecCt->AecDelayRingBuf.p = 0;
	AecCt->AecDelayRingBuf.cb = NULL;

	memset(AecCt->SourceBuf_Aec, 0, CFG_BTHF_PARA_SAMPLES_PER_FRAME * 2);
}

void Audio_AECDeinit(AudioAECContext* AecCt)
{
	if(AecCt->SourceBuf_Aec)
	{
		osPortFree(AecCt->SourceBuf_Aec);
		AecCt->SourceBuf_Aec = NULL;
	}

	AecCt->AecDelayRingBuf.addr = NULL;
	AecCt->AecDelayRingBuf.mem_capacity = 0;
	AecCt->AecDelayRingBuf.mem_len = 0;
	AecCt->AecDelayRingBuf.p = 0;
	AecCt->AecDelayRingBuf.cb = NULL;

	if(AecCt->AecDelayBuf)
	{
		osPortFree(AecCt->AecDelayBuf);
		AecCt->AecDelayBuf = NULL;
	}

	// aec expander deinit
	gCtrlVars.expander_for_aec_unit.enable = 0;
	if(gCtrlVars.expander_for_aec_unit.ct != NULL)
	{
		osPortFree(gCtrlVars.expander_for_aec_unit.ct);
		gCtrlVars.expander_for_aec_unit.ct  = NULL;
	}

	// aec eq deinit
	gCtrlVars.eq_for_aec_unit.enable = 0;
	if(gCtrlVars.eq_for_aec_unit.ct != NULL)
	{
		osPortFree(gCtrlVars.eq_for_aec_unit.ct);
		gCtrlVars.eq_for_aec_unit.ct  = NULL;
	}

	// aec drc deinit
	gCtrlVars.drc_for_aec_unit.channel = 2;
	gCtrlVars.drc_for_aec_unit.enable = 0;
	if(gCtrlVars.drc_for_aec_unit.ct != NULL)
	{
		osPortFree(gCtrlVars.drc_for_aec_unit.ct);
		gCtrlVars.drc_for_aec_unit.ct  = NULL;
	}
}


uint32_t Audio_AECRingDataSet(AudioAECContext* AecCt, void *InBuf, uint16_t InLen)
{
	if(InLen == 0)
		return 0;

	return mv_mwrite(InBuf, 1, InLen*2, &AecCt->AecDelayRingBuf);
}

uint32_t Audio_AECRingDataGet(AudioAECContext* AecCt, void* OutBuf, uint16_t OutLen)
{
	if(OutLen == 0)
		return 0;

	return mv_mread(OutBuf, 1, OutLen*2, &AecCt->AecDelayRingBuf);
}

int32_t Audio_AECRingDataSpaceLenGet(AudioAECContext* AecCt)
{
	return mv_mremain(&AecCt->AecDelayRingBuf)/2;
}

int32_t Audio_AECRingDataLenGet(AudioAECContext* AecCt)
{
	return mv_msize(&AecCt->AecDelayRingBuf)/2;
}

int16_t *Audio_AecInBuf(AudioAECContext* AecCt, uint16_t OutLen)
{
	if(Audio_AECRingDataLenGet(AecCt) >= OutLen)
	{
		Audio_AECRingDataGet(AecCt, AecCt->SourceBuf_Aec , OutLen);
	}
	else
	{
		memset(AecCt->SourceBuf_Aec, 0, OutLen * 2);
	}
	return (int16_t *)AecCt->SourceBuf_Aec;
}

