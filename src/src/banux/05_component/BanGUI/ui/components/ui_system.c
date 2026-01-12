/**
 * @file    ui_system.c
 * @brief   UI system compatibility layer implementation
 * @author  BG Card Team
 * @date    2025-01-09
 */

#include "ui_system.h"
#include "bg_lcd.h"
#include "gui_tool.h"
#include <string.h>

/*===========================================================================
 * Private variables
 *===========================================================================*/

static UI_SystemConfig_t sys_config;
static UI_SystemState_t sys_state;
static UI_SystemState_t prev_state;
static UI_Menu_t* main_menu;
static bool system_ready;

/* Popup window structure */
static struct {
    bool active;
    const char* title;
    const char* message;
    uint16_t duration;
    uint32_t timer;
} popup;

/*===========================================================================
 * Private functions
 *===========================================================================*/

/**
 * @brief Draw popup window
 */
static void draw_popup(void)
{
    uint16_t box_w = 140;
    uint16_t box_h = 60;
    uint16_t box_x = (UI_SCREEN_WIDTH - box_w) / 2;
    uint16_t box_y = (UI_SCREEN_HEIGHT - box_h) / 2;
    const char* p;
    uint16_t x;
    
    /* Draw popup background */
    BG_lcd.Box(box_x, box_y, box_w, box_h, UI_COLOR_DARK_GRAY);
    
    /* Draw popup border */
    BG_lcd.DrawLine(box_x, box_y, box_x + box_w - 1, box_y, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x, box_y + box_h - 1, box_x + box_w - 1, box_y + box_h - 1, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x, box_y, box_x, box_y + box_h - 1, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x + box_w - 1, box_y, box_x + box_w - 1, box_y + box_h - 1, UI_COLOR_WHITE);
    
    /* Draw popup title */
    if (popup.title) {
        x = box_x + (box_w - strlen(popup.title) * 8) / 2;
        p = popup.title;
        while (*p) {
            BG_lcd.ShowChar(x, box_y + 8, *p, UI_COLOR_CYAN);
            x += 8;
            p++;
        }
    }
    
    /* Draw popup message */
    if (popup.message) {
        x = box_x + (box_w - strlen(popup.message) * 8) / 2;
        p = popup.message;
        while (*p) {
            BG_lcd.ShowChar(x, box_y + 28, *p, UI_COLOR_WHITE);
            x += 8;
            p++;
        }
    }
    
    /* Draw popup hint */
    {
        const char* hint = "[OK] Close";
        x = box_x + (box_w - strlen(hint) * 8) / 2;
        p = hint;
        while (*p) {
            BG_lcd.ShowChar(x, box_y + box_h - 14, *p, UI_COLOR_GRAY);
            x += 8;
            p++;
        }
    }
}

/**
 * @brief Change system state
 */
static void change_state(UI_SystemState_t new_state)
{
    if (sys_state == new_state) return;
    
    prev_state = sys_state;
    sys_state = new_state;
    
    /* Handle state transition */
    switch (new_state) {
        case UI_SYS_STATE_IDLE:
            UI_Menu_SetVisible(false);
            /* 注意: view的on_draw会绘制状态栏，这里不需要重复调用 */
            break;
            
        case UI_SYS_STATE_MENU:
            UI_Menu_SetVisible(true);
            UI_StatusBar_Draw();
            UI_Menu_Draw();
            break;
            
        case UI_SYS_STATE_POPUP:
            draw_popup();
            break;
            
        default:
            break;
    }
}

/*===========================================================================
 * API Implementation
 *===========================================================================*/

void UI_System_Init(const UI_SystemConfig_t* config)
{
    if (config) {
        memcpy(&sys_config, config, sizeof(UI_SystemConfig_t));
    } else {
        /* Default configuration */
        sys_config.skip_boot = true;
        sys_config.auto_statusbar = true;
        sys_config.idle_timeout = 0;
    }
    
    /* Initialize submodules */
    UI_StatusBar_Init();
    UI_Menu_Init();
    
    sys_state = UI_SYS_STATE_IDLE;
    prev_state = UI_SYS_STATE_IDLE;
    main_menu = NULL;
    system_ready = true;
    
    memset(&popup, 0, sizeof(popup));
}

void UI_System_Start(void)
{
    system_ready = true;
    change_state(UI_SYS_STATE_IDLE);
}

void UI_System_Update(uint16_t delta_ms)
{
    /* State machine */
    switch (sys_state) {
        case UI_SYS_STATE_IDLE:
            UI_StatusBar_Update();
            break;
            
        case UI_SYS_STATE_MENU:
            UI_StatusBar_Update();
            UI_Menu_Update();
            break;
            
        case UI_SYS_STATE_POPUP:
            if (popup.duration > 0) {
                popup.timer += delta_ms;
                if (popup.timer >= popup.duration) {
                    UI_System_ClosePopup();
                }
            }
            break;
            
        default:
            break;
    }
}

void UI_System_SetState(UI_SystemState_t state)
{
    change_state(state);
}

UI_SystemState_t UI_System_GetState(void)
{
    return sys_state;
}

void UI_System_ShowMenu(void)
{
    if (main_menu) {
        UI_Menu_SetRoot(main_menu);
    }
    change_state(UI_SYS_STATE_MENU);
}

void UI_System_HideMenu(void)
{
    change_state(UI_SYS_STATE_IDLE);
}

void UI_System_ShowPopup(const char* title, const char* message, uint16_t duration_ms)
{
    popup.active = true;
    popup.title = title;
    popup.message = message;
    popup.duration = duration_ms;
    popup.timer = 0;
    
    change_state(UI_SYS_STATE_POPUP);
}

void UI_System_ClosePopup(void)
{
    popup.active = false;
    change_state(prev_state);
    /* 注意: change_state已经会绘制界面，不需要再调用UI_System_Refresh */
}

void UI_System_Refresh(void)
{
    if (sys_config.auto_statusbar) {
        UI_StatusBar_Draw();
    }
    
    switch (sys_state) {
        case UI_SYS_STATE_IDLE:
            /* Redraw idle screen */
            break;
            
        case UI_SYS_STATE_MENU:
            UI_Menu_Draw();
            break;
            
        default:
            break;
    }
}

void UI_System_SetMainMenu(UI_Menu_t* menu)
{
    main_menu = menu;
}

UI_Menu_t* UI_System_GetMainMenu(void)
{
    return main_menu;
}

bool UI_System_IsReady(void)
{
    return system_ready;
}
