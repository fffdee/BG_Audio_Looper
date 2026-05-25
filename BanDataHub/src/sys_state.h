/**
 * sys_state.h - 系统状态管理模块
 *
 * 三种状态：
 *   SYS_STATE_IDLE     —— 空闲态：无音频 I/O 且超过空闲超时，DAC 已静音（低功耗）
 *   SYS_STATE_NORMAL   —— 正常态：音频系统活跃（BT/USB/ADC/Looper 有数据 I/O）
 *   SYS_STATE_TRANSFER —— 数据传输态：WAV BLE 导出或 OTA 大数据传输中，
 *                          音频已强制静音，App 应禁止其他操作
 *
 * 状态变化时，若 BLE 已连接，自动通过 BLE_CMD_SYSTEM/BLE_SYSTEM_SUB_STATE 上报 App。
 */

#ifndef __SYS_STATE_H__
#define __SYS_STATE_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SYS_STATE_IDLE     = 0,
    SYS_STATE_NORMAL   = 1,
    SYS_STATE_TRANSFER = 2,
} SysState_t;

/**
 * @brief 初始化系统状态模块，在系统启动后调用一次
 */
void SysState_Init(void);

/**
 * @brief 周期性更新（在 hardware_check() 中每 50ms 调用）
 *        自动探测低功耗状态和 WAV 传输状态，状态变化时通过 BLE 上报
 */
void SysState_Update(void);

/**
 * @brief 手动进入数据传输态（WAV/OTA 开始时调用）
 *        立即强制静音 DAC，并向 App 发送 TRANSFER 状态通知
 */
void SysState_EnterTransfer(void);

/**
 * @brief 手动退出数据传输态（WAV/OTA 结束时调用）
 *        恢复正常音频，并向 App 发送 NORMAL 状态通知
 */
void SysState_ExitTransfer(void);

/**
 * @brief 获取当前系统状态
 */
SysState_t SysState_Get(void);

#endif /* __SYS_STATE_H__ */
