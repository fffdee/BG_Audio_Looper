#ifndef _PCF_8574_H__
#define _PCF_8574_H__

#include "bg_iic.h"
#define IO_EXT 0x40

uint8_t PCF_GPIO_Read_All(uint8_t addr);

uint8_t PCF_GPIO_Read(uint8_t addr,uint8_t val);

void PCF_GPIO_Write_All(uint8_t addr,uint8_t val);

void PCF_GPIO_Write(uint8_t addr,uint8_t IO_Num,uint8_t val);

#endif
