#include "bg_flash_manager.h"
#include "delay.h"

#include <stdint.h>
#include <string.h>


void flash_init(void (*Write)(uint8_t*,uint16_t),void (*Read)(uint8_t*,uint16_t));
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
void Erase_Write_data_Sector(uint32_t Address,uint32_t Write_data_NUM,uint8_t dev);
void Write_Page(uint32_t WriteAddr,uint8_t* pBuffer,uint16_t NumByteToWrite,uint8_t dev);
BG_Flash_Manager BG_flash_manager = {

	.Init = flash_init,
	.PageProgram = Write_Page,
	.SectorErase = flash_SectorErase,
	.DataErase = Erase_Write_data_Sector,
	.WriteEnable = flash_WriteEnable,
	.ReadData = flash_ReadData,
	.ReadID = flash_ReadID,
	.GetRemainingCapacity = flash_GetRemainingCapacity,
	.GetTotalByte = flash_GetTotalByte,
	.EraseAll = flash_EraseAll
};

void flash_init(void (*Write)(uint8_t*,uint16_t),void (*Read)(uint8_t*,uint16_t))
{
		BG_flash_manager.Write = Write;
		BG_flash_manager.Read = Read;

}


void flash_write_byte(uint8_t data){

	BG_flash_manager.Write(&data, 1);


}

void flash_write(uint8_t *data,uint16_t size){

	BG_flash_manager.Write(data, size);


}
uint8_t flash_read_byte(){

	uint8_t data;
	BG_flash_manager.Read(&data, 1);

	return data;
}
void flash_read(uint8_t* data,uint16_t size){


		BG_flash_manager.Read(data, size);

}



void flash_ReadID(uint8_t* manufacturerID, uint8_t* memoryType, uint8_t* deviceID,uint8_t dev) {
  if(dev==DEV_NOR){
	// ����JEDEC ID����
	FLASH_CS_ENABLE(); // ����CS�����Կ�ʼͨ��
	flash_write_byte(FLASH_CMD_JEDEC_ID); // ����JEDEC ID����

    // ��ȡID
    *manufacturerID = flash_read_byte(); // ��ȡ������ID
    *memoryType = flash_read_byte();     // ��ȡ�ڴ�����
    *deviceID = flash_read_byte();       // ��ȡ�豸ID

    FLASH_CS_DISABLE(); // ����CS�����Խ���ͨ��
  }
  if(dev==DEV_NAND){
		// ����JEDEC ID����
		NAND_CS_ENABLE(); // ����CS�����Կ�ʼͨ��
		flash_write_byte(FLASH_CMD_JEDEC_ID); // ����JEDEC ID����
		flash_read_byte();
	    // ��ȡID
	    *manufacturerID = flash_read_byte(); // ��ȡ������ID
	    *memoryType = flash_read_byte();     // ��ȡ�ڴ�����
	    *deviceID = flash_read_byte();       // ��ȡ�豸ID

	    NAND_CS_DISABLE(); // ����CS�����Խ���ͨ��
  }
}

uint32_t flash_GetTotalByte(uint8_t dev) {
    uint32_t capacity = 0;
    uint8_t manufacturerID, memoryType, deviceID;
    flash_ReadID(&manufacturerID, &memoryType, &deviceID,dev);

		usb_printf("ID is %d  %d  %d \n",manufacturerID ,memoryType ,deviceID);
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
            // δ֪���豸ID���޷�ȷ������
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
            // δ֪���豸ID���޷�ȷ������
            capacity = 0;
            break;
    }

    return capacity;
}


bool flash_IsSectorErased(uint32_t sectorAddress,uint8_t dev) {
    uint8_t data;
    // ��ȡ�����ĵ�һ���ֽ�
    flash_ReadData(sectorAddress, &data, 1,dev);
    // �����һ���ֽ���0xFF��������������
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
        // �ƶ�����һ������
        sectorAddress += SECTOR_SIZE;
    }
    usb_printf("Total is:%d KByte,Remain is:%d KByte\n",(Windbond_GetCapacity(deviceID,dev)/SECTOR_SIZE)*4,remainingCapacity*4);
    return remainingCapacity*64;
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


// ��ȡ״̬�Ĵ���
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

// д״̬�Ĵ���
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

// �ȴ�д�����
void flash_WaitForWriteEnd(uint8_t dev) {
    while ((flash_ReadStatusReg(dev) & 0x01) == 0x01);
}




// ��������
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
//������ַ���ڵ�����
void Erase_Write_data_Sector(uint32_t Address,uint32_t Write_data_NUM,uint8_t dev)
{
	uint16_t i;
	//�ܹ�4096������
	//���� д�����ݿ�ʼ�ĵ�ַ + Ҫд�����ݸ���������ַ ����������
	if(dev==DEV_NOR){
		uint16_t Star_Sector,End_Sector,Num_Sector;
		Star_Sector = Address / 4096;						//����д�뿪ʼ������
		End_Sector = (Address + Write_data_NUM) / 4096;		//����д�����������
		Num_Sector = End_Sector - Star_Sector;  			//����д��缸������

		//��ʼ��������
		for(i=0;i <= Num_Sector;i++)
		{
			flash_SectorErase(Address,dev);
			Address += 4095;
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
    flash_WriteEnable(1,dev);
    NAND_CS_ENABLE();
    flash_write_byte(0xFF);
    NAND_CS_DISABLE();
    flash_WaitForWriteEnd(dev);
	}
}



// ҳ���
uint8_t flash_PageProgram(uint32_t address, uint8_t* data, uint16_t size,uint8_t dev) {
//    uint16_t page_remain;
//    uint16_t offset = 0;
//    uint16_t i;
////    uint8_t buffer[100];
//    while (size > 0) {
//        page_remain = PAGE_SIZE - (address % PAGE_SIZE);
//        if (size < page_remain) {
//            page_remain = size;  // ���ʣ�����ݲ���һҳ��ֻд��ʣ������
//        }
//
//        //flash_SectorErase(address);  // ����ҳ��
//
//        flash_WriteEnable(1);  // ʹ��д����
//        FLASH_CS_ENABLE();     // ѡ��W25Q64
//        flash_write_byte(FLASH_CMD_PAGE_PROGRAM);  // ����ҳ���ָ��
//        flash_write_byte((address >> 16) & 0xFF);  // ���͵�ַ�ĸ��ֽ�
//        flash_write_byte((address >> 8) & 0xFF);   // ���͵�ַ�����ֽ�
//        flash_write_byte(address & 0xFF);          // ���͵�ַ�ĵ��ֽ�
//
////        memcpy(buffer, data, 96);
////        flash_write(buffer,96);
//
//        for (i = 0; i < page_remain; i++) {
//        	//usb_printf("data is %x\n",data[offset + i]);
//            flash_write_byte(data[offset + i]);  // д������
//        }
//
//        flash_WaitForWriteEnd();  // �ȴ�д�����
//        FLASH_CS_DISABLE();      // �ͷ�Ƭѡ�ź�
//
//        address += page_remain;  // ���µ�ַ
//        offset += page_remain;   // ��������ƫ��
//        size -= page_remain;     // ����ʣ�����ݴ�С
//    }
//    return FLASH_STATUS_OK;  // ���سɹ�״̬
			if(dev==DEV_NOR){
	 	 	flash_WriteEnable(1,dev);  // ʹ��д����
	        FLASH_CS_ENABLE();     // ѡ��W25Q64
	        flash_write_byte(FLASH_CMD_PAGE_PROGRAM);  // ����ҳ���ָ��
	        flash_write_byte((address >> 16) & 0xFF);  // ���͵�ַ�ĸ��ֽ�
	        flash_write_byte((address >> 8) & 0xFF);   // ���͵�ַ�����ֽ�
	        flash_write_byte(address & 0xFF);          // ���͵�ַ�ĵ��ֽ�


	        flash_write(data,size);
	        flash_WaitForWriteEnd(dev);  // �ȴ�д�����
	        FLASH_CS_DISABLE();      // �ͷ�Ƭѡ�ź�
			}
			if(dev==DEV_NAND){
	 	 	flash_WriteEnable(1,dev);  // ʹ��д����
	        NAND_CS_ENABLE();     // ѡ��W25Q64
	        flash_write_byte(FLASH_CMD_PAGE_PROGRAM);  // ����ҳ���ָ��
	        flash_write_byte((address >> 16) & 0xFF);  // ���͵�ַ�ĸ��ֽ�
	        flash_write_byte((address >> 8) & 0xFF);   // ���͵�ַ�����ֽ�
	        flash_write_byte(address & 0xFF);          // ���͵�ַ�ĵ��ֽ�


	        flash_write(data,size);
	        flash_WaitForWriteEnd(dev);  // �ȴ�д�����
	        NAND_CS_DISABLE();      // �ͷ�Ƭѡ�ź�
			}

    return FLASH_STATUS_OK;  // ���سɹ�״̬
}

void Write_Page(uint32_t WriteAddr,uint8_t* pBuffer,uint16_t NumByteToWrite,uint8_t dev)
{
	if(dev==DEV_NOR){
	uint16_t Word_remain;
	Word_remain=256-WriteAddr%256; 	//��λҳʣ�������

	if(NumByteToWrite <= Word_remain)
		Word_remain=NumByteToWrite;		//��λҳ��һ��д��
	while(1)
	{
		flash_PageProgram(WriteAddr,pBuffer,Word_remain,dev);
		if(NumByteToWrite==Word_remain)
		{
			break;	//�ж�д��� break
		}
	 	else //ûд�꣬��ҳ��
		{
			pBuffer += Word_remain;		//ֱ����Ƶ�ҳ��д����
			WriteAddr += Word_remain;
			NumByteToWrite -= Word_remain;	//��ȥ�Ѿ�д���˵�����
			if(NumByteToWrite>256)
				Word_remain=256; 		//һ�ο���д��256����
			else
				Word_remain=NumByteToWrite; 	//����256������
		}
	}
}
}
// ��ȡ����
void flash_ReadData(uint32_t address, uint8_t* data, uint16_t size ,uint8_t dev) {

	if(dev==DEV_NOR){
	FLASH_CS_ENABLE();
	uint8_t address_u8[4];
	address_u8[0] = FLASH_CMD_READ_DATA;
	address_u8[1] = (address & 0xFF0000) >> 16;
	address_u8[2] = (address & 0xFF00) >> 8;
	address_u8[3] = address & 0xFF;
	flash_write(address_u8,4);
//	flash_write_byte(FLASH_CMD_READ_DATA);
//	flash_write_byte((address & 0xFF0000) >> 16);
//	flash_write_byte((address & 0xFF00) >> 8);
//	flash_write_byte(address & 0xFF);

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
