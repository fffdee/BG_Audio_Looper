/**
 *****************************************************************************
 * @file     shell_cmd_mode.h
 * @author   BG Audio Team
 * @version  V1.0.0
 * @date     06-February-2026
 * @brief    设备模式Shell命令接口 - 控制主副音箱模式切换
 *****************************************************************************
 */

#ifndef __SHELL_CMD_MODE_H__
#define __SHELL_CMD_MODE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * 设备工作模式定义
 ******************************************************************************/
typedef enum {
    DEVICE_MODE_MAIN = 0,        /* 主音箱模式（默认模式，完整功能） */
    DEVICE_MODE_SECONDARY,       /* 副音箱模式（仅混音，无效果和looper） */
    DEVICE_MODE_MAX
} DeviceMode_t;

/*******************************************************************************
 * 公共API函数
 ******************************************************************************/

/**
 * @brief 注册mode命令到Shell系统
 * @return 0: 成功, -1: 失败
 */
int ShellCmdMode_Register(void);

/**
 * @brief 执行mode命令
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 0: 成功, -1: 失败
 */
int ShellCmdMode_Execute(int argc, char *argv[]);

/**
 * @brief 获取当前设备模式
 * @return 当前设备模式
 */
DeviceMode_t DeviceMode_GetCurrent(void);

/**
 * @brief 设置设备模式（内部使用，不保存到flash）
 * @param mode 要设置的模式
 * @return 0: 成功, -1: 失败
 */
int DeviceMode_Set(DeviceMode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_MODE_H__ */
