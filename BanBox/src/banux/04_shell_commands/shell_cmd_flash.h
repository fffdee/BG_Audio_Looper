/**
 *****************************************************************************
 * @file     shell_cmd_flash.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     03-April-2026
 * @brief    NAND Flash 测试 Shell 命令头文件
 *
 * 注册命令模块 "nandtest"，提供以下选项:
 *   nandtest -r [blocks]  - 启动后台测试任务 (BBM + 功能 + 速度)
 *   nandtest -s           - 停止后台测试任务
 *   nandtest -q           - 查询任务运行状态
 *   nandtest -b           - 仅扫描坏块 (阻塞执行)
 *   nandtest -p [blocks]  - 仅执行速度测试 (阻塞执行, 默认 4 块)
 *****************************************************************************
 */

#ifndef __SHELL_CMD_FLASH_H__
#define __SHELL_CMD_FLASH_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 NAND Flash 测试命令到 Shell
 */
void ShellCmdFlash_Register(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_FLASH_H__ */
