/**
 * @file shell_cmd_soundbank.h
 * @brief 音源管理 Shell 命令模块声明
 */
#ifndef __SHELL_CMD_SOUNDBANK_H__
#define __SHELL_CMD_SOUNDBANK_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 注册 soundbank Shell 命令模块
 * 需要在 Shell_RegisterAllModules() 中调用
 * @return 0=成功
 */
int ShellCmdSoundbank_Register(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_SOUNDBANK_H__ */
