/**
 * bg_audio_io_manager.c - 音频输入/输出管理器
 * 本文件包含重构后的函数划分版本
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_manager.h"
#include "product_def.h"
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
#include "remind_sound.h"
#include "app_config.h"		/* ENABLE_POWER_ON_SOUND */
#include "audio_effect.h"
#include "ctrlvars.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_stor.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "otg_device_audio.h"
#include "otg_device_cdc.h"
#include "reverb.h"
#include "bg_shell.h"
#include "shell_io_cdc.h"
#include "shell_io_ble.h"
#include "shell_io_manager.h"

#include "metronome.h"
#include "sra.h"
#include "spi_flash.h"
#include "resampler.h"
#include "audio_decoder_api.h"

// Effect Graph 模块
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "effect_graph_vfs.h"
#include "chain_graph_apply.h"
#include "shell_cmd_graph.h"

/* BanGTsynth 合成器源节点 */
#if BANGTSYNTH_EN
#include "bangtsynth_node.h"
#endif

#include "otg_device_cdc.h"
#include "product_def.h"
#include "sys_param.h"

// System Monitor 模块
#include "shell_cmd_sysmon.h"
#include "shell_cmd_metronome.h"
#include "shell_cmd_mode.h"
#include "shell_cmd_flash.h"

// 低功耗管理
#include "bg_low_power.h"

#include "bt_manager.h"
#include "rtos_api.h"
#include "FreeRTOS.h"

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
BG_Audio_Io_Manager BG_AudioManager __attribute__((section(".data"))) = {
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
#define BT_SBC_DECODER_INPUT_LEN (8 * 1024)
#define BT_SBC_LEVEL_HIGH (BT_SBC_DECODER_INPUT_LEN - BT_SBC_PACKET_SIZE * 4)
#define BT_SBC_LEVEL_LOW (BT_SBC_PACKET_SIZE * 6)
#define BT_SBC_LEVEL_START (BT_SBC_LEVEL_HIGH - BT_SBC_PACKET_SIZE * 3)
#define SBC_DECODER_FIFO_MIN (119 * 2)
/* 蓝牙解码最大帧长：SBC单帧最大128样本，双声道=256样本，640预留充足 */
#define BT_DECODED_BUFFER_SIZE 256   /* 优化：与 EFFECT_GRAPH_BUFFER_SIZE 对齐 */

uint8_t a2dp_sbcBuf[BT_SBC_DECODER_INPUT_LEN];
static uint8_t decoder_buf[1024 * 4] = {0};
static uint8_t DecoderInitialized = 0;

MemHandle SBC_MemHandle;
ResamplerContext bt_resmaper;

// ==================== 蓝牙预解码缓冲区（全局，供AudioLoopWithGraph和BT回调共享）====================
static uint32_t bt_decoded_buffer[BT_DECODED_BUFFER_SIZE];  /* 优化：从640降到256，节省 1536 bytes */
static uint16_t bt_decoded_len = 0;     /* 预解码数据长度 */
static bool bt_has_decoded_data = false; /* 是否有预解码数据 */
static uint32_t bt_current_sample_rate = 0; /* 当前蓝牙音频采样率 */
static uint32_t system_default_sample_rate = 44100; /* 系统默认采样率（蓝牙断开后恢复） */

// ==================== BT/USB 音量增益（Q8定点数，由SetVolume计算） ====================
static uint16_t s_bt_gain_q8  = 256;  /* BT音乐增益 256=1.0x，由bt_max_volume和旋钮映射 */
static uint16_t s_usb_gain_q8 = 256;  /* USB音乐增益 256=1.0x，由usb_max_volume和旋钮映射 */

// ==================== USB输出（device→PC）音量控制 ====================
static uint16_t s_usb_out_gain_q8 = 256;  /* USB输出增益 256=1.0x，由usb_out_volume映射 */
static uint8_t  s_usb_out_mute = 0;       /* USB输出静音 0=off 1=on */

// ==================== 前向声明（内部函数） ====================
static void SaveDataToSbcBuffer(uint8_t *data, uint16_t dataLen);

// Effect Graph 音频设备回调函数 - 源节点
static uint16_t ADC0_ReadGuitarData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
static uint16_t ADC1_ReadMicData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
static uint16_t USB_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
static uint16_t BT_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);

// Metronome、Remind 和 Looper 源节点回调
static uint16_t Metronome_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
static uint16_t Metronome_GetAvailCallback(EffectNode_t *node);
static uint16_t Remind_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
static uint16_t Remind_GetAvailCallback(EffectNode_t *node);
static uint16_t LooperPlay_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
static uint16_t LooperPlay_GetAvailCallback(EffectNode_t *node);

// Looper 录制 Sink 节点回调
static void LooperRecord_SinkCallback(EffectNode_t *node, uint32_t *in_buf, uint16_t len);

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
static void ADC_Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
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
static void InitADC0LineIn(uint16_t SampleRate)
{
	AudioADC_AnaInit();
	AudioADC_DynamicElementMatch(ADC0_MODULE, TRUE, TRUE);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_RIGHT, LINEIN_NONE);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_LEFT, LINEIN_NONE);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_RIGHT, LINEIN5_RIGHT);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_LEFT, LINEIN5_LEFT);
	AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_RIGHT, LINEIN5_RIGHT, 32, 0);
	AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_LEFT, LINEIN5_LEFT, 32, 0);
	AudioADC_VcomConfig(1);
	AudioADC_DigitalInit(ADC0_MODULE, SampleRate, (void *)AudioADC1Buf, sizeof(AudioADC1Buf));
}

// 初始化ADC1（麦克风）
static void InitADC1Mic(uint16_t SampleRate)
{
	AudioADC_DynamicElementMatch(ADC1_MODULE, TRUE, TRUE);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1);
	AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2, 28, 1);
	AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_LEFT,  LINEIN3_LEFT_OR_MIC1,  28, 1);
	AudioADC_MicBias1Enable(TRUE);
	AudioADC_VcomConfig(1);
	AudioADC_DigitalInit(ADC1_MODULE, SampleRate, (void *)AudioADC2Buf, sizeof(AudioADC2Buf));

}



// 初始化音频效果（混响等）
static void InitAudioEffects(uint16_t SampleRate)
{
	extern int osPortRemainMem(void);  /* 获取剩余内存 */
	int mem_before, mem_after;
	
	gCtrlVars.audio_effect_init_flag = 1;
	
	APP_DBG("[AudioInit] Memory available at start: %d bytes\n", osPortRemainMem());
	
	// 混响效果（Reverb）
	mem_before = osPortRemainMem();
	gCtrlVars.reverb_unit.enable = 1;
	gCtrlVars.plate_reverb_unit.enable = 0;
	AudioEffectReverbInit(&gCtrlVars.reverb_unit, 2, SampleRate);
	mem_after = osPortRemainMem();
	APP_DBG("[AudioInit] Reverb allocated: %d bytes (remain: %d)\n", mem_before - mem_after, mem_after);

	// 动态范围压缩（DRC）- ADC输入通道
	mem_before = osPortRemainMem();
	gCtrlVars.mic_drc_unit.enable = 1;
	AudioEffectDRCInit(&gCtrlVars.mic_drc_unit, 2, SampleRate);
	mem_after = osPortRemainMem();
	APP_DBG("[AudioInit] DRC allocated: %d bytes (remain: %d)\n", mem_before - mem_after, mem_after);

	/* ========== 仅初始化效果图实际使用的5个EQ单元 ========== */
	/* 节点4-7: ADC通道独立EQ (单声道, 10段) */
	APP_DBG("[AudioInit] Initializing 4x ADC EQ (mono, 10-band)...\n");
	mem_before = osPortRemainMem();
	
	gCtrlVars.eq_guitar_l_unit.enable = 1;
	gCtrlVars.eq_guitar_l_unit.channel = 1;
	AudioEffectEQInit(&gCtrlVars.eq_guitar_l_unit, 1, SampleRate);
	mem_after = osPortRemainMem();
	APP_DBG("[AudioInit] EQ_guitar_l: en=%d ct=%p allocated=%d (remain: %d)\n", 
		gCtrlVars.eq_guitar_l_unit.enable, gCtrlVars.eq_guitar_l_unit.ct, mem_before - mem_after, mem_after);
	
	mem_before = osPortRemainMem();
	gCtrlVars.eq_guitar_r_unit.enable = 1;
	gCtrlVars.eq_guitar_r_unit.channel = 1;
	AudioEffectEQInit(&gCtrlVars.eq_guitar_r_unit, 1, SampleRate);
	mem_after = osPortRemainMem();
	APP_DBG("[AudioInit] EQ_guitar_r: en=%d ct=%p allocated=%d (remain: %d)\n", 
		gCtrlVars.eq_guitar_r_unit.enable, gCtrlVars.eq_guitar_r_unit.ct, mem_before - mem_after, mem_after);
	
	mem_before = osPortRemainMem();
	gCtrlVars.eq_mic_l_unit.enable = 1;
	gCtrlVars.eq_mic_l_unit.channel = 1;
	AudioEffectEQInit(&gCtrlVars.eq_mic_l_unit, 1, SampleRate);
	mem_after = osPortRemainMem();
	APP_DBG("[AudioInit] EQ_mic_l: en=%d ct=%p allocated=%d (remain: %d)\n", 
		gCtrlVars.eq_mic_l_unit.enable, gCtrlVars.eq_mic_l_unit.ct, mem_before - mem_after, mem_after);
	
	mem_before = osPortRemainMem();
	gCtrlVars.eq_mic_r_unit.enable = 1;
	gCtrlVars.eq_mic_r_unit.channel = 1;
	AudioEffectEQInit(&gCtrlVars.eq_mic_r_unit, 1, SampleRate);
	mem_after = osPortRemainMem();
	APP_DBG("[AudioInit] EQ_mic_r: en=%d ct=%p allocated=%d (remain: %d)\n", 
		gCtrlVars.eq_mic_r_unit.enable, gCtrlVars.eq_mic_r_unit.ct, mem_before - mem_after, mem_after);

	/* 节点14: USB/BT路径EQ (双声道) */
	APP_DBG("[AudioInit] Initializing USB/BT EQ (stereo)...\n");
	mem_before = osPortRemainMem();
	gCtrlVars.music_out_eq_unit.enable = 1;
	gCtrlVars.music_out_eq_unit.channel = 2;
	/* 修正类型：BAND_PASS会导致音量衰减严重 */
	{
		int i;
		for (i = 0; i < 10; i++) {
			if (gCtrlVars.music_out_eq_unit.eq_params[i].type == 5) {
				gCtrlVars.music_out_eq_unit.eq_params[i].type = 0;  /* PEAKING */
			}
			if (gCtrlVars.music_out_eq_unit.filter_params && i < gCtrlVars.music_out_eq_unit.filter_count) {
				if (gCtrlVars.music_out_eq_unit.filter_params[i].type == 5) {
					gCtrlVars.music_out_eq_unit.filter_params[i].type = 0;
				}
			}
		}
	}
	AudioEffectEQInit(&gCtrlVars.music_out_eq_unit, 2, SampleRate);
	mem_after = osPortRemainMem();
	APP_DBG("[AudioInit] USB/BT_EQ: en=%d ct=%p allocated=%d (remain: %d)\n", 
		gCtrlVars.music_out_eq_unit.enable, gCtrlVars.music_out_eq_unit.ct, mem_before - mem_after, mem_after);
	
	APP_DBG("[AudioInit] All EQ initialization completed, final memory: %d bytes\n", osPortRemainMem());

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
#ifndef BANBOX_II
	// GPIO_B6: 扬声器/耳机切换 (BANBOX_II: B6 = PSRAM CS, 不能做扬声器切换)
	GPIO_RegOneBitClear(GPIO_B_IE, GPIOB6);
	GPIO_RegOneBitSet(GPIO_B_OE, GPIOB6);
	GPIO_RegOneBitSet(GPIO_B_OUT, GPIOB6);
#endif

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
#if LINE1_INPUT_DETECT_EN && !defined(BANBOX_II)
	// GPIO_A_INDEX29: 吉他检测输入，上拉
	// (BANBOX_II: A29 = NAND Flash CS, 不能做吉他检测)
	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX29);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX29);
	GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX29);
	GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX29);
#endif
#if LINE2_INPUT_DETECT_EN
	ADC_PowerkeyChannelEnable();
#endif
#if MIC_INPUT_DETECT_EN
	// GPIO_A_INDEX30: 麦克风检测输入，下拉
	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX30);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX30);
	GPIO_RegOneBitClear(GPIO_A_PU, GPIO_INDEX30);
	GPIO_RegOneBitSet(GPIO_A_PD, GPIO_INDEX30);
#endif
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
	InitADC0LineIn(SampleRate);
	InitADC1Mic(SampleRate);

	/* 开机消噪：DAC先静音，防止DAC启动时的pop噪声输出到扬声器
	 * 同时ADC也静音，等VCOM/PGA稳定后再一起解除 */
	AudioDAC_VolSet(DAC0, 0, 0);  /* DAC0 左右声道静音 */
	AudioDAC_VolSet(DAC1, 0, 0);  /* DAC1 静音 */
	AudioADC_SoftMute(ADC0_MODULE, TRUE, TRUE);
	AudioADC_SoftMute(ADC1_MODULE, TRUE, TRUE);

	/* 开机 VCOM/PGA 稳定等待：ADC 模拟前端（VCOM 参考电压、PGA 建立）
	 * 需要约 200~500ms 才能稳定，期间 ADC 输出包含 DC 偏移 + HPF 瞬态失真。
	 * 先静音，等 300ms 后再解除，消除开机时几秒钟的失真现象。 */
	vTaskDelay((300 + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS);
	AudioADC_SoftMute(ADC0_MODULE, FALSE, FALSE);
	AudioADC_SoftMute(ADC1_MODULE, FALSE, FALSE);
	/* DAC音量将在Audio_loop()的SetVolume()中恢复，无需手动解除 */

	InitAudioEffects(SampleRate);
	InitControlGPIO();
	InitDetectionGPIO();

	/* 开机提示音：非阻塞模式，通过 Effect Graph REMIND 源节点混音输出
	 * 解码器缓冲区为静态分配（19KB BSS），不与效果器争夺堆内存，
	 * 因此可在 InitAudioEffects 之后启动 */
#if ENABLE_POWER_ON_SOUND
	RemindSound_Init();
	RemindSound_Start("on");
#endif
	A2dp_DecoderInit();
	BtStackServiceStart();
	
	// 初始化Shell IO管理器（自动管理CDC和BLE接口）
	ShellIOManager_Init();

	// ========== Effect Graph 初始化 ==========
#if EFFECT_GRAPHICS_EN
	DBG("[Audio] Initializing Effect Graph...\n");
	
	// 1. 初始化 Effect Graph 核心模块
	if (EffectGraph_Init() != 0) {
		DBG("[Audio] ERROR: Effect Graph Init failed!\n");
		// 不要直接return，继续初始化其他组件
	} else {
		// 2. 加载默认预设（可根据需求选择其他预设）
		if (EffectGraphConfig_LoadPreset(GRAPH_PRESET_DEFAULT) != 0) {
			DBG("[Audio] ERROR: Effect Graph Load Preset failed!\n");
			DBG("[Audio] Attempting fallback initialization...\n");
			// 尝试继续，因为回调设置可能仍然可以工作
		}

		// 3. 自动应用保存的chain graphs（如果有的话）
		// 【修复】启用自动应用，从保存的配置恢复音频链路
		DBG("[Audio] Auto-applying saved chain graphs...\n");
		ChainGraph_AutoApplyOnStartup();

		// 4. 自动挂载效果图到VFS（供命令行和文件系统访问）
		EffectGraphVfs_TryAutoMount();

		// 5. 挂接实际音频设备回调
		BG_AudioIO_SetupEffectGraphCallbacks();
	}
#endif /* EFFECT_GRAPHICS_EN */

	// 6. 注册 Shell 命令（支持 CDC/BLE 远程控制）
	ShellCmdGraph_Register();

	// 7. 注册系统监控命令（CPU/内存/任务统计）
	ShellCmdSysmon_Register();
	
	// 8. 注册节拍器命令
	ShellCmdMetronome_Register();
	
	// 9. 注册模式切换命令（主音箱/副音箱模式）
	ShellCmdMode_Register();

        // 10. 注册 NAND Flash 测试命令
        ShellCmdFlash_Register();
	// ==========================================

	// 开机自动初始化 Audio Looper（存储类型由 LOOPER_STORAGE_TYPE 宏决定，默认自动检测）
	AudioLooper.Init();
	
	// 初始化节拍器模块
	MetronomeModule.Init();

	// 初始化低功耗管理器（所有音频模块初始化完毕后调用）
	LowPower_Init();

//	AudioDAC_FadeDisable(DAC0);
//
//
//	AudioSetting_SetMic1VolumePercent(g_sys_param.volume.mic1_volume);
//
//
//	AudioSetting_SetMic2VolumePercent(g_sys_param.volume.mic2_volume);
//
//
//
//	AudioSetting_SetGuitar1VolumePercent( g_sys_param.volume.guitar1_volume );
//
//
//	AudioSetting_SetGuitar2VolumePercent( g_sys_param.volume.guitar2_volume );



}
/**
 * 设置输出音量（通过ADC读取电位器值）
 * 同时计算BT/USB音乐的增益映射（Q8定点数）
 *
 * 增益映射逻辑：
 *   wheel_pct = DC_Data / 0x3FFF  (0~1)
 *   bt_gain   = wheel_pct * bt_max_volume  / 100
 *   usb_gain  = wheel_pct * usb_max_volume / 100
 *   Q8 = gain * 256
 */
static void SetVolume(void)
{
	uint16_t DC_Data;
	uint32_t wheel_pct;  /* 0~16383, 即 wheel_pct = DC_Data */
#if HW_VOLUME_ADC_EN
	GPIO_RegOneBitClear(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
	GPIO_RegOneBitSet(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
	DC_Data = ADC_SingleModeDataGet(HW_VOLUME_ADC_CHANNEL) * 4;
#else
	/* BANBOX_II: 无音量旋钮，固定最大音量 */
	DC_Data = 0x3FFF;
#endif
	AudioDAC_VolSet(DAC0, DC_Data, DC_Data);
	AudioDAC_VolSet(DAC1, DC_Data, 0);

	/* 计算BT/USB增益映射 */
	wheel_pct = DC_Data;  /* 0~16383 */
	/* bt_gain_q8 = wheel_pct * bt_max_volume / 16383 * 256 / 100
	 *            = wheel_pct * bt_max_volume * 256 / (16383 * 100)
	 * 简化: 先算 wheel_pct * 256 / 16383 得到旋钮Q8，再乘 bt_max_volume / 100 */
	s_bt_gain_q8  = (uint16_t)((uint32_t)wheel_pct * g_sys_param.volume.bt_max_volume  * 256 / (16383 * 100));
	s_usb_gain_q8 = (uint16_t)((uint32_t)wheel_pct * g_sys_param.volume.usb_max_volume * 256 / (16383 * 100));
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
static void ProcessGuitarOutput()
{
#ifdef BANBOX_II
	/* BANBOX_II: A29 = NAND Flash CS, 不能读取吉他检测信号 */
	(void)0;
#else
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
#endif /* !BANBOX_II */
}

/**
 * 处理麦克风信号输出
 */
static void ProcessMicOutput()
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
#ifndef BANBOX_II
		GPIO_RegOneBitClear(GPIO_B_OUT, GPIOB6); /* BANBOX_II: B6 = PSRAM CS */
#endif
		BG_AudioManager.Audio_data.det_state  = SPEAKER_DET;
	}
	else
	{
#ifndef BANBOX_II
		GPIO_RegOneBitSet(GPIO_B_OUT, GPIOB6); /* BANBOX_II: B6 = PSRAM CS */
#endif
		BG_AudioManager.Audio_data.det_state  = EARPHONE_DET;
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
	

	if (gCtrlVars.mic_out_eq_unit.enable)
	{
		AudioEffectEQApply(&gCtrlVars.mic_out_eq_unit,
		                   (int16_t *)temp_buf2,
		                   (int16_t *)temp_buf1,
		                   len,
		                   2);
	}
	else
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
	static uint16_t s_gpio_div_bt = 0;        /* GPIO 检测降频计数器 */
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
			if (++s_gpio_div_bt >= 50)
			{
				s_gpio_div_bt = 0;
				ProcessGuitarOutput();
				ProcessMicOutput();
				ProcessSpeakerSwitch();
			}
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
	/* GPIO 检测降频：插拔事件是慢速 DC 事件，每 50 帧 (~50ms) 检测一次即可，
	 * 避免每帧 GPIO 写操作产生 ~1kHz 方波，耦合到 ADC 输入造成高频底噪 */
	static uint16_t s_gpio_div_minimal = 0;

	while(AudioADC_DataLenGet(ADC0_MODULE) >= MIN_SAMPLE)
	{
		ReadAudioData(MIN_SAMPLE, &RealLen);
		ApplyAudioEffects(RealLen);
		if (++s_gpio_div_minimal >= 50)
		{
			s_gpio_div_minimal = 0;
			ProcessGuitarOutput();
			ProcessMicOutput();
			ProcessSpeakerSwitch();
		}

		/* Looper录制处理 - 根据各段配置的录制源分别采集:
		 * ALL_MIX → guitar_buf_out (混音后信号)
		 * MIC_L/R → mic_buf_in (ADC1 原始)
		 * LINEIN_L/R → guitar_buf_in (ADC0 原始) */
		g_looper_src_mic    = BG_AudioManager.Audio_data.mic_buf_in;
		g_looper_src_linein = BG_AudioManager.Audio_data.guitar_buf_in;
		if (AudioLooper.IsRecording())
		{
			AudioLooper.ProcessRecording32(BG_AudioManager.Audio_data.guitar_buf_out,
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
				/* 逐声道饱和加法：guitar_out + USB + looper */
				int32_t acc_l = (int16_t)(BG_AudioManager.Audio_data.guitar_buf_out[i] & 0xFFFF)
				              + (int16_t)(BG_AudioManager.Audio_data.USB_dac_buf[i]    & 0xFFFF)
				              + (int16_t)(looper_playback_buffer[i]                    & 0xFFFF);
				int32_t acc_r = (int16_t)((BG_AudioManager.Audio_data.guitar_buf_out[i] >> 16) & 0xFFFF)
				              + (int16_t)((BG_AudioManager.Audio_data.USB_dac_buf[i]    >> 16) & 0xFFFF)
				              + (int16_t)((looper_playback_buffer[i]                    >> 16) & 0xFFFF);
				if (acc_l >  32767) acc_l =  32767;
				if (acc_l < -32768) acc_l = -32768;
				if (acc_r >  32767) acc_r =  32767;
				if (acc_r < -32768) acc_r = -32768;
				BG_AudioManager.Audio_data.OutPut_buf[i] =
					((uint32_t)(uint16_t)(int16_t)acc_r << 16) | ((uint16_t)(int16_t)acc_l & 0xFFFF);
			}
		}
		else
		{
			for (i = 0; i < RealLen; i++)
			{
				/* 逐声道饱和加法：guitar_out + mic + looper */
				int32_t acc_l = (int16_t)(BG_AudioManager.Audio_data.guitar_buf_out[i] & 0xFFFF)
				              + (int16_t)(BG_AudioManager.Audio_data.mic_buf_in[i]    & 0xFFFF)
				              + (int16_t)(looper_playback_buffer[i]                  & 0xFFFF);
				int32_t acc_r = (int16_t)((BG_AudioManager.Audio_data.guitar_buf_out[i] >> 16) & 0xFFFF)
				              + (int16_t)((BG_AudioManager.Audio_data.mic_buf_in[i]    >> 16) & 0xFFFF)
				              + (int16_t)((looper_playback_buffer[i]                  >> 16) & 0xFFFF);
				if (acc_l >  32767) acc_l =  32767;
				if (acc_l < -32768) acc_l = -32768;
				if (acc_r >  32767) acc_r =  32767;
				if (acc_r < -32768) acc_r = -32768;
				BG_AudioManager.Audio_data.OutPut_buf[i] =
					((uint32_t)(uint16_t)(int16_t)acc_r << 16) | ((uint16_t)(int16_t)acc_l & 0xFFFF);
			}
		}
		handle_usb_record(RealLen);
		OutputAudioData(RealLen);

#if LOOPER_IO_BUFFER_ENABLE
		/* 音频已输出到DAC，现在安全执行Flash IO（刷写缓冲 + 填读缓存）
		 * 即使Flash写入耗时较长(~0.8ms)也不影响当帧音频输出 */
		looper_flush_io();
#endif
	}
}

// ==================== 主音频循环 ====================

/* Effect Graph 处理模式开关 (1=使用Effect Graph, 0=使用传统模式)
 * 当前强制设为 0，使用传统 AudioLoopMinimal 路径验证 Looper 正确性 */
#ifdef USE_EFFECT_GRAPH_MODE
#undef USE_EFFECT_GRAPH_MODE
#endif
#define USE_EFFECT_GRAPH_MODE  1

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

uint8_t flag_on=1;
static void AudioLoopWithGraph(void)
{
	uint16_t frame_size = 0;
	uint16_t processed_samples;
	uint16_t adc0_avail, adc1_avail;
	bool bt_streaming;
	EffectGraphRuntime_t *graph;
	const uint16_t MIN_FRAME = 48;
	const uint16_t MAX_FRAME = BT_DECODED_BUFFER_SIZE;  /* 与缓冲区大小对齐 */
	static bool last_bt_streaming = false;  /* 上一帧的蓝牙状态 */
	static uint16_t s_gpio_div_graph = 0;   /* GPIO 检测降频计数器 */

	// if(flag_on){
	// 	const char *cmd = "sb -t 60 20 3000\r";
	// 	Shell_InputData((uint8_t *)cmd, strlen(cmd));
	// 	flag_on = 0;
	// }
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
	
	/* 6. Looper 录制/播放时强制 frame_size=48
	 * 原因：Looper 每帧处理 48 个采样，凑满 64 采样(256字节)写一页 PSRAM。
	 * 若 frame_size>48，录制端只保存前 48 个采样（丢弃剩余），
	 * 导致音频时间压缩 → 播放加速。
	 * 强制 frame_size=48 使录制和播放速率匹配
	 */
	if (AudioLooper.IsRecording() || AudioLooper.IsPlaying()) {
		if (frame_size > 48) {
			frame_size = 48;
		}
	}

	/* 【内存保护】frame_size 绝不能超过 EFFECT_GRAPH_BUFFER_SIZE，否则节点缓冲区溢出 */
	if (frame_size > EFFECT_GRAPH_BUFFER_SIZE) {
		frame_size = EFFECT_GRAPH_BUFFER_SIZE;
	}

	/* 调用 Effect Graph 处理 */
	processed_samples = EffectGraph_Process(frame_size);
	
	if (processed_samples > 0) {
		/* 更新 GPIO 检测状态（每 50 帧一次，避免高频 GPIO 切换耦合噪声） */
		if (++s_gpio_div_graph >= 50)
		{
			s_gpio_div_graph = 0;
			ProcessGuitarOutput();
			ProcessMicOutput();
			ProcessSpeakerSwitch();
		}
		
		BG_AudioManager.Audio_data.guitar_count++;
		BG_AudioManager.Audio_data.mic_count++;
	}

#if LOOPER_IO_BUFFER_ENABLE
	/* Effect Graph已处理完毕（含 DAC 输出），现在安全执行Flash IO */
	if (processed_samples > 0) {
		looper_flush_io();
	}
#endif
}

/**
 * 音频主循环处理函数
 */
/**
 * @brief USB 状态更新（不再动态 enable/disable，避免 USB 重初始化失败）
 *
 * USB 在 InitUSBDevice() 中已一次性完整初始化（OTG_DeviceModeSel + UsbDevicePlayInit + UsbDeviceEnable）。
 * 这里只更新连接状态供状态栏显示，不再对硬件做任何操作。
 */
static void USB_HotplugCheck(void)
{
	/* 只记录连接状态，不做 enable/disable操作 */
	bool now_connected = OTG_PortDeviceIsLink();
	// if (now_connected) {
	// 	DBG("[USB] Device linked\n");
	// }
}

void Audio_loop(void)
{
#if USE_EFFECT_GRAPH_MODE
	/* ==== 混合模式：蓝牙用老方案，非蓝牙用 Effect Graph ==== */
	{
	uint8_t lp_activity = 0;

	BtStackServiceRun();
	SetVolume();  /* 【修复】恢复音量设置，否则所有音频静音 */
	OTG_DeviceRequestProcess();

	/* CDC串口任务处理 - 必须周期性调用以接收数据 */
	OTG_DeviceCDC_Task();

	/* USB 热插拔检测（与 UI 解耦，直接在音频系统处理） */
	USB_HotplugCheck();

	/* ==================== 低功耗活动检测 ==================== */
	if (usb_speaker_enable || usb_mic_enable)
		lp_activity |= LP_ACT_USB_AUDIO;
	if (GetA2dpState() == BT_A2DP_STATE_STREAMING)
		lp_activity |= LP_ACT_BT_AUDIO;
	if (OTG_DeviceCDC_GetRxCount() > 0)
		lp_activity |= LP_ACT_CDC_COMM;
	if (ShellIOManager_HasIncomingData())
		lp_activity |= LP_ACT_BLE_COMM;
	if (lp_activity)
		LowPower_FeedActivity(lp_activity);
	/* ADC 信号活动由 ADC 源节点回调自动喂入（正常模式），
	 * 低功耗模式下由 LowPower_Tick 内部旁路检测接管 */
	LowPower_Tick();
	/* ======================================================= */

	if (!LowPower_IsLowPower()) {
		/* 正常模式：执行音频图处理 */
		AudioLoopWithGraph();
		/* 消费回绕触发的 pending 标志（衔接/接入/延迟停止），必须在音频处理之后调用 */
		Looper_TimedOps_Process();
	}
	/* 低功耗模式下仍处理 Shell，以便接收唤醒命令 */
	ShellIOManager_Process();
	} /* end block */

#else
	/* ==== 传统模式（保留用于对比/回退） ==== */
	static uint32_t bt_audio_buffer[640] = {0};  /* 支持 SBC 最大帧长 */

	BtStackServiceRun();
	SetVolume();
	OTG_DeviceRequestProcess();

	/* CDC串口任务处理 - 必须周期性调用以接收数据 */
	OTG_DeviceCDC_Task();

	/* USB 热插拔检测（与 UI 解耦，直接在音频系统处理） */
	USB_HotplugCheck();

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

	/* 消费回绕触发的 pending 标志（衔接/接入/延迟停止），必须在音频处理之后调用 */
	Looper_TimedOps_Process();
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
	
	/* 限制帧长到 BT_DECODED_BUFFER_SIZE (与效果图缓冲区对齐) */
	if (pcm_len > BT_DECODED_BUFFER_SIZE) {
		pcm_len = BT_DECODED_BUFFER_SIZE;
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

	/* 读取ADC数据（32位=L/R两个16位声道打包） */
	if (samples_to_read > 0) {
		AudioADC_DataGet(ADC0_MODULE, out_buf, samples_to_read);
		/* 同步到共享缓冲区供 Looper 按源选择时直接访问 */
		memcpy(BG_AudioManager.Audio_data.guitar_buf_in, out_buf, samples_to_read * sizeof(uint32_t));
		/* 低功耗：检测吉他输入信号是否超过门限 */
		LowPower_CheckADCSignal(out_buf, samples_to_read);
		/* 提示音播放期间，ADC数据不输出给DAC（仍读取以消耗FIFO） */
		if (RemindSound_IsPlaying()) {
			memset(out_buf, 0, samples_to_read * sizeof(uint32_t));
		}
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

	/* 读取ADC数据（32位=L/R两个16位声道打包） */
	if (samples_to_read > 0) {
		AudioADC_DataGet(ADC1_MODULE, out_buf, samples_to_read);
		/* 同步到共享缓冲区供 Looper 按源选择时直接访问 */
		memcpy(BG_AudioManager.Audio_data.mic_buf_in, out_buf, samples_to_read * sizeof(uint32_t));
		/* 低功耗：检测麦克风输入信号是否超过门限 */
		LowPower_CheckADCSignal(out_buf, samples_to_read);
		/* 提示音播放期间，ADC数据不输出给DAC（仍读取以消耗FIFO） */
		if (RemindSound_IsPlaying()) {
			memset(out_buf, 0, samples_to_read * sizeof(uint32_t));
		}
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
 * 应用 usb_max_volume 增益映射（Q8定点数乘法）
 */
static uint16_t USB_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t available;
	uint16_t i;
	uint16_t ret_len;
	
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
		ret_len = max_len;
	}
	else if (available > 0) {
		/* USB 数据不足，读取可用数据，剩余填零 */
		UsbAudioSpeakerDataGet(out_buf, available);
		for (i = available; i < max_len; i++) {
			out_buf[i] = 0;
		}
		ret_len = max_len;
	}
	else {
		/* USB 无数据，全部填零避免噪声 */
		for (i = 0; i < max_len; i++) {
			out_buf[i] = 0;
		}
		ret_len = max_len;
	}
	
	/* 应用USB音乐增益映射 (Q8定点数乘法) */
	if (s_usb_gain_q8 != 256) {
		int16_t *samples = (int16_t *)out_buf;
		for (i = 0; i < ret_len * 2; i++) {
			int32_t s = (int32_t)samples[i] * s_usb_gain_q8 >> 8;
			if (s > 32767) s = 32767;
			else if (s < -32768) s = -32768;
			samples[i] = (int16_t)s;
		}
	}
	
	return ret_len;
}

/**
 * USB Audio Sink 回调 - 写入音频数据到 USB
 * 应用 usb_out_volume 增益和 usb_out_mute 静音控制
 */
static void USB_WriteAudioData(EffectNode_t *node, uint32_t *in_buf, uint16_t len)
{
	uint16_t samples_to_write;
	uint16_t i;
	
	(void)node;
	
	// 检查 USB 麦克风是否启用
	if (!usb_mic_enable) {
		return;
	}
	
	samples_to_write = len;
	if (samples_to_write > 640) {
		samples_to_write = 640;
	}
	
	/* USB输出静音：发送零数据 */
	if (s_usb_out_mute) {
		uint32_t zero_buf[640];
		memset(zero_buf, 0, sizeof(uint32_t) * samples_to_write);
		UsbAudioMicDataSet(zero_buf, samples_to_write);
		return;
	}
	
	/* 应用USB输出增益 (Q8定点数乘法) */
	if (s_usb_out_gain_q8 != 256) {
		int16_t *samples = (int16_t *)in_buf;
		for (i = 0; i < samples_to_write * 2; i++) {
			int32_t s = (int32_t)samples[i] * s_usb_out_gain_q8 >> 8;
			if (s > 32767) s = 32767;
			else if (s < -32768) s = -32768;
			samples[i] = (int16_t)s;
		}
	}
	
	// 写入数据
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
		
		/* 复制缓存数据到输出缓冲区，同时应用BT音乐增益映射 */
		if (s_bt_gain_q8 == 256) {
			/* 增益1.0x，直接复制 */
			for (i = 0; i < pcm_len; i++) {
				out_buf[i] = bt_decoded_buffer[i];
			}
		} else {
			/* 应用Q8增益 */
			int16_t *dst = (int16_t *)out_buf;
			int16_t *src = (int16_t *)bt_decoded_buffer;
			for (i = 0; i < pcm_len * 2; i++) {
				int32_t s = (int32_t)src[i] * s_bt_gain_q8 >> 8;
				if (s > 32767) s = 32767;
				else if (s < -32768) s = -32768;
				dst[i] = (int16_t)s;
			}
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
 * ADC混音器处理回调 - 将4个单声道EQ输出合并成2个32位双声道
 * 输入: in_bufs[0]=guitar_L, [1]=guitar_R, [2]=mic_L, [3]=mic_R (各16位单声道)
 * 输出: out_buf = [guitar_L+mic_L | guitar_R+mic_R] (32位双声道)
 * 
 * 【副音箱模式支持】当只有2个输入时（ADC0, ADC1），直接混合立体声数据
 */
static void ADC_Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
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
static void Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
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

// ==================== Metronome 和 Looper 节点回调 ====================

/**
 * 节拍器源节点回调 - 生成节拍器音频数据
 * 注意：这里只生成数据，不做混音（混音由Mixer节点完成）
 */
static uint16_t Metronome_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t i;
	uint16_t generated = 0;
	
	(void)node;
	
	/* 限制最大长度 */
	if (max_len > 640) {
		max_len = 640;
	}
	
	/* 先清零缓冲区 */
	for (i = 0; i < max_len; i++) {
		out_buf[i] = 0;
	}
	
	/* 如果节拍器未启用，返回静音 */
	if (!MetronomeModule.IsEnabled()) {
		return max_len;
	}
	
	/* 生成节拍器音频（直接写入，不混音） */
	generated = MetronomeModule.GenerateAudio(out_buf, max_len);
	
	return (generated > 0) ? generated : max_len;
}

/**
 * 节拍器可用数据量查询回调
 */
static uint16_t Metronome_GetAvailCallback(EffectNode_t *node)
{
	(void)node;
	/* 节拍器始终可以生成数据 */
	return 48;
}

/**
 * 提示音源节点回调 - 解码提示音 PCM 数据
 * 非阻塞：每次调用解码一小帧，混入 USB_BT_MIXER
 */
static uint16_t Remind_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	(void)node;
	return RemindSound_GenerateAudio(out_buf, max_len);
}

/**
 * 提示音可用数据量查询回调
 */
static uint16_t Remind_GetAvailCallback(EffectNode_t *node)
{
	(void)node;
	return RemindSound_GetAvailableData();
}

/**
 * Looper播放源节点回调 - 从Flash读取录制的音频
 * 支持任意长度请求，通过循环读取多页来填充
 */
static uint16_t LooperPlay_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
	uint16_t i;
	uint16_t samples_filled = 0;
	const uint16_t samples_per_page = 48;  /* 每页固定 48 个样本，必须与录音保持一致 */
	
	(void)node;
	
	/* 先清零缓冲区（samples_per_page 大小，不能超） */
	for (i = 0; i < max_len && i < samples_per_page; i++) {
		out_buf[i] = 0;
	}
	
	/* 如果Looper正在播放，每次只读取固定的 samples_per_page(48) 个样本
	 * 播放速率必须与录音速率一致：每次回调 48 个样本，不能多。 */
	if (AudioLooper.IsPlaying()) {
		samples_filled = (max_len < samples_per_page) ? max_len : samples_per_page;
		
		/* 读取一页数据（固定 48 样本），填充到 out_buf */
		AudioLooper.ProcessPlayback32(out_buf, looper_flash_buffer, samples_filled);
	} else {
		/* 未播放时返回 0，pre_reverb_mixer 中 looper 贡献为静音（正确行为） */
		samples_filled = 0;
	}
	
	return samples_filled;
}

/**
 * Looper播放可用数据量查询回调
 */
static uint16_t LooperPlay_GetAvailCallback(EffectNode_t *node)
{
	(void)node;
	/* Looper始终可以提供数据（播放或静音） */
	return 48;
}

/**
 * Looper录制输出节点回调 - 将音频写入Flash
 */
static void LooperRecord_SinkCallback(EffectNode_t *node, uint32_t *in_buf, uint16_t len)
{
	(void)node;
	
	/* 安全检查：确保输入缓冲区有效 */
	if (!in_buf || len == 0) {
		return;
	}
	
	/* 限制最大长度，避免缓冲区溢出 (每页固定48采样) */
	if (len > 48) {
		len = 48;
	}
	
	/* 设置各录制源缓冲区指针 (Effect Graph 路径) */
	g_looper_src_mic    = BG_AudioManager.Audio_data.mic_buf_in;
	g_looper_src_linein = BG_AudioManager.Audio_data.guitar_buf_in;

	/* 如果Looper正在录制，写入数据 */
	if (AudioLooper.IsRecording()) {
		AudioLooper.ProcessRecording32(in_buf, looper_flash_buffer, len);
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
static void EQ_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len)
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
 * 挂接 Effect Graph 节点的音频设备回调
 * 注意：此函数在预设加载后需要被重新调用，以确保新节点的回调函数正确注册
 */
/**
 * @brief 设置USB输出（device→PC）音量和静音
 * @param vol 音量 0-100
 * @param mute 静音 0=off 1=on
 */
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
