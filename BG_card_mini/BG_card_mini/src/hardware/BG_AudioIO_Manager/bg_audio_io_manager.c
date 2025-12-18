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

#include "spi_flash.h"

// ==================== 全局缓冲区定义 ====================
#define SETTING_ADDR 0x80000000
uint32_t AudioADC1Buf[1024] = {0};
uint32_t AudioADC2Buf[1024] = {0};
uint16_t OutPut_bufx[AUDIO_BUF_MAX];

#define DAC_FIFO_SAMPLES 1024
uint32_t DAC0_FIFO[DAC_FIFO_SAMPLES];
#define DAC0_FIFO_LEN sizeof(DAC0_FIFO)
uint32_t DAC1_FIFO[DAC_FIFO_SAMPLES];
#define DAC1_FIFO_LEN sizeof(DAC1_FIFO)

// Looper音频缓冲区
static uint8_t looper_flash_buffer[512];
static uint32_t looper_playback_buffer[256];

// ==================== 外部变量 ====================
extern uint32_t usb_speaker_enable;
extern uint32_t usb_mic_enable;

// ==================== 前向声明 ====================
void BG_audio_Init(uint16_t SampleRate);
uint8_t BG_Audio_Det(void);
void Audio_loop(void);
void BG_Set_LineIn1_Vol(uint8_t vol);
void BG_Set_LineIn2_Vol(uint8_t vol);
void BG_Set_Mic_Vol(uint8_t vol);
void BG_Set_LineOut_Vol(uint16_t left_vol, uint16_t right_vol);
void BG_LineIn1_IsEnable(uint8_t Enable);
void BG_LineIn2_IsEnable(uint8_t Enable);
void BG_MIC_IsEnable(uint8_t Enable);

// CDC串口处理函数（可选的示例功能）
static void CDC_Process_Example(void);

// ==================== 管理器结构体初始化 ====================
BG_Audio_Io_Manager BG_AudioManager = {
	.Audio_Init = BG_audio_Init,
	.Audio_Loop = Audio_loop,
	.SetMicVol = BG_Set_Mic_Vol,
	.SetLineIn1Vol = BG_Set_LineIn1_Vol,
	.SetLineIn2Vol = BG_Set_LineIn2_Vol,
	.SetLineOutVol = BG_Set_LineOut_Vol,
	.LineIn1_OnOff = BG_LineIn1_IsEnable,
	.LineIn2_OnOff = BG_LineIn2_IsEnable,
	.MIC_OnOff = BG_MIC_IsEnable,
	.Audio_data = {
		.guitar_count = 0,
		.mic_count = 0,
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

// ==================== 前向声明（内部函数） ====================
static void SaveDataToSbcBuffer(uint8_t *data, uint16_t dataLen);

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
	gCtrlVars.reverb_unit.enable = 1;
	gCtrlVars.plate_reverb_unit.enable = 0;
	AudioEffectReverbInit(&gCtrlVars.reverb_unit, 2, SampleRate);
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
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA1);

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
	
	// 初始化Audio Looper（使用NOR Flash）
	AudioLooper.InitWithFlashType(FLASH_TYPE_NOR);
}

// ==================== 音量控制函数 ====================

/**
 * 设置LineIn1音量
 */
void BG_Set_LineIn1_Vol(uint8_t vol)
{
	AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_LEFT, vol);
}

/**
 * 设置LineIn2音量
 */
void BG_Set_LineIn2_Vol(uint8_t vol)
{
	AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_RIGHT, vol);
}

/**
 * 设置LineOut音量
 */
void BG_Set_LineOut_Vol(uint16_t left_vol, uint16_t right_vol)
{
	AudioDAC_VolSet(DAC0, left_vol, right_vol);
}

/**
 * 设置麦克风音量
 */
void BG_Set_Mic_Vol(uint8_t vol)
{
	AudioADC_VolSet(ADC1_MODULE, vol, vol);
}

// ==================== 输入使能控制 ====================

void BG_LineIn1_IsEnable(uint8_t Enable)
{
	// 功能预留
}

void BG_LineIn2_IsEnable(uint8_t Enable)
{
	// 功能预留
}

/**
 * 控制麦克风使能
 */
void BG_MIC_IsEnable(uint8_t Enable)
{
	BG_AudioManager.Audio_data.MicEnable = Enable;
	AudioADC_SoftMute(ADC1_MODULE, BG_AudioManager.Audio_data.MicEnable, BG_AudioManager.Audio_data.MicEnable);
}

// ==================== 检测和设置函数 ====================

/**
 * 检测音频输入设备状态变化
 */
uint8_t BG_Audio_Det(void)
{
	if (GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX30) != BG_AudioManager.Audio_data.MicEnable)
	{
		BG_AudioManager.Audio_data.MicEnable = GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX30);
		DBG("MIC %d\n", BG_AudioManager.Audio_data.MicEnable);
	}
	if (GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX29) != BG_AudioManager.Audio_data.LineOutEnable)
	{
		BG_AudioManager.Audio_data.LineOutEnable = GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX29);
		DBG("earphone %d\n", BG_AudioManager.Audio_data.LineOutEnable);
	}
	return 0;
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
		GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA17);
	}
	else
	{
		GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA17);
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
	}
	else
	{
		GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA1);
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
	}
	else
	{
		GPIO_RegOneBitSet(GPIO_B_OUT, GPIOB6);
	}
}

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
	AudioEffectReverbApply(&gCtrlVars.reverb_unit,
		(int16_t *)BG_AudioManager.Audio_data.OutPut_buf,
		(int16_t *)BG_AudioManager.Audio_data.guitar_buf_out,
		len);
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
				SampleRateCC = audio_decoder->song_info->sampling_rate;
				/* 同步DAC和ADC采样率，避免移频 */
				AudioDAC_SampleRateChange(DAC0, audio_decoder->song_info->sampling_rate);
				AudioDAC_SampleRateChange(DAC1, audio_decoder->song_info->sampling_rate);

				DBG("BT Audio Rate: %ld Hz (DAC+ADC synced)\n", (long)audio_decoder->song_info->sampling_rate);
			}

			n = audio_decoder->song_info->pcm_data_length;
			
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
	uint16_t n = 0;
	uint16_t i;
	const uint16_t MIN_SAMPLE = 48;

	if (AudioADC_DataLenGet(ADC0_MODULE) >= MIN_SAMPLE)
	{
		n = AudioDAC_DataSpaceLenGet(DAC0);
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
			else if (usb_data_len > 0)
			{
				/* USB数据不足，读取可用数据，剩余填零 */
				UsbAudioSpeakerDataGet(BG_AudioManager.Audio_data.USB_dac_buf, usb_data_len);
				for (i = usb_data_len; i < RealLen; i++)
				{
					BG_AudioManager.Audio_data.USB_dac_buf[i] = 0;
				}
			}
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
	if (RT_SUCCESS == audio_decoder_can_continue())
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

// ==================== 保留的原始函数（兼容性） ====================

void det_int(void)
{
	InitDetectionGPIO();
}
