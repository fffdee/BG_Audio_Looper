/**
 * @file    bg_ui.h
 * @brief   BanGUI - Unified UI System Object
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 这是 BanGUI 系统的统一入口，将 UI 系统抽象为一个对象 (BG_UI)
 * 
 * 使用方法:
 *   BG_UI.Init();           // 初始化
 *   BG_UI.Start();          // 启动
 *   BG_UI.Update(20);       // 主循环更新
 *   BG_UI.SetState(state);  // 切换状态
 * 
 * 文件结构:
 *   ui/
 *   ├── core/           核心层
 *   │   ├── bg_ui.h/.c      UI 主对象
 *   │   ├── ui_types.h      类型定义
 *   │   ├── ui_config.h     配置常量
 *   │   └── ui_input.h/.c   输入处理
 *   ├── components/     组件层
 *   │   ├── ui_statusbar.h/.c   状态栏
 *   │   ├── ui_menu.h/.c        菜单
 *   │   └── ui_popup.h/.c       弹窗
 *   ├── views/          视图层
 *   │   ├── view_home.h/.c      主界面
 *   │   ├── view_menu.h/.c      菜单界面
 *   │   └── view_looper.h/.c    Looper界面
 *   └── resources/      资源层
 *       ├── ui_icons.h/.c       图标资源
 *       └── ui_fonts.h          字体资源
 */

#ifndef __BG_UI_H__
#define __BG_UI_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 类型定义
 *===========================================================================*/

/**
 * @brief UI 状态枚举
 */
typedef enum {
    UI_STATE_BOOT = 0,      /* 启动画面 */
    UI_STATE_IDLE,          /* 主界面 */
    UI_STATE_MENU,          /* 菜单界面 */
    UI_STATE_POPUP,         /* 弹窗 */
    UI_STATE_LOOPER,        /* Looper 控制界面 */
    UI_STATE_SETTINGS,      /* 设置界面 */
    UI_STATE_COUNT
} UI_State_t;

/**
 * @brief 按钮 ID
 */
typedef enum {
    UI_BTN_UP = 0,          /* 上 - GPIO_A0 */
    UI_BTN_DOWN,            /* 下 - GPIO_B5 */
    UI_BTN_ENTER,           /* 确认 - GPIO_A15 */
    UI_BTN_BACK,            /* 返回 - GPIO_A16 */
    UI_BTN_COUNT
} UI_BtnID_t;

/**
 * @brief 按钮事件类型
 */
typedef enum {
    UI_BTN_EVT_NONE = 0,
    UI_BTN_EVT_CLICK,       /* 单击 */
    UI_BTN_EVT_LONG_PRESS,  /* 长按 */
    UI_BTN_EVT_REPEAT,      /* 重复 */
} UI_BtnEvent_t;

/**
 * @brief 按钮事件数据
 */
typedef struct {
    UI_BtnID_t id;
    UI_BtnEvent_t event;
    uint16_t duration;
} UI_BtnEventData_t;

/**
 * @brief View 接口
 */
typedef struct UI_View {
    const char* name;
    void (*on_enter)(void);
    void (*on_exit)(void);
    void (*on_update)(uint16_t delta_ms);
    void (*on_draw)(void);
    bool (*on_button)(UI_BtnEventData_t* event);
    bool visible;
    bool dirty;
} UI_View_t;

/**
 * @brief 状态变化回调
 */
typedef void (*UI_StateCallback_t)(UI_State_t from, UI_State_t to);

/*===========================================================================
 * UI 对象接口
 *===========================================================================*/

/**
 * @brief BG_UI 对象结构体
 */
typedef struct {
    /*--- 生命周期 ---*/
    void (*Init)(void);
    void (*Start)(void);
    void (*Update)(uint16_t delta_ms);
    
    /*--- 状态管理 ---*/
    void (*SetState)(UI_State_t state);
    UI_State_t (*GetState)(void);
    UI_State_t (*GetPrevState)(void);
    bool (*IsReady)(void);
    
    /*--- View 管理 ---*/
    void (*RegisterView)(UI_State_t state, UI_View_t* view);
    void (*UnregisterView)(UI_View_t* view);
    
    /*--- 按钮处理 ---*/
    void (*HandleButton)(UI_BtnEventData_t* event);
    void (*ScanButtons)(uint16_t delta_ms);
    bool (*IsButtonPressed)(UI_BtnID_t id);
    
    /*--- 弹窗 ---*/
    void (*ShowPopup)(const char* title, const char* msg, uint16_t duration_ms);
    void (*ClosePopup)(void);
    bool (*IsPopupActive)(void);
    
    /*--- 刷新 ---*/
    void (*Invalidate)(void);
    void (*InvalidateView)(UI_View_t* view);
    
    /*--- 回调注册 ---*/
    void (*OnStateChange)(UI_StateCallback_t callback);
    
    /*--- 状态栏 ---*/
    void (*StatusBar_SetBT)(uint8_t status);
    void (*StatusBar_SetVolume)(uint8_t volume);
    void (*StatusBar_SetBattery)(uint8_t level);
    void (*StatusBar_Update)(void);
    
    /*--- 调试 ---*/
    const char* (*GetStateName)(UI_State_t state);
    void (*SetDebug)(bool enable);
    
} BG_UI_t;

/*===========================================================================
 * 全局对象
 *===========================================================================*/

extern const BG_UI_t BG_UI;

/*===========================================================================
 * 便捷宏
 *===========================================================================*/

#define UI_SCREEN_WIDTH     160
#define UI_SCREEN_HEIGHT    128

/* 颜色定义 */
#define UI_BLACK            0x0000
#define UI_WHITE            0xFFFF
#define UI_RED              0xF800
#define UI_GREEN            0x07E0
#define UI_BLUE             0x001F
#define UI_YELLOW           0xFFE0
#define UI_CYAN             0x07FF
#define UI_GRAY             0x8410
#define UI_DARK_GRAY        0x2104

#ifdef __cplusplus
}
#endif

#endif /* __BG_UI_H__ */
