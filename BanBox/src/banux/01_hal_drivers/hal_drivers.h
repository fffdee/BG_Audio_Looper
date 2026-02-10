/**
 *****************************************************************************
 * @file     hal_drivers.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    HAL driver layer unified header file
 *****************************************************************************
 * @attention
 *
 * This file summarizes all HAL driver adaptation layer header files.
 * 
 * HAL layer positioning:
 *   - Do not reimplement existing SDK drivers
 *   - Provide unified HAL interface encapsulation
 *   - Shield SDK underlying implementation details
 *   - Convenient for upper layer driver calls
 *
 * Directory structure:
 *   01_hal_drivers/
 *   ├── hal_drivers.h      (This file - unified header file)
 *   ├── spi/
 *   │   └── hal_spi.h      (SPI HAL interface)
 *   ├── gpio/
 *   │   └── hal_gpio.h     (GPIO HAL interface)
 *   └── adc/
 *       └── hal_adc.h      (ADC HAL interface)
 *
 * SDK original driver location:
 *   MVsB1_Base_SDK/driver/driver/inc/       (Register-level driver header files)
 *   MVsB1_Base_SDK/driver/driver/libDriver.a (Compiled driver library)
 *   MVsB1_Base_SDK/driver/driver_api/       (Driver interface layer)
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
 * HAL layer description
 ******************************************************************************/
/*
 * Why is the HAL layer a wrapper layer rather than reimplementation?
 *
 * 1. SDK already provides complete underlying driver implementation (libDriver.a)
 * 2. Reimplementation will lead to code redundancy and maintenance difficulties
 * 3. Wrapper layer provides unified interface, easy to port to other platforms
 * 4. If need to change chip platform, only need to modify HAL layer adaptation
 *
 * Usage example:
 *
 *   // SPI initialization
 *   HAL_SPI_Init(HAL_SPI_MODE0, HAL_SPI_CLK_12M);
 *   HAL_SPI_PortSelect(HAL_SPI_PORT0);
 *
 *   // SPI send data
 *   uint8_t txBuf[4] = {0x01, 0x02, 0x03, 0x04};
 *   HAL_SPI_Send(txBuf, 4);
 *
 *   // GPIO control
 *   HAL_GPIO_SetOutput(GPIO_A_START, GPIO_INDEX10);
 *   HAL_GPIO_SetHigh(GPIO_A_START, GPIO_INDEX10);
 *
 *   // ADC reading
 *   uint16_t adcVal = HAL_ADC_SingleRead(HAL_ADC_CH_A31);
 *   float voltage = HAL_ADC_ToVoltage(adcVal);
 */

#ifdef __cplusplus
}
#endif

#endif /* __HAL_DRIVERS_H__ */
