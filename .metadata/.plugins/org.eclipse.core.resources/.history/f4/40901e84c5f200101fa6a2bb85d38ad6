#include "battery_drv.h"
// Include hardware header files according to your platform to ensure GPIO/ADC definitions are valid
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
    // Provide ADC read logic
    GPIO_RegOneBitClear(GPIO_A_ANA_EN, GPIO_INDEX31);  // Disable GPIOA31 analog
    GPIO_RegOneBitSet(GPIO_A_ANA_EN, GPIO_INDEX31);     // Enable GPIOA31 analog function
    bat_adc_val = ADC_SingleModeDataGet(ADC_CHANNEL_GPIOA31);  // Get ADC channel value
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
 * @brief Battery API: get remaining battery SOC percentage
 * @return Battery SOC percentage (0~100)
 */
uint8_t battery_get_soc(void)
{
    // 1. Get raw ADC value from hardware, apply filtering if needed
    uint16_t new_adc_val = battery_adc_read();
    if(new_adc_val == 0) Shell_Printf("ADC Read Error!\r\n"); // Debug print
    // 2. Convert to voltage, apply filtering if needed
    float bat_volt = adc2volt(new_adc_val);
    // 3. Convert voltage to SOC
    return volt2soc(bat_volt);
}
/**
 * @brief Battery API: get real-time battery voltage (for display)
 * @return Real battery voltage (V)
 */
float battery_get_volt(void)
{
    // Get raw ADC value from hardware, apply filtering logic if needed
    uint16_t new_adc_val = battery_adc_read();
    // Directly convert ADC value to voltage (no averaging here)
    return adc2volt(new_adc_val);
}

