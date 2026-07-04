/**
 * @file    app_sys_handler.c
 * @brief   应用层系统事件处理 — 订阅系统状态事件，驱动 LED / UI / BLE 上报
 * @author  BanGO
 *
 * 本模块将原来 main.c 中 hardware_check() 里的轮询逻辑
 * 迁移为事件驱动，通过 BG_EVT_SUB 订阅系统状态变化：
 *
 *   - LED 指示灯：订阅电量/USB/BT 事件，按状态切换亮灭闪烁
 *   - BT 状态显示：订阅 EVT_SYS_SUB_STATE 更新 UI
 *   - BLE 电量上报：订阅电量变化事件，定时上报
 *   - 开关机 LED：订阅 EVT_SYS_POWER_ON/OFF
 */

#include "bg_event.h"
#include "sys_state.h"
#include "debug.h"
#include "gpio.h"
#include "product_def.h"

/* 外部接口 */
#include "battery_calib.h"
#include "otg_detect.h"
#include "bt_manager.h"

#ifdef UI_EN
#include "comp_statusbar.h"
#endif

#include "ble_protocol.h"

extern uint8_t BleConnectFlag;

/* ====================== LED 指示灯管理 ====================== */

/* LED 闪烁状态 (50ms tick) */
static uint16_t s_led_blink_count = 0;
static uint8_t  s_led_last_state  = 0;

/* 当前 LED 模式: 0=灭, 1=常亮, 2=闪烁 */
static uint8_t  s_led_mode = 1;

/**
 * @brief 根据 LED 模式执行硬件控制 (每 50ms 由 led_tick 调用)
 */
static void led_hardware_update(void)
{
    if (s_led_mode == 2) {
        /* 闪烁: 500ms 亮 / 500ms 灭 */
        s_led_blink_count++;
        if (s_led_blink_count >= 10) {
            s_led_blink_count = 0;
            s_led_last_state = !s_led_last_state;
        }
        if (s_led_last_state) {
            GPIO_RegOneBitSet(GPIO_A_OUT, HW_LED_GPIO_PIN);
        } else {
            GPIO_RegOneBitClear(GPIO_A_OUT, HW_LED_GPIO_PIN);
        }
    } else {
        s_led_blink_count = 0;
        s_led_last_state = s_led_mode;
        if (s_led_mode) {
            GPIO_RegOneBitSet(GPIO_A_OUT, HW_LED_GPIO_PIN);
        } else {
            GPIO_RegOneBitClear(GPIO_A_OUT, HW_LED_GPIO_PIN);
        }
    }
}

/**
 * @brief 根据电池和 USB 状态重新计算 LED 模式
 */
static void led_refresh_mode(void)
{
    uint8_t soc = BattCalib_GetSOC();
    bool usb_connected = OTG_PortDeviceIsLink();

    /* 充电完成: USB 连接且电量满 → 熄灭 */
    if (usb_connected && soc >= 100) {
        s_led_mode = 0;
    }
    /* 低电量闪烁 */
    else if (soc < 15) {
        s_led_mode = 2;
    }
    /* 正常: 常亮 */
    else {
        s_led_mode = 1;
    }
}

/**
 * @brief 50ms 定时 tick — LED 闪烁时序驱动
 * @note  仍需从 hardware_check() 调用 (闪烁需要定时器)
 */
void AppSys_LedTick(void)
{
    led_hardware_update();
}

/* ---- 订阅: 子系统状态变化 → 刷新 LED 模式 ---- */
static void on_sub_state_for_led(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventSysSubState_t *evt = (const BG_EventSysSubState_t *)data;
    (void)topic; (void)size;

    /* 仅在 USB / 充电 / 低电量相关位变化时刷新 */
    if (evt->changed_bits & (SYS_SUB_USB_CONNECTED | SYS_SUB_BATT_CHARGING | SYS_SUB_BATT_LOW)) {
        led_refresh_mode();
    }
}
BG_EVT_SUB(EVT_SYS_SUB_STATE, on_sub_state_for_led);

/* ---- 订阅: 开机 → LED 常亮 ---- */
static void on_power_on(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic; (void)data; (void)size;
    s_led_mode = 1;
    GPIO_RegOneBitSet(GPIO_A_OUT, HW_LED_GPIO_PIN);
}
BG_EVT_SUB(EVT_SYS_POWER_ON, on_power_on);

/* ---- 订阅: 关机 → LED 熄灭 ---- */
static void on_power_off(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic; (void)data; (void)size;
    s_led_mode = 0;
    GPIO_RegOneBitClear(GPIO_A_OUT, HW_LED_GPIO_PIN);
}
BG_EVT_SUB(EVT_SYS_POWER_OFF, on_power_off);

/* ====================== BT 状态 UI 更新 ====================== */

/**
 * @brief 订阅子系统状态 → 更新状态栏 BT 图标
 */
static void on_sub_state_for_ui(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventSysSubState_t *evt = (const BG_EventSysSubState_t *)data;
    (void)topic; (void)size;

#ifdef UI_EN
    /* 仅在 BT 相关位变化时更新 UI */
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

    /* USB 连接状态变化 → 更新状态栏 */
    if (evt->changed_bits & SYS_SUB_USB_CONNECTED) {
        UI_StatusBar_SetUSBConnected((evt->new_state & SYS_SUB_USB_CONNECTED) ? true : false);
    }
#endif
}
BG_EVT_SUB(EVT_SYS_SUB_STATE, on_sub_state_for_ui);

/* ====================== BLE 电量上报 ====================== */

/* 电量上报定时计数器 (50ms tick, 600 = 30s) */
static uint16_t s_battery_report_count = 0;

/**
 * @brief 50ms 定时 tick — BLE 电量上报
 * @note  仍需从 hardware_check() 调用 (需要定时器)
 */
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

/* ====================== 运行态变化日志 ====================== */

static void on_run_state(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventSysRunState_t *evt = (const BG_EventSysRunState_t *)data;
    (void)topic; (void)size;
    DBG("[AppSys] RunState: %d -> %d\n", evt->old_state, evt->new_state);
}
BG_EVT_SUB(EVT_SYS_RUN_STATE, on_run_state);
