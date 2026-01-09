/**
 * @file    comp_menu.h
 * @brief   Menu component - Navigation and selection (New Architecture)
 * @author  BG Card Team
 * @date    2025-01-09
 * 
 * 菜单组件 - 提供多级菜单导航和项目选择
 * 支持：Action、Submenu、Toggle、Value、Select、Back 等菜单项类型
 */

#ifndef __COMP_MENU_H__
#define __COMP_MENU_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Configuration
 *===========================================================================*/

#define UI_MENU_MAX_ITEMS       16      /* Max items per menu */
#define UI_MENU_MAX_NAME_LEN    20      /* Max menu item name length */
#define UI_MENU_MAX_DEPTH       4       /* Max menu depth */

/*===========================================================================
 * Type definitions
 *===========================================================================*/

/* Menu item type */
typedef enum {
    UI_MENU_ITEM_ACTION = 0,    /* Action item - callback on click */
    UI_MENU_ITEM_SUBMENU,       /* Submenu item - enter submenu */
    UI_MENU_ITEM_TOGGLE,        /* Toggle item - switch ON/OFF */
    UI_MENU_ITEM_VALUE,         /* Value item - display/adjust value */
    UI_MENU_ITEM_SELECT,        /* Select item - choose from list */
    UI_MENU_ITEM_BACK,          /* Back item - return to parent menu */
} UI_MenuItemType_t;

/* Forward declarations */
struct UI_Menu;
struct UI_MenuItem;

/* Menu item callback function */
typedef void (*UI_MenuCallback_t)(struct UI_MenuItem* item);

/* Value get/set callback */
typedef int32_t (*UI_MenuValueGet_t)(void);
typedef void (*UI_MenuValueSet_t)(int32_t value);

/* Menu item structure */
typedef struct UI_MenuItem {
    const char* name;               /* Menu item name */
    const uint8_t* icon;            /* Icon data (optional, 8x8) */
    UI_MenuItemType_t type;         /* Menu item type */
    
    union {
        /* ACTION type */
        struct {
            UI_MenuCallback_t callback;
        } action;
        
        /* SUBMENU type */
        struct {
            struct UI_Menu* submenu;
        } submenu;
        
        /* TOGGLE type */
        struct {
            bool* value;            /* Bound bool variable */
            UI_MenuCallback_t on_change;
        } toggle;
        
        /* VALUE type */
        struct {
            int32_t* value;         /* Bound value variable */
            int32_t min;
            int32_t max;
            int32_t step;
            const char* unit;       /* Unit string (e.g. "%" "dB") */
            UI_MenuCallback_t on_change;
        } value;
        
        /* SELECT type */
        struct {
            uint8_t* index;         /* Current selection index */
            const char** options;   /* Option string array */
            uint8_t option_count;
            UI_MenuCallback_t on_change;
        } select;
    } data;
    
    bool enabled;                   /* Is enabled */
    bool visible;                   /* Is visible */
    uint32_t user_data;             /* User data */
} UI_MenuItem_t;

/* Menu structure */
typedef struct UI_Menu {
    const char* title;              /* Menu title */
    UI_MenuItem_t* items;           /* Menu item array */
    uint8_t item_count;             /* Number of menu items */
    uint8_t selected;               /* Current selected index */
    uint8_t scroll_offset;          /* Scroll offset */
    struct UI_Menu* parent;         /* Parent menu */
} UI_Menu_t;

/* Menu system state */
typedef struct {
    UI_Menu_t* current;             /* Current menu */
    UI_Menu_t* stack[UI_MENU_MAX_DEPTH];  /* Menu stack */
    uint8_t stack_depth;            /* Stack depth */
    bool editing;                   /* In edit mode (adjusting value) */
    bool need_redraw;               /* Need redraw */
    bool visible;                   /* Is visible */
} UI_MenuState_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize menu component
 */
void UI_Menu_Init(void);

/**
 * @brief Set root menu
 * @param menu Root menu pointer
 */
void UI_Menu_SetRoot(UI_Menu_t* menu);

/**
 * @brief Get current menu
 * @return Current menu pointer
 */
UI_Menu_t* UI_Menu_GetCurrent(void);

/**
 * @brief Draw current menu
 */
void UI_Menu_Draw(void);

/**
 * @brief Update menu (handle redraw if needed)
 */
void UI_Menu_Update(void);

/**
 * @brief Navigate to previous item
 */
void UI_Menu_Up(void);

/**
 * @brief Navigate to next item
 */
void UI_Menu_Down(void);

/**
 * @brief Confirm/Enter
 */
void UI_Menu_Enter(void);

/**
 * @brief Back
 */
void UI_Menu_Back(void);

/**
 * @brief Go to root menu
 */
void UI_Menu_GoRoot(void);

/**
 * @brief Enter specified menu
 * @param menu Target menu
 */
void UI_Menu_GoTo(UI_Menu_t* menu);

/**
 * @brief Set menu visibility
 * @param visible true to show
 */
void UI_Menu_SetVisible(bool visible);

/**
 * @brief Check if menu is visible
 * @return true if visible
 */
bool UI_Menu_IsVisible(void);

/**
 * @brief Check if in edit mode
 * @return true if editing
 */
bool UI_Menu_IsEditing(void);

/**
 * @brief Request redraw
 */
void UI_Menu_RequestRedraw(void);

/**
 * @brief Get menu state
 * @return State pointer
 */
UI_MenuState_t* UI_Menu_GetState(void);

/**
 * @brief Initialize default menu
 */
void UI_Menu_InitDefault(void);

/*===========================================================================
 * Convenience macros - for static menu definition
 *===========================================================================*/

/* Define action menu item */
#define UI_MENU_ACTION(n, cb) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_ACTION, \
      .data.action.callback = (cb), .enabled = true, .visible = true }

/* Define submenu item */
#define UI_MENU_SUBMENU(n, sub) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_SUBMENU, \
      .data.submenu.submenu = (sub), .enabled = true, .visible = true }

/* Define toggle item */
#define UI_MENU_TOGGLE(n, val, cb) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_TOGGLE, \
      .data.toggle.value = (val), .data.toggle.on_change = (cb), \
      .enabled = true, .visible = true }

/* Define value item */
#define UI_MENU_VALUE(n, val, mi, ma, st, un, cb) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_VALUE, \
      .data.value.value = (val), .data.value.min = (mi), .data.value.max = (ma), \
      .data.value.step = (st), .data.value.unit = (un), .data.value.on_change = (cb), \
      .enabled = true, .visible = true }

/* Define select item */
#define UI_MENU_SELECT(n, idx, opts, cnt, cb) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_SELECT, \
      .data.select.index = (idx), .data.select.options = (opts), \
      .data.select.option_count = (cnt), .data.select.on_change = (cb), \
      .enabled = true, .visible = true }

/* Define back item */
#define UI_MENU_BACK_ITEM(n) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_BACK, \
      .enabled = true, .visible = true }

/* Define menu */
#define UI_MENU_DEF(name, title, items_array) \
    UI_Menu_t name = { \
        .title = (title), \
        .items = (items_array), \
        .item_count = sizeof(items_array) / sizeof(items_array[0]), \
        .selected = 0, \
        .scroll_offset = 0, \
        .parent = NULL \
    }

#ifdef __cplusplus
}
#endif

#endif /* __COMP_MENU_H__ */
