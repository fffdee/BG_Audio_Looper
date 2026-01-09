/**
 * @file    view_menu.c
 * @brief   Menu View - Navigation menu implementation (New Architecture)
 * @author  BG Card Team
 * @date    2025-01-08
 */

#include "view_menu.h"
#include "../components/comp_statusbar.h"
#include "../components/comp_menu.h"
#include <string.h>

/*===========================================================================
 * 绉佹湁鍙橀噺
 *===========================================================================*/

static UI_View_t s_menu_view;
static UI_Menu_t* s_current_menu = NULL;

/*===========================================================================
 * View 鍥炶皟
 *===========================================================================*/

static void menu_on_enter(void)
{
    s_menu_view.visible = true;
    s_menu_view.dirty = true;
    
    UI_Menu_SetVisible(true);
    if (s_current_menu) {
        UI_Menu_SetRoot(s_current_menu);
    }
}

static void menu_on_exit(void)
{
    s_menu_view.visible = false;
    UI_Menu_SetVisible(false);
}

static void menu_on_update(uint16_t delta_ms)
{
    (void)delta_ms;
    UI_StatusBar_Update();
    UI_Menu_Update();
}

static void menu_on_draw(void)
{
    UI_StatusBar_Draw();
    UI_Menu_Draw();
}

static bool menu_on_button(UI_BtnEventData_t* event)
{
    if (event->event != UI_BTN_EVT_CLICK &&
        event->event != UI_BTN_EVT_REPEAT) {
        return false;
    }
    
    switch (event->id) {
        case UI_BTN_UP:
            UI_Menu_Up();
            s_menu_view.dirty = true;
            return true;
            
        case UI_BTN_DOWN:
            UI_Menu_Down();
            s_menu_view.dirty = true;
            return true;
            
        case UI_BTN_ENTER:
            UI_Menu_Enter();
            s_menu_view.dirty = true;
            return true;
            
        case UI_BTN_BACK:
            /* 妫�煡鏄惁鍙互杩斿洖涓婄骇鑿滃崟 */
            if (UI_Menu_GetCurrent() && UI_Menu_GetCurrent()->parent) {
                UI_Menu_Back();
                s_menu_view.dirty = true;
            } else if (!UI_Menu_IsEditing()) {
                /* 閫�嚭鑿滃崟杩斿洖涓荤晫闈�*/
                BG_UI.SetState(UI_STATE_IDLE);
            } else {
                UI_Menu_Back();
                s_menu_view.dirty = true;
            }
            return true;
            
        default:
            break;
    }
    
    return false;
}

/*===========================================================================
 * 鍏叡 API
 *===========================================================================*/

UI_View_t* View_Menu_Create(void)
{
    memset(&s_menu_view, 0, sizeof(s_menu_view));
    
    s_menu_view.name = "Menu";
    s_menu_view.on_enter = menu_on_enter;
    s_menu_view.on_exit = menu_on_exit;
    s_menu_view.on_update = menu_on_update;
    s_menu_view.on_draw = menu_on_draw;
    s_menu_view.on_button = menu_on_button;
    s_menu_view.visible = false;
    s_menu_view.dirty = true;
    
    /* 鍒濆鍖栬彍鍗曠郴缁�*/
    UI_Menu_Init();
    
    /* 娉ㄥ唽鍒�MENU 鐘舵� */
    BG_UI.RegisterView(UI_STATE_MENU, &s_menu_view);
    
    return &s_menu_view;
}

void View_Menu_Destroy(void)
{
    BG_UI.UnregisterView(&s_menu_view);
}

void View_Menu_SetMenu(UI_Menu_t* menu)
{
    s_current_menu = menu;
    if (s_menu_view.visible) {
        UI_Menu_SetRoot(menu);
        s_menu_view.dirty = true;
    }
}

UI_Menu_t* View_Menu_GetMenu(void)
{
    return s_current_menu;
}

void View_Menu_Refresh(void)
{
    s_menu_view.dirty = true;
}
