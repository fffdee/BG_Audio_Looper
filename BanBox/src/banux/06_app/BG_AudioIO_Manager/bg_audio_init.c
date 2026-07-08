/**
 * @file bg_audio_init.c
 * @brief Audio hardware and subsystem initialization.
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_internal.h"
#include "product_def.h"
#include "debug.h"

#include "gpio.h"
#include "app_config.h"
#include "audio_adc.h"
#include "adc_interface.h"
#include "dac_interface.h"
#include "adc.h"
#include "dac.h"
#include "audio_effect.h"
#include "ctrlvars.h"
#include "otg_device_hcd.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "remind_sound.h"
#include "metronome.h"
#include "bg_low_power.h"
#include "bt_manager.h"
#include "rtos_api.h"
#include "FreeRTOS.h"
#include "sys_param.h"
#include "audio_looper.h"
#include "bg_shell.h"
#include "shell_io_manager.h"
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "effect_graph_vfs.h"
#include "chain_graph_apply.h"
#include "shell_cmd_graph.h"
#include "shell_cmd_sysmon.h"
#include "shell_cmd_metronome.h"
#include "shell_cmd_mode.h"
#include "shell_cmd_flash.h"

#define DAC0_FIFO_LEN (sizeof(DAC0_FIFO))
#define DAC1_FIFO_LEN (sizeof(DAC1_FIFO))

static void InitUSBDevice(void)
{
	// 使用AUDIO_MIC_CDC模式：音频+麦克风+CDC串口复合设备
	OTG_DeviceModeSel(AUDIO_MIC_CDC, 0x1234, 0x1234);
	UsbDevicePlayInit();
	UsbDeviceEnable();
	s_usb_connected = OTG_PortDeviceIsLink();
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
