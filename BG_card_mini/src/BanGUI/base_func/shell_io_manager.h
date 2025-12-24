/**
 *****************************************************************************
 * @file     shell_io_manager.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     16-December-2025
 * @brief    Shell IO管理器 - 自动切换CDC/BLE接口并提供访问保护
 *****************************************************************************
 */

#ifndef __SHELL_IO_MANAGER_H__
#define __SHELL_IO_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bg_shell.h"

/*******************************************************************************
 * 配置定义
 ******************************************************************************/
#define SHELL_IO_TIMEOUT_MS     3000    /* IO接口超时时间（毫秒），超时后可切换 */
#define SHELL_IO_LOCK_TIMEOUT   5000    /* 锁定超时时间（毫秒），防止死锁 */

/*******************************************************************************
 * IO接口类型枚举
 ******************************************************************************/
typedef enum {
    SHELL_IO_NONE = 0,      /* 无活跃接口 */
    SHELL_IO_CDC,           /* USB CDC接口 */
    SHELL_IO_BLE            /* BLE SPP接口 */
} ShellIOType_t;

/*******************************************************************************
 * IO管理器状态枚举
 ******************************************************************************/
typedef enum {
    SHELL_IO_STATE_IDLE = 0,    /* 空闲，可接受任意接口数据 */
    SHELL_IO_STATE_ACTIVE,      /* 活跃，正在与某接口通信 */
    SHELL_IO_STATE_LOCKED       /* 锁定，禁止切换（正在处理命令） */
} ShellIOState_t;

/*******************************************************************************
 * IO管理器结构体
 ******************************************************************************/
typedef struct {
    ShellIOType_t   active_io;          /* 当前活跃的IO接口 */
    ShellIOState_t  state;              /* 管理器状态 */
    uint32_t        last_activity_tick; /* 最后活动时间 */
    uint32_t        lock_tick;          /* 锁定开始时间 */
    uint8_t         cdc_pending;        /* CDC有待处理数据 */
    uint8_t         ble_pending;        /* BLE有待处理数据 */
} ShellIOManager_t;

/*******************************************************************************
 * API函数声明
 ******************************************************************************/

/**
 * @brief  初始化IO管理器
 * @note   在Shell_Init之后调用
 */
void ShellIOManager_Init(void);

/**
 * @brief  IO管理器处理函数
 * @note   在主循环中调用，替代直接调用Shell_Process
 *         自动检测活跃接口并处理数据
 */
void ShellIOManager_Process(void);

/**
 * @brief  获取当前活跃的IO类型
 * @return 当前活跃的IO接口类型
 */
ShellIOType_t ShellIOManager_GetActiveIO(void);

/**
 * @brief  获取当前状态
 * @return 管理器状态
 */
ShellIOState_t ShellIOManager_GetState(void);

/**
 * @brief  尝试锁定IO接口（开始处理命令时调用）
 * @param  io_type 请求锁定的IO类型
 * @return 1=成功锁定, 0=锁定失败（另一接口正在使用）
 */
uint8_t ShellIOManager_TryLock(ShellIOType_t io_type);

/**
 * @brief  解锁IO接口（命令处理完成时调用）
 */
void ShellIOManager_Unlock(void);

/**
 * @brief  强制切换到指定IO接口
 * @param  io_type 目标IO类型
 * @return 1=成功, 0=失败（当前被锁定）
 */
uint8_t ShellIOManager_SwitchIO(ShellIOType_t io_type);

/**
 * @brief  更新活动时间戳（收到数据时调用）
 * @param  io_type 收到数据的IO类型
 */
void ShellIOManager_UpdateActivity(ShellIOType_t io_type);

/**
 * @brief  获取IO类型名称字符串
 * @param  io_type IO类型
 * @return 名称字符串
 */
const char* ShellIOManager_GetIOName(ShellIOType_t io_type);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_IO_MANAGER_H__ */
