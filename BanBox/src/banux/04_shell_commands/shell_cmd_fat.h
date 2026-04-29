/**
 * @file shell_cmd_fat.h
 * @brief FAT32 文件系统 Shell 命令模块声明
 */
#ifndef __SHELL_CMD_FAT_H__
#define __SHELL_CMD_FAT_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 注册 FAT32 文件系统 Shell 命令模块
 * 需要在 Shell_RegisterAllModules() 中调用
 * @return 0=成功
 */
int ShellCmdFat_Register(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_FAT_H__ */