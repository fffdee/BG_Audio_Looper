/**
 * @file    app_sys_handler.c
 * @brief   应用层系统事件处理 — 订阅系统状态事件，驱动 UI / BLE 上报
 */

#include "bg_event.h"
#include "sys_state.h"
#include "debug.h"
#include "product_def.h"
#include "banux/banux_config.h"

#include "battery_calib.h"
#include "bt_manager.h"

#ifdef UI_EN
#include "comp_statusbar.h"
#endif

#include "ble_protocol.h"
#include "sys_led.h"

extern uint8_t BleConnectFlag;

void AppSys_LedTick(void)
{
#if SYS_LED_EN
    SysLed_Tick50ms();
#endif
}

/* ====================== BT 状态 UI 更新 ====================== */

static void on_sub_state_for_ui(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventSysSubState_t *evt = (const BG_EventSysSubState_t *)data;
    (void)topic; (void)size;

#ifdef UI_EN
    if (evt->changed_bits & (SYS_SUB_BT_CONNECTED | SYS_SUB_BT_STREAMING)) {
        uint16_t sub = evt->new_state;
        UI_BTStatus_t bt_status;

        if (sub & SYS_SUB_BT_STREAMING) {
            bt_status = UI_BT_PLAYING;
        } else if (sub & SYS_SUB_BT_CONNECTED) {
            bt_status = UI_BT_CONNECTED;
        } else {
            bt_status = UI_BT_DISCONNECTED;
        }
        UI_StatusBar_SetBTStatus(bt_status);
    }

    if (evt->changed_bits & SYS_SUB_USB_CONNECTED) {
        UI_StatusBar_SetUSBConnected((evt->new_state & SYS_SUB_USB_CONNECTED) ? true : false);
    }
#endif
}
BG_EVT_SUB(EVT_SYS_SUB_STATE, on_sub_state_for_ui);

/* ====================== BLE 电量上报 ====================== */

static uint16_t s_battery_report_count = 0;

void AppSys_BatteryTick(void)
{
    s_battery_report_count++;
    if (s_battery_report_count >= 600) {
        s_battery_report_count = 0;
        if (BleConnectFlag) {
            uint8_t payload[2];
            payload[0] = BLE_SYSTEM_SUB_BATTERY;
            payload[1] = BattCalib_GetSOC();
            BleProto_SendOnce(BLE_CMD_SYSTEM, payload, 2);
        }
    }
}

static void on_run_state(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventSysRunState_t *evt = (const BG_EventSysRunState_t *)data;
    (void)topic; (void)size;
    DBG("[AppSys] RunState: %d -> %d\n", evt->old_state, evt->new_state);
}
BG_EVT_SUB(EVT_SYS_RUN_STATE, on_run_state);
