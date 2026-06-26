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
#include "bg_lcd.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
/* Driver Framework */
#include "drv_device.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_stor.h"
#include "gui_tool.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "otg_device_audio.h"
#include "ctrlvars.h"
#include "product_def.h"
#include "flash_boot.h"
#if CDC_FILE_MANAGER_EN
#include "cdc_file_manager.h"  /* USB CDC NAND Flash download */
#endif
#if FAT32_EN
#include "fat32_nand.h"         /* NAND FAT32 file system */
#include "looper_wav_export.h"  /* Audio Looper WAV export */
#endif
/* Page Manager - Now in BanGUI core (via bangui.h) */
/* #include "page_manager.h" - Removed, use bangui.h */

#include "bg_audio_io_manager.h"

#include "battery_drv.h"
#include "battery_calib.h"

/* Physical BLE connection flag (defined in ble_app_callback.c) */
extern uint8_t BleConnectFlag;

#include "framebuffer.h"
#include "audio_looper.h"

/* System Parameter Storage */
#include "sys_param.h"   
#include "shell_lcd_adapter.h"  /* Shell LCD console adapter */
#include "looper_wav_ble_export.h"  /* Audio Looper WAV BLE export */
#include "ble_protocol.h"           /* BLE protocol types */
#include "bg_shell.h"           /* Shell console API */
#include "audio_setting.h"
/* New UI Architecture - Single entry point */
#include "bangui.h"             /* BanGUI unified UI system (includes page manager) */


#include "drv_init.h"           /* Driver Framework Initialization */
#include "app_config.h"         /* HAS_BOOTLOADER, FLASH_BOOT_EN */

/* 事件发布-订阅系统 */
#include "bg_event.h"

/* 系统状态管理（空闲/正常/数据传输） */
#include "sys_state.h"

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
/* Boot partition detection and confirmation */
#include "dual_partition.h"

/* Firmware upgrade engine */
#include "app_upgrade.h"
#include "cdc_upgrade.h"

>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
/* 开机提示音模块 — 音频数据已内嵌到 remind_sound.c 的调用表中 */
#include "remind_sound.h"

/* BanGTsynth MIDI 合成器模块 */
#if BANGTSYNTH_EN
#include "bangtsynth_node.h"
#include "bg_storage.h"
#include "soundbank_manager.h"
#include "bg_osal.h"              /* bg_tick_increment() */
#if SYNTH_SD_NAND_PSRAM_EN
#include "synth_sdnandpsram.h"    /* SYNTH_LoadTick() */
#endif
#endif

//#define UI_EN

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
uint16_t time = 0;
uint16_t left_time = 0;
uint16_t right_time = 0;
uint8_t left_flag = false;
uint8_t right_flag = false;

/* BG_page 鐜板湪鍦�app_pages.c 涓畾涔夛紝閫氳繃 bangui.h 寮曞叆 */
/* extern BG_Page BG_page; -- 鐢�app_pages.h 澹版槑 */
uint8_t UI_count = 0, UI_flag = 0;

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

#if BANGTSYNTH_EN
	/* 驱动合成器 HAL 毫秒计数器（独立于 FreeRTOS 调度器） */
	bg_tick_increment();
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

	// GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX20);
	// GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX24);

	/* LED 指示灯初始化: GPIO_A16 输出，默认常亮 */
	GPIO_RegOneBitClear(GPIO_A_IE, HW_LED_GPIO_PIN);
	GPIO_RegOneBitSet(GPIO_A_OE, HW_LED_GPIO_PIN);
	GPIO_RegOneBitSet(GPIO_A_OUT, HW_LED_GPIO_PIN);

	CtrlVarsInit();

	/* SPI and Driver Framework already initialized in main() */
	/* All hardware drivers (LCD, Flash, etc.) auto-initialized by framework */
	DBG("[Task] Hardware drivers already initialized in main()\n");

#ifdef  UI_EN
	BG_lcd.Init();
#endif
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


#if BANGTSYNTH_EN
	DBG("[Task] Running synthesizer startup sequence...\n");
	SYNTH_StartupSequence();
	/*=====================================================
	 * BanGTsynth MIDI 合成器初始化
	 * 初始化 MIDI 控制器、音频处理流水线
	 * 必须在 Audio_Init 之后调用 (Effect Graph 已创建)
	 *====================================================*/
	DBG("[Task] Initializing BanGTsynth...\n");
	{
		extern int osPortRemainMem(void);
		DBG("[Task] Heap before BanGTsynth: %d bytes\n", osPortRemainMem());
	}

	if (BanGTsynth_Node_Init() == 0) {
		DBG("[Task] BanGTsynth initialized OK\n");
		/* 设置内嵌 SF2 存储驱动并加载默认音源 */
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
	BG_AudioManager.Audio_Init(44100);

#if FAT32_EN
	/*=====================================================
	 * NAND FAT32 文件系统初始化
	 * 用于 WAV 导出和 CDC 文件管理器
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
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
>>>>>>> Stashed changes
	 * BLE data command dispatcher
	 * Route incoming BLE data frames to the correct handler
	 *====================================================*/
	{
		extern void BleProto_Init(void);
		BleProto_Init();
		extern void BleProto_RegisterDataHandler(BleProto_DataHandler_t handler);
		BleProto_RegisterDataHandler(ble_data_cmd_dispatch);
		DBG("[Task] BLE data handler registered\n");
<<<<<<< Updated upstream
=======
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
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
	}

	/*=====================================================
	 * Battery calibration — load saved curve from flash
	 *====================================================*/
	BattCalib_Init();
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
	/* Register LowPower_FeedActivity callback for battery calibration (02→06 decoupling) */
	{
		extern void BattCalib_RegisterFeedActivity(void (*feed_fn)(uint8_t));
		extern void LowPower_FeedActivity(uint8_t mask);
		BattCalib_RegisterFeedActivity(LowPower_FeedActivity);
	}
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
	DBG("[Task] Battery calibration initialized\n");

	/*=====================================================
	 * System state machine (IDLE / NORMAL / TRANSFER)
	 *====================================================*/
	SysState_Init();
	DBG("[Task] SysState initialized\n");

#ifdef UI_EN

	BANGUI_QUICK_INIT();


	/* 璁剧疆 Home 瑙嗗浘鍥炬爣鍥炶皟 */
	View_Home_SetIconCallback(HOME_ICON_LOOPER, NULL);  /* TODO: 缁戝畾 Looper 瑙嗗浘 */

	/*=====================================================
	 * BOOT SPLASH SCREEN - 寮�満鐣岄潰
	 * 鍚姩 UI 绯荤粺锛屼粠寮�満鐣岄潰寮�锛圠ogo + 杩涘害鏉★級
	 * 鍔ㄧ敾鐢�view_boot.c 鐨勭姸鎬佹満椹卞姩锛屾棤纭欢杩�	 * 鍔ㄧ敾瀹屾垚鍚庤嚜鍔ㄥ垏鎹㈠埌 Home 妗岄潰
	 *====================================================*/
	BANGUI_START(UI_STATE_BOOT);


	DBG("[Main] System initialized successfully\n");
	DBG("[Main] Starting from Boot Screen...\n");

	/* Initialize Shell LCD console adapter */

	ShellLCD_Adapter_Init();
#endif


	DBG("[Main] Entering main loop...\n");

	/* 开机提示音在 BG_audio_Init() 末尾（所有模块初始化完毕后）播放 */

}

void power_off()
{
	/* 释放混响内存，为关机提示音腾出 ~57KB 堆空间 */
	BG_AudioIO_PrepareForShutdown();
	/* 关机前保存已修改的参数到 Flash，避免用户设置丢失 */
	if (SysParam_IsModified()) {
		DBG("[PowerOff] Saving modified parameters to flash...\n");
		SysParam_Save();
	}
	/* 关机提示音 */
	RemindSound_PlayByName("off");
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX24);
	/* 关机熄灭LED */
	GPIO_RegOneBitClear(GPIO_A_OUT, HW_LED_GPIO_PIN);
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
/* hardware_check() 被 50ms 定时调用，600次 = 30秒 */
static uint16_t battery_report_count = 0;
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======

/* LED 指示灯状态 */
static uint16_t led_blink_count = 0;   /* 闪烁计数器 */
static uint8_t  led_last_state = 0;    /* 上次LED状态，用于变化检测 */

/**
 * @brief LED 指示灯更新（在 hardware_check 中每 50ms 调用）
 *
 * 逻辑：
 *   - 充电完成（USB连接 + SOC>=100%）→ 熄灭
 *   - 电量 < 15% 且未充电 → 闪烁（500ms亮 / 500ms灭）
 *   - 其他（正常状态）→ 常亮
 */
static void led_update(void)
{
	uint8_t soc = BattCalib_GetSOC();
	bool usb_connected = OTG_PortDeviceIsLink();
	uint8_t desired;  /* 0=灭, 1=亮, 2=闪烁 */

	/* 充电完成：USB连接且电量满 → 熄灭 */
	if (usb_connected && soc >= 100) {
		desired = 0;
	}
	/* 低电量闪烁（仅未充电时，充电中低电量也闪烁提醒） */
	else if (soc < 15) {
		desired = 2;
	}
	/* 正常状态：常亮 */
	else {
		desired = 1;
	}

	/* 执行LED控制 */
	if (desired == 2) {
		/* 闪烁模式：500ms亮 / 500ms灭 = 10次50ms */
		led_blink_count++;
		if (led_blink_count >= 10) {
			led_blink_count = 0;
			led_last_state = !led_last_state;
		}
		if (led_last_state) {
			GPIO_RegOneBitSet(GPIO_A_OUT, HW_LED_GPIO_PIN);
		} else {
			GPIO_RegOneBitClear(GPIO_A_OUT, HW_LED_GPIO_PIN);
		}
	} else {
		led_blink_count = 0;
		led_last_state = desired;
		if (desired) {
			GPIO_RegOneBitSet(GPIO_A_OUT, HW_LED_GPIO_PIN);
		} else {
			GPIO_RegOneBitClear(GPIO_A_OUT, HW_LED_GPIO_PIN);
		}
	}
}

>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
void hardware_check()
{

	UI_StatusBar_SetBTStatus(GetA2dpState());
	time_count++;
	if(time_count>=100){
		if(ADC_SingleModeDataGet(ADC_CHANNEL_POWERKEY)>4000){
			 AudioSetting_SetGuitar2VolumePercent(AudioSetting_GetGuitar2VolumePercent()) ;
		}else{
			 AudioSetting_SetGuitar2VolumePercent(0) ;
		}
		time_count = 0;
	}

	/* 每 30 秒通过 BLE 上报一次电量（仅在已连接时发送）*/
	battery_report_count++;
	if (battery_report_count >= 600) {
		battery_report_count = 0;
		if (BleConnectFlag) {
			uint8_t payload[2];
			payload[0] = BLE_SYSTEM_SUB_BATTERY;
			payload[1] = BattCalib_GetSOC();  /* 使用矫正曲线 SOC（无矫正则回退到电压法） */
			BleProto_SendOnce(BLE_CMD_SYSTEM, payload, 2);
		}
	}

	/* Battery calibration voltage tick (每次 hardware_check 调用约 50ms) */
	BattCalib_Tick();

	/* 系统状态机更新（检测空闲/传输状态，BLE 上报到 App） */
	SysState_Update();
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======

	/* LED 指示灯状态更新 */
	led_update();
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}

void MainTask() {




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
	Boot_ConfirmSuccess();

	/* Initialize firmware upgrade engine (CDC + BLE OTA) */
	App_Upgrade_Init();
	CDC_Upgrade_Init();
	DBG("[Main] Upgrade engine initialized\n");

#if CDC_FILE_MANAGER_EN
	/* Step 10: Initialise CDC file manager for NAND Flash download. */
	CDC_FileManager_Init();
#endif

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

#if CDC_FILE_MANAGER_EN
		/* CDC 文件管理模式检测和处理 (NAND Flash 下载) */
		if (!CDC_FileManager_InMode()) {
			CDC_FileManager_CheckEnter();  /* 检测上位机 ENTER_NAND 命令 */
		}
		if (CDC_FileManager_InMode()) {
			CDC_FileManager_Process();      /* 处理文件下载命令 */
			continue;                        /* 跳过 Audio/Shell 循环 */
		}
#endif /* CDC_FILE_MANAGER_EN */

		/* CDC firmware upgrade mode — process upgrade packets */
		if (CDC_Upgrade_InMode()) {
			CDC_Upgrade_Process();
			continue;  /* Skip Audio/Shell during upgrade */
		}

		BG_AudioManager.Audio_Loop();

#if BANGTSYNTH_EN && SYNTH_SD_NAND_PSRAM_EN
		/* 驱动 Program Change 预热状态机（每主循环一步，无阻塞） */
		SYNTH_LoadTick();
#endif

		/* Update UI System (handles button input, menu, status bar) */
		if(UI_flag == 1){
			UI_flag = 0;

			/* Update hardware status (Bluetooth, etc.) */
			hardware_check();

			/* Scan ADC/DAC insert detection pins and update statusbar data */
			UI_StatusBar_ScanDetect();

			/* If Shell console enabled, only update console display */
#ifdef UI_EN
			if (Shell_ConsoleIsEnabled()) {
				Shell_ConsoleUpdate();
			} else {
				/* Update BanGUI system (handles views and input) */
				BG_UI.Update(20);

				/* Update statusbar (incremental update for changed items) */
				UI_StatusBar_Update();

			}
#endif
#ifdef USE_FRAME_BUFFER
			BG_lcd.FlushFrameBuffer();
#endif
		}
	}
}

void FlashNewDriverTask(void)
{
	spi_init();
	BG_lcd.Init();
	BG_lcd.Clear(BLUE);  // 钃濊壊琛ㄧず浣跨敤鏂伴┍鍔�
	DBG("\n");
	DBG("**************************************************\n");
	DBG("*     New Flash Driver Architecture Test        *\n");
	DBG("**************************************************\n");

	// 杩愯瀹屾暣娴嬭瘯
	FlashNewDriver_Test();

	// 娴嬭瘯瀹屾垚锛屾樉绀虹粨鏋滈鑹插凡鍦ㄦ祴璇曞嚱鏁颁腑璁剧疆
	DBG("\nNew Flash Driver test completed.\n");
	DBG("You can also run FlashNewDriver_QuickTest() for quick debug.\n");
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

	Remap_InitTcm(0, 12);
	SpiFlashInit(80000000, MODE_4BIT, 0, 1);
	DMA_ChannelAllocTableSet(DmaChannelMap);
#endif

	/* Boot partition detection — detect which partition we're running on.
	 * With bootloader, the jump decision is already made by bootloader.
	 * This just sets the internal tracking flag. */
	diag_putc('4');  /* before Boot_CheckAndJump */
	Boot_CheckAndJump();
	diag_putc('5');  /* Boot_CheckAndJump done */

#if FLASH_BOOT_EN
	report_up_grate();
#endif

	GIE_ENABLE();
	diag_putc('6');  /* GIE_ENABLE done */
//	SysTickInit();
	Timer_Config(TIMER2, 1000, 0);
	Timer_Start(TIMER2);
	NVIC_EnableIRQ(Timer2_IRQn);
	diag_putc('7');  /* Timer2 done */

	DBG("****************************************************************\n");
	DBG("                          BG_CARD SDK                           \n");
	DBG("****************************************************************\n");

	prvInitialiseHeap();
	diag_putc('8');  /* heap init done */

	NVIC_EnableIRQ(SWI_IRQn);

	SarADC_Init();
	diag_putc('9');  /* SarADC done */
	xQueue = xQueueCreate(4, sizeof(uint32_t));

	/* Initialize SPI hardware BEFORE driver framework (drivers need it) */
	DBG("[Main] Initializing SPI hardware...\n");
	spi_init();
	diag_putc('a');  /* spi_init done */

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


	xTaskCreate((TaskFunction_t )MainTask, "MainTask", 4096, NULL, 1, NULL);


	DBG("[Main] Starting FreeRTOS scheduler...\n");
	diag_putc('c');  /* about to start scheduler */
	vTaskStartScheduler();
	diag_putc('!');  /* should never reach here */

	while (1)
		;
}


