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

/* Version: increment on each release (format V<major>.<minor>.<patch>) */
#define APP_VERSION_MAJOR   0
#define APP_VERSION_MINOR   2
#define APP_VERSION_PATCH   1
#define APP_VERSION_STR     "V0.2.15"

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
/* Driver Framework */
#include "drv_device.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_stor.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "otg_device_audio.h"
#include "ctrlvars.h"
#include "product_def.h"
#if FAT32_EN
#include "fat32_nand.h"         /* NAND FAT32 file system */
#include "looper_wav_export.h"  /* Audio Looper WAV export */
#endif

#include "bg_audio_io_manager.h"

#include "battery_drv.h"
#include "battery_calib.h"

/* Physical BLE connection flag (defined in ble_app_callback.c) */
extern uint8_t BleConnectFlag;

#include "audio_looper.h"

/* System Parameter Storage */
#include "sys_param.h"   
#include "looper_wav_ble_export.h"  /* Audio Looper WAV BLE export */
#include "ble_protocol.h"           /* BLE protocol types */
#include "bg_shell.h"           /* Shell console API */
#include "audio_setting.h"


#include "drv_init.h"           /* Driver Framework Initialization */
#include "app_config.h"         /* HAS_BOOTLOADER */

/* 事件发布-订阅系统 */
#include "bg_event.h"

/* 系统状态管理（空闲/正常/数据传输） */
#include "sys_state.h"
#include "app_sys_handler.h"

/* Firmware upgrade component */
#include "banux/05_component/firmware_upgrade/fw_upgrade.h"

#if SYS_LED_EN
#include "sys_led.h"
#endif

/* 开机提示音模块 — 音频数据已内嵌到 remind_sound.c 的调用表中 */
#include "remind_sound.h"

extern void SysTickInit(void);
extern void UsbAudioMicDacInit(void);
extern void OTG_DeviceAudioInit();
extern bool SYNTH_StartupSequence(void);  /* Synthesizer startup initialization */

extern void UsbAudioTimer1msProcess(void);
//__attribute__((section(".driver.isr")))

uint8_t record_flag = 0;
uint16_t read_write = 0;
uint8_t play_flag = 0;
uint16_t rec = 0, rea = 0, play = 0;
uint8_t UI_flag = 0;
uint16_t time = 0;
uint16_t left_time = 0;
uint16_t right_time = 0;
uint8_t left_flag = false;
uint8_t right_flag = false;

/* BLE sync command timing */
static uint32_t ble_tick_counter = 0;

uint8_t power_flag = 0;
uint8_t count_flag = 0;

uint16_t power_count =0;
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
	//BG_page.Loop(&BG_page);
#ifdef CFG_APP_USB_AUDIO_MODE_EN
	UsbAudioTimer1msProcess(); //1ms閿熷彨鏂》鎷烽敓锟�
#endif

	/* Increment BLE tick counter for sync command timing */
	ble_tick_counter++;

#if SYS_LED_EN
	SysLed_Tick1ms();
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
/* SpimBuf_TX/RX 已删除 (RAM优化: 释放8KB，未使用) */

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

void spi_init(void) {
	SPIM_SetDmaEn(1);
	SPIM_IoConfig(SPIM_PORT0_A5_A6_A7);
	Clock_SPIMClkDivSet(1);
	DMA_ChannelAllocTableSet(DmaChannelMap);
	if (SPIM_Init(spimMode, spimRate)) {
		DBG("SPI init success!\n");
		DBG("spim mode:%d\n", spimMode); //
		DBG("spim rate:%d\n", spimRate); //
		DBG("spim_cs  :%s\n", spimIO[0][0]);
		DBG("spim_miso:%s\n", spimIO[0][1]);
		DBG("spim_clk :%s\n", spimIO[0][2]);
		DBG("spim_mosi:%s\n", spimIO[0][3]);
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

/**
 * @brief BLE data command dispatcher
 * Routes incoming BLE data frames to the correct module handler.
 */
static void ble_data_cmd_dispatch(const BleProtoFrame_t *frame)
{
	switch (frame->cmd) {
	case BLE_CMD_WAV_EXPORT:
		LooperWavBle_HandleCommand(frame->payload, frame->len);
		break;
	case BLE_CMD_BATTERY_CALIB:
		BattCalib_HandleBleCmd(frame->payload, frame->len);
		break;
	default:
		DBG("[BLE] Unhandled data cmd: 0x%02X\n", frame->cmd);
		break;
	}
}

void power_on()
{
	/* SysState: 推进运行态 OFF → BOOT → RUNNING
	 * 注意: SysState_PowerOn() 内部会执行已注册的 PowerOn 回调和 IO 初始化，
	 * 以下代码是 legacy 直接初始化，逐步迁移到回调注册模式 */
	SysState_PowerOn();

#if SYS_LED_EN
	SysLed_Init();
#else
	GPIO_RegOneBitClear(GPIO_A_IE, HW_LED_GPIO_PIN);
	GPIO_RegOneBitSet(GPIO_A_OE, HW_LED_GPIO_PIN);
	GPIO_RegOneBitSet(GPIO_A_OUT, HW_LED_GPIO_PIN);
#endif

	/* 初始化音频控制变量默认值（效果器参数、增益等）
	 * 必须在 SysParam_Init() 和 Audio_Init() 之前调用，
	 * 否则 gCtrlVars 全零导致所有效果器参数异常 → 无声 */
	CtrlVarsInit();

	/* SPI and Driver Framework already initialized in main() */
	DBG("[Task] Hardware drivers already initialized in main()\n");

	/*=====================================================
	 * System Parameter Initialization
	 * 浠庡唴閮‵lash鍔犺浇淇濆瓨鐨勫弬鏁板埌鍏ㄥ眬鍙橀噺
	 * 蹇呴』鍦ㄧ‖浠跺垵濮嬪寲鍚庛�鍔熻兘妯″潡鍒濆鍖栧墠璋冪敤
	 *====================================================*/
	DBG("[Task] Loading system parameters from flash...\n");
	{
		SysParam_Status_t param_status = SysParam_Init();
		if (param_status == SYSPARAM_OK) {
			DBG("[Task] Parameters loaded successfully from flash\n");
			/* Apply saved parameters to audio system (override CtrlVarsInit defaults) */
			SysParam_ApplyToAudio();
		} else {
			DBG("[Task] Using default parameters (status=%d)\n", param_status);
			/* First boot or flash corruption - defaults already loaded and saved */
			/* Call ApplyToAudio to ensure defaults are synced to gCtrlVars */
			SysParam_ApplyToAudio();
		}
	}


	BG_AudioManager.Audio_Init(44100);

#if FAT32_EN
	/*=====================================================
	 * NAND FAT32 文件系统初始化
	 * 用于 WAV 导出
	 *====================================================*/
	DBG("[Task] Initializing NAND FAT32 file system...\n");
	{
		BG_ERR ret = FAT32_NAND_Init();
		if (ret == SUCCESS) {
			DBG("[Task] NAND FAT32 initialized OK\n");
			/* 初始化 WAV 导出功能 */
			ret = LooperWAV_Init();
			if (ret == SUCCESS) {
				DBG("[Task] WAV export initialized (free: %u KB)\n",
					LooperWAV_GetFreeSpace() / 1024);
			} else {
				DBG("[Task] WAV export init failed: %d\n", ret);
			}
		} else {
			DBG("[Task] NAND FAT32 init failed: %d (may need format)\n", ret);
		}
	}
#endif

	/*=====================================================
=======
	 * BLE application layer — sync provider + data handler
	 * 注册 05_component/ble_app 层的同步提供者和数据命令处理器，
	 * 解耦 02_device_drivers 与 05_component 的直接依赖。
	 *====================================================*/
	{
		/* 先注册回调（BleApp_Init），再初始化协议（BleProto_Init），
		 * 确保 BleProto_Init 中的 g_send_init_handler 回调已就绪 */
		extern void BleApp_Init(void);
		BleApp_Init();
		extern void BleProto_Init(void);
		BleProto_Init();
		DBG("[Task] BLE app layer initialized (sync + data handler)\n");
	}

	/*=====================================================
	 * Battery calibration — load saved curve from flash
	 *====================================================*/
	BattCalib_Init();
	/* Register LowPower_FeedActivity callback for battery calibration (02→06 decoupling) */
	{
		extern void BattCalib_RegisterFeedActivity(void (*feed_fn)(uint8_t));
		extern void LowPower_FeedActivity(uint8_t mask);
		BattCalib_RegisterFeedActivity(LowPower_FeedActivity);
	}
	DBG("[Task] Battery calibration initialized\n");

	/*=====================================================
	 * System state machine — already initialized in main()
	 *====================================================*/


	DBG("[Main] System initialized successfully\n");
	DBG("[Main] Entering main loop...\n");

	/* 开机提示音在 BG_audio_Init() 末尾（所有模块初始化完毕后）播放 */

}

void power_off()
{
	/* SysState: 推进运行态 RUNNING/IDLE → SHUTDOWN → OFF
	 * 注意: SysState_PowerOff() 内部会执行已注册的 PowerOff 回调，
	 * 以下代码是 legacy 直接清理，逐步迁移到回调注册模式 */
	SysState_PowerOff();

	/* 释放混响内存，为关机提示音腾出 ~57KB 堆空间 */
	BG_AudioIO_PrepareForShutdown();
	/* 关机前保存已修改的参数到 Flash，避免用户设置丢失 */
	if (SysParam_IsModified()) {
		DBG("[PowerOff] Saving modified parameters to flash...\n");
		SysParam_Save();
	}
	/* 关机提示音（非阻塞，通过 Effect Graph 混音输出） */
	RemindSound_Start("off");
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX24);
#if SYS_LED_EN
	SysLed_SetMode(SYS_LED_MODE_OFF);
#else
	GPIO_RegOneBitClear(GPIO_A_OUT, HW_LED_GPIO_PIN);
#endif
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
			Reset_McuSystem ();

		}

	}else{
		valid_press = 1;
		count_flag = 0;
	}


}


uint8_t time_count = 0;

void hardware_check()
{
	time_count++;
	if(time_count>=100){
		if(ADC_SingleModeDataGet(ADC_CHANNEL_POWERKEY)>4000){
			 AudioSetting_SetGuitar2VolumePercent(AudioSetting_GetGuitar2VolumePercent()) ;
		}else{
			 AudioSetting_SetGuitar2VolumePercent(0) ;
		}
		time_count = 0;
	}
	/* Battery calibration voltage tick (每次 hardware_check 调用约 50ms) */
	BattCalib_Tick();

	/* 系统状态机更新（检测空闲/传输状态，发布事件到订阅者） */
	SysState_Update();

	/* 应用层 50ms tick (LED 闪烁时序, BLE 电量上报) */
	AppSys_LedTick();
	AppSys_BatteryTick();
}

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


#if BUTTON_POWER_ENABLE
	pwr_button_init();
	/* 按钮开机模式：长按按钮1秒才能开机 */
	while (!power_flag)
	{
		pwr_butoon_handler();
	}
#else
	/* 直接上电开机模式：无需按钮 */
	power_on();
	DBG("Power ON directly (BUTTON_POWER_ENABLE disabled)\n");
#endif

	/* Confirm boot success — reset boot_fail_cnt in partition flags.
	 * This tells the bootloader that the current partition booted OK. */
	FwUpgrade_ConfirmBootSuccess();

	/* Initialize firmware upgrade engine (CDC + BLE OTA) */
	FwUpgrade_Init();
	DBG("[Main] Upgrade engine initialized\n");

	while (1) {

		//pwr_butoon_handler();
		/* Check and send delayed BLE sync responses.
		 * Skip during CDC upgrade mode — BLE API may use USB/BT state
		 * that is unsafe to touch while CDC data path is active. */
		extern void BLE_CheckSyncResponse(void);
		BLE_CheckSyncResponse();

		extern void BleProto_Process(void);
		BleProto_Process();

		/* WAV BLE export: send one data packet per tick */
		LooperWavBle_ProcessTick();

		/* CDC firmware upgrade mode — auto-detect SOF or process packets */
		if (!FwUpgrade_InCdcMode()) {
			FwUpgrade_CheckCdcEnter();  /* 嗅探 0xAA SOF，自动进入升级模式 */
		}
		if (FwUpgrade_InCdcMode()) {
			FwUpgrade_ProcessCdc();
			continue;  /* Skip Audio/Shell during upgrade */
		}

		BG_AudioManager.Audio_Loop();

		/* Update UI System (handles button input, menu, status bar) */
		if(UI_flag == 1){
			UI_flag = 0;

			/* Update hardware status (Bluetooth, etc.) */
			hardware_check();
		}
	}
}

void FlashNewDriverTask(void)
{
	spi_init();
	DBG("\n");
	DBG("**************************************************\n");
	DBG("*     New Flash Driver Architecture Test        *\n");
	DBG("**************************************************\n");

#if FLASH_TEST_EN
	FlashNewDriver_Test();
	DBG("\nNew Flash Driver test completed.\n");
	DBG("You can also run FlashNewDriver_QuickTest() for quick debug.\n");
#else
	DBG("FlashNewDriverTask: FLASH_TEST_EN=0, test disabled\n");
#endif

}

/* BLE timing functions for sync command buffering */
uint32_t BLE_GetTick(void) {
    return ble_tick_counter;
}

uint8_t BLE_IsDelayElapsed(uint32_t start_tick, uint32_t delay_ms) {
    uint32_t current_tick = ble_tick_counter;
    uint32_t elapsed_ticks = current_tick - start_tick;
    /* Assuming Timer2Interrupt runs at 1ms intervals, so ticks = ms */
    return (elapsed_ticks >= delay_ms);
}

void prvInitialiseHeap(void);

/* Direct UART1 write for early diagnostics — bypasses DBG/printf.
 * UART1 is already initialized by the bootloader. */
#define DIAG_UART1_STATUS  (*(volatile uint32_t *)0x40006014)
#define DIAG_UART1_TX      (*(volatile uint32_t *)0x40006018)
static inline void diag_putc(char c)
{
	while (!(DIAG_UART1_STATUS & (1u << 9))) ;
	DIAG_UART1_TX = (uint32_t)(unsigned char)c;
}

int main(void) {
#if HAS_BOOTLOADER
	diag_putc('M');  /* main() entered — confirms startup completed */
	/* When started by bootloader, Chip_Init, clock, UART, SPI flash, DMA,
	 * and TCM are already configured.  Re-initializing them can hang
	 * (e.g. PLL re-lock failure) or break the running UART.              */
	WDG_Disable();
	diag_putc('1');  /* WDG_Disable done */

	/* Re-initialize UART driver software state.
	 * __c_init() just cleared .bss and copied .data from flash, wiping out
	 * the UART driver's runtime state that the bootloader had set up.
	 * Without re-init, DBG/printf hangs on the first call because the
	 * driver's internal variables (ring buffer pointers, init flags, etc.)
	 * are reset to zero.  The hardware is fine — DbgUartInit just resets
	 * the software side.                                                    */
	DbgUartInit(1, 115200, 8, 0, 1);
	diag_putc('U');  /* DbgUartInit done */

	/* Re-init SPI flash and DMA driver state (same reason as UART above).
	 * The bootloader configured the hardware, but __c_init() wiped the
	 * driver's software state in .bss/.data.                               */
	Remap_InitTcm(0, 12);
	SpiFlashInit(80000000, MODE_4BIT, 0, 1);
	DMA_ChannelAllocTableSet(DmaChannelMap);
	diag_putc('R');  /* Remap/SPI/DMA re-init done */

	/* Quick heartbeat: toggle GPIOA16 so we can confirm main() is reached
	 * even if UART output is lost.                                         */
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
	/* SpiFlashInit moved AFTER spi_init() — SPIM_Init can reset the XIP flash config */
	DMA_ChannelAllocTableSet(DmaChannelMap);
#endif

	/* Boot partition detection — detect which partition we're running on.
	 * With bootloader, the jump decision is already made by bootloader.
	 * This just sets the internal tracking flag. */
	diag_putc('4');  /* before firmware upgrade boot init */
	FwUpgrade_BootInit();
	diag_putc('5');  /* firmware upgrade boot init done */

	GIE_ENABLE();
	diag_putc('6');  /* GIE_ENABLE done */
//	SysTickInit();
	Timer_Config(TIMER2, 1000, 0);
	Timer_Start(TIMER2);
	NVIC_EnableIRQ(Timer2_IRQn);
	diag_putc('7');  /* Timer2 done */

	DBG("****************************************************************\n");
	DBG("                          BG_CARD SDK                           \n");
	DBG("                          APP " APP_VERSION_STR "                           \n");
	DBG("****************************************************************\n");

	prvInitialiseHeap();
	diag_putc('8');  /* heap init done */

	NVIC_EnableIRQ(SWI_IRQn);

	SarADC_Init();
	diag_putc('9');  /* SarADC done */
	xQueue = xQueueCreate(4, sizeof(uint32_t));

	/* Initialize SPI hardware BEFORE driver framework (drivers need it).
	 *
	 * IMPORTANT: In the bootloader path, spi_init() → SPIM_Init() reconfigures
	 * the shared SPI controller and BREAKS XIP Flash access.  The following
	 * SpiFlashInit() call is meant to restore XIP, but SpiFlashInit() itself
	 * runs from XIP Flash — so the CPU faults before it can restore anything.
	 *
	 * In the bootloader path the SPI controller was already configured by
	 * SpiFlashInit() at line 737 above; spi_init() is NOT needed here.
	 * Drivers (flash_driver.c etc.) call SPIM_DMA_* directly and work
	 * correctly with the XIP configuration.
	 *
	 * In the non-bootloader path, SpiFlashInit() runs FIRST, then spi_init()
	 * is called to additionally configure the SPI master for peripherals
	 * (LCD, external NOR/NAND, etc.).  This ordering is safe because
	 * SpiFlashInit() runs while XIP is still valid (from the SDK startup). */
#if !HAS_BOOTLOADER
	DBG("[Main] Initializing SPI hardware...\n");
	SpiFlashInit(80000000, MODE_4BIT, 0, 1);
	spi_init();
	diag_putc('a');  /* spi_init done */
#else
	diag_putc('a');  /* skipped spi_init in bootloader path (XIP already configured) */
#endif

	/* Initialize Driver Framework BEFORE RTOS */
	DBG("[Main] Initializing Driver Framework (before RTOS)...\n");
	DrvFramework_FullInit();
	diag_putc('b');  /* DrvFramework done */

	/* EffectGraph VFS initialization (moved from drv_init.c to decouple 03→05) */
#if EFFECT_GRAPHICS_EN
	{
		extern int EffectGraphVfs_MountDefault(void);
		int eg_ret = EffectGraphVfs_MountDefault();
		DBG("[Main] Audio Graph VFS %s\n", eg_ret == 0 ? "mounted OK" : "deferred");
#if USE_EFFECT_GRAPH_VFS
		extern void ShellCmdAudioVfs_Register(void);
		ShellCmdAudioVfs_Register();
#endif
	}
#endif


	
	//xTaskCreate( (TaskFunction_t)FlashNewDriverTask, "FlashNewDriverTask", 1024, NULL, 1, NULL );
	// xTaskCreate( (TaskFunction_t)InternalFlashTestTask, "InternalFlashTest", 1024, NULL, 1, NULL );

	/* Initialize system state module (must be before MainTask / PowerOn) */
	SysState_Init();

	/* Initialize event publish-subscribe system (auto-registers all BG_EVT_SUB) */
	BG_Event_Init();

	xTaskCreate((TaskFunction_t )MainTask, "MainTask", 4096, NULL, 1, NULL);


	DBG("[Main] Starting FreeRTOS scheduler...\n");
	diag_putc('c');  /* about to start scheduler */
	vTaskStartScheduler();
	diag_putc('!');  /* should never reach here */

	while (1)
		;
}


