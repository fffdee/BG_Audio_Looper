/**
 **************************************************************************************
 * @file    main.c
 * @brief   BanEffector application entry (banux2 second-generation framework facade).
 *
 * Boot model:
 *   main()          -> chip/clock/UART/SPI-flash/DMA/Timer2/heap/SarADC init,
 *                      SysState_Init, create MainTask, start FreeRTOS scheduler.
 *   MainTask()      -> Banux_Init(facade) -> power_on() -> FwUpgrade -> while(1) Banux_Process().
 *
 * Banux facade wiring (BanuxConfig_t):
 *   logWriter       = App_LogWriter          (route framework DBG to UART printf)
 *   shellIo         = ShellIO_CDC_Get()       (ShellIOManager auto-switches CDC<->BLE in Audio_Loop)
 *   filesystemInit  = NULL                    (no file system: VFS/FatFs/InternalFlashFs all disabled)
 *   driverInit      = BanuxDriver_RegisterAll (USB-CDC / NAND / PSRAM / Battery DrvDevice)
 *   platformInit    = NULL                    (see note below)
 *   platformProcess = App_PlatformProcess     (BLE + Audio_Loop + SysState/hardware_check)
 *
 * NOTE on platformInit: Banux_Init() invokes platformInit *before* Shell_Init() and
 * driverInit(). Audio_Init() internally calls ShellIOManager_Init() (needs the shell)
 * and SysParam/BLE need the driver framework, so the full power_on() sequence is run
 * explicitly in MainTask() *after* Banux_Init() returns. This preserves the original
 * one-generation ordering (framework init -> power_on in RTOS context).
 *
 * @author  Peter
 * @version V2.0.0
 **************************************************************************************
 */

/* Version: increment on each release (format V<major>.<minor>.<patch>) */
#define APP_VERSION_MAJOR   0
#define APP_VERSION_MINOR   2
#define APP_VERSION_PATCH   1
#define APP_VERSION_STR     "V0.2.15"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <nds32_intrinsic.h>

/* SDK / chip */
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
#include "chip_info.h"
#include "reset.h"              /* Reset_McuSystem() soft reset */
#include "audio_adc.h"
#include "adc_interface.h"
#include "sadc_interface.h"     /* SarADC_Init() */
#include "dac_interface.h"
#include "spim_interface.h"
#include "spim.h"
#include "otg_detect.h"
#include "usb_audio_api.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "delay.h"

/* Banux second-generation framework facade */
#include "Banux.h"
#include "driver_init.h"        /* BanuxDriver_RegisterAll */
#include "shell_io_cdc.h"       /* ShellIO_CDC_Get */
#include "shell_io_ble.h"       /* BLE_CheckSyncResponse */

/* Product / board configuration */
#include "product_def.h"
#include "app_config.h"         /* HAS_BOOTLOADER */

/* Audio (direct in/out effect graph) */
#include "ctrlvars.h"
#include "bg_audio_io_manager.h"
#include "bg_audio_detection.h"

/* BLE control channel */
#include "ble_protocol.h"

/* System services */
#include "sys_param.h"
#include "sys_state.h"
#include "app_sys_handler.h"
#include "battery_drv.h"
#include "battery_calib.h"
#include "fw_upgrade.h"
#if SYS_LED_EN
#include "sys_led.h"
#endif

extern void UsbAudioTimer1msProcess(void);
extern void prvInitialiseHeap(void);

/*------------------------------------------------------------------------------
 * Global state
 *----------------------------------------------------------------------------*/
uint8_t  record_flag = 0;
uint8_t  UI_flag = 0;
uint16_t time = 0;

/* BLE sync command timing (1 ms tick counter, incremented in Timer2 ISR) */
static uint32_t ble_tick_counter = 0;

uint8_t  power_flag = 0;
uint8_t  count_flag = 0;
uint16_t power_count = 0;

xQueueHandle xQueue;

uint8_t spimRate = SPIM_CLK_DIV_12M;
uint8_t spimMode = 0;

const char* spimIO[][4] = {
//    cs      miso     clk      mosi
		{ "A22", "A7", "A6", "A5" }, { "A8", "A22", "A21", "A20" }, };

static uint8_t DmaChannelMap[29] = {
		255, //PERIPHERAL_ID_SPIS_RX = 0,		//0
		255, //PERIPHERAL_ID_SPIS_TX,			//1
		255, //PERIPHERAL_ID_TIMER3,			//2
		8, //PERIPHERAL_ID_SDIO_RX,
		9, //PERIPHERAL_ID_SDIO_TX,
		255, //PERIPHERAL_ID_UART0_RX,			//5
		255, //PERIPHERAL_ID_TIMER1,				//6
		255, //PERIPHERAL_ID_TIMER2,				//7
		255, //PERIPHERAL_ID_SDPIF_RX,			//8
		255, //PERIPHERAL_ID_SDPIF_TX,			//9
		0, //PERIPHERAL_ID_SPIM_RX,			//10
		1, //PERIPHERAL_ID_SPIM_TX,			//11
		255, //PERIPHERAL_ID_UART0_TX,			//12
		7, //PERIPHERAL_ID_UART1_RX,			//13
		6, //PERIPHERAL_ID_UART1_TX,			//14
		255, //PERIPHERAL_ID_TIMER4,				//15
		255, //PERIPHERAL_ID_TIMER5,				//16
		255, //PERIPHERAL_ID_TIMER6,				//17
		2, //PERIPHERAL_ID_AUDIO_ADC0_RX,		//18
		3, //PERIPHERAL_ID_AUDIO_ADC1_RX,		//19
		4, //PERIPHERAL_ID_AUDIO_DAC0_TX,		//20
		5, //PERIPHERAL_ID_AUDIO_DAC1_TX,		//21
		255, //PERIPHERAL_ID_I2S0_RX,			//22
		255, //PERIPHERAL_ID_I2S0_TX,			//23
		255, //PERIPHERAL_ID_I2S1_RX,			//24
		255, //PERIPHERAL_ID_I2S1_TX,			//25
		255, //PERIPHERAL_ID_PPWM,				//26
		255, //PERIPHERAL_ID_ADC,     			//27
		255, //PERIPHERAL_ID_SOFTWARE,			//28
		};

/*------------------------------------------------------------------------------
 * Timer2 1 ms ISR
 *----------------------------------------------------------------------------*/
void Timer2Interrupt(void) {
	Timer_InterruptFlagClear(TIMER2, UPDATE_INTERRUPT_SRC);
	OTG_PortLinkCheck();

	if (time > 50)
		time = 0;
	if (time == 0)
		UI_flag = 1;

	time++;

	if (record_flag == 0) {
		record_flag = 1;
	}

#ifdef CFG_APP_USB_AUDIO_MODE_EN
	UsbAudioTimer1msProcess(); /* 1 ms audio tick / ping-pong */
#endif

	/* Increment BLE tick counter for sync command timing */
	ble_tick_counter++;

#if SYS_LED_EN
	SysLed_Tick1ms();
#endif

	if (count_flag) {
		power_count++;
	} else {
		power_count = 0;
	}
}

/*------------------------------------------------------------------------------
 * SPI helpers
 *----------------------------------------------------------------------------*/
void spi_init(void) {
	SPIM_SetDmaEn(1);
	SPIM_IoConfig(SPIM_PORT0_A5_A6_A7);
	Clock_SPIMClkDivSet(1);
	DMA_ChannelAllocTableSet(DmaChannelMap);
	if (SPIM_Init(spimMode, spimRate)) {
		DBG("SPI init success!\n");
		DBG("spim mode:%d\n", spimMode);
		DBG("spim rate:%d\n", spimRate);
		DBG("spim_cs  :%s\n", spimIO[0][0]);
		DBG("spim_miso:%s\n", spimIO[0][1]);
		DBG("spim_clk :%s\n", spimIO[0][2]);
		DBG("spim_mosi:%s\n", spimIO[0][3]);
	} else {
		DBG("****** Err: SPI init fail ******\n");
	}
}

void spi_write(uint8_t *data, uint16_t size)
{
	SPIM_DMA_Send_Start(data, size);
	while (!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));
}

void spi_read(uint8_t *data, uint16_t size)
{
	SPIM_DMA_Recv_Start(data, size);
	while (!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));
}

/*------------------------------------------------------------------------------
 * Banux framework log writer (routes BanuxDebug_Printf output to UART printf)
 *----------------------------------------------------------------------------*/
static void App_LogWriter(const char *text)
{
	printf("%s", text);
}

/*------------------------------------------------------------------------------
 * Power on: application-level bring-up (runs after Banux_Init in MainTask)
 *----------------------------------------------------------------------------*/
void power_on()
{
	/* SysState: advance OFF -> BOOT -> RUNNING (executes registered PowerOn
	 * callbacks and IO init). The IO config is set in MainTask before this. */
	SysState_PowerOn();

#if SYS_LED_EN
	SysLed_Init();
#else
	GPIO_RegOneBitClear(GPIO_A_IE, HW_LED_GPIO_PIN);
	GPIO_RegOneBitSet(GPIO_A_OE, HW_LED_GPIO_PIN);
	GPIO_RegOneBitSet(GPIO_A_OUT, HW_LED_GPIO_PIN);
#endif

	/* Initialize audio control-variable defaults (effect params, gains, etc.).
	 * Must run before SysParam_Init() and Audio_Init(), otherwise gCtrlVars is
	 * all-zero and every effect parameter is invalid -> no sound. */
	CtrlVarsInit();

	DBG("[Task] Hardware drivers already initialized in main()\n");

	/*===== System Parameter Initialization =====
	 * Load saved parameters from flash into global variables. Must run after
	 * hardware/driver init and before functional modules. */
	DBG("[Task] Loading system parameters from flash...\n");
	{
		SysParam_Status_t param_status = SysParam_Init();
		if (param_status == SYSPARAM_OK) {
			DBG("[Task] Parameters loaded successfully from flash\n");
			/* Apply saved parameters to audio (override CtrlVarsInit defaults) */
			SysParam_ApplyToAudio();
		} else {
			DBG("[Task] Using default parameters (status=%d)\n", param_status);
			/* First boot or flash corruption - defaults already loaded/saved */
			SysParam_ApplyToAudio();
		}
	}

	/* Audio graph: ADC0(guitar)+ADC1(mic)+USB_IN -> Mixer -> DAC0(speaker)+USB_OUT */
	BG_AudioManager.Audio_Init(44100);

	/*===== BLE application layer: sync provider + data handler =====
	 * Register callbacks (BleApp_Init) before initializing the protocol
	 * (BleProto_Init) so the send-init handler is ready. */
	{
		extern void BleApp_Init(void);
		BleApp_Init();
		extern void BleProto_Init(void);
		BleProto_Init();
		DBG("[Task] BLE app layer initialized (sync + data handler)\n");
	}

	/*===== Battery calibration: load saved curve from flash =====*/
	BattCalib_Init();
	{
		extern void BattCalib_RegisterFeedActivity(void (*feed_fn)(uint8_t));
		extern void LowPower_FeedActivity(uint8_t mask);
		BattCalib_RegisterFeedActivity(LowPower_FeedActivity);
	}
	DBG("[Task] Battery calibration initialized\n");

	DBG("[Main] System initialized successfully\n");
	DBG("[Main] Entering main loop...\n");
}

/*------------------------------------------------------------------------------
 * Power off: save params + release audio memory + LED off
 *----------------------------------------------------------------------------*/
void power_off()
{
	/* SysState: advance RUNNING/IDLE -> SHUTDOWN -> OFF (executes PowerOff callbacks) */
	SysState_PowerOff();

	/* Release reverb memory (~57KB heap) before shutdown */
	BG_AudioIO_PrepareForShutdown();

	/* Save modified parameters to flash so user settings are not lost */
	if (SysParam_IsModified()) {
		DBG("[PowerOff] Saving modified parameters to flash...\n");
		SysParam_Save();
	}

	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX24);
#if SYS_LED_EN
	SysLed_SetMode(SYS_LED_MODE_OFF);
#else
	GPIO_RegOneBitClear(GPIO_A_OUT, HW_LED_GPIO_PIN);
#endif
}

/*------------------------------------------------------------------------------
 * Power button (BUTTON_POWER_ENABLE boards)
 *----------------------------------------------------------------------------*/
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
	if (GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX23) == 0 && valid_press == 1) {
		count_flag = 1;

		if (power_count > 1000 && power_flag == 0 && GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX23) == 0) {
			count_flag = 0;
			valid_press = 0;
			power_on();
			power_flag = 1;
			power_count = 0;
			DBG("Power ON triggered\n");
		} else if (power_count > 1000 && power_flag == 1 && GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX23) == 0) {
			valid_press = 0;
			count_flag = 0;
			power_off();
			power_flag = 0;
			power_count = 0;
			DBG("Power OFF triggered\n");
			Reset_McuSystem();
		}
	} else {
		valid_press = 1;
		count_flag = 0;
	}
}

/*------------------------------------------------------------------------------
 * 50 ms hardware/status check
 *----------------------------------------------------------------------------*/
uint8_t time_count = 0;

void hardware_check()
{
	time_count++;
	if (time_count >= 20) {   /* once per second (50ms tick x 20) */
		time_count = 0;

		/* Line2 (Line-In right) insert detection: POWERKEY ADC independent sample */
		BG_AudioDetection_Line2Poll();
	}
	/* Battery calibration voltage tick (~50ms per call) */
	BattCalib_Tick();
	/* System state machine update (idle/transfer detection, publishes events) */
	SysState_Update();

	/* Application 50ms tick (LED blink timing, BLE battery report) */
	AppSys_LedTick();
	AppSys_BatteryTick();
}

/*------------------------------------------------------------------------------
 * Banux platformProcess hook: one nonblocking application iteration.
 * Called by Banux_Process() from the MainTask while(1) loop.
 *----------------------------------------------------------------------------*/
static void App_PlatformProcess(void)
{
	/* Delayed BLE sync responses. Skipped during CDC upgrade mode - the BLE API
	 * may touch USB/BT state that is unsafe while the CDC data path is active. */
	BLE_CheckSyncResponse();

	/* BLE application protocol (control channel) */
	{
		extern void BleProto_Process(void);
		BleProto_Process();
	}

	/* CDC firmware upgrade mode - auto-detect SOF or process packets */
	if (!FwUpgrade_InCdcMode()) {
		FwUpgrade_CheckCdcEnter();  /* sniff 0xAA SOF, auto-enter upgrade mode */
	}
	if (FwUpgrade_InCdcMode()) {
		FwUpgrade_ProcessCdc();
		return;  /* skip audio/shell during upgrade */
	}

	/* Audio graph processing (also drives BT/BLE stack + ShellIOManager internally) */
	BG_AudioManager.Audio_Loop();

	/* 50 ms UI/hardware tick */
	if (UI_flag == 1) {
		UI_flag = 0;
		hardware_check();
	}
}

/*------------------------------------------------------------------------------
 * BLE timing helpers for sync command buffering
 *----------------------------------------------------------------------------*/
uint32_t BLE_GetTick(void) {
	return ble_tick_counter;
}

uint8_t BLE_IsDelayElapsed(uint32_t start_tick, uint32_t delay_ms) {
	uint32_t current_tick = ble_tick_counter;
	uint32_t elapsed_ticks = current_tick - start_tick;
	/* Timer2Interrupt runs at 1 ms, so ticks == ms */
	return (elapsed_ticks >= delay_ms);
}

/*------------------------------------------------------------------------------
 * MainTask: Banux framework init -> power_on -> upgrade engine -> Banux_Process loop
 *----------------------------------------------------------------------------*/
void MainTask() {

	/* Configure power-on IO levels (applied by SysState_PowerOn) */
	{
		static const SysIoConfig_t io_cfg = {
			.port_out_set   = 0,
			.port_out_clear = 0,
			.port_oe_set    = HW_LED_GPIO_PIN,
			.port_oe_clear  = 0,
			.port_ie_clear  = HW_LED_GPIO_PIN,
			.port_pu_set    = 0,
			.port_pd_set    = 0,
		};
		SysState_SetIoConfig(&io_cfg);
	}

	/* Initialize the Banux second-generation framework: components, event system,
	 * driver framework, platform DrvDevice registration and the Shell (over CDC,
	 * auto-switching to BLE at runtime via ShellIOManager inside Audio_Loop). */
	{
		BanuxConfig_t banux_cfg;
		int init_ret;

		banux_cfg.logWriter       = App_LogWriter;
		banux_cfg.shellIo         = ShellIO_CDC_Get();
		banux_cfg.filesystemInit  = NULL;                 /* no file system */
		banux_cfg.driverInit      = BanuxDriver_RegisterAll;
		banux_cfg.platformInit    = NULL;                 /* power_on() runs below (needs shell+drivers) */
		banux_cfg.platformProcess = App_PlatformProcess;

		init_ret = Banux_Init(&banux_cfg);
		if (init_ret != 0) {
			DBG("[Main] Banux_Init failed: %d\n", init_ret);
		}
	}

#if BUTTON_POWER_ENABLE
	pwr_button_init();
	/* Button power mode: hold button 1s to power on */
	while (!power_flag) {
		pwr_butoon_handler();
	}
#else
	/* Direct power-on mode: no button needed */
	power_on();
	DBG("Power ON directly (BUTTON_POWER_ENABLE disabled)\n");
#endif

	/* Confirm boot success - reset boot_fail_cnt so the bootloader knows this
	 * partition booted OK, then start the upgrade engine (CDC + BLE OTA). */
	FwUpgrade_ConfirmBootSuccess();
	FwUpgrade_Init();
	DBG("[Main] Upgrade engine initialized\n");

	/* Main application loop: one Banux iteration per pass. */
	while (1) {
		Banux_Process();
	}
}

/*------------------------------------------------------------------------------
 * Early UART1 diagnostics - bypasses DBG/printf.
 * UART1 is already initialized by the bootloader / early main().
 *----------------------------------------------------------------------------*/
#define DIAG_UART1_STATUS  (*(volatile uint32_t *)0x40006014)
#define DIAG_UART1_TX      (*(volatile uint32_t *)0x40006018)
static inline void diag_putc(char c)
{
	while (!(DIAG_UART1_STATUS & (1u << 9))) ;
	DIAG_UART1_TX = (uint32_t)(unsigned char)c;
}

int main(void) {
#if HAS_BOOTLOADER
	diag_putc('M');  /* main() entered - confirms startup completed */
	/* When started by bootloader, Chip_Init, clock, UART, SPI flash, DMA and TCM
	 * are already configured. Re-initializing them can hang (PLL re-lock failure)
	 * or break the running UART. */
	WDG_Disable();
	diag_putc('1');  /* WDG_Disable done */

	/* Re-initialize UART driver software state: __c_init() cleared .bss and copied
	 * .data, wiping the UART driver runtime state set up by the bootloader. The
	 * hardware is fine - DbgUartInit just resets the software side. */
	DbgUartInit(1, 115200, 8, 0, 1);
	diag_putc('U');  /* DbgUartInit done */

	/* Re-init SPI flash and DMA driver state (same reason as UART above). */
	Remap_InitTcm(0, 12);
	SpiFlashInit(80000000, MODE_4BIT, 0, 1);
	DMA_ChannelAllocTableSet(DmaChannelMap);
	diag_putc('R');  /* Remap/SPI/DMA re-init done */

	/* Heartbeat: toggle GPIOA16 to confirm main() is reached even if UART is lost. */
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX16);
	diag_putc('2');  /* GPIO toggle done */

	DBG("[APP] main() entered (from bootloader)\n");
	diag_putc('3');  /* DBG done */
#else
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

	GPIO_PortAModeSet(GPIOA9, 1);				//Rx, A24:uart1_rxd_0
	GPIO_PortAModeSet(GPIOA10, 3);				//Tx, A25:uart1_txd_0
	DbgUartInit(1, 115200, 8, 0, 1);

	Clock_USBClkDivSet(4);
	Clock_USBClkSelect(APLL_CLK_MODE);

	Remap_DisableTcm();
	Remap_InitTcm(0x40000, TCM_SIZE);
	/* SpiFlashInit moved AFTER spi_init() - SPIM_Init can reset the XIP flash config */
	DMA_ChannelAllocTableSet(DmaChannelMap);
#endif

	/* Route Banux framework DBG output to UART as early as possible so that both
	 * banux2-style DBG (BanuxDebug_Printf) and later logs are visible. */
	BanuxDebug_SetWriter(App_LogWriter);

	/* Boot partition detection. With a bootloader the jump decision is already
	 * made; this just sets the internal tracking flag. */
	diag_putc('4');  /* before firmware upgrade boot init */
	FwUpgrade_BootInit();
	diag_putc('5');  /* firmware upgrade boot init done */

	GIE_ENABLE();
	diag_putc('6');  /* GIE_ENABLE done */

	Timer_Config(TIMER2, 1000, 0);
	Timer_Start(TIMER2);
	NVIC_EnableIRQ(Timer2_IRQn);
	diag_putc('7');  /* Timer2 done */

	DBG("****************************************************************\n");
	DBG("                       BanEffector SDK                          \n");
	DBG("                          APP " APP_VERSION_STR "                           \n");
	DBG("****************************************************************\n");

	prvInitialiseHeap();
	diag_putc('8');  /* heap init done */

	NVIC_EnableIRQ(SWI_IRQn);

	SarADC_Init();
	diag_putc('9');  /* SarADC done */
	xQueue = xQueueCreate(4, sizeof(uint32_t));

	/* Initialize SPI hardware BEFORE the driver framework (drivers need it).
	 *
	 * In the bootloader path, spi_init() -> SPIM_Init() reconfigures the shared
	 * SPI controller and BREAKS XIP flash access, so it is skipped (the SPI
	 * controller was already configured by SpiFlashInit above). In the standalone
	 * path SpiFlashInit() runs first (while XIP is still valid from SDK startup),
	 * then spi_init() configures the SPI master for peripherals. */
#if !HAS_BOOTLOADER
	DBG("[Main] Initializing SPI hardware...\n");
	SpiFlashInit(80000000, MODE_4BIT, 0, 1);
	spi_init();
	diag_putc('a');  /* spi_init done */
#else
	diag_putc('a');  /* skipped spi_init in bootloader path (XIP already configured) */
#endif

	/* Initialize the system state module (must be before MainTask / PowerOn).
	 * The Banux framework (driver framework + event system + shell) and the
	 * application power_on() sequence are initialized inside MainTask via
	 * Banux_Init(), which must run in RTOS context. */
	SysState_Init();

	xTaskCreate((TaskFunction_t)MainTask, "MainTask", 4096, NULL, 1, NULL);

	DBG("[Main] Starting FreeRTOS scheduler...\n");
	diag_putc('c');  /* about to start scheduler */
	vTaskStartScheduler();
	diag_putc('!');  /* should never reach here */

	while (1)
		;
}
