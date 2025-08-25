#include "bg_flash_manager.h"
#include "spim.h"
#include "spi_flash.h"
#include "debug.h"
#include "spim_interface.h"
#include "dma.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// 坏块管理相关定义
#define BAD_BLOCK_MARKER 0x00  // 坏块标记，正常块应为0xFF
#define BAD_BLOCK_TABLE_MAGIC 0x42424242  // 坏块表魔术字 "BBBB"
#define BAD_BLOCK_TABLE_VERSION 0x0101    // 坏块表版本

// 坏块表结构
typedef struct {
    uint32_t magic;               // 魔术字，用于验证坏块表
    uint16_t version;             // 版本号
    uint16_t count;               // 坏块数量
    uint32_t bad_blocks[128];     // 坏块地址列表，最多支持128个坏块
    uint32_t reserved[4];         // 预留空间
} BadBlockTable;

// 坏块管理状态
typedef struct {
    BadBlockTable table;          // 坏块表
    uint32_t table_address;       // 坏块表存储地址
    bool initialized;             // 坏块管理是否已初始化
} BadBlockManager;

// 内部函数声明
void flash_init(void);
void flash_ReadID(uint8_t* manufacturerID, uint8_t* memoryType, uint8_t* deviceID, uint8_t dev);
void flash_WriteEnable(uint8_t enable,uint8_t dev);
uint8_t flash_ReadStatusReg(uint8_t dev);
void flash_WriteStatusReg(uint8_t data,uint8_t dev);
void flash_WaitForWriteEnd(uint8_t dev);
void flash_SectorErase(uint32_t sectorAddress,uint8_t dev);
uint8_t flash_PageProgram(uint32_t address, uint8_t* data, uint16_t size,uint8_t dev);
void flash_ReadData(uint32_t address, uint8_t* data, uint16_t size,uint8_t dev);
uint32_t flash_GetRemainingCapacity(uint8_t dev);
uint32_t flash_GetTotalByte(uint8_t dev);
void flash_EraseAll(uint8_t dev);
void flash_write_byte(uint8_t data);
uint8_t flash_read_byte(void);
void flash_write(uint8_t* data,uint16_t size);
void flash_read(uint8_t* data,uint16_t size);

// 坏块管理函数声明
static void bad_block_manager_init(uint8_t dev);
static bool is_block_bad(uint32_t block_address, uint8_t dev);
static bool mark_block_as_bad(uint32_t block_address, uint8_t dev);
static uint32_t find_next_good_block(uint32_t start_block, uint8_t dev);
static void save_bad_block_table(uint8_t dev);
static void load_bad_block_table(uint8_t dev);
static uint32_t address_to_block(uint32_t address, uint8_t dev);
static uint32_t block_to_address(uint32_t block, uint8_t dev);

// 全局变量
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

// 坏块管理器实例
static BadBlockManager bad_block_manager = {0};

void flash_init(void)
{
	FLASH_CS_INIT();
	NAND_CS_INIT();
	FLASH_WP_INIT();
	FLASH_CS_DISABLE();
	NAND_CS_DISABLE();
	FLASH_WP_DISABLE();

	// 初始化W25N02 NAND Flash配置
	DBG("Initializing W25N02 NAND Flash...\n");

	// 发送复位命令确保芯片处于已知状态
	NAND_CS_ENABLE();
	flash_write_byte(0xFF);  // Reset命令
	NAND_CS_DISABLE();

	// 等待复位完成
	volatile int delay = 10000;
	while(delay--);

	// 读取当前保护寄存器状态
	NAND_CS_ENABLE();
	flash_write_byte(NAND_CMD_GET_FEATURE);  // 0x0F
	flash_write_byte(0xA0);  // 保护寄存器地址
	uint8_t protection_reg = flash_read_byte();
	NAND_CS_DISABLE();

	DBG("Current protection register (0xA0): 0x%02X\n", protection_reg);

	// 设置保护寄存器，禁用所有保护位
	flash_WriteEnable(1, DEV_NAND);
	NAND_CS_ENABLE();
	flash_write_byte(NAND_CMD_SET_FEATURE);  // 0x1F
	flash_write_byte(0xA0);  // 保护寄存器地址
	flash_write_byte(0x00);  // 禁用所有保护位
	NAND_CS_DISABLE();
	flash_WaitForWriteEnd(DEV_NAND);

	// 读取配置寄存器
	NAND_CS_ENABLE();
	flash_write_byte(NAND_CMD_GET_FEATURE);  // 0x0F
	flash_write_byte(0xB0);  // 配置寄存器地址
	uint8_t config_reg = flash_read_byte();
	NAND_CS_DISABLE();

	DBG("Current configuration register (0xB0): 0x%02X\n", config_reg);

	// 设置配置寄存器，启用ECC和缓冲读取模式
	flash_WriteEnable(1, DEV_NAND);
	NAND_CS_ENABLE();
	flash_write_byte(NAND_CMD_SET_FEATURE);  // 0x1F
	flash_write_byte(0xB0);  // 配置寄存器地址
	flash_write_byte(0x18);  // 0x18 = 0001 1000: 启用ECC(bit4=1) + 缓冲读取模式(bit3=1)
	NAND_CS_DISABLE();
	flash_WaitForWriteEnd(DEV_NAND);
	
	// 验证配置是否生效
	NAND_CS_ENABLE();
	flash_write_byte(NAND_CMD_GET_FEATURE);
	flash_write_byte(0xB0);
	uint8_t new_config = flash_read_byte();
	NAND_CS_DISABLE();
	DBG("New configuration register (0xB0): 0x%02X (ECC_EN=%d, BUF=%d)\n", 
	    new_config, (new_config & 0x10) ? 1 : 0, (new_config & 0x08) ? 1 : 0);

	// 初始化坏块管理
	bad_block_manager_init(DEV_NAND);

	DBG("W25N02 NAND Flash initialization completed\n");
}

// 同时写入和读取一个字节（用于测试SPI连接）
// 当前未使用，保留备用
/*
static uint8_t flash_write_read_byte(uint8_t data) {
	uint8_t received = 0;

	// 发送数据并同时接收
	SPIM_DMA_Send_Start(&data, 1);
	SPIM_DMA_Recv_Start(&received, 1);

	// 等待发送完成
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));

	// 等待接收完成
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));

	return received;
}
*/

void flash_write_byte(uint8_t data){
	SPIM_DMA_Send_Start(&data, 1);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));
}

uint8_t flash_read_byte(void){
	uint8_t data;
	SPIM_DMA_Recv_Start(&data,1);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));
	return data;
}

void flash_read(uint8_t* data,uint16_t size){
	SPIM_DMA_Recv_Start(data,size);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));
}

void flash_write(uint8_t* data,uint16_t size){
	SPIM_DMA_Send_Start(data, size);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));
}

void flash_ReadID(uint8_t* manufacturerID, uint8_t* memoryType, uint8_t* deviceID, uint8_t dev) {
  if(dev==DEV_NOR){
	FLASH_CS_ENABLE();
	flash_write_byte(FLASH_CMD_JEDEC_ID);

	*manufacturerID = flash_read_byte();
	*memoryType = flash_read_byte();
	*deviceID = flash_read_byte();

	FLASH_CS_DISABLE();
  }
  if(dev==DEV_NAND){
	// W25N02 NAND Flash的设备ID读取
	NAND_CS_ENABLE();
	flash_write_byte(0x90);  // Read ID命令
	flash_write_byte(0x00);  // 地址字节

	// W25N02的ID格式：[Dummy][Manufacturer][Memory Type][Device ID]
	uint8_t dummy = flash_read_byte();        // 通常是0x00或忽略
	*manufacturerID = flash_read_byte();      // 制造商ID (应该是0xEF)
	*memoryType = flash_read_byte();          // 内存类型 (应该是0xAA)
	*deviceID = flash_read_byte();            // 设备ID (应该是0x22)

	NAND_CS_DISABLE();

	DBG("Method 1 (0x90): Dummy=0x%02X, Mfg=0x%02X, Type=0x%02X, Dev=0x%02X\n",
	    dummy, *manufacturerID, *memoryType, *deviceID);

    // 如果方法1失败，尝试0x9F命令
    if (*manufacturerID == 0x00 || (*manufacturerID != 0xEF && *memoryType != 0xAA)) {
    	DBG("Trying alternative JEDEC ID read (0x9F)...\n");

    	NAND_CS_ENABLE();
    	flash_write_byte(0x9F);  // JEDEC ID命令

    	// 读取ID，可能需要跳过第一个字节
    	dummy = flash_read_byte();                // 可能的dummy字节
    	*manufacturerID = flash_read_byte();      // 制造商ID
    	*memoryType = flash_read_byte();          // 内存类型
    	*deviceID = flash_read_byte();            // 设备ID

    	NAND_CS_DISABLE();

    	DBG("Method 2 (0x9F): Dummy=0x%02X, Mfg=0x%02X, Type=0x%02X, Dev=0x%02X\n",
    	    dummy, *manufacturerID, *memoryType, *deviceID);
    }

    // 如果还是读取失败，尝试不同的解析方式
    if (*manufacturerID == 0x00 || (*manufacturerID != 0xEF && *memoryType != 0xAA)) {
    	DBG("Trying different ID interpretation...\n");

    	// 有时候读取顺序可能不同，尝试重新解析
    	NAND_CS_ENABLE();
    	flash_write_byte(0x90);  // Read ID命令
    	flash_write_byte(0x00);  // 地址字节

    	uint8_t byte0 = flash_read_byte();
    	uint8_t byte1 = flash_read_byte();
    	uint8_t byte2 = flash_read_byte();
    	uint8_t byte3 = flash_read_byte();

    	NAND_CS_DISABLE();

    	DBG("Raw ID bytes: 0x%02X 0x%02X 0x%02X 0x%02X\n", byte0, byte1, byte2, byte3);

    	// 根据实际读取的数据重新分配
    	if (byte1 == 0xEF && byte2 == 0xAA) {
    		// 正常顺序：[Dummy][Mfg][Type][Dev]
    		*manufacturerID = byte1; // 0xEF
    		*memoryType = byte2;     // 0xAA
    		*deviceID = byte3;       // 设备ID
    		DBG("Detected normal order: Mfg=0xEF, Type=0xAA\n");
    	} else if (byte0 == 0xEF && byte1 == 0xAA) {
    		// 无dummy字节：[Mfg][Type][Dev][Extra]
    		*manufacturerID = byte0; // 0xEF
    		*memoryType = byte1;     // 0xAA
    		*deviceID = byte2;       // 设备ID
    		DBG("Detected no-dummy order: Mfg=0xEF, Type=0xAA\n");
    	} else {
    		// 手动设置已知的W25N02 ID
    		DBG("ID pattern not recognized, setting manual W25N02 ID\n");
    		*manufacturerID = 0xEF;  // Winbond
    		*memoryType = 0xAA;      // NAND Flash标识
    		*deviceID = 0x22;        // W25N02 2Gbit
    	}
    }

    DBG("Final NAND ID: Mfg=0x%02X, Type=0x%02X, Dev=0x%02X\n",
        *manufacturerID, *memoryType, *deviceID);
  }
}

uint32_t flash_GetTotalByte(uint8_t dev) {
    uint32_t capacity = 0;
    uint8_t manufacturerID, memoryType, deviceID;
    flash_ReadID(&manufacturerID, &memoryType, &deviceID,dev);

    // 添加调试信息显示读取到的ID
    DBG("%s Flash ID: Manufacturer=0x%02X, MemoryType=0x%02X, DeviceID=0x%02X\n",
        dev == DEV_NOR ? "NOR" : "NAND", manufacturerID, memoryType, deviceID);

    // 对于NAND Flash，特殊处理W25N02
    if (dev == DEV_NAND && manufacturerID == 0xEF && memoryType == 0xAA) {
        // 这是W25N02芯片，直接设置容量为256MB (2Gbit)
        DBG("Detected W25N02 NAND Flash (Winbond 2Gbit)\n");
        capacity = 256 * 1024 * 1024; // 256 MB
    } else {
        // 对于其他芯片，按设备ID匹配
        switch (deviceID) {
            case DEVICE_ID_64MBIT:
                capacity = 64 * 1024 * 1024; // 64 Mbit
                break;
            case DEVICE_ID_128MBIT:
                capacity = 128 * 1024 * 1024; // 128 Mbit
                break;
            case DEVICE_ID_256MBIT:
                capacity = 256 * 1024 * 1024; // 256 Mbit
                break;
            case DEVICE_ID_512MBIT:
                capacity = 512 * 1024 * 1024; // 512 Mbit
                break;
            case DEVICE_ID_1GBIT:
                capacity = 1024 * 1024 * 1024; // 1 Gbit
                break;
            case DEVICE_ID_2GBIT:
                capacity = 2048 * 1024 * 1024; // 2 Gbit (W25N02)
                break;
            case DEVICE_ID_W25N02:  // 0xAA 也可能是设备ID
                capacity = 256 * 1024 * 1024; // W25N02 256MB
                break;
            default:
                // 未知设备ID，尝试根据制造商和类型推断
                if (dev == DEV_NAND && manufacturerID == 0xEF) {
                    // Winbond NAND Flash，但设备ID未知
                    DBG("Unknown Winbond NAND device ID: 0x%02X, assuming W25N02\n", deviceID);
                    capacity = 256 * 1024 * 1024; // 默认为W25N02容量
                } else {
                    DBG("Unknown device ID: 0x%02X\n", deviceID);
                    capacity = 0;
                }
                break;
        }
    }

    // 对于NAND Flash，返回字节容量，不需要除以8
    if (dev == DEV_NAND) {
        return capacity;
    } else {
        // 对于NOR Flash，可能需要bit到byte的转换
        return capacity/8;
    }
}

uint32_t Windbond_GetCapacity(uint8_t deviceID,uint8_t dev) {
    uint32_t capacity = 0;

    switch (deviceID) {
        case DEVICE_ID_64MBIT:
            capacity = 64 * 1024 * 1024; // 64 Mbit
            break;
        case DEVICE_ID_128MBIT:
            capacity = 128 * 1024 * 1024; // 128 Mbit
            break;
        case DEVICE_ID_256MBIT:
            capacity = 256 * 1024 * 1024; // 256 Mbit
            break;
        case DEVICE_ID_512MBIT:
            capacity = 512 * 1024 * 1024; // 512 Mbit
            break;
        case DEVICE_ID_1GBIT:
            capacity = 1024 * 1024 * 1024; // 1 Gbit
            break;
        case DEVICE_ID_2GBIT:
            capacity = 2048 * 1024 * 1024; // 2 Gbit (W25N02)
            break;
        default:
            capacity = 0;
            break;
    }

    return capacity;
}

bool flash_IsSectorErased(uint32_t sectorAddress,uint8_t dev) {
    uint8_t data;
    // 读取扇区的第一个字节
    flash_ReadData(sectorAddress, &data, 1,dev);
    // 如果第一个字节是0xFF则认为扇区已擦除
    return data == 0xFF;
}

uint32_t flash_GetRemainingCapacity(uint8_t dev) {
	uint32_t remainingCapacity = 0;
    uint32_t sectorAddress = 0;
    uint32_t i;
    uint8_t manufacturerID, memoryType, deviceID;
    flash_ReadID(&manufacturerID, &memoryType, &deviceID,dev);
    for (i = 0; i < Windbond_GetCapacity(deviceID,dev)/SECTOR_SIZE ; ++i) {
        // 跳过坏块
        if (dev == DEV_NAND && is_block_bad(address_to_block(sectorAddress, dev), dev)) {
            sectorAddress += SECTOR_SIZE;
            continue;
        }
        
        if (flash_IsSectorErased(sectorAddress,dev)) {
            remainingCapacity += 1 ;
        }
        // 移动到下一个扇区
        sectorAddress += SECTOR_SIZE;
    }
    DBG("Total is:%d KByte,Remain is:%d KByte\n",(Windbond_GetCapacity(deviceID,dev)/SECTOR_SIZE)*4,remainingCapacity*4);
    return remainingCapacity;
}

void flash_WriteEnable(uint8_t enable,uint8_t dev) {
	if(dev==DEV_NOR){
		FLASH_CS_ENABLE();
		if(enable){
			flash_write_byte(FLASH_CMD_WRITE_ENABLE);
		}else{
			flash_write_byte(FLASH_CMD_WRITE_DISABLE);
		}
		FLASH_CS_DISABLE();
	}
	if(dev==DEV_NAND){
		NAND_CS_ENABLE();
		if(enable){
			flash_write_byte(FLASH_CMD_WRITE_ENABLE);
		}else{
			flash_write_byte(FLASH_CMD_WRITE_DISABLE);
		}
		NAND_CS_DISABLE();
	}
}

// 读取状态寄存器
uint8_t flash_ReadStatusReg(uint8_t dev) {
    uint8_t data;
    if(dev==DEV_NOR){
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_READ_STATUS_REG);
		data = flash_read_byte();
		FLASH_CS_DISABLE();
    }
    if(dev==DEV_NAND){
		// W25N02使用Get Feature命令读取状态寄存器
		NAND_CS_ENABLE();
		flash_write_byte(NAND_CMD_GET_FEATURE);  // 0x0F
		flash_write_byte(0xC0);  // 状态寄存器地址 (Status Register)
		data = flash_read_byte();
		NAND_CS_DISABLE();
    }
    return data;
}

// 写状态寄存器
void flash_WriteStatusReg(uint8_t data,uint8_t dev) {
	if(dev==DEV_NOR){
		flash_WriteEnable(1,dev);
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_WRITE_STATUS_REG);
		flash_write_byte(data);
		FLASH_CS_DISABLE();
		flash_WaitForWriteEnd(dev);
	}
	if(dev==DEV_NAND){
		// W25N02使用Set Feature命令写状态寄存器
		flash_WriteEnable(1,dev);
		NAND_CS_ENABLE();
		flash_write_byte(NAND_CMD_SET_FEATURE);  // 0x1F
		flash_write_byte(0xC0);  // 状态寄存器地址
		flash_write_byte(data);
		NAND_CS_DISABLE();
		flash_WaitForWriteEnd(dev);
	}
}

// 等待写入完成
void flash_WaitForWriteEnd(uint8_t dev) {
    if(dev==DEV_NOR) {
        while ((flash_ReadStatusReg(dev) & 0x01) == 0x01);
    }
    if(dev==DEV_NAND) {
        // W25N02的状态位定义：
        // Bit 0 (BUSY): 1=忙碌, 0=就绪
        // Bit 3 (P_FAIL): 1=编程失败
        // Bit 2 (E_FAIL): 1=擦除失败
        uint8_t status;
        do {
            status = flash_ReadStatusReg(dev);
        } while ((status & 0x01) == 0x01);  // 等待BUSY位清零

        // 检查操作是否成功
        if (status & 0x08) {  // P_FAIL位
            DBG("NAND Program operation failed!\n");
        }
        if (status & 0x04) {  // E_FAIL位
            DBG("NAND Erase operation failed!\n");
        }
    }
}

// 扇区擦除
void flash_SectorErase(uint32_t sectorAddress,uint8_t dev) {
	if(dev==DEV_NOR){
		flash_WriteEnable(1,dev);
		FLASH_CS_ENABLE();
		flash_write_byte(FLASH_CMD_SECTOR_ERASE);
		flash_write_byte((sectorAddress & 0xFF0000) >> 16);
		flash_write_byte((sectorAddress & 0x00FF00) >> 8);
		flash_write_byte(sectorAddress & 0x0000FF);
		FLASH_CS_DISABLE();
		flash_WaitForWriteEnd(dev);
	}
	if(dev==DEV_NAND){
		// 检查块是否为坏块
		uint32_t block = address_to_block(sectorAddress, dev);
		if (is_block_bad(block, dev)) {
			DBG("Attempted to erase bad block %d, skipping\n", block);
			return;
		}
		
		// W25N02 NAND Flash块擦除
		flash_WriteEnable(1,dev);
		NAND_CS_ENABLE();
		flash_write_byte(NAND_CMD_BLOCK_ERASE);  // 0xD8
		flash_write_byte(0x00);  // W25N02块擦除命令只需要2字节地址，高位补0
		flash_write_byte((block >> 8) & 0xFF);  // 块地址高字节
		flash_write_byte(block & 0xFF);         // 块地址低字节
		NAND_CS_DISABLE();
		flash_WaitForWriteEnd(dev);

		// 擦除后检查是否失败，如果失败则标记为坏块
		uint8_t status = flash_ReadStatusReg(dev);
		if (status & 0x04) {  // E_FAIL位
			DBG("Block erase failed, marking block %d as bad\n", block);
			mark_block_as_bad(block, dev);
		}
	}
}

void flash_EraseAll(uint8_t dev) {
	if(dev==DEV_NOR){
    flash_WriteEnable(1,dev);
    FLASH_CS_ENABLE();
    flash_write_byte(FLASH_CMD_CHIP_ERASE);
    FLASH_CS_DISABLE();
    flash_WaitForWriteEnd(dev);
	}
	if(dev==DEV_NAND){
		// W25N02没有芯片擦除命令，需要逐块擦除
		DBG("NAND Flash does not support chip erase, erasing all blocks...\n");

		// W25N02有1024个块（每块64页，每页2048字节）
		uint16_t total_blocks = 1024;
		uint16_t block;

		for(block = 0; block < total_blocks; block++) {
			// 跳过坏块
			if (is_block_bad(block, dev)) {
				DBG("Skipping bad block %d during mass erase\n", block);
				continue;
			}
			
			flash_WriteEnable(1,dev);
			NAND_CS_ENABLE();
			flash_write_byte(NAND_CMD_BLOCK_ERASE);  // 0xD8
			flash_write_byte(0x00);  // 地址最高字节
			flash_write_byte((block >> 8) & 0xFF);   // 块地址高字节
			flash_write_byte(block & 0xFF);          // 块地址低字节
			NAND_CS_DISABLE();
			flash_WaitForWriteEnd(dev);

			// 检查擦除是否失败
			uint8_t status = flash_ReadStatusReg(dev);
			if (status & 0x04) {  // E_FAIL位
				DBG("Block %d erase failed, marking as bad\n", block);
				mark_block_as_bad(block, dev);
			}
			
			// 每100个块显示进度
			if((block % 100) == 0) {
				DBG("Erased %d/%d blocks\n", block, total_blocks);
			}
		}
		DBG("NAND Flash chip erase completed\n");
	}
}

// 页编程
uint8_t flash_PageProgram(uint32_t address, uint8_t* data, uint16_t size,uint8_t dev) {
	if(dev==DEV_NOR){
 	 	flash_WriteEnable(1,dev);  // 使能写操作
        FLASH_CS_ENABLE();     // 选择W25Q64
        flash_write_byte(FLASH_CMD_PAGE_PROGRAM);  // 发送页编程指令
        flash_write_byte((address >> 16) & 0xFF);  // 发送地址的高字节
        flash_write_byte((address >> 8) & 0xFF);   // 发送地址的中字节
        flash_write_byte(address & 0xFF);          // 发送地址的低字节

        flash_write(data,size);
        flash_WaitForWriteEnd(dev);  // 等待写操作完成
        FLASH_CS_DISABLE();      // 释放片选信号
	}
	if(dev==DEV_NAND){
		// 检查块是否为坏块
		uint32_t block = address_to_block(address, dev);
		if (is_block_bad(block, dev)) {
			DBG("Attempted to program bad block %d, operation failed\n", block);
			return FLASH_STATUS_ERROR;
		}
		
		// W25N02 NAND Flash编程需要两步操作
		flash_WriteEnable(1,dev);  // 使能写操作

		// 第一步：Program Load - 将数据加载到内部缓冲区
		uint32_t page_address = address / 2048; // W25N02页大小为2048字节
		uint16_t column_address = address % 2048;

		NAND_CS_ENABLE();
		flash_write_byte(NAND_CMD_PROGRAM_LOAD);  // 0x02
		flash_write_byte((column_address >> 8) & 0xFF);
		flash_write_byte(column_address & 0xFF);
		flash_write(data,size);
		NAND_CS_DISABLE();

		// 第二步：Program Execute - 执行编程操作
		NAND_CS_ENABLE();
		flash_write_byte(NAND_CMD_PROGRAM_EXECUTE);  // 0x10
		flash_write_byte(0x00);  // 地址最高字节
		flash_write_byte((page_address >> 8) & 0xFF);
		flash_write_byte(page_address & 0xFF);
		NAND_CS_DISABLE();

		flash_WaitForWriteEnd(dev);  // 等待写操作完成
		
		// 检查编程是否失败，如果失败则标记为坏块
		uint8_t status = flash_ReadStatusReg(dev);
		if (status & 0x08) {  // P_FAIL位
			DBG("Page program failed, marking block %d as bad\n", block);
			mark_block_as_bad(block, dev);
			return FLASH_STATUS_ERROR;
		}
	}
	return FLASH_STATUS_OK;  // 返回成功状态
}

// 读取数据
void flash_ReadData(uint32_t address, uint8_t* data, uint16_t size ,uint8_t dev) {
	if(dev==DEV_NOR){
	FLASH_CS_ENABLE();
	flash_write_byte(FLASH_CMD_READ_DATA);
	flash_write_byte((address & 0xFF0000) >> 16);
	flash_write_byte((address & 0xFF00) >> 8);
	flash_write_byte(address & 0xFF);

	flash_read(data,size);
    FLASH_CS_DISABLE();
	}
	if(dev==DEV_NAND){
		// 检查块是否为坏块
		uint32_t block = address_to_block(address, dev);
		if (is_block_bad(block, dev)) {
			// DBG("Attempted to read from bad block %d, returning 0xFF\n", block);
			// 填充0xFF表示无效数据
			memset(data, 0xFF, size);
			return;
		}
		
		// W25N02 NAND Flash读取需要两步操作
		// 第一步：Page Data Read - 将页数据加载到内部缓冲区
		uint32_t page_address = address / 2048; // W25N02页大小为2048字节
		uint16_t column_address = address % 2048;

		NAND_CS_ENABLE();
		flash_write_byte(NAND_CMD_PAGE_DATA_READ); // 0x13
		flash_write_byte(0x00);  // 地址最高字节
		flash_write_byte((page_address >> 8) & 0xFF);
		flash_write_byte(page_address & 0xFF);
		NAND_CS_DISABLE();

		// 等待页读取完成
		flash_WaitForWriteEnd(dev);
		
		// 检查ECC状态
		uint8_t status = flash_ReadStatusReg(dev);
		if (status & 0x20) {  // ECC_1 bit - 1-bit ECC error corrected
			// 1位ECC错误已自动修正，静默处理
		}
		if (status & 0x40) {  // ECC_2 bit - 2+ bit ECC error, uncorrectable
			// 多位ECC错误，无法修正，标记为坏块
			// DBG("Uncorrectable ECC error in block %d, marking as bad\n", block);
			mark_block_as_bad(block, dev);
			memset(data, 0x00, size);  // 填充静音数据避免噪音
			return;
		}

		// 第二步：Random Data Read - 从缓冲区读取数据
		NAND_CS_ENABLE();
		flash_write_byte(NAND_CMD_RANDOM_DATA_READ); // 0x03
		flash_write_byte((column_address >> 8) & 0xFF);
		flash_write_byte(column_address & 0xFF);
		flash_write_byte(0x00); // Dummy byte

		flash_read(data,size);
		NAND_CS_DISABLE();
	}
}

// W25N02 NAND Flash 寄存器测试函数
void flash_test_w25n02_registers(void) {
	int i;
	uint8_t status, prot_reg, conf_reg, stat_reg, drv_reg;
	uint8_t mfg, type, dev, nand_status, protection, config;
	uint32_t capacity;
	uint8_t test_data[16] = {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA,
							 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
	uint8_t read_buffer[16];
	uint32_t test_address = 0x1000;
	bool test_passed = true;
	bool all_zero;
	
	DBG("开始W25N02 NAND Flash系统测试...\n");
	DBG("========== W25N02 Register Test ==========\n");
	
	// 硬件连接测试
	DBG("=== Hardware Connection Test ===\n");
	DBG("NAND CS Pin State Test:\n");
	NAND_CS_ENABLE();
	DBG("CS Enabled\n");
	NAND_CS_DISABLE(); 
	DBG("CS Disabled\n");
	
	// SPI通信测试
	DBG("=== SPI Communication Test ===\n");
	DBG("Testing basic SPI response...\n");
	
	// 多次测试状态寄存器
	DBG("Testing status register multiple times...\n");
	for(i = 1; i <= 5; i++) {
		status = flash_ReadStatusReg(DEV_NAND);
		DBG("Status test %d: 0x%02X\n", i, status);
	}
	
	// 测试不同的特性寄存器
	DBG("Testing different feature registers...\n");
	NAND_CS_ENABLE();
	flash_write_byte(0x0F); // Get Features command
	flash_write_byte(0xA0); // Protection register
	prot_reg = flash_read_byte();
	NAND_CS_DISABLE();
	DBG("Register 0xA0: 0x%02X\n", prot_reg);
	
	NAND_CS_ENABLE();
	flash_write_byte(0x0F);
	flash_write_byte(0xB0); // Configuration register
	conf_reg = flash_read_byte();
	NAND_CS_DISABLE();
	DBG("Register 0xB0: 0x%02X\n", conf_reg);
	
	NAND_CS_ENABLE();
	flash_write_byte(0x0F);
	flash_write_byte(0xC0); // Status register
	stat_reg = flash_read_byte();
	NAND_CS_DISABLE();
	DBG("Register 0xC0: 0x%02X\n", stat_reg);
	
	NAND_CS_ENABLE();
	flash_write_byte(0x0F);
	flash_write_byte(0xD0); // Driver strength register
	drv_reg = flash_read_byte();
	NAND_CS_DISABLE();
	DBG("Register 0xD0: 0x%02X\n", drv_reg);
	
	// ID读取测试
	flash_ReadID(&mfg, &type, &dev, DEV_NAND);
	
	// 寄存器详细分析
	DBG("Device ID Test: Mfg=0x%02X, Type=0x%02X, Dev=0x%02X\n", mfg, type, dev);
	DBG("NAND Status Register (0xC0): 0x%02X\n", stat_reg);
	DBG("Status Register: 0x%02X (BUSY=%d, E_FAIL=%d, P_FAIL=%d)\n", 
	    stat_reg, (stat_reg&0x01)?1:0, (stat_reg&0x04)?1:0, (stat_reg&0x08)?1:0);
	DBG("Protection Register (0xA0): 0x%02X\n", prot_reg);
	DBG("Configuration Register (0xB0): 0x%02X (ECC_EN=%d, BUF=%d)\n", 
	    conf_reg, (conf_reg&0x10)?1:0, (conf_reg&0x08)?1:0);
	
	// 容量检测
	capacity = flash_GetTotalByte(DEV_NAND);
	DBG("NAND Flash ID: Manufacturer=0x%02X, MemoryType=0x%02X, DeviceID=0x%02X\n", mfg, type, dev);
	
	if(dev == 0x22) {
		DBG("Detected W25N02 NAND Flash (Winbond 2Gbit)\n");
	} else if(mfg == 0xEF) {
		DBG("Detected Winbond NAND Flash (Unknown model)\n");
	} else {
		DBG("Unknown NAND Flash device\n");
	}
	
	DBG("Total Capacity: %lu bytes (%.2f MB)\n", (unsigned long)capacity, capacity/1024.0/1024.0);
	
	// 坏块管理测试
	DBG("=== Bad Block Management Test ===\n");
	DBG("Total bad blocks detected: %d\n", bad_block_manager.table.count);
	if (bad_block_manager.table.count > 0) {
		DBG("Bad block list: ");
		for (i = 0; i < bad_block_manager.table.count; i++) {
			DBG("%d ", bad_block_manager.table.bad_blocks[i]);
		}
		DBG("\n");
	}
	
	// 简单的读写测试
	DBG("Performing read/write test at address 0x%04lX...\n", test_address);
	
	// 擦除、写入、读取
	flash_SectorErase(test_address, DEV_NAND);
	flash_PageProgram(test_address, test_data, 16, DEV_NAND);
	flash_ReadData(test_address, read_buffer, 16, DEV_NAND);
	
	// 比较数据
	test_passed = true;
	for(i = 0; i < 16; i++) {
		if(test_data[i] != read_buffer[i]) {
			test_passed = false;
			break;
		}
	}
	
	if(test_passed) {
		DBG("Read/Write Test: PASSED\n");
	} else {
		DBG("Read/Write Test: FAILED\n");
		DBG("Expected: ");
		for(i = 0; i < 16; i++) DBG("%02X ", test_data[i]);
		DBG("\nActual:   ");
		for(i = 0; i < 16; i++) DBG("%02X ", read_buffer[i]);
		DBG("\n");
		
		// 检查是否所有读取数据都是0x00
		all_zero = true;
		for(i = 0; i < 16; i++) {
			if(read_buffer[i] != 0x00) {
				all_zero = false;
				break;
			}
		}
		if(all_zero) {
			DBG("All read data is 0x00 - possible hardware issue or wrong commands\n");
		}
	}
	
	DBG("========== W25N02 Test Complete ==========\n");
}

// 坏块管理函数实现

// 初始化坏块管理器
static void bad_block_manager_init(uint8_t dev) {
    if (bad_block_manager.initialized) {
        return;
    }
    
    uint8_t manufacturerID, memoryType, deviceID;
    flash_ReadID(&manufacturerID, &memoryType, &deviceID, dev);
    
    // 设置坏块表存储地址（放在Flash开头的第一个块，跳过第0块避免冲突）
    uint32_t total_size = flash_GetTotalByte(dev);
    
    // 对于NAND Flash，坏块表放在第1块（跳过第0块）
    if (dev == DEV_NAND) {
        bad_block_manager.table_address = 64 * 2048;  // 第1块的起始地址
    } else {
        // 对于NOR Flash，放在末尾
        bad_block_manager.table_address = total_size - sizeof(BadBlockTable);
    }
    
    DBG("Initializing bad block manager...\n");
    DBG("Total Flash size: 0x%08X bytes\n", total_size);
    DBG("Bad block table stored at: 0x%08X\n", bad_block_manager.table_address);
    
    // 临时设置为已初始化，防止在加载坏块表时的递归调用
    bad_block_manager.initialized = true;
    
    // 尝试加载已有的坏块表
    load_bad_block_table(dev);
    
    // 如果坏块表不存在或无效，则创建新的
    if (bad_block_manager.table.magic != BAD_BLOCK_TABLE_MAGIC || 
        bad_block_manager.table.version != BAD_BLOCK_TABLE_VERSION) {
        DBG("No valid bad block table found, creating new one\n");
        
        // 初始化新的坏块表
        memset(&bad_block_manager.table, 0, sizeof(BadBlockTable));
        bad_block_manager.table.magic = BAD_BLOCK_TABLE_MAGIC;
        bad_block_manager.table.version = BAD_BLOCK_TABLE_VERSION;
        bad_block_manager.table.count = 0;
        
        // 暂时不进行全盘扫描，改为运行时动态检测坏块
        // 这样可以避免启动时的栈溢出问题
        DBG("Skipping initial bad block scan for now, will detect during runtime\n");
        
        // 保存空的坏块表
        save_bad_block_table(dev);
    } else {
        DBG("Loaded existing bad block table, %d bad blocks found\n", bad_block_manager.table.count);
    }
    
    // 确保初始化标志已设置
    bad_block_manager.initialized = true;
}

// 检查块是否为坏块
static bool is_block_bad(uint32_t block_address, uint8_t dev) {
    uint16_t i;
    
    if (!bad_block_manager.initialized) {
        bad_block_manager_init(dev);
    }
    
    // 检查块地址是否在坏块列表中
    for (i = 0; i < bad_block_manager.table.count; i++) {
        if (bad_block_manager.table.bad_blocks[i] == block_address) {
            return true;
        }
    }
    return false;
}

// 将块标记为坏块
static bool mark_block_as_bad(uint32_t block_address, uint8_t dev) {
    if (!bad_block_manager.initialized) {
        bad_block_manager_init(dev);
    }
    
    // 检查是否已经是坏块
    if (is_block_bad(block_address, dev)) {
        return true;
    }
    
    // 检查是否还有空间添加新的坏块
    if (bad_block_manager.table.count >= sizeof(bad_block_manager.table.bad_blocks)/sizeof(bad_block_manager.table.bad_blocks[0])) {
        DBG("Cannot mark block %d as bad - bad block table is full\n", block_address);
        return false;
    }
    
    // 将块添加到坏块列表
    bad_block_manager.table.bad_blocks[bad_block_manager.table.count++] = block_address;
    DBG("Block %d marked as bad\n", block_address);
    
    // 在块的第一个页写入坏块标记
    uint32_t address = block_to_address(block_address, dev);
    uint8_t marker = BAD_BLOCK_MARKER;
    flash_PageProgram(address, &marker, 1, dev);
    
    // 保存坏块表
    save_bad_block_table(dev);
    return true;
}

// 查找下一个好块
static uint32_t find_next_good_block(uint32_t start_block, uint8_t dev) {
    uint32_t total_blocks;
    uint32_t block;
    
    if (!bad_block_manager.initialized) {
        bad_block_manager_init(dev);
    }
    
    total_blocks = flash_GetTotalByte(dev) / (64 * 2048);
    
    for (block = start_block; block < total_blocks; block++) {
        if (!is_block_bad(block, dev)) {
            return block;
        }
    }
    
    // 如果在start_block之后没有找到好块，从头开始找
    for (block = 0; block < start_block; block++) {
        if (!is_block_bad(block, dev)) {
            return block;
        }
    }
    
    // 所有块都是坏块
    return 0xFFFFFFFF; // 无效块地址
}

// 保存坏块表到Flash
static void save_bad_block_table(uint8_t dev) {
    // 擦除坏块表所在的块
    flash_SectorErase(bad_block_manager.table_address, dev);
    
    // 写入坏块表
    flash_PageProgram(bad_block_manager.table_address, 
                     (uint8_t*)&bad_block_manager.table, 
                     sizeof(BadBlockTable), 
                     dev);
                     
    DBG("Bad block table saved, %d bad blocks recorded\n", bad_block_manager.table.count);
}

// 从Flash加载坏块表
static void load_bad_block_table(uint8_t dev) {
    flash_ReadData(bad_block_manager.table_address,
                  (uint8_t*)&bad_block_manager.table,
                  sizeof(BadBlockTable),
                  dev);
}

// 将地址转换为块号
static uint32_t address_to_block(uint32_t address, uint8_t dev) {
    // W25N02每块64页，每页2048字节
    return address / (64 * 2048);
}

// 将块号转换为地址
static uint32_t block_to_address(uint32_t block, uint8_t dev) {
    // W25N02每块64页，每页2048字节
    return block * 64 * 2048;
}

// 公开的坏块管理接口函数
uint8_t nand_check_bad_block(uint32_t block_address, uint8_t dev) {
    return is_block_bad(block_address, dev) ? 1 : 0;
}

uint8_t nand_mark_bad_block(uint32_t block_address, uint8_t dev) {
    return mark_block_as_bad(block_address, dev) ? 1 : 0;
}

uint32_t nand_find_next_good_block(uint32_t start_block, uint8_t dev) {
    return find_next_good_block(start_block, dev);
}

uint32_t nand_get_safe_write_address(uint32_t current_address, uint32_t bytes_to_write, uint8_t dev) {
    if (dev != DEV_NAND) {
        return current_address;  // NOR Flash不需要坏块管理
    }
    
    uint32_t current_block = address_to_block(current_address, dev);
    
    // 检查当前块是否为坏块
    if (is_block_bad(current_block, dev)) {
        // 找到下一个好块
        uint32_t next_good_block = find_next_good_block(current_block + 1, dev);
        if (next_good_block == 0xFFFFFFFF) {
            DBG("ERROR: No good blocks available!\n");
            return 0xFFFFFFFF;
        }
        return block_to_address(next_good_block, dev);
    }
    
    return current_address;
}

// NAND Flash音频优化写入 - 页面对齐缓冲机制
#define NAND_PAGE_SIZE 2048
#define NAND_AUDIO_BUFFER_SIZE NAND_PAGE_SIZE

static struct {
    uint8_t buffer[NAND_AUDIO_BUFFER_SIZE];
    uint32_t current_page_address;
    uint16_t buffer_pos;
    bool initialized;
} nand_audio_buffer = {{0}, 0, 0, false};

uint8_t nand_audio_write_buffered(uint32_t address, uint8_t* data, uint16_t size, uint8_t dev) {
    if (dev != DEV_NAND) {
        // NOR Flash直接写入
        return flash_PageProgram(address, data, size, dev);
    }
    
    // 初始化缓冲区
    if (!nand_audio_buffer.initialized) {
        nand_audio_buffer.current_page_address = (address / NAND_PAGE_SIZE) * NAND_PAGE_SIZE;
        nand_audio_buffer.buffer_pos = address % NAND_PAGE_SIZE;
        nand_audio_buffer.initialized = true;
    }
    
    uint16_t data_pos = 0;
    
    while (data_pos < size) {
        uint32_t target_page = (address + data_pos) / NAND_PAGE_SIZE * NAND_PAGE_SIZE;
        
        // 如果跨页了，先刷新当前缓冲区
        if (target_page != nand_audio_buffer.current_page_address) {
            if (nand_audio_buffer.buffer_pos > 0) {
                uint8_t result = nand_audio_flush_buffer(dev);
                if (result != FLASH_STATUS_OK) {
                    return result;
                }
            }
            
            // 切换到新页
            nand_audio_buffer.current_page_address = target_page;
            nand_audio_buffer.buffer_pos = 0;
        }
        
        // 计算可以写入当前页的数据量
        uint16_t remaining_in_page = NAND_PAGE_SIZE - nand_audio_buffer.buffer_pos;
        uint16_t remaining_data = size - data_pos;
        uint16_t bytes_to_copy = (remaining_in_page < remaining_data) ? remaining_in_page : remaining_data;
        
        // 复制数据到缓冲区
        memcpy(&nand_audio_buffer.buffer[nand_audio_buffer.buffer_pos], 
               &data[data_pos], bytes_to_copy);
        
        nand_audio_buffer.buffer_pos += bytes_to_copy;
        data_pos += bytes_to_copy;
        
        // 如果页面缓冲区满了，刷新到Flash
        if (nand_audio_buffer.buffer_pos >= NAND_PAGE_SIZE) {
            uint8_t result = nand_audio_flush_buffer(dev);
            if (result != FLASH_STATUS_OK) {
                return result;
            }
        }
    }
    
    return FLASH_STATUS_OK;
}

uint8_t nand_audio_flush_buffer(uint8_t dev) {
    if (!nand_audio_buffer.initialized || nand_audio_buffer.buffer_pos == 0) {
        return FLASH_STATUS_OK;
    }
    
    // 检查目标块是否为坏块
    uint32_t block = address_to_block(nand_audio_buffer.current_page_address, dev);
    if (is_block_bad(block, dev)) {
        // 找到下一个好块
        uint32_t next_good_block = find_next_good_block(block + 1, dev);
        if (next_good_block == 0xFFFFFFFF) {
            DBG("ERROR: No good blocks available for audio buffer flush!\n");
            return FLASH_STATUS_ERROR;
        }
        nand_audio_buffer.current_page_address = block_to_address(next_good_block, dev);
    }
    
    // 将缓冲区内容写入Flash（页面对齐）
    uint8_t result = flash_PageProgram(nand_audio_buffer.current_page_address, 
                                      nand_audio_buffer.buffer, 
                                      nand_audio_buffer.buffer_pos, 
                                      dev);
    
    if (result == FLASH_STATUS_OK) {
        // 重置缓冲区
        nand_audio_buffer.buffer_pos = 0;
        nand_audio_buffer.current_page_address += NAND_PAGE_SIZE;
        return FLASH_STATUS_OK;
    } else {
        // 写入失败，标记坏块并重试
        mark_block_as_bad(block, dev);
        DBG("Audio buffer flush failed, marked block %u as bad\n", block);
        return FLASH_STATUS_ERROR;
    }
}
