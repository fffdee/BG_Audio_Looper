/**
 * @file    comp_popup.c
 * @brief   Popup Component Implementation
 * @author  BG Card Team
 * @date    2025-01-08
 */

#include "comp_popup.h"
#include "bg_lcd.h"
#include <string.h>

/*===========================================================================
 * 閰嶇疆甯搁噺
 *===========================================================================*/

#define POPUP_WIDTH         140
#define POPUP_HEIGHT        60
#define POPUP_BORDER        2

/*===========================================================================
 * 绉佹湁鍙橀噺
 *===========================================================================*/

static struct {
    bool active;
    PopupType_t type;
    const char* title;
    const char* message;
    uint16_t duration;
    uint32_t timer;
} s_popup;

/*===========================================================================
 * 绉佹湁鍑芥暟
 *===========================================================================*/

static uint16_t get_type_color(PopupType_t type)
{
    switch (type) {
        case POPUP_TYPE_WARNING: return UI_YELLOW;
        case POPUP_TYPE_ERROR:   return UI_RED;
        case POPUP_TYPE_SUCCESS: return UI_GREEN;
        default:                 return UI_CYAN;
    }
}

/*===========================================================================
 * 鍏叡 API
 *===========================================================================*/

void Comp_Popup_Init(void)
{
    memset(&s_popup, 0, sizeof(s_popup));
}

void Comp_Popup_Show(PopupType_t type, const char* title, 
                     const char* message, uint16_t duration_ms)
{
    s_popup.active = true;
    s_popup.type = type;
    s_popup.title = title;
    s_popup.message = message;
    s_popup.duration = duration_ms;
    s_popup.timer = 0;
}

void Comp_Popup_Close(void)
{
    s_popup.active = false;
}

bool Comp_Popup_IsActive(void)
{
    return s_popup.active;
}

void Comp_Popup_Update(uint16_t delta_ms)
{
    if (!s_popup.active) return;
    
    if (s_popup.duration > 0) {
        s_popup.timer += delta_ms;
        if (s_popup.timer >= s_popup.duration) {
            Comp_Popup_Close();
        }
    }
}

void Comp_Popup_Draw(void)
{
    if (!s_popup.active) return;
    
    uint16_t x = (UI_SCREEN_WIDTH - POPUP_WIDTH) / 2;
    uint16_t y = (UI_SCREEN_HEIGHT - POPUP_HEIGHT) / 2;
    uint16_t type_color = get_type_color(s_popup.type);
    
    /* 缁樺埗鑳屾櫙 */
    BG_lcd.Box(x, y, POPUP_WIDTH, POPUP_HEIGHT, UI_DARK_GRAY);
    
    /* 缁樺埗杈规 */
    BG_lcd.DrawLine(x, y, x + POPUP_WIDTH - 1, y, type_color);
    BG_lcd.DrawLine(x, y + POPUP_HEIGHT - 1, x + POPUP_WIDTH - 1, y + POPUP_HEIGHT - 1, type_color);
    BG_lcd.DrawLine(x, y, x, y + POPUP_HEIGHT - 1, type_color);
    BG_lcd.DrawLine(x + POPUP_WIDTH - 1, y, x + POPUP_WIDTH - 1, y + POPUP_HEIGHT - 1, type_color);
    
    /* 缁樺埗鏍囬 */
    if (s_popup.title) {
        uint16_t title_len = strlen(s_popup.title);
        uint16_t tx = x + (POPUP_WIDTH - title_len * 8) / 2;
        const char* p = s_popup.title;
        while (*p) {
            BG_lcd.ShowChar(tx, y + 8, *p++, type_color);
            tx += 8;
        }
    }
    
    /* 缁樺埗娑堟伅 */
    if (s_popup.message) {
        uint16_t msg_len = strlen(s_popup.message);
        uint16_t mx = x + (POPUP_WIDTH - msg_len * 8) / 2;
        if (mx < x + 4) mx = x + 4;  /* 宸﹁竟璺�*/
        
        const char* p = s_popup.message;
        while (*p) {
            BG_lcd.ShowChar(mx, y + 28, *p++, UI_WHITE);
            mx += 8;
        }
    }
    
    /* 缁樺埗鍏抽棴鎻愮ず */
    const char* hint = "Press any key";
    uint16_t hint_len = strlen(hint);
    uint16_t hx = x + (POPUP_WIDTH - hint_len * 6) / 2;
    const char* hp = hint;
    while (*hp) {
        BG_lcd.ShowChar(hx, y + POPUP_HEIGHT - 12, *hp++, UI_GRAY);
        hx += 6;
    }
}

bool Comp_Popup_HandleButton(UI_BtnEventData_t* event)
{
    if (!s_popup.active) return false;
    
    /* 浠绘剰鎸夐敭鍏抽棴寮圭獥 */
    if (event->event == UI_BTN_EVT_CLICK) {
        Comp_Popup_Close();
        return true;
    }
    
    return false;
}
