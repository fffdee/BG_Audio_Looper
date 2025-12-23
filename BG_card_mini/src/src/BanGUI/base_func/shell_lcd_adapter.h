/**
 *****************************************************************************
 * @file     shell_lcd_adapter.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     16-December-2025
 * @brief    Shell LCD Adapter - Connect Shell console with LCD driver
 *****************************************************************************
 * @attention
 *
 * This module provides LCD driver adaptation for Shell LCD console.
 * Decouples Shell module from specific LCD driver via abstract interface.
 * 
 * Usage:
 * 1. Call ShellLCD_Adapter_Init() during initialization
 * 2. Shell module will automatically use LCD for console display
 * 
 *****************************************************************************
 */

#ifndef __SHELL_LCD_ADAPTER_H__
#define __SHELL_LCD_ADAPTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"
#include <stdbool.h>

/**
 * @brief  Initialize Shell LCD adapter
 * @return TRUE on success
 * @note   Automatically registers LCD interface to Shell module
 */
bool ShellLCD_Adapter_Init(void);

/**
 * @brief  Enable Shell LCD console (forcefully takes over screen)
 * @param  enable: TRUE to enable, FALSE to disable
 */
void ShellLCD_Console_Enable(bool enable);

/**
 * @brief  Check if Shell LCD console is enabled
 * @return TRUE if enabled
 */
bool ShellLCD_Console_IsEnabled(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_LCD_ADAPTER_H__ */
