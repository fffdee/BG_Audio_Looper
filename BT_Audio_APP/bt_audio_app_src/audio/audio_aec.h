/**
 **************************************************************************************
 * @file    audio_aec.h
 * @brief
 *
 * @author  Sam
 * @version V1.0.0
 *
 * $Created: 2021-3-12 11:17:21$
 *
 * @Copyright (C) 2021, Shanghai Mountain View Silicon Co.,Ltd. All rights reserved.
 **************************************************************************************
 */
#ifndef __AUDIO_AEC_H__
#define __AUDIO_AEC_H__

#include "typedefine.h"
#include "blue_ns_core.h"

typedef struct _AudioAECContext
{
	uint8_t				*AecDelayBuf;		//BLK_LEN*LEN_PER_SAMPLE*MAX_DELAY_BLOCK
	MemHandle			AecDelayRingBuf;
	uint16_t			*SourceBuf_Aec;

}AudioAECContext;

#define BT_HFP_AEC_ECHO_LEVEL			3 //Echo suppression level: 0(min)~5(max)
#define BT_HFP_AEC_NOISE_LEVEL			0 //Noise suppression level: 0(min)~5(max)

#define USB_PHONE_AEC_ECHO_LEVEL		5 //Echo suppression level: 0(min)~5(max)
#define USB_PHONE_AEC_NOISE_LEVEL		1 //Noise suppression level: 0(min)~5(max)

//AEC
#define FRAME_SIZE					BLK_LEN
#define AEC_SAMPLE_RATE				16000
#define LEN_PER_SAMPLE				2 //mono
#define DELAY_LEN_PER_BLOCK			(64)
#define MAX_DELAY_BLOCK				32//BT_HFP_AEC_MAX_DELAY_BLK
#define DEFAULT_DELAY_BLK			8//BT_HFP_AEC_DELAY_BLK


void Audio_AECEffectInit(uint32_t echoLevel, uint32_t noiseLevel);
bool Audio_AECInit(AudioAECContext* AecCt);
void Audio_AECReset(AudioAECContext* AecCt, uint32_t DelayBlock);
void Audio_AECDeinit(AudioAECContext* AecCt);
uint32_t Audio_AECRingDataSet(AudioAECContext* AecCt, void *InBuf, uint16_t InLen);
uint32_t Audio_AECRingDataGet(AudioAECContext* AecCt, void* OutBuf, uint16_t OutLen);
int32_t Audio_AECRingDataSpaceLenGet(AudioAECContext* AecCt);
int32_t Audio_AECRingDataLenGet(AudioAECContext* AecCt);
int16_t *Audio_AecInBuf(AudioAECContext* AecCt, uint16_t OutLen);

#endif//__AUDIO_AEC_H__
