/**
 * @file    app_event_example.c
 * @brief   事件订阅使用示例 — 应用层如何使用事件系统
 * @author  BanGO
 *
 * 使用方法:
 *   1. 在文件任意位置定义回调函数
 *   2. 用 BG_EVT_SUB(话题, 回调) 声明订阅 (文件作用域)
 *   3. 系统启动时 BG_Event_Init() 自动完成注册, 无需手动调用
 *
 * 事件流:
 *   [GPIO扫描] → bg_ui.c 发布 EVT_BTN_CLICK → 事件总线 → 本文件 on_btn_click()
 *   [BLE协议栈] → ble_app_callback.c 发布 EVT_BLE_CONNECTED → 事件总线 → on_ble_connect()
 *   [A2DP回调] → bt_a2dp_app.c 发布 EVT_SYS_BT_CONNECT → 事件总线 → on_bt_state()
 *
 * 注意:
 *   本文件仅为演示, 如不需要可从编译中移除或添加 #if 0 包裹。
 */

#include "bg_event.h"
#include "debug.h"

/* ============================================
 * 1. 按钮事件回调
 * ============================================ */

static void on_btn_click(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventBtnData_t *btn = (const BG_EventBtnData_t *)data;
    (void)topic; (void)size;

    switch (btn->btn_id) {
    case 0: DBG("[Evt] UP clicked\n");    break;
    case 1: DBG("[Evt] DOWN clicked\n");  break;
    case 2: DBG("[Evt] ENTER clicked\n"); break;
    case 3: DBG("[Evt] BACK clicked\n");  break;
    default: break;
    }
}

static void on_btn_long_press(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventBtnData_t *btn = (const BG_EventBtnData_t *)data;
    (void)topic; (void)size;
    DBG("[Evt] Button %d long press (%d ms)\n", btn->btn_id, btn->duration_ms);
}

static void on_btn_double_click(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventBtnData_t *btn = (const BG_EventBtnData_t *)data;
    (void)topic; (void)size;
    DBG("[Evt] Button %d double click!\n", btn->btn_id);
}

/* 声明即注册 — 编译后自动进入订阅系统, 无需任何 Init 调用 */
BG_EVT_SUB(EVT_BTN_CLICK,        on_btn_click);
BG_EVT_SUB(EVT_BTN_LONG_PRESS,   on_btn_long_press);
BG_EVT_SUB(EVT_BTN_DOUBLE_CLICK, on_btn_double_click);

/* ============================================
 * 2. BLE 事件回调
 * ============================================ */

static void on_ble_connect(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic; (void)data; (void)size;
    DBG("[Evt] BLE connected\n");
}

static void on_ble_disconnect(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic; (void)data; (void)size;
    DBG("[Evt] BLE disconnected\n");
}

static void on_ble_data(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventBleRxData_t *rx = (const BG_EventBleRxData_t *)data;
    (void)topic; (void)size;
    DBG("[Evt] BLE data received: %d bytes\n", rx->len);
}

BG_EVT_SUB(EVT_BLE_CONNECTED,     on_ble_connect);
BG_EVT_SUB(EVT_BLE_DISCONNECTED,  on_ble_disconnect);
BG_EVT_SUB(EVT_BLE_DATA_RECEIVED, on_ble_data);

/* ============================================
 * 3. 蓝牙 A2DP / 系统事件回调
 * ============================================ */

static void on_bt_state(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)data; (void)size;
    switch ((uint16_t)topic) {
    case EVT_SYS_BT_CONNECT:    DBG("[Evt] BT A2DP connected\n");   break;
    case EVT_SYS_BT_DISCONNECT: DBG("[Evt] BT A2DP disconnected\n"); break;
    case EVT_SYS_BT_STREAMING:  DBG("[Evt] BT A2DP streaming\n");   break;
    case EVT_SYS_BT_SUSPENDED:  DBG("[Evt] BT A2DP suspended\n");   break;
    default: break;
    }
}

BG_EVT_SUB(EVT_SYS_BT_CONNECT,    on_bt_state);
BG_EVT_SUB(EVT_SYS_BT_DISCONNECT, on_bt_state);
BG_EVT_SUB(EVT_SYS_BT_STREAMING,  on_bt_state);
BG_EVT_SUB(EVT_SYS_BT_SUSPENDED,  on_bt_state);

/* ============================================
 * 4. 音频插拔事件回调（Guitar / MIC / Headphone）
 * USB 已移出事件系统，在 InitUSBDevice() 中直接初始化
 * ============================================ */

static void on_audio_detect(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventAudioDetData_t *det = (const BG_EventAudioDetData_t *)data;
    (void)size;
    DBG("[Evt] Audio port %d %s (topic=0x%04X)\n",
        det->port_id, det->connected ? "IN" : "OUT", (uint16_t)topic);
}

static void on_usb_event(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)data; (void)size;
    if ((uint16_t)topic == EVT_SYS_USB_CONNECT)
        DBG("[Evt] USB connected\n");
    else
        DBG("[Evt] USB disconnected\n");
}

BG_EVT_SUB(EVT_AUDIO_MIC_IN,       on_audio_detect);
BG_EVT_SUB(EVT_AUDIO_GUITAR_IN,    on_audio_detect);
BG_EVT_SUB(EVT_AUDIO_HP_OUT,       on_audio_detect);
/* USB 事件订阅已移除：USB 直接初始化，不依赖事件系统 */
/* BG_EVT_SUB(EVT_SYS_USB_CONNECT,    on_usb_event); */
/* BG_EVT_SUB(EVT_SYS_USB_DISCONNECT, on_usb_event); */

/* ============================================
 * 5. 系统状态事件回调 (运行态 / 子系统状态)
 * ============================================ */

#include "sys_state.h"

static void on_sys_run_state(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventSysRunState_t *evt = (const BG_EventSysRunState_t *)data;
    (void)topic; (void)size;

    static const char *state_names[] = {
        "OFF", "BOOT", "RUNNING", "IDLE", "SHUTDOWN"
    };
    DBG("[Evt] RunState: %s -> %s\n",
        state_names[evt->old_state], state_names[evt->new_state]);
}

static void on_sys_sub_state(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventSysSubState_t *evt = (const BG_EventSysSubState_t *)data;
    (void)topic; (void)size;

    DBG("[Evt] SubState diff=0x%04X new=0x%04X\n",
        evt->changed_bits, evt->new_state);

    if (evt->changed_bits & SYS_SUB_BT_CONNECTED)
        DBG("[Evt]   BT %s\n", (evt->new_state & SYS_SUB_BT_CONNECTED) ? "connected" : "disconnected");
    if (evt->changed_bits & SYS_SUB_BLE_CONNECTED)
        DBG("[Evt]   BLE %s\n", (evt->new_state & SYS_SUB_BLE_CONNECTED) ? "connected" : "disconnected");
    if (evt->changed_bits & SYS_SUB_USB_CONNECTED)
        DBG("[Evt]   USB %s\n", (evt->new_state & SYS_SUB_USB_CONNECTED) ? "connected" : "disconnected");
}

BG_EVT_SUB(EVT_SYS_RUN_STATE, on_sys_run_state);
BG_EVT_SUB(EVT_SYS_SUB_STATE, on_sys_sub_state);
