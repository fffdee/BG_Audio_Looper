/**
 * @file shell_cmd_speedtest.h
 * @brief PSRAM & SD Card 读写速度测试 Shell 命令接口
 */

#ifndef __SHELL_CMD_SPEEDTEST_H__
#define __SHELL_CMD_SPEEDTEST_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册速度测试模块到 Shell
 * @return 0=成功, -1=失败
 */
int ShellCmdSpeedTest_Register(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_SPEEDTEST_H__ */
