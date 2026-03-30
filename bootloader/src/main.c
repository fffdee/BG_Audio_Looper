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
#include "clk.h"
#include "dma.h"
#include "timer.h"
#include "watchdog.h"
#include "spi_flash.h"
#include "remap.h"
#include "irqn.h"
#include "FreeRTOS.h"
#include "task.h"
#include "delay.h"
#include "chip_info.h"

#include <string.h>
#include <stdio.h>

/* USB */
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_stor.h"
#include "otg_device_cdc.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "otg_device_audio.h"

/* Bluetooth / BLE – REMOVED from bootloader.
 * BLE OTA is now handled entirely inside the user firmware (BanBox). */
// #include "bt_stack_service.h"

/* Firmware upgrade over USB CDC only (BLE removed) */
#include "upgrade.h"
#include "audio_api.h"

uint8_t power_flag = 0;
uint8_t count_flag = 0;
uint16_t power_count =0;
void Timer2Interrupt(void) {
	Timer_InterruptFlagClear(TIMER2, UPDATE_INTERRUPT_SRC);
	OTG_PortLinkCheck();
#ifdef CFG_APP_USB_AUDIO_MODE_EN
	UsbAudioTimer1msProcess();
#endif

	if(count_flag){
		power_count++;
	}
	else{
		power_count = 0;
	}
}

static uint8_t DmaChannelMap[29] = {
		255, //PERIPHERAL_ID_SPIS_RX = 0,		//0
		255, //PERIPHERAL_ID_SPIS_TX,			//1
		255, //PERIPHERAL_ID_TIMER3,			//2
		255, //PERIPHERAL_ID_SDIO_RX,			//3
		255, //PERIPHERAL_ID_SDIO_TX,			//4
		255, //PERIPHERAL_ID_UART0_RX,			//5
		255, //PERIPHERAL_ID_TIMER1,				//6
		255, //PERIPHERAL_ID_TIMER2,				//7
		255, //PERIPHERAL_ID_SDPIF_RX,			//8
		255, //PERIPHERAL_ID_SDPIF_TX,			//9
		255, //PERIPHERAL_ID_SPIM_RX,			//10  (not used)
		255, //PERIPHERAL_ID_SPIM_TX,			//11  (not used)
		255, //PERIPHERAL_ID_UART0_TX,			//12
		7,   //PERIPHERAL_ID_UART1_RX,			//13
		6,   //PERIPHERAL_ID_UART1_TX,			//14
		255, //PERIPHERAL_ID_TIMER4,				//15
		255, //PERIPHERAL_ID_TIMER5,				//16
		255, //PERIPHERAL_ID_TIMER6,				//17
		0,   //PERIPHERAL_ID_AUDIO_ADC0_RX,		//18
		1,   //PERIPHERAL_ID_AUDIO_ADC1_RX,		//19
		2,   //PERIPHERAL_ID_AUDIO_DAC0_TX,		//20
		3,   //PERIPHERAL_ID_AUDIO_DAC1_TX,		//21
		255, //PERIPHERAL_ID_I2S0_RX,			//22
		255, //PERIPHERAL_ID_I2S0_TX,			//23
		255, //PERIPHERAL_ID_I2S1_RX,			//24
		255, //PERIPHERAL_ID_I2S1_TX,			//25
		255, //PERIPHERAL_ID_PPWM,				//26
		255, //PERIPHERAL_ID_ADC,     			//27
		255, //PERIPHERAL_ID_SOFTWARE,			//28
		};


/**
 * 闊抽涓诲惊鐜鐞嗗嚱鏁�
 */
/**
 * @brief USB 鐑彃鎷旀娴嬩笌閲嶅垵濮嬪寲锛堜笌 UI 绯荤粺瑙ｈ�锛岀洿鎺ュ湪闊抽寰幆涓鐞嗭級
 *
 * 姣忔 Audio_loop 璋冪敤鏃堕�杩囪鏁板櫒闄愰�锛岀害姣�100ms 杞涓� USB 杩炴帴鐘舵�銆�
 * 鐘舵�鍙樺寲鏃剁珛鍗宠皟鐢�UsbDeviceEnable / UsbDeviceDisable锛屾棤闇�緷璧�UI 鏇存柊璺緞銆�
 */
static void USB_HotplugCheck(void)
{
	static bool last_usb_connected = false;
	static uint32_t check_counter   = 0;

	/* 闄愰�锛氫笉闇�姣忔闊抽寰幆閮借疆璇紝姣�~100ms 妫�煡涓�鍗冲彲 */
	if (++check_counter < 100)
		return;
	check_counter = 0;

	bool now_connected = OTG_PortDeviceIsLink();
	if (now_connected == last_usb_connected)
		return;

	last_usb_connected = now_connected;
	if (now_connected) {
		DBG("[USB] Device connected - re-enabling USB device\n");
		UsbDeviceEnable();
	} else {
		DBG("[USB] Device disconnected - disabling USB device\n");
		UsbDeviceDisable();
	}
}

void power_on(void)
{
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX24);
	DBG("[Main] Power ON\n");
}



// 初始化 USB 设备：音频声卡(Speaker+Mic) + CDC 串口升级
static void InitUSBDevice(void)
{
	/* AUDIO_MIC_CDC: USB Audio (Speaker + Mic) + CDC serial, all active. */
	OTG_DeviceModeSel(AUDIO_MIC_CDC, 0x8888, 0x1722);
	UsbDevicePlayInit();
	UsbDeviceEnable();

	/* Initialise DAC hardware for USB speaker playback.
	 * DMA channels 2/3 (DAC0/DAC1) must already be allocated above. */
	audio_init(44100);
}


void power_off(void)
{
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX24);
	DBG("[Main] Power OFF\n");
}
void pwr_button_init()
{
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
}

uint8_t valid_press = 0;
void pwr_butoon_handler()
{
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

		}

	}else{
		valid_press = 1;
		count_flag = 0;
	}


}






void UpdataTask(void)
{
	InitUSBDevice();
	Upgrade_Init();

	/* BLE upgrade is handled in user firmware (BanBox).
	 * Bootloader only supports USB CDC upgrade for factory flashing. */

	while (1) {
		/* Must be called every loop iteration to service USB enumeration
		 * and control requests.  Without this the host never sees the
		 * device (no descriptor response → no recognition). */
		OTG_DeviceRequestProcess();

		/* Drive CDC RX/TX ring-buffers (required for data transfer). */
		OTG_DeviceCDC_Task();

		/* Firmware upgrade via USB CDC */
		Upgrade_Process();

		/* USB audio: play speaker data via DAC; feed ADC to USB mic.
		 * Paused while a firmware write is in progress to avoid flash
		 * erase latency causing audio glitches or CDC timeouts. */
		if (!Upgrade_IsActive()) {
			audio_process();
		}

		USB_HotplugCheck();
	}
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

	GPIO_PortAModeSet(GPIOA9, 1);				//Rx, A24:uart1_rxd_0
	GPIO_PortAModeSet(GPIOA10, 3);				//Tx, A25:uart1_txd_0
	DbgUartInit(1, 115200, 8, 0, 1);

	Clock_USBClkDivSet(4);
	Clock_USBClkSelect(APLL_CLK_MODE);

	Remap_InitTcm(0, 12);
	SpiFlashInit(80000000, MODE_4BIT, 0, 1);
	DMA_ChannelAllocTableSet(DmaChannelMap);

	GIE_ENABLE();
//	SysTickInit();
	Timer_Config(TIMER2, 1000, 0);
	Timer_Start(TIMER2);
	NVIC_EnableIRQ(Timer2_IRQn);

	DBG("****************************************************************\n");
	DBG("                          BG_CARD SDK                           \n");
	DBG("****************************************************************\n");

	/* Check partition flags; jump to application if not in upgrade mode.
	 * This call may never return (when a valid application is found). */
	Boot_CheckAndJumpIfNeeded();

	prvInitialiseHeap();

	NVIC_EnableIRQ(SWI_IRQn);

	xTaskCreate((TaskFunction_t )UpdataTask, "UpdataTask", 4096, NULL, 1, NULL);


	DBG("[Main] Starting FreeRTOS scheduler...\n");
	vTaskStartScheduler();

	while (1)
		;
}


