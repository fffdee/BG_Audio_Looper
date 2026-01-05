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

// ==================== 蓝牙预解码缓冲区（全局，供AudioLoopWithGraph和BT回调共享）====================
static uint32_t bt_decoded_buffer[640];  /* 预解码缓冲区，支持 SBC 最大帧长 ~595，使用 uint32_t 统一格式 */
static uint16_t bt_decoded_len = 0;     /* 预解码数据长度 */
static bool bt_has_decoded_data = false; /* 是否有预解码数据 */
static uint32_t bt_current_sample_rate = 0; /* 当前蓝牙音频采样率 */
static uint32_t system_default_sample_rate = 44100; /* 系统默认采样率（蓝牙断开后恢复） */

// ==================== 前向声明（内部函数） ====================
static void SaveDataToSbcBuffer(uint8_t *data, uint16_t dataLen);

// Effect Graph 音频设备回调函数 - 源节点
static uint16_t ADC0_ReadGuitarData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
static uint16_t ADC1_ReadMicData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
static uint16_t USB_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
static uint16_t BT_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);

// Effect Graph 源节点可用数据量查询回调 (用于自适应帧长)
static uint16_t ADC0_GetAvailableData(EffectNode_t *node);
static uint16_t ADC1_GetAvailableData(EffectNode_t *node);
static uint16_t USB_GetAvailableData(EffectNode_t *node);
static uint16_t BT_GetAvailableData(EffectNode_t *node);

// Effect Graph 音频设备回调函数 - 输出节点
static void DAC0_WriteSpeakerData(EffectNode_t *node, uint32_t *in_buf, uint16_t len);
static void USB_WriteAudioData(EffectNode_t *node, uint32_t *in_buf, uint16_t len);

// Effect Graph 效果器处理回调函数
static void Expander_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
static void DRC_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
static void EQ_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
static void Reverb_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
static void Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
static void Passthrough_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);

// Effect Graph 回调挂接函数
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
	system_default_sample_rate = SampleRate;  /* 保存默认采样率，蓝牙断开后恢复 */

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
	static uint32_t last_bt_sample_rate = 0;  /* 上次蓝牙采样率，用于检测变化 */
	uint16_t RealLen = 0;
	uint16_t n = 0;
	uint16_t i;

	if (RT_SUCCESS == audio_decoder_can_continue())
	{
		if (mv_msize(&SBC_MemHandle) <= SBC_DECODER_FIFO_MIN)
			return;

		if (audio_decoder_decode() == RT_SUCCESS)
		{
			n = audio_decoder->song_info->pcm_data_length;

			/* 检查采样率是否变化，如果变化则同步 DAC/ADC */
			if (last_bt_sample_rate != audio_decoder->song_info->sampling_rate)
			{
				last_bt_sample_rate = audio_decoder->song_info->sampling_rate;
				BG_AudioManager.Audio_data.SampleRate = last_bt_sample_rate;
				DBG("[BT] Sample rate: %ld Hz, syncing DAC/ADC...\n", (long)last_bt_sample_rate);

				/* 同步 DAC 采样率 */
				AudioDAC_SampleRateChange(DAC0, last_bt_sample_rate);
				AudioDAC_SampleRateChange(DAC1, last_bt_sample_rate);

				/* 同步 ADC 采样率 */
				AudioADC_SampleRateSet(ADC0_MODULE, last_bt_sample_rate);
				AudioADC_SampleRateSet(ADC1_MODULE, last_bt_sample_rate);
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

/* Effect Graph 处理模式开关 (1=使用Effect Graph, 0=使用传统模式) */
#ifndef USE_EFFECT_GRAPH_MODE
#define USE_EFFECT_GRAPH_MODE  1
#endif

/**
 * Effect Graph 驱动的音频处理循环 (修正版 v4)
 * 
 * 核心设计思路 (严格参考老方案 AudioLoopWithBT):
 *   - BT 模式: 
 *     1. BT_GetAvailableData 预解码，获取实际帧长 n
 *     2. 用 n 等待 DAC 空间和 ADC 数据（与老方案 while 等待一致）
 *     3. 调用 EffectGraph_Process(n)，所有节点统一使用帧长 n
 *   - ADC 模式: 由 ADC 可用数据量驱动
 * 
 * 【修正 v4】解决模式频繁切换问题:
 *   - 使用更稳定的蓝牙活跃判断：只看 A2DP 状态，不依赖预解码缓存
 *   - 蓝牙模式下即使数据暂时不足也保持 BT 模式，等待数据到来
 *   - 避免 ADC/BT 模式频繁切换导致的失真
 */
static void AudioLoopWithGraph(void)
{
	uint16_t frame_size = 0;
	uint16_t processed_samples;
	uint16_t adc0_avail, adc1_avail;
	bool bt_streaming;
	EffectGraph_t *graph;
	const uint16_t MIN_FRAME = 48;
	const uint16_t MAX_FRAME = 640;  /* 支持 SBC 最大帧长 ~595 */
	static bool last_bt_streaming = false;  /* 上一帧的蓝牙状态 */
	
	/* 获取图实例 */
	graph = EffectGraph_GetInstance();
	if (!graph) {
		return;
	}
	
	/* 设置帧长限制 */
	graph->min_frame_size = MIN_FRAME;
	graph->max_frame_size = MAX_FRAME;
	
	/* 【修正】只看 A2DP 流状态，不依赖预解码数据状态 */
	/* 这样即使 SBC 缓冲区暂时数据不足，也保持蓝牙模式 */
	bt_streaming = (GetA2dpState() == BT_A2DP_STATE_STREAMING);
	
	/* 检测模式切换，只在真正切换时打印 */
	if (bt_streaming != last_bt_streaming) {
		if (bt_streaming) {
			DBG("[Audio] Switched to BT streaming mode\n");
		} else {
			DBG("[Audio] Switched to ADC mode\n");
		}
		last_bt_streaming = bt_streaming;
	}
	
	if (bt_streaming) {
		/* ========== 蓝牙驱动模式 (严格参考 AudioLoopWithBT) ========== */
		
		/* 1. 【关键】检查 SBC 缓冲区是否有数据（与老方案一致） */
		if (mv_msize(&SBC_MemHandle) <= SBC_DECODER_FIFO_MIN && !bt_has_decoded_data) {
			/* SBC 数据不足且无预解码数据，等待数据到来，不切换到 ADC 模式 */
			return;
		}
		
		/* 2. 预解码获取实际帧长（与老方案一致：先解码才知道 n） */
		frame_size = BT_GetAvailableData(NULL);
		if (frame_size == 0) {
			return;  /* 蓝牙真的没数据，等待 */
		}
		
		/* frame_size 现在是 BT 解码后的实际帧长 */
		
		/* 3. 【关键】阻塞等待 DAC 有足够空间 (与老方案一致!) */
		while (AudioDAC_DataSpaceLenGet(DAC0) < frame_size) {
			/* 忙等待 */
		}
		
		/* 4. 【关键】阻塞等待 ADC 有足够数据 (与老方案一致!) */
		while (AudioADC_DataLenGet(ADC0_MODULE) < frame_size || AudioADC_DataLenGet(ADC1_MODULE) < frame_size) {
			/* 忙等待 */
		}
		
		/* 5. 设置 BT 驱动模式（不打印日志避免刷屏） */
		graph->drive_mode = DRIVE_MODE_BT;
		
	} else {
		/* ========== ADC 驱动模式 (参考 AudioLoopMinimal) ========== */
		/* 清除可能残留的蓝牙预解码数据 */
		if (bt_has_decoded_data) {
			bt_has_decoded_data = false;
			bt_decoded_len = 0;
		}
		
		/* 【关键】蓝牙断开后恢复默认采样率 */
		if (bt_current_sample_rate != 0 && 
		    BG_AudioManager.Audio_data.SampleRate != system_default_sample_rate) {
			DBG("[Audio] BT disconnected, restoring sample rate to %ld Hz\n", 
			    (long)system_default_sample_rate);
			
			AudioDAC_SampleRateChange(DAC0, system_default_sample_rate);
			AudioDAC_SampleRateChange(DAC1, system_default_sample_rate);
			AudioADC_SampleRateSet(ADC0_MODULE, system_default_sample_rate);
			AudioADC_SampleRateSet(ADC1_MODULE, system_default_sample_rate);
			
			BG_AudioManager.Audio_data.SampleRate = system_default_sample_rate;
			bt_current_sample_rate = 0;
		}
		
		/* 获取 ADC 数据可用量 */
		adc0_avail = AudioADC_DataLenGet(ADC0_MODULE);
		adc1_avail = AudioADC_DataLenGet(ADC1_MODULE);
		
		/* 取最小值 */
		frame_size = (adc0_avail < adc1_avail) ? adc0_avail : adc1_avail;
		
		/* 帧大小限制 */
		if (frame_size < MIN_FRAME) {
			return; /* 数据不足 */
		}
		if (frame_size > MAX_FRAME) {
			frame_size = MAX_FRAME;
		}
		
		/* 设置 ADC 驱动模式（不打印日志避免刷屏） */
		graph->drive_mode = DRIVE_MODE_ADC;
	}
	
	/* 6. 调用 Effect Graph 处理 */
	processed_samples = EffectGraph_Process(frame_size);
	
	if (processed_samples > 0) {
		/* 更新 GPIO 检测状态 */
		ProcessGuitarOutput(processed_samples);
		ProcessMicOutput(processed_samples);
		ProcessSpeakerSwitch();
		
		BG_AudioManager.Audio_data.guitar_count++;
		BG_AudioManager.Audio_data.mic_count++;
	}
}

/**
 * 音频主循环处理函数
 */
void Audio_loop(void)
{
#if USE_EFFECT_GRAPH_MODE
	/* ==== 混合模式：蓝牙用老方案，非蓝牙用 Effect Graph ==== */
	static uint32_t bt_audio_buffer[640] = {0};  /* 支持 SBC 最大帧长 */

	BtStackServiceRun();
	SetVolume();
	OTG_DeviceRequestProcess();
	
	/* CDC串口任务处理 - 必须周期性调用以接收数据 */
	OTG_DeviceCDC_Task();
	
	// /* 检查是否有蓝牙音频数据要处理 */
	// if (GetA2dpState() == BT_A2DP_STATE_STREAMING)
	// {
	// 	if (mv_msize(&SBC_MemHandle) > SBC_DECODER_FIFO_MIN)
	// 	{
	// 		/* 蓝牙播放：严格使用老方案逻辑，确保稳定 */
	// 		AudioLoopWithBT(bt_audio_buffer);
	// 	}
	// }
	// else
	// {
	// 	/* 非蓝牙模式：使用 Effect Graph */
	// 	AudioLoopWithGraph();
	// }
	AudioLoopWithGraph();
	/* CDC串口应用处理 - 使用Shell IO管理器（自动切换CDC/BLE） */
	ShellIOManager_Process();

#else
	/* ==== 传统模式（保留用于对比/回退） ==== */
	static uint32_t bt_audio_buffer[640] = {0};  /* 支持 SBC 最大帧长 */

	BtStackServiceRun();
	SetVolume();
	OTG_DeviceRequestProcess();
	
	/* CDC串口任务处理 - 必须周期性调用以接收数据 */
	OTG_DeviceCDC_Task();

	/* 检查是否有蓝牙音频数据要处理 */
	if (GetA2dpState() == BT_A2DP_STATE_STREAMING)
	{
		if (mv_msize(&SBC_MemHandle) > SBC_DECODER_FIFO_MIN)
			AudioLoopWithBT(bt_audio_buffer);
	}
	else
	{
		AudioLoopMinimal(bt_audio_buffer);
	}
	
	/* CDC串口应用处理 - 使用Shell IO管理器（自动切换CDC/BLE） */
	ShellIOManager_Process();
#endif
}

// ==================== Effect Graph 音频设备回调实现 ====================

// ==================== 源节点可用数据量查询回调 ====================

/**
 * ADC0 可用数据量查询 - 吉他输入
 */
static uint16_t ADC0_GetAvailableData(EffectNode_t *node)
{
	(void)node;
	return AudioADC_DataLenGet(ADC0_MODULE);
}

/**
 * ADC1 可用数据量查询 - 麦克风输入
 */
static uint16_t ADC1_GetAvailableData(EffectNode_t *node)
{
	(void)node;
	return AudioADC_DataLenGet(ADC1_MODULE);
}

/**
 * USB 可用数据量查询 - USB音频输入
 */
static uint16_t USB_GetAvailableData(EffectNode_t *node)
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
static uint16_t BT_GetAvailableData(EffectNode_t *node)
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
	
	/* 限制帧长 */
	if (pcm_len > 640) {
		pcm_len = 640;
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
static uint16_t ADC0_ReadGuitarData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t samples_to_read;
	
	(void)node;
	
	/* 限制最大长度 */
	samples_to_read = max_len;
	if (samples_to_read > 640) {
		samples_to_read = 640;
	}
	
	/* 直接读取请求的长度（调用方已保证数据充足） */
	if (samples_to_read > 0) {
		AudioADC_DataGet(ADC0_MODULE, out_buf, samples_to_read);
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
static uint16_t ADC1_ReadMicData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t samples_to_read;
	
	(void)node;
	
	/* 限制最大长度 */
	samples_to_read = max_len;
	if (samples_to_read > 640) {
		samples_to_read = 640;
	}
	
	/* 直接读取请求的长度（调用方已保证数据充足） */
	if (samples_to_read > 0) {
		AudioADC_DataGet(ADC1_MODULE, out_buf, samples_to_read);
	}
	return samples_to_read;
}

/**
 * DAC Sink 回调 - 写入数据到 DAC0 扬声器输出
 */
static void DAC0_WriteSpeakerData(EffectNode_t *node, uint32_t *in_buf, uint16_t len)
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
		// 直接写入数据，无需类型转换
		AudioDAC_DataSet(DAC0, in_buf, samples_to_write);
	}
}

/**
 * USB Audio Source 回调 - 从 USB 读取音频数据
 * 参考老方案 BuildFinalOutput: 数据不足时填零，保证输出长度一致
 */
static uint16_t USB_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t available;
	uint16_t i;
	
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
		return max_len;
	}
	else if (available > 0) {
		/* USB 数据不足，读取可用数据，剩余填零 */
		UsbAudioSpeakerDataGet(out_buf, available);
		for (i = available; i < max_len; i++) {
			out_buf[i] = 0;
		}
		return max_len;
	}
	else {
		/* USB 无数据，全部填零避免噪声 */
		for (i = 0; i < max_len; i++) {
			out_buf[i] = 0;
		}
		return max_len;
	}
}

/**
 * USB Audio Sink 回调 - 写入音频数据到 USB
 */
static void USB_WriteAudioData(EffectNode_t *node, uint32_t *in_buf, uint16_t len)
{
	uint16_t samples_to_write;
	
	(void)node;
	
	// 检查 USB 麦克风是否启用
	if (!usb_mic_enable) {
		return;
	}
	
	samples_to_write = len;
	if (samples_to_write > 640) {
		samples_to_write = 640;
	}
	
	// 直接写入数据，无需类型转换
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
static uint16_t BT_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
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
		
		/* 直接复制缓存数据到输出缓冲区 */
		for (i = 0; i < pcm_len; i++) {
			out_buf[i] = bt_decoded_buffer[i];
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

// ==================== Effect Graph 效果器处理回调 ====================

/**
 * 混音器处理回调 - 将多路输入混合为一路输出
 */
static void Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	uint16_t i;
	uint8_t j;
	
	(void)node; /* 未使用，消除警告 */
	
	/* 清零输出缓冲区 */
	for (i = 0; i < len; i++) {
		out_buf[i] = 0;
	}
	
	/* 累加所有输入 */
	for (j = 0; j < in_count; j++) {
		if (in_bufs[j]) {
			for (i = 0; i < len; i++) {
				out_buf[i] += in_bufs[j][i];
			}
		}
	}
}

/**
 * 扩展器处理回调 - 动态范围扩展
 * 调用 SDK AudioEffectExpanderApply
 */
static void Expander_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
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
static void DRC_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	(void)node;
	
	if (in_count < 1 || !in_bufs[0]) {
		return;
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
 */
static void EQ_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	(void)node;
	
	if (in_count < 1 || !in_bufs[0]) {
		return;
	}
	
	#if CFG_AUDIO_EFFECT_MIC_OUT_EQ_EN
	if (gCtrlVars.mic_out_eq_unit.enable) {
		AudioEffectEQApply(&gCtrlVars.mic_out_eq_unit,
		                   (int16_t *)in_bufs[0],
		                   (int16_t *)out_buf,
		                   len,
		                   2);  /* 2 = 立体声 */
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
 * 直通处理回调 - 直接复制输入到输出，不做任何处理
 * 用于 USB/BT 路径，保持与老方案一致（BT 音频不经过效果处理）
 */
static void Passthrough_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
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
static void Reverb_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
{
	(void)node;
	
	if (in_count < 1 || !in_bufs[0]) {
		return;
	}
	
	if (gCtrlVars.reverb_unit.enable) {
		AudioEffectReverbApply(&gCtrlVars.reverb_unit,
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
 * 挂接 Effect Graph 节点的音频设备回调
 */
static void SetupEffectGraphCallbacks(void)
{
	EffectNode_t* node = NULL;
	
	DBG("[Audio] Setting up Effect Graph callbacks...\n");
	
	/* ===== 输入源节点回调 ===== */
	
	/* 挂接吉他输入节点 */
	node = EffectGraph_FindNodeByName("guitar_in");
	if (node) {
		node->func.source = ADC0_ReadGuitarData;
		node->avail_func = ADC0_GetAvailableData;
		DBG("[Audio] Guitar input callback registered\n");
	}
	
	/* 挂接麦克风输入节点 */
	node = EffectGraph_FindNodeByName("mic_in");
	if (node) {
		node->func.source = ADC1_ReadMicData;
		node->avail_func = ADC1_GetAvailableData;
		DBG("[Audio] Mic input callback registered\n");
	}
	
	/* 挂接 USB 输入节点（Speaker） */
	node = EffectGraph_FindNodeByName("usb_in");
	if (node) {
		node->func.source = USB_ReadAudioData;
		node->avail_func = USB_GetAvailableData;
		DBG("[Audio] USB input callback registered\n");
	}
	
	/* 挂接蓝牙输入节点 */
	node = EffectGraph_FindNodeByName("bt_in");
	if (node) {
		node->func.source = BT_ReadAudioData;
		node->avail_func = BT_GetAvailableData;
		DBG("[Audio] BT input callback registered\n");
	}
	
	/* ===== 输出节点回调 ===== */
	
	/* 挂接 DAC 输出节点 */
	node = EffectGraph_FindNodeByName("dac_out");
	if (node) {
		node->func.sink = DAC0_WriteSpeakerData;
		DBG("[Audio] DAC output callback registered\n");
	}
	
	/* 挂接 USB 输出节点（Mic） */
	node = EffectGraph_FindNodeByName("usb_out");
	if (node) {
		node->func.sink = USB_WriteAudioData;
		DBG("[Audio] USB output callback registered\n");
	}
	
	/* ===== 混音器节点回调 ===== */
	
	/* ADC 混音器 */
	node = EffectGraph_FindNodeByName("adc_mixer");
	if (node) {
		node->func.process = Mixer_Process;
		DBG("[Audio] ADC mixer callback registered\n");
	}
	
	/* USB/BT 混音器 */
	node = EffectGraph_FindNodeByName("usb_bt_mixer");
	if (node) {
		node->func.process = Mixer_Process;
		DBG("[Audio] USB/BT mixer callback registered\n");
	}
	
	/* 最终混音器 */
	node = EffectGraph_FindNodeByName("final_mixer");
	if (node) {
		node->func.process = Mixer_Process;
		DBG("[Audio] Final mixer callback registered\n");
	}
	
	/* ===== 效果器节点回调 ===== */
	
	/* 扩展器 */
	node = EffectGraph_FindNodeByName("expander");
	if (node) {
		node->func.process = Expander_Process;
		DBG("[Audio] Expander callback registered\n");
	}
	
	/* DRC */
	node = EffectGraph_FindNodeByName("drc");
	if (node) {
		node->func.process = DRC_Process;
		DBG("[Audio] DRC callback registered\n");
	}
	
	/* EQ (ADC 路径) */
	node = EffectGraph_FindNodeByName("eq");
	if (node) {
		node->func.process = EQ_Process;
		DBG("[Audio] EQ callback registered\n");
	}
	
	/* USB/BT EQ (快速路径) - 使用直通处理，与老方案一致（BT 音频不经过 EQ） */
	node = EffectGraph_FindNodeByName("usb_bt_eq");
	if (node) {
		node->func.process = Passthrough_Process;
		DBG("[Audio] USB/BT passthrough callback registered\n");
	}
	
	/* 混响 */
	node = EffectGraph_FindNodeByName("reverb");
	if (node) {
		node->func.process = Reverb_Process;
		DBG("[Audio] Reverb callback registered\n");
	}
	
	DBG("[Audio] All Effect Graph callbacks setup completed\n");
}
