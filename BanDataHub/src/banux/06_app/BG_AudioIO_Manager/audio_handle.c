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
#include "dac.h"
#include "clk.h"
#include "gpio.h"
#include "debug.h"
#include "type.h"
#include "remind_sound.h"
#include "audio_effect.h"
#include "ctrlvars.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_stor.h"
#include "otg_detect.h"
#include "otg_device_cdc.h"
#include "otg_device_audio.h"
#include "hal_sdio.h"
#include "irqn.h"
#include "flash_bus.h"
#include "bg_shell.h"
#include "shell_io_cdc.h"
#include "shell_io_manager.h"
#include "bg_low_power.h"
#include "rtos_api.h"
#include "FreeRTOS.h"
#include "shell_cmd_sysmon.h"

#if HW_DRV_SSD1306_EN
#include "ssd1306.h"
#endif

#if HW_DRV_ENCODER_EN
#include "rotary_encoder.h"
#endif

static uint32_t AudioADC1Buf[1024] = {0};
static uint32_t AudioADC2Buf[1024] = {0};

#define DAC_FIFO_SAMPLES 1024
static uint32_t DAC0_FIFO[DAC_FIFO_SAMPLES];
#define DAC0_FIFO_LEN sizeof(DAC0_FIFO)
static uint32_t DAC1_FIFO[DAC_FIFO_SAMPLES];
#define DAC1_FIFO_LEN sizeof(DAC1_FIFO)

extern uint32_t usb_speaker_enable;
extern uint32_t usb_mic_enable;

void BG_audio_Init(uint16_t SampleRate);
void Audio_loop(void);

BG_Audio_Io_Manager BG_AudioManager __attribute__((section(".data"))) = {
	.Audio_Init = BG_audio_Init,
	.Audio_Loop = Audio_loop,
	.Audio_data = {
		.guitar_count = 0,
		.mic_count = 0,
		.det_state = NONE,
	},
};

static uint8_t DmaChannelMap[29] = {
	255, 255, 255,
	4, 5,
	255, 255, 255, 255, 255,
	0, 1,
	255, 7, 6,
	255, 255, 255,
	2, 3, 8, 9,
	255, 255, 255, 255,
	255, 255, 255,
};

static void InitUSBDevice(void)
{
	OTG_DeviceModeSel(CDC_READER, 0x1234, 0x1234);
	OTG_DeviceInit();
	NVIC_EnableIRQ(Usb_IRQn);
	NVIC_SetPriority(Usb_IRQn, 0);

	/* SD 卡已在 BG_FlashMgr.Init() → FlashDevices_Init() 中初始化，
	   不再重复初始化，避免 SDIO 端口重配置导致问题 */

	OTG_DeviceCDC_Init();
	DBG("USB CDC+MassStorage mode initialized\n");
}

static void InitDAC(uint16_t SampleRate)
{
	AudioDAC_Init(ALL, SampleRate, (void *)DAC0_FIFO, DAC0_FIFO_LEN, (void *)DAC1_FIFO, DAC1_FIFO_LEN);
	AudioDAC_DoutModeSet(DAC0, MODE2, WIDTH_16_BIT);
	AudioDAC_DoutModeSet(DAC1, MODE2, WIDTH_16_BIT);
	AudioDAC_VolSet(DAC0, 0x3FFF, 0x3FFF);
	AudioDAC_VolSet(DAC1, 0x3FFF, 0);
}

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

static void InitAudioEffects(uint16_t SampleRate)
{
	extern int osPortRemainMem(void);

	gCtrlVars.audio_effect_init_flag = 1;

	gCtrlVars.reverb_unit.enable = 1;
	gCtrlVars.plate_reverb_unit.enable = 0;
	AudioEffectReverbInit(&gCtrlVars.reverb_unit, 2, SampleRate);

	gCtrlVars.mic_drc_unit.enable = 1;
	AudioEffectDRCInit(&gCtrlVars.mic_drc_unit, 2, SampleRate);

	gCtrlVars.eq_guitar_l_unit.enable = 1;
	gCtrlVars.eq_guitar_l_unit.channel = 1;
	AudioEffectEQInit(&gCtrlVars.eq_guitar_l_unit, 1, SampleRate);

	gCtrlVars.eq_guitar_r_unit.enable = 1;
	gCtrlVars.eq_guitar_r_unit.channel = 1;
	AudioEffectEQInit(&gCtrlVars.eq_guitar_r_unit, 1, SampleRate);

	gCtrlVars.eq_mic_l_unit.enable = 1;
	gCtrlVars.eq_mic_l_unit.channel = 1;
	AudioEffectEQInit(&gCtrlVars.eq_mic_l_unit, 1, SampleRate);

	gCtrlVars.eq_mic_r_unit.enable = 1;
	gCtrlVars.eq_mic_r_unit.channel = 1;
	AudioEffectEQInit(&gCtrlVars.eq_mic_r_unit, 1, SampleRate);

	gCtrlVars.music_out_eq_unit.enable = 1;
	gCtrlVars.music_out_eq_unit.channel = 2;
	AudioEffectEQInit(&gCtrlVars.music_out_eq_unit, 2, SampleRate);

	gCtrlVars.mic_expander_unit.enable = 1;
	AudioEffectExpanderInit(&gCtrlVars.mic_expander_unit, 2, SampleRate);

	DBG("[Audio] Effects initialized, heap: %d bytes\n", osPortRemainMem());
}

static void InitControlGPIO(void)
{
	/* 电源保持: GPIO_A1 输出高电平保持开机 */
	GPIO_RegOneBitClear(GPIO_A_IE, (1 << HW_PWR_BTN_HOLD_PIN));
	GPIO_RegOneBitSet(GPIO_A_OE, (1 << HW_PWR_BTN_HOLD_PIN));
	GPIO_RegOneBitSet(GPIO_A_OUT, (1 << HW_PWR_BTN_HOLD_PIN));

	/* 电源按钮检测: GPIO_A0 输入上拉 */
	GPIO_RegOneBitSet(GPIO_A_IE, (1 << HW_PWR_BTN_DET_PIN));
	GPIO_RegOneBitClear(GPIO_A_OE, (1 << HW_PWR_BTN_DET_PIN));
	GPIO_RegOneBitSet(GPIO_A_PU, (1 << HW_PWR_BTN_DET_PIN));

	/* MIC 插入检测: GPIO_A23 输入上拉 */
	GPIO_RegOneBitSet(GPIO_A_IE, (1 << HW_MIC_DET_PIN));
	GPIO_RegOneBitClear(GPIO_A_OE, (1 << HW_MIC_DET_PIN));
	GPIO_RegOneBitSet(GPIO_A_PU, (1 << HW_MIC_DET_PIN));

	/* MIC 模拟开关切换: GPIO_A24 输出，默认低电平(MIC未插入) */
	GPIO_RegOneBitClear(GPIO_A_IE, (1 << HW_MIC_SWITCH_PIN));
	GPIO_RegOneBitSet(GPIO_A_OE, (1 << HW_MIC_SWITCH_PIN));
	GPIO_RegOneBitClear(GPIO_A_OUT, (1 << HW_MIC_SWITCH_PIN));

	/* 立体声输入模式切换: GPIO_B6 输出，默认低电平 */
	GPIO_RegOneBitClear(GPIO_B_IE, (1 << HW_STEREO_SWITCH_PIN));
	GPIO_RegOneBitSet(GPIO_B_OE, (1 << HW_STEREO_SWITCH_PIN));
	GPIO_RegOneBitClear(GPIO_B_OUT, (1 << HW_STEREO_SWITCH_PIN));

	DBG("[GPIO] Control GPIO initialized (PWR_HOLD=A%d, MIC_DET=A%d, MIC_SW=A%d, STEREO_SW=B%d)\n",
	    HW_PWR_BTN_HOLD_PIN, HW_MIC_DET_PIN, HW_MIC_SWITCH_PIN, HW_STEREO_SWITCH_PIN);
}

static void SetVolume(void)
{
	uint16_t DC_Data;
#if HW_VOLUME_ADC_EN
	GPIO_RegOneBitClear(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
	GPIO_RegOneBitSet(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
	DC_Data = ADC_SingleModeDataGet(HW_VOLUME_ADC_CHANNEL) * 4;
#else
	DC_Data = 0x3FFF;
#endif
	AudioDAC_VolSet(DAC0, DC_Data, DC_Data);
	AudioDAC_VolSet(DAC1, DC_Data, 0);
}

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

	for (i = 0; i < len; i++)
	{
		BG_AudioManager.Audio_data.OutPut_buf[i] =
			BG_AudioManager.Audio_data.guitar_buf_in[i] +
			BG_AudioManager.Audio_data.mic_buf_in[i];
	}

	*pRealLen = len;
}

static void ApplyAudioEffects(uint16_t len)
{
	static uint32_t temp_buf1[512];
	static uint32_t temp_buf2[512];

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

void BG_audio_Init(uint16_t SampleRate)
{
	BG_AudioManager.Audio_data.SampleRate = SampleRate;

	InitUSBDevice();
	InitDAC(SampleRate);
	InitADC0LineIn(SampleRate);
	InitADC1Mic(SampleRate);

	AudioADC_SoftMute(ADC0_MODULE, TRUE, TRUE);
	AudioADC_SoftMute(ADC1_MODULE, TRUE, TRUE);
	vTaskDelay((300 + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS);
	AudioADC_SoftMute(ADC0_MODULE, FALSE, FALSE);
	AudioADC_SoftMute(ADC1_MODULE, FALSE, FALSE);

	InitAudioEffects(SampleRate);
	InitControlGPIO();

	ShellIOManager_Init();
	ShellCmdSysmon_Register();

	LowPower_Init();

	DBG("[Audio] BanDataHub audio system initialized at %d Hz\n", SampleRate);
}

void BG_AudioIO_PrepareForShutdown(void)
{
	if (gCtrlVars.reverb_unit.ct != NULL) {
		gCtrlVars.reverb_unit.enable = 0;
		osPortFree(gCtrlVars.reverb_unit.ct);
		gCtrlVars.reverb_unit.ct = NULL;
		DBG("[Audio] Reverb freed for shutdown sound\n");
	}
}

static void USB_HotplugCheck(void)
{
	(void)OTG_PortDeviceIsLink();
}

void Audio_loop(void)
{
	uint16_t RealLen = 0;
	const uint16_t MIN_SAMPLE = 48;
	static uint16_t s_gpio_div = 0;

#ifdef BANDATAHUB
	/* SD卡热拔插检测: 使用 DET 引脚 (GPIO_B5)
	 * 低电平 = 卡插入, 高电平 = 卡拔出
	 * 独立计时，每 ~500ms 检测一次，带消抖
	 */
	static uint8_t sd_last_state = 0;  /* 0=无卡, 1=有卡 */
	static bool sd_hotplug_inited = false;
	static uint8_t sd_debounce = 0;    /* 消抖计数器 */
	static uint8_t sd_debounce_val = 0; /* 消抖采样值 */
	static uint16_t sd_poll_div = 0;   /* 独立轮询分频器 */

	if (!sd_hotplug_inited) {
		/* 初始化 DET 引脚: GPIO_B5 输入上拉 */
		GPIO_RegOneBitSet(GPIO_B_IE, (1 << HW_SDCARD_DET_PIN));
		GPIO_RegOneBitClear(GPIO_B_OE, (1 << HW_SDCARD_DET_PIN));
		GPIO_RegOneBitSet(GPIO_B_PU, (1 << HW_SDCARD_DET_PIN));
		/* 读取初始状态 */
		sd_last_state = GPIO_RegOneBitGet(GPIO_B_IN, (1 << HW_SDCARD_DET_PIN)) ? 0 : 1;
		sd_hotplug_inited = true;
		DBG("[SD_Hotplug] Initial state: %s\n", sd_last_state ? "INSERTED" : "REMOVED");
	}

	/* 独立轮询: 每500次 Audio_loop 调用检测一次 (~500ms) */
	if (++sd_poll_div >= 500)
	{
		sd_poll_div = 0;

		uint8_t sd_current = GPIO_RegOneBitGet(GPIO_B_IN, (1 << HW_SDCARD_DET_PIN)) ? 0 : 1;

		/* 消抖: 连续3次读到的值相同才认为状态变化 */
		if (sd_current != sd_last_state) {
			if (sd_debounce_val != sd_current) {
				sd_debounce_val = sd_current;
				sd_debounce = 1;
			} else {
				sd_debounce++;
			}

			if (sd_debounce >= 3) {
				sd_debounce = 0;

				if (sd_current && !sd_last_state)
				{
					/* 卡插入: 重新初始化SD卡
					 * 卡刚插入时可能需要时间稳定，带重试
					 */
					DBG("[SD_Hotplug] Card INSERTED, re-initializing...\n");
					vTaskDelay(300);  /* 等待卡稳定 */
					{
						extern FlashDevice_t* FlashDevices_GetSDCardFlash(void);
						FlashDevice_t *sd_dev = FlashDevices_GetSDCardFlash();
						if (sd_dev) {
							int8_t retry;
							for (retry = 0; retry < 3; retry++) {
								if (sd_dev->ops && sd_dev->ops->deinit)
									sd_dev->ops->deinit(sd_dev);
								sd_dev->initialized = false;
								if (FlashDev_Init(sd_dev) == FLASH_OK) {
									DBG("[SD_Hotplug] SD card re-initialized OK (attempt %d)\n", retry + 1);
									break;
								}
								DBG("[SD_Hotplug] Init attempt %d failed, retrying...\n", retry + 1);
								vTaskDelay(200);
							}
							if (retry >= 3) {
								DBG("[SD_Hotplug] SD card init failed after 3 attempts\n");
							}
						}
					}
				}
				else if (!sd_current && sd_last_state)
				{
					/* 卡拔出: 去初始化SD卡 */
					DBG("[SD_Hotplug] Card REMOVED\n");
					{
						extern FlashDevice_t* FlashDevices_GetSDCardFlash(void);
						FlashDevice_t *sd_dev = FlashDevices_GetSDCardFlash();
						if (sd_dev && sd_dev->initialized) {
							if (sd_dev->ops && sd_dev->ops->deinit)
								sd_dev->ops->deinit(sd_dev);
							sd_dev->initialized = false;
							DBG("[SD_Hotplug] SD card deinitialized\n");
						}
					}
				}
				sd_last_state = sd_current;
			}
		} else {
			sd_debounce = 0;
		}
	}
#endif /* BANDATAHUB */

	SetVolume();
	OTG_DeviceRequestProcess();
	OTG_DeviceCDC_Task();
	OTG_DeviceStorProcess();
	USB_HotplugCheck();

	/* MIC 插入检测: 检测到低电平表示MIC插入，切换模拟开关防止MIC电源对信号干扰 */
	if (s_gpio_div == 0)
	{
		if (GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_MIC_DET_PIN)))
		{
			/* MIC 未插入: 模拟开关断开 */
			GPIO_RegOneBitClear(GPIO_A_OUT, (1 << HW_MIC_SWITCH_PIN));
		}
		else
		{
			/* MIC 已插入: 模拟开关接通 */
			GPIO_RegOneBitSet(GPIO_A_OUT, (1 << HW_MIC_SWITCH_PIN));
		}
	}

	while (AudioADC_DataLenGet(ADC0_MODULE) >= MIN_SAMPLE)
	{
		ReadAudioData(MIN_SAMPLE, &RealLen);
		ApplyAudioEffects(RealLen);

		if (++s_gpio_div >= 50)
		{
			s_gpio_div = 0;
		}

		/* 直接输出效果处理后的音频到 DAC */
		AudioDAC_DataSet(DAC0, BG_AudioManager.Audio_data.guitar_buf_out, RealLen);
	}

	ShellIOManager_Process();
}

/*===========================================================================
 * 整机硬件测试 — 在 SSD1306 OLED 上实时显示所有硬件状态
 * 编码旋钮值、音量旋钮ADC、电源按钮、MIC检测、电池电压、SD卡状态
 * 长按编码器按键退出测试模式
 *===========================================================================*/

#if HW_DRV_SSD1306_EN

static int16_t test_encoder_count = 0;
static uint8_t test_exit_flag = 0;

/* 绘制进度条 (x, y 为左上角, w 为最大宽度, h 为高度, pct 为 0~100) */
static void Test_DrawBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pct)
{
	if (pct > 100) pct = 100;
	SSD1306_DrawRect(x, y, w, h, 1);
	if (pct > 0 && h > 2) {
		uint8_t fill_w = (uint8_t)((uint16_t)(w - 2) * pct / 100);
		if (fill_w > 0) {
			SSD1306_FillRect(x + 1, y + 1, fill_w, h - 2, 1);
		}
	}
}

void HW_Test_Run(void)
{
	char buf[24];
	uint16_t vol_adc, bat_adc;
	uint8_t pwr_btn, mic_det, enc_btn;
	uint8_t enc_a, enc_b;
	uint8_t update_div = 0;

#if HW_DRV_ENCODER_EN
	/* Encoder already initialized in main.c power_on(), skip re-init */
#endif

	test_encoder_count = 0;
	test_exit_flag = 0;

	DBG("[HW_Test] Hardware test started, long-press encoder to exit\n");

	while (!test_exit_flag)
	{
#if HW_DRV_ENCODER_EN
		RotaryEncoder_Scan();
		{
			int16_t delta = RotaryEncoder_GetDelta();
			test_encoder_count += delta;
			if (delta != 0) {
				DBG("[HW_Test] ENC delta=%d total=%d A=%d B=%d\n",
				    delta, test_encoder_count,
				    GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_ENCODER_A_PIN)) ? 1 : 0,
				    GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_ENCODER_B_PIN)) ? 1 : 0);
			}
		}
		if (RotaryEncoder_IsButtonLongPressed()) {
			test_exit_flag = 1;
			DBG("[HW_Test] Exit by long-press\n");
		}
#endif

		/* 每 5 次循环刷新一次屏幕 (~50ms)，避免闪烁 */
		if (++update_div < 5) {
			vTaskDelay(1);
			continue;
		}
		update_div = 0;

		/* 读取所有硬件状态 */
#if HW_VOLUME_ADC_EN
		GPIO_RegOneBitClear(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
		GPIO_RegOneBitSet(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
		vol_adc = ADC_SingleModeDataGet(HW_VOLUME_ADC_CHANNEL);
#else
		vol_adc = 0;
#endif

#if HW_DRV_BATTERY_EN
		GPIO_RegOneBitClear(HW_BATTERY_ADC_GPIO_PORT, HW_BATTERY_ADC_GPIO_PIN);
		GPIO_RegOneBitSet(HW_BATTERY_ADC_GPIO_PORT, HW_BATTERY_ADC_GPIO_PIN);
		bat_adc = ADC_SingleModeDataGet(HW_BATTERY_ADC_CHANNEL);
#else
		bat_adc = 0;
#endif

		pwr_btn = GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_PWR_BTN_DET_PIN)) ? 0 : 1;
		mic_det = GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_MIC_DET_PIN)) ? 0 : 1;
		enc_btn = GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_ENCODER_BTN_PIN)) ? 0 : 1;
		enc_a   = GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_ENCODER_A_PIN)) ? 1 : 0;
		enc_b   = GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_ENCODER_B_PIN)) ? 1 : 0;

		/* 绘制屏幕 */
		SSD1306_Clear();

		/* 第1行: 标题 */
		SSD1306_DrawString(0, 0, "HW Test", 2, 1);

		/* 第2行: 编码器旋钮值 + 按键状态 */
		snprintf(buf, sizeof(buf), "ENC:%-5d %s", test_encoder_count, enc_btn ? "BTN" : "   ");
		SSD1306_DrawString(0, 18, buf, 1, 1);
		/* 编码器A/B相电平 */
		snprintf(buf, sizeof(buf), "A:%d B:%d", enc_a, enc_b);
		SSD1306_DrawString(80, 18, buf, 1, 1);

		/* 第3行: 音量旋钮 ADC 值 + 进度条 */
		snprintf(buf, sizeof(buf), "VOL:%-5d", vol_adc);
		SSD1306_DrawString(0, 28, buf, 1, 1);
		Test_DrawBar(60, 28, 60, 8, (uint8_t)((uint32_t)vol_adc * 100 / 4095));

		/* 第4行: 电池 ADC 值 + 进度条 */
		snprintf(buf, sizeof(buf), "BAT:%-5d", bat_adc);
		SSD1306_DrawString(0, 38, buf, 1, 1);
		Test_DrawBar(60, 38, 60, 8, bat_adc > 3100 ? 100 : (uint8_t)((uint32_t)(bat_adc - 2400) * 100 / 700));

		/* 第5行: 电源按钮 + MIC检测 + SD卡 */
		{
			/* 使用 DET 引脚检测SD卡，不调用 HAL_SD_Detect() 避免SDK打印 "SD link!" */
			uint8_t sd_present = GPIO_RegOneBitGet(GPIO_B_IN, (1 << HW_SDCARD_DET_PIN)) ? 0 : 1;
			snprintf(buf, sizeof(buf), "PWR:%s MIC:%s SD:%s",
			         pwr_btn ? "ON " : "OFF",
			         mic_det ? "IN " : "OUT",
			         sd_present ? "OK " : "NO ");
			SSD1306_DrawString(0, 48, buf, 1, 1);
		}

		/* 第6行: 退出提示 */
		SSD1306_DrawString(0, 57, "Long-press ENC exit", 1, 1);

		SSD1306_Update();

		vTaskDelay(1);
	}

	/* 退出测试，恢复正常显示 */
	SSD1306_Clear();
	SSD1306_DrawString(0, 0, "BanDataHub", 2, 1);
	SSD1306_DrawString(0, 20, "Test Done", 1, 1);
	SSD1306_Update();
	DBG("[HW_Test] Hardware test finished\n");
}

#endif /* HW_DRV_SSD1306_EN */
