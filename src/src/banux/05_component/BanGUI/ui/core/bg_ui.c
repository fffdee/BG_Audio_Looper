/**
 * @file    bg_ui.c
 * @brief   BanGUI - Unified UI System Implementation
 * @author  BG Card Team
 * @date    2025-01-08
 */

#include "bg_ui.h"
#include "bg_lcd.h"
#include "gpio.h"
#include <string.h>

/*===========================================================================
 * ����
 *===========================================================================*/

#define MAX_VIEWS_PER_STATE     4
#define BTN_DEBOUNCE_MS         20
#define BTN_LONG_PRESS_MS       800
#define BTN_REPEAT_MS           200
#define STATUSBAR_HEIGHT        12

/*===========================================================================
 * ��ť GPIO ӳ��
 *===========================================================================*/

typedef struct {
    uint32_t port_in;
    uint32_t pin;
} BtnGPIO_t;

static const BtnGPIO_t s_btn_gpio[UI_BTN_COUNT] = {
    { GPIO_A_IN, GPIO_INDEX0  },    /* UP - A0 */
    { GPIO_B_IN, GPIO_INDEX5  },    /* DOWN - B5 */
    { GPIO_A_IN, GPIO_INDEX15 },    /* ENTER - A15 */
    { GPIO_A_IN, GPIO_INDEX16 },    /* BACK - A16 */
};

/*===========================================================================
 * ��ť״̬
 *===========================================================================*/

typedef struct {
    uint8_t raw;
    uint8_t debounce;
    uint16_t press_time;
    bool long_fired;
} BtnState_t;

static BtnState_t s_btn_state[UI_BTN_COUNT];

/*===========================================================================
 * View ����
 *===========================================================================*/

typedef struct {
    UI_View_t* views[MAX_VIEWS_PER_STATE];
    uint8_t count;
} ViewSlot_t;

static ViewSlot_t s_views[UI_STATE_COUNT];

/*===========================================================================
 * ״̬����
 *===========================================================================*/

static UI_State_t s_state = UI_STATE_IDLE;
static UI_State_t s_prev_state = UI_STATE_IDLE;
static bool s_ready = false;
static bool s_dirty = true;
static UI_StateCallback_t s_state_callback = NULL;
static bool s_debug = false;
static uint16_t s_bg_color = 0x0000;  /* 默认黑色背景 */

/*===========================================================================
 * ����״̬
 *===========================================================================*/

static struct {
    bool active;
    const char* title;
    const char* message;
    uint16_t duration;
    uint32_t timer;
    bool drawn;          /* ����Ƿ��ѻ��ƣ������ظ����� */
} s_popup;

/*===========================================================================
 * ״̬������
 *===========================================================================*/

static struct {
    uint8_t bt_status;
    uint8_t volume;
    uint8_t battery;
    bool dirty;
} s_statusbar;

/*===========================================================================
 * ״̬����
 *===========================================================================*/

static const char* s_state_names[] = {
    "BOOT", "IDLE", "MENU", "POPUP", "LOOPER", "SETTINGS"
};

/*===========================================================================
 * ˽�к���
 *===========================================================================*/

static uint8_t read_btn_gpio(UI_BtnID_t id)
{
    if (id >= UI_BTN_COUNT) return 1;
    return GPIO_RegOneBitGet(s_btn_gpio[id].port_in, s_btn_gpio[id].pin) ? 1 : 0;
}

static void push_btn_event(UI_BtnID_t id, UI_BtnEvent_t event, uint16_t duration)
{
    UI_BtnEventData_t evt = { id, event, duration };
    /* ֱ�Ӵ����¼� */
    BG_UI.HandleButton(&evt);
}

static void exit_state(UI_State_t state)
{
    ViewSlot_t* slot = &s_views[state];
    uint8_t i;
    for (i = 0; i < slot->count; i++) {
        if (slot->views[i] && slot->views[i]->on_exit) {
            slot->views[i]->on_exit();
        }
    }
}

static void enter_state(UI_State_t state)
{
    ViewSlot_t* slot = &s_views[state];
    uint8_t i;
    for (i = 0; i < slot->count; i++) {
        if (slot->views[i] && slot->views[i]->on_enter) {
            slot->views[i]->on_enter();
        }
    }
    s_dirty = true;
}

static void draw_statusbar(void)
{
    /* ����״̬������ */
    BG_lcd.Box(0, 0, UI_SCREEN_WIDTH, STATUSBAR_HEIGHT, UI_DARK_GRAY);
    
    /* ����ͼ�� (�򻯰�) */
    if (s_statusbar.bt_status > 0) {
        uint16_t color = (s_statusbar.bt_status >= 3) ? UI_BLUE : UI_GRAY;
        BG_lcd.ShowChar(2, 2, 'B', color);
    }
    
    /* ������ʾ */
    char bat_str[4];
    bat_str[0] = '0' + (s_statusbar.battery / 10) % 10;
    bat_str[1] = '0' + s_statusbar.battery % 10;
    bat_str[2] = '%';
    bat_str[3] = '\0';
    BG_lcd.ShowChar(UI_SCREEN_WIDTH - 24, 2, bat_str[0], UI_WHITE);
    BG_lcd.ShowChar(UI_SCREEN_WIDTH - 16, 2, bat_str[1], UI_WHITE);
    BG_lcd.ShowChar(UI_SCREEN_WIDTH - 8, 2, bat_str[2], UI_WHITE);
    
    s_statusbar.dirty = false;
}

static void draw_popup(void)
{
    if (!s_popup.active) return;
    
    uint16_t w = 140, h = 60;
    uint16_t x = (UI_SCREEN_WIDTH - w) / 2;
    uint16_t y = (UI_SCREEN_HEIGHT - h) / 2;
    
    /* ���� */
    BG_lcd.Box(x, y, w, h, UI_DARK_GRAY);
    
    /* �߿� */
    BG_lcd.DrawLine(x, y, x + w - 1, y, UI_WHITE);
    BG_lcd.DrawLine(x, y + h - 1, x + w - 1, y + h - 1, UI_WHITE);
    BG_lcd.DrawLine(x, y, x, y + h - 1, UI_WHITE);
    BG_lcd.DrawLine(x + w - 1, y, x + w - 1, y + h - 1, UI_WHITE);
    
    /* ���� */
    if (s_popup.title) {
        uint16_t tx = x + (w - strlen(s_popup.title) * 8) / 2;
        const char* p = s_popup.title;
        while (*p) {
            BG_lcd.ShowChar(tx, y + 8, *p++, UI_CYAN);
            tx += 8;
        }
    }
    
    /* ��Ϣ */
    if (s_popup.message) {
        uint16_t mx = x + (w - strlen(s_popup.message) * 8) / 2;
        const char* p = s_popup.message;
        while (*p) {
            BG_lcd.ShowChar(mx, y + 28, *p++, UI_WHITE);
            mx += 8;
        }
    }
}

static void update_views(uint16_t delta_ms)
{
    ViewSlot_t* slot = &s_views[s_state];
    uint8_t i;
    for (i = 0; i < slot->count; i++) {
        UI_View_t* v = slot->views[i];
        if (v && v->visible && v->on_update) {
            v->on_update(delta_ms);
        }
    }
}

static void draw_views(void)
{
    ViewSlot_t* slot = &s_views[s_state];
    uint8_t i;
    for (i = 0; i < slot->count; i++) {
        UI_View_t* v = slot->views[i];
        if (v && v->visible && v->on_draw && (v->dirty || s_dirty)) {
            v->on_draw();
            v->dirty = false;
        }
    }
    s_dirty = false;
}

static bool dispatch_btn_to_views(UI_BtnEventData_t* event)
{
    ViewSlot_t* slot = &s_views[s_state];
    int i;
    for (i = slot->count - 1; i >= 0; i--) {
        UI_View_t* v = slot->views[i];
        if (v && v->visible && v->on_button) {
            if (v->on_button(event)) {
                return true;
            }
        }
    }
    return false;
}

/*===========================================================================
 * API ʵ��
 *===========================================================================*/

static void ui_init(void)
{
    /* ��� view ��λ */
    memset(s_views, 0, sizeof(s_views));
    
    /* ��ʼ����ť״̬ */
    int i;
    for (i = 0; i < UI_BTN_COUNT; i++) {
        s_btn_state[i].raw = 1;
        s_btn_state[i].debounce = 0;
        s_btn_state[i].press_time = 0;
        s_btn_state[i].long_fired = false;
    }
    
    /* ��ʼ��״̬�� */
    s_statusbar.bt_status = 0;
    s_statusbar.volume = 50;
    s_statusbar.battery = 100;
    s_statusbar.dirty = true;
    
    /* ��յ��� */
    memset(&s_popup, 0, sizeof(s_popup));
    
    /* ��ʼ��ΪBOOT״̬���ȴ�SetState����ʵ������״̬ */
    s_state = UI_STATE_BOOT;
    s_prev_state = UI_STATE_BOOT;
    s_ready = false;
    s_dirty = true;
}

static void ui_start(void)
{
    s_ready = true;
    enter_state(s_state);
}

static void ui_update(uint16_t delta_ms)
{
    /* ɨ�谴ť */
    BG_UI.ScanButtons(delta_ms);
    
    /* ���µ�����ʱ */
    if (s_popup.active && s_popup.duration > 0) {
        s_popup.timer += delta_ms;
        if (s_popup.timer >= s_popup.duration) {
            BG_UI.ClosePopup();
        }
    }
    
    /* ���� views */
    update_views(delta_ms);
    
    /* ״̬���ɸ�View�Լ���on_draw�л��ƣ����ﲻ�ظ����� */
    /* Boot���治��ʾ״̬����Home/Menu�Ƚ����Լ�����UI_StatusBar_Draw() */
    
    /* ���� views */
    draw_views();
    
    /* ���Ƶ��� (���ϲ�) - ֻ���״λ�dirtyʱ���� */
    if (s_popup.active && !s_popup.drawn) {
        draw_popup();
        s_popup.drawn = true;
    }
}

static void ui_set_state(UI_State_t state)
{
    if (state >= UI_STATE_COUNT || state == s_state) return;
    
    /* �رյ��� */
    if (s_popup.active && state != UI_STATE_POPUP) {
        BG_UI.ClosePopup();
    }
    
    exit_state(s_state);
    s_prev_state = s_state;
    s_state = state;
    enter_state(state);
    
    if (s_state_callback) {
        s_state_callback(s_prev_state, s_state);
    }
}

static UI_State_t ui_get_state(void)
{
    return s_state;
}

static UI_State_t ui_get_prev_state(void)
{
    return s_prev_state;
}

static bool ui_is_ready(void)
{
    return s_ready;
}

static void ui_register_view(UI_State_t state, UI_View_t* view)
{
    if (state >= UI_STATE_COUNT || !view) return;
    
    ViewSlot_t* slot = &s_views[state];
    if (slot->count < MAX_VIEWS_PER_STATE) {
        slot->views[slot->count++] = view;
    }
}

static void ui_unregister_view(UI_View_t* view)
{
    int s, i, j;
    if (!view) return;
    
    for (s = 0; s < UI_STATE_COUNT; s++) {
        ViewSlot_t* slot = &s_views[s];
        for (i = 0; i < slot->count; i++) {
            if (slot->views[i] == view) {
                for (j = i; j < slot->count - 1; j++) {
                    slot->views[j] = slot->views[j + 1];
                }
                slot->count--;
                return;
            }
        }
    }
}

static void ui_handle_button(UI_BtnEventData_t* event)
{
    if (!event) return;
    
    /* ����ʱ�����ⰴť�ر� */
    if (s_popup.active) {
        if (event->event == UI_BTN_EVT_CLICK) {
            BG_UI.ClosePopup();
        }
        return;
    }
    
    /* �ַ��� views */
    dispatch_btn_to_views(event);
}

static void ui_scan_buttons(uint16_t delta_ms)
{
    int i;
    for (i = 0; i < UI_BTN_COUNT; i++) {
        BtnState_t* btn = &s_btn_state[i];
        uint8_t cur = read_btn_gpio((UI_BtnID_t)i);
        
        /* ȥ�� */
        if (cur != btn->raw) {
            btn->debounce++;
            if (btn->debounce >= (BTN_DEBOUNCE_MS / delta_ms)) {
                btn->debounce = 0;
                btn->raw = cur;
                
                if (cur == 0) {
                    /* ���� */
                    btn->press_time = 0;
                    btn->long_fired = false;
                } else {
                    /* �ͷ� */
                    if (!btn->long_fired) {
                        push_btn_event((UI_BtnID_t)i, UI_BTN_EVT_CLICK, btn->press_time);
                    }
                    btn->press_time = 0;
                }
            }
        } else {
            btn->debounce = 0;
        }
        
        /* ������� */
        if (btn->raw == 0) {
            btn->press_time += delta_ms;
            
            if (!btn->long_fired && btn->press_time >= BTN_LONG_PRESS_MS) {
                btn->long_fired = true;
                push_btn_event((UI_BtnID_t)i, UI_BTN_EVT_LONG_PRESS, btn->press_time);
            }
        }
    }
}

static bool ui_is_button_pressed(UI_BtnID_t id)
{
    if (id >= UI_BTN_COUNT) return false;
    return (s_btn_state[id].raw == 0);
}

static void ui_show_popup(const char* title, const char* msg, uint16_t duration_ms)
{
    s_popup.active = true;
    s_popup.title = title;
    s_popup.message = msg;
    s_popup.duration = duration_ms;
    s_popup.timer = 0;
    s_popup.drawn = false;  /* ���Ϊδ���� */
    s_dirty = true;
}

static void ui_close_popup(void)
{
    s_popup.active = false;
    s_popup.drawn = false;
    s_dirty = true;
}

static bool ui_is_popup_active(void)
{
    return s_popup.active;
}

static void ui_invalidate(void)
{
    int i;
    s_dirty = true;
    ViewSlot_t* slot = &s_views[s_state];
    for (i = 0; i < slot->count; i++) {
        if (slot->views[i]) {
            slot->views[i]->dirty = true;
        }
    }
}

static void ui_invalidate_view(UI_View_t* view)
{
    if (view) {
        view->dirty = true;
    }
}

static void ui_on_state_change(UI_StateCallback_t callback)
{
    s_state_callback = callback;
}

static void ui_statusbar_set_bt(uint8_t status)
{
    s_statusbar.bt_status = status;
    s_statusbar.dirty = true;
}

static void ui_statusbar_set_volume(uint8_t volume)
{
    s_statusbar.volume = volume;
    s_statusbar.dirty = true;
}

static void ui_statusbar_set_battery(uint8_t level)
{
    s_statusbar.battery = level;
    s_statusbar.dirty = true;
}

static void ui_statusbar_update(void)
{
    s_statusbar.dirty = true;
}

static const char* ui_get_state_name(UI_State_t state)
{
    if (state < UI_STATE_COUNT) {
        return s_state_names[state];
    }
    return "UNKNOWN";
}

static void ui_set_debug(bool enable)
{
    s_debug = enable;
}

static void ui_set_background_color(uint16_t color)
{
    s_bg_color = color;
    /* 标记需要重绘，在下次UI刷新时会使用新背景色 */
    s_dirty = true;
}

static uint16_t ui_get_background_color(void)
{
    return s_bg_color;
}

/*===========================================================================
 * BG_UI ����ʵ��
 *===========================================================================*/

const BG_UI_t BG_UI = {
    /* �������� */
    .Init = ui_init,
    .Start = ui_start,
    .Update = ui_update,
    
    /* ״̬���� */
    .SetState = ui_set_state,
    .GetState = ui_get_state,
    .GetPrevState = ui_get_prev_state,
    .IsReady = ui_is_ready,
    
    /* View ���� */
    .RegisterView = ui_register_view,
    .UnregisterView = ui_unregister_view,
    
    /* ��ť���� */
    .HandleButton = ui_handle_button,
    .ScanButtons = ui_scan_buttons,
    .IsButtonPressed = ui_is_button_pressed,
    
    /* ���� */
    .ShowPopup = ui_show_popup,
    .ClosePopup = ui_close_popup,
    .IsPopupActive = ui_is_popup_active,
    
    /* ˢ�� */
    .Invalidate = ui_invalidate,
    .InvalidateView = ui_invalidate_view,
    
    /* 背景色 */
    .SetBackgroundColor = ui_set_background_color,
    .GetBackgroundColor = ui_get_background_color,
    
    /* �ص�ע�� */
    .OnStateChange = ui_on_state_change,
    
    /* ״̬�� */
    .StatusBar_SetBT = ui_statusbar_set_bt,
    .StatusBar_SetVolume = ui_statusbar_set_volume,
    .StatusBar_SetBattery = ui_statusbar_set_battery,
    .StatusBar_Update = ui_statusbar_update,
    
    /* ���� */
    .GetStateName = ui_get_state_name,
    .SetDebug = ui_set_debug,
};
