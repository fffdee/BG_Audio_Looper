/**
 * @file    ui_menu.c
 * @brief   Menu system module implementation
 * @author  BG Card Team
 * @date    2025-12-18
 */

#include "ui_menu.h"
#include "ui_config.h"
#include "ui_statusbar.h"
#include "bg_lcd.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Private variables
 *===========================================================================*/

static UI_MenuState_t menu_state;
static UI_Menu_t* root_menu = NULL;

/* Number of visible menu items */
#define VISIBLE_ITEMS   ((UI_MENU_HEIGHT - 18) / UI_MENU_ITEM_HEIGHT)

/*===========================================================================
 * Private functions
 *===========================================================================*/

/**
 * @brief Draw a single menu item
 */
static void draw_menu_item(uint8_t index, uint16_t y, bool selected)
{
    UI_Menu_t* menu = menu_state.current;
    if (!menu || index >= menu->item_count) return;
    
    UI_MenuItem_t* item = &menu->items[index];
    if (!item->visible) return;
    
    uint16_t bg_color = selected ? UI_MENU_SEL_BG_COLOR : UI_MENU_BG_COLOR;
    uint16_t fg_color = item->enabled ? 
                        (selected ? UI_MENU_SEL_FG_COLOR : UI_MENU_FG_COLOR) :
                        UI_COLOR_GRAY;
    
    /* Draw background */
    BG_lcd.Box(0, y, UI_SCREEN_WIDTH, UI_MENU_ITEM_HEIGHT, bg_color);
    
    /* Draw icon (if any) */
    uint16_t text_x = 4;
    if (item->icon) {
        /* Simple draw 8x8 icon */
        uint8_t i, j;
        for (i = 0; i < 8; i++) {
            uint8_t row = item->icon[i];
            for (j = 0; j < 8; j++) {
                if (row & (0x80 >> j)) {
                    BG_lcd.DrawPoint(4 + j, y + 6 + i, UI_MENU_ICON_COLOR);
                }
            }
        }
        text_x = 16;
    }
    
    /* Draw menu item name */
    const char* name = item->name;
    while (*name) {
        BG_lcd.ShowChar(text_x, y + 2, *name, fg_color);
        text_x += 8;
        name++;
    }
    
    /* Draw value/status */
    char value_str[16] = "";
    uint16_t value_x = UI_SCREEN_WIDTH - 8;
    
    switch (item->type) {
        case UI_MENU_ITEM_TOGGLE:
            if (item->data.toggle.value) {
                strcpy(value_str, *item->data.toggle.value ? "ON" : "OFF");
            }
            break;
            
        case UI_MENU_ITEM_VALUE:
            if (item->data.value.value) {
                if (item->data.value.unit) {
                    snprintf(value_str, sizeof(value_str), "%d%s", 
                             (int)*item->data.value.value, item->data.value.unit);
                } else {
                    snprintf(value_str, sizeof(value_str), "%d", 
                             (int)*item->data.value.value);
                }
            }
            break;
            
        case UI_MENU_ITEM_SELECT:
            if (item->data.select.index && item->data.select.options) {
                uint8_t idx = *item->data.select.index;
                if (idx < item->data.select.option_count) {
                    strncpy(value_str, item->data.select.options[idx], sizeof(value_str) - 1);
                }
            }
            break;
            
        case UI_MENU_ITEM_SUBMENU:
            strcpy(value_str, ">");
            break;
            
        case UI_MENU_ITEM_BACK:
            strcpy(value_str, "<");
            break;
            
        default:
            break;
    }
    
    /* Right-align draw value */
    if (value_str[0]) {
        int len = strlen(value_str);
        value_x = UI_SCREEN_WIDTH - 4 - (len * 8);
        char* p = value_str;
        while (*p) {
            BG_lcd.ShowChar(value_x, y + 2, *p, fg_color);
            value_x += 8;
            p++;
        }
    }
    
    /* Edit mode indicator */
    if (selected && menu_state.editing) {
        /* Draw edit indicator [ ] */
        BG_lcd.ShowChar(UI_SCREEN_WIDTH - 4 - strlen(value_str) * 8 - 10, y + 2, '[', UI_COLOR_YELLOW);
        BG_lcd.ShowChar(UI_SCREEN_WIDTH - 4, y + 2, ']', UI_COLOR_YELLOW);
    }
}

/**
 * @brief Draw menu title
 */
static void draw_menu_title(void)
{
    if (!menu_state.current) return;
    
    uint16_t y = UI_StatusBar_GetHeight();
    
    /* Draw title background */
    BG_lcd.Box(0, y, UI_SCREEN_WIDTH, 18, UI_COLOR_DARK_GRAY);
    
    /* Draw title text */
    const char* title = menu_state.current->title;
    uint16_t x = 4;
    while (*title) {
        BG_lcd.ShowChar(x, y + 1, *title, UI_COLOR_WHITE);
        x += 8;
        title++;
    }
    
    /* If there is a parent menu, show return indicator */
    if (menu_state.current->parent) {
        BG_lcd.ShowChar(UI_SCREEN_WIDTH - 12, y + 1, '<', UI_COLOR_CYAN);
    }
}

/**
 * @brief Draw scrollbar
 */
static void draw_scrollbar(void)
{
    UI_Menu_t* menu = menu_state.current;
    if (!menu || menu->item_count <= VISIBLE_ITEMS) return;
    
    uint16_t bar_y = UI_StatusBar_GetHeight() + 18;
    uint16_t bar_height = UI_MENU_HEIGHT - 18;
    
    /* Calculate scrollbar position and size */
    uint16_t thumb_height = (bar_height * VISIBLE_ITEMS) / menu->item_count;
    if (thumb_height < 4) thumb_height = 4;
    
    uint16_t thumb_y = bar_y + (bar_height - thumb_height) * menu->scroll_offset / 
                       (menu->item_count - VISIBLE_ITEMS);
    
    /* Draw scrollbar background */
    BG_lcd.Box(UI_SCREEN_WIDTH - 3, bar_y, 3, bar_height, UI_COLOR_DARK_GRAY);
    
    /* Draw scrollbar thumb */
    BG_lcd.Box(UI_SCREEN_WIDTH - 3, thumb_y, 3, thumb_height, UI_COLOR_LIGHT_GRAY);
}

/**
 * @brief Ensure selected item is visible
 */
static void ensure_selection_visible(void)
{
    UI_Menu_t* menu = menu_state.current;
    if (!menu) return;
    
    /* Scroll up */
    if (menu->selected < menu->scroll_offset) {
        menu->scroll_offset = menu->selected;
    }
    
    /* Scroll down */
    if (menu->selected >= menu->scroll_offset + VISIBLE_ITEMS) {
        menu->scroll_offset = menu->selected - VISIBLE_ITEMS + 1;
    }
}

/**
 * @brief Find the next visible and available item
 */
static int8_t find_next_item(int8_t from, int8_t dir)
{
    UI_Menu_t* menu = menu_state.current;
    if (!menu) return -1;
    
    int8_t i = from + dir;
    while (i >= 0 && i < menu->item_count) {
        if (menu->items[i].visible && menu->items[i].enabled) {
            return i;
        }
        i += dir;
    }
    return -1;
}

/*===========================================================================
 * API Implementation
 *===========================================================================*/

void UI_Menu_Init(void)
{
    memset(&menu_state, 0, sizeof(menu_state));
    menu_state.visible = false;
    menu_state.need_redraw = true;
    root_menu = NULL;
}

void UI_Menu_SetRoot(UI_Menu_t* menu)
{
    root_menu = menu;
    menu_state.current = menu;
    menu_state.stack_depth = 0;
    if (menu) {
        menu->parent = NULL;
        menu->selected = 0;
        menu->scroll_offset = 0;
    }
    menu_state.need_redraw = true;
}

UI_Menu_t* UI_Menu_GetCurrent(void)
{
    return menu_state.current;
}

void UI_Menu_Draw(void)
{
    if (!menu_state.visible || !menu_state.current) return;
    
    UI_Menu_t* menu = menu_state.current;
    uint16_t y = UI_StatusBar_GetHeight();
    
    /* Clear menu area */
    BG_lcd.Box(0, y, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT - y, UI_MENU_BG_COLOR);
    
    /* Draw title */
    draw_menu_title();
    
    /* Draw menu items */
    y += 18;  /* Title height */
    uint8_t i;
    for (i = 0; i < VISIBLE_ITEMS && (menu->scroll_offset + i) < menu->item_count; i++) {
        uint8_t item_idx = menu->scroll_offset + i;
        bool selected = (item_idx == menu->selected);
        draw_menu_item(item_idx, y, selected);
        y += UI_MENU_ITEM_HEIGHT;
    }
    
    /* Draw scrollbar */
    draw_scrollbar();
    
    menu_state.need_redraw = false;
}

void UI_Menu_Update(void)
{
    if (menu_state.need_redraw) {
        UI_Menu_Draw();
    }
}

void UI_Menu_Up(void)
{
    if (!menu_state.current) return;
    
    UI_Menu_t* menu = menu_state.current;
    
    if (menu_state.editing) {
        /* Edit mode: Increase value */
        UI_MenuItem_t* item = &menu->items[menu->selected];
        if (item->type == UI_MENU_ITEM_VALUE && item->data.value.value) {
            int32_t val = *item->data.value.value + item->data.value.step;
            if (val <= item->data.value.max) {
                *item->data.value.value = val;
                if (item->data.value.on_change) {
                    item->data.value.on_change(item);
                }
            }
        } else if (item->type == UI_MENU_ITEM_SELECT && item->data.select.index) {
            uint8_t idx = *item->data.select.index;
            if (idx + 1 < item->data.select.option_count) {
                *item->data.select.index = idx + 1;
                if (item->data.select.on_change) {
                    item->data.select.on_change(item);
                }
            }
        }
    } else {
        /* Navigation mode: Select up */
        int8_t next = find_next_item(menu->selected, -1);
        if (next >= 0) {
            menu->selected = next;
            ensure_selection_visible();
        }
    }
    
    menu_state.need_redraw = true;
}

void UI_Menu_Down(void)
{
    if (!menu_state.current) return;
    
    UI_Menu_t* menu = menu_state.current;
    
    if (menu_state.editing) {
        /* Edit mode: Decrease value */
        UI_MenuItem_t* item = &menu->items[menu->selected];
        if (item->type == UI_MENU_ITEM_VALUE && item->data.value.value) {
            int32_t val = *item->data.value.value - item->data.value.step;
            if (val >= item->data.value.min) {
                *item->data.value.value = val;
                if (item->data.value.on_change) {
                    item->data.value.on_change(item);
                }
            }
        } else if (item->type == UI_MENU_ITEM_SELECT && item->data.select.index) {
            uint8_t idx = *item->data.select.index;
            if (idx > 0) {
                *item->data.select.index = idx - 1;
                if (item->data.select.on_change) {
                    item->data.select.on_change(item);
                }
            }
        }
    } else {
        /* Navigation mode: Select down */
        int8_t next = find_next_item(menu->selected, 1);
        if (next >= 0) {
            menu->selected = next;
            ensure_selection_visible();
        }
    }
    
    menu_state.need_redraw = true;
}

void UI_Menu_Enter(void)
{
    if (!menu_state.current) return;
    
    UI_Menu_t* menu = menu_state.current;
    UI_MenuItem_t* item = &menu->items[menu->selected];
    
    if (!item->enabled) return;
    
    switch (item->type) {
        case UI_MENU_ITEM_ACTION:
            if (item->data.action.callback) {
                item->data.action.callback(item);
            }
            break;
            
        case UI_MENU_ITEM_SUBMENU:
            if (item->data.submenu.submenu) {
                /* Push current menu to stack */
                if (menu_state.stack_depth < UI_MENU_MAX_DEPTH) {
                    menu_state.stack[menu_state.stack_depth++] = menu;
                }
                
                /* Enter submenu */
                UI_Menu_t* submenu = item->data.submenu.submenu;
                submenu->parent = menu;
                submenu->selected = 0;
                submenu->scroll_offset = 0;
                menu_state.current = submenu;
            }
            break;
            
        case UI_MENU_ITEM_TOGGLE:
            if (item->data.toggle.value) {
                *item->data.toggle.value = !*item->data.toggle.value;
                if (item->data.toggle.on_change) {
                    item->data.toggle.on_change(item);
                }
            }
            break;
            
        case UI_MENU_ITEM_VALUE:
        case UI_MENU_ITEM_SELECT:
            /* Enter/Exit edit mode */
            menu_state.editing = !menu_state.editing;
            break;
            
        case UI_MENU_ITEM_BACK:
            UI_Menu_Back();
            return;
            
        default:
            break;
    }
    
    menu_state.need_redraw = true;
}

void UI_Menu_Back(void)
{
    if (menu_state.editing) {
        /* Exit edit mode */
        menu_state.editing = false;
        menu_state.need_redraw = true;
        return;
    }
    
    if (!menu_state.current) return;
    
    /* Return to upper menu */
    if (menu_state.stack_depth > 0) {
        menu_state.current = menu_state.stack[--menu_state.stack_depth];
    } else if (menu_state.current->parent) {
        menu_state.current = menu_state.current->parent;
    }
    
    menu_state.need_redraw = true;
}

void UI_Menu_GoRoot(void)
{
    menu_state.editing = false;
    menu_state.stack_depth = 0;
    menu_state.current = root_menu;
    if (root_menu) {
        root_menu->selected = 0;
        root_menu->scroll_offset = 0;
    }
    menu_state.need_redraw = true;
}

void UI_Menu_GoTo(UI_Menu_t* menu)
{
    if (!menu) return;
    
    menu_state.editing = false;
    menu_state.current = menu;
    menu->selected = 0;
    menu->scroll_offset = 0;
    menu_state.need_redraw = true;
}

void UI_Menu_SetVisible(bool visible)
{
    if (menu_state.visible != visible) {
        menu_state.visible = visible;
        menu_state.need_redraw = true;
    }
}

bool UI_Menu_IsVisible(void)
{
    return menu_state.visible;
}

bool UI_Menu_IsEditing(void)
{
    return menu_state.editing;
}

void UI_Menu_RequestRedraw(void)
{
    menu_state.need_redraw = true;
}

UI_MenuState_t* UI_Menu_GetState(void)
{
    return &menu_state;
}
