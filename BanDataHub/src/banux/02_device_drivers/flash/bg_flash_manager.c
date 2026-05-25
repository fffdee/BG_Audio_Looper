#include "bg_flash_manager.h"
#include "spim.h"
#include "spi_flash.h"
#include "debug.h"
#include "spim_interface.h"
#include "dma.h"
#include "product_def.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifdef BANDATAHUB
/* BanDataHub: no NOR/NAND Flash, provide stub implementations */

void flash_init(void)
{
	DBG("Flash init skipped (BANDATAHUB: no NOR/NAND Flash)\n");
}

static void flash_stub_no_hw(void)
{
	DBG("Flash operation skipped (BANDATAHUB: no NOR/NAND Flash)\n");
}

void flash_ReadID(uint8_t* manufacturerID, uint8_t* memoryType, uint8_t* deviceID, uint8_t dev) {
	*manufacturerID = 0;
	*memoryType = 0;
	*deviceID = 0;
}

void flash_WriteEnable(uint8_t enable, uint8_t dev) { (void)enable; (void)dev; }
uint8_t flash_ReadStatusReg(uint8_t dev) { (void)dev; return 0; }
void flash_WriteStatusReg(uint8_t data, uint8_t dev) { (void)data; (void)dev; }
void flash_WaitForWriteEnd(uint8_t dev) { (void)dev; }
void flash_SectorErase(uint32_t sectorAddress, uint8_t dev) { (void)sectorAddress; (void)dev; }
uint8_t flash_PageProgram(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev) { (void)address; (void)data; (void)size; (void)dev; return FLASH_STATUS_OK; }
void flash_ReadData(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev) { (void)address; (void)data; (void)size; (void)dev; }
uint32_t flash_GetRemainingCapacity(uint8_t dev) { (void)dev; return 0; }
uint32_t flash_GetTotalByte(uint8_t dev) { (void)dev; return 0; }
void flash_EraseAll(uint8_t dev) { (void)dev; }

BG_Flash_Manager BG_flash_manager = {
	.Init = flash_init,
	.PageProgram = flash_PageProgram,
	.SectorErase = flash_SectorErase,
	.WriteEnable = flash_WriteEnable,
	.ReadData = flash_ReadData,
	.ReadID = flash_ReadID,
	.GetRemainingCapacity = flash_GetRemainingCapacity,
	.GetTotalByte = flash_GetTotalByte,
	.EraseAll = flash_EraseAll,
};

#else /* !BANDATAHUB - original flash manager with NOR/NAND support */

/* Bad block management definitions */
#define BAD_BLOCK_MARKER 0x00
#define BAD_BLOCK_TABLE_MAGIC 0x42424242
#define BAD_BLOCK_TABLE_VERSION 0x0101

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t count;
	uint32_t bad_blocks[128];
	uint32_t reserved[4];
} BadBlockTable;

typedef struct {
	BadBlockTable table;
	uint32_t table_address;
	bool initialized;
} BadBlockManager;

/* Internal function declarations */
void flash_init(void);
void flash_ReadID(uint8_t* manufacturerID, uint8_t* memoryType, uint8_t* deviceID, uint8_t dev);
void flash_WriteEnable(uint8_t enable, uint8_t dev);
uint8_t flash_ReadStatusReg(uint8_t dev);
void flash_WriteStatusReg(uint8_t data, uint8_t dev);
void flash_WaitForWriteEnd(uint8_t dev);
void flash_SectorErase(uint32_t sectorAddress, uint8_t dev);
uint8_t flash_PageProgram(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev);
void flash_ReadData(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev);
uint32_t flash_GetRemainingCapacity(uint8_t dev);
uint32_t flash_GetTotalByte(uint8_t dev);
void flash_EraseAll(uint8_t dev);
void flash_write_byte(uint8_t data);
uint8_t flash_read_byte(void);
void flash_write(uint8_t* data, uint16_t size);
void flash_read(uint8_t* data, uint16_t size);

/* Bad block management function declarations */
static void bad_block_manager_init(uint8_t dev);
static bool is_block_bad(uint32_t block_address, uint8_t dev);
static bool mark_block_as_bad(uint32_t block_address, uint8_t dev);
static uint32_t find_next_good_block(uint32_t start_block, uint8_t dev);
static void save_bad_block_table(uint8_t dev);
static void load_bad_block_table(uint8_t dev);
static uint32_t address_to_block(uint32_t address, uint8_t dev);
static uint32_t block_to_address(uint32_t block, uint8_t dev);
static uint8_t nand_audio_flush_buffer(uint8_t dev);

/* Global variable */
BG_Flash_Manager BG_flash_manager = {
	.Init = flash_init,
	.PageProgram = flash_PageProgram,
	.SectorErase = flash_SectorErase,
	.WriteEnable = flash_WriteEnable,
	.ReadData = flash_ReadData,
	.ReadID = flash_ReadID,
	.GetRemainingCapacity = flash_GetRemainingCapacity,
	.GetTotalByte = flash_GetTotalByte,
	.EraseAll = flash_EraseAll,
};

static BadBlockManager bad_block_manager = {0};

void flash_init(void)
{
	FLASH_CS_INIT();
	NAND_CS_INIT();
#ifndef BANBOX_II
	FLASH_WP_INIT();
#endif
	FLASH_CS_DISABLE();
	NAND_CS_DISABLE();
#ifndef BANBOX_II
	FLASH_WP_DISABLE();
#endif
	DBG("Dual NOR Flash initialized (CS=A21, A22)\n");
}

void flash_write_byte(uint8_t data)
{
	SPIM_DMA_Send_Start(&data, 1);
	while (!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));
}

uint8_t flash_read_byte(void)
{
	uint8_t data;
	SPIM_DMA_Recv_Start(&data, 1);
	while (!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));
	return data;
}

void flash_read(uint8_t* data, uint16_t size)
{
	SPIM_DMA_Recv_Start(data, size);
	while (!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));
}

void flash_write(uint8_t* data, uint16_t size)
{
	SPIM_DMA_Send_Start(data, size);
	while (!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));
}

void flash_ReadID(uint8_t* manufacturerID, uint8_t* memoryType, uint8_t* deviceID, uint8_t dev)
{
	if (dev == DEV_NOR1) {
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_JEDEC_ID);
		*manufacturerID = flash_read_byte();
		*memoryType = flash_read_byte();
		*deviceID = flash_read_byte();
		FLASH_CS_DISABLE();
	} else if (dev == DEV_NOR2) {
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_JEDEC_ID);
		*manufacturerID = flash_read_byte();
		*memoryType = flash_read_byte();
		*deviceID = flash_read_byte();
		NAND_CS_DISABLE();
	}
}

uint32_t flash_GetTotalByte(uint8_t dev)
{
	uint32_t capacity = 0;
	uint8_t manufacturerID, memoryType, deviceID;
	flash_ReadID(&manufacturerID, &memoryType, &deviceID, dev);

	DBG("%s Flash ID: Manufacturer=0x%02X, MemoryType=0x%02X, DeviceID=0x%02X\n",
		dev == DEV_NOR1 ? "NOR1" : "NOR2", manufacturerID, memoryType, deviceID);

	switch (deviceID) {
		case DEVICE_ID_64MBIT:  capacity = 64 * 1024 * 1024; break;
		case DEVICE_ID_128MBIT: capacity = 128 * 1024 * 1024; break;
		case DEVICE_ID_256MBIT: capacity = 256 * 1024 * 1024; break;
		case DEVICE_ID_512MBIT: capacity = 512 * 1024 * 1024; break;
		case DEVICE_ID_1GBIT:   capacity = 1024 * 1024 * 1024; break;
		case DEVICE_ID_2GBIT:   capacity = 2048 * 1024 * 1024; break;
		case DEVICE_ID_W25N02:  capacity = 256 * 1024 * 1024; break;
		default:
			if (dev == DEV_NAND && manufacturerID == 0xEF) {
				DBG("Unknown Winbond NAND device ID: 0x%02X, assuming W25N02\n", deviceID);
				capacity = 256 * 1024 * 1024;
			} else {
				DBG("Unknown device ID: 0x%02X\n", deviceID);
				capacity = 0;
			}
			break;
	}
	return capacity / 8;
}

uint32_t Windbond_GetCapacity(uint8_t deviceID, uint8_t dev)
{
	uint32_t capacity = 0;
	(void)dev;
	switch (deviceID) {
		case DEVICE_ID_64MBIT:  capacity = 64 * 1024 * 1024; break;
		case DEVICE_ID_128MBIT: capacity = 128 * 1024 * 1024; break;
		case DEVICE_ID_256MBIT: capacity = 256 * 1024 * 1024; break;
		case DEVICE_ID_512MBIT: capacity = 512 * 1024 * 1024; break;
		case DEVICE_ID_1GBIT:   capacity = 1024 * 1024 * 1024; break;
		case DEVICE_ID_2GBIT:   capacity = 2048 * 1024 * 1024; break;
		default: capacity = 0; break;
	}
	return capacity;
}

bool flash_IsSectorErased(uint32_t sectorAddress, uint8_t dev)
{
	uint8_t data;
	flash_ReadData(sectorAddress, &data, 1, dev);
	return data == 0xFF;
}

uint32_t flash_GetRemainingCapacity(uint8_t dev)
{
	uint32_t remainingCapacity = 0;
	uint32_t sectorAddress = 0;
	uint32_t i;
	uint8_t manufacturerID, memoryType, deviceID;
	flash_ReadID(&manufacturerID, &memoryType, &deviceID, dev);
	for (i = 0; i < Windbond_GetCapacity(deviceID, dev) / SECTOR_SIZE; ++i) {
		if (flash_IsSectorErased(sectorAddress, dev)) {
			remainingCapacity += 1;
		}
		sectorAddress += SECTOR_SIZE;
	}
	DBG("Total is:%d KByte,Remain is:%d KByte\n",
		(int)(Windbond_GetCapacity(deviceID, dev) / SECTOR_SIZE) * 4, (int)remainingCapacity * 4);
	return remainingCapacity;
}

void flash_WriteEnable(uint8_t enable, uint8_t dev)
{
	if (dev == DEV_NOR1) {
		FLASH_CS_ENABLE();
		flash_write_byte(enable ? FLASH_CMD_WRITE_ENABLE : FLASH_CMD_WRITE_DISABLE);
		FLASH_CS_DISABLE();
	} else if (dev == DEV_NOR2) {
		NAND_CS_ENABLE();
		flash_write_byte(enable ? FLASH_CMD_WRITE_ENABLE : FLASH_CMD_WRITE_DISABLE);
		NAND_CS_DISABLE();
	}
}

uint8_t flash_ReadStatusReg(uint8_t dev)
{
	uint8_t data;
	if (dev == DEV_NOR1) {
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_READ_STATUS_REG);
		data = flash_read_byte();
		FLASH_CS_DISABLE();
	} else if (dev == DEV_NOR2) {
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_READ_STATUS_REG);
		data = flash_read_byte();
		NAND_CS_DISABLE();
	} else {
		data = 0;
	}
	return data;
}

void flash_WriteStatusReg(uint8_t data, uint8_t dev)
{
	flash_WriteEnable(1, dev);
	if (dev == DEV_NOR1) {
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_WRITE_STATUS_REG);
		flash_write_byte(data);
		FLASH_CS_DISABLE();
	} else if (dev == DEV_NOR2) {
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_WRITE_STATUS_REG);
		flash_write_byte(data);
		NAND_CS_DISABLE();
	}
	flash_WaitForWriteEnd(dev);
}

void flash_WaitForWriteEnd(uint8_t dev)
{
	while ((flash_ReadStatusReg(dev) & 0x01) == 0x01);
}

void flash_SectorErase(uint32_t sectorAddress, uint8_t dev)
{
	flash_WriteEnable(1, dev);
	if (dev == DEV_NOR1) {
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_SECTOR_ERASE);
		flash_write_byte((sectorAddress & 0xFF0000) >> 16);
		flash_write_byte((sectorAddress & 0x00FF00) >> 8);
		flash_write_byte(sectorAddress & 0x0000FF);
		FLASH_CS_DISABLE();
	} else if (dev == DEV_NOR2) {
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_SECTOR_ERASE);
		flash_write_byte((sectorAddress & 0xFF0000) >> 16);
		flash_write_byte((sectorAddress & 0x00FF00) >> 8);
		flash_write_byte(sectorAddress & 0x0000FF);
		NAND_CS_DISABLE();
	}
	flash_WaitForWriteEnd(dev);
}

void flash_EraseAll(uint8_t dev)
{
	flash_WriteEnable(1, dev);
	if (dev == DEV_NOR) {
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_CHIP_ERASE);
		FLASH_CS_DISABLE();
	} else if (dev == DEV_NOR2) {
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_CHIP_ERASE);
		NAND_CS_DISABLE();
	}
	flash_WaitForWriteEnd(dev);
}

uint8_t flash_PageProgram(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev)
{
	flash_WriteEnable(1, dev);
	if (dev == DEV_NOR1) {
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_PAGE_PROGRAM);
		flash_write_byte((address >> 16) & 0xFF);
		flash_write_byte((address >> 8) & 0xFF);
		flash_write_byte(address & 0xFF);
		flash_write(data, size);
		FLASH_CS_DISABLE();
	} else if (dev == DEV_NOR2) {
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_PAGE_PROGRAM);
		flash_write_byte((address >> 16) & 0xFF);
		flash_write_byte((address >> 8) & 0xFF);
		flash_write_byte(address & 0xFF);
		flash_write(data, size);
		NAND_CS_DISABLE();
	}
	flash_WaitForWriteEnd(dev);
	return FLASH_STATUS_OK;
}

void flash_ReadData(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev)
{
	if (dev == DEV_NOR1) {
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_READ_DATA);
		flash_write_byte((address & 0xFF0000) >> 16);
		flash_write_byte((address & 0x00FF00) >> 8);
		flash_write_byte(address & 0x0000FF);
		flash_read(data, size);
		FLASH_CS_DISABLE();
	} else if (dev == DEV_NOR2) {
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_READ_DATA);
		flash_write_byte((address & 0xFF0000) >> 16);
		flash_write_byte((address & 0x00FF00) >> 8);
		flash_write_byte(address & 0x0000FF);
		flash_read(data, size);
		NAND_CS_DISABLE();
	}
}

void flash_test_w25n02_registers(void)
{
	DBG("W25N02 test skipped (not BANDATAHUB, but test not implemented)\n");
}

/* Bad block management functions */
static void bad_block_manager_init(uint8_t dev)
{
	uint8_t manufacturerID, memoryType, deviceID;
	uint32_t total_size;

	if (bad_block_manager.initialized) return;

	flash_ReadID(&manufacturerID, &memoryType, &deviceID, dev);
	total_size = flash_GetTotalByte(dev);

	if (dev == DEV_NAND) {
		bad_block_manager.table_address = 64 * 2048;
	} else {
		bad_block_manager.table_address = total_size - sizeof(BadBlockTable);
	}

	bad_block_manager.initialized = true;
	load_bad_block_table(dev);

	if (bad_block_manager.table.magic != BAD_BLOCK_TABLE_MAGIC ||
		bad_block_manager.table.version != BAD_BLOCK_TABLE_VERSION) {
		memset(&bad_block_manager.table, 0, sizeof(BadBlockTable));
		bad_block_manager.table.magic = BAD_BLOCK_TABLE_MAGIC;
		bad_block_manager.table.version = BAD_BLOCK_TABLE_VERSION;
		bad_block_manager.table.count = 0;
		save_bad_block_table(dev);
	}
}

static bool is_block_bad(uint32_t block_address, uint8_t dev)
{
	uint16_t i;
	if (!bad_block_manager.initialized) bad_block_manager_init(dev);
	for (i = 0; i < bad_block_manager.table.count; i++) {
		if (bad_block_manager.table.bad_blocks[i] == block_address) return true;
	}
	return false;
}

static bool mark_block_as_bad(uint32_t block_address, uint8_t dev)
{
	uint32_t address;
	uint8_t marker;
	if (!bad_block_manager.initialized) bad_block_manager_init(dev);
	if (is_block_bad(block_address, dev)) return true;
	if (bad_block_manager.table.count >= sizeof(bad_block_manager.table.bad_blocks) / sizeof(bad_block_manager.table.bad_blocks[0])) return false;
	bad_block_manager.table.bad_blocks[bad_block_manager.table.count++] = block_address;
	address = block_to_address(block_address, dev);
	marker = BAD_BLOCK_MARKER;
	flash_PageProgram(address, &marker, 1, dev);
	save_bad_block_table(dev);
	return true;
}

static uint32_t find_next_good_block(uint32_t start_block, uint8_t dev)
{
	uint32_t total_blocks, block;
	if (!bad_block_manager.initialized) bad_block_manager_init(dev);
	total_blocks = flash_GetTotalByte(dev) / (64 * 2048);
	for (block = start_block; block < total_blocks; block++) {
		if (!is_block_bad(block, dev)) return block;
	}
	for (block = 0; block < start_block; block++) {
		if (!is_block_bad(block, dev)) return block;
	}
	return 0xFFFFFFFF;
}

static void save_bad_block_table(uint8_t dev)
{
	flash_SectorErase(bad_block_manager.table_address, dev);
	flash_PageProgram(bad_block_manager.table_address, (uint8_t*)&bad_block_manager.table, sizeof(BadBlockTable), dev);
}

static void load_bad_block_table(uint8_t dev)
{
	flash_ReadData(bad_block_manager.table_address, (uint8_t*)&bad_block_manager.table, sizeof(BadBlockTable), dev);
}

static uint32_t address_to_block(uint32_t address, uint8_t dev) { (void)dev; return address / (64 * 2048); }
static uint32_t block_to_address(uint32_t block, uint8_t dev) { (void)dev; return block * 64 * 2048; }

uint8_t nand_check_bad_block(uint32_t block_address, uint8_t dev) { return is_block_bad(block_address, dev) ? 1 : 0; }
uint8_t nand_mark_bad_block(uint32_t block_address, uint8_t dev) { return mark_block_as_bad(block_address, dev) ? 1 : 0; }
uint32_t nand_find_next_good_block(uint32_t start_block, uint8_t dev) { return find_next_good_block(start_block, dev); }

uint32_t nand_get_safe_write_address(uint32_t current_address, uint32_t bytes_to_write, uint8_t dev)
{
	uint32_t current_block, next_good_block;
	(void)bytes_to_write;
	if (dev != DEV_NAND) return current_address;
	current_block = address_to_block(current_address, dev);
	if (is_block_bad(current_block, dev)) {
		next_good_block = find_next_good_block(current_block + 1, dev);
		if (next_good_block == 0xFFFFFFFF) return 0xFFFFFFFF;
		return block_to_address(next_good_block, dev);
	}
	return current_address;
}

/* NAND Flash audio buffer */
#define NAND_AUDIO_PAGE_SIZE 2048
#define NAND_AUDIO_BUFFER_SIZE NAND_AUDIO_PAGE_SIZE

static struct {
	uint8_t buffer[NAND_AUDIO_BUFFER_SIZE];
	uint32_t current_page_address;
	uint16_t buffer_pos;
	bool initialized;
} nand_audio_buffer = {{0}, 0, 0, false};

uint8_t nand_audio_write_buffered(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev)
{
	uint16_t data_pos = 0;
	if (dev != DEV_NAND) return flash_PageProgram(address, data, size, dev);
	if (!nand_audio_buffer.initialized) {
		nand_audio_buffer.current_page_address = (address / NAND_AUDIO_PAGE_SIZE) * NAND_AUDIO_PAGE_SIZE;
		nand_audio_buffer.buffer_pos = address % NAND_AUDIO_PAGE_SIZE;
		nand_audio_buffer.initialized = true;
	}
	while (data_pos < size) {
		uint32_t target_page = (address + data_pos) / NAND_AUDIO_PAGE_SIZE * NAND_AUDIO_PAGE_SIZE;
		uint16_t remaining_in_page, remaining_data, bytes_to_copy;
		if (target_page != nand_audio_buffer.current_page_address) {
			uint8_t result;
			if (nand_audio_buffer.buffer_pos > 0) {
				result = nand_audio_flush_buffer(dev);
				if (result != FLASH_STATUS_OK) return result;
			}
			nand_audio_buffer.current_page_address = target_page;
			nand_audio_buffer.buffer_pos = 0;
		}
		remaining_in_page = NAND_AUDIO_PAGE_SIZE - nand_audio_buffer.buffer_pos;
		remaining_data = size - data_pos;
		bytes_to_copy = (remaining_in_page < remaining_data) ? remaining_in_page : remaining_data;
		memcpy(&nand_audio_buffer.buffer[nand_audio_buffer.buffer_pos], &data[data_pos], bytes_to_copy);
		nand_audio_buffer.buffer_pos += bytes_to_copy;
		data_pos += bytes_to_copy;
		if (nand_audio_buffer.buffer_pos >= NAND_AUDIO_PAGE_SIZE) {
			uint8_t result = nand_audio_flush_buffer(dev);
			if (result != FLASH_STATUS_OK) return result;
		}
	}
	return FLASH_STATUS_OK;
}

void nand_smart_audio_init(void)
{
	nand_audio_buffer.buffer_pos = 0;
	nand_audio_buffer.current_page_address = 0;
	nand_audio_buffer.initialized = true;
}

uint32_t nand_smart_audio_get_address(void)
{
	if (!nand_audio_buffer.initialized) return 0;
	return nand_audio_buffer.current_page_address + nand_audio_buffer.buffer_pos;
}

uint8_t nand_audio_flush_buffer(uint8_t dev)
{
	uint32_t block;
	uint8_t result;
	if (!nand_audio_buffer.initialized || nand_audio_buffer.buffer_pos == 0) return FLASH_STATUS_OK;
	block = address_to_block(nand_audio_buffer.current_page_address, dev);
	if (is_block_bad(block, dev)) {
		uint32_t next_good_block = find_next_good_block(block + 1, dev);
		if (next_good_block == 0xFFFFFFFF) return FLASH_STATUS_ERROR;
		nand_audio_buffer.current_page_address = block_to_address(next_good_block, dev);
	}
	result = flash_PageProgram(nand_audio_buffer.current_page_address,
	                           nand_audio_buffer.buffer,
	                           nand_audio_buffer.buffer_pos,
	                           dev);
	if (result == FLASH_STATUS_OK) {
		nand_audio_buffer.buffer_pos = 0;
		nand_audio_buffer.current_page_address += NAND_AUDIO_PAGE_SIZE;
		return FLASH_STATUS_OK;
	} else {
		mark_block_as_bad(block, dev);
		return FLASH_STATUS_ERROR;
	}
}

uint8_t nand_smart_audio_write(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev)
{
	return nand_audio_write_buffered(address, data, size, dev);
}

uint8_t nand_smart_audio_read(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev)
{
	BG_flash_manager.ReadData(address, data, size, dev);
	return FLASH_STATUS_OK;
}

uint8_t nand_smart_audio_flush(uint8_t dev)
{
	return nand_audio_flush_buffer(dev);
}

#endif /* BANDATAHUB */
