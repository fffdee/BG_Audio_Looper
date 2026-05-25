#include "battery_drv.h"
#include "product_def.h"
#include "gpio.h"
#include "adc.h"

// ADC filter buffer (define length in header file)
static uint16_t adc_buf[FILTER_BUF_LEN] = {0};
static uint8_t adc_buf_idx = 0;

/**
 * @brief Read battery ADC value from hardware
 * @return Raw ADC value (0~4095)
 * @note Provides ADC read logic
 */
static uint16_t battery_adc_read(void)
{
    uint16_t bat_adc_val = 0;
    GPIO_RegOneBitClear(HW_BATTERY_ADC_GPIO_PORT, HW_BATTERY_ADC_GPIO_PIN);
    GPIO_RegOneBitSet(HW_BATTERY_ADC_GPIO_PORT, HW_BATTERY_ADC_GPIO_PIN);
    bat_adc_val = ADC_SingleModeDataGet(HW_BATTERY_ADC_CHANNEL);
    return bat_adc_val;
}

/**
 * @brief Convert battery ADC value to voltage
 * @param adc_val: Raw ADC value
 * @return Battery voltage (V)
 */
static float adc2volt(uint16_t adc_val)
{
    float adc_volt = (float)adc_val / ADC_MAX * ADC_REF_VOLT;
    return adc_volt * VOLT_DIV_RATIO;
}

/**
 * @brief Convert battery voltage to SOC percentage (mapping)
 * @param volt: Battery voltage (V)
 * @return Battery SOC percentage (0~100)
 */
static uint8_t volt2soc(float volt)
{
    if (volt >= FULL_VOLT)          return 100;
    else if (volt >= 4.1f)          return 90;
    else if (volt >= 4.0f)          return 80;
    else if (volt >= 3.9f)          return 70;
    else if (volt >= 3.8f)          return 60;
    else if (volt >= 3.75f)         return 50;
    else if (volt >= 3.7f)          return 40;
    else if (volt >= 3.65f)         return 30;
    else if (volt >= 3.6f)          return 20;
    else if (volt >= 3.4f)          return 10;
    else if (volt >= EMPTY_VOLT)    return 5;   // Low battery warning
    else                            return 0;   // Voltage too low
}

/**
 * @brief Convert ADC value to millivolts (integer only, no FPU required)
 * @param adc_val Raw ADC value (0~4095)
 * @return Battery voltage in millivolts
 */
static uint16_t adc_to_mv(uint16_t adc_val)
{
    /* (adc_val * ADC_REF_VOLT_mV * VOLT_DIV_RATIO) / ADC_MAX
     * = adc_val * 3300 * 2 / 4095 = adc_val * 6600 / 4095
     * max: 4095 * 6600 / 4095 = 6600 -> fits uint16_t */
    return (uint16_t)(((uint32_t)adc_val * 6600u) / 4095u);
}

/**
 * @brief Battery API: get remaining battery SOC percentage (integer only)
 * @return Battery SOC percentage (0~100)
 */
uint8_t battery_get_soc(void)
{
    uint16_t adc_val = battery_adc_read();
    uint16_t mv;
    if (adc_val == 0u) {
        return 50u; /* ADC fault: return mid-value */
    }
    mv = adc_to_mv(adc_val);
    if (mv >= 4200u)      return 100u;
    else if (mv >= 4100u) return 90u;
    else if (mv >= 4000u) return 80u;
    else if (mv >= 3900u) return 70u;
    else if (mv >= 3800u) return 60u;
    else if (mv >= 3750u) return 50u;
    else if (mv >= 3700u) return 40u;
    else if (mv >= 3650u) return 30u;
    else if (mv >= 3600u) return 20u;
    else if (mv >= 3400u) return 10u;
    else if (mv >= 3000u) return 5u;
    else                  return 0u;
}

/**
 * @brief Battery API: get voltage in millivolts (integer only, no FPU)
 * @return Battery voltage in millivolts
 */
uint16_t battery_get_volt_mv(void)
{
    return adc_to_mv(battery_adc_read());
}

/**
 * @brief Battery API: get real-time battery voltage (for display)
 * @return Real battery voltage (V)
 * @note Uses soft-float; prefer battery_get_volt_mv() to avoid FPU
 */
float battery_get_volt(void)
{
    // Get raw ADC value from hardware, apply filtering logic if needed
    uint16_t new_adc_val = battery_adc_read();
    // Directly convert ADC value to voltage (no averaging here)
    return adc2volt(new_adc_val);
}

