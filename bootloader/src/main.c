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

#include <math.h>
#include <string.h>
#include <stdio.h>
/* Driver Framework */

#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_stor.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "otg_device_audio.h"



#include "bg_shell.h"           /* Shell console API */


#include "drv_init.h"           /* Driver Framework Initialization */


#ifdef BANGTSYNTH_EN
#include "bangtsynth_node.h"
#include "bg_storage.h"
#include "soundbank_manager.h"
#endif


extern void SysTickInit(void);
extern void UsbAudioMicDacInit(void);
extern void OTG_DeviceAudioInit();

extern void UsbAudioTimer1msProcess(void);
//__attribute__((section(".driver.isr")))

uint8_t record_flag = 0;

uint16_t rec = 0, rea = 0, play = 0;
uint16_t time = 0;

#define DAC_FIFO_SAMPLES 1024
static uint32_t DAC0_FIFO[DAC_FIFO_SAMPLES];
#define DAC0_FIFO_LEN sizeof(DAC0_FIFO)
static uint32_t DAC1_FIFO[DAC_FIFO_SAMPLES];
#define DAC1_FIFO_LEN sizeof(DAC1_FIFO)

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
	UsbAudioTimer1msProcess(); //1ms闂佽法鍠庤ぐ銊╁棘椤撶姰锟介柟椋庡厴閺佹捇鏁撻敓锟�
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

void power_on()
{

	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX24);




#ifdef BANGTSYNTH_EN
	/*=====================================================
	 * BanGTsynth MIDI 閸氬牊鍨氶崳銊ュ灥婵瀵�
	 * 閸掓繂顫愰崠锟組IDI 閹貉冨煑閸ｃ劊锟介棅鎶筋暥婢跺嫮鎮婂ù浣规寜缁撅拷
	 * 韫囧懘銆忛崷锟紸udio_Init 娑斿鎮楃拫鍐暏 (Effect Graph 瀹告彃鍨卞锟�
	 *====================================================*/
	DBG("[Task] Initializing BanGTsynth...\n");
	{
		extern int osPortRemainMem(void);
		DBG("[Task] Heap before BanGTsynth: %d bytes\n", osPortRemainMem());
	}
	const char *cmd = "sb -t 60 20 3000\r";
	if (BanGTsynth_Node_Init() == 0) {
		DBG("[Task] BanGTsynth initialized OK\n");

		BG_Storage.SetDriver(&bg_storage_driver_embedded);
		if (soundbank_manager.Init(0) == SUCCESS) {
			DBG("[Task] Embedded SF2 soundbank loaded OK\n");
			const char *cmd = "sb -t 60 20 3000\r";
			Shell_InputData((uint8_t *)cmd, strlen(cmd));
		} else {
			DBG("[Task] Embedded SF2 soundbank load FAILED\n");
		}
	} else {
		DBG("[Task] BanGTsynth init FAILED\n");
	}
#endif

	DBG("[Main] Entering main loop...\n");
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX20);
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIO_INDEX24);

}



/**
 * 鐠佸墽鐤嗘潏鎾冲毉闂婃娊鍣洪敍鍫ワ拷鏉╁槆DC鐠囪褰囬悽鍏哥秴閸ｃ劌锟介敍锟�
 */
static void SetVolume(void)
{
	uint16_t DC_Data;
	GPIO_RegOneBitClear(GPIO_A_ANA_EN, GPIO_INDEX28);
	GPIO_RegOneBitSet(GPIO_A_ANA_EN, GPIO_INDEX28);
	DC_Data = ADC_SingleModeDataGet(ADC_CHANNEL_GPIOA28) * 4;
	AudioDAC_VolSet(DAC0, DC_Data, DC_Data);
	AudioDAC_VolSet(DAC1, DC_Data, 0);

}

// 閸掓繂顫愰崠鏈B閸滃矁顔曟径鍥佸锟�
static void InitUSBDevice(void)
{
	// 娴ｈ法鏁UDIO_MIC_CDC濡�绱￠敍姘剁叾妫帮拷妤癸箑鍘犳锟紺DC娑撴彃褰涙径宥呮値鐠佹儳顦�
	OTG_DeviceModeSel(AUDIO_MIC_CDC, 0x1234, 0x1234);
	UsbDevicePlayInit();
	UsbDeviceEnable();
}


void tip_audio_init()
{
	AudioDAC_Init(ALL, 48000, (void *)DAC0_FIFO, DAC0_FIFO_LEN, (void *)DAC1_FIFO, DAC1_FIFO_LEN);
	AudioDAC_DoutModeSet(DAC0, MODE2, WIDTH_16_BIT);
	AudioDAC_DoutModeSet(DAC1, MODE2, WIDTH_16_BIT);
	AudioDAC_VolSet(DAC0, 0x3FFF, 0x3FFF);
	AudioDAC_VolSet(DAC1, 0x3FFF, 0);
}

void audio_loop()
{

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

		}

	}else{
		valid_press = 1;
		count_flag = 0;
	}


}




uint8_t time_count = 0;


void UpdataTask() {

	pwr_button_init();

	while (!power_flag)
	{
		pwr_butoon_handler();
	}
	audio_init();

	InitUSBDevice();
	

	BtStackServiceStart();

	while (1) {

		pwr_butoon_handler();
		/* Check and send delayed BLE sync responses */
		extern void BLE_CheckSyncResponse(void);
		BLE_CheckSyncResponse();

		ShellIOManager_Process();

		/* USB 鐑彃鎷旀娴嬶紙涓�UI 瑙ｈ�锛岀洿鎺ュ湪闊抽绯荤粺澶勭悊锛�*/
		USB_HotplugCheck();
		/* Update UI System (handles button input, menu, status bar) */
		BtStackServiceRun();
		
		OTG_DeviceRequestProcess();

		/* CDC涓插彛浠诲姟澶勭悊 - 蹇呴』鍛ㄦ湡鎬ц皟鐢ㄤ互鎺ユ敹鏁版嵁 */
		OTG_DeviceCDC_Task();
		audio_loop();
	}
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

	xQueue = xQueueCreate(4, sizeof(uint32_t));
	SarADC_Init();
	/* Initialize SPI hardware BEFORE driver framework (drivers need it) */
	DBG("[Main] Initializing SPI hardware...\n");
	spi_init();
	DBG("[Main] SPI initialized successfully\n");



	xTaskCreate((TaskFunction_t )UpdataTask, "UpdataTask", 4096, NULL, 1, NULL);


	DBG("[Main] Starting FreeRTOS scheduler...\n");
	vTaskStartScheduler();

	while (1)
		;
}


