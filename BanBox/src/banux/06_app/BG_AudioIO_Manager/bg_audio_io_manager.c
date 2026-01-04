/**
 * bg_audio_io_manager.c - 音频输入/输出管理器
 * 本文件包含重构后的函数划分版本
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_manager.h"
#include "audio_adc.h"
#include "adc_interface.h"
#include "dac_interface.h"
#include "adc.h"
#include "dma.h"
#include "reset.h"
#include "dac.h"
#include "clk.h"
#include "gpio.h"
#include "debug.h"
#include "type.h"
#include "audio_effect.h"
#include "ctrlvars.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_stor.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "otg_device_audio.h"
#include "otg_device_cdc.h"
#include "bg_shell.h"
#include "shell_io_cdc.h"
#include "shell_io_ble.h"
#include "shell_io_manager.h"
#include "audio_looper.h"
#include "sra.h"
#include "spi_flash.h"
#include "resampler.h"
#include "audio_decoder_api.h"

// Effect Graph 模块
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "shell_cmd_graph.h"

#include "bt_manager.h"
// ==================== 全局缓冲区定义 ====================
static uint32_t AudioADC1Buf[1024] = {0};
static uint32_t AudioADC2Buf[1024] = {0};

#define DAC_FIFO_SAMPLES 1024
static uint32_t DAC0_FIFO[DAC_FIFO_SAMPLES];
#define DAC0_FIFO_LEN sizeof(DAC0_FIFO)
static uint32_t DAC1_FIFO[DAC_FIFO_SAMPLES];
#define DAC1_FIFO_LEN sizeof(DAC1_FIFO)

// Looper音频缓冲区
static uint8_t looper_flash_buffer[512];
static uint32_t looper_playback_buffer[256];

// ==================== 外部变量 ====================
extern uint32_t usb_speaker_enable;
extern uint32_t usb_mic_enable;

// ==================== 前向声明 ====================
void BG_audio_Init(uint16_t SampleRate);
void Audio_loop(void);

// ==================== 管理器结构体初始化 ====================
BG_Audio_Io_Manager BG_AudioManager = {
	.Audio_Init = BG_audio_Init,
	.Audio_Loop = Audio_loop,
	.Audio_data = {
		.guitar_count = 0,
		.mic_count = 0,
		.det_state = NONE,
	},
};

// ==================== 蓝牙SBC解码器相关定义 ====================
#include "audio_decoder_api.h"
#include "bt_app_interface.h"
#include "bt_stack_service.h"

#define BT_SBC_PACKET_SIZE 595
#define BT_SBC_DECODER_INPUT_LEN (4 * 1024)
#define BT_SBC_LEVEL_HIGH (BT_SBC_DECODER_INPUT_LEN - BT_SBC_PACKET_SIZE * 4)
#define BT_SBC_LEVEL_LOW (BT_SBC_PACKET_SIZE * 6)
#define BT_SBC_LEVEL_START (BT_SBC_LEVEL_HIGH - BT_SBC_PACKET_SIZE * 3)
#define SBC_DECODER_FIFO_MIN (119 * 2)

uint8_t a2dp_sbcBuf[BT_SBC_DECODER_INPUT_LEN];
static uint8_t decoder_buf[1024 * 20] = {0};
static uint8_t DecoderInitialized = 0;

MemHandle SBC_MemHandle;
ResamplerContext bt_resmaper;
// ==================== 前向声明（内部函数） ====================
static void SaveDataToSbcBuffer(uint8_t *data, uint16_t dataLen);

// Effect Graph 音频设备回调函数
static uint16_t ADC0_ReadGuitarData(EffectNode_t *node, int32_t *out_buf, uint16_t max_len);
static uint16_t ADC1_ReadMicData(EffectNode_t *node, int32_t *out_buf, uint16_t max_len);
static void DAC0_WriteSpeakerData(EffectNode_t *node, int32_t *in_buf, uint16_t len);
static uint16_t USB_ReadAudioData(EffectNode_t *node, int32_t *out_buf, uint16_t max_len);
static void USB_WriteAudioData(EffectNode_t *node, int32_t *in_buf, uint16_t len);
static uint16_t BT_ReadAudioData(EffectNode_t *node, int32_t *out_buf, uint16_t max_len);
static void SetupEffectGraphCallbacks(void);

// ==================== 初始化函数群 ====================

// 初始化SBC解码器
static void A2dp_DecoderInit(void)
{
	memset(a2dp_sbcBuf, 0, BT_SBC_DECODER_INPUT_LEN);
	SBC_MemHandle.addr = a2dp_sbcBuf;
	SBC_MemHandle.mem_capacity = BT_SBC_DECODER_INPUT_LEN;
	SBC_MemHandle.mem_len = 0;
	SBC_MemHandle.p = 0;
	SaveA2dpStreamDataToBuffer = SaveDataToSbcBuffer;
}

// 初始化USB和设备模式
static void InitUSBDevice(void)
{
	// 使用AUDIO_MIC_CDC模式：音频+麦克风+CDC串口复合设备
	OTG_DeviceModeSel(AUDIO_MIC_CDC, 0x1234, 0x1234);
	UsbDevicePlayInit();
	UsbDeviceEnable();
}

// 初始化DAC（数字模拟转换器）
static void InitDAC(uint16_t SampleRate)
{
	AudioDAC_Init(ALL, SampleRate, (void *)DAC0_FIFO, DAC0_FIFO_LEN, (void *)DAC1_FIFO, DAC1_FIFO_LEN);
	AudioDAC_DoutModeSet(DAC0, MODE2, WIDTH_16_BIT);
	AudioDAC_DoutModeSet(DAC1, MODE2, WIDTH_16_BIT);
	AudioDAC_VolSet(DAC0, 0x3FFF, 0x3FFF);
	AudioDAC_VolSet(DAC1, 0x3FFF, 0);
}

// 初始化ADC0（LineIn5）
static void InitADC0LineIn(void)
{
	AudioADC_AnaInit();
	AudioADC_DynamicElementMatch(ADC0_MODULE, TRUE, TRUE);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_RIGHT, LINEIN_NONE);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_LEFT, LINEIN_NONE);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_RIGHT, LINEIN5_RIGHT);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_LEFT, LINEIN5_LEFT);
	AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_RIGHT, LINEIN5_RIGHT, 32, 0);
	AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_LEFT, LINEIN5_LEFT, 32, 0);
}

// 初始化ADC1（麦克风）
static void InitADC1Mic(void)
{
	AudioADC_DynamicElementMatch(ADC1_MODULE, TRUE, TRUE);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1);
	AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2, 12, 2);
	AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1, 12, 2);
	AudioADC_VolSet(ADC1_MODULE, 0xFFF, 0xFFF);
}

// 初始化数字化ADC采样
static void InitADCDigital(uint16_t SampleRate)
{
	AudioADC_DigitalInit(ADC0_MODULE, SampleRate, (void *)AudioADC1Buf, sizeof(AudioADC1Buf));
	AudioADC_MicBias1Enable(TRUE);
	AudioADC_VcomConfig(1);
	AudioADC_DigitalInit(ADC1_MODULE, SampleRate, (void *)AudioADC2Buf, sizeof(AudioADC2Buf));
}

// 初始化音频效果（混响等）
static void InitAudioEffects(uint16_t SampleRate)
{
	gCtrlVars.audio_effect_init_flag = 1;
	
	// 混响效果（Reverb）
	gCtrlVars.reverb_unit.enable = 1;
	gCtrlVars.plate_reverb_unit.enable = 0;
	AudioEffectReverbInit(&gCtrlVars.reverb_unit, 2, SampleRate);
	
	// 动态范围压缩（DRC）- ADC输入通道
	gCtrlVars.mic_drc_unit.enable = 1;
	AudioEffectDRCInit(&gCtrlVars.mic_drc_unit, 2, SampleRate);
	
	// 均衡器（EQ）- ADC输出EQ
	gCtrlVars.mic_out_eq_unit.enable = 1;
	AudioEffectEQInit(&gCtrlVars.mic_out_eq_unit, 2, SampleRate);
	
	// 啸叫抑制（Howling Detector）
	#if CFG_AUDIO_EFFECT_MIC_HOWLING_DECTOR_EN
	gCtrlVars.howling_dector_unit.enable = 0;
	AudioEffectHowlingSuppressorInit(&gCtrlVars.howling_dector_unit);
	#endif
	
	// 噪声抑制（Noise Suppressor）
	#if CFG_AUDIO_EFFECT_MIC_NOISE_SUPPRESSOR_EN
	gCtrlVars.MicAudioSdct_unit.enable = 1;
	AudioEffectSilenceDectorInit(&gCtrlVars.MicAudioSdct_unit, 2, SampleRate);
	#endif
	
	// 扩展器（Expander）- 麦克风通道
	gCtrlVars.mic_expander_unit.enable = 1;
	AudioEffectExpanderInit(&gCtrlVars.mic_expander_unit, 2, SampleRate);
}

// 初始化控制GPIO输出
static void InitControlGPIO(void)
{
	// GPIO_B6: 扬声器/耳机切换
	GPIO_RegOneBitClear(GPIO_B_IE, GPIOB6);
	GPIO_RegOneBitSet(GPIO_B_OE, GPIOB6);
	GPIO_RegOneBitSet(GPIO_B_OUT, GPIOB6);

	// GPIO_A1: 麦克风指示
	GPIO_RegOneBitClear(GPIO_A_IE, GPIOA1);
	GPIO_RegOneBitSet(GPIO_A_OE, GPIOA1);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA1);

	// GPIO_A17: 吉他指示
	GPIO_RegOneBitClear(GPIO_A_IE, GPIOA17);
	GPIO_RegOneBitSet(GPIO_A_OE, GPIOA17);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA17);
}

// 初始化GPIO检测引脚
static void InitDetectionGPIO(void)
{
	// GPIO_A_INDEX29: 吉他检测输入，上拉
	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX29);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX29);
	GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX29);
	GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX29);

	// GPIO_A_INDEX30: 麦克风检测输入，下拉
	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX30);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX30);
	GPIO_RegOneBitClear(GPIO_A_PU, GPIO_INDEX30);
	GPIO_RegOneBitSet(GPIO_A_PD, GPIO_INDEX30);

	// GPIO_B_INDEX4: 耳机检测输入，上拉
	GPIO_RegOneBitSet(GPIO_B_IE, GPIO_INDEX4);
	GPIO_RegOneBitClear(GPIO_B_OE, GPIO_INDEX4);
	GPIO_RegOneBitSet(GPIO_B_PU, GPIO_INDEX4);
	GPIO_RegOneBitClear(GPIO_B_PD, GPIO_INDEX4);
}

/**
 * 初始化音频系统
 * @param SampleRate 采样率
 */
void BG_audio_Init(uint16_t SampleRate)
{
	BG_AudioManager.Audio_data.SampleRate = SampleRate;

	InitUSBDevice();
	InitDAC(SampleRate);
	InitADC0LineIn();
	InitADC1Mic();
	InitADCDigital(SampleRate);
	InitAudioEffects(SampleRate);
	InitControlGPIO();
	InitDetectionGPIO();
	A2dp_DecoderInit();
	BtStackServiceStart();
	
	// 初始化Shell IO管理器（自动管理CDC和BLE接口）
	ShellIOManager_Init();

	// ========== Effect Graph 初始化 ==========
	DBG("[Audio] Initializing Effect Graph...\n");
	
	// 1. 初始化 Effect Graph 核心模块
	if (EffectGraph_Init() != 0) {
		DBG("[Audio] ERROR: Effect Graph Init failed!\n");
		return;
	}
	
	// 2. 加载默认预设（可根据需求选择其他预设）
	if (EffectGraphConfig_LoadPreset(GRAPH_PRESET_DEFAULT) != 0) {
		DBG("[Audio] ERROR: Effect Graph Load Preset failed!\n");
		return;
	}
	
	// 3. 挂接实际音频设备回调
	SetupEffectGraphCallbacks();
	
	// 4. 注册 Shell 命令（支持 CDC/BLE 远程控制）
	ShellCmdGraph_Register();
	
	DBG("[Audio] Effect Graph initialized successfully\n");
	// ==========================================

	// 初始化Audio Looper（使用NOR Flash）
	AudioLooper.InitWithFlashType(FLASH_TYPE_NOR);

	AudioDAC_FadeDisable(DAC0);

	AudioDAC_Pause(DAC0);
	

}
/**
 * 设置输出音量（通过ADC读取电位器值）
 */
static void SetVolume(void)
{
	uint16_t DC_Data;
	GPIO_RegOneBitClear(GPIO_A_ANA_EN, GPIO_INDEX28);
	GPIO_RegOneBitSet(GPIO_A_ANA_EN, GPIO_INDEX28);
	DC_Data = ADC_SingleModeDataGet(ADC_CHANNEL_GPIOA28) * 4;
	AudioDAC_VolSet(DAC0, DC_Data, DC_Data);
	AudioDAC_VolSet(DAC1, DC_Data, 0);
}




// ==================== 音频解码和处理 ====================

/**
 * 保存SBC数据到缓冲区
 */
static void SaveDataToSbcBuffer(uint8_t *data, uint16_t dataLen)
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

/**
 * 处理吉他信号输出
 */
static void ProcessGuitarOutput(uint16_t n)
{
	if (!GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX29))
	{

		BG_AudioManager.Audio_data.det_state  = GUITAR_DET_OUT;
		GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA17);

	}
	else
	{
		GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA17);
		BG_AudioManager.Audio_data.det_state  = GUITAR_DET_IN;


	}

}

/**
 * 处理麦克风信号输出
 */
static void ProcessMicOutput(uint16_t n)
{
	if (GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX30))
	{
		GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA1);
		BG_AudioManager.Audio_data.det_state = MIC_DET_OUT;

	}
	else
	{
		GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA1);
		BG_AudioManager.Audio_data.det_state  = MIC_DET_IN;


	}
}

/**
 * 处理扬声器/耳机切换
 */
static void ProcessSpeakerSwitch(void)
{
	if (GPIO_RegOneBitGet(GPIO_B_IN, GPIO_INDEX4))
	{
		GPIO_RegOneBitClear(GPIO_B_OUT, GPIOB6);
		BG_AudioManager.Audio_data.det_state  = SPEAKER_DET;
	}
	else
	{

		GPIO_RegOneBitSet(GPIO_B_OUT, GPIOB6);
		BG_AudioManager.Audio_data.det_state  = EARPHONE_DET;
	}
}

/*
static void Detect_check()
{
	switch(BG_AudioManager.Audio_data.det_state)
	{
		case NONE:
			break;
		case MIC_DET_IN:
			GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA1);
			Shell_Printf("Mic Detect IN! \n");

			BG_AudioManager.Audio_data.det_state = NONE;
			break;
		case MIC_DET_OUT:

			Shell_Printf("Mic Detect OUT! \n");
			DMA_CircularFIFOClear(PERIPHERAL_ID_AUDIO_DAC0_TX);
			DMA_CircularFIFOClear(PERIPHERAL_ID_AUDIO_ADC0_RX);
			BG_AudioManager.Audio_data.det_state  = NONE;
			break;
		case GUITAR_DET_IN:
			GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA17);
			Shell_Printf("Guitar Detect IN! \n");
			BG_AudioManager.Audio_data.det_state = NONE;
			break;
		case GUITAR_DET_OUT:
			GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA17);
			Shell_Printf("Guiatr Detect OUT! \n");
			DMA_CircularFIFOClear(PERIPHERAL_ID_AUDIO_DAC0_TX);
			DMA_CircularFIFOClear(PERIPHERAL_ID_AUDIO_ADC1_RX);
			BG_AudioManager.Audio_data.det_state = NONE;
			break;
		case EARPHONE_DET:
			GPIO_RegOneBitSet(GPIO_B_OUT, GPIOB6);
			Shell_Printf("Line Out Detect ! \n");
			BG_AudioManager.Audio_data.det_state  = NONE;
			break;
		case SPEAKER_DET:
			GPIO_RegOneBitClear(GPIO_B_OUT, GPIOB6);
			Shell_Printf("Speaker mode ! \n");
			BG_AudioManager.Audio_data.det_state  = NONE;
			break;
		default:
			Shell_Printf("Param Err,please check! \n");
			break;
	}
}
*/
/**
 * 读取音频数据（ADC和混音）
 */
static void ReadAudioData(uint16_t len, uint16_t *pRealLen)
{
	uint16_t RealLen = 0;
	uint16_t i;

	RealLen = AudioADC_DataGet(ADC1_MODULE, BG_AudioManager.Audio_data.mic_buf_in, len);
	if (len > RealLen)
		len = RealLen;

	RealLen = AudioADC_DataGet(ADC0_MODULE, BG_AudioManager.Audio_data.guitar_buf_in, len);
	if (len > RealLen)
		len = RealLen;

	// 混合吉他和麦克风信号
	for (i = 0; i < len; i++)
	{
		BG_AudioManager.Audio_data.OutPut_buf[i] =
			BG_AudioManager.Audio_data.guitar_buf_in[i] +
			BG_AudioManager.Audio_data.mic_buf_in[i];
	}

	*pRealLen = len;
}

/**
 * 应用音频效果
 */
static void ApplyAudioEffects(uint16_t len)
{
	static uint32_t temp_buf1[512];
	static uint32_t temp_buf2[512];
	
	// 效果链处理顺序：
	// 输入(OutPut_buf) -> 扩展器 -> DRC -> EQ -> 混响 -> 输出(guitar_buf_out)
	
	// 1. 扩展器（Expander）- 处理动态范围的扩展部分
	if (gCtrlVars.mic_expander_unit.enable)
	{
		AudioEffectExpanderApply(&gCtrlVars.mic_expander_unit,
		                         (int16_t *)BG_AudioManager.Audio_data.OutPut_buf,
		                         (int16_t *)temp_buf1,
		                         len);
	}
	else
	{
		memcpy(temp_buf1, BG_AudioManager.Audio_data.OutPut_buf, len * sizeof(uint32_t));
	}
	
	// 2. 动态范围压缩（DRC）- 压缩音频动态范围
	#if CFG_AUDIO_EFFECT_MIC_DRC_EN
	if (gCtrlVars.mic_drc_unit.enable)
	{
		AudioEffectDRCApply(&gCtrlVars.mic_drc_unit,
		                    (int16_t *)temp_buf1,
		                    (int16_t *)temp_buf2,
		                    len);
	}
	else
	#endif
	{
		memcpy(temp_buf2, temp_buf1, len * sizeof(uint32_t));
	}
	
	// 3. 均衡器（EQ）- 调整频率响应
	#if CFG_AUDIO_EFFECT_MIC_OUT_EQ_EN
	if (gCtrlVars.mic_out_eq_unit.enable)
	{
		AudioEffectEQApply(&gCtrlVars.mic_out_eq_unit,
		                   (int16_t *)temp_buf2,
		                   (int16_t *)temp_buf1,
		                   len,
		                   2);
	}
	else
	#endif
	{
		memcpy(temp_buf1, temp_buf2, len * sizeof(uint32_t));
	}
	
	// 4. 啸叫抑制（Howling Detector）
	#if CFG_AUDIO_EFFECT_MIC_HOWLING_DECTOR_EN
	if (gCtrlVars.howling_dector_unit.enable)
	{
		AudioEffectHowlingSuppressorApply(&gCtrlVars.howling_dector_unit,
		                                  (int16_t *)temp_buf1,
		                                  (int16_t *)temp_buf2,
		                                  len);
	}
	else
	#endif
	{
		memcpy(temp_buf2, temp_buf1, len * sizeof(uint32_t));
	}
	
	// 5. 混响（Reverb）- 最后添加空间感
	if (gCtrlVars.reverb_unit.enable)
	{
		AudioEffectReverbApply(&gCtrlVars.reverb_unit,
		                       (int16_t *)temp_buf2,
		                       (int16_t *)BG_AudioManager.Audio_data.guitar_buf_out,
		                       len);
	}
	else
	{
		memcpy(BG_AudioManager.Audio_data.guitar_buf_out, temp_buf2, len * sizeof(uint32_t));
	}
}

void handle_usb_record(uint16_t len)
{
	if(usb_mic_enable)
	{
		 UsbAudioMicDataSet(BG_AudioManager.Audio_data.OutPut_buf,len);
	}

}
/**
 * 构建最终输出（考虑USB音频）
 */
static void BuildFinalOutput(uint16_t len, uint32_t *bt_audio_buffer)
{
	uint16_t i;
	uint16_t usb_data_len;
	if (usb_speaker_enable)
	{
		usb_data_len = UsbAudioSpeakerDataLenGet();
		if (usb_data_len >= len)
		{
			/* USB数据充足，正常读取 */
			UsbAudioSpeakerDataGet(BG_AudioManager.Audio_data.USB_dac_buf, len);
		}
		else if (usb_data_len > 0)
		{
			/* USB数据不足，读取可用数据，剩余填零 */
			UsbAudioSpeakerDataGet(BG_AudioManager.Audio_data.USB_dac_buf, usb_data_len);
			for (i = usb_data_len; i < len; i++)
			{
				BG_AudioManager.Audio_data.USB_dac_buf[i] = 0;
			}
		}
		else
		{
			/* USB无数据，全部填零避免噪声 */
			for (i = 0; i < len; i++)
			{
				BG_AudioManager.Audio_data.USB_dac_buf[i] = 0;
			}
		}
		for (i = 0; i < len; i++)
		{
			BG_AudioManager.Audio_data.OutPut_buf[i] =
				BG_AudioManager.Audio_data.guitar_buf_out[i] +
				BG_AudioManager.Audio_data.USB_dac_buf[i] +
				bt_audio_buffer[i];
		}
	}
	else
	{
		for (i = 0; i < len; i++)
		{
			BG_AudioManager.Audio_data.OutPut_buf[i] =
				BG_AudioManager.Audio_data.guitar_buf_out[i] +
				BG_AudioManager.Audio_data.mic_buf_out[i] +
				bt_audio_buffer[i];
		}
	}
}

/**
 * 输出音频数据到DAC
 */
static void OutputAudioData(uint16_t len)
{
	AudioDAC_DataSet(DAC0, BG_AudioManager.Audio_data.OutPut_buf, len);
}

/**
 * 音频主循环（有蓝牙音频数据时）
 */
static void AudioLoopWithBT(uint32_t *bt_audio_buffer)
{
	uint32_t SampleRateCC = BG_AudioManager.Audio_data.SampleRate;
	uint16_t RealLen = 0;
	uint16_t n = 0;
	uint16_t i;

	if (RT_SUCCESS == audio_decoder_can_continue())
	{
		if (mv_msize(&SBC_MemHandle) <= SBC_DECODER_FIFO_MIN)
			return;

		if (audio_decoder_decode() == RT_SUCCESS)
		{
			if (SampleRateCC != audio_decoder->song_info->sampling_rate)
			{
				//SampleRateCC = audio_decoder->song_info->sampling_rate;
				/* 同步DAC和ADC采样率，避免移频 */
//				AudioDAC_SampleRateChange(DAC0, audio_decoder->song_info->sampling_rate);
//				AudioDAC_SampleRateChange(DAC1, audio_decoder->song_info->sampling_rate);
//				resampler_init(&bt_resmaper,2,audio_decoder->song_info->sampling_rate,CFG_PARA_SAMPLE_RATE,0,0);
//				n = resampler_apply(&bt_resmaper,(int16_t *)audio_decoder->song_info->pcm_addr,(int16_t *)audio_decoder->song_info->pcm_addr,audio_decoder->song_info->pcm_data_length);
//				DBG("BT Audio Rate: %ld Hz (DAC+ADC synced)\n", (long)audio_decoder->song_info->sampling_rate);
			}else{
				n = audio_decoder->song_info->pcm_data_length;

			}
			


			/* 转换PCM数据到bt_audio_buffer */
			for (i = 0; i < n; i++)
				bt_audio_buffer[i] = (uint32_t)audio_decoder->song_info->pcm_addr[i];

			/* 等待DAC有足够空间 */
			while (AudioDAC0DataSpaceLenGet() < n);
			
			/* 等待ADC有足够数据（确保吉他/麦克风数据准备好） */
			while (AudioADC_DataLenGet(ADC0_MODULE) < n || AudioADC_DataLenGet(ADC1_MODULE) < n);

			ReadAudioData(n, &RealLen);
			ApplyAudioEffects(RealLen);
			ProcessGuitarOutput(RealLen);
			ProcessMicOutput(RealLen);
			ProcessSpeakerSwitch();
			BuildFinalOutput(RealLen, bt_audio_buffer);
			handle_usb_record(RealLen);
			OutputAudioData(RealLen);
		}
	}
}

/**
 * 音频主循环（无蓝牙音频，最小缓冲处理）
 */
static void AudioLoopMinimal(uint32_t *bt_audio_buffer)
{
	uint16_t RealLen = 0;
	uint16_t i;
	const uint16_t MIN_SAMPLE = 48;

	if (AudioADC_DataLenGet(ADC0_MODULE) >= MIN_SAMPLE)
	{
		ReadAudioData(MIN_SAMPLE, &RealLen);
		ApplyAudioEffects(RealLen);
		ProcessGuitarOutput(RealLen);
		ProcessMicOutput(RealLen);
		ProcessSpeakerSwitch();

		/* Looper录制处理 - 使用mic_buf_in（麦克风输入）*/
		if (AudioLooper.IsRecording())
		{
			AudioLooper.ProcessRecording32(BG_AudioManager.Audio_data.mic_buf_in,
			                               looper_flash_buffer, RealLen);
		}

		/* 清零播放缓冲区 */
		for (i = 0; i < RealLen; i++)
		{
			looper_playback_buffer[i] = 0;
		}
		
		/* Looper播放处理 */
		if (AudioLooper.IsPlaying())
		{
			AudioLooper.ProcessPlayback32(looper_playback_buffer, looper_flash_buffer, RealLen);
		}
		
		/* 混合节拍器音频（独立于播放，节拍器可单独使用） */
		if (AudioLooper.MetronomeIsEnabled())
		{
			metronome_process_audio(looper_playback_buffer, RealLen);
		}

		if (usb_speaker_enable)
		{
			uint16_t usb_data_len = UsbAudioSpeakerDataLenGet();
			if (usb_data_len >= RealLen)
			{
				/* USB数据充足，正常读取 */
				UsbAudioSpeakerDataGet(BG_AudioManager.Audio_data.USB_dac_buf, RealLen);
			}
//			else if (usb_data_len > 0)
//			{
//				/* USB数据不足，读取可用数据，剩余填零 */
//				UsbAudioSpeakerDataGet(BG_AudioManager.Audio_data.USB_dac_buf, usb_data_len);
//				for (i = usb_data_len; i < RealLen; i++)
//				{
//					BG_AudioManager.Audio_data.USB_dac_buf[i] = 0;
//				}
//			}
			else
			{
				/* USB无数据，全部填零避免噪声 */
				for (i = 0; i < RealLen; i++)
				{
					BG_AudioManager.Audio_data.USB_dac_buf[i] = 0;
				}
			}
			for (i = 0; i < RealLen; i++)
			{
				BG_AudioManager.Audio_data.OutPut_buf[i] =
					BG_AudioManager.Audio_data.guitar_buf_out[i] +
					BG_AudioManager.Audio_data.USB_dac_buf[i] +
					BG_AudioManager.Audio_data.mic_buf_in[i] +
					looper_playback_buffer[i];
			}
		}
		else
		{
			for (i = 0; i < RealLen; i++)
			{
				BG_AudioManager.Audio_data.OutPut_buf[i] =
					BG_AudioManager.Audio_data.guitar_buf_out[i] +
					BG_AudioManager.Audio_data.mic_buf_in[i] +
					looper_playback_buffer[i];
			}
		}
		handle_usb_record(RealLen);
		OutputAudioData(RealLen);
	}
}

// ==================== 主音频循环 ====================

/**
 * 音频主循环处理函数
 */
void Audio_loop(void)
{
	static uint32_t bt_audio_buffer[256] = {0};

	BtStackServiceRun();
	SetVolume();
	OTG_DeviceRequestProcess();
	
	// CDC串口任务处理 - 必须周期性调用以接收数据
	OTG_DeviceCDC_Task();

	// 检查是否有蓝牙音频数据要处理
	if (GetA2dpState() == BT_A2DP_STATE_STREAMING)
	{
		if (mv_msize(&SBC_MemHandle) > SBC_DECODER_FIFO_MIN)
			AudioLoopWithBT(bt_audio_buffer);
	}
	else
	{
		AudioLoopMinimal(bt_audio_buffer);
	}
	
	// CDC串口应用处理 - 使用Shell IO管理器（自动切换CDC/BLE）
	ShellIOManager_Process();


}

// ==================== Effect Graph 音频设备回调实现 ====================

/**
 * Guitar ADC Source 回调 - 从 ADC0 读取吉他输入数据
 */
static uint16_t ADC0_ReadGuitarData(EffectNode_t *node, int32_t *out_buf, uint16_t max_len)
{
	uint16_t available_samples;
	uint16_t samples_to_read;
	uint16_t i;
	uint32_t temp_buf[256];
	
	// 从 AudioADC1Buf 读取吉他输入数据
	available_samples = AudioADC_DataLenGet(ADC0_MODULE);
	samples_to_read = (max_len < available_samples) ? max_len : available_samples;
	
	if (samples_to_read > 256) {
		samples_to_read = 256;
	}
	
	if (samples_to_read > 0) {
		// 读取立体声数据 (左右声道)
		AudioADC_DataGet(ADC0_MODULE, temp_buf, samples_to_read);
		
		// 转换为 int32_t 格式
		for (i = 0; i < samples_to_read; i++) {
			out_buf[i] = (int32_t)temp_buf[i];
		}
		
		return samples_to_read;
	}
	return 0;
}

/**
 * Mic ADC Source 回调 - 从 ADC1 读取麦克风数据
 */
static uint16_t ADC1_ReadMicData(EffectNode_t *node, int32_t *out_buf, uint16_t max_len)
{
	uint16_t available_samples;
	uint16_t samples_to_read;
	uint16_t i;
	uint32_t temp_buf[256];
	
	// 从 AudioADC2Buf 读取麦克风数据
	available_samples = AudioADC_DataLenGet(ADC1_MODULE);
	samples_to_read = (max_len < available_samples) ? max_len : available_samples;
	
	if (samples_to_read > 256) {
		samples_to_read = 256;
	}
	
	if (samples_to_read > 0) {
		AudioADC_DataGet(ADC1_MODULE, temp_buf, samples_to_read);
		
		// 转换为 int32_t 格式
		for (i = 0; i < samples_to_read; i++) {
			out_buf[i] = (int32_t)temp_buf[i];
		}
		
		return samples_to_read;
	}
	return 0;
}

/**
 * DAC Sink 回调 - 写入数据到 DAC0 扬声器输出
 */
static void DAC0_WriteSpeakerData(EffectNode_t *node, int32_t *in_buf, uint16_t len)
{
	uint16_t free_space;
	uint16_t samples_to_write;
	uint16_t i;
	uint32_t temp_buf[256];
	
	// 获取 DAC FIFO 可用空间
	free_space = AudioDAC_DataSpaceLenGet(DAC0);
	samples_to_write = (len < free_space) ? len : free_space;
	
	if (samples_to_write > 256) {
		samples_to_write = 256;
	}
	
	if (samples_to_write > 0) {
		// 转换为 uint32_t 格式
		for (i = 0; i < samples_to_write; i++) {
			temp_buf[i] = (uint32_t)in_buf[i];
		}
		
		AudioDAC_DataSet(DAC0, temp_buf, samples_to_write);
	}
}

/**
 * USB Audio Source 回调 - 从 USB 读取音频数据
 */
static uint16_t USB_ReadAudioData(EffectNode_t *node, int32_t *out_buf, uint16_t max_len)
{
	uint16_t available;
	uint16_t samples_to_read;
	uint16_t i;
	uint32_t temp_buf[256];
	
	// 检查 USB 音频是否启用
	if (!usb_speaker_enable) {
		return 0;
	}
	
	// 从 USB 音频接口读取数据
	available = UsbAudioSpeakerDataLenGet();
	samples_to_read = (max_len < available) ? max_len : available;
	
	if (samples_to_read > 256) {
		samples_to_read = 256;
	}
	
	if (samples_to_read > 0) {
		UsbAudioSpeakerDataGet(temp_buf, samples_to_read);
		
		// 转换为 int32_t 格式
		for (i = 0; i < samples_to_read; i++) {
			out_buf[i] = (int32_t)temp_buf[i];
		}
		
		return samples_to_read;
	}
	return 0;
}

/**
 * USB Audio Sink 回调 - 写入音频数据到 USB
 */
static void USB_WriteAudioData(EffectNode_t *node, int32_t *in_buf, uint16_t len)
{
	uint16_t i;
	uint32_t temp_buf[256];
	uint16_t samples_to_write;
	
	// 检查 USB 麦克风是否启用
	if (!usb_mic_enable) {
		return;
	}
	
	samples_to_write = len;
	if (samples_to_write > 256) {
		samples_to_write = 256;
	}
	
	// 转换为 uint32_t 格式
	for (i = 0; i < samples_to_write; i++) {
		temp_buf[i] = (uint32_t)in_buf[i];
	}
	
	// 写入数据到 USB 音频接口
	UsbAudioMicDataSet(temp_buf, samples_to_write);
}

/**
 * 蓝牙 Audio Source 回调 - 从蓝牙解码器读取音频数据
 */
static uint16_t BT_ReadAudioData(EffectNode_t *node, int32_t *out_buf, uint16_t max_len)
{
	uint16_t i;
	int16_t *pcm_data;
	uint16_t pcm_len;
	
	// 检查蓝牙是否处于流式传输状态
	if (GetA2dpState() != BT_A2DP_STATE_STREAMING) {
		return 0;
	}
	
	// 从 SBC 解码器读取数据
	if (mv_msize(&SBC_MemHandle) < SBC_DECODER_FIFO_MIN) {
		return 0;
	}
	
	// 解码（audio_decoder_decode 不接受参数）
	if (audio_decoder_decode() != RT_SUCCESS) {
		return 0;
	}
	
	// 获取解码后的 PCM 数据
	if (audio_decoder && audio_decoder->song_info) {
		pcm_data = audio_decoder->song_info->pcm_addr;
		pcm_len = audio_decoder->song_info->pcm_data_length;
		
		// 限制复制长度
		if (pcm_len > max_len) {
			pcm_len = max_len;
		}
		
		// 转换并复制数据到缓冲区（int16_t -> int32_t）
		for (i = 0; i < pcm_len; i++) {
			out_buf[i] = (int32_t)pcm_data[i];
		}
		
		return pcm_len;
	}
	
	return 0;
}

/**
 * 挂接 Effect Graph 节点的音频设备回调
 */
static void SetupEffectGraphCallbacks(void)
{
	EffectNode_t* node = NULL;
	
	DBG("[Audio] Setting up Effect Graph callbacks...\n");
	
	// 挂接吉他输入节点
	node = EffectGraph_FindNodeByName("guitar_in");
	if (node) {
		node->func.source = ADC0_ReadGuitarData;
		DBG("[Audio] Guitar input callback registered\n");
	}
	
	// 挂接麦克风输入节点
	node = EffectGraph_FindNodeByName("mic_in");
	if (node) {
		node->func.source = ADC1_ReadMicData;
		DBG("[Audio] Mic input callback registered\n");
	}
	
	// 挂接 DAC 输出节点
	node = EffectGraph_FindNodeByName("dac_out");
	if (node) {
		node->func.sink = DAC0_WriteSpeakerData;
		DBG("[Audio] DAC output callback registered\n");
	}
	
	// 挂接 USB 输入节点（Speaker）
	node = EffectGraph_FindNodeByName("usb_in");
	if (node) {
		node->func.source = USB_ReadAudioData;
		DBG("[Audio] USB input callback registered\n");
	}
	
	// 挂接 USB 输出节点（Mic）
	node = EffectGraph_FindNodeByName("usb_out");
	if (node) {
		node->func.sink = USB_WriteAudioData;
		DBG("[Audio] USB output callback registered\n");
	}
	
	// 挂接蓝牙输入节点
	node = EffectGraph_FindNodeByName("bt_in");
	if (node) {
		node->func.source = BT_ReadAudioData;
		DBG("[Audio] BT input callback registered\n");
	}
	
	DBG("[Audio] All callbacks setup completed\n");
}
