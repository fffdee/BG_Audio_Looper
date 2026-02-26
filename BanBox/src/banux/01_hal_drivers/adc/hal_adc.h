/**
 *****************************************************************************
 * @file     hal_adc.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    ADC HAL适配层 - 封装SDK底层ADC驱动
 *****************************************************************************
 * @attention
 *
 * 本文件是SDK ADC驱动的包装层，提供统一的HAL接口。
 * 用于电池电压检测等功能。
 *
 * SDK原始文件:
 *   - driver/driver/inc/adc.h
 *   - driver/driver_api/inc/adc_interface.h
 *
 *****************************************************************************
 */

#ifndef __HAL_ADC_H__
#define __HAL_ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

/* 直接引用SDK的ADC驱动头文件 */
#include "adc.h"
#include "adc_interface.h"

/*******************************************************************************
 * ADC通道定义 (来自SDK adc.h)
 ******************************************************************************/
#define HAL_ADC_CH_A0       ADC_CHANNEL_GPIOA0
#define HAL_ADC_CH_A1       ADC_CHANNEL_GPIOA1
#define HAL_ADC_CH_A2       ADC_CHANNEL_GPIOA2
#define HAL_ADC_CH_A3       ADC_CHANNEL_GPIOA3
#define HAL_ADC_CH_A4       ADC_CHANNEL_GPIOA4
#define HAL_ADC_CH_A5       ADC_CHANNEL_GPIOA5
#define HAL_ADC_CH_A6       ADC_CHANNEL_GPIOA6
#define HAL_ADC_CH_A7       ADC_CHANNEL_GPIOA7
#define HAL_ADC_CH_A20      ADC_CHANNEL_GPIOA20
#define HAL_ADC_CH_A21      ADC_CHANNEL_GPIOA21
#define HAL_ADC_CH_A22      ADC_CHANNEL_GPIOA22
#define HAL_ADC_CH_A23      ADC_CHANNEL_GPIOA23
#define HAL_ADC_CH_A24      ADC_CHANNEL_GPIOA24
#define HAL_ADC_CH_A25      ADC_CHANNEL_GPIOA25
#define HAL_ADC_CH_A26      ADC_CHANNEL_GPIOA26
#define HAL_ADC_CH_A27      ADC_CHANNEL_GPIOA27
#define HAL_ADC_CH_A28      ADC_CHANNEL_GPIOA28
#define HAL_ADC_CH_A29      ADC_CHANNEL_GPIOA29
#define HAL_ADC_CH_A30      ADC_CHANNEL_GPIOA30
#define HAL_ADC_CH_A31      ADC_CHANNEL_GPIOA31

/*******************************************************************************
 * ADC配置常量
 ******************************************************************************/
#define HAL_ADC_MAX_VALUE   4095    /* 12位ADC最大值 */
#define HAL_ADC_REF_VOLT    3.3f    /* 参考电压(V) */

/*******************************************************************************
 * HAL ADC接口
 ******************************************************************************/

/**
 * @brief  单次ADC采样
 * @param  channel: ADC通道 (HAL_ADC_CH_xxx)
 * @return ADC值 (0~4095)
 */
static inline uint16_t HAL_ADC_SingleRead(uint8_t channel)
{
    return ADC_SingleModeDataGet(channel);
}

/**
 * @brief  ADC值转换为电压
 * @param  adcValue: ADC原始值
 * @return 电压值(V)
 */
static inline float HAL_ADC_ToVoltage(uint16_t adcValue)
{
    return ((float)adcValue / HAL_ADC_MAX_VALUE) * HAL_ADC_REF_VOLT;
}

/**
 * @brief  获取ADC通道的电压值
 * @param  channel: ADC通道
 * @return 电压值(V)
 */
static inline float HAL_ADC_GetVoltage(uint8_t channel)
{
    uint16_t adcVal = HAL_ADC_SingleRead(channel);
    return HAL_ADC_ToVoltage(adcVal);
}

#ifdef __cplusplus
}
#endif

#endif /* __HAL_ADC_H__ */
