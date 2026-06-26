/**
 *****************************************************************************
 * @file     device_stor.c
 * @author   Owen
 * @version  V1.0.0
 * @date     7-September-2015
 * @brief    device mass-storage module driver interface
 *****************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT 2013 MVSilicon </center></h2>
 */

#include <string.h>
#include "type.h"
#include "otg_device_hcd.h"
#include "sd_card.h"
#include "timeout.h"
#include "debug.h"
#include "delay.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "sd_card.h"
#include "sdio.h"
#include "dma.h"
#include "gpio.h"
#include "reset.h"
#include "hal_sdio.h"
#include "product_def.h"
#ifdef FUNC_OS_EN
#include "rtos_api.h"
#endif


#define TEST_UNIT_READY				0x00
#define REQUEST_SENSE				0x03
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
#define FORMAT_UNIT					0x04
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
#define READ_6						0x08
#define WRITE_6						0x0A
#define SEEK_6						0x0B
#define INQUIRY						0x12
#define MODE_SELECT					0x15
#define MODE_SENSE					0x1A
#define START_STOP_UNIT				0x1B
#define ALLOW_MEDIUM_REMOVAL		0x1E
#define	READ_FORMAT_CAPACITY		0x23
#define READ_CAPACITY				0x25
#define READ_10						0x28
#define WRITE_10					0x2A
#define SEEK_10						0x2B
#define VERYFY						0x2F
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
#define SYNCHRONIZE_CACHE			0x35
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
#define MODE_SELECT_10				0x55
#define MODE_SENSE_10				0x5A
#define READ_12						0xA8
#define WRITE_12					0xAA
#define READ_DISC_STRUCTURE			0xAD
#define DVD_MACHANISM_STATUS  		0xBD
#define REPORT_KEY			  		0xA4
#define SEND_KEY			  		0xA3
#define READ_TOC			  		0x43
#define READ_MSF			  		0xB9



#define READER_UNREADY				0
#define READER_INIT					1
#define READER_READY				2
#define READER_READ					3
#define READER_WIRTE				4
#define READER_INQUIRY				5
#define READER_READ_FORMAT_CAPACITY 6
#define READER_READ_CAPACITY		7
#define READER_NOT_ALLOW_REMOVAL	8
#define READER_ALLOW_REMOVAL		9


#define	DEVICE_STOR_BLOCK_SIZE	512

extern void OTG_DeviceRequestProcess(void);
extern SD_CARD		    SDCard;

uint32_t CARD_BUF_A[512/4];
uint32_t CARD_BUF_B[512/4];


uint8_t    gInquiryData[] =
{
	0x00,
	0x80,
	0x02,
	0x02,
	0x1F,
	0x00,
	0x00,
	0x00,
	'B', 'a', 'n', 'D', 0, 0, 0, 0,
	'D', 'a', 't', 'a', 'H', 'u', 'b', ' ', 'S', 'D', ' ', '0', 0, 0, 0, 0,
	'V', '1', '.', '0'
};

uint8_t gFmtCapacityData[] =
{
	0x00, 0x00, 0x00, 0x08,
	0x00, 0x00, 0x00, 0x00,
	0x03,
	0x00,
	(uint8_t)(DEVICE_STOR_BLOCK_SIZE >> 8),
	(uint8_t)DEVICE_STOR_BLOCK_SIZE
};

uint8_t gCapacityData[] =
{
	0x00, 0x00, 0x00, 0x00,
	0x00,
	0x00,
	(uint8_t)(DEVICE_STOR_BLOCK_SIZE >> 8),
	(uint8_t)DEVICE_STOR_BLOCK_SIZE
};


const uint8_t gModeSenseProtectPage[] = {0x03, 0x00, 0x00, 0x00};

uint8_t	gModeSenseAllPage[] =
{
	0x0B,
	0x00,
	0x00,
	0x08,
	0x00,
	0x02, 0x00, 0x00,
	0x00,
	0x00, 0x02, 0x00,
};

uint8_t gModeSenseCachingPage[] =
{
	0x0F, 0x00, 0x00, 0x00,
	0x08, 0x0A, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

const uint8_t gRequestSenseNotReady[] =
{
	0x70, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0A,
	0x00, 0x00, 0x00, 0x00, 0x3A, 0x00, 0x00, 0x00,
	0x00, 0x00
};

const uint8_t gRequestSenseReady[] =
{
	0x70, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x0A,
	0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00,
	0x00, 0x00
};

uint8_t gCBW[64];

uint8_t gCSW[] = {'U', 'S', 'B', 'S', 0x08, 0x70, 0xBA, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};

bool DeviceStorPreventFlag;
TIMER DeviceStorPreventTimer;

bool DeviceStorStoppedFlag;
bool DeviceStorCmdFlag;
TIMER DeviceStorCmdTimer;

bool DeviceStorIsCardInitOK;

static uint8_t sReaderState = READER_UNREADY;
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
static volatile bool s_local_access_busy = FALSE;
static uint32_t s_lba_offset = 0;
static bool s_lba_offset_valid = FALSE;
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes

typedef struct
{
	uint32_t block_count;
	uint32_t block_size;
} DeviceStorMediaInfo;

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
static uint16_t rd16_le(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32_le(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		   ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool DeviceStorIsFatPartType(uint8_t type)
{
	return type == 0x01 || type == 0x04 || type == 0x06 ||
		   type == 0x0B || type == 0x0C || type == 0x0E;
}

static bool DeviceStorIsFatBoot(const uint8_t *buf)
{
	uint8_t spc = buf[0x0D];
	uint8_t fats = buf[0x10];
	uint16_t rsvd = rd16_le(&buf[0x0E]);
	uint16_t fatsz16 = rd16_le(&buf[0x16]);
	uint32_t fatsz32 = rd32_le(&buf[0x24]);
	uint16_t total16 = rd16_le(&buf[0x13]);
	uint32_t total32 = rd32_le(&buf[0x20]);

	return (buf[510] == 0x55 && buf[511] == 0xAA &&
			(buf[0] == 0xEB || buf[0] == 0xE9) &&
			rd16_le(&buf[0x0B]) == DEVICE_STOR_BLOCK_SIZE &&
			(spc == 1 || spc == 2 || spc == 4 || spc == 8 ||
			 spc == 16 || spc == 32 || spc == 64 || spc == 128) &&
			(fats == 1 || fats == 2) &&
			rsvd != 0 &&
			(total16 != 0 || total32 != 0) &&
			(fatsz16 != 0 || fatsz32 != 0) &&
			(buf[0x15] == 0xF8 || buf[0x15] == 0xF0));
}

static bool DeviceStorReadBlockRaw(uint32_t lba, uint8_t *buf)
{
	bool ok = FALSE;

	if(!HAL_SD_Lock(HAL_SD_LOCK_WAIT_FOREVER))
	{
		return FALSE;
	}
	ok = (SDCard_ReadBlock(lba, buf, 1) == NONE_ERR) ? TRUE : FALSE;
	HAL_SD_Unlock();
	return ok;
}

static void DeviceStorInvalidateLbaOffset(void)
{
	s_lba_offset = 0;
	s_lba_offset_valid = FALSE;
}

static uint32_t DeviceStorGetLbaOffset(void)
{
#ifdef BANDATAHUB
	static uint8_t probe[DEVICE_STOR_BLOCK_SIZE];
	static const uint32_t common_offsets[] = {0x2000u};
	uint8_t i;

	if(s_lba_offset_valid)
	{
		return s_lba_offset;
	}

	s_lba_offset = 0;

	if(!DeviceStorReadBlockRaw(0, probe))
	{
		DBG("[MSC] LBA offset probe: read LBA0 failed\n");
		return 0;
	}

	if(DeviceStorIsFatBoot(probe))
	{
		s_lba_offset_valid = TRUE;
		DBG("[MSC] Expose FAT volume at LBA 0\n");
		return 0;
	}

	if(probe[510] == 0x55 && probe[511] == 0xAA)
	{
		for(i = 0; i < 4; i++)
		{
			uint16_t off = (uint16_t)(0x1BE + i * 16);
			uint8_t type = probe[off + 4];
			uint32_t lba = rd32_le(&probe[off + 8]);
			uint32_t secs = rd32_le(&probe[off + 12]);
			if(DeviceStorIsFatPartType(type) && lba != 0 && secs != 0 &&
			   DeviceStorReadBlockRaw(lba, probe) && DeviceStorIsFatBoot(probe))
			{
				s_lba_offset = lba;
				s_lba_offset_valid = TRUE;
				DBG("[MSC] Expose FAT partition at LBA %lu\n", (unsigned long)s_lba_offset);
				return s_lba_offset;
			}
		}
	}

	for(i = 0; i < (uint8_t)(sizeof(common_offsets) / sizeof(common_offsets[0])); i++)
	{
		if(DeviceStorReadBlockRaw(common_offsets[i], probe) && DeviceStorIsFatBoot(probe))
		{
			s_lba_offset = common_offsets[i];
			s_lba_offset_valid = TRUE;
			DBG("[MSC] Expose FAT volume at LBA %lu\n", (unsigned long)s_lba_offset);
			return s_lba_offset;
		}
	}

	if(probe[510] != 0x55 || probe[511] != 0xAA)
	{
		s_lba_offset = 0x2000u;
		s_lba_offset_valid = TRUE;
		DBG("[MSC] No FAT boot found, expose default BanDataHub volume at LBA %lu\n",
		    (unsigned long)s_lba_offset);
		return s_lba_offset;
	}

	DBG("[MSC] LBA offset probe: no FAT boot found (LBA0=%02X %02X %02X %02X sig=%02X%02X)\n",
	    probe[0], probe[1], probe[2], probe[3], probe[510], probe[511]);
#endif
	return 0;
}

=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
uint8_t GetSdReaderState(void)
{
	return sReaderState;
}

void SetSdReaderState(uint8_t State)
{
	sReaderState = State;
}

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
void OTG_DeviceStorSetLocalAccess(bool busy)
{
	s_local_access_busy = busy ? TRUE : FALSE;
	if(busy)
	{
		DeviceStorStoppedFlag = TRUE;
		sReaderState = READER_UNREADY;
	}
	else
	{
		DeviceStorStoppedFlag = FALSE;
		sReaderState = READER_READY;
	}
}

bool OTG_DeviceStorIsLocalAccess(void)
{
	return s_local_access_busy;
}

=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
static bool DeviceStorGetMediaInfo(DeviceStorMediaInfo *Info)
{
	HAL_SD_CardInfo_t SdInfo;

	if(Info == NULL)
	{
		return FALSE;
	}

	memset(Info, 0, sizeof(DeviceStorMediaInfo));

	if(HAL_SD_GetInfo(&SdInfo) != HAL_SD_OK)
	{
		if((SDCard.CardInit == SD_INITED) && (SDCard.BlockNum > 0))
		{
<<<<<<< Updated upstream
			Info->block_count = SDCard.BlockNum;
			Info->block_size = DEVICE_STOR_BLOCK_SIZE;
			return TRUE;
=======
<<<<<<< HEAD
			Info->block_count = SDCard.BlockNum;
			Info->block_size = DEVICE_STOR_BLOCK_SIZE;
			return TRUE;
=======
<<<<<<< HEAD
			uint32_t offset = DeviceStorGetLbaOffset();
			Info->block_count = (SDCard.BlockNum > offset) ? (SDCard.BlockNum - offset) : 0;
			Info->block_size = DEVICE_STOR_BLOCK_SIZE;
			return (Info->block_count > 0) ? TRUE : FALSE;
=======
			Info->block_count = SDCard.BlockNum;
			Info->block_size = DEVICE_STOR_BLOCK_SIZE;
			return TRUE;
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
		}
		else
		{
			return FALSE;
		}
	}

	if((SdInfo.block_count == 0) || (SdInfo.block_size != DEVICE_STOR_BLOCK_SIZE))
	{
		return FALSE;
	}

<<<<<<< Updated upstream
	Info->block_count = SdInfo.block_count;
	Info->block_size = SdInfo.block_size;
	return TRUE;
=======
<<<<<<< HEAD
	Info->block_count = SdInfo.block_count;
	Info->block_size = SdInfo.block_size;
	return TRUE;
=======
<<<<<<< HEAD
	{
		uint32_t offset = DeviceStorGetLbaOffset();
		Info->block_count = (SdInfo.block_count > offset) ? (SdInfo.block_count - offset) : 0;
	}
	Info->block_size = SdInfo.block_size;
	return (Info->block_count > 0) ? TRUE : FALSE;
=======
	Info->block_count = SdInfo.block_count;
	Info->block_size = SdInfo.block_size;
	return TRUE;
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}

static uint32_t DeviceStorGetBlockCount(void)
{
	DeviceStorMediaInfo Info;

	if(DeviceStorGetMediaInfo(&Info))
	{
		return Info.block_count;
	}

	return 0;
}

bool DeviceStorIsCardLink(void)
{
#ifdef BANDATAHUB
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
	if(s_local_access_busy)
	{
		DeviceStorIsCardInitOK = FALSE;
		return FALSE;
	}

=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
	/* BanDataHub: 用 HAL 统一确认 SD 已初始化且容量有效，避免向主机上报 0 容量介质。 */
	DeviceStorMediaInfo Info;
	if(DeviceStorGetMediaInfo(&Info))
	{
		DeviceStorIsCardInitOK = TRUE;
		return TRUE;
	}
	DeviceStorIsCardInitOK = FALSE;
	return FALSE;
#else
	if(SDCard_Detect() != NONE_ERR)
	{
		DeviceStorIsCardInitOK = FALSE;
	}
	else if(!DeviceStorIsCardInitOK)
	{
		if(SDCard.CardInit == SD_INITED)
		{
			DeviceStorIsCardInitOK = TRUE;
		}
		return FALSE;
	}

	return DeviceStorIsCardInitOK;
#endif
}

void DeviceStorSendCSW(uint8_t Status)
{
	gCSW[12] = Status;
	OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, gCSW, sizeof(gCSW), 100000);
}

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
static void DeviceStorDrainOutData(void)
{
	uint32_t xfer_len = rd32_le(&gCBW[8]);

	if((gCBW[12] & 0x80) != 0)
	{
		return;
	}

	while(xfer_len > 0)
	{
		uint32_t data_len = 0;
		uint32_t chunk = (xfer_len > sizeof(CARD_BUF_A)) ? sizeof(CARD_BUF_A) : xfer_len;
		if(OTG_DeviceBulkReceive(DEVICE_MSC_OUT_EP, (uint8_t*)CARD_BUF_A,
		                         chunk, &data_len, 10000) != DEVICE_NONE_ERR)
		{
			break;
		}
		if(data_len == 0 || data_len > xfer_len)
		{
			break;
		}
		xfer_len -= data_len;
	}
}

=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
void DeviceStorReadFmtCapacity(void)
{
	uint32_t BlockCount = DeviceStorGetBlockCount();

	if(BlockCount == 0)
	{
		DeviceStorSendCSW(1);
		return;
	}

	((uint32_t*)&gFmtCapacityData[4])[0] = CpuToBe32(BlockCount);
	OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, gFmtCapacityData, sizeof(gFmtCapacityData), 10000);
	DeviceStorSendCSW(0);
}

extern uint8_t Setup[];

void DeviceStorReadCapacity(void)
{
	uint32_t BlockCount = DeviceStorGetBlockCount();

	if(BlockCount == 0)
	{
		DeviceStorSendCSW(1);
		return;
	}

	BlockCount -= 1;

	((uint32_t*)&gCapacityData[0])[0] = CpuToBe32(BlockCount);
	OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, gCapacityData, sizeof(gCapacityData), 1000);
	DeviceStorSendCSW(0);
}

void DeviceStorRead10(void)
{
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
	uint32_t ReqLBA = Be32ToCpu(*(uint32_t*)(&gCBW[17]));
	uint16_t ReqCnt = Be16ToCpu(*(uint16_t*)(&gCBW[22]));
	uint32_t LbaOffset = DeviceStorGetLbaOffset();
	uint8_t csw_status = 0;
	static uint8_t rd_buf[512];  /* Single block buffer to reduce blocking time */
	uint16_t offset = 0;

	if(s_local_access_busy)
	{
		DeviceStorSendCSW(1);
		return;
	}

	while (offset < ReqCnt)
	{
		/* Read one block at a time to minimize blocking */
		if(!HAL_SD_Lock(HAL_SD_LOCK_WAIT_FOREVER))
		{
			csw_status = 1;
			break;
		}
		if (SDCard_ReadBlock(LbaOffset + ReqLBA + offset, rd_buf, 1) != NONE_ERR)
		{
			csw_status = 1;
			HAL_SD_Unlock();
			break;
		}
		HAL_SD_Unlock();

		OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, rd_buf, 512, 10000);
		offset++;
	}

	DeviceStorSendCSW(csw_status);
	if(HAL_SD_Lock(HAL_SD_LOCK_WAIT_FOREVER))
	{
		Reset_FunctionReset(SDIO_FUNC_SEPA);
		HAL_SD_Unlock();
	}
=======
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
	TIMER Timer;
	uint32_t ReqLBA = Be32ToCpu(*(uint32_t*)(&gCBW[17]));
	uint16_t ReqCnt = Be16ToCpu(*(uint16_t*)(&gCBW[22]));
	if(!SDCard.IsSDHC)
	{
		ReqLBA *= SD_BLOCK_SIZE;
	}
#ifdef FUNC_OS_EN
	osMutexLock(SDIOMutex);
#endif
	SDIO_ClkEnable();
	SDIO_ClearClkHalt();
	TimeOutSet(&Timer,50);
	while(SDIO_IsDataLineBusy() && !IsTimeOut(&Timer));
	DMA_ChannelDisable(PERIPHERAL_ID_SDIO_RX);
	DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT);
	DMA_BlockConfig(PERIPHERAL_ID_SDIO_RX);
	DMA_BlockBufSet(PERIPHERAL_ID_SDIO_RX, (ReqCnt % 2) ? CARD_BUF_A : CARD_BUF_B, SD_BLOCK_SIZE);
	DMA_ChannelEnable(PERIPHERAL_ID_SDIO_RX);
	SDIO_MultiBlockDisable();
	SDIO_AutoKillRXClkEnable();
	SDIO_SingleBlockConfig(SDIO_DIR_RX,SD_BLOCK_SIZE);
	SDIO_DataTransfer(1);
	SDIO_CmdSend(CMD18_READ_MULTIPLE_BLOCK, ReqLBA, 20);
	TimeOutSet(&Timer, 250);
	while(!DMA_InterruptFlagGet(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT) && !IsTimeOut(&Timer));
	if(!IsTimeOut(&Timer))
	{
		DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT);

		while(ReqCnt-- > 1)
		{
			SDIO_DataTransfer(0);
			DMA_ChannelDisable(PERIPHERAL_ID_SDIO_RX);
			DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT);
			DMA_BlockConfig(PERIPHERAL_ID_SDIO_RX);
			DMA_BlockBufSet(PERIPHERAL_ID_SDIO_RX, (ReqCnt % 2) ? CARD_BUF_A : CARD_BUF_B, SD_BLOCK_SIZE);
			DMA_ChannelEnable(PERIPHERAL_ID_SDIO_RX);
			SDIO_DataTransfer(1);
			SDIO_ClkEnable();
			OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, (ReqCnt % 2) ? (uint8_t*)CARD_BUF_B : (uint8_t*)CARD_BUF_A, 512, 10000);
			TimeOutSet(&Timer, 250);
			while(!DMA_InterruptFlagGet(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT) && !IsTimeOut(&Timer))
			{
				;
			}
			if(IsTimeOut(&Timer))
			{
				break;
			}
			DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_RX, DMA_DONE_INT);
		}
		OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, (ReqCnt % 2) ? (uint8_t*)CARD_BUF_B : (uint8_t*)CARD_BUF_A, 512, 10000);
		SDIO_DataTransfer(0);
		SDIO_ClkEnable();
		SDIO_CmdDoneCheckBusy(1);
		SDIO_CmdSend(CMD12_STOP_TRANSMISSION, 0, 300);
	}
	SDIO_CmdDoneCheckBusy(0);
	SDIO_DataTransfer(0);
	SDIO_ClkDisable();

	DeviceStorSendCSW(0);
	Reset_FunctionReset(SDIO_FUNC_SEPA);
#ifdef FUNC_OS_EN
	osMutexUnlock(SDIOMutex);
#endif
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}

void DeviceStorWrite10(void)
{
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
	uint32_t ReqLBA = Be32ToCpu(*(uint32_t*)(&gCBW[17]));
	uint16_t ReqCnt = Be16ToCpu(*(uint16_t*)(&gCBW[22]));
	uint32_t LbaOffset = DeviceStorGetLbaOffset();
	uint8_t csw_status = 0;
	static uint8_t wr_buf[512];  /* Single block buffer to reduce blocking time */
	uint16_t offset = 0;
	uint32_t DataLength = 0;

	if(s_local_access_busy)
	{
		DeviceStorSendCSW(1);
		return;
	}

	while (offset < ReqCnt)
	{
		/* Receive one block from host */
		if (OTG_DeviceBulkReceive(DEVICE_MSC_OUT_EP, wr_buf, 512, &DataLength, 10000) != DEVICE_NONE_ERR)
		{
			csw_status = 1;
			goto done;
		}

		/* Write one block at a time to minimize blocking */
		if(!HAL_SD_Lock(HAL_SD_LOCK_WAIT_FOREVER))
		{
			csw_status = 1;
			break;
		}
		if (SDCard_WriteBlock(LbaOffset + ReqLBA + offset, wr_buf, 1) != NONE_ERR)
		{
			csw_status = 1;
			HAL_SD_Unlock();
			break;
		}
		HAL_SD_Unlock();
		if(ReqLBA + offset < 128u)
		{
			DeviceStorInvalidateLbaOffset();
		}

		offset++;
	}

done:
	DeviceStorSendCSW(csw_status);
	if(HAL_SD_Lock(HAL_SD_LOCK_WAIT_FOREVER))
	{
		Reset_FunctionReset(SDIO_FUNC_SEPA);
		HAL_SD_Unlock();
	}
=======
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
	TIMER Timer;
	uint32_t ReqLBA = Be32ToCpu(*(uint32_t*)(&gCBW[17]));
	uint16_t ReqCnt = Be16ToCpu(*(uint16_t*)(&gCBW[22]));
	uint32_t DataLength = 0;
	if(!SDCard.IsSDHC)
	{
		ReqLBA *= SD_BLOCK_SIZE;
	}
#ifdef FUNC_OS_EN
	osMutexLock(SDIOMutex);
#endif
	Reset_FunctionReset(SDIO_FUNC_SEPA);
	SDIO_ClkEnable();
	while(SDIO_IsDataLineBusy());
	SDIO_ClearClkHalt();
	SDIO_MultiBlockDisable();
	SDIO_AutoKillTXClkEnable();
	SDIO_DataTransfer(0);
	OTG_DeviceBulkReceive(DEVICE_MSC_OUT_EP, (ReqCnt%2) ? (uint8_t*)CARD_BUF_A : (uint8_t*)CARD_BUF_B, 512, &DataLength, 10000);
	SDIO_CmdSend(CMD25_WRITE_MULTIPLE_BLOCK, ReqLBA, 50);
	while(ReqCnt--)
	{
		while(SDIO_IsDataLineBusy())
		{
			;
		}
		SDIO_DataTransfer(0);
		DMA_ChannelDisable(PERIPHERAL_ID_SDIO_TX);
		DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT);
		DMA_BlockConfig(PERIPHERAL_ID_SDIO_TX);
		DMA_BlockBufSet(PERIPHERAL_ID_SDIO_TX, (uint8_t*)((ReqCnt%2) ? (uint8_t*)CARD_BUF_B : (uint8_t*)CARD_BUF_A), SD_BLOCK_SIZE);
		DMA_ChannelEnable(PERIPHERAL_ID_SDIO_TX);
		SDIO_SingleBlockConfig(SDIO_DIR_TX,SD_BLOCK_SIZE);
		SDIO_DataTransfer(1);
		SDIO_ClkEnable();

		if(ReqCnt != 0)
		{
			OTG_DeviceBulkReceive(DEVICE_MSC_OUT_EP, (ReqCnt % 2) ? (uint8_t*)CARD_BUF_A : (uint8_t*)CARD_BUF_B, 512, &DataLength, 10000);
		}
		TimeOutSet(&Timer, 500);
		while((!DMA_InterruptFlagGet(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT) || !SDIO_DataIsDone()) && !IsTimeOut(&Timer));
		if(IsTimeOut(&Timer))
		{
			break;
		}
		DMA_InterruptFlagClear(PERIPHERAL_ID_SDIO_TX, DMA_DONE_INT);
	}
	SDIO_DataTransfer(0);
	SDIO_ClkDisable();

	SDIO_ClearClkHalt();
	SDIO_CmdDoneCheckBusy(1);
	SDIO_CmdSend(CMD12_STOP_TRANSMISSION, 0, 300);
	SDIO_CmdDoneCheckBusy(0);
	SDIO_DataTransfer(0);
	SDIO_ClkDisable();
#ifdef FUNC_OS_EN
	osMutexUnlock(SDIOMutex);
#endif
	DeviceStorSendCSW(0);
	Reset_FunctionReset(SDIO_FUNC_SEPA);
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}

extern uint8_t Setup[];
void DeviceStorModeSense(void)
{
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
	static uint8_t mode_buf[192];
	const uint8_t *resp = NULL;
	uint32_t resp_len = 0;
	uint32_t alloc_len = gCBW[19];
	uint32_t send_len;

=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
	if(!DeviceStorIsCardLink())
	{
		OTG_DeviceStallSend(DEVICE_MSC_IN_EP);
#ifdef FUNC_OS_EN
		osTaskDelay(10);
#else
		WaitMs(10);
#endif
		OTG_DeviceRequestProcess();
		memset(Setup,0,8);
		DeviceStorSendCSW(1);
		return;
	}

	((uint32_t*)&gModeSenseAllPage[4])[0] = CpuToBe32(DeviceStorGetBlockCount());

	switch(gCBW[17])
	{
		case 0x08:
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
			resp = gModeSenseCachingPage;
			resp_len = sizeof(gModeSenseCachingPage);
			break;

		case 0x1C:
			resp = gModeSenseProtectPage;
			resp_len = sizeof(gModeSenseProtectPage);
			break;

		case 0x3F:
			resp = gModeSenseAllPage;
			resp_len = sizeof(gModeSenseAllPage);
=======
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
			OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, gModeSenseCachingPage, sizeof(gModeSenseCachingPage), 10000);
			DeviceStorSendCSW(0);
			break;

		case 0x1C:
			OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, (uint8_t*)gModeSenseProtectPage, sizeof(gModeSenseProtectPage), 10000);
			DeviceStorSendCSW(0);
			break;

		case 0x3F:
			OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, gModeSenseAllPage, sizeof(gModeSenseAllPage), 10000);
			DeviceStorSendCSW(0);
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
			break;

		default:
			OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, 0, 0, 10000);
			DeviceStorSendCSW(0);
<<<<<<< Updated upstream
			break;
	}
=======
<<<<<<< HEAD
			break;
	}
=======
<<<<<<< HEAD
			return;
	}

	send_len = alloc_len;
	if(send_len > sizeof(mode_buf))
	{
		send_len = sizeof(mode_buf);
	}

	memset(mode_buf, 0, sizeof(mode_buf));
	memcpy(mode_buf, resp, (resp_len < send_len) ? resp_len : send_len);
	OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, mode_buf, send_len, 10000);
	DeviceStorSendCSW(0);
=======
			break;
	}
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}

bool DeviceStorIsPrevent(void)
{
	if((!DeviceStorPreventFlag) || IsTimeOut(&DeviceStorPreventTimer))
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

bool DeviceStorIsStopped(void)
{
	if(DeviceStorStoppedFlag)
	{
		return TRUE;
	}
	else if(DeviceStorCmdFlag && IsTimeOut(&DeviceStorCmdTimer))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void OTG_DeviceStorInit(void)
{
	sReaderState = READER_INIT;
	DeviceStorStoppedFlag = FALSE;
	DeviceStorCmdFlag = FALSE;
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
	DeviceStorInvalidateLbaOffset();
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
	sReaderState = READER_READY;
	DBG("MSC Storage initialized\n");
}

extern uint8_t Setup[];
<<<<<<< Updated upstream
void OTG_DeviceStorProcess(void)
=======
<<<<<<< HEAD
void OTG_DeviceStorProcess(void)
=======
<<<<<<< HEAD
bool OTG_DeviceStorProcess(void)
=======
void OTG_DeviceStorProcess(void)
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
{
	uint32_t DataLen = 0;

	if(OTG_DeviceBulkReceive(DEVICE_MSC_OUT_EP, (uint8_t*)&gCBW, 31, &DataLen, 1) != DEVICE_NONE_ERR)
	{
<<<<<<< Updated upstream
		return;
=======
<<<<<<< HEAD
		return;
=======
<<<<<<< HEAD
		return FALSE;
=======
		return;
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
	}

	gCSW[4] = gCBW[4];
	gCSW[5] = gCBW[5];
	gCSW[6] = gCBW[6];
	gCSW[7] = gCBW[7];

	switch(gCBW[15])
	{
		case INQUIRY:
<<<<<<< Updated upstream
			DBG("INQUIRY\n");
=======
<<<<<<< HEAD
			DBG("INQUIRY\n");
=======
<<<<<<< HEAD
=======
			DBG("INQUIRY\n");
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
			sReaderState = READER_INQUIRY;
			OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, gInquiryData, sizeof(gInquiryData), 10000);
			DeviceStorSendCSW(0);
			break;

		case READ_FORMAT_CAPACITY:
<<<<<<< Updated upstream
			DBG("READ_FORMAT_CAPACITY\n");
=======
<<<<<<< HEAD
			DBG("READ_FORMAT_CAPACITY\n");
=======
<<<<<<< HEAD
=======
			DBG("READ_FORMAT_CAPACITY\n");
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
			sReaderState = READER_READ_FORMAT_CAPACITY;
			DeviceStorReadFmtCapacity();
			break;

		case READ_CAPACITY:
<<<<<<< Updated upstream
			DBG("READ_CAPACITY\n");
=======
<<<<<<< HEAD
			DBG("READ_CAPACITY\n");
=======
<<<<<<< HEAD
=======
			DBG("READ_CAPACITY\n");
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
			sReaderState = READER_READ_CAPACITY;
			DeviceStorReadCapacity();
			break;

		case READ_10:
<<<<<<< Updated upstream
			DBG("R\n");
=======
<<<<<<< HEAD
			DBG("R\n");
=======
<<<<<<< HEAD
=======
			DBG("R\n");
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
			sReaderState = READER_READ;
			DeviceStorRead10();
			break;

		case WRITE_10:
<<<<<<< Updated upstream
			DBG("W\n");
=======
<<<<<<< HEAD
			DBG("W\n");
=======
<<<<<<< HEAD
=======
			DBG("W\n");
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
			sReaderState = READER_WIRTE;
			DeviceStorWrite10();
			TimeOutSet(&DeviceStorPreventTimer, 2000);
			break;

		case TEST_UNIT_READY:
			sReaderState = READER_UNREADY;
			DeviceStorStoppedFlag = FALSE;
			if(DeviceStorIsCardLink())
			{
				DeviceStorSendCSW(0);
			}
			else
			{
				DeviceStorSendCSW(1);
			}
			break;

		case MODE_SENSE:
			DeviceStorModeSense();
			break;

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
		case MODE_SELECT:
		case MODE_SELECT_10:
			DeviceStorDrainOutData();
			DeviceStorSendCSW(0);
			break;

=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
		case ALLOW_MEDIUM_REMOVAL:
			if((gCBW[19] == 0) && DeviceStorIsCardLink())
			{
				sReaderState = READER_NOT_ALLOW_REMOVAL;
				DeviceStorPreventFlag = FALSE;
				DeviceStorSendCSW(0);
			}
			else
			{
				sReaderState = READER_ALLOW_REMOVAL;
				DeviceStorPreventFlag = TRUE;
				DeviceStorSendCSW(0);
			}
			break;

		case REQUEST_SENSE:
			if((!DeviceStorIsCardLink()) || DeviceStorStoppedFlag)
			{
				OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, (uint8_t*)gRequestSenseNotReady, sizeof(gRequestSenseNotReady), 10000);
			}
			else
			{
				OTG_DeviceBulkSend(DEVICE_MSC_IN_EP, (uint8_t*)gRequestSenseReady, sizeof(gRequestSenseReady), 10000);
			}
			DeviceStorSendCSW(0);
			break;

		case VERYFY:
			DeviceStorSendCSW(0);
			break;

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
		case FORMAT_UNIT:
			DeviceStorDrainOutData();
			DeviceStorSendCSW(0);
			break;

		case SYNCHRONIZE_CACHE:
			DeviceStorSendCSW(0);
			break;

=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
		case START_STOP_UNIT:
			DeviceStorSendCSW(0);
			DeviceStorStoppedFlag = TRUE;
			sReaderState = READER_UNREADY;
			break;

		default:
			DeviceStorSendCSW(1);
			break;
	}

	DeviceStorCmdFlag = TRUE;
	TimeOutSet(&DeviceStorCmdTimer, 1500);
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
	return TRUE;
}

/* ========== MSC FreeRTOS Task ========== */
#include "rtos_api.h"
#include "FreeRTOS.h"
#include "task.h"

void MSC_TaskMain(void *param)
{
	uint8_t idle_count = 0;

	(void)param;
	DBG("[MSC] Task started\n");

	while (1) {
		OTG_DeviceRequestProcess();
		if(OTG_DeviceStorProcess())
		{
			idle_count = 0;
			vTaskDelay(1);
		}
		else
		{
			if(idle_count < 4)
			{
				idle_count++;
				vTaskDelay(1);
			}
			else
			{
				vTaskDelay(1);  /* Sleep only after sustained idle. */
			}
		}
	}
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}