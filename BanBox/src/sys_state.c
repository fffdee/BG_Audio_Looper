/**
 * sys_state.c - 系统状态管理模块实现
 *
 * 状态转移规则：
 *   TRANSFER（手动）优先级最高；
 *   IDLE 由 LowPower 模块判断（LowPower_IsLowPower()）；
 *   其余为 NORMAL。
 */

#include "sys_state.h"
#include "ble_protocol.h"
#include "bg_low_power.h"
#include "looper_wav_ble_export.h"
#include "debug.h"

/* BLE 连接标志（定义在 ble_app_callback.c） */
extern uint8_t BleConnectFlag;

/* ====================== 私有状态 ====================== */
static SysState_t g_sys_state         = SYS_STATE_NORMAL;
static uint8_t    g_transfer_override = 0; /* 1 = 手动传输态，不自动退出 */

/* ====================== 内部辅助 ====================== */

static void notify_app(SysState_t state)
{
    uint8_t payload[2];
    if (!BleConnectFlag) {
        return;
    }
    payload[0] = BLE_SYSTEM_SUB_STATE;
    payload[1] = (uint8_t)state;
    BleProto_SendOnce(BLE_CMD_SYSTEM, payload, 2);
    DBG("[SysState] Notified App: state=%d\n", (int)state);
}

static void set_state(SysState_t new_state)
{
    if (new_state == g_sys_state) {
        return;
    }
    DBG("[SysState] %d -> %d\n", (int)g_sys_state, (int)new_state);
    g_sys_state = new_state;
    notify_app(new_state);
}

/* ====================== 公开 API ====================== */

void SysState_Init(void)
{
    g_sys_state         = SYS_STATE_NORMAL;
    g_transfer_override = 0;
    DBG("[SysState] Initialized\n");
}

void SysState_Update(void)
{
    SysState_t new_state;

    /* 手动传输态：优先级最高，Update 不改变它 */
    if (g_transfer_override) {
        set_state(SYS_STATE_TRANSFER);
        return;
    }

    /* WAV BLE 导出自动检测 */
    if (LooperWavBle_IsBusy()) {
        if (g_sys_state != SYS_STATE_TRANSFER) {
            LowPower_ForceEnter();   /* 静音 DAC */
        }
        set_state(SYS_STATE_TRANSFER);
        return;
    }

    /* WAV 传输刚结束：恢复音频 */
    if (g_sys_state == SYS_STATE_TRANSFER && !g_transfer_override) {
        LowPower_ForceClear();
    }

    /* 低功耗模块判断 IDLE / NORMAL */
    if (LowPower_IsLowPower()) {
        new_state = SYS_STATE_IDLE;
    } else {
        new_state = SYS_STATE_NORMAL;
    }
    set_state(new_state);
}

void SysState_EnterTransfer(void)
{
    g_transfer_override = 1;
    LowPower_ForceEnter();
    set_state(SYS_STATE_TRANSFER);
}

void SysState_ExitTransfer(void)
{
    g_transfer_override = 0;
    LowPower_ForceClear();
    /* 下次 Update 会根据实际活动掩码决定 IDLE/NORMAL */
    set_state(SYS_STATE_NORMAL);
}

SysState_t SysState_Get(void)
{
    return g_sys_state;
}
