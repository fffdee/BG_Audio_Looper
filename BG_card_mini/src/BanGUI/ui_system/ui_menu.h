/**
 * @file    ui_menu.h
 * @brief   菜单系统模块
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * 功能:
 *   - 多级菜单支持
 *   - 图标和文字菜单项
 *   - 滚动显示
 *   - 4按键导航 (上/下/确认/返回)
 *   - 菜单项回调
 */

#ifndef __UI_MENU_H__
#define __UI_MENU_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 配置
 *===========================================================================*/

#define UI_MENU_MAX_ITEMS       16      /* 单个菜单最大项数 */
#define UI_MENU_MAX_NAME_LEN    20      /* 菜单项名称最大长度 */
#define UI_MENU_MAX_DEPTH       4       /* 最大菜单深度 */

/*===========================================================================
 * 类型定义
 *===========================================================================*/

/* 菜单项类型 */
typedef enum {
    UI_MENU_ITEM_ACTION = 0,    /* 动作项 - 点击执行回调 */
    UI_MENU_ITEM_SUBMENU,       /* 子菜单项 - 进入子菜单 */
    UI_MENU_ITEM_TOGGLE,        /* 开关项 - 切换ON/OFF */
    UI_MENU_ITEM_VALUE,         /* 数值项 - 显示/调整数值 */
    UI_MENU_ITEM_SELECT,        /* 选择项 - 从列表中选择 */
    UI_MENU_ITEM_BACK,          /* 返回项 - 返回上级菜单 */
} UI_MenuItemType_t;

/* 前向声明 */
struct UI_Menu;
struct UI_MenuItem;

/* 菜单项回调函数 */
typedef void (*UI_MenuCallback_t)(struct UI_MenuItem* item);

/* 数值获取/设置回调 */
typedef int32_t (*UI_MenuValueGet_t)(void);
typedef void (*UI_MenuValueSet_t)(int32_t value);

/* 菜单项结构 */
typedef struct UI_MenuItem {
    const char* name;               /* 菜单项名称 */
    const uint8_t* icon;            /* 图标数据 (可选, 8x8) */
    UI_MenuItemType_t type;         /* 菜单项类型 */
    
    union {
        /* ACTION 类型 */
        struct {
            UI_MenuCallback_t callback;
        } action;
        
        /* SUBMENU 类型 */
        struct {
            struct UI_Menu* submenu;
        } submenu;
        
        /* TOGGLE 类型 */
        struct {
            bool* value;            /* 绑定的bool变量 */
            UI_MenuCallback_t on_change;
        } toggle;
        
        /* VALUE 类型 */
        struct {
            int32_t* value;         /* 绑定的数值变量 */
            int32_t min;
            int32_t max;
            int32_t step;
            const char* unit;       /* 单位字符串 (如 "%" "dB") */
            UI_MenuCallback_t on_change;
        } value;
        
        /* SELECT 类型 */
        struct {
            uint8_t* index;         /* 当前选择索引 */
            const char** options;   /* 选项字符串数组 */
            uint8_t option_count;
            UI_MenuCallback_t on_change;
        } select;
    } data;
    
    bool enabled;                   /* 是否可用 */
    bool visible;                   /* 是否可见 */
    uint32_t user_data;             /* 用户数据 */
} UI_MenuItem_t;

/* 菜单结构 */
typedef struct UI_Menu {
    const char* title;              /* 菜单标题 */
    UI_MenuItem_t* items;           /* 菜单项数组 */
    uint8_t item_count;             /* 菜单项数量 */
    uint8_t selected;               /* 当前选中索引 */
    uint8_t scroll_offset;          /* 滚动偏移 */
    struct UI_Menu* parent;         /* 父菜单 */
} UI_Menu_t;

/* 菜单系统状态 */
typedef struct {
    UI_Menu_t* current;             /* 当前菜单 */
    UI_Menu_t* stack[UI_MENU_MAX_DEPTH];  /* 菜单栈 */
    uint8_t stack_depth;            /* 栈深度 */
    bool editing;                   /* 是否在编辑模式(调整数值) */
    bool need_redraw;               /* 需要重绘 */
    bool visible;                   /* 是否可见 */
} UI_MenuState_t;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化菜单系统
 */
void UI_Menu_Init(void);

/**
 * @brief 设置根菜单
 * @param menu 根菜单指针
 */
void UI_Menu_SetRoot(UI_Menu_t* menu);

/**
 * @brief 获取当前菜单
 * @return 当前菜单指针
 */
UI_Menu_t* UI_Menu_GetCurrent(void);

/**
 * @brief 绘制当前菜单
 */
void UI_Menu_Draw(void);

/**
 * @brief 更新菜单 (处理需要重绘的情况)
 */
void UI_Menu_Update(void);

/**
 * @brief 导航到上一项
 */
void UI_Menu_Up(void);

/**
 * @brief 导航到下一项
 */
void UI_Menu_Down(void);

/**
 * @brief 确认/进入
 */
void UI_Menu_Enter(void);

/**
 * @brief 返回
 */
void UI_Menu_Back(void);

/**
 * @brief 返回到根菜单
 */
void UI_Menu_GoRoot(void);

/**
 * @brief 进入指定菜单
 * @param menu 目标菜单
 */
void UI_Menu_GoTo(UI_Menu_t* menu);

/**
 * @brief 设置菜单可见性
 * @param visible true显示
 */
void UI_Menu_SetVisible(bool visible);

/**
 * @brief 检查菜单是否可见
 * @return true可见
 */
bool UI_Menu_IsVisible(void);

/**
 * @brief 检查是否在编辑模式
 * @return true编辑中
 */
bool UI_Menu_IsEditing(void);

/**
 * @brief 请求重绘
 */
void UI_Menu_RequestRedraw(void);

/**
 * @brief 获取菜单状态
 * @return 状态指针
 */
UI_MenuState_t* UI_Menu_GetState(void);

/**
 * @brief 初始化默认菜单 (在ui_menu_def.c中实现)
 */
void UI_Menu_InitDefault(void);

/**
 * @brief 获取默认主菜单
 * @return 默认主菜单指针
 */
UI_Menu_t* UI_GetDefaultMainMenu(void);

/*===========================================================================
 * 便捷宏 - 用于静态定义菜单
 *===========================================================================*/

/* 定义动作菜单项 */
#define UI_MENU_ACTION(n, cb) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_ACTION, \
      .data.action.callback = (cb), .enabled = true, .visible = true }

/* 定义子菜单项 */
#define UI_MENU_SUBMENU(n, sub) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_SUBMENU, \
      .data.submenu.submenu = (sub), .enabled = true, .visible = true }

/* 定义开关项 */
#define UI_MENU_TOGGLE(n, val, cb) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_TOGGLE, \
      .data.toggle.value = (val), .data.toggle.on_change = (cb), \
      .enabled = true, .visible = true }

/* 定义数值项 */
#define UI_MENU_VALUE(n, val, mi, ma, st, un, cb) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_VALUE, \
      .data.value.value = (val), .data.value.min = (mi), .data.value.max = (ma), \
      .data.value.step = (st), .data.value.unit = (un), .data.value.on_change = (cb), \
      .enabled = true, .visible = true }

/* 定义选择项 */
#define UI_MENU_SELECT(n, idx, opts, cnt, cb) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_SELECT, \
      .data.select.index = (idx), .data.select.options = (opts), \
      .data.select.option_count = (cnt), .data.select.on_change = (cb), \
      .enabled = true, .visible = true }

/* 定义返回项 */
#define UI_MENU_BACK_ITEM(n) \
    { .name = (n), .icon = NULL, .type = UI_MENU_ITEM_BACK, \
      .enabled = true, .visible = true }

/* 定义菜单 */
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

#endif /* __UI_MENU_H__ */
