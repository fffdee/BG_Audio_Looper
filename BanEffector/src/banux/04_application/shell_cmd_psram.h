/**
 * @file shell_cmd_psram.h
 * @brief PSRAM 内存管理 Shell 命令模块声明
 */
#ifndef __SHELL_CMD_PSRAM_H__
#define __SHELL_CMD_PSRAM_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 注册独立的 psram 命令模块
 * 直接输入 'psram' 即可显示内存占用报告
 * @return 0=成功
 */
int ShellCmdPsram_Register(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_PSRAM_H__ */