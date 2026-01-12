/**
 *****************************************************************************
 * @file     shell_cmd_audio_vfs.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     06-January-2026
 * @brief    /audio 目录Shell命令头文件
 *****************************************************************************
 */

#ifndef __SHELL_CMD_AUDIO_VFS_H__
#define __SHELL_CMD_AUDIO_VFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bg_shell.h"

/*******************************************************************************
 * API函数
 ******************************************************************************/

/**
 * @brief 注册audio VFS Shell命令
 */
void ShellCmdAudioVfs_Register(void);

/**
 * @brief audio命令执行入口
 * @param argc 参数个数
 * @param argv 参数列表
 * @return 0成功，其他失败
 */
int ShellCmdAudioVfs_Execute(int argc, char *argv[]);

/**
 * @brief 检查并自动挂载默认效果图
 * @note  此函数可在音频系统初始化后被调用，会自动挂载graph0
 */
void ShellCmdAudioVfs_CheckAutoMount(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_AUDIO_VFS_H__ */
