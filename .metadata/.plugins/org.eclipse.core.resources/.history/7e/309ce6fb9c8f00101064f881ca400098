/**
 **************************************************************************************
 * @file    Demo_BT.c
 * @brief   BT Demo
 *          BT演示工程，主要演示BT的音乐播放、通话功能
 *
 * @author  KK
 * @version V1.0.0
 *
 * $Created: 2023-12-31 11:30:00$
 *
 * @copyright Shanghai Mountain View Silicon Technology Co.,Ltd. All rights reserved.
 **************************************************************************************
 */

#include <stdlib.h>
#include <nds32_intrinsic.h>
#include <string.h>
#include "uarts.h"
#include "uarts_interface.h"
#include "type.h"
#include "debug.h"
#include "timeout.h"
#include "clk.h"
#include "dma.h"
#include "dac.h"
#include "timer.h"
#include "adc.h"
#include "i2s.h"
#include "watchdog.h"
#include "spi_flash.h"
#include "remap.h"
#include "audio_adc.h"
#include "gpio.h"
#include "chip_info.h"
#include "adc_interface.h"
#include "dac_interface.h"

#include "bt_stack_service.h"
#include "blue_aec.h"

#include "typedefine.h"
#include "mvstdio.h"
#include "audio_decoder_api.h"
#include "bt_hfp_api.h"

#define SYSTEM_SAMPLE_RATE		44100

uint32_t system_mode = 0;//0=bt play; 1=bt call;

extern void __c_init_rom(void);

// 定义2个全局buf，用于缓存ADC和DAC的数据，注意单位
uint32_t AudioADC1Buf[1024] = {0}; // 1024 * 4 = 4K
uint32_t AudioDACBuf[1024] = {0};  // 1024 * 4 = 4K

#define BT_SBC_PACKET_SIZE					595
#define BT_SBC_DECODER_INPUT_LEN			(20*1024)
#define BT_SBC_LEVEL_HIGH					(BT_SBC_DECODER_INPUT_LEN - BT_SBC_PACKET_SIZE * 4)
#define BT_SBC_LEVEL_LOW					(BT_SBC_PACKET_SIZE *6)//(BT_SBC_LEVEL_HIGH  - BT_SBC_PACKET_SIZE * 3)
#define BT_SBC_LEVEL_START					(BT_SBC_LEVEL_HIGH  - BT_SBC_PACKET_SIZE * 3)
#define SBC_DECODER_FIFO_MIN				(119*2)

uint8_t  a2dp_sbcBuf[BT_SBC_DECODER_INPUT_LEN];

uint8_t  hfp_msbcBuf[20*1024];

static uint8_t dac0_dma_buffer[5120 * 4] = {0};

AudioDecoderContext *audio_decoder = NULL;
uint8_t  audio_decoder_buf[6*1024];

MemHandle SBC_MemHandle;

AudioDecoderContext msbc_audio_decoder = {0};
MemHandle MSBC_MemHandle;

static uint8_t DecoderInitialized = 0;
static uint8_t MsbcDecoderInitialized = 0;

#define BLK_LEN                             128         //copy from blus_ns_core.h

/*
 * 通话相关配置
 */
//AEC相关参数配置 (MIC gain, AGC, DAC gain, 降噪参数)
#define BT_HFP_AEC_ENABLE

//MIC无运放,使用如下参数(参考)
#define BT_HFP_MIC_PGA_GAIN				6//2  //ADC PGA GAIN +18db(0~31, 0:max, 31:min)
#define BT_HFP_MIC_DIGIT_GAIN			30000
#define BT_HFP_INPUT_DIGIT_GAIN			2000

#define BT_HFP_AEC_ECHO_LEVEL			4 //Echo suppression level: 0(min)~5(max)
#define BT_HFP_AEC_NOISE_LEVEL			1 //Noise suppression level: 0(min)~5(max)

#define BT_HFP_AEC_MAX_DELAY_BLK		32
#define BT_HFP_AEC_DELAY_BLK			0//2//10 //MIC无运放参考值
//#define BT_HFP_AEC_DELAY_BLK			14 //MIC有运放参考值(参考开发板)

//AEC
#define FRAME_SIZE					BLK_LEN
#define AEC_SAMPLE_RATE				16000
#define LEN_PER_SAMPLE				2 //mono
#define MAX_DELAY_BLOCK				BT_HFP_AEC_MAX_DELAY_BLK
#define DEFAULT_DELAY_BLK			BT_HFP_AEC_DELAY_BLK

#define BT_MSBC_DECODER_INPUT_LEN	2*1024
#define MSBC_DECODER_FIFO_MIN		10*57

//发送memory和fifo
MemHandle	HfSend_MemHandle;
uint8_t		HfSend_fifo[1024];

uint8_t hfpSendBuf[120];

int16_t pcm_fifo[1024]={0};
int16_t cvsd_fifo[240]={0};
uint32_t micAdcBuf[512];
int16_t pcmDelayBuf[256];
int16_t micAecBuf[256];

void A2dp_DecoderInit(void);
void A2dp_ReceiveData(uint8_t * data, uint16_t dataLen);

//aec
typedef struct __AecUnit
{
	BlueAECContext 		 ct;
	uint32_t 			 enable;
	int32_t 			 es_level;
//	int32_t 		     ns_level;
	uint32_t   	 		 param_cnt;
	uint8_t              channel;

} AecUnit;
AecUnit		mic_aec_unit;

#define ONE_BLOCK_WRITE 2048
MemHandle aec_debug_fifo;
//static uint8_t aec_debug_raw_buf[4096*8];
//static char current_vol[8];//disk volume like 0:/, 1:/
uint8_t aec_temp_buf[ONE_BLOCK_WRITE];
int16_t aec_temp_buf1[256*2];
uint8_t hfp_status_for_aec_debug = 0;
bool has_sdcard = FALSE;
char file_string[64];

static uint8_t DmaChannelMap[] =
{
    PERIPHERAL_ID_AUDIO_ADC0_RX,
    PERIPHERAL_ID_AUDIO_ADC1_RX,
    PERIPHERAL_ID_AUDIO_DAC0_TX,
    255,
    255,
    255,
};

////////////////////////////////////////////////////////////////////////////////////////////////
/*******************************************************************************
 * AEC
 * 用于进行AEC的缓存远端发送数据
 * uint:sample
 ******************************************************************************/
 //aec
uint8_t				AecDelayBuf[BLK_LEN*LEN_PER_SAMPLE*MAX_DELAY_BLOCK];
MemHandle			AecDelayRingBuf;
uint16_t			SourceBuf_Aec[BLK_LEN * 2];

//extern void aec_help_effect_Init(void);
void BtHf_AECEffectInit(void)
{
	mic_aec_unit.enable				 = 1;
	mic_aec_unit.es_level    		 = BT_HFP_AEC_ECHO_LEVEL;
//	mic_aec_unit.ns_level     		 = BT_HFP_AEC_NOISE_LEVEL;
	mic_aec_unit.channel			 = 1;

	blue_aec_init(&mic_aec_unit.ct, mic_aec_unit.es_level);
//	aec_help_effect_Init();
}

bool BtHf_AECInit(void)
{
	BtHf_AECEffectInit();
	memset(AecDelayBuf, 0, sizeof(AecDelayBuf));

	AecDelayRingBuf.addr = AecDelayBuf;
	AecDelayRingBuf.mem_capacity = (BLK_LEN*LEN_PER_SAMPLE*MAX_DELAY_BLOCK);
	AecDelayRingBuf.mem_len = BLK_LEN*LEN_PER_SAMPLE*DEFAULT_DELAY_BLK;
	AecDelayRingBuf.p = 0;

	memset(SourceBuf_Aec, 0, sizeof(SourceBuf_Aec));
	return TRUE;
}

void BtHf_AECReset(void)
{
	memset(AecDelayBuf, 0, sizeof(AecDelayBuf));

	AecDelayRingBuf.addr = AecDelayBuf;
	AecDelayRingBuf.mem_capacity = (BLK_LEN*LEN_PER_SAMPLE*MAX_DELAY_BLOCK);
	AecDelayRingBuf.mem_len = BLK_LEN*LEN_PER_SAMPLE*DEFAULT_DELAY_BLK;
	AecDelayRingBuf.p = 0;

	memset(SourceBuf_Aec, 0, sizeof(SourceBuf_Aec));
}

uint32_t BtHf_AECRingDataSet(void *InBuf, uint16_t InLen)
{
	if(InLen == 0)
		return 0;

	return mv_mwrite(InBuf, 1, InLen*2, &AecDelayRingBuf);
}

uint32_t BtHf_AECRingDataGet(void* OutBuf, uint16_t OutLen)
{
	if(OutLen == 0)
		return 0;

	return mv_mread(OutBuf, 1, OutLen*2, &AecDelayRingBuf);
}

int32_t BtHf_AECRingDataSpaceLenGet(void)
{
	return mv_mremain(&AecDelayRingBuf)/2;
}

int32_t BtHf_AECRingDataLenGet(void)
{
	return mv_msize(&AecDelayRingBuf)/2;
}

int16_t *BtHf_AecInBuf(void)
{
	if(BtHf_AECRingDataLenGet() > BLK_LEN)
	{
		BtHf_AECRingDataGet(SourceBuf_Aec , BLK_LEN);
	}
	else
	{
		memset(SourceBuf_Aec, 0, sizeof(SourceBuf_Aec));
	}
	return (int16_t *)SourceBuf_Aec;
}


/****************************************************************************************
 * A2DP process
 ***************************************************************************************/
void A2dp_DecoderInit(void)
{
	memset(a2dp_sbcBuf, 0, BT_SBC_DECODER_INPUT_LEN);

	SBC_MemHandle.addr = a2dp_sbcBuf;
	SBC_MemHandle.mem_capacity = BT_SBC_DECODER_INPUT_LEN;
	SBC_MemHandle.mem_len = 0;
	SBC_MemHandle.p = 0;
	
	//dac
    DACParamCt ct;
    ct.DACModel = DAC_Single;
    ct.DACLoadStatus = DAC_NOLoad;
    ct.PVDDModel = PVDD33;
    ct.DACEnergyModel = DACCommonEnergy;

	audio_decoder = audio_decoder_buf;
    //初始化DAC
    AudioDAC_Init(&ct, SYSTEM_SAMPLE_RATE, 16, dac0_dma_buffer, sizeof(dac0_dma_buffer), NULL, 0);
}


void A2dp_ReceiveData(uint8_t * data, uint16_t dataLen)
{
	uint32_t	insertLen = 0;
	int32_t		remainLen = 0;

	if(1/*sbcDecoderInitFlag*/)
	{
		remainLen = mv_mremain(&SBC_MemHandle);
		if(BT_SBC_DECODER_INPUT_LEN - remainLen > BT_SBC_LEVEL_LOW ) //水位过门槛后
		{

		}
		if(remainLen <= (dataLen+8))
		{
			printf("F");//sbc fifo full
			//增加读指针
			//
			return ;
		}
		insertLen = mv_mwrite(data, dataLen, 1,&SBC_MemHandle);
		if(BT_SBC_DECODER_INPUT_LEN - remainLen < BT_SBC_DECODER_INPUT_LEN >> 3)
		{//fifo数据不足时，填入数据及时通知decoder

		}
		if(insertLen != dataLen)
		{
			printf("insert data len err! i:%ld,d:%d\n", insertLen, dataLen);
		}

		if( !DecoderInitialized )
		{
			int32_t ret = audio_decoder_initialize(audio_decoder, &SBC_MemHandle, (int32_t)IO_TYPE_MEMORY, SBC_DECODER);
			if( ret != RT_SUCCESS )
			{
				printf(" error audio_decoder_initialize %x\n", ret);
			}
			else
			{
				DecoderInitialized = 1;
			}
		}
	}

}

void A2dp_Decode(void)
{
	static uint32_t SampleRateCC = 44100;

	if(DecoderInitialized && RT_SUCCESS == audio_decoder_can_continue(audio_decoder) )
	{
		if(mv_msize(&SBC_MemHandle) <= SBC_DECODER_FIFO_MIN)
		{
			return;
		}
		if(audio_decoder_decode(audio_decoder) == RT_SUCCESS)
		{
			/*if( SYSTEM_SAMPLE_RATE != audio_decoder.song_info.sampling_rate)
			{
				SampleRateCC = audio_decoder.song_info.sampling_rate;
//				AudioDAC_SampleRateChange(DAC0, audio_decoder.song_info.sampling_rate);
				AudioDAC0_SampleRateChange(44100);//恢复
			}*/

            AudioDAC0_DataSet(audio_decoder->song_info.pcm_addr, audio_decoder->song_info.pcm_data_length);
		}
		else
		{
			printf("[INFO]: ERROR%d\n", (int)audio_decoder->error_code);
		}
	}
}

/****************************************************************************************
 * HFP process
 ***************************************************************************************/
void Msbc_DecoderInit(void)
{
	memset(hfp_msbcBuf, 0, sizeof(hfp_msbcBuf));

	memset(&msbc_audio_decoder, 0, sizeof(AudioDecoderContext));

	MSBC_MemHandle.addr = hfp_msbcBuf;
	MSBC_MemHandle.mem_capacity = sizeof(hfp_msbcBuf);
	MSBC_MemHandle.mem_len = 0;
	MSBC_MemHandle.p = 0;

	//发送fifo(cvsd:pcm / msbc编码)
	memset(HfSend_fifo, 0, 2*1024);
	HfSend_MemHandle.addr = HfSend_fifo;
	HfSend_MemHandle.mem_capacity = 2*1024;
	HfSend_MemHandle.mem_len = 0;
	HfSend_MemHandle.p = 0;

	MsbcDecoderInitialized = 1;
	BtHf_AECInit();
}

void Msbc_Decode(void)
{
	if(AudioADC1_DataLenGet() >= BLK_LEN)
	{
		AudioADC1_DataGet(micAdcBuf, BLK_LEN);

		if(!MsbcDecoderInitialized)
		{
			return;
		}

		if(BtHf_AECRingDataLenGet()>=BLK_LEN)
		{
			BtHf_AECRingDataGet(pcmDelayBuf,BLK_LEN);
			blue_aec_run(&mic_aec_unit.ct,  (int16_t *)(pcmDelayBuf), (int16_t *)(micAdcBuf), (int16_t *)(micAecBuf));
			mv_mwrite(micAecBuf, BLK_LEN*2, 1, &HfSend_MemHandle);
		}
	}

	//dac out
	if(AudioDAC0_DataSpaceLenGet() >= BLK_LEN)
	{
		uint8_t i;
		int32_t memLen = mv_msize(&MSBC_MemHandle);
		if(memLen > BLK_LEN*2)
		{
			mv_mread(cvsd_fifo, BLK_LEN*2, 1, &MSBC_MemHandle);
			for(i=0;i<BLK_LEN;i++)
			{
				pcm_fifo[i*2]=cvsd_fifo[i];
				pcm_fifo[i*2+1]=cvsd_fifo[i];
			}
			AudioDAC0_DataSet(pcm_fifo, BLK_LEN);
		}
	}
}


void BtHfpDisconnectedDev(void)
{
	system_mode = 0;
}

void BtHfpScoLinkConnected(void)
{
	system_mode = 1;
	MsbcDecoderInitialized = 0;
	Msbc_DecoderInit();

	//切换到16KHz
    uint32_t SampleRate = 8000;
    uint32_t DACBitWidth = 16;
    AUDIO_BitWidth ADCBitWidth = ADC_WIDTH_16BITS;

	//ADC
	//Mic1	 digital
    AudioADC_AnaInit(ADC1_MODULE, CHANNEL_LEFT, MIC_LEFT, Single, ADCCommonEnergy, 15);  
	
    AudioADC_DigitalInit(ADC1_MODULE, SampleRate, ADCBitWidth, (void *)AudioADC1Buf, sizeof(AudioADC1Buf));

    DACParamCt ct;
    ct.DACModel = DAC_Single;
    ct.DACLoadStatus = DAC_NOLoad;
    ct.PVDDModel = PVDD33;
    ct.DACEnergyModel = DACCommonEnergy;

    // DAC init
    AudioDAC_Init(&ct, SampleRate, DACBitWidth, (void *)AudioDACBuf, sizeof(AudioDACBuf), NULL, 0);
}

void BtHfpScoLinkDisconnected(void)
{
	if(system_mode)
	{
		DecoderInitialized = 0;

	    //初始化解码器
	    A2dp_DecoderInit();
	}
	system_mode = 0;
}

void BtHfpScoDataReceived(uint8_t *data,  uint16_t dataLen)
{
	uint32_t	insertLen = 0;
	int32_t 	remainLen = 0;

	if(system_mode)
	{
		//发
		remainLen = mv_msize(&HfSend_MemHandle);
		if(remainLen >= 120)
		{
			mv_mread(hfpSendBuf, 120, 1, &HfSend_MemHandle);
			HfpSendScoData(0, hfpSendBuf, 120);
			//printf("s ");
		}

		//收
		remainLen = mv_mremain(&MSBC_MemHandle);
		if(remainLen <= (dataLen+8))
		{
			printf("F");//msbc fifo full
			//增加读指针
			//
			return;
		}
		insertLen = mv_mwrite(data, dataLen, 1,&MSBC_MemHandle);

		if(BtHf_AECRingDataSpaceLenGet()>(dataLen/2))
		{
			BtHf_AECRingDataSet(data, (dataLen/2));
		}

		if(insertLen != dataLen)
		{
			printf("insert data len err! i:%ld,d:%d\n", insertLen, dataLen);
		}
		if( !DecoderInitialized )
		{
			DecoderInitialized=1;
		}
	}
}

/****************************************************************************************
 * main process
 ***************************************************************************************/
int main(void)
{
    Chip_Init(1);
    WDG_Disable();
    __c_init_rom();
    Clock_Config(1, 24000000);
    Clock_HOSCCurrentSet(15);  // 加大了晶体的偏置电流
    Clock_PllLock(240 * 1000); // 240M频率
    Clock_APllLock(240 * 1000);
    Clock_Module1Enable(ALL_MODULE1_CLK_SWITCH);
    Clock_Module2Enable(ALL_MODULE2_CLK_SWITCH);
    Clock_Module3Enable(ALL_MODULE3_CLK_SWITCH);
    Clock_SysClkSelect(PLL_CLK_MODE);
    Clock_UARTClkSelect(PLL_CLK_MODE);
    Clock_HOSCCurrentSet(5);

    SpiFlashInit(80000000, MODE_4BIT, 0, 1);

    // BP15系列开发板启用串口，默认使用
    GPIO_PortAModeSet(GPIOA9, 1);  // Rx, A9:uart1_rxd_1
    GPIO_PortAModeSet(GPIOA10, 5); // Tx, A10:uart1_txd_1
    DbgUartInit(1, 2000000, 8, 0, 1);

	DBG("\n");
	DBG("********************************************************************************\n");
	DBG("|                    MVsilicon B5 BT Demo                                      |\n");
	DBG("|            Mountain View Silicon Technology Co.,Ltd.                         |\n");
	DBG("|Audio Decoder Version: %s\n", (unsigned char *)audio_decoder_get_lib_version());
	DBG("|BtLib Version: %s\n", (unsigned char *)GetLibVersionBt());
	DBG("********************************************************************************\n");

	//初始化DAC所需要的DMA通道
    DMA_ChannelAllocTableSet((uint8_t*)DmaChannelMap);

    //初始化解码器
    A2dp_DecoderInit();

    //启动蓝牙服务
    BtStackServiceStart();

    while(1);
}
