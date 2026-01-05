/**
 *****************************************************************************
 * @file     shell_fs.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    Shell文件系统 - 管理/bin目录下的系统命令
 *****************************************************************************
 * @attention
 *
 * 本模块负责在VFS中创建/bin目录，并提供系统命令的注册接口
 *
 * 目录结构：
 *   /bin
 *       ├── sys
 *       │   ├── info
 *       │   ├── mem
 *       │   ├── tasks
 *       │   └── uptime
 *       └── (future commands...)
 *
 *****************************************************************************
 */

#ifndef __SHELL_FS_H__
#define __SHELL_FS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "vfs.h"

/*******************************************************************************
 * API函数
 ******************************************************************************/

/**
 * @brief  初始化Shell文件系统（创建/bin目录）
 * @return VFS_OK成功，其他失败
 * @note   必须先调用Vfs_Init()
 */
VfsError_t ShellFs_Init(void);

/**
 * @brief  获取/bin目录节点
 * @return /bin目录节点指针
 */
VfsNode_t* ShellFs_GetBinDir(void);

/**
 * @brief  在/bin下注册一个命令节点
 * @param  name: 命令名称（如"sys"、"lcd"等）
 * @return 创建的命令节点，NULL表示失败
 */
VfsNode_t* ShellFs_RegisterCommand(const char *name);

/**
 * @brief  注册所有系统命令到/bin（每个命令为单一节点）
 * @note   调用此函数注册sys、lcd、debug等所有内置命令
 */
void ShellFs_RegisterAllCommands(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_FS_H__ */
