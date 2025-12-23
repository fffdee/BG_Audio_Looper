#include "stdio.h"
#include "gpio.h"
#include "timeout.h"
#include "debug.h"

// 假设的GPIO操作函数，您需要根据实际硬件平台进行修改
void SDA_SetOutput(void) {
    // 设置SDA为输出模式
	GPIO_RegOneBitClear(GPIO_A_IE, GPIOA30);
	GPIO_RegOneBitSet(GPIO_A_OE, GPIOA30);

}

void SDA_SetInput(void) {
    // 设置SDA为输入模式
	GPIO_RegOneBitClear(GPIO_A_OE, GPIOA30);
	GPIO_RegOneBitSet(GPIO_A_IE, GPIOA30);
}

void SCL_SetOutput(void) {
    // 设置SCL为输出模式
	GPIO_RegOneBitClear(GPIO_A_IE, GPIOA29);
	GPIO_RegOneBitSet(GPIO_A_OE, GPIOA29);
}

void SDA_High(void) {
    // 设置SDA为高电平
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA30);

}

void SDA_Low(void) {
    // 设置SDA为低电平
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA30);
}

void SCL_High(void) {
    // 设置SCL为高电平
	GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA29);
}

void SCL_Low(void) {
    // 设置SCL为低电平
	GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA29);
}

bool SDA_Read(void) {
    // 读取SDA线的状态
	uint8_t SDA_Val;
	SDA_Val = GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX30);
    return SDA_Val; // 返回值需要根据实际读取结果修改
}

void I2C_Delay(void) {
    // I2C延时函数，用于生成所需的时序
    DelayMs(1); // 延时5微秒，具体值可能需要根据实际硬件调整
}

// 启动I2C通信
void I2C_Start(void) {
	SDA_SetOutput();
	SCL_SetOutput();
    SDA_High();
    SCL_High();
    I2C_Delay();
    SDA_Low();
    I2C_Delay();
    SCL_Low();
}

// 停止I2C通信
void I2C_Stop(void) {
    SDA_Low();
    SCL_High();
    I2C_Delay();
    SDA_High();
    I2C_Delay();
}

// I2C发送一个字节，并接收应答位
bool I2C_WriteByte(uint8_t byte) {
	uint8_t i;
    for (i = 0; i < 8; i++) {
        if (byte & 0x80) {
            SDA_High();
        } else {
            SDA_Low();
        }
        I2C_Delay();
        SCL_High();
        I2C_Delay();
        SCL_Low();
        byte <<= 1;
    }

    // 释放SDA线，准备接收应答位
    SDA_SetInput();
    I2C_Delay();
    SCL_High();
    bool ack = !SDA_Read(); // 如果设备应答，则SDA会被拉低
    I2C_Delay();
    SCL_Low();
    SDA_SetOutput(); // 恢复SDA为输出模式

    return ack; // 返回应答状态
}

// I2C读取一个字节，并发送应答位
uint8_t I2C_ReadByte(bool ack) {
    uint8_t i, byte = 0;
    SDA_SetInput();

    for (i = 0; i < 8; i++) {
        byte <<= 1;
        SCL_High();
        I2C_Delay();
        if (SDA_Read()) {
            byte |= 0x01;
        }
        SCL_Low();
        I2C_Delay();
    }

    // 发送应答位
    SDA_SetOutput();
    if (ack) {
        SDA_Low();
    } else {
        SDA_High();
    }
    I2C_Delay();
    SCL_High();
    I2C_Delay();
    SCL_Low();

    return byte;
}


void I2C_NoAck(void) {
    // 设置SDA为输出
    SDA_SetOutput();
    // SDA线保持高电平
    SDA_High();
    // 时钟脉冲
    I2C_Delay();
    SCL_High();
    I2C_Delay();
    SCL_Low();
    I2C_Delay();
}

void I2C_WriteByteNoAck(uint8_t data) {

	I2C_Start();
    I2C_WriteByte(data);
    // 发送无应答信号
    DBG("Write bit\n");
    I2C_NoAck();
    I2C_Stop();
}

uint8_t I2C_ReadByteNoAck(void) {

	I2C_Start();
    uint8_t data = I2C_ReadByte(0);
    // 发送无应答信号
    DBG("Read bit\n");
    I2C_NoAck();
    I2C_Stop();
    return data;
}
