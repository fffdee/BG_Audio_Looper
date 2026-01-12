/**
 * @file    ui_page.h
 * @brief   UI Page Management System (Core Layer)
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 页面管理系统 - 管理应用页面的导航和状态
 * 
 * 功能:
 *   - 页面注册和切换
 *   - 导航堆栈 (支持返回)
 *   - 页面生命周期回调
 *   - 与 BG_UI 状态系统协同工作
 * 
 * 与旧 BG_Page 的区别:
 *   - 更简洁的 API
 *   - 与 UI 状态机集成
 *   - 支持页面参数传递
 */

#ifndef __UI_PAGE_H__
#define __UI_PAGE_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 配置常量
 *===========================================================================*/

#define UI_PAGE_MAX_COUNT       16      /* 最大页面数量 */
#define UI_PAGE_STACK_DEPTH     8       /* 导航堆栈深度 */
#define UI_PAGE_NAME_MAX        16      /* 页面名称最大长度 */

/*===========================================================================
 * 页面 ID 定义
 *===========================================================================*/

typedef uint8_t UI_PageID_t;

#define UI_PAGE_INVALID         0xFF    /* 无效页面 ID */

/*===========================================================================
 * 页面回调函数类型
 *===========================================================================*/

/**
 * @brief 页面初始化回调
 * @param param 传入参数
 */
typedef void (*UI_PageInitFunc_t)(void* param);

/**
 * @brief 页面退出回调
 */
typedef void (*UI_PageExitFunc_t)(void);

/**
 * @brief 页面更新回调 (每帧调用)
 */
typedef void (*UI_PageUpdateFunc_t)(void);

/**
 * @brief 页面绘制回调
 */
typedef void (*UI_PageDrawFunc_t)(void);

/**
 * @brief 页面按键事件回调
 * @param key_id 按键 ID (0=UP, 1=DOWN, 2=ENTER, 3=BACK)
 * @param event 事件类型 (1=CLICK, 2=LONG_PRESS, 3=REPEAT)
 * @return true 如果事件被消费
 */
typedef bool (*UI_PageKeyFunc_t)(uint8_t key_id, uint8_t event);

/*===========================================================================
 * 页面定义结构
 *===========================================================================*/

/**
 * @brief 页面定义
 */
typedef struct {
    const char* name;               /* 页面名称 */
    UI_PageID_t id;                 /* 页面 ID */
    
    /* 导航链接 */
    UI_PageID_t nav_up;             /* 按上键跳转的页面 */
    UI_PageID_t nav_down;           /* 按下键跳转的页面 */
    UI_PageID_t nav_enter;          /* 按确认键跳转的页面 */
    UI_PageID_t nav_back;           /* 按返回键跳转的页面 */
    
    /* 生命周期回调 */
    UI_PageInitFunc_t on_init;      /* 初始化 (进入页面时) */
    UI_PageExitFunc_t on_exit;      /* 退出 (离开页面时) */
    UI_PageUpdateFunc_t on_update;  /* 更新 (每帧) */
    UI_PageDrawFunc_t on_draw;      /* 绘制 */
    UI_PageKeyFunc_t on_key;        /* 按键事件 */
    
    /* 状态 */
    bool needs_redraw;              /* 需要重绘标志 */
    void* user_data;                /* 用户数据 */
} UI_Page_t;

/*===========================================================================
 * 页面管理器对象
 *===========================================================================*/

/**
 * @brief 页面管理器对象结构
 */
typedef struct {
    /*--- 初始化和管理 ---*/
    void (*Init)(void);
    void (*Deinit)(void);
    
    /*--- 页面注册 ---*/
    bool (*Register)(UI_Page_t* page);
    bool (*Unregister)(UI_PageID_t id);
    UI_Page_t* (*GetPage)(UI_PageID_t id);
    
    /*--- 导航 ---*/
    void (*GotoPage)(UI_PageID_t id, void* param);
    void (*Back)(void);
    void (*Home)(void);
    
    /*--- 按键处理 ---*/
    void (*HandleKey)(uint8_t key_id, uint8_t event);
    void (*NavUp)(void);
    void (*NavDown)(void);
    void (*NavEnter)(void);
    void (*NavBack)(void);
    
    /*--- 更新和绘制 ---*/
    void (*Update)(void);
    void (*Draw)(void);
    void (*Invalidate)(void);
    
    /*--- 状态查询 ---*/
    UI_PageID_t (*GetCurrentID)(void);
    UI_Page_t* (*GetCurrent)(void);
    const char* (*GetCurrentName)(void);
    uint8_t (*GetStackDepth)(void);
    
} UI_PageMgr_t;

/*===========================================================================
 * 全局对象
 *===========================================================================*/

extern const UI_PageMgr_t UI_PageMgr;

/*===========================================================================
 * 便捷宏
 *===========================================================================*/

/**
 * @brief 定义页面 (静态定义)
 */
#define UI_PAGE_DEFINE(page_name, page_id) \
    static UI_Page_t page_name = { \
        .name = #page_name, \
        .id = page_id, \
        .nav_up = page_id, \
        .nav_down = page_id, \
        .nav_enter = page_id, \
        .nav_back = page_id, \
    }

/**
 * @brief 设置页面导航
 */
#define UI_PAGE_SET_NAV(page, up, down, enter, back) do { \
    (page)->nav_up = (up); \
    (page)->nav_down = (down); \
    (page)->nav_enter = (enter); \
    (page)->nav_back = (back); \
} while(0)

/**
 * @brief 设置页面回调
 */
#define UI_PAGE_SET_CALLBACKS(page, init_fn, exit_fn, update_fn, draw_fn, key_fn) do { \
    (page)->on_init = (init_fn); \
    (page)->on_exit = (exit_fn); \
    (page)->on_update = (update_fn); \
    (page)->on_draw = (draw_fn); \
    (page)->on_key = (key_fn); \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* __UI_PAGE_H__ */
