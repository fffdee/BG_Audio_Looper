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
#include "audio_looper.h"
#include <math.h>
#include <string.h>
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_stor.h"
#include "usb_audio_api.h"
#include "otg_detect.h"
#include "otg_device_audio.h"
#include "gui_tool.h"
#include "ctrlvars.h"
#include "page_manager.h"
#include "audio_looper.h"

extern void SysTickInit(void);
extern void UsbAudioMicDacInit(void);
extern void OTG_DeviceAudioInit();

extern void UsbAudioTimer1msProcess(void);
//__attribute__((section(".driver.isr")))

uint8_t record_flag = 0;
uint16_t read_write = 0;
uint8_t play_flag=0;
uint16_t rec = 0,rea=0,play=0;
uint16_t time=0;
uint16_t left_time = 0;
uint16_t right_time = 0;
uint8_t left_flag = false;
uint8_t right_flag = false;

BG_Page BG_page;
uint8_t UI_count =0,UI_flag=0;

uint8_t key_flag =1;
uint16_t key_time;
void Timer2Interrupt(void)
{
	Timer_InterruptFlagClear(TIMER2, UPDATE_INTERRUPT_SRC);
	OTG_PortLinkCheck();
	//DBG("time run");

	if(left_flag==1)
		left_time++;
	else left_time=0;

	if(right_flag==1)
		right_time++;
	else right_time=0;

	if(key_flag==0){
		key_time++;
	}else{
		key_time=0;
	}

	if(key_time>=1000){
		key_flag = 1;
	}
	rea++;
	time++;
	UI_count++;
	if(UI_count==10){
		UI_flag=1;
	}else{
		UI_flag=0;
	}
	if(time>65530)time=0;
	
	// 移除record_flag设置，录制现在基于音频数据可用性
	// if(record_flag==0){
	//     record_flag=1;  // 不再需要定时器控制录制
	// }

	// 更新loop状态 - 在1ms定时中断中处理所有实时状态更新
	loop_timer_update();

#ifdef CFG_APP_USB_AUDIO_MODE_EN
	UsbAudioTimer1msProcess(); //1ms锟叫断硷拷锟�
#endif
}


xQueueHandle xQueue;

uint32_t SendCount = 0;
uint32_t RecvCount = 0;
uint32_t result[100];


//2锟斤拷全锟斤拷buf锟节伙拷锟斤拷ADC锟斤拷DAC锟捷ｏ拷注锟解单位
uint32_t AudioADC1Buf[1024] = {0}; //1024 * 4 = 4K
uint32_t AudioADC2Buf[1024] = {0}; //1024 * 4 = 4K
uint32_t AudioDACBuf[1024] = {0}; //1024 * 4 = 4K

static uint32_t PcmBuf1[512] = {0};  // 增大缓冲区以支持更多数据
static uint32_t PcmBuf2[512] = {0};
static uint32_t PcmBuf3[512] = {0};
static uint32_t PcmBuf4[512] = {0};
int16_t CRC[100] = {0};
int16_t WriteBufer[96] = {0};

int16_t CRC2[96] = {0};
int16_t ReadBuf[96] = {0};
uint32_t sectorAddress = 0;

uint32_t record_time;
#define  MAX_BUF_LEN   4096

uint8_t spimRate  = SPIM_CLK_DIV_24M;
uint8_t spimMode = 0;
uint8_t SpimBuf_TX[MAX_BUF_LEN];
uint8_t SpimBuf_RX[MAX_BUF_LEN];



const char* spimIO[][4] =
{
	//    cs      miso     clk      mosi
		{"A22",    "A7",    "A6",     "A5"},
		{"A8",    "A22",   "A21",    "A20"},
};


static uint8_t DmaChannelMap[29] = {
	255,//PERIPHERAL_ID_SPIS_RX = 0,		//0
	255,//PERIPHERAL_ID_SPIS_TX,			//1
	255,//PERIPHERAL_ID_TIMER3,			//2
	255,//PERIPHERAL_ID_SDIO_RX,			//3
	255,//PERIPHERAL_ID_SDIO_TX,			//4
	255,//PERIPHERAL_ID_UART0_RX,			//5
	255,//PERIPHERAL_ID_TIMER1,				//6
	255,//PERIPHERAL_ID_TIMER2,				//7
	255,//PERIPHERAL_ID_SDPIF_RX,			//8 SPDIF_RX /TX锟斤拷使锟斤拷同一通锟斤拷
	255,//PERIPHERAL_ID_SDPIF_TX,			//9
	0,//PERIPHERAL_ID_SPIM_RX,			//10
	1,//PERIPHERAL_ID_SPIM_TX,			//11
	255,//PERIPHERAL_ID_UART0_TX,			//12
	255,//PERIPHERAL_ID_UART1_RX,			//13
	255,//PERIPHERAL_ID_UART1_TX,			//14
	255,//PERIPHERAL_ID_TIMER4,				//15
	255,//PERIPHERAL_ID_TIMER5,				//16
	255,//PERIPHERAL_ID_TIMER6,				//17
	2,//PERIPHERAL_ID_AUDIO_ADC0_RX,		//18
	3,//PERIPHERAL_ID_AUDIO_ADC1_RX,		//19
	4,//PERIPHERAL_ID_AUDIO_DAC0_TX,		//20
	5,//PERIPHERAL_ID_AUDIO_DAC1_TX,		//21
	255,//PERIPHERAL_ID_I2S0_RX,			//22
	255,//PERIPHERAL_ID_I2S0_TX,			//23
	255,//PERIPHERAL_ID_I2S1_RX,			//24
	255,//PERIPHERAL_ID_I2S1_TX,			//25
	255,//PERIPHERAL_ID_PPWM,				//26
	255,//PERIPHERAL_ID_ADC,     			//27
	255,//PERIPHERAL_ID_SOFTWARE,			//28
};

void spi_init(void)
{
		SPIM_SetDmaEn(1);
		SPIM_IoConfig(SPIM_PORT0_A5_A6_A7);
		DMA_ChannelAllocTableSet(DmaChannelMap);
		if(SPIM_Init(spimMode,spimRate))
		{
				DBG("SPI init success!\n");
				DBG("spim mode:%d\n",spimMode);//
				DBG("spim rate:%d\n",spimRate);//
				DBG("spim_cs  :%s\n",spimIO[0][0]);
				DBG("spim_miso:%s\n",spimIO[0][1]);
				DBG("spim_clk :%s\n",spimIO[0][2]);
				DBG("spim_mosi:%s\n",spimIO[0][3]);
		}
		else
		{
			DBG("****** Err: SPI init fail ******\n");
		}

}
static void AudioMicExample(void)
{
    uint32_t  SampleRate = 44100;
    uint16_t  RealLen;
    uint16_t  n;

	//DAC init
	AudioDAC_Init(DAC0, SampleRate, (void*)AudioDACBuf, sizeof(AudioDACBuf), NULL, 0);

	//Mic1 Mic2  analog
	AudioADC_AnaInit();


	AudioADC_MicBias1Enable(TRUE);
	//AudioADC_VcomConfig(1);//MicBias en
	//模锟斤拷通为NONE止锟较达拷通然锟斤拷要锟斤拷模锟斤拷通锟斤拷

	AudioADC_DynamicElementMatch(ADC1_MODULE, TRUE, TRUE);

	AudioADC_PGASel(ADC1_MODULE, CHANNEL_RIGHT, LINEIN_NONE);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_LEFT, LINEIN_NONE);

	AudioADC_PGASel(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1);

	AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2, 15, 4);
	AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1, 15, 4);

	//Mic1 Mic2  digital
	AudioADC_DigitalInit(ADC1_MODULE, SampleRate, (void*)AudioADC1Buf, sizeof(AudioADC1Buf));

	while(1)
	{
		if(AudioADC_DataLenGet(ADC1_MODULE) >= 256)
		{
			n = AudioDAC_DataSpaceLenGet(DAC0);
			RealLen = AudioADC_DataGet(ADC1_MODULE, PcmBuf1, 256);
			if(n > RealLen)
			{
				n = RealLen;
			}
			AudioDAC_DataSet(DAC0, PcmBuf1, n);
		}
	}
}

static void AudioLineInExample()
{
    uint32_t  SampleRate = 48000;
    uint16_t  RealLen;
    uint16_t  n;

	//DAC init
	AudioDAC_Init(DAC0, SampleRate, (void*)AudioDACBuf, sizeof(AudioDACBuf), NULL, 0);

	//LineIn1  analog
	AudioADC_AnaInit();

	AudioADC_DynamicElementMatch(ADC0_MODULE, TRUE, TRUE);

	//模锟斤拷通为NONE止锟较达拷通然锟斤拷要锟斤拷模锟斤拷通锟斤拷
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_RIGHT, LINEIN_NONE);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_LEFT, LINEIN_NONE);

	AudioADC_PGASel(ADC0_MODULE, CHANNEL_RIGHT, LINEIN1_RIGHT);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_LEFT, LINEIN1_LEFT);

	AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_RIGHT, LINEIN1_RIGHT, 12, 4);
	AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_LEFT, LINEIN1_LEFT, 12, 4);

	//LineIn1  digital
	AudioADC_DigitalInit(ADC0_MODULE, SampleRate, (void*)AudioADC1Buf, sizeof(AudioADC1Buf));

	while(1)
	{
		if(AudioADC_DataLenGet(ADC0_MODULE) >= 256)
		{
			n = AudioDAC_DataSpaceLenGet(DAC0);
			RealLen = AudioADC_DataGet(ADC0_MODULE, PcmBuf1, 256);
			if(n > RealLen)
			{
				n = RealLen;
			}
			AudioDAC_DataSet(DAC0, PcmBuf1, n);
		}
	}
}

static void AudioMixerExample(void)
{
	uint32_t  SampleRate = 48000;
	uint16_t  RealLen;
	uint16_t  n;
	uint32_t  cnt;

	//DAC init
	AudioDAC_Init(DAC0, SampleRate, (void*)AudioDACBuf, sizeof(AudioDACBuf), NULL, 0);

	//ADC
	//LineIn1  analog
	AudioADC_AnaInit();

	AudioADC_DynamicElementMatch(ADC0_MODULE, TRUE, TRUE);

	//模锟斤拷通为NONE止锟较达拷通然锟斤拷要锟斤拷模锟斤拷通锟斤拷
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_RIGHT, LINEIN_NONE);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_LEFT, LINEIN_NONE);

	AudioADC_PGASel(ADC0_MODULE, CHANNEL_RIGHT, LINEIN1_RIGHT);
	AudioADC_PGASel(ADC0_MODULE, CHANNEL_LEFT, LINEIN1_LEFT);

	AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_RIGHT, LINEIN1_RIGHT, 12, 4);
	AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_LEFT, LINEIN1_LEFT, 12, 4);
	//LineIn2  digital
	AudioADC_DigitalInit(ADC0_MODULE, SampleRate, (void*)AudioADC1Buf, sizeof(AudioADC1Buf));




	AudioADC_MicBias1Enable(TRUE);

	AudioADC_DynamicElementMatch(ADC1_MODULE, TRUE, TRUE);

	//模锟斤拷通为NONE止锟较达拷通然锟斤拷要锟斤拷模锟斤拷通锟斤拷
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_RIGHT, LINEIN_NONE);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_LEFT, LINEIN_NONE);

	AudioADC_PGASel(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2);
	AudioADC_PGASel(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1);

	AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2, 15, 4);
	AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1, 15, 4);
	//Mic1 Mic2  digital
	AudioADC_DigitalInit(ADC1_MODULE, SampleRate, (void*)AudioADC2Buf, sizeof(AudioADC2Buf));

	while(1)
	{
		if(AudioADC_DataLenGet(ADC1_MODULE) >= 256)
		{
			n = AudioDAC_DataSpaceLenGet(DAC0);
			RealLen = AudioADC_DataGet(ADC1_MODULE, PcmBuf1, 256);
			if(n > RealLen)
			{
				n = RealLen;
			}

			RealLen = AudioADC_DataGet(ADC0_MODULE, PcmBuf2, 256);
			if(n > RealLen)
			{
				n = RealLen;
			}
			for(cnt = 0; cnt< n; cnt++)
			{

				PcmBuf3[cnt] = PcmBuf1[cnt] + PcmBuf2[cnt];
			}
			AudioDAC_DataSet(DAC0, PcmBuf3, n);
		}
	}
}
void SendTask(void)
{
	while(1){
	DelayMs(500);
	SendCount++;
	DBG("SendMsg:0x%x\n",(unsigned int)SendCount);
	xQueueSend( xQueue, &SendCount, portMAX_DELAY );
	}
}

void RecvTask(void)
{
	uint32_t temp,i;
	while(1){
	 xQueueReceive( xQueue, &temp, portMAX_DELAY );
	 DBG("RecvMsg:0x%x\n\n",(unsigned int)temp);
	 //DBG("RecvMsg:%d\n\n",(unsigned int)temp);
	 result[RecvCount++] = temp;

	 if(RecvCount == 100)
		 RecvCount = 0;
	}
}


//uint32_t distort(uint32_t input) {
//    // 值锟窖撅拷一0锟斤拷UINT32_MAX之锟斤拷
//    uint32_t output;
//    if (input > (UINT32_MAX / 2) + (UINT32_MAX / 10)) {
//        output = UINT32_MAX; // 锟斤拷锟酵碉拷锟斤拷锟街�
//    } else if (input < (UINT32_MAX / 2) - (UINT32_MAX / 10)) {
//        output = 0; // 锟斤拷锟酵碉拷锟斤拷小值
//    } else {
//        // 锟斤拷模锟斤拷失锟芥，虻サ爻锟斤拷锟�.1锟斤拷实锟斤拷应锟斤拷锟叫匡拷要锟接碉拷锟姐法
//        output = (input * 11) / 10;
//        if (output > UINT32_MAX) {
//            output = UINT32_MAX;
//        }
//    }
//    return output;
//}

void convertUint8ArrayToInt16Array(const uint8_t *input, int16_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; ++i) {
            // 假设系统是小端
            int16_t sample = (int16_t)(input[i * 2]) | ((int16_t)(input[i * 2 + 1]) << 8);
            output[i] = sample;
        }
}

void convertUint32ArrayToUint8Array(const uint32_t *input, uint8_t *output, size_t size) {
	size_t i;
	for (i = 0; i < size; i++) {
        // 每个uint32_t转换为4个uint8_t，保持双声道数据完整
        // 假设系统是小端，uint32_t格式为: [右声道低8位][右声道高8位][左声道低8位][左声道高8位]
        output[i * 4]     = (uint8_t)(input[i] & 0xFF);         // 右声道低8位
        output[i * 4 + 1] = (uint8_t)((input[i] >> 8) & 0xFF);  // 右声道高8位
        output[i * 4 + 2] = (uint8_t)((input[i] >> 16) & 0xFF); // 左声道低8位
        output[i * 4 + 3] = (uint8_t)((input[i] >> 24) & 0xFF); // 左声道高8位
    }
}

void convertUint8ArrayToUint32Array(const uint8_t *input, uint32_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; i++) {
        // 将4个uint8_t重新组合为1个uint32_t，恢复双声道数据
        output[i] = (uint32_t)input[i * 4] |
                    ((uint32_t)input[i * 4 + 1] << 8) |
                    ((uint32_t)input[i * 4 + 2] << 16) |
                    ((uint32_t)input[i * 4 + 3] << 24);
    }
}

void convertInt16ArrayToUint8Array(const int16_t *input, uint8_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; ++i) {
        // 假设系统是小端
        output[i * 2] = (uint8_t)(input[i] & 0xFF); // 低8位
        output[i * 2 + 1] = (uint8_t)((input[i] >> 8) & 0xFF); // 高8位
    }
}
void AudioTask(void)
{
	//AudioMicExample();

	uint16_t  RealLen;
	uint16_t  n;
	uint16_t  i;




	while(1){


		DBG("record is ready\n");
		if(AudioADC_DataLenGet(ADC1_MODULE) >= 256)
			{
				n = AudioDAC_DataSpaceLenGet(DAC0);
				RealLen = AudioADC_DataGet(ADC1_MODULE, PcmBuf1, 256);
				if(n > RealLen)
				{
					n = RealLen;
				}

				//


				 AudioDAC_DataSet(DAC0, PcmBuf1, n);

//				for(i = 0; i<n; i++){
//
//									//PcmBuf2[i] = distort(PcmBuf1[i]);
//						PcmBuf1[i] = 0xf5f5f5f5;
//									//DBG("{0x%08X}\n",PcmBuf1[i]);
//				}


			}
	}

}

void RecordTask(){
	uint8_t Buffer[1024];

//	spi_init();
//	BG_flash_manager.Init();
//	BG_flash_manager.EraseAll();
	play_flag=0;

	while(1){

		if(play_flag==1&&record_flag==1){

			convertUint32ArrayToUint8Array(PcmBuf3,Buffer ,240);  // 240个uint32样本 = 960字节 (240*4)
							//DBG("{0x%08X, 0x%08X}\n",PcmBuf1[50],sectorAddress);
							//BG_flash_manager.SectorErase(sectorAddress);
							if(BG_flash_manager.PageProgram(sectorAddress, Buffer, 960,DEV_NOR)==0){  // 写入960字节

									sectorAddress =sectorAddress+960;
							}
							//BG_flash_manager.GetTotalByte()
							DBG("have not space ! recording ,Total is %d\n",BG_flash_manager.GetTotalByte(DEV_NOR));
							if(sectorAddress>BG_flash_manager.GetTotalByte(DEV_NOR)){
										DBG("have not space ! recording ,Total is %d\n",BG_flash_manager.GetTotalByte(DEV_NOR));
										play_flag =!play_flag;
										sectorAddress=0;
						 }
		}else if(play_flag==0){
							BG_flash_manager.ReadData(sectorAddress, Buffer, 960,DEV_NOR);

							convertUint8ArrayToUint32Array(Buffer,PcmBuf2,240);  // 将960字节转换为240个uint32样本

							//if(PcmBuf3[i]!=PcmBuf2[128])
							//DBG("{0x%08X  ,0x%08X }\n",sectorAddress,PcmBuf2[50]);
							AudioDAC_DataSet(DAC0, PcmBuf2, 240);  // 播放240个uint32样本
							sectorAddress +=960;
							if(sectorAddress>BG_flash_manager.GetTotalByte(DEV_NOR))
								sectorAddress = 0;
		}
	}

}

void input_det(){



		if(!GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX28)){
					DBG("HP1 SI INPUT");
		}
		if(!GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX29)){
					DBG("HP2 SI INPUT");
		}
		if(!GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX30)){
					DBG("HP3 SI INPUT");
		}
}



void FlashTask(void)
{
	spi_init();
	BG_lcd.Init();
	BG_lcd.Clear(RED);
	// 初始化闪存管理器
	BG_flash_manager.Init();

	DBG("EARSE ALL\n");
	// 擦除所有闪存内容
	BG_flash_manager.EraseAll(DEV_NAND);
	DBG("EARSE FINISH\n");

	// 读取闪存ID
	uint8_t manufacturerID, memoryType, deviceID;
	BG_flash_manager.ReadID(&manufacturerID, &memoryType, &deviceID,DEV_NOR);

	// 打印闪存ID
	DBG("ID is %d%d%d\n", manufacturerID, memoryType, deviceID);
	BG_flash_manager.ReadID(&manufacturerID, &memoryType, &deviceID,DEV_NAND);

	// 打印闪存ID
	DBG("ID is %X%X%X\n", manufacturerID, memoryType, deviceID);

	// ================== 第一个扇区读写测试 ==================
	DBG("========== Flash Sector Test Start ==========\n");
	
	// 测试数据缓冲区
	uint8_t write_buffer[256];
	uint8_t read_buffer[256];
	uint32_t test_address = 0x1000; // 使用第二个4K扇区，避开地址0
	uint16_t test_size = 256;  // 测试数据大小
	uint16_t i;
	bool test_passed = true;
	
	// 1. 准备测试数据
	DBG("Preparing test data...\n");
	for(i = 0; i < test_size; i++) {
		write_buffer[i] = (uint8_t)(0xA0 + (i & 0x0F)); // 模式：A0,A1,A2...AF,A0,A1... 更容易识别偏移
	}
	
	// 打印写入数据（前32字节）
	DBG("Write data (first 32 bytes):\n");
	for(i = 0; i < 32 && i < test_size; i++) {
		if(i % 16 == 0) DBG("\n0x%04X: ", i);
		DBG("%02X ", write_buffer[i]);
	}
	DBG("\n");
	
	// 1.5 单字节测试 - 先进行简单的单字节读写测试
	DBG("=== Single Byte Test ===\n");
	uint8_t test_byte = 0xAA;
	uint8_t read_byte;
	
	// 测试NOR Flash单字节
	BG_flash_manager.SectorErase(test_address, DEV_NOR);
	BG_flash_manager.PageProgram(test_address, &test_byte, 1, DEV_NOR);
	BG_flash_manager.ReadData(test_address, &read_byte, 1, DEV_NOR);
	DBG("NOR single byte: wrote 0x%02X, read 0x%02X\n", test_byte, read_byte);
	
	// 测试NAND Flash单字节
	BG_flash_manager.SectorErase(test_address, DEV_NAND);
	BG_flash_manager.PageProgram(test_address, &test_byte, 1, DEV_NAND);
	BG_flash_manager.ReadData(test_address, &read_byte, 1, DEV_NAND);
	DBG("NAND single byte: wrote 0x%02X, read 0x%02X\n", test_byte, read_byte);
	DBG("=== End Single Byte Test ===\n");
	
	// 2. 擦除第一个扇区（NOR Flash）
	DBG("Erasing first sector (NOR)...\n");
	BG_flash_manager.SectorErase(test_address, DEV_NOR);
	DBG("NOR sector erase completed\n");
	
	// 3. 写入测试数据到NOR Flash
	DBG("Writing test data to NOR flash at address 0x%08lX...\n", (unsigned long)test_address);
	if(BG_flash_manager.PageProgram(test_address, write_buffer, test_size, DEV_NOR) == 0) {
		DBG("NOR write success, %d bytes written\n", test_size);
	} else {
		DBG("NOR write failed\n");
		test_passed = false;
	}
	
	// 4. 读取数据并验证（NOR Flash）
	DBG("Reading data from NOR flash...\n");
	memset(read_buffer, 0, sizeof(read_buffer)); // 清空读取缓冲区

	BG_flash_manager.ReadData(test_address, read_buffer, test_size, DEV_NOR);
	DBG("NOR read completed, %d bytes read\n", test_size);
	
	// 打印读取数据（前32字节）
	DBG("Read data (first 32 bytes):\n");
	for(i = 0; i < 32 && i < test_size; i++) {
		if(i % 16 == 0) DBG("\n0x%04X: ", i);
		DBG("%02X ", read_buffer[i]);
	}
	DBG("\n");
		
	// 验证数据
	bool nor_verify_ok = true;
	uint16_t error_count = 0;
	for(i = 0; i < test_size; i++) {
		if(read_buffer[i] != write_buffer[i]) {
			if(error_count < 10) { // 只打印前10个错误
				DBG("NOR data mismatch at offset %d: wrote 0x%02X, read 0x%02X\n",
					i, write_buffer[i], read_buffer[i]);
			}
			error_count++;
			nor_verify_ok = false;
			test_passed = false;
		}
	}
	
	if(nor_verify_ok) {
		DBG("NOR data verification PASSED - All %d bytes match\n", test_size);
	} else {
		DBG("NOR data verification FAILED - %d errors found\n", error_count);
	}
	
	// 5. 测试NAND Flash（如果可用）
	DBG("Testing NAND flash...\n");
	
	// 擦除NAND第一个扇区
	BG_flash_manager.SectorErase(test_address, DEV_NAND);
	DBG("NAND sector erase completed\n");
	
	// 写入测试数据到NAND Flash
	if(BG_flash_manager.PageProgram(test_address, write_buffer, test_size, DEV_NAND) == 0) {
		DBG("NAND write success, %d bytes written\n", test_size);
		
		// 读取并验证NAND数据
		memset(read_buffer, 0, sizeof(read_buffer));
		BG_flash_manager.ReadData(test_address, read_buffer, test_size, DEV_NAND);
		DBG("NAND read completed, %d bytes read\n", test_size);
		
		// 打印NAND读取数据（前32字节）
		DBG("NAND read data (first 32 bytes):\n");
		for(i = 0; i < 32 && i < test_size; i++) {
			if(i % 16 == 0) DBG("\n0x%04X: ", i);
			DBG("%02X ", read_buffer[i]);
		}
		DBG("\n");
		
		bool nand_verify_ok = true;
		uint16_t nand_error_count = 0;
		for(i = 0; i < test_size; i++) {
			if(read_buffer[i] != write_buffer[i]) {
				if(nand_error_count < 10) { // 只打印前10个错误
					DBG("NAND data mismatch at offset %d: wrote 0x%02X, read 0x%02X\n",
						i, write_buffer[i], read_buffer[i]);
				}
				nand_error_count++;
				nand_verify_ok = false;
				test_passed = false;
			}
		}
		
		if(nand_verify_ok) {
			DBG("NAND data verification PASSED - All %d bytes match\n", test_size);
		} else {
			DBG("NAND data verification FAILED - %d errors found\n", nand_error_count);
		}
	} else {
		DBG("NAND write failed\n");
	}
	
	// 6. 显示测试结果
	if(test_passed) {
		DBG("========== Flash Test PASSED ==========\n");
		BG_lcd.Clear(GREEN); // 绿色表示测试通过
	} else {
		DBG("========== Flash Test FAILED ==========\n");
		BG_lcd.Clear(RED);   // 红色表示测试失败
	}
	
	// 7. 显示Flash信息摘要
	DBG("Flash Test Summary:\n");
	DBG("- Test Address: 0x%08lX\n", (unsigned long)test_address);
	DBG("- Test Size: %d bytes\n", test_size);
	DBG("- NOR Flash ID: 0x%02X%02X%02X\n", manufacturerID, memoryType, deviceID);
	DBG("- Address alignment: %s\n", (test_address % 256 == 0) ? "Page aligned" : "Not page aligned");
	DBG("========== Flash Sector Test End ==========\n");

}
void audio_process(void);
void USBTask(){

	OTG_DeviceModeSel(AUDIO_MIC,0x1234,0x1234);


	audio_init(48000);
	UsbDevicePlayInit();
	UsbDeviceEnable();

	while(1){

		OTG_DeviceRequestProcess();
		OTG_DeviceStorProcess();
		audio_process();
	}

}
void audio_Init(uint16_t SampleRate){


		//DAC init
		AudioDAC_Init(DAC0, SampleRate, (void*)AudioDACBuf, sizeof(AudioDACBuf), NULL, 0);

		//Mic1 Mic2  analog
		AudioADC_AnaInit();

		AudioADC_DynamicElementMatch(ADC0_MODULE, TRUE, TRUE);

		//模拟通道先配置为NONE，防止上次配置通道残留，然后再配置需要的模拟通道
		AudioADC_PGASel(ADC0_MODULE, CHANNEL_RIGHT, LINEIN_NONE);
		AudioADC_PGASel(ADC0_MODULE, CHANNEL_LEFT, LINEIN_NONE);

		AudioADC_PGASel(ADC0_MODULE, CHANNEL_RIGHT, LINEIN5_RIGHT);
		AudioADC_PGASel(ADC0_MODULE, CHANNEL_LEFT, LINEIN5_LEFT);

		AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_RIGHT, LINEIN5_RIGHT, 16, 0);
		AudioADC_PGAGainSet(ADC0_MODULE, CHANNEL_LEFT, LINEIN5_LEFT, 16, 0);
		//LineIn2  digital
		AudioADC_DigitalInit(ADC0_MODULE, SampleRate, (void*)AudioADC1Buf, sizeof(AudioADC1Buf));


		AudioADC_MicBias1Enable(TRUE);
		//AudioADC_VcomConfig(1);//MicBias en
		//模锟斤拷通为NONE止锟较达拷通然锟斤拷要锟斤拷模锟斤拷通锟斤拷

		AudioADC_DynamicElementMatch(ADC1_MODULE, TRUE, TRUE);

		AudioADC_PGASel(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2);
		AudioADC_PGASel(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1);

		AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_RIGHT, LINEIN3_RIGHT_OR_MIC2, 15, 4);
		AudioADC_PGAGainSet(ADC1_MODULE, CHANNEL_LEFT, LINEIN3_LEFT_OR_MIC1, 15, 4);
		AudioDAC_DoutModeSet(AUDIO_DAC1, MODE2, WIDTH_16_BIT);

		AudioADC_DigitalInit(ADC1_MODULE, SampleRate, (void*)AudioADC2Buf, sizeof(AudioADC2Buf));


		gCtrlVars.sample_rate = SampleRate;
}

int compareInt16Arrays(const int16_t *array1, const int16_t *array2, size_t size) {
	size_t i;
	for (i = 0; i < size; ++i) {
        if (array1[i] != array2[i]) {
            return 0; // 只要发现一个不相等的元素，立即返回0
        }
    }
    return 1; // 所有元素都相等
}

void HardWareTask(void)
{
	//uint16_t* range;
	BG_encoder.button_init();

//	BG_encoder.set_range(0,50,LIMIT_MODE);
//	BG_encoder.set_value(25);

	//enter 按键初始化，上拉输入。
	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX23);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX23);
	GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX23);
	GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX23);

	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX24);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX24);
	GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX24);
	GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX24);

	while(1){
		// 注释掉冲突的按键检测，使用EffectTask中的loop按键处理
		/*
		if(!BG_encoder.enter()){
				DelayMs(50);
				if(!BG_encoder.enter()){

					play_flag = 1;
					sectorAddress = 0;
					DBG("record is %d",record_flag);
				}
			}
		*/
		
		// 让出CPU时间给其他任务
		DelayMs(100);
	}
}

void SerialTask(){

	uint8_t Buf[200];
	uint8_t len = 0;

	while(1){
		if((UART1_IOCtl(UART_IOCTL_RXSTAT_GET, 1) & 0x01))//接收的频次变稀，则可能会错过数据
					{
					     len = UART1_Recv( Buf,10,100);//TimeOut增大，一次最长接收的长度则变长
					     UART1_IOCtl( UART_IOCTL_RXINT_CLR, 1);
					     if(len > 0)//如果接收到数据
					     {
					    	 UART1_Send(Buf,len,20);//把接收到的数据打印出来
					     }
					}

	}


}

uint32_t record_time = 0;

void EffectTask(){

	uint16_t  RealLen;
	uint16_t  n;
	uint16_t  i;
	uint8_t last_value;
	uint8_t Buffer[200];
	audio_Init(48000);
	CtrlVarsInit();
	spi_init();
	GPIO_RegOneBitClear(GPIO_A_IE, GPIOA24);
	GPIO_RegOneBitSet(GPIO_A_OE, GPIOA24);
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA24);
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA24);

	GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX1);
	GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX1);
	GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX1);
	GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX1);

	BG_lcd.Init();
	BG_lcd.Clear(RED);
	BG_lcd.Clear(0x00);
	BGUI_tool.DrawPoint(64,80,0xFFFF);
	BGUI_tool.DrawPoint(1,1,0xFFFF);
	BGUI_tool.ShowImage(16, 16, 40, 40, gImage_qq);
	//BG_page  = BG_Page_Init(table,MAX_PAGE);
	BG_encoder.button_init();

	BG_encoder.set_range(0,50,LIMIT_MODE);
	BG_encoder.set_value(25);

	//AudioEffectsInit();
	gCtrlVars.audio_effect_init_flag =1;
	gCtrlVars.echo_unit.enable = 1;
	gCtrlVars.reverb_unit.enable = 1;
	//AudioEffectEchoInit(&gCtrlVars.echo_unit,  2, 48000);

	AudioEffectReverbInit(&gCtrlVars.reverb_unit,  2, 48000);
	
	play_flag = 0;
	BG_flash_manager.Init();
	//BG_flash_manager.EraseAll(DEV_NOR);

	// 初始化loop管理器
	loop_init();
	loop_set_flash_type(FLASH_TYPE_NAND);
	DBG("Loop manager is ready\n");
	while(1){
//

			if(!BG_encoder.enter()){
				// 使用loop函数处理按键
//				loop_reset();
//				BG_flash_manager.EraseAll(DEV_NOR);
				if(key_flag==1){
				loop_handle_button_press();
				key_flag=0;

			}
		}
//
//		if(GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX1)==1 ){
//			DelayMs(100);
//			if(GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX1)==1 ){
//				// 使用loop函数处理按键
//				loop_handle_button_press();
//			}
//		}

		if(AudioADC_DataLenGet(ADC0_MODULE) >= 48)
			{
				n = AudioDAC_DataSpaceLenGet(DAC0);
				RealLen = AudioADC_DataGet(ADC1_MODULE, PcmBuf1,48);

				if(n > RealLen)
				{
					n = RealLen;
				}
				RealLen = AudioADC_DataGet(ADC0_MODULE, PcmBuf2,48);
				if(n > RealLen)
				{
					n = RealLen;
				}
				//AudioEffectEchoApply(&gCtrlVars.echo_unit, PcmBuf1, PcmBuf2, n);
				//memset(PcmBuf3,0x00,48);
				// AudioEffectReverbApply期望int16_t*参数，而我们的缓冲区是uint32_t*
				// 需要正确转换：n个uint32_t = n*2个int16_t样本
				AudioEffectReverbApply(&gCtrlVars.reverb_unit, PcmBuf1, PcmBuf3, n);
				
				// 处理录制（录制混响后的麦克风数据）
				// 使用uint32_t格式录制，保持原始数据格式
				loop_process_recording_uint32(PcmBuf3, Buffer, n);

				// 初始化输出缓冲区为混响后的麦克风信号
				memcpy((void*)PcmBuf4, (void*)PcmBuf3, n * sizeof(uint32_t));
				
				// 处理播放（将录音内容混合到输出缓冲区）
				// 使用uint32_t格式播放
				loop_process_playback_uint32(PcmBuf4, Buffer, n);

				// 最终混合：(混响麦克风 + 播放内容) + Line输入
				// 直接在uint32_t级别进行混合，保持原始32位格式
				for(i = 0; i < n; i++) {
					// 分别提取左右声道进行混合
					int16_t mic_left = (int16_t)(PcmBuf3[i] & 0xFFFF);
					int16_t mic_right = (int16_t)((PcmBuf3[i] >> 16) & 0xFFFF);
					int16_t playback_left = (int16_t)(PcmBuf4[i] & 0xFFFF);
					int16_t playback_right = (int16_t)((PcmBuf4[i] >> 16) & 0xFFFF);
					int16_t line_left = (int16_t)(PcmBuf2[i] & 0xFFFF);
					int16_t line_right = (int16_t)((PcmBuf2[i] >> 16) & 0xFFFF);

					// 混合左右声道
					int32_t mixed_left = (int32_t)mic_left + (int32_t)playback_left + (int32_t)line_left;
					int32_t mixed_right = (int32_t)mic_right + (int32_t)playback_right + (int32_t)line_right;

					// 重新组合为uint32_t
					PcmBuf4[i] = ((uint32_t)__nds32__clips(mixed_right, 15) << 16) |
					             ((uint32_t)__nds32__clips(mixed_left, 15) & 0xFFFF);
				}
				AudioDAC_DataSet(DAC0, PcmBuf4, n);

			}

	}
}
void prvInitialiseHeap(void);
int main(void)
{
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


	GPIO_PortAModeSet(GPIOA9, 1);//Rx, A24:uart1_rxd_0
	GPIO_PortAModeSet(GPIOA10, 3);//Tx, A25:uart1_txd_0
	DbgUartInit(1, 115200, 8, 0, 1);

	Clock_USBClkDivSet(4);
	Clock_USBClkSelect(APLL_CLK_MODE);

	Remap_InitTcm(0, 12);
	SpiFlashInit(80000000, MODE_4BIT, 0, 1);
	DMA_ChannelAllocTableSet(DmaChannelMap);

	GIE_ENABLE();
//	SysTickInit();
	Timer_Config(TIMER2,1000,0);
 	Timer_Start(TIMER2);
 	NVIC_EnableIRQ(Timer2_IRQn);

	DBG("****************************************************************\n");
	DBG("                          BG_CARD SDK                           \n");
	DBG("****************************************************************\n");

	prvInitialiseHeap();

	NVIC_EnableIRQ(SWI_IRQn);

	xQueue = xQueueCreate( 4, sizeof(uint32_t) );

	//xTaskCreate( (TaskFunction_t)SendTask, "SendMsgTask", 512, NULL, 1, NULL );

	//xTaskCreate( (TaskFunction_t)RecvTask, "RecvMsgTask", 512, NULL, 2, NULL );

	//xTaskCreate( (TaskFunction_t)AudioTask, "AudioTask", 512, NULL, 2, NULL );

	//xTaskCreate( (TaskFunction_t)RecordTask, "RecordTask", 1024, NULL, 1, NULL );

//	xTaskCreate( (TaskFunction_t)HardWareTask, "HardWareTask", 512, NULL, 2, NULL );

	//xTaskCreate( (TaskFunction_t)FlashTask, "FlashTask", 512, NULL, 1, NULL );
	//xTaskCreate( (TaskFunction_t)USBTask, "USBTask", 512, NULL, 1, NULL );
	xTaskCreate( (TaskFunction_t)EffectTask, "EffectTask",2048, NULL, 1, NULL );

	//xTaskCreate( (TaskFunction_t)SerialTask, "SerialTask",2048, NULL, 1, NULL );

	vTaskStartScheduler();

	while(1);
}


