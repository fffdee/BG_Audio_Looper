/**
 *****************************************************************************
 * @file     shell_io_cdc.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     16-December-2025
 * @brief    Shell CDC IO适配器
 *****************************************************************************
 * @attention
 *
 * 将USB CDC接口适配为Shell IO接口
 * Bootloader中不依赖bg_shell.h，仅提供CDC IO接口指针。
 *
 *****************************************************************************
 */

#ifndef __SHELL_IO_CDC_H__
#define __SHELL_IO_CDC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bg_shell.h"

/**
 * @brief  获取CDC IO接口
 * @return ShellIO_t指针
 */
const ShellIO_t* ShellIO_CDC_Get(void);

/**
 * @brief  初始化CDC Shell（快捷函数）
 */
void ShellIO_CDC_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_IO_CDC_H__ */
