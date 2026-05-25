/**
 **************************************************************************************
 * @file    freertos_example.c
 * @brief   freertos example
 *
 * @author  Peter
 * @version V1.0.0
 *
 * $Created: 2019-05-30 11:30:00$
 *
 * @Copyright (C) 2019, Shanghai Mountain View Silicon Co.,Ltd. All rights reserved.
 **************************************************************************************
 */

#include <stdlib.h>
#include <stdbool.h>
#include <nds32_intrinsic.h>
#include "gpio.h"
#include "uarts.h"
#include "uarts_interface.h"
#include "type.h"
#include "debug.h"
#include "timeout.h"
#include "clk.h"
#include "dma.h"
#include "timer.h"
#include "adc.h"
#include "dac.h"
#include "watchdog.h"
#include "spi_flash.h"
#include "remap.h"
#include "irqn.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "delay.h"
#include "chip_info.h"
#include "audio_adc.h"
#include "adc_interface.h"
#include "dac_interface.h"
#include "spim_interface.h"
#include "spim.h"
#include "bg_flash_manager.h"
#include "BG_FlashMgr.h"
#include "flash_test.h"
#include "internal_flash_test.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "drv_device.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_stor.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "ctrlvars.h"
#include "product_def.h"
#include "flash_boot.h"

#if HW_DRV_LCD_EN
#include "bg_lcd.h"
#include "framebuffer.h"
#endif

#if HW_DRV_SSD1306_EN
#include "ssd1306.h"
#endif

#if HW_DRV_ENCODER_EN
#include "rotary_encoder.h"
#endif

#if CDC_FILE_MANAGER_EN
#include "cdc_file_manager.h"
#endif

#include "bg_audio_io_manager.h"

#if HW_DRV_BATTERY_EN
#include "battery_drv.h"
#include "battery_calib.h"
#endif

#if HW_DRV_BT_EN
extern uint8_t BleConnectFlag;
#include "ble_protocol.h"
#endif

#include "sys_param.h"
#include "bg_shell.h"
#include "audio_setting.h"



#include "drv_init.h"
#include "bg_event.h"
#include "sys_state.h"
#include "remind_sound.h"



extern void SysTickInit(void);
extern void UsbAudioMicDacInit(void);
extern void OTG_DeviceAudioInit();
extern void UsbAudioTimer1msProcess(void);

uint8_t record_flag = 0;
uint16_t read_write = 0;
uint8_t play_flag = 0;
uint16_t rec = 0, rea = 0, play = 0;
uint16_t time = 0;
uint16_t left_time = 0;
uint16_t right_time = 0;
uint8_t left_flag = false;
uint8_t right_flag = false;

uint8_t UI_count = 0, UI_flag = 0;

static uint32_t ble_tick_counter = 0;

uint8_t power_flag = 0;
uint8_t count_flag = 0;

uint16_t power_count = 0;

#if HW_DRV_ENCODER_EN
static uint8_t encoder_scan_counter = 0;
#endif

void Timer2Interrupt(void) {
	Timer_InterruptFlagClear(TIMER2, UPDATE_INTERRUPT_SRC);
	OTG_PortLinkCheck();

	if (time > 50)
		time = 0;
	if(time==0)
		UI_flag = 1;

	time++;

	if (record_flag == 0) {
		record_flag = 1;
	}

#ifdef CFG_APP_USB_AUDIO_MODE_EN
	UsbAudioTimer1msProcess();
#endif

	ble_tick_counter++;

#if BANGTSYNTH_EN
	bg_tick_increment();
#endif

#if HW_DRV_ENCODER_EN
	encoder_scan_counter++;
	if (encoder_scan_counter >= 10) {
		encoder_scan_counter = 0;
		RotaryEncoder_Scan();
	}
#endif

	if(count_flag){
		power_count++;
	}
	else{
		power_count = 0;
	}
}

xQueueHandle xQueue;

uint32_t SendCount = 0;
uint32_t RecvCount = 0;
uint32_t result[100];

int16_t CRC[100] = { 0 };

uint32_t sectorAddress = 0;

uint32_t record_time;
#define  MAX_BUF_LEN   4096

uint8_t spimRate = SPIM_CLK_DIV_12M;
uint8_t spimMode = 0;
uint8_t SpimBuf_TX[MAX_BUF_LEN];
uint8_t SpimBuf_RX[MAX_BUF_LEN];

const char* spimIO[][4] = {
		{ "A22", "A7", "A6", "A5" }, { "A8", "A22", "A21", "A20" }, };

static uint8_t DmaChannelMap[29] = {
		255, 255, 255,
		4, 5,     /* SDIO RX/TX */
		255, 255, 255, 255, 255,
		0, 1,
		255, 7, 6,
		255, 255, 255,
		2, 3, 8, 9,
		255, 255, 255, 255,
		255, 255, 255,
		};

void spi_init(void) {
	SPIM_SetDmaEn(1);
	SPIM_IoConfig(SPIM_PORT0_A5_A6_A7);
	Clock_SPIMClkDivSet(1);
	DMA_ChannelAllocTableSet(DmaChannelMap);
	if (SPIM_Init(spimMode, spimRate)) {
		DBG("SPI init success!\n");
	} else {
		DBG("****** Err: SPI init fail ******\n");
	}
}

void spi_write(uint8_t *data,uint16_t size)
{
	SPIM_DMA_Send_Start(data, size);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));
}

void spi_read(uint8_t *data,uint16_t size)
{
	SPIM_DMA_Recv_Start(data, size);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));
}

#if HW_DRV_BT_EN
static void ble_data_cmd_dispatch(const BleProtoFrame_t *frame)
{
	switch (frame->cmd) {
#if HW_DRV_BATTERY_EN
	case BLE_CMD_BATTERY_CALIB:
		BattCalib_HandleBleCmd(frame->payload, frame->len);
		break;
#endif
	default:
		DBG("[BLE] Unhandled data cmd: 0x%02X\n", frame->cmd);
		break;
	}
}
#endif

void power_on()
{
	CtrlVarsInit();

	DBG("[Task] Hardware drivers already initialized in main()\n");

#ifdef UI_EN
#if HW_DRV_LCD_EN
	BG_lcd.Init();
#endif
#endif

	DBG("[Task] Loading system parameters from flash...\n");
	{
		SysParam_Status_t param_status = SysParam_Init();
		if (param_status == SYSPARAM_OK) {
			DBG("[Task] Parameters loaded successfully from flash\n");
			SysParam_ApplyToAudio();
		} else {
			DBG("[Task] Using default parameters (status=%d)\n", param_status);
			SysParam_ApplyToAudio();
		}
	}

#if BANGTSYNTH_EN
	DBG("[Task] Running synthesizer startup sequence...\n");
	SYNTH_StartupSequence();
	DBG("[Task] Initializing BanGTsynth...\n");
	{
		extern int osPortRemainMem(void);
		DBG("[Task] Heap before BanGTsynth: %d bytes\n", osPortRemainMem());
	}
	if (BanGTsynth_Node_Init() == 0) {
		DBG("[Task] BanGTsynth initialized OK\n");
		BG_Storage.SetDriver(&bg_storage_driver_embedded);
		if (soundbank_manager.Init(0) == SUCCESS) {
			DBG("[Task] Embedded SF2 soundbank loaded OK\n");
		} else {
			DBG("[Task] Embedded SF2 soundbank load FAILED\n");
		}
	} else {
		DBG("[Task] BanGTsynth init FAILED\n");
	}
#endif

	/* 初始化 Flash 设备（含 SD 卡），必须在 RTOS 调度器启动后调用，
	   因为 SD 卡初始化内部使用 vTaskDelay() */
	DBG("[Task] Initializing Flash devices (PSRAM, SD card)...\n");
	BG_FlashMgr.Init();
	DBG("[Task] Flash devices initialized\n");

	BG_AudioManager.Audio_Init(44100);

#if HW_DRV_BT_EN
	{
		extern void BleProto_Init(void);
		BleProto_Init();
		extern void BleProto_RegisterDataHandler(BleProto_DataHandler_t handler);
		BleProto_RegisterDataHandler(ble_data_cmd_dispatch);
		DBG("[Task] BLE data handler registered\n");
	}
#endif

#if HW_DRV_BATTERY_EN
	BattCalib_Init();
	DBG("[Task] Battery calibration initialized\n");
#endif

	SysState_Init();
	DBG("[Task] SysState initialized\n");

#if HW_DRV_SSD1306_EN
	SSD1306_Init();
	SSD1306_Clear();
	SSD1306_DrawString(0, 0, "BanDataHub", 2, 1);
	SSD1306_DrawString(0, 20, "Init OK", 1, 1);
	SSD1306_Update();
	DBG("[Task] SSD1306 OLED initialized\n");
#endif

#if HW_DRV_ENCODER_EN
	RotaryEncoder_Init();
	DBG("[Task] Rotary Encoder initialized\n");
#endif

#ifdef UI_EN
	BANGUI_QUICK_INIT();
	View_Home_SetIconCallback(HOME_ICON_LOOPER, NULL);
	BANGUI_START(UI_STATE_BOOT);
	DBG("[Main] System initialized successfully\n");
	DBG("[Main] Starting from Boot Screen...\n");
	ShellLCD_Adapter_Init();
#endif

	DBG("[Main] Entering main loop...\n");
}

void power_off()
{
	BG_AudioIO_PrepareForShutdown();
	RemindSound_PlayByName("off");
#ifdef BANDATAHUB
	/* BanDataHub: A1=PWR_HOLD, A20=EncoderA, A24=MIC_SW - don't touch them */
	GPIO_RegOneBitClear(GPIO_A_OUT, (1 << HW_PWR_BTN_HOLD_PIN));
#else
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX24);
#endif
}

void pwr_button_init()
{
#ifdef BANDATAHUB
	/* BanDataHub: A0=PWR_DET, A1=PWR_HOLD, A20=EncoderA, A23=MIC_DET, A24=MIC_SW */
	GPIO_RegOneBitSet(GPIO_A_IE, (1 << HW_PWR_BTN_DET_PIN));
	GPIO_RegOneBitClear(GPIO_A_OE, (1 << HW_PWR_BTN_DET_PIN));
	GPIO_RegOneBitSet(GPIO_A_PU, (1 << HW_PWR_BTN_DET_PIN));

	GPIO_RegOneBitSet(GPIO_A_OE, (1 << HW_PWR_BTN_HOLD_PIN));
	GPIO_RegOneBitClear(GPIO_A_IE, (1 << HW_PWR_BTN_HOLD_PIN));
	GPIO_RegOneBitSet(GPIO_A_OUT, (1 << HW_PWR_BTN_HOLD_PIN));
#else
	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX23);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX23);
	GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX23);
	GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX23);

	GPIO_RegOneBitSet(GPIO_A_OE, GPIO_INDEX20);
	GPIO_RegOneBitClear(GPIO_A_IE, GPIO_INDEX20);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX20);

	GPIO_RegOneBitSet(GPIO_A_OE, GPIO_INDEX24);
	GPIO_RegOneBitClear(GPIO_A_IE, GPIO_INDEX24);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX24);
#endif
}

uint8_t valid_press = 0;
void pwr_butoon_handler()
{
#ifdef BANDATAHUB
	if(GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_PWR_BTN_DET_PIN)) == 0 && valid_press ==1)
	{
		count_flag = 1;

		if(power_count > 1000 && power_flag == 0 && GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_PWR_BTN_DET_PIN)) == 0){
			count_flag  = 0;
			valid_press  = 0;
			power_on();
			power_flag = 1;
			power_count=0;
			DBG("Power ON triggered\n");

		}else if (power_count > 1000 && power_flag == 1 && GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_PWR_BTN_DET_PIN)) == 0)
		{
			valid_press  = 0;
			count_flag  = 0;
			power_off();
			power_flag = 0;
			power_count = 0;
			DBG("Power OFF triggered\n");
			Reset_McuSystem ();
		}

	}else{
		valid_press = 1;
		count_flag = 0;
	}
#else
	if(GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX23) == 0 && valid_press ==1)
	{
		count_flag = 1;

		if(power_count > 1000 && power_flag == 0 && GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX23) == 0){
			count_flag  = 0;
			valid_press  = 0;
			power_on();
			power_flag = 1;
			power_count=0;
			DBG("Power ON triggered\n");

		}else if (power_count > 1000 && power_flag == 1 && GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX23) == 0)
		{
			valid_press  = 0;
			count_flag  = 0;
			power_off();
			power_flag = 0;
			power_count = 0;
			DBG("Power OFF triggered\n");
			Reset_McuSystem ();
		}

	}else{
		valid_press = 1;
		count_flag = 0;
	}
#endif
}

#if HW_DRV_BT_EN
/* comp_statusbar.h removed - provide stub */
__attribute__((weak)) void UI_StatusBar_SetBTStatus(uint8_t status) { (void)status; }
#endif
static uint8_t time_count = 0;
static uint16_t battery_report_count = 0;

void hardware_check()
{
#if HW_DRV_BT_EN
	UI_StatusBar_SetBTStatus(GetA2dpState());
#endif

	time_count++;
	if(time_count>=100){
		if(ADC_SingleModeDataGet(ADC_CHANNEL_POWERKEY)>4000){
			 AudioSetting_SetGuitar2VolumePercent(AudioSetting_GetGuitar2VolumePercent()) ;
		}else{
			 AudioSetting_SetGuitar2VolumePercent(0) ;
		}
		time_count = 0;
	}

#if HW_DRV_BT_EN
	battery_report_count++;
	if (battery_report_count >= 600) {
		battery_report_count = 0;
		if (BleConnectFlag) {
			uint8_t payload[2];
			payload[0] = BLE_SYSTEM_SUB_BATTERY;
			payload[1] = BattCalib_GetSOC();
			BleProto_SendOnce(BLE_CMD_SYSTEM, payload, 2);
		}
	}
#endif

#if HW_DRV_BATTERY_EN
	BattCalib_Tick();
#endif

	SysState_Update();
}

void MainTask() {

#if BUTTON_POWER_ENABLE
	pwr_button_init();
	while (!power_flag)
	{
		pwr_butoon_handler();
	}
#else
	power_on();
	DBG("Power ON directly (BUTTON_POWER_ENABLE disabled)\n");
#endif

#if CDC_FILE_MANAGER_EN
	CDC_FileManager_Init();
#endif

	while (1) {

#if HW_DRV_BT_EN
		extern void BLE_CheckSyncResponse(void);
		BLE_CheckSyncResponse();

		extern void BleProto_Process(void);
		BleProto_Process();
#endif

#if CDC_FILE_MANAGER_EN
		if (!CDC_FileManager_InMode()) {
			CDC_FileManager_CheckEnter();
		}
		if (CDC_FileManager_InMode()) {
			CDC_FileManager_Process();
			continue;
		}
#endif

		BG_AudioManager.Audio_Loop();

#if BANGTSYNTH_EN && SYNTH_SD_NAND_PSRAM_EN
		SYNTH_LoadTick();
#endif

		if(UI_flag == 1){
			UI_flag = 0;

			hardware_check();

#ifdef UI_EN
			UI_StatusBar_ScanDetect();
			if (Shell_ConsoleIsEnabled()) {
				Shell_ConsoleUpdate();
			} else {
				BG_UI.Update(20);
				UI_StatusBar_Update();
			}
#endif

#if HW_DRV_LCD_EN && defined(USE_FRAME_BUFFER)
			BG_lcd.FlushFrameBuffer();
#endif
		}
	}
}

uint32_t BLE_GetTick(void) {
    return ble_tick_counter;
}

uint8_t BLE_IsDelayElapsed(uint32_t start_tick, uint32_t delay_ms) {
    uint32_t current_tick = ble_tick_counter;
    uint32_t elapsed_ticks = current_tick - start_tick;
    return (elapsed_ticks >= delay_ms);
}

void prvInitialiseHeap(void);

int main(void) {
	Chip_Init(1);
	WDG_Disable();

	Clock_Module1Enable(ALL_MODULE1_CLK_SWITCH);
	Clock_Module2Enable(ALL_MODULE2_CLK_SWITCH);
	Clock_Module3Enable(ALL_MODULE3_CLK_SWITCH);

	Clock_Module1Enable(ALL_MODULE1_CLK_SWITCH);
	Clock_Module2Enable(ALL_MODULE2_CLK_SWITCH);
	Clock_Module3Enable(ALL_MODULE3_CLK_SWITCH);

	Clock_Config(1, 24000000);
	Clock_PllLock(288000);
	Clock_APllLock(240000);

	Clock_SysClkSelect(PLL_CLK_MODE);
	Clock_UARTClkSelect(APLL_CLK_MODE);
	Clock_Timer3ClkSelect(SYSTEM_CLK_MODE);

	Clock_USBClkDivSet(4);
	Clock_USBClkSelect(APLL_CLK_MODE);

	GPIO_PortAModeSet(GPIOA9, 1);
	GPIO_PortAModeSet(GPIOA10, 3);
	DbgUartInit(1, 115200, 8, 0, 1);

	Clock_USBClkDivSet(4);
	Clock_USBClkSelect(APLL_CLK_MODE);

	Remap_InitTcm(0, 12);
	SpiFlashInit(80000000, MODE_4BIT, 0, 1);
	DMA_ChannelAllocTableSet(DmaChannelMap);

#if FLASH_BOOT_EN
	report_up_grate();
#endif

	GIE_ENABLE();
	Timer_Config(TIMER2, 1000, 0);
	Timer_Start(TIMER2);
	NVIC_EnableIRQ(Timer2_IRQn);

	DBG("****************************************************************\n");
	DBG("                     BanDataHub SDK                             \n");
	DBG("****************************************************************\n");

	prvInitialiseHeap();

	NVIC_EnableIRQ(SWI_IRQn);

	SarADC_Init();
	xQueue = xQueueCreate(4, sizeof(uint32_t));

	DBG("[Main] Initializing SPI hardware...\n");
	spi_init();
	DBG("[Main] SPI initialized successfully\n");

	DBG("[Main] Initializing Driver Framework (before RTOS)...\n");
	DrvFramework_FullInit();
	DBG("[Main] Driver Framework initialized successfully\n");

	xTaskCreate((TaskFunction_t )MainTask, "MainTask", 4096, NULL, 1, NULL);

	DBG("[Main] Starting FreeRTOS scheduler...\n");
	vTaskStartScheduler();

	while (1)
		;
}
