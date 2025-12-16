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
#include "bg_encoder.h"
#include "bg_flash_manager.h"
#include "bg_lcd.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_stor.h"
#include "menu_slider.h"
#include "gui_tool.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "otg_device_audio.h"
#include "ctrlvars.h"
#include "page_manager.h"
#include "pcf8574.h"
#include "bg_audio_io_manager.h"
#include "hardware_conf.h"
#include "framebuffer.h"


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

BG_Page BG_page;
uint8_t UI_count = 0, UI_flag = 0;

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
	UsbAudioTimer1msProcess(); //1ms锟叫断硷拷锟�
#endif
}

xQueueHandle xQueue;

uint32_t SendCount = 0;
uint32_t RecvCount = 0;
uint32_t result[100];

//2锟斤拷全锟斤拷buf锟节伙拷锟斤拷ADC锟斤拷DAC锟捷ｏ拷注锟解单位

//

int16_t CRC[100] = { 0 };
static int16_t WriteBufer[96] = { 0 };

static int16_t CRC2[96] = { 0 };
static int16_t ReadBuf[96] = { 0 };
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

static uint8_t DmaChannelMap[29] = { 255, //PERIPHERAL_ID_SPIS_RX = 0,		//0
		255, //PERIPHERAL_ID_SPIS_TX,			//1
		255, //PERIPHERAL_ID_TIMER3,			//2
		255, //PERIPHERAL_ID_SDIO_RX,			//3
		255, //PERIPHERAL_ID_SDIO_TX,			//4
		255, //PERIPHERAL_ID_UART0_RX,			//5
		255, //PERIPHERAL_ID_TIMER1,				//6
		255, //PERIPHERAL_ID_TIMER2,				//7
		255, //PERIPHERAL_ID_SDPIF_RX,			//8 SPDIF_RX /TX锟斤拷使锟斤拷同一通锟斤拷
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


void button_init()
{
	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX16);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX16);
	GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX16);
	GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX16);

	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX0);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX0);
	GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX0);
	GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX0);

	GPIO_RegOneBitSet(GPIO_B_IE, GPIO_INDEX5);
	GPIO_RegOneBitClear(GPIO_B_OE, GPIO_INDEX5);
	GPIO_RegOneBitSet(GPIO_B_PU, GPIO_INDEX5);
	GPIO_RegOneBitClear(GPIO_B_PD, GPIO_INDEX5);

	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX15);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX15);
	GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX15);
	GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX15);
}

void EffectTask() {


	SarADC_Init();
	CtrlVarsInit();
	spi_init();
	GPIO_RegOneBitClear(GPIO_B_IE, GPIOB6);
	GPIO_RegOneBitSet(GPIO_B_OE, GPIOB6);
	GPIO_RegOneBitSet(GPIO_B_OUT, GPIOB6);
#ifdef BAN_SPEAKER_V2
	GPIO_RegOneBitClear(GPIO_A_IE, GPIOA1);
	GPIO_RegOneBitSet(GPIO_A_OE, GPIOA1);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA1);
#endif
	GPIO_RegOneBitClear(GPIO_A_IE, GPIOA17);
	GPIO_RegOneBitSet(GPIO_A_OE, GPIOA17);
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA17);
	BG_lcd.Init();
	 BG_lcd.Clear(BLUE);
	// 初始化帧缓冲系统
//	FrameBuffer_Init();

	BG_page = BG_Page_Init(table, MAX_PAGE);
//
//	// 使用帧缓冲清屏
//	FrameBuffer_Clear(0x0000);
//	FrameBuffer_Flush(); // 立即刷新到屏幕

	button_init();
	//BG_page.Next(&BG_page);
	BG_AudioManager.Audio_Init(44100);
	while (1) {
		BG_AudioManager.Audio_Loop();
		
		if(UI_flag == 1){
			UI_flag =0;
		BG_page.Loop(&BG_page);

#ifdef USE_FRAME_BUFFER
    BG_lcd.FlushFrameBuffer();
#endif
		}
//		if (UI_flag == 1) {
//			UI_flag = 0;
//			BG_page.Loop(&BG_page);
//		}
//		if(GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX0)){
//			if (UI_flag == 1) {
//						UI_flag = 0;
//			BG_page.Enter(&BG_page);
//			}
//		}
//
//		if(GPIO_RegOneBitGet(GPIO_B_IN, GPIO_INDEX5)){
//			if (UI_flag == 1) {
//									UI_flag = 0;
//			BG_page.Next(&BG_page);
//			}
//		}
//
//		if(GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX15)){
//			if (UI_flag == 1) {
//									UI_flag = 0;
//			BG_page.Last(&BG_page);
//			}
//		}
//
//		if(GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX16)){
//			if (UI_flag == 1) {
//									UI_flag = 0;
//			BG_page.Exit(&BG_page);
//			}
//		}

	}
}

//static uint8_t DmaTxBuf[512];
//static uint8_t DmaRxBuf[1024];
//static uint8_t DmaTempBuf[512];
//static uint8_t DmaTempBuf1[512];
//#define DMA_TST_BUF_LEN 10
//
//static void DmaInterruptUart1Tx(void)
//{
//	static int Flag = 0;
//	if(DMA_InterruptFlagGet(PERIPHERAL_ID_UART1_TX, DMA_THRESHOLD_INT))
//	{
//		if(Flag==0)
//		{
//			DMA_CircularDataPut(PERIPHERAL_ID_UART1_TX, DmaTempBuf, sizeof(DmaTempBuf)/2);
//			Flag = 1;
//		}else
//		{
//			DMA_CircularDataPut(PERIPHERAL_ID_UART1_TX, DmaTempBuf1, sizeof(DmaTempBuf1)/2);
//			Flag = 0;
//		}
//
//		UARTS_DMA_TxIntFlgClr(UART_PORT1, DMA_THRESHOLD_INT);
//		//DBG("\nUART1 TX DMA_THRESHOLD_INT\n");
//	}
//	if(DMA_InterruptFlagGet(PERIPHERAL_ID_UART1_TX, DMA_DONE_INT))
//	{
//		UARTS_DMA_TxIntFlgClr(UART_PORT1, DMA_DONE_INT);
//		DBG("\nUART1 TX DMA_DONE_INT\n");
//	}
//	if(DMA_InterruptFlagGet(PERIPHERAL_ID_UART1_TX, DMA_ERROR_INT))
//	{
//		UARTS_DMA_TxIntFlgClr(UART_PORT1, DMA_ERROR_INT);
//		DBG("\nUART1 TX DMA_ERROR_INT\n");
//	}
//}
//void SerialTask() {
//
//	int i,Echo;
//	UARTS_DMA_TxInit(UART_PORT1, (void*)DmaTxBuf, sizeof(DmaTxBuf), DMA_TST_BUF_LEN, DmaInterruptUart1Tx);//配置
//
//		//step 4:开始传输数据
//	UARTS_DMA_SendDataStart(UART_PORT1,DmaTxBuf,256);
//
//	DMA_ChannelDisable(PERIPHERAL_ID_UART1_RX);
//	DMA_CircularConfig(PERIPHERAL_ID_UART1_RX,sizeof(DmaRxBuf)/2,DmaRxBuf,sizeof(DmaRxBuf));
//	DMA_ChannelEnable(PERIPHERAL_ID_UART1_RX);
//
//	//step 4:使能RX
//	UART1_IOCtl(UART_IOCTL_DMA_RX_EN, 1);
//
//	DBG("\n***** you will receive what you have sent *****\n");
//	while(1)
//	{
//		//数据处理示例：
//		Echo = DMA_CircularDataLenGet(PERIPHERAL_ID_UART1_RX);
//		if(Echo>0)
//		{
//			DMA_CircularDataGet(PERIPHERAL_ID_UART1_RX,DmaRxBuf,Echo);
//			for(i=0;i<Echo;i++)
//			{
//				DBG("%c",DmaRxBuf[i]);//展示此次收到的数据
//			}
//		}
//	}
//
//}

//void btTask()
//{
//	BtStackServiceRun();
//}
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


	xTaskCreate((TaskFunction_t )EffectTask, "EffectTask", 2048, NULL, 1, NULL);

	//xTaskCreate( (TaskFunction_t)btTask, "btTask",2048, NULL, 1, NULL );

	vTaskStartScheduler();

	while (1)
		;
}

