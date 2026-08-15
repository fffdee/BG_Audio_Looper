/**
 **************************************************************************************
 * @file    main.c
 * @brief   Bootloader – USB CDC firmware upgrade only
 *
 * Minimal bootloader:
 *   1. Initialise hardware (chip, clock, UART, SPI flash, DMA, Timer2)
 *   2. Check partition flags; jump to application if valid firmware found
 *   3. If no valid firmware, start FreeRTOS and run USB CDC upgrade task
 *
 * No audio, no BLE, no power-button handling — just boot-or-upgrade.
 **************************************************************************************
 */

/* Version: increment on each release (format V<major>.<minor>.<patch>) */
#define BOOTLOADER_VERSION_MAJOR   0
#define BOOTLOADER_VERSION_MINOR   2
#define BOOTLOADER_VERSION_PATCH   1
#define BOOTLOADER_VERSION_STR     "V0.2.1"

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

#include <string.h>
#include <stdio.h>

/* USB CDC + Audio (copied from BanBox) */
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_cdc.h"
#include "otg_detect.h"
#include "usb_audio_api.h"
#include "audio_api.h"

#include "upgrade.h"
#include "usb_identity.h"

/* ──────────────────────────────────────────────────────────────────────────
 * Timer2 ISR (1 ms tick)
 * Services USB port-link detection and USB audio state machine.
 * UsbAudioTimer1msProcess() handles speaker/mic enable-disable logic
 * based on USB streaming activity (AltSet/FramCount).
 * ────────────────────────────────────────────────────────────────────────── */
void Timer2Interrupt(void) {
	Timer_InterruptFlagClear(TIMER2, UPDATE_INTERRUPT_SRC);
	OTG_PortLinkCheck();
	UsbAudioTimer1msProcess();
}

/* ──────────────────────────────────────────────────────────────────────────
 * DMA channel allocation table
 * UART1 RX/TX and audio ADC/DAC channels are assigned.
 * NOTE: audio_init() in audio_api.c will overwrite this table with its own
 *       DmaChannelMap that allocates channels 0-3 for ADC0/1 + DAC0/1.
 *       The assignments here are for the bootloader phase before audio_init()
 *       is called, so UART1 debug output works during early boot.
 * ────────────────────────────────────────────────────────────────────────── */
static uint8_t DmaChannelMap[29] = {
		255, //PERIPHERAL_ID_SPIS_RX = 0,
		255, //PERIPHERAL_ID_SPIS_TX,
		255, //PERIPHERAL_ID_TIMER3,
		255, //PERIPHERAL_ID_SDIO_RX,
		255, //PERIPHERAL_ID_SDIO_TX,
		255, //PERIPHERAL_ID_UART0_RX,
		255, //PERIPHERAL_ID_TIMER1,
		255, //PERIPHERAL_ID_TIMER2,
		255, //PERIPHERAL_ID_SDPIF_RX,
		255, //PERIPHERAL_ID_SDPIF_TX,
		255, //PERIPHERAL_ID_SPIM_RX,
		255, //PERIPHERAL_ID_SPIM_TX,
		255, //PERIPHERAL_ID_UART0_TX,
		7,   //PERIPHERAL_ID_UART1_RX,
		6,   //PERIPHERAL_ID_UART1_TX,
		255, //PERIPHERAL_ID_TIMER4,
		255, //PERIPHERAL_ID_TIMER5,
		255, //PERIPHERAL_ID_TIMER6,
		0,   //PERIPHERAL_ID_AUDIO_ADC0_RX,  -- audio_init() reuses
		1,   //PERIPHERAL_ID_AUDIO_ADC1_RX,  -- audio_init() reuses
		2,   //PERIPHERAL_ID_AUDIO_DAC0_TX,  -- audio_init() reuses
		3,   //PERIPHERAL_ID_AUDIO_DAC1_TX,  -- audio_init() reuses
		255, //PERIPHERAL_ID_I2S0_RX,
		255, //PERIPHERAL_ID_I2S0_TX,
		255, //PERIPHERAL_ID_I2S1_RX,
		255, //PERIPHERAL_ID_I2S1_TX,
		255, //PERIPHERAL_ID_PPWM,
		255, //PERIPHERAL_ID_ADC,
		255, //PERIPHERAL_ID_SOFTWARE,
		};

/* ──────────────────────────────────────────────────────────────────────────
 * USB CDC upgrade task
 * ────────────────────────────────────────────────────────────────────────── */
static void UpgradeTask(void)
{
	/* AUDIO_MIC_CDC: CDC+声卡复合设备模式
	 * Bootloader 使用 BG 身份协议 VID/PID（见 usb_identity.h），
	 * 上位机据此识别产品并进入升级。APP 使用不同 VID/PID (0x1234/0x1234)。
	 * UsbDeviceEnable() 内部调用 OTG_DeviceInit() + NVIC_EnableIRQ(Usb_IRQn) */
	OTG_DeviceModeSel(AUDIO_MIC_CDC, BOOTLOADER_USB_VID, BOOTLOADER_USB_PID);
	UsbDevicePlayInit();
	UsbDeviceEnable();
	/* audio_init() 复用 USB 输入流（speaker 播放）和麦克风输入流（mic capture）
	 * 内部会重新分配 DMA 通道表，初始化 ADC1/DAC0/DAC1 + FIFO */
	audio_init(44100);
	Upgrade_Init();

	DBG("[BOOT] USB CDC+Audio upgrade ready (VID=0x%04X PID=0x%04X)\n",
	    BOOTLOADER_USB_VID, BOOTLOADER_USB_PID);

	while (1) {
		/* Service USB enumeration and control requests */
		OTG_DeviceRequestProcess();

		/* Drive CDC RX/TX ring-buffers */
		OTG_DeviceCDC_Task();

		/* Drive USB audio speaker/mic data flow (1ms tick from Timer2 ISR
		 * updates usb_speaker_enable / usb_mic_enable flags) */
		audio_process();

		/* Firmware upgrade state machine */
		Upgrade_Process();
	}
}

/* ──────────────────────────────────────────────────────────────────────────
 * Entry point
 * ────────────────────────────────────────────────────────────────────────── */
void prvInitialiseHeap(void);

int main(void) {
	Chip_Init(1);
	WDG_Disable();

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

	GPIO_PortAModeSet(GPIOA9, 1);    /* UART1 RX  */
	GPIO_PortAModeSet(GPIOA10, 3);   /* UART1 TX  */
	DbgUartInit(1, 115200, 8, 0, 1);

	/* CRITICAL: Disable any stale partition-B remap from a previous boot.
	 * If the remap persists across reset (e.g. watchdog reset after APP
	 * crash), CPU reads/writes to 0x040000 would be redirected to 0x240000
	 * (or vice-versa), corrupting the firmware check and partition flag
	 * operations. This must happen BEFORE Boot_CheckAndJumpIfNeeded(). */
	Remap_AddrRemapDisable(ADDR_REMAP0);
	Remap_AddrRemapDisable(ADDR_REMAP1);
	Remap_AddrRemapDisable(ADDR_REMAP2);

	Remap_InitTcm(0, 12);
	SpiFlashInit(80000000, MODE_4BIT, 0, 1);
	DMA_ChannelAllocTableSet(DmaChannelMap);

	/* Detect flash capacity and compute safe partition flag address.
	 * This must be called BEFORE Boot_CheckAndJumpIfNeeded() and
	 * BEFORE any PartFlag_Read/Write calls. */
	PartFlag_Init();

	GIE_ENABLE();

	Timer_Config(TIMER2, 1000, 0);
	Timer_Start(TIMER2);
	NVIC_EnableIRQ(Timer2_IRQn);

	DBG("****************************************************************\n");
	DBG("                          BG_CARD SDK                           \n");
	DBG("                          Bootloader " BOOTLOADER_VERSION_STR "                    \n");
	DBG("****************************************************************\n");

	/* Check partition flags; jump to application if not in upgrade mode.
	 * This call may never return (when a valid application is found). */
	Boot_CheckAndJumpIfNeeded();

	/* No valid firmware — stay resident for USB CDC upgrade */
	prvInitialiseHeap();
	NVIC_EnableIRQ(SWI_IRQn);

	
	xTaskCreate((TaskFunction_t)UpgradeTask, "UpgTask", 2048, NULL, 1, NULL);

	DBG("[Main] Starting FreeRTOS scheduler...\n");
	vTaskStartScheduler();

	while (1)
		;
}
