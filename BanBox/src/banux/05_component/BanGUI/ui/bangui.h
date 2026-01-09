/**
 * @file    bangui.h
 * @brief   BanGUI - Unified UI System Main Header
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * ===== BanGUI 统一 UI 系统 =====
 * 
 * 这是 BanGUI 系统的唯一入口头文件。
 * 只需包含此文件即可使用完整的 UI 系统。
 * 
 * 使用示例:
 * 
 *   #include "bangui.h"
 *   
 *   void app_init() {
 *       // 初始化 UI 系统
 *       BG_UI.Init();
 *       
 *       // 创建视图
 *       View_Home_Create();
 *       View_Menu_Create();
 *       View_Looper_Create();
 *       
 *       // 启动 UI
 *       BG_UI.Start();
 *   }
 *   
 *   void app_loop() {
 *       // 更新 UI (20ms 周期)
 *       BG_UI.Update(20);
 *   }
 * 
 * 文件结构:
 * 
 *   ui/
 *   ├── bangui.h            <-- 主入口 (本文件)
 *   ├── core/               核心层
 *   │   ├── bg_ui.h/.c          UI 主对象
 *   │   ├── ui_page.h/.c        页面管理器
 *   │   └── bg_page_compat.h/.c 旧 API 兼容层
 *   ├── components/         组件层
 *   │   ├── comp_statusbar.h/.c 状态栏
 *   │   └── comp_popup.h/.c     弹窗
 *   ├── views/              视图层
 *   │   ├── view_home.h/.c      主界面
 *   │   ├── view_menu.h/.c      菜单
 *   │   └── view_looper.h/.c    Looper
 *   └── resources/          资源层
 *       ├── ui_icons.h          图标
 *       └── ui_fonts.h          字体
 */

#ifndef __BANGUI_H__
#define __BANGUI_H__

/*===========================================================================
 * 核心层
 *===========================================================================*/

#include "core/bg_ui.h"
#include "core/ui_page.h"
#include "core/bg_page_compat.h"  /* 旧 API 兼容 */

/*===========================================================================
 * 组件层
 *===========================================================================*/

#include "components/ui_config.h"
#include "components/comp_statusbar.h"
#include "components/comp_popup.h"
#include "components/comp_menu.h"
#include "components/ui_system.h"   /* 旧 UI 系统兼容层 */

/*===========================================================================
 * 视图层
 *===========================================================================*/

#include "views/view_boot.h"
#include "views/view_home.h"
#include "views/view_menu.h"
#include "views/view_looper.h"
#include "views/app_pages.h"     /* 应用页面定义 (兼容旧 API) */

/*===========================================================================
 * 版本信息
 *===========================================================================*/

#define BANGUI_VERSION_MAJOR    1
#define BANGUI_VERSION_MINOR    0
#define BANGUI_VERSION_PATCH    0
#define BANGUI_VERSION_STRING   "1.0.0"

/*===========================================================================
 * 便捷宏
 *===========================================================================*/

/**
 * @brief 快速初始化 UI 系统
 * 初始化 BG_UI 并创建所有默认视图
 */
#define BANGUI_QUICK_INIT() do { \
    BG_UI.Init();                \
    Comp_StatusBar_Init();       \
    Comp_Popup_Init();           \
    View_Home_Create();          \
    View_Menu_Create();          \
    View_Looper_Create();        \
} while(0)

/**
 * @brief 快速启动 UI 系统
 * @param start_state 初始状态
 */
#define BANGUI_START(start_state) do { \
    BG_UI.SetState(start_state);       \
    BG_UI.Start();                     \
} while(0)

#endif /* __BANGUI_H__ */
