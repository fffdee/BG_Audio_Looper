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
#include "otg_device_standard_request.h"  /* AUDIO_MIC_CDC */
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "bg_low_power.h"
#include "bt_stack_service.h"  /* BtStackServiceStart */
#include "rtos_api.h"
#include "FreeRTOS.h"
#include "sys_param.h"
#include "audio_setting.h"
#include "bg_shell.h"
#include "shell_io_manager.h"
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "chain_graph_apply.h"
#include "shell_cmd_graph.h"
#include "shell_cmd_sysmon.h"
#include "shell_cmd_mode.h"
#include "shell_cmd_flash.h"
#include "shell_cmd_effect.h"
#include "shell_cmd_psram.h"
#include "shell_cmd_param.h"
#include "shell_cmd_battery_calib.h"

#define DAC0_FIFO_LEN (sizeof(DAC0_FIFO))
#define DAC1_FIFO_LEN (sizeof(DAC1_FIFO))

/* ADC1 is used by the mic/line3 front-end. Keep the analog gain below
 * unity so the following mixer/effects have headroom before clipping. */
#define ADC1_MIC_PGA_GAIN       32/* mic_db_table: about -4.46 dB */
#define ADC1_MIC_BOOST_BYPASS   4

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
	AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_RIGHT, LINEIN5_RIGHT, 32, 3);
	AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_LEFT, LINEIN5_LEFT, 32, 3);
	AudioADC_VcomConfig(1);
	AudioADC_DigitalInit(ADC0_MODULE, SampleRate, (void *)AudioADC1Buf, sizeof(AudioADC1Buf));
}

// 初始化ADC1（麦克风）
static void ConfigADC1MicFrontend(void)
{
	AudioADC_VcomConfig(1);
	AudioADC_MicBias1Enable(TRUE);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_RIGHT, LINEIN_NONE);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_LEFT, LINEIN_NONE);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1);
	AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2,
	                     ADC1_MIC_PGA_GAIN, ADC1_MIC_BOOST_BYPASS);
	AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1,
	                     ADC1_MIC_PGA_GAIN, ADC1_MIC_BOOST_BYPASS);
}

static void InitADC1Mic(uint16_t SampleRate)
{
	AudioADC_DynamicElementMatch(ADC1_MODULE, FALSE,FALSE);
	ConfigADC1MicFrontend();
	AudioADC_DigitalInit(ADC1_MODULE, SampleRate, (void *)AudioADC2Buf, sizeof(AudioADC2Buf));

	/* DigitalInit touches ADC registers; restore the analog front-end. */
	ConfigADC1MicFrontend();
}



// 初始化音频效果（综合效果器：Delay + Chorus）
static void InitAudioEffects(uint16_t SampleRate)
{
	extern int osPortRemainMem(void);  /* 获取剩余内存 */
	int mem_before, mem_after;

	gCtrlVars.audio_effect_init_flag = 1;

	APP_DBG("[AudioInit] Memory available at start: %d bytes\n", osPortRemainMem());

	/* ============ Delay (系统自带 PCM Delay) ============ */
#if CFG_AUDIO_EFFECT_MUSIC_DELAY_EN
	mem_before = osPortRemainMem();
	gCtrlVars.music_delay_unit.enable          = 1;
	gCtrlVars.music_delay_unit.channel         = 2;    /* 立体声 */
	gCtrlVars.music_delay_unit.high_quality    = 0;    /* 压缩存储，省内存 */
	gCtrlVars.music_delay_unit.max_delay       = FX_DELAY_MAX_MS;
	gCtrlVars.music_delay_unit.max_delay_samples = (int32_t)((uint32_t)FX_DELAY_MAX_MS * SampleRate / 1000);
	gCtrlVars.music_delay_unit.delay           = DEFAULT_DELAY_MS;
	gCtrlVars.music_delay_unit.delay_samples   = (int32_t)((uint32_t)DEFAULT_DELAY_MS * SampleRate / 1000);
	AudioEffectPcmDelayInit(&gCtrlVars.music_delay_unit, 2, SampleRate);
	mem_after = osPortRemainMem();
	APP_DBG("[AudioInit] Delay: en=%d ct=%p allocated=%d (remain: %d)\n",
		gCtrlVars.music_delay_unit.enable, gCtrlVars.music_delay_unit.ct,
		mem_before - mem_after, mem_after);
#else
	(void)SampleRate;
#endif

	/* ============ Chorus (系统自带 Chorus, 单声道算法, L/R 各一路) ============ */
#if CFG_AUDIO_EFFECT_CHORUS_EN
	/* 左声道 */
	mem_before = osPortRemainMem();
	gCtrlVars.chorus_unit.enable = 1;
	gCtrlVars.chorus_unit.channel = 1;   /* mono */
	AudioEffectChorusInit(&gCtrlVars.chorus_unit, 1, SampleRate);
	mem_after = osPortRemainMem();
	APP_DBG("[AudioInit] Chorus(L): en=%d ct=%p allocated=%d (remain: %d)\n",
		gCtrlVars.chorus_unit.enable, gCtrlVars.chorus_unit.ct,
		mem_before - mem_after, mem_after);

	/* 右声道 */
	mem_before = osPortRemainMem();
	gCtrlVars.chorus_unit_r.enable = 1;
	gCtrlVars.chorus_unit_r.channel = 1; /* mono */
	AudioEffectChorusInit(&gCtrlVars.chorus_unit_r, 1, SampleRate);
	mem_after = osPortRemainMem();
	APP_DBG("[AudioInit] Chorus(R): en=%d ct=%p allocated=%d (remain: %d)\n",
		gCtrlVars.chorus_unit_r.enable, gCtrlVars.chorus_unit_r.ct,
		mem_before - mem_after, mem_after);
#else
	(void)SampleRate;
#endif

	APP_DBG("[AudioInit] Effects init done, final memory: %d bytes\n", osPortRemainMem());
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
	/* SoftMute 后再恢复麦克风模拟前端，避免 Bias/PGA 被冲掉。 */
	ConfigADC1MicFrontend();
	/* DAC音量将在Audio_loop()的SetVolume()中恢复，无需手动解除 */

	InitAudioEffects(SampleRate);

	InitControlGPIO();
	InitDetectionGPIO();

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

		// 4. 挂接实际音频设备回调
		BG_AudioIO_SetupEffectGraphCallbacks();
	}
#endif /* EFFECT_GRAPHICS_EN */

	// 6. 注册 Shell 命令（支持 CDC/BLE 远程控制）
	ShellCmdGraph_Register();

	// 7. 注册系统监控命令（CPU/内存/任务统计）
	ShellCmdSysmon_Register();
	
	// 8. 注册模式切换命令（主音箱/副音箱模式）
	ShellCmdMode_Register();

        // 9. 注册 NAND Flash 测试命令
        ShellCmdFlash_Register();

	// 10. 注册效果器参数命令（effect）
	ShellCmdEffect_Register();

	// 11. 注册 PSRAM 测试命令
	ShellCmdPsram_Register();

	// 12. 注册系统参数命令（param）
	ShellCmd_Param_Init();

	// 13. 注册电池校准命令（battery calib）
	ShellCmd_BattCalib_Init();
	// ==========================================

	// 初始化低功耗管理器（所有音频模块初始化完毕后调用）
	LowPower_Init();

	/* SysParam_ApplyToAudio() 可能在 AudioADC_DigitalInit 之前调用，
	 * DigitalInit 会重置数字音量；此处在 ADC 就绪后再次应用，避免麦克风/吉他无声。
	 * 若 Flash 里 mic 音量为 0（旧版本误存），回退到默认 80，避免永久静音。 */
	{
		uint8_t mic1 = g_sys_param.volume.mic1_volume;
		uint8_t mic2 = g_sys_param.volume.mic2_volume;
		if (mic1 == 0) {
			mic1 = 80;
		}
		if (mic2 == 0) {
			mic2 = 80;
		}
		AudioSetting_SetMic1VolumePercent(mic1);
		AudioSetting_SetMic2VolumePercent(mic2);
		AudioSetting_SetGuitar1VolumePercent(g_sys_param.volume.guitar1_volume);
		AudioSetting_SetGuitar2VolumePercent(g_sys_param.volume.guitar2_volume);
	}
}
