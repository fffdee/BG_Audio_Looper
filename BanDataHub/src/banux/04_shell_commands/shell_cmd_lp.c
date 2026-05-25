/**
 * shell_cmd_lp.c - 低功耗模式控制 Shell 命令
 *
 * 命令格式:
 *   lp          - 显示当前自动低功耗启用状态
 *   lp 1 / on   - 启用自动低功耗（默认）
 *   lp 0 / off  - 禁用自动低功耗
 *
 * 变更会立即生效并持久化到 Flash。
 * 变更后通过 BLE 通知 App 当前状态（BLE_CMD_SYSTEM / BLE_SYSTEM_SUB_LP_STATE）。
 */

#include "bg_shell.h"
#include "bg_low_power.h"
#include "sys_param.h"
#include "ble_protocol.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>

extern uint8_t BleConnectFlag;

static void lp_notify_app(void)
{
    uint8_t pl[2];
    if (!BleConnectFlag) return;
    pl[0] = BLE_SYSTEM_SUB_LP_STATE;
    pl[1] = LowPower_GetEnabled();
    BleProto_SendOnce(BLE_CMD_SYSTEM, pl, 2);
    pl[0] = BLE_SYSTEM_SUB_LP_TIMEOUT;
    pl[1] = LowPower_GetTimeoutMin();
    BleProto_SendOnce(BLE_CMD_SYSTEM, pl, 2);
}

static int cmd_lp_main(int argc, char *argv[])
{
    uint8_t new_val;

    if (argc == 0) {
        Shell_Printf("Auto-LP: %s, timeout: %d min\r\n",
                     LowPower_GetEnabled() ? "enabled (1)" : "disabled (0)",
                     LowPower_GetTimeoutMin());
        return 0;
    }

    if (strcmp(argv[0], "on") == 0 || strcmp(argv[0], "1") == 0) {
        new_val = 1;
    } else if (strcmp(argv[0], "off") == 0 || strcmp(argv[0], "0") == 0) {
        new_val = 0;
    } else {
        Shell_Printf("Usage: lp [0|1|on|off]\r\n");
        return -1;
    }

    LowPower_SetEnabled(new_val);
    SysParam_Get()->system.lp_enable = new_val;
    SysParam_Save();
    lp_notify_app();

    Shell_Printf("Auto-LP %s, saved to flash\r\n", new_val ? "enabled" : "disabled");
    return 0;
}

static int cmd_lp_timeout(int argc, char *argv[])
{
    int minutes;
    if (argc < 1) {
        Shell_Printf("LP timeout: %d min\r\n", LowPower_GetTimeoutMin());
        return 0;
    }
    minutes = atoi(argv[0]);
    if (minutes < 1 || minutes > 60) {
        Shell_Printf("Error: timeout must be 1-60 minutes\r\n");
        return -1;
    }
    LowPower_SetTimeoutMin((uint8_t)minutes);
    SysParam_Get()->system.lp_timeout_min = (uint8_t)minutes;
    SysParam_Save();
    if (BleConnectFlag) {
        uint8_t pl[2];
        pl[0] = BLE_SYSTEM_SUB_LP_TIMEOUT;
        pl[1] = (uint8_t)minutes;
        BleProto_SendOnce(BLE_CMD_SYSTEM, pl, 2);
    }
    Shell_Printf("LP timeout set to %d min, saved to flash\r\n", minutes);
    return 0;
}

static const ShellOpt_t g_LpOpts[] = {
    { "",  NULL,      "[0|1|on|off]",  "Low-power auto-detect control",           cmd_lp_main    },
    { "t", "timeout", "<min>",         "Set idle timeout in minutes (1-60)",      cmd_lp_timeout },
    OPT_END()
};

static const ShellModule_t g_LpModule = {
    "lp",
    "Auto low-power mode enable/disable",
    MOD_CAT_SYSTEM,
    g_LpOpts,
    1
};

void ShellCmdLp_Register(void)
{
    Shell_RegisterModule(&g_LpModule);
}
