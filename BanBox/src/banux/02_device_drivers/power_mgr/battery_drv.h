#ifndef SRC_HARDWARE_BATTERY_BATTERY_DRV_H_
#define SRC_HARDWARE_BATTERY_BATTERY_DRV_H_

#include <stdint.h>

/************************ Hardware parameters, can be modified according to actual situation ************************/
/**
 * @brief ADC maximum value, 12-bit ADC is 4095
 */
#define ADC_MAX         4095

/**
 * @brief ADC reference voltage in V, STM32 default 3.3V, ESP32 may be 3.3V/1.1V
 */
#define ADC_REF_VOLT    3.3f

/**
 * @brief Voltage divider ratio, 1:1 voltage divider, measured voltage = ADC voltage * 2, for 2:1 divider is 3
 *        Calculation formula: voltage ratio = (upper resistor + lower resistor)/lower resistor
 */
#define VOLT_DIV_RATIO  2.0f

/**
 * @brief Full battery voltage in V, lithium battery max 4.2V, some 3.65V
 */
#define FULL_VOLT       4.2f

/**
 * @brief Battery discharge cutoff voltage in V, lithium battery 3.0V, some lower
 */
#define EMPTY_VOLT      3.0f

/**
 * @brief Filter buffer length, larger value better filtering, slower response, 3~8
 */
#define FILTER_BUF_LEN  5

/************************ Battery related API functions ************************/
/**
 * @brief Get battery level, state of charge SOC
 * @return Battery percentage 0~100, 0 means empty, 100 means full
 * @note This function internally automatically reads ADC, filters, converts voltage to percentage, can be called directly
 */
uint8_t battery_get_soc(void);

/**
 * @brief For debugging, get battery real-time voltage
 * @return Battery actual voltage in V
 */
float battery_get_volt(void);

#endif /* SRC_HARDWARE_BATTERY_BATTERY_DRV_H_ */
