/**
 *****************************************************************************
 * @file     shell_fs_commands.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    Shell文件系统命令接口
 *****************************************************************************
 * @attention
 *
 * 本模块提供类Linux文件系统操作命令：
 * - pwd:  显示当前目录路径
 * - cd:   切换目录
 * - ls:   列出目录内容
 * - cat:  读取参数值
 * - echo: 写入参数值
 *
 * 使用示例：
 *   pwd                    -> /
 *   cd driver/spi/st7735   -> (切换到st7735目录)
 *   pwd                    -> /driver/spi/st7735
 *   ls                     -> name  width  height
 *   cat width              -> 160
 *   echo 128 > width       -> (设置width为128)
 *   cat width              -> 128
 *   cd ..                  -> (返回上级目录)
 *   pwd                    -> /driver/spi
 *
 *****************************************************************************
 */

#ifndef __SHELL_FS_COMMANDS_H__
#define __SHELL_FS_COMMANDS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

/**
 * @brief  注册文件系统相关的Shell命令
 * @note   在Shell系统初始化后调用
 */
void ShellFs_RegisterCommands(void);

/**
 * @brief  获取当前路径（供Shell提示符使用）
 * @param  buf: 输出缓冲区
 * @param  maxLen: 缓冲区大小
 */
void ShellFs_GetPromptPath(char *buf, uint16_t maxLen);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_FS_COMMANDS_H__ */
