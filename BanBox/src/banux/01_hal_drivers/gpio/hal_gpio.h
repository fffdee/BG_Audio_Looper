/**
 *****************************************************************************
 * @file     hal_gpio.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    GPIO HAL适配层 - 封装SDK底层GPIO驱动
 *****************************************************************************
 * @attention
 *
 * 本文件是SDK GPIO驱动的包装层，提供统一的HAL接口。
 * 实际驱动实现在 MVsB1_Base_SDK/driver/driver/inc/gpio.h
 *
 *****************************************************************************
 */

#ifndef __HAL_GPIO_H__
#define __HAL_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

/* 直接引用SDK的GPIO驱动头文件 */
#include "gpio.h"

/*******************************************************************************
 * GPIO端口定义
 ******************************************************************************/
#define HAL_GPIO_A      GPIO_A_START
#define HAL_GPIO_B      GPIO_B_START

/*******************************************************************************
 * GPIO引脚索引 (来自SDK gpio.h)
 ******************************************************************************/
#define HAL_PIN_0       GPIO_INDEX0
#define HAL_PIN_1       GPIO_INDEX1
#define HAL_PIN_2       GPIO_INDEX2
#define HAL_PIN_3       GPIO_INDEX3
#define HAL_PIN_4       GPIO_INDEX4
#define HAL_PIN_5       GPIO_INDEX5
#define HAL_PIN_6       GPIO_INDEX6
#define HAL_PIN_7       GPIO_INDEX7
#define HAL_PIN_8       GPIO_INDEX8
#define HAL_PIN_9       GPIO_INDEX9
#define HAL_PIN_10      GPIO_INDEX10
#define HAL_PIN_11      GPIO_INDEX11
#define HAL_PIN_12      GPIO_INDEX12
#define HAL_PIN_13      GPIO_INDEX13
#define HAL_PIN_14      GPIO_INDEX14
#define HAL_PIN_15      GPIO_INDEX15
#define HAL_PIN_16      GPIO_INDEX16
#define HAL_PIN_17      GPIO_INDEX17
#define HAL_PIN_18      GPIO_INDEX18
#define HAL_PIN_19      GPIO_INDEX19
#define HAL_PIN_20      GPIO_INDEX20
#define HAL_PIN_21      GPIO_INDEX21
#define HAL_PIN_22      GPIO_INDEX22
#define HAL_PIN_23      GPIO_INDEX23
#define HAL_PIN_24      GPIO_INDEX24
#define HAL_PIN_25      GPIO_INDEX25
#define HAL_PIN_26      GPIO_INDEX26
#define HAL_PIN_27      GPIO_INDEX27
#define HAL_PIN_28      GPIO_INDEX28
#define HAL_PIN_29      GPIO_INDEX29
#define HAL_PIN_30      GPIO_INDEX30
#define HAL_PIN_31      GPIO_INDEX31

/*******************************************************************************
 * HAL GPIO接口
 ******************************************************************************/

/**
 * @brief  设置GPIO为输出模式
 * @param  port: GPIO端口 (GPIO_A_START 或 GPIO_B_START)
 * @param  pin: 引脚掩码 (GPIO_INDEXx)
 */
static inline void HAL_GPIO_SetOutput(uint32_t port, uint32_t pin)
{
    GPIO_RegOneBitSet(port + 1, pin);  /* OE寄存器 */
}

/**
 * @brief  设置GPIO为输入模式
 * @param  port: GPIO端口
 * @param  pin: 引脚掩码
 */
static inline void HAL_GPIO_SetInput(uint32_t port, uint32_t pin)
{
    GPIO_RegOneBitClear(port + 1, pin);  /* OE寄存器 */
    GPIO_RegOneBitSet(port + 2, pin);    /* IE寄存器 */
}

/**
 * @brief  输出高电平
 * @param  port: GPIO端口
 * @param  pin: 引脚掩码
 */
static inline void HAL_GPIO_SetHigh(uint32_t port, uint32_t pin)
{
    GPIO_RegOneBitSet(port, pin);
}

/**
 * @brief  输出低电平
 * @param  port: GPIO端口
 * @param  pin: 引脚掩码
 */
static inline void HAL_GPIO_SetLow(uint32_t port, uint32_t pin)
{
    GPIO_RegOneBitClear(port, pin);
}

/**
 * @brief  翻转GPIO电平
 * @param  port: GPIO端口
 * @param  pin: 引脚掩码
 */
static inline void HAL_GPIO_Toggle(uint32_t port, uint32_t pin)
{
    if (GPIO_RegOneBitGet(port, pin)) {
        GPIO_RegOneBitClear(port, pin);
    } else {
        GPIO_RegOneBitSet(port, pin);
    }
}

/**
 * @brief  读取GPIO电平
 * @param  port: GPIO端口
 * @param  pin: 引脚掩码
 * @return 电平状态 (0或非0)
 */
static inline uint32_t HAL_GPIO_Read(uint32_t port, uint32_t pin)
{
    return GPIO_RegOneBitGet(port, pin);
}

/**
 * @brief  使能GPIO上拉
 * @param  port: GPIO端口
 * @param  pin: 引脚掩码
 */
static inline void HAL_GPIO_PullUpEnable(uint32_t port, uint32_t pin)
{
    GPIO_RegOneBitSet(port + 3, pin);  /* PU寄存器 */
}

/**
 * @brief  禁用GPIO上拉
 * @param  port: GPIO端口
 * @param  pin: 引脚掩码
 */
static inline void HAL_GPIO_PullUpDisable(uint32_t port, uint32_t pin)
{
    GPIO_RegOneBitClear(port + 3, pin);
}

/**
 * @brief  使能GPIO下拉
 * @param  port: GPIO端口
 * @param  pin: 引脚掩码
 */
static inline void HAL_GPIO_PullDownEnable(uint32_t port, uint32_t pin)
{
    GPIO_RegOneBitSet(port + 4, pin);  /* PD寄存器 */
}

/**
 * @brief  禁用GPIO下拉
 * @param  port: GPIO端口
 * @param  pin: 引脚掩码
 */
static inline void HAL_GPIO_PullDownDisable(uint32_t port, uint32_t pin)
{
    GPIO_RegOneBitClear(port + 4, pin);
}

#ifdef __cplusplus
}
#endif

#endif /* __HAL_GPIO_H__ */
