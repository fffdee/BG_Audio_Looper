/**
 * @file shell_cmd_wav.h
 * @brief Shell 命令 - WAV 文件导出和管理
 */

#ifndef __SHELL_CMD_WAV_H__
#define __SHELL_CMD_WAV_H__

#include "product_def.h"

#if FAT32_EN && HW_DRV_FLASH_NAND_EN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 WAV 命令到 Shell
 */
void ShellCmdWav_Register(void);

#ifdef __cplusplus
}
#endif

#endif /* FAT32_EN && HW_DRV_FLASH_NAND_EN */

#endif /* __SHELL_CMD_WAV_H__ */
