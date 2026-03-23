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
/* Page Manager - Now in BanGUI core (via bangui.h) */
/* #include "page_manager.h" - Removed, use bangui.h */

#include "bg_audio_io_manager.h"

#include "framebuffer.h"
#include "audio_looper.h"

/* System Parameter Storage */
#include "sys_param.h"
#include "shell_lcd_adapter.h"  /* Shell LCD console adapter */
#include "bg_shell.h"           /* Shell console API */
#include "audio_setting.h"
/* New UI Architecture - Single entry point */
#include "bangui.h"             /* BanGUI unified UI system (includes page manager) */


#include "drv_init.h"           /* Driver Framework Initialization */

/* BanGTsynth MIDI 合成器模块 */
#ifdef BANGTSYNTH_EN
#include "bangtsynth_node.h"
#include "bg_storage.h"
#include "soundbank_manager.h"
#endif

//#define UI_EN

extern void SysTickInit(void);
extern void UsbAudioMicDacInit(void);
extern void OTG_DeviceAudioInit();

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

uint8_t spimRate = SPIM_CLK_DIV_24M;
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
		255, //PERIPHERAL_ID_SDIO_RX,			//3
		255, //PERIPHERAL_ID_SDIO_TX,			//4
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

void power_on()
{

	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX24);
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

	DBG("[Task] Initializing Audio Manager...\n");
	/* Initialize Audio Manager (includes AudioLooper which needs Flash) */
	BG_AudioManager.Audio_Init(44100);

#ifdef BANGTSYNTH_EN
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

	DBG("[Task] Initializing UI System...\n");


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
#ifdef UI_EN
	ShellLCD_Adapter_Init();
#endif


	DBG("[Main] Entering main loop...\n");
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX24);

const char *cmd = "sb -t 60 20 4000\r";
	Shell_InputData((uint8_t *)cmd, strlen(cmd));

}

void power_off()
{
	const char *cmd = "sb -t 59 20 2000\r";
	Shell_InputData((uint8_t *)cmd, strlen(cmd));
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIO_INDEX24);
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
/*============================================================================
 * Looper 4-Button Control
 * Button mapping:
 *   GPIO_A0  -> Segment 0 (鎸夐敭鎸変笅涓轰綆鐢靛钩)
 *   GPIO_B5  -> Segment 1
 *   GPIO_A15 -> Segment 2
 *   GPIO_A16 -> Segment 3 Sending
 *===========================================================================*/

/* 鎸夐敭鐘舵�璁板綍锛堢敤浜庤竟娌挎娴嬶級 */
static uint8_t looper_btn_last_state[4] = {1, 1, 1, 1};  /* 涓婃媺锛岄粯璁ら珮鐢靛钩 */
static uint8_t looper_btn_debounce[4] = {0, 0, 0, 0};

void Looper_ProcessButtons(void)
{
	uint8_t btn_current[4];
	uint8_t i;

	/* 璇诲彇4涓寜閿綋鍓嶇姸鎬侊紙浣庣數骞虫湁鏁堬級 */
	btn_current[0] = GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX0) ? 1 : 0;
	btn_current[1] = GPIO_RegOneBitGet(GPIO_B_IN, GPIO_INDEX5) ? 1 : 0;
	btn_current[2] = GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX15) ? 1 : 0;
	btn_current[3] = GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX16) ? 1 : 0;

	/* 澶勭悊姣忎釜鎸夐敭 */
	for (i = 0; i < 4; i++)
	{
		if (btn_current[i] == 0 && looper_btn_last_state[i] == 1)
		{
			/* 涓嬮檷娌挎娴�- 鎸夐敭鎸変笅 */
			looper_btn_debounce[i]++;
			if (looper_btn_debounce[i] >= 3)  /* 绠�崟鍘绘姈 */
			{
				/* 璋冪敤Looper娈垫寜閿鐞�*/
				AudioLooper.SegmentButtonPress(i);
				looper_btn_debounce[i] = 0;
				looper_btn_last_state[i] = 0;
			}
		}
		else if (btn_current[i] == 1)
		{
			/* 鎸夐敭閲婃斁 */
			looper_btn_last_state[i] = 1;
			looper_btn_debounce[i] = 0;
		}
	}
}


uint8_t time_count = 0;
void hardware_check()
{

	UI_StatusBar_SetBTStatus(GetA2dpState());
	time_count++;
	if(time_count>=100){
		if(ADC_SingleModeDataGet(ADC_CHANNEL_POWERKEY)>4000){
			 AudioSetting_SetGuitar2VolumePercent(AudioSetting_GetGuitar2VolumePercent) ;
		}else{
			 AudioSetting_SetGuitar2VolumePercent(0) ;
		}
		time_count = 0;
	}
}

void EffectTask() {


	pwr_button_init();

	while (!power_flag)
	{
		pwr_butoon_handler();
	}
	while (1) {

		pwr_butoon_handler();
		/* Check and send delayed BLE sync responses */
		extern void BLE_CheckSyncResponse(void);
		BLE_CheckSyncResponse();

		BG_AudioManager.Audio_Loop();

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
void FlashTask(void)
{
	spi_init();
	BG_lcd.Init();
	BG_lcd.Clear(RED);
	// 鍒濆鍖栭棯瀛樼鐞嗗櫒
	BG_flash_manager.Init();

	// 璇诲彇涓や釜NOR Flash鐨処D
	uint8_t manufacturerID, memoryType, deviceID;

	DBG("========== Dual NOR Flash Test ==========\n");

	// 璇诲彇绗竴涓狽OR Flash ID (CS = A21)
	BG_flash_manager.ReadID(&manufacturerID, &memoryType, &deviceID, DEV_NOR1);
	DBG("NOR1 (CS=A21) ID: 0x%02X 0x%02X 0x%02X\n", manufacturerID, memoryType, deviceID);

	// 璇诲彇绗簩涓狽OR Flash ID (CS = A22)
	BG_flash_manager.ReadID(&manufacturerID, &memoryType, &deviceID, DEV_NOR2);
	DBG("NOR2 (CS=A22) ID: 0x%02X 0x%02X 0x%02X\n", manufacturerID, memoryType, deviceID);

	// ================== Flash璇诲啓娴嬭瘯 ==================
	DBG("========== Flash Sector Test Start ==========\n");

	// 娴嬭瘯鏁版嵁缂撳啿鍖�
	uint8_t write_buffer[256];
	uint8_t read_buffer[256];
	uint32_t test_address = 0x1000; // 浣跨敤绗簩涓�K鎵囧尯锛岄伩寮�湴鍧�
	uint16_t test_size = 256;		// 娴嬭瘯鏁版嵁澶у皬
	uint16_t i;
	bool test_passed = true;

	// 1. 鍑嗗娴嬭瘯鏁版嵁
	DBG("Preparing test data...\n");
	for (i = 0; i < test_size; i++)
	{
		write_buffer[i] = (uint8_t)(0xA0 + (i & 0x0F)); // 妯″紡锛欰0,A1,A2...AF,A0,A1...
	}

	// 鎵撳嵃鍐欏叆鏁版嵁锛堝墠32瀛楄妭锛�	DBG("Write data (first 32 bytes):\n");
	for (i = 0; i < 32 && i < test_size; i++)
	{
		if (i % 16 == 0)
			DBG("\n0x%04X: ", i);
		DBG("%02X ", write_buffer[i]);
	}
	DBG("\n");

	// ========== 娴嬭瘯NOR1 (CS=A21) ==========
	DBG("\n=== Testing NOR1 (CS=A21) ===\n");

	// 鍗曞瓧鑺傛祴璇�
	uint8_t test_byte = 0xAA;
	uint8_t read_byte;
	BG_flash_manager.SectorErase(test_address, DEV_NOR1);
	BG_flash_manager.PageProgram(test_address, &test_byte, 1, DEV_NOR1);
	BG_flash_manager.ReadData(test_address, &read_byte, 1, DEV_NOR1);
	DBG("NOR1 single byte: wrote 0x%02X, read 0x%02X %s\n",
	    test_byte, read_byte, (test_byte == read_byte) ? "[OK]" : "[FAIL]");
	if (test_byte != read_byte) test_passed = false;

	// 256瀛楄妭娴嬭瘯
	BG_flash_manager.SectorErase(test_address, DEV_NOR1);
	DBG("Writing %d bytes to NOR1 at 0x%08lX...\n", test_size, (unsigned long)test_address);
	BG_flash_manager.PageProgram(test_address, write_buffer, test_size, DEV_NOR1);

	memset(read_buffer, 0, sizeof(read_buffer));
	BG_flash_manager.ReadData(test_address, read_buffer, test_size, DEV_NOR1);

	// 楠岃瘉鏁版嵁
	bool nor1_ok = true;
	uint16_t error_count = 0;
	for (i = 0; i < test_size; i++)
	{
		if (read_buffer[i] != write_buffer[i])
		{
			if (error_count < 5)
				DBG("NOR1 mismatch at %d: wrote 0x%02X, read 0x%02X\n", i, write_buffer[i], read_buffer[i]);
			error_count++;
			nor1_ok = false;
		}
	}
	if (nor1_ok)
		DBG("NOR1 verification PASSED - All %d bytes match\n", test_size);
	else
	{
		DBG("NOR1 verification FAILED - %d errors\n", error_count);
		test_passed = false;
	}

	// ========== 娴嬭瘯NOR2 (CS=A22) ==========
	DBG("\n=== Testing NOR2 (CS=A22) ===\n");

	// 鍗曞瓧鑺傛祴璇�
	test_byte = 0x55;
	BG_flash_manager.SectorErase(test_address, DEV_NOR2);
	BG_flash_manager.PageProgram(test_address, &test_byte, 1, DEV_NOR2);
	BG_flash_manager.ReadData(test_address, &read_byte, 1, DEV_NOR2);
	DBG("NOR2 single byte: wrote 0x%02X, read 0x%02X %s\n",
	    test_byte, read_byte, (test_byte == read_byte) ? "[OK]" : "[FAIL]");
	if (test_byte != read_byte) test_passed = false;

	// 256瀛楄妭娴嬭瘯 - 浣跨敤涓嶅悓鐨勬祴璇曟ā寮�
	for (i = 0; i < test_size; i++)
	{
		write_buffer[i] = (uint8_t)(0x50 + (i & 0x0F)); // 妯″紡锛�0,51,52...5F
	}

	BG_flash_manager.SectorErase(test_address, DEV_NOR2);
	DBG("Writing %d bytes to NOR2 at 0x%08lX...\n", test_size, (unsigned long)test_address);
	BG_flash_manager.PageProgram(test_address, write_buffer, test_size, DEV_NOR2);

	memset(read_buffer, 0, sizeof(read_buffer));
	BG_flash_manager.ReadData(test_address, read_buffer, test_size, DEV_NOR2);

	// 楠岃瘉鏁版嵁
	bool nor2_ok = true;
	error_count = 0;
	for (i = 0; i < test_size; i++)
	{
		if (read_buffer[i] != write_buffer[i])
		{
			if (error_count < 5)
				DBG("NOR2 mismatch at %d: wrote 0x%02X, read 0x%02X\n", i, write_buffer[i], read_buffer[i]);
			error_count++;
			nor2_ok = false;
		}
	}
	if (nor2_ok)
		DBG("NOR2 verification PASSED - All %d bytes match\n", test_size);
	else
	{
		DBG("NOR2 verification FAILED - %d errors\n", error_count);
		test_passed = false;
	}

	// ========== 鏄剧ず娴嬭瘯缁撴灉 ==========
	if (test_passed)
	{
		DBG("\n========== Dual NOR Flash Test PASSED ==========\n");
		BG_lcd.Clear(GREEN);
	}
	else
	{
		DBG("\n========== Dual NOR Flash Test FAILED ==========\n");
		BG_lcd.Clear(RED);
	}

	DBG("Flash Test Summary:\n");
	DBG("- NOR1 (CS=A21): %s\n", nor1_ok ? "OK" : "FAIL");
	DBG("- NOR2 (CS=A22): %s\n", nor2_ok ? "OK" : "FAIL");
	DBG("========== Flash Test End ==========\n");
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

	prvInitialiseHeap();

	NVIC_EnableIRQ(SWI_IRQn);

	SarADC_Init();
	xQueue = xQueueCreate(4, sizeof(uint32_t));

	/* Initialize SPI hardware BEFORE driver framework (drivers need it) */
	DBG("[Main] Initializing SPI hardware...\n");
	spi_init();
	DBG("[Main] SPI initialized successfully\n");

	/* Initialize Driver Framework BEFORE RTOS */
	DBG("[Main] Initializing Driver Framework (before RTOS)...\n");
	DrvFramework_FullInit();
	DBG("[Main] Driver Framework initialized successfully\n");


	//xTaskCreate( (TaskFunction_t)FlashTask, "FlashTask", 512, NULL, 1, NULL );
	//xTaskCreate( (TaskFunction_t)FlashNewDriverTask, "FlashNewDriverTask", 1024, NULL, 1, NULL );
	// xTaskCreate( (TaskFunction_t)InternalFlashTestTask, "InternalFlashTest", 1024, NULL, 1, NULL );


	xTaskCreate((TaskFunction_t )EffectTask, "EffectTask", 4096, NULL, 1, NULL);


	DBG("[Main] Starting FreeRTOS scheduler...\n");
	vTaskStartScheduler();

	while (1)
		;
}


