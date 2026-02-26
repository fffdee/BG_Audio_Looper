/**
 *****************************************************************************
 * @file     hal_drivers.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    HAL驱动层统一头文件
 *****************************************************************************
 * @attention
 *
 * 本文件汇总所有HAL驱动适配层头文件。
 * 
 * HAL层定位：
 *   - 不重复实现SDK已有的驱动
 *   - 提供统一的HAL接口封装
 *   - 屏蔽SDK底层实现细节
 *   - 方便上层驱动调用
 *
 * 目录结构：
 *   01_hal_drivers/
 *   ├── hal_drivers.h      (本文件 - 统一头文件)
 *   ├── spi/
 *   │   └── hal_spi.h      (SPI HAL接口)
 *   ├── gpio/
 *   │   └── hal_gpio.h     (GPIO HAL接口)
 *   └── adc/
 *       └── hal_adc.h      (ADC HAL接口)
 *
 * SDK原始驱动位置：
 *   MVsB1_Base_SDK/driver/driver/inc/       (寄存器级驱动头文件)
 *   MVsB1_Base_SDK/driver/driver/libDriver.a (编译好的驱动库)
 *   MVsB1_Base_SDK/driver/driver_api/       (驱动接口层)
 *
 *****************************************************************************
 */

#ifndef __HAL_DRIVERS_H__
#define __HAL_DRIVERS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * 包含所有HAL驱动头文件
 ******************************************************************************/

/* SPI HAL */
#include "spi/hal_spi.h"

/* GPIO HAL */
#include "gpio/hal_gpio.h"

/* ADC HAL */
#include "adc/hal_adc.h"

/*******************************************************************************
 * HAL层说明
 ******************************************************************************/
/*
 * 为什么HAL层是包装层而不是重新实现？
 *
 * 1. SDK已提供完整的底层驱动实现（libDriver.a）
 * 2. 重新实现会导致代码冗余和维护困难
 * 3. 包装层提供统一接口，便于移植到其他平台
 * 4. 如需更换芯片平台，只需修改HAL层适配
 *
 * 使用示例：
 *
 *   // SPI初始化
 *   HAL_SPI_Init(HAL_SPI_MODE0, HAL_SPI_CLK_12M);
 *   HAL_SPI_PortSelect(HAL_SPI_PORT0);
 *
 *   // SPI发送数据
 *   uint8_t txBuf[4] = {0x01, 0x02, 0x03, 0x04};
 *   HAL_SPI_Send(txBuf, 4);
 *
 *   // GPIO控制
 *   HAL_GPIO_SetOutput(GPIO_A_START, GPIO_INDEX10);
 *   HAL_GPIO_SetHigh(GPIO_A_START, GPIO_INDEX10);
 *
 *   // ADC读取
 *   uint16_t adcVal = HAL_ADC_SingleRead(HAL_ADC_CH_A31);
 *   float voltage = HAL_ADC_ToVoltage(adcVal);
 */

#ifdef __cplusplus
}
#endif

#endif /* __HAL_DRIVERS_H__ */
