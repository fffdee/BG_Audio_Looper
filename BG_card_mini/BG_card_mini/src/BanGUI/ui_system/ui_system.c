/**
 * @file    ui_system.c
 * @brief   UI系统主模块实现
 * @author  BG Card Team
 * @date    2025-12-18
 */

#include "ui_system.h"
#include "bg_lcd.h"
#include <string.h>

/*===========================================================================
 * 私有变量
 *===========================================================================*/

static UI_SystemConfig_t sys_config;
static UI_SystemState_t sys_state;
static UI_SystemState_t prev_state;
static UI_Menu_t* main_menu;
static bool system_ready;
static uint32_t idle_timer;

/* 弹出框状态 */
static struct {
    bool active;
    const char* title;
    const char* message;
    uint16_t duration;
    uint32_t timer;
} popup;

/*===========================================================================
 * 私有函数
 *===========================================================================*/

/**
 * @brief 绘制空闲界面 (主界面)
 */
static void draw_idle_screen(void)
{
    uint16_t y = UI_StatusBar_GetHeight();
    
    /* 清除主区域 */
    BG_lcd.Box(0, y, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT - y, UI_COLOR_BLACK);
    
    /* 显示简单的主界面信息 */
    uint16_t text_y = y + 30;
    const char* text = "BG Card Mini";
    uint16_t text_x = (UI_SCREEN_WIDTH - strlen(text) * 8) / 2;
    const char* p = text;
    while (*p) {
        BG_lcd.ShowChar(text_x, text_y, *p, UI_COLOR_WHITE);
        text_x += 8;
        p++;
    }
    
    /* 显示提示 */
    text = "ENTER for Menu";
    text_x = (UI_SCREEN_WIDTH - strlen(text) * 8) / 2;
    text_y += 24;
    p = text;
    while (*p) {
        BG_lcd.ShowChar(text_x, text_y, *p, UI_COLOR_GRAY);
        text_x += 8;
        p++;
    }
}

/**
 * @brief 绘制弹出框
 */
static void draw_popup(void)
{
    uint16_t box_w = 140;
    uint16_t box_h = 60;
    uint16_t box_x = (UI_SCREEN_WIDTH - box_w) / 2;
    uint16_t box_y = (UI_SCREEN_HEIGHT - box_h) / 2;
    
    /* 绘制背景 */
    BG_lcd.Box(box_x, box_y, box_w, box_h, UI_COLOR_DARK_GRAY);
    
    /* 绘制边框 */
    BG_lcd.DrawLine(box_x, box_y, box_x + box_w - 1, box_y, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x, box_y + box_h - 1, box_x + box_w - 1, box_y + box_h - 1, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x, box_y, box_x, box_y + box_h - 1, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x + box_w - 1, box_y, box_x + box_w - 1, box_y + box_h - 1, UI_COLOR_WHITE);
    
    /* 绘制标题 */
    if (popup.title) {
        uint16_t title_x = box_x + (box_w - strlen(popup.title) * 8) / 2;
        const char* p = popup.title;
        while (*p) {
            BG_lcd.ShowChar(title_x, box_y + 8, *p, UI_COLOR_CYAN);
            title_x += 8;
            p++;
        }
    }
    
    /* 绘制消息 */
    if (popup.message) {
        uint16_t msg_x = box_x + (box_w - strlen(popup.message) * 8) / 2;
        const char* p = popup.message;
        while (*p) {
            BG_lcd.ShowChar(msg_x, box_y + 28, *p, UI_COLOR_WHITE);
            msg_x += 8;
            p++;
        }
    }
    
    /* 绘制提示 */
    const char* hint = "[OK] Close";
    uint16_t hint_x = box_x + (box_w - strlen(hint) * 8) / 2;
    const char* p = hint;
    while (*p) {
        BG_lcd.ShowChar(hint_x, box_y + box_h - 14, *p, UI_COLOR_GRAY);
        hint_x += 8;
        p++;
    }
}

/**
 * @brief 切换状态
 */
static void change_state(UI_SystemState_t new_state)
{
    if (sys_state == new_state) return;
    
    prev_state = sys_state;
    sys_state = new_state;
    idle_timer = 0;
    
    /* 状态进入处理 */
    switch (new_state) {
        case UI_STATE_IDLE:
            UI_Menu_SetVisible(false);
            UI_StatusBar_Draw();
            draw_idle_screen();
            break;
            
        case UI_STATE_MENU:
            UI_Menu_SetVisible(true);
            UI_StatusBar_Draw();
            UI_Menu_Draw();
            break;
            
        case UI_STATE_POPUP:
            draw_popup();
            break;
            
        default:
            break;
    }
}

/*===========================================================================
 * API 实现
 *===========================================================================*/

void UI_System_Init(const UI_SystemConfig_t* config)
{
    if (config) {
        memcpy(&sys_config, config, sizeof(UI_SystemConfig_t));
    } else {
        /* 默认配置 */
        sys_config.skip_boot = false;
        sys_config.auto_statusbar = true;
        sys_config.idle_timeout = 0;  /* 禁用空闲超时 */
    }
    
    /* 初始化各子模块 */
    UI_Button_Init();
    UI_StatusBar_Init();
    UI_Menu_Init();
    UI_BootScreen_Init(NULL);
    
    sys_state = UI_STATE_BOOT;
    prev_state = UI_STATE_BOOT;
    main_menu = NULL;
    system_ready = false;
    idle_timer = 0;
    
    memset(&popup, 0, sizeof(popup));
}

void UI_System_Start(void)
{
    if (sys_config.skip_boot) {
        system_ready = true;
        change_state(UI_STATE_IDLE);
    } else {
        sys_state = UI_STATE_BOOT;
        UI_BootScreen_Start();
    }
}

void UI_System_Update(uint16_t delta_ms)
{
    /* 按键扫描 */
    UI_Button_Scan(delta_ms);
    
    /* 处理按键事件 */
    UI_ButtonEventData_t event;
    while (UI_Button_GetEvent(&event)) {
        UI_System_HandleEvent(&event);
    }
    
    /* 状态更新 */
    switch (sys_state) {
        case UI_STATE_BOOT:
            if (!UI_BootScreen_Update(delta_ms)) {
                /* 开机画面完成 */
                system_ready = true;
                change_state(UI_STATE_IDLE);
            }
            break;
            
        case UI_STATE_IDLE:
            /* 状态栏更新 */
            UI_StatusBar_Update();
            
            /* 空闲超时处理 */
            if (sys_config.idle_timeout > 0) {
                idle_timer += delta_ms;
                if (idle_timer >= sys_config.idle_timeout * 1000) {
                    /* 超时处理 - 可以进入省电模式等 */
                    idle_timer = 0;
                }
            }
            break;
            
        case UI_STATE_MENU:
            UI_StatusBar_Update();
            UI_Menu_Update();
            break;
            
        case UI_STATE_POPUP:
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

void UI_System_HandleEvent(UI_ButtonEventData_t* event)
{
    if (!event) return;
    
    /* 开机画面时按任意键跳过 */
    if (sys_state == UI_STATE_BOOT) {
        if (event->event == UI_BTN_EVENT_CLICKED) {
            UI_BootScreen_Skip();
            system_ready = true;
            change_state(UI_STATE_IDLE);
        }
        return;
    }
    
    /* 弹出框处理 */
    if (sys_state == UI_STATE_POPUP) {
        if (event->event == UI_BTN_EVENT_CLICKED) {
            UI_System_ClosePopup();
        }
        return;
    }
    
    /* 只处理单击和长按事件 */
    if (event->event != UI_BTN_EVENT_CLICKED && 
        event->event != UI_BTN_EVENT_LONG_PRESS &&
        event->event != UI_BTN_EVENT_REPEAT) {
        return;
    }
    
    idle_timer = 0;  /* 重置空闲计时 */
    
    switch (sys_state) {
        case UI_STATE_IDLE:
            /* 空闲界面：ENTER进入菜单 */
            if (event->id == UI_BTN_ENTER && event->event == UI_BTN_EVENT_CLICKED) {
                UI_System_ShowMenu();
            }
            break;
            
        case UI_STATE_MENU:
            /* 菜单界面导航 */
            switch (event->id) {
                case UI_BTN_UP:
                    UI_Menu_Up();
                    break;
                case UI_BTN_DOWN:
                    UI_Menu_Down();
                    break;
                case UI_BTN_ENTER:
                    UI_Menu_Enter();
                    break;
                case UI_BTN_BACK:
                    /* 如果在根菜单，返回空闲界面 */
                    if (UI_Menu_GetCurrent() == main_menu && !UI_Menu_IsEditing()) {
                        UI_System_HideMenu();
                    } else {
                        UI_Menu_Back();
                    }
                    break;
                default:
                    break;
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
    change_state(UI_STATE_MENU);
}

void UI_System_HideMenu(void)
{
    change_state(UI_STATE_IDLE);
}

void UI_System_ShowPopup(const char* title, const char* message, uint16_t duration_ms)
{
    popup.active = true;
    popup.title = title;
    popup.message = message;
    popup.duration = duration_ms;
    popup.timer = 0;
    
    change_state(UI_STATE_POPUP);
}

void UI_System_ClosePopup(void)
{
    popup.active = false;
    change_state(prev_state);
    UI_System_Refresh();
}

void UI_System_Refresh(void)
{
    if (sys_config.auto_statusbar) {
        UI_StatusBar_Draw();
    }
    
    switch (sys_state) {
        case UI_STATE_IDLE:
            draw_idle_screen();
            break;
        case UI_STATE_MENU:
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
