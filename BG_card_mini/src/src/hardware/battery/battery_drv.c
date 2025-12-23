#include "battery_drv.h"
// 引入硬件相关头文件（根据你的平台补充，确保GPIO/ADC宏定义有效）
#include "gpio.h"
#include "adc.h"

// 滑动滤波缓存（长度由头文件宏定义配置）
static uint16_t adc_buf[FILTER_BUF_LEN] = {0};
static uint8_t adc_buf_idx = 0;

/**
 * @brief 【驱动内部】硬件级读取电池ADC采样值
 * @return ADC原始采样值（0~4095）
 * @note 整合你提供的ADC读取逻辑
 */
static uint16_t battery_adc_read(void)
{
    uint16_t bat_adc_val = 0;

    // 你提供的ADC核心读取逻辑
    GPIO_RegOneBitClear(GPIO_A_ANA_EN, GPIO_INDEX31);  // 配置GPIOA31模拟使能
    GPIO_RegOneBitSet(GPIO_A_ANA_EN, GPIO_INDEX31);     // 使能GPIOA31模拟功能
    bat_adc_val = ADC_SingleModeDataGet(ADC_CHANNEL_GPIOA31);  // 读取ADC通道值

    return bat_adc_val;
}

/**
 * @brief 【驱动内部】ADC采样值转电池电压
 * @param adc_val: ADC原始采样值
 * @return 电池电压(V)
 */
static float adc2volt(uint16_t adc_val)
{
    float adc_volt = (float)adc_val / ADC_MAX * ADC_REF_VOLT;
    return adc_volt * VOLT_DIV_RATIO;
}

/**
 * @brief 【驱动内部】电池电压转电量百分比（粗略映射）
 * @param volt: 电池电压(V)
 * @return 电量百分比(0~100)
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
    else if (volt >= EMPTY_VOLT)    return 5;   // 低电量提醒
    else                            return 0;   // 电压过低
}

/**
 * @brief 【对外API】获取电池剩余电量百分比（SOC）
 * @return 电量百分比（0~100）
 */
uint8_t battery_get_soc(void)
{
    // 1. 读取硬件ADC原始值（跳过滤波）
    uint16_t new_adc_val = battery_adc_read();
    if(new_adc_val == 0) Shell_Printf("ADC Read Error!\r\n"); // 调试打印

    // 2. 直接转换电压（不滤波）
    float bat_volt = adc2volt(new_adc_val);

    // 3. 电压转电量
    return volt2soc(bat_volt);
}
/**
 * @brief 【对外API】获取电池实时电压（便于调试）
 * @return 电池实际电压（V）
 */
float battery_get_volt(void)
{
    // 仅读取单次ADC原始值（跳过滤波逻辑）
    uint16_t new_adc_val = battery_adc_read();

    // 直接用单次ADC值转换为电压返回（无平均、无缓存）
    return adc2volt(new_adc_val);
}

