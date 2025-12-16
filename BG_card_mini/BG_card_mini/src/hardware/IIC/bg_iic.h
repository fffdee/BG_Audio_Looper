#ifndef __bg_IIC_H_
#define __bg_IIC_H_

#include <stdint.h>
#include <stdbool.h>

void I2C_WriteByteNoAck(uint8_t data);

uint8_t I2C_ReadByteNoAck(void);

uint8_t I2C_ReadByte(bool ack);

bool I2C_WriteByte(uint8_t byte);

void I2C_Start(void);

void I2C_Stop(void);

#endif
