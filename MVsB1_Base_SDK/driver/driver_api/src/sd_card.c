/**
 *****************************************************************************
 * @file     sdio_card1.c
 * @author   owen
 * @version  V1.0.0
 * @date     2017-05-15
 * @brief    sd card module driver interface
 *****************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT 2013 MVSilicon </center></h2>
 */
#include <string.h>
#include "type.h"
#include "debug.h"
#include "delay.h"
#include "timeout.h"
#include "sdio.h"
#include "sd_card.h"
#include "dma.h"
#include "core_d1088.h"
#include "gpio.h"

#ifdef FUNC_OS_EN
#include "rtos_api.h" //add for mutex declare

osMutexId SDIOMutex = NULL;
#endif

/**<����״̬ */
#define SD_STATUS_ACCEPTED				0x02
#define SD_STATUS_CRC_ERROR				0x05
#define SD_STATUS_WRITE_ERROR			0x06

/**<������*/
#define	MC_MCDMAS_MMC					1
#define	MC_MCDMAS_SD					2

#define	SD_BLOCK_SIZE					512             /**<���С�̶�Ϊ512�ֽ�*/

/**<��״̬����*/
#define	CURRENT_STATE_IDLE				0
#define	CURRENT_STATE_READY				1
#define	CURRENT_STATE_IDENT				2              /**<identification*/
#define	CURRENT_STATE_STBY				3
#define	CURRENT_STATE_TRAN				4
#define	CURRENT_STATE_DATA				5              /**<sending data*/
#define	CURRENT_STATE_RCV				6
#define	CURRENT_STATE_PRG				7
#define	CURRENT_STATE_DIS				8              /**<disconnect*/

#define	VOLTAGE_WINDOWS					0x40FF8000     /**<2.7v~3.6v*/
#define VOLTAGE_PATTERN                 0x000001AA     /**<0x01:2.7-3.6V,0xAA:check pattern,pattern can use any 8-bit*/

#define CARD_DBG	printf

SD_CARD		    SDCard;
SD_CARD_ID 		SDCardId;
void CardPortInit(uint8_t SdioPort)
{
	switch(SdioPort)
	{
		case SDIO_A15_A16_A17:
			GPIO_RegOneBitClear(GPIO_A_PD, GPIOA15);
			GPIO_RegOneBitClear(GPIO_A_PD, GPIOA16);
			GPIO_RegOneBitClear(GPIO_A_PD, GPIOA17);
			GPIO_RegOneBitSet(GPIO_A_PU, GPIOA15);
			GPIO_RegOneBitSet(GPIO_A_PU, GPIOA16);
			GPIO_RegOneBitSet(GPIO_A_PU, GPIOA17);
			GPIO_PortAModeSet(GPIOA15, 1);//DAT
			GPIO_PortAModeSet(GPIOA16, 2);//CLK
			GPIO_PortAModeSet(GPIOA17, 1);//CMD
			break;

		case SDIO_A20_A21_A22:
			GPIO_RegOneBitClear(GPIO_A_PD, GPIOA20);
			GPIO_RegOneBitClear(GPIO_A_PD, GPIOA21);
			GPIO_RegOneBitClear(GPIO_A_PD, GPIOA22);
			GPIO_RegOneBitSet(GPIO_A_PU, GPIOA20);
			GPIO_RegOneBitSet(GPIO_A_PU, GPIOA21);
			GPIO_RegOneBitSet(GPIO_A_PU, GPIOA22);
			GPIO_PortAModeSet(GPIOA20, 1 );//DAT
			GPIO_PortAModeSet(GPIOA21, 11);//CLK
			GPIO_PortAModeSet(GPIOA22, 1 );//CMD

			break;

		default:
			GPIO_RegOneBitClear(GPIO_A_PD, GPIOA15);
			GPIO_RegOneBitClear(GPIO_A_PD, GPIOA16);
			GPIO_RegOneBitClear(GPIO_A_PD, GPIOA17);
			GPIO_RegOneBitSet(GPIO_A_PU, GPIOA15);
			GPIO_RegOneBitSet(GPIO_A_PU, GPIOA16);
			GPIO_RegOneBitSet(GPIO_A_PU, GPIOA17);
			GPIO_PortAModeSet(GPIOA15, 1);//DAT
			GPIO_PortAModeSet(GPIOA16, 2);//CLK
			GPIO_PortAModeSet(GPIOA17, 1);//CMD
			break;
	}
}

void SDCardDeinit(uint8_t SdioPort)
{
	switch(SdioPort)
	{
		case SDIO_A15_A16_A17:

			GPIO_RegOneBitClear(GPIO_A_PU, GPIOA15);
			GPIO_RegOneBitClear(GPIO_A_PU, GPIOA16);
			GPIO_RegOneBitClear(GPIO_A_PU, GPIOA17);
			GPIO_PortAModeSet(GPIOA15, 0);//DAT
			GPIO_PortAModeSet(GPIOA16, 0);//CLK
			GPIO_PortAModeSet(GPIOA17, 0);//CMD
			break;

		case SDIO_A20_A21_A22:
			GPIO_RegOneBitClear(GPIO_A_PU, GPIOA20);
			GPIO_RegOneBitClear(GPIO_A_PU, GPIOA21);
			GPIO_RegOneBitClear(GPIO_A_PU, GPIOA22);
			GPIO_PortAModeSet(GPIOA20, 0);//DAT
			GPIO_PortAModeSet(GPIOA21, 0);//CLK
			GPIO_PortAModeSet(GPIOA22, 0);//CMD
	}
	SDIO_ClkDisable();

}

/**
 * @brief  ��ʼ��SDIO������
 * @param  NONE
 * @return NONE
 * @note   ���ｫ��дģʽĬ�ϳ�ʼ��Ϊ������д��ʽ
 */
void SDCard_ControllerInit(void)
{
	SDCard.RCA = 0;
	SDCard.IsSDHC = FALSE;
#ifdef FUNC_OS_EN
	if(SDIOMutex == NULL)
		SDIOMutex = osMutexCreate();
#endif
	SDIO_SysToSdioDivSet(1);
	SDIO_Init();
	SDIO_ByteModeEnable();
	SDCard.CardInit = SD_CONTROLER_INIT;
}
uint32_t volatile  cmd_timeout_count=0;
extern uint32_t gSysTick;

/**
 * @brief  SDIO��������
 * @param  SDIO��Ҫ���͵�������
 * @param  ��������Ĳ���
 * @return ��������ɹ�����NO_ERR�����󷵻�SEND_CMD_TIME_OUT_ERR
 * @note
 */
bool SDIO_CmdSend(uint8_t Cmd, uint32_t Param, uint16_t TimeOut)
{
	TIMER timer;
	SDIO_CmdStart(Cmd,Param);
	TimeOutSet(&timer, TimeOut);
	while(!SDIO_CmdIsDone())
	{
		if(Cmd == 0 || Cmd == 4 || Cmd == 15)
		{
	#ifdef FUNC_OS_EN
		vTaskDelay(1);
	#else
		__udelay(1000);
	#endif
		}
		if(IsTimeOut(&timer))
		{
			SDIO_CmdStop();
			return SEND_CMD_TIME_OUT_ERR;
		}
	#ifdef FUNC_OS_EN
		vTaskDelay(1);
	#else
		__udelay(1000);
	#endif
	}
	SDIO_CmdStop();
	return NO_ERR;
}



static SD_CARD_ERR_CODE SDCard_SendAppCommand(uint8_t Cmd, uint32_t Param)
{
	if(SDIO_CmdSend(CMD55_APP_CMD, SDCard.RCA, 20) == NO_ERR)
	{
		DelayUs(5);// > 1us between two cmd  by pi 2017.12.21
		if(SDIO_CmdSend(Cmd, Param, 20) == NO_ERR)
		{
			return NONE_ERR;
		}
		return CMD_SEND_TIME_OUT_ERR;
	}
	return CMD55_SEND_ERR;
}


bool SDCard_Ocr_Get(void)
{
	uint8_t  resp[4];
	SDIO_CmdRespGet(resp, SDIO_RESP_TYPE_R3);

	if(*(uint32_t *)resp & 0x80000000)//ocr register bit definition, bit31-Card power up status bit (busy) bit30-Card Capacity Status (CCS)
	{
		if(*(uint32_t *)resp & 0x40000000)//for SDHC
		{
			SDCard.IsSDHC = TRUE;
		}
		if(SDCard.CardInit == SD_IDLE)
		{
			SDCard.CardInit = SD_READY;
		}
		return TRUE;
	}
	return FALSE;
}

/**
 * @brief  SD�����
 * @param  NONE
 * @return NONE
 * @note
 */
SD_CARD_ERR_CODE SDCard_Detect(void)
{

	if(SDCard.CardInit == SD_INITED)//���ѳ�ʼ��
	{
		if(SDIO_CmdSend(CMD13_SEND_STATUS, SDCard.RCA, 20)  == NO_ERR)              	//read card status
		{
			return NONE_ERR;
		}
		else
		{
			return CMD13_SEND_ERR;
		}
	}
	else//��δ��ʼ��
	{
		if(SDCard.CardInit < SD_CONTROLER_INIT)
		{
			SDCard_ControllerInit();
		}
		SDIO_CmdSend(CMD0_GO_IDLE_STATE, 0, 20);
		SDCard.CardInit = SD_IDLE;
		SDIO_CmdSend(CMD8_SEND_IF_COND, VOLTAGE_PATTERN, 20);
		if(SDCard_SendAppCommand(ACMD41_SD_SEND_OP_COND, VOLTAGE_WINDOWS) == NONE_ERR)
		{
			DBG("SD link!\n");
			SDCard.CardType = MC_MCDMAS_SD;
			SDCard_Ocr_Get();
			return NONE_ERR;
		}

		//check mmc card
		SDIO_CmdSend(CMD0_GO_IDLE_STATE, 0, 20);
		SDCard.CardInit = SD_IDLE;
		SDIO_CmdSend(CMD8_SEND_IF_COND, VOLTAGE_PATTERN, 20);
		if(SDIO_CmdSend(CMD1_SEND_OP_COND, VOLTAGE_WINDOWS, 20) ==NO_ERR)
		{
			DBG("MMC link!\n");
			SDCard.CardType = MC_MCDMAS_MMC;
			SDCard_Ocr_Get();
			return NONE_ERR;
		}
		return NOCARD_LINK_ERR;
	}
}

SD_CARD_ERR_CODE SDCard_Identify(void)
{
	TIMER Timer;
	uint8_t  resp[16];
	uint8_t  i;

	//���õ�ѹ����
	TimeOutSet(&Timer, 2000);
	while(SDCard.CardInit == SD_IDLE)
	{
		DelayUs(10);//��������ʱ
		if(SDCard.CardType == MC_MCDMAS_SD)// && !Com41Send)
		{
			if(SDCard_SendAppCommand(ACMD41_SD_SEND_OP_COND, VOLTAGE_WINDOWS))
			{
				return ACMD41_SEND_ERR;
			}
		}
		if(SDCard.CardType == MC_MCDMAS_MMC)// && !Com41Send)
		{
			if(SDIO_CmdSend(CMD1_SEND_OP_COND, VOLTAGE_WINDOWS, 20))
			{
				return CMD1_SEND_ERR;
			}
		}

		SDCard_Ocr_Get();

		if(IsTimeOut(&Timer))
		{
			DBG("CMD_SEND_TIME_OUT_ERR\n");
			return CMD_SEND_TIME_OUT_ERR;
		}
	}
	DelayUs(10);
	if(SDCard.CardInit == SD_READY)
	{
		//��ȡCID(unique card identification)
		if(SDIO_CmdSend(CMD2_ALL_SEND_CID, 0, 20))      //success card will from ready switch to identification
		{
			DBG("SdCardInit() ERROR 005!\n");
			return CMD2_SEND_ERR;
		}
		SDIO_CmdRespGet(resp, SDIO_RESP_TYPE_R2);
		for(i = 0; i < 15; i++)
		{
			*((uint8_t *)(&(SDCardId)) + i + 1) = resp[14 - i];
		}
		SDCard.CardInit = SD_IDENT;
	}

	if(SDCard.CardInit >= SD_IDENT)
	{
		//��ȡ��Ե�ַ���������10��
		for(i = 0; i < 10; i++)
		{
			if(SDIO_CmdSend(CMD3_SEND_RELATIVE_ADDR, 0, 20) == NO_ERR)
			{
				SDIO_CmdRespGet(resp, SDIO_RESP_TYPE_R6);
				if(((*(uint32_t *)(resp)) & 0x00001E00) >> 9 == CURRENT_STATE_STBY)
				{
					//SDCard.RCA = *(uint32_t *)resp;
					SDCard.RCA = (*(uint32_t *)resp) & 0xFFFF0000;
					break;
				}
			}
		}
		if(SDCard.CardInit < SD_STANDBY)
		{
			SDCard.CardInit = SD_STANDBY;
		}
		DBG("RCA: %08lx\n", SDCard.RCA);
	}

	//��ȡCSD�Ĵ�����Card Specific Data�����������ֵ��Ʃ�磬�鳤�ȣ���������Ϣ�ȡ�
	if(SDIO_CmdSend(CMD9_SEND_CSD, SDCard.RCA, 20))
	{
		return CMD9_SEND_ERR;
	}
	SDIO_CmdRespGet(resp, SDIO_RESP_TYPE_R2);
	//����CSD����BlockNum
	if(resp[14])	//CSD V2
	{
		SDCard.BlockNum = (((resp[7] & 0x3F) << 16) + (resp[6] << 8) + resp[5] + 1) << 10;	// memory capacity = (C_SIZE+1) * 512KByte
	}
	else			//CSD V1
	{
		uint8_t exp;
		SDCard.BlockNum = ((resp[8] & 0x03) << 10) + (resp[7] << 2) + ((resp[6] & 0xC0) >> 6) + 1;

		exp = ((resp[5] & 0x03) << 1) + ((resp[4] & 0x80) >> 7) + 2 + (resp[9] & 0x0F) - 9;
		SDCard.BlockNum <<= exp;	// (C_SIZE + 1) * 2 ^ (C_SIZE_MULT + 2)
	}
	DBG("BlockNum:%lu\n", SDCard.BlockNum);
	//get card max transfer data speed
	i = (resp[11] >> 3) & 0x0F;

	DBG("resp:%X\n", resp[11]);
	if(i == 0 )
	{
		DBG("Csd error!\n");
	}
	if((resp[11] & 0x07) == 1)	//base 1M
	{
		if(i == 0x0F)//8M
		{
			SDCard.MaxTransSpeed = 0x02;	//Ƶ����Ϊ9MHz 36/4
		}
		else if(i > 0x08)//4.0M,4.5M,5.0M,5.5M,6.0M,7.0M
		{
			SDCard.MaxTransSpeed = 0x03;	//Ƶ����Ϊ4.5MHz 36/8
		}
		else if(i > 0x04)//2.0M,2.5M,3.0M,3.5M
		{
			SDCard.MaxTransSpeed = 0x04;	//Ƶ����Ϊ2.25MHz 36/16
		}
		else //1.0M,1.2M,1.3M,1.5M
		{
			SDCard.MaxTransSpeed = 0x05;	//Ƶ����Ϊ1.125MHz 36/32
		}
	}
	else if((resp[11] & 0x07) == 2)	//base 10M
	{
		if(i < 0x04)//10M, 12M, 13M
		{
			SDCard.MaxTransSpeed = 0x02;	//10M,Ƶ����Ϊ9MHz  36/4
		}
		else if(i < 0x07)//15M,20M,25M
		{
			SDCard.MaxTransSpeed = 0x01;	//Ƶ����Ϊ18MHz  36/2
		}
		else
		{
			SDCard.MaxTransSpeed = 0x00;	//Ƶ����Ϊ36MHz
		}
	}
	else
	{
		DBG("Csd max speed error!\n");
	}

	DBG("SDCard.MaxTransSpeed:%d\n", SDCard.MaxTransSpeed);
	//����transfer state
	if(SDIO_CmdSend(CMD7_SELECT_DESELECT_CARD, SDCard.RCA, 20))
	{
		DBG("CMD7_SELECT_DESELECT_CARD\n");
		return CMD7_SEND_ERR;
	}
	DelayUs(5);
	//��ȡ״̬
	if(SDIO_CmdSend(CMD13_SEND_STATUS, SDCard.RCA, 20))
	{
		DBG("CMD13_SEND_STATUS\n");
		return CMD13_SEND_ERR;
	}
	SDIO_CmdRespGet(resp, SDIO_RESP_TYPE_R1);
	if(((*(uint32_t *)(resp)) & 0x00001E00) >> 9  != CURRENT_STATE_TRAN)
	{
		DBG("CMD13_SEND_STATUS SDIO_RESP_TYPE_R1\n");
		return GET_RESPONSE_STATUS_ERR;
	}
	//���ÿ鳤��
	if(SDIO_CmdSend(CMD16_SET_BLOCKLEN, SD_BLOCK_SIZE, 20))
	{
		DBG("NONSPPORT CMD16_SET_BLOCKLEN  \n");
	}
	SDCard.CardInit = SD_INITED;
	return NONE_ERR;
}

SD_CARD_ERR_CODE SDCard_Init(void)
{
	uint8_t Retry = 4;
#ifdef FUNC_OS_EN
	osMutexLock(SDIOMutex);
#endif
	SDCard_ControllerInit();

	//��������SD�������Ծ��飬���뿨����ʱһ��ʱ��
#ifdef FUNC_OS_EN
	vTaskDelay(150);
#else
	WaitMs(150);
#endif
	while(1)
	{
		SDCard_Detect();//��Ҫ��̽���Ƿ��п�����
		if(SDCard_Identify() == NONE_ERR)
		{
			DBG("CardGetInfo OK\n");
			break;
		}
		if(!(--Retry))
		{
			DBG("retry 4 times was still failed\n");
#ifdef FUNC_OS_EN
			osMutexUnlock(SDIOMutex);
#endif
			return GET_SD_CARD_INFO_ERR;
		}
	}

	SDIO_ClkSet(SDCard.MaxTransSpeed);
	SDIO_ClkEnable();
#ifdef FUNC_OS_EN
	osMutexUnlock(SDIOMutex);
#endif
	return NONE_ERR;
}



SD_CARD_ERR_CODE SDCard_ReadBlock(uint32_t Block, uint8_t* Buffer,uint8_t Size)
{
	TIMER Timer;
	SD_CARD_ERR_CODE ret = NONE_ERR;
	
	if(Block > SDCard.BlockNum)
	{
		return BLOCK_NUM_EXCEED_BOUNDARY;
	}
#ifdef FUNC_OS_EN
		osMutexLock(SDIOMutex);
#endif
	SDIO_ClkEnable();
	SDIO_ClearClkHalt();
	TimeOutSet(&Timer,50);
	while(SDIO_IsDataLineBusy() && !IsTimeOut(&Timer));
	if(!SDCard.IsSDHC)
	{
		Block *= SD_BLOCK_SIZE;
	}
	TimeOutSet(&Timer, 350 + 20 * Size);  //ĳ�ſ�1bit Nac��370ms��Ϊ���ݣ�������ޡ�
	if(Size == 1)
	{
		SDIO_ClearClkHalt();
		DMA_ChannelDisable(PERIPHERAL_ID_SDIO_RX);
		DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT);
		DMA_BlockConfig(PERIPHERAL_ID_SDIO_RX);
		DMA_BlockBufSet(PERIPHERAL_ID_SDIO_RX, Buffer, SD_BLOCK_SIZE);
		DMA_ChannelEnable(PERIPHERAL_ID_SDIO_RX);
		SDIO_SingleBlockConfig(SDIO_DIR_RX,SD_BLOCK_SIZE);
		SDIO_DataTransfer(1);
		SDIO_CmdSend(CMD17_READ_SINGLE_BLOCK, Block, 20);
		while(1)
		{
			if(DMA_InterruptFlagGet(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT)&&SDIO_DataIsDone())
			{
				DMA_ChannelDisable(PERIPHERAL_ID_SDIO_RX);
				DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT);
				/* Invalidate D-Cache for DMA buffer (cache-line aligned) */
				{
					uint32_t _a = (uint32_t)Buffer & ~0xF;
					uint32_t _e = (((uint32_t)Buffer + SD_BLOCK_SIZE) + 0xF) & ~0xF;
					DataCacheRangeInvalid(_a, _e - _a);
				}
				break;
			}
			if(IsTimeOut(&Timer))
			{
				DMA_ChannelDisable(PERIPHERAL_ID_SDIO_RX);
				DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT);
				SDIO_DataTransfer(0);
			#ifdef FUNC_OS_EN
					osMutexUnlock(SDIOMutex);
			#endif
					DBG("1 sector read timeout\n");
					return READ_SD_CARD_TIME_OUT_ERR;
			}
		#ifdef FUNC_OS_EN
			vTaskDelay(1);
		#endif
		}
		SDIO_DataTransfer(0);
		SDIO_ClkDisable();
	}
	else
	{
		SDIO_ClearClkHalt();
		DMA_ChannelDisable(PERIPHERAL_ID_SDIO_RX);
		DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT);
		DMA_BlockConfig(PERIPHERAL_ID_SDIO_RX);
		DMA_BlockBufSet(PERIPHERAL_ID_SDIO_RX, Buffer, SD_BLOCK_SIZE*Size);
		DMA_ChannelEnable(PERIPHERAL_ID_SDIO_RX);
		SDIO_MultiBlockConfig(SDIO_DIR_RX,SD_BLOCK_SIZE,Size);
		SDIO_DataTransfer(1);
		SDIO_CmdSend(CMD18_READ_MULTIPLE_BLOCK, Block, 20);
		while(1)
		{
			if(DMA_InterruptFlagGet(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT)&&SDIO_MultiBlockTransferDone())
			{
				DMA_ChannelDisable(PERIPHERAL_ID_SDIO_RX);
				DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT);
				/* Invalidate D-Cache for DMA buffer (cache-line aligned) */
				{
					uint32_t _a = (uint32_t)Buffer & ~0xF;
					uint32_t _e = (((uint32_t)Buffer + SD_BLOCK_SIZE * Size) + 0xF) & ~0xF;
					DataCacheRangeInvalid(_a, _e - _a);
				}
				break;
			}
			if(IsTimeOut(&Timer))
			{
				SDIO_ClkEnable();
				SDIO_CmdDoneCheckBusy(1);
				SDIO_CmdSend(CMD12_STOP_TRANSMISSION, 0, 300);
				SDIO_CmdDoneCheckBusy(0);
				SDIO_DataTransfer(0);
				DMA_ChannelDisable(PERIPHERAL_ID_SDIO_RX);
				DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT);
			#ifdef FUNC_OS_EN
				osMutexUnlock(SDIOMutex);
			#endif
				DBG("%d sectors read timeout\n", Size);
				return READ_SD_CARD_TIME_OUT_ERR;
			}
		#ifdef FUNC_OS_EN
			vTaskDelay(1);
		#endif
		}
		SDIO_ClkEnable();
		SDIO_CmdDoneCheckBusy(1);
		SDIO_CmdSend(CMD12_STOP_TRANSMISSION, 0, 300);
		SDIO_CmdDoneCheckBusy(0);
		SDIO_DataTransfer(0);
		SDIO_ClkDisable();
	}
#ifdef FUNC_OS_EN
	osMutexUnlock(SDIOMutex);
#endif
	return ret;
}





SD_CARD_ERR_CODE SDCard_WriteBlock(uint32_t Block, uint8_t* Buffer, uint8_t Size)
{
	TIMER Timer;
	SD_CARD_ERR_CODE ret = NONE_ERR;
	

	if(Block > SDCard.BlockNum)
	{
		return BLOCK_NUM_EXCEED_BOUNDARY;
	}
#ifdef FUNC_OS_EN
	osMutexLock(SDIOMutex);
#endif

	if(!SDCard.IsSDHC)
	{
		Block *= SD_BLOCK_SIZE;
	}
	SDIO_ClkEnable();
	TimeOutSet(&Timer,50);
	while(SDIO_IsDataLineBusy() && !IsTimeOut(&Timer));
	if(SDIO_IsDataLineBusy())
	{
		ret = WRITE_SD_CARD_TIME_OUT_ERR;
	}
	else
	{
		if(Size == 1)
		{
			SDIO_ClearClkHalt();
			/* Invalidate D-Cache before DMA reads from memory (WRITE_THROUGH mode
			 * ensures CPU writes already reached memory, but invalidate to be safe) */
			{
				uint32_t _a = (uint32_t)Buffer & ~0xF;
				uint32_t _e = (((uint32_t)Buffer + SD_BLOCK_SIZE) + 0xF) & ~0xF;
				DataCacheRangeInvalid(_a, _e - _a);
			}
			SDIO_CmdSend(CMD24_WRITE_BLOCK, Block, 50);
			DMA_ChannelDisable(PERIPHERAL_ID_SDIO_TX);
			DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT);
			DMA_BlockConfig(PERIPHERAL_ID_SDIO_TX);
			DMA_BlockBufSet(PERIPHERAL_ID_SDIO_TX, (uint8_t*)(Buffer), SD_BLOCK_SIZE);
			DMA_ChannelEnable(PERIPHERAL_ID_SDIO_TX);
			SDIO_SingleBlockConfig(SDIO_DIR_TX,SD_BLOCK_SIZE);
			SDIO_DataTransfer(1);
			TimeOutSet(&Timer,300);
			while(1)
			{
				if(DMA_InterruptFlagGet(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT)&&SDIO_DataIsDone())
				{
					DMA_ChannelDisable(PERIPHERAL_ID_SDIO_TX);
					DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT);
					break;
				}
				if(IsTimeOut(&Timer))
				{
					SDIO_DataTransfer(0);
					SDIO_ClkDisable();
					DMA_ChannelDisable(PERIPHERAL_ID_SDIO_TX);
					DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT);
				#ifdef FUNC_OS_EN
					osMutexUnlock(SDIOMutex);
				#endif
					DBG("1 sector write timeout\n");
					return WRITE_SD_CARD_TIME_OUT_ERR;
				}
			#ifdef FUNC_OS_EN
				vTaskDelay(1);
			#endif
			}
			SDIO_DataTransfer(0);
			SDIO_ClkDisable();
		#ifdef FUNC_OS_EN
			osMutexUnlock(SDIOMutex);
		#endif
			return ret;
		}
		else
		{
			SDIO_ClearClkHalt();
			/* Invalidate D-Cache before DMA reads from memory */
			{
				uint32_t _a = (uint32_t)Buffer & ~0xF;
				uint32_t _e = (((uint32_t)Buffer + SD_BLOCK_SIZE * Size) + 0xF) & ~0xF;
				DataCacheRangeInvalid(_a, _e - _a);
			}
			ret = SDIO_CmdSend(CMD25_WRITE_MULTIPLE_BLOCK, Block, 50);
			DMA_ChannelDisable(PERIPHERAL_ID_SDIO_TX);
			DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT);
			DMA_BlockConfig(PERIPHERAL_ID_SDIO_TX);
			DMA_BlockBufSet(PERIPHERAL_ID_SDIO_TX, (uint8_t*)(Buffer), SD_BLOCK_SIZE*Size);
			DMA_ChannelEnable(PERIPHERAL_ID_SDIO_TX);
			SDIO_MultiBlockConfig(SDIO_DIR_TX,SD_BLOCK_SIZE,Size);
			SDIO_DataTransfer(1);
			TimeOutSet(&Timer,250 + 50 * Size);
			while(1)
			{
				if((DMA_InterruptFlagGet(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT))&&(SDIO_MultiBlockTransferDone()))
				{
					DMA_ChannelDisable(PERIPHERAL_ID_SDIO_TX);
					DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT);
					break;
				}
				if(IsTimeOut(&Timer))
				{
					SDIO_ClearClkHalt();
					SDIO_CmdDoneCheckBusy(1);
					SDIO_CmdSend(CMD12_STOP_TRANSMISSION, 0, 300);
					SDIO_CmdDoneCheckBusy(0);
					SDIO_DataTransfer(0);
					SDIO_ClkDisable();
					DMA_ChannelDisable(PERIPHERAL_ID_SDIO_TX);
					DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT);
				#ifdef FUNC_OS_EN
					osMutexUnlock(SDIOMutex);
				#endif
					DBG("%d sectors write timeout\n", Size);
					return WRITE_SD_CARD_TIME_OUT_ERR;
				}
			#ifdef FUNC_OS_EN
					vTaskDelay(1);
			#endif
			}
			SDIO_ClearClkHalt();
			SDIO_CmdDoneCheckBusy(1);
			ret=SDIO_CmdSend(CMD12_STOP_TRANSMISSION, 0, 300);
			SDIO_CmdDoneCheckBusy(0);
			SDIO_DataTransfer(0);
			SDIO_ClkDisable();
		}
	}
#ifdef FUNC_OS_EN
		osMutexUnlock(SDIOMutex);
#endif
	return ret;
}

SD_CARD* SDCard_GetCardInfo()
{
	return &SDCard;
}

uint32_t SDCard_CapacityGet(void)
{
	return SDCard.BlockNum;
}

