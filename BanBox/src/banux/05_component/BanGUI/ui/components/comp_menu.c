/**
 * @file    comp_menu.c
 * @brief   Menu component implementation
 * @author  BG Card Team
 * @date    2025-01-09
 */

#include "comp_menu.h"
#include <string.h>
#include <stdlib.h>

/*===========================================================================
 * 私有变量
 *===========================================================================*/

static UI_MenuState_t s_menu_state = {
    .current = NULL,
    .stack_depth = 0,
    .editing = false,
    .need_redraw = true,
    .visible = false
};

/*===========================================================================
 * 私有函数
 *===========================================================================*/

static void menu_push_stack(UI_Menu_t* menu)
{
    if (s_menu_state.stack_depth < UI_MENU_MAX_DEPTH) {
        s_menu_state.stack[s_menu_state.stack_depth] = menu;
        s_menu_state.stack_depth++;
    }
}

static UI_Menu_t* menu_pop_stack(void)
{
    if (s_menu_state.stack_depth > 0) {
        s_menu_state.stack_depth--;
        return s_menu_state.stack[s_menu_state.stack_depth];
    }
    return NULL;
}

static UI_Menu_t* menu_peek_stack(void)
{
    if (s_menu_state.stack_depth > 0) {
        return s_menu_state.stack[s_menu_state.stack_depth - 1];
    }
    return NULL;
}

/*===========================================================================
 * API 实现
 *===========================================================================*/

void UI_Menu_Init(void)
{
    memset(&s_menu_state, 0, sizeof(s_menu_state));
    s_menu_state.editing = false;
    s_menu_state.need_redraw = true;
    s_menu_state.visible = false;
}

void UI_Menu_SetRoot(UI_Menu_t* menu)
{
    if (menu == NULL) {
        return;
    }
    
    s_menu_state.current = menu;
    s_menu_state.stack_depth = 0;
    menu_push_stack(menu);
    s_menu_state.editing = false;
    s_menu_state.need_redraw = true;
}

UI_Menu_t* UI_Menu_GetCurrent(void)
{
    return s_menu_state.current;
}

void UI_Menu_Draw(void)
{
    if (!s_menu_state.visible || s_menu_state.current == NULL) {
        return;
    }
    
    /* TODO: Implement actual menu drawing */
    /* This is a placeholder that should be replaced with actual LCD drawing code */
    s_menu_state.need_redraw = false;
}

void UI_Menu_Update(void)
{
    if (s_menu_state.need_redraw) {
        UI_Menu_Draw();
    }
}

void UI_Menu_Up(void)
{
    if (s_menu_state.current == NULL) {
        return;
    }
    
    if (s_menu_state.current->selected > 0) {
        s_menu_state.current->selected--;
    } else {
        s_menu_state.current->selected = s_menu_state.current->item_count - 1;
    }
    
    s_menu_state.need_redraw = true;
}

void UI_Menu_Down(void)
{
    if (s_menu_state.current == NULL) {
        return;
    }
    
    if (s_menu_state.current->selected < s_menu_state.current->item_count - 1) {
        s_menu_state.current->selected++;
    } else {
        s_menu_state.current->selected = 0;
    }
    
    s_menu_state.need_redraw = true;
}

void UI_Menu_Enter(void)
{
    if (s_menu_state.current == NULL || s_menu_state.current->item_count == 0) {
        return;
    }
    
    UI_MenuItem_t* item = &s_menu_state.current->items[s_menu_state.current->selected];
    
    if (!item->enabled || !item->visible) {
        return;
    }
    
    switch (item->type) {
        case UI_MENU_ITEM_ACTION:
            if (item->data.action.callback) {
                item->data.action.callback(item);
            }
            break;
            
        case UI_MENU_ITEM_SUBMENU:
            if (item->data.submenu.submenu) {
                s_menu_state.current = item->data.submenu.submenu;
                menu_push_stack(s_menu_state.current);
                s_menu_state.current->selected = 0;
                s_menu_state.editing = false;
            }
            break;
            
        case UI_MENU_ITEM_TOGGLE:
            if (item->data.toggle.value) {
                *item->data.toggle.value = !(*item->data.toggle.value);
                if (item->data.toggle.on_change) {
                    item->data.toggle.on_change(item);
                }
            }
            break;
            
        case UI_MENU_ITEM_VALUE:
            s_menu_state.editing = !s_menu_state.editing;
            break;
            
        case UI_MENU_ITEM_SELECT:
            s_menu_state.editing = !s_menu_state.editing;
            break;
            
        case UI_MENU_ITEM_BACK:
            UI_Menu_Back();
            break;
            
        default:
            break;
    }
    
    s_menu_state.need_redraw = true;
}

void UI_Menu_Back(void)
{
    if (s_menu_state.stack_depth <= 1) {
        s_menu_state.editing = false;
        return;
    }
    
    menu_pop_stack();
    s_menu_state.current = menu_peek_stack();
    s_menu_state.editing = false;
    s_menu_state.need_redraw = true;
}

void UI_Menu_GoRoot(void)
{
    if (s_menu_state.stack_depth > 0) {
        s_menu_state.current = s_menu_state.stack[0];
        s_menu_state.stack_depth = 1;
        s_menu_state.editing = false;
        s_menu_state.need_redraw = true;
    }
}

void UI_Menu_GoTo(UI_Menu_t* menu)
{
    if (menu == NULL) {
        return;
    }
    
    s_menu_state.current = menu;
    menu_push_stack(menu);
    s_menu_state.editing = false;
    s_menu_state.need_redraw = true;
}

void UI_Menu_SetVisible(bool visible)
{
    s_menu_state.visible = visible;
    if (visible) {
        s_menu_state.need_redraw = true;
    }
}

bool UI_Menu_IsVisible(void)
{
    return s_menu_state.visible;
}

bool UI_Menu_IsEditing(void)
{
    return s_menu_state.editing;
}

void UI_Menu_RequestRedraw(void)
{
    s_menu_state.need_redraw = true;
}

UI_MenuState_t* UI_Menu_GetState(void)
{
    return &s_menu_state;
}
