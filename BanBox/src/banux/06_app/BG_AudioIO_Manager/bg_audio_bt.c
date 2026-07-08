/**
 * @file bg_audio_bt.c
 * @brief Bluetooth A2DP SBC decoder bridge.
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_internal.h"
#include "product_def.h"
#include "debug.h"

#include "audio_decoder_api.h"
#include "bt_app_interface.h"
#include "bt_stack_service.h"

void A2dp_DecoderInit(void)
{
	memset(a2dp_sbcBuf, 0, BT_SBC_DECODER_INPUT_LEN);
	SBC_MemHandle.addr = a2dp_sbcBuf;
	SBC_MemHandle.mem_capacity = BT_SBC_DECODER_INPUT_LEN;
	SBC_MemHandle.mem_len = 0;
	SBC_MemHandle.p = 0;
	SaveA2dpStreamDataToBuffer = SaveDataToSbcBuffer;
}
/**
 * 保存SBC数据到缓冲区
 */
void SaveDataToSbcBuffer(uint8_t *data, uint16_t dataLen)
{
	uint32_t insertLen = 0;
	int32_t remainLen = 0;

	remainLen = mv_mremain(&SBC_MemHandle);
	if (BT_SBC_DECODER_INPUT_LEN - remainLen > BT_SBC_LEVEL_LOW)
	{
		// 缓冲区水位过高
	}
	if (remainLen <= (dataLen + 8))
	{
		BT_DBG("F");
		return;
	}

	insertLen = mv_mwrite(data, dataLen, 1, &SBC_MemHandle);
	if (BT_SBC_DECODER_INPUT_LEN - remainLen < BT_SBC_DECODER_INPUT_LEN >> 3)
	{
		// 缓冲区数据足够时通知解码器
	}

	if (insertLen != dataLen)
	{
		DBG("insert data len err! i:%ld,d:%d\n", insertLen, dataLen);
	}

	if (!DecoderInitialized)
	{
		int32_t ret = audio_decoder_initialize(decoder_buf, &SBC_MemHandle, (int32_t)IO_TYPE_MEMORY, MSBC_DECODER);
		if (ret != RT_SUCCESS)
			printf(" error audio_decoder_initialize %ld\n", (long)ret);
		else
			DecoderInitialized = 1;
	}
}
