#include "bg_flash_manager.h"
#include "spim.h"
#include "spi_flash.h"
#include "debug.h"
#include "spim_interface.h"
#include "dma.h"
#include <stdint.h>
#include <string.h>


void flash_init(void);
void flash_ReadID(uint8_t* manufacturerID, uint8_t* memoryType, uint8_t* deviceID,uint8_t dev);
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
BG_Flash_Manager BG_flash_manager = {

	.Init = flash_init,
	.PageProgram = flash_PageProgram,
	.SectorErase = flash_SectorErase,
	.WriteEnable = flash_WriteEnable,
	.ReadData = flash_ReadData,
	.ReadID = flash_ReadID,
	.GetRemainingCapacity = flash_GetRemainingCapacity,
	.GetTotalByte = flash_GetTotalByte,
	.EraseAll = flash_EraseAll
};

void flash_init(void)
{
	FLASH_CS_INIT();
	NAND_CS_INIT();
//	FLASH_HOLD_INIT();
	FLASH_WP_INIT();
	FLASH_CS_DISABLE();
//	FLASH_HOLD_ENABLE();
	FLASH_WP_DISABLE();

}


void flash_write_byte(uint8_t data){

	SPIM_DMA_Send_Start(&data, 1);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));

}

void flash_write(uint8_t *data,uint16_t size){

	SPIM_DMA_Send_Start(data, size);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));

}
uint8_t flash_read_byte(){

	uint8_t data;
	SPIM_DMA_Recv_Start(&data,1);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));

	return data;
}
void flash_read(uint8_t* data,uint16_t size){


	SPIM_DMA_Recv_Start(data,size);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));

}



void flash_ReadID(uint8_t* manufacturerID, uint8_t* memoryType, uint8_t* deviceID,uint8_t dev) {
  if(dev==DEV_NOR){
	// 发送JEDEC ID命令
	FLASH_CS_ENABLE(); // 拉低CS引脚以开始通信
	flash_write_byte(FLASH_CMD_JEDEC_ID); // 发送JEDEC ID命令

    // 读取ID
    *manufacturerID = flash_read_byte(); // 读取制造商ID
    *memoryType = flash_read_byte();     // 读取内存类型
    *deviceID = flash_read_byte();       // 读取设备ID

    FLASH_CS_DISABLE(); // 拉高CS引脚以结束通信
  }
  if(dev==DEV_NAND){
		// 发送JEDEC ID命令
		NAND_CS_ENABLE(); // 拉低CS引脚以开始通信
		flash_write_byte(FLASH_CMD_JEDEC_ID); // 发送JEDEC ID命令
		flash_read_byte();
	    // 读取ID
	    *manufacturerID = flash_read_byte(); // 读取制造商ID
	    *memoryType = flash_read_byte();     // 读取内存类型
	    *deviceID = flash_read_byte();       // 读取设备ID

	    NAND_CS_DISABLE(); // 拉高CS引脚以结束通信
  }
}

uint32_t flash_GetTotalByte(uint8_t dev) {
    uint32_t capacity = 0;
    uint8_t manufacturerID, memoryType, deviceID;
    flash_ReadID(&manufacturerID, &memoryType, &deviceID,dev);
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
        default:
            // 未知的设备ID，无法确定容量
            capacity = 0;
            break;
    }

    return capacity/8;
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
        default:
            // 未知的设备ID，无法确定容量
            capacity = 0;
            break;
    }

    return capacity;
}


bool flash_IsSectorErased(uint32_t sectorAddress,uint8_t dev) {
    uint8_t data;
    // 读取扇区的第一个字节
    flash_ReadData(sectorAddress, &data, 1,dev);
    // 如果第一个字节是0xFF，则扇区被擦除
    return data == 0xFF;
}

uint32_t flash_GetRemainingCapacity(uint8_t dev) {

	uint32_t remainingCapacity = 0;
    uint32_t sectorAddress = 0;
    uint32_t i;
    uint8_t manufacturerID, memoryType, deviceID;
    flash_ReadID(&manufacturerID, &memoryType, &deviceID,dev);
    for (i = 0; i < Windbond_GetCapacity(deviceID,dev)/SECTOR_SIZE ; ++i) {
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
    		NAND_CS_ENABLE();
    		flash_write_byte(FLASH_CMD_READ_STATUS_REG);
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
		flash_WriteEnable(1,dev);
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_WRITE_STATUS_REG);
		flash_write_byte(data);
		NAND_CS_DISABLE();
		flash_WaitForWriteEnd(dev);
	}
}

// 等待写入结束
void flash_WaitForWriteEnd(uint8_t dev) {
    while ((flash_ReadStatusReg(dev) & 0x01) == 0x01);
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
		flash_WriteEnable(1,dev);
		NAND_CS_ENABLE();
		flash_write_byte(FLASH_CMD_SECTOR_ERASE);
		flash_write_byte((sectorAddress & 0xFF0000) >> 16);
		flash_write_byte((sectorAddress & 0x00FF00) >> 8);
		flash_write_byte(sectorAddress & 0x0000FF);
		NAND_CS_DISABLE();
		flash_WaitForWriteEnd(dev);
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
    flash_WriteEnable(1,dev);
    NAND_CS_ENABLE();
    flash_write_byte(0xFF);
    NAND_CS_DISABLE();
    flash_WaitForWriteEnd(dev);
	}
}



// 页编程
uint8_t flash_PageProgram(uint32_t address, uint8_t* data, uint16_t size,uint8_t dev) {
//    uint16_t page_remain;
//    uint16_t offset = 0;
//    uint16_t i;
////    uint8_t buffer[100];
//    while (size > 0) {
//        page_remain = PAGE_SIZE - (address % PAGE_SIZE);
//        if (size < page_remain) {
//            page_remain = size;  // 如果剩余数据不足一页，只写入剩余数据
//        }
//
//        //flash_SectorErase(address);  // 擦除页面
//
//        flash_WriteEnable(1);  // 使能写操作
//        FLASH_CS_ENABLE();     // 选中W25Q64
//        flash_write_byte(FLASH_CMD_PAGE_PROGRAM);  // 发送页编程指令
//        flash_write_byte((address >> 16) & 0xFF);  // 发送地址的高字节
//        flash_write_byte((address >> 8) & 0xFF);   // 发送地址的中字节
//        flash_write_byte(address & 0xFF);          // 发送地址的低字节
//
////        memcpy(buffer, data, 96);
////        flash_write(buffer,96);
//
//        for (i = 0; i < page_remain; i++) {
//        	//DBG("data is %x\n",data[offset + i]);
//            flash_write_byte(data[offset + i]);  // 写入数据
//        }
//
//        flash_WaitForWriteEnd();  // 等待写入结束
//        FLASH_CS_DISABLE();      // 释放片选信号
//
//        address += page_remain;  // 更新地址
//        offset += page_remain;   // 更新数据偏移
//        size -= page_remain;     // 更新剩余数据大小
//    }
//    return FLASH_STATUS_OK;  // 返回成功状态
			if(dev==DEV_NOR){
	 	 	flash_WriteEnable(1,dev);  // 使能写操作
	        FLASH_CS_ENABLE();     // 选中W25Q64
	        flash_write_byte(FLASH_CMD_PAGE_PROGRAM);  // 发送页编程指令
	        flash_write_byte((address >> 16) & 0xFF);  // 发送地址的高字节
	        flash_write_byte((address >> 8) & 0xFF);   // 发送地址的中字节
	        flash_write_byte(address & 0xFF);          // 发送地址的低字节


	        flash_write(data,size);
	        flash_WaitForWriteEnd(dev);  // 等待写入结束
	        FLASH_CS_DISABLE();      // 释放片选信号
			}
			if(dev==DEV_NAND){
	 	 	flash_WriteEnable(1,dev);  // 使能写操作
	        NAND_CS_ENABLE();     // 选中W25Q64
	        flash_write_byte(FLASH_CMD_PAGE_PROGRAM);  // 发送页编程指令
	        flash_write_byte((address >> 16) & 0xFF);  // 发送地址的高字节
	        flash_write_byte((address >> 8) & 0xFF);   // 发送地址的中字节
	        flash_write_byte(address & 0xFF);          // 发送地址的低字节


	        flash_write(data,size);
	        flash_WaitForWriteEnd(dev);  // 等待写入结束
	        NAND_CS_DISABLE();      // 释放片选信号
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
	NAND_CS_ENABLE();
	flash_write_byte(FLASH_CMD_READ_DATA);
	flash_write_byte((address & 0xFF0000) >> 16);
	flash_write_byte((address & 0xFF00) >> 8);
	flash_write_byte(address & 0xFF);

	flash_read(data,size);
	NAND_CS_DISABLE();
	}
}
