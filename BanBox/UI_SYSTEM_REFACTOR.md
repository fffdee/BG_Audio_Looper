# UI 系统重构说明

## 新架构概述

重构后的 UI 系统采用 **View-Core-Bridge** 三层架构：

```
┌─────────────────────────────────────────────────────────────────┐
│                     Application Layer                           │
│  (Page System, Looper, Audio Manager, User Code)               │
└─────────────────────────────┬───────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────┐
│                   Page-UI Bridge (page_ui_bridge)               │
│  - 统一按钮事件分发                                               │
│  - Page/UI 系统模式切换                                          │
│  - 自定义事件处理器                                               │
└─────────────────────────────┬───────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────┐
│                      UI Core (ui_core)                          │
│  - 状态机管理 (BOOT, IDLE, MENU, POPUP, etc.)                   │
│  - 事件队列和分发                                                 │
│  - View 生命周期管理                                              │
│  - Popup 管理                                                    │
└────────┬─────────────────────────────────────┬──────────────────┘
         │                                     │
┌────────▼────────┐                   ┌───────▼────────────────┐
│  Input Layer    │                   │      View Layer        │
│  - ui_button    │                   │  - ui_view_home        │
│  - (touch/knob) │                   │  - ui_view_menu        │
└─────────────────┘                   │  - ui_statusbar        │
                                      │  - ui_bootscreen       │
                                      └────────────────────────┘
```

## 核心概念

### 1. UI State (UI 状态)
```c
typedef enum {
    UI_STATE_BOOT,      // 启动画面
    UI_STATE_IDLE,      // 主界面 (Home)
    UI_STATE_MENU,      // 菜单导航
    UI_STATE_POPUP,     // 弹窗对话框
    UI_STATE_PLAYER,    // 播放器界面
    UI_STATE_LOOPER,    // Looper 控制界面
    UI_STATE_SETTINGS,  // 设置界面
    UI_STATE_CUSTOM,    // 自定义扩展
} UI_State_t;
```

### 2. View (视图)
每个视图是一个可绘制的 UI 组件，包含完整的生命周期回调：

```c
typedef struct UI_View {
    const char* name;                   // 视图名称
    
    // 生命周期回调
    void (*on_create)(void);            // 创建时调用
    void (*on_destroy)(void);           // 销毁时调用
    void (*on_enter)(void);             // 进入时调用
    void (*on_exit)(void);              // 离开时调用
    
    // 更新和绘制
    void (*on_update)(uint16_t delta_ms);   // 每帧更新
    void (*on_draw)(void);                  // 需要重绘时调用
    
    // 事件处理
    bool (*on_event)(UI_Event_t* event);    // 返回 true 表示事件已消费
    
    // 视图属性
    bool visible;                       // 是否可见
    bool needs_redraw;                  // 是否需要重绘
    uint8_t z_order;                    // Z 序（层级）
} UI_View_t;
```

### 3. Event (事件)
统一的事件结构：

```c
typedef struct {
    UI_EventType_t type;
    union {
        UI_ButtonEventData_t button;    // 按钮事件
        struct {
            UI_State_t from;
            UI_State_t to;
        } state_change;                 // 状态变化事件
        uint32_t custom;                // 自定义事件
    } data;
} UI_Event_t;
```

## 文件结构

```
ui_system/
├── ui_core.h/.c           # UI 核心 - 状态机、事件分发、View 管理
├── ui_config.h            # 配置常量 (屏幕尺寸、颜色等)
├── ui_button.h/.c         # 按钮输入处理
├── ui_view_home.h/.c      # 主界面视图
├── ui_view_menu.h/.c      # 菜单视图
├── ui_statusbar.h/.c      # 状态栏组件
├── ui_menu.h/.c           # 菜单组件
├── ui_bootscreen.h/.c     # 启动画面组件
└── ui_system.h/.c         # 旧版兼容层 (保留)

page/
├── bg_page.h/.c           # Page 系统核心
├── page_manager.h/.c      # 页面管理器
└── page_ui_bridge.h/.c    # Page-UI 桥接模块
```

## 使用方法

### 1. 初始化

```c
// 初始化 UI Core
UI_Core_Init();

// 初始化视图
UI_View_Home_Init();
UI_View_Menu_Init();

// 启动 UI Core
UI_Core_Start(UI_STATE_IDLE);

// 初始化 Page 系统
BG_page = BG_Page_Init(table, MAX_PAGE);

// 初始化 Bridge
PageUI_Bridge_Init(&BG_page, NULL);
```

### 2. 主循环更新

```c
while (1) {
    // 更新 Bridge（处理按钮、更新 Page/UI）
    PageUI_Bridge_Update(20);
    
    // 刷新显示
    BG_lcd.FlushFrameBuffer();
}
```

### 3. 创建自定义视图

```c
// 定义视图回调
static void my_view_on_draw(void) {
    BG_lcd.Box(0, 0, 160, 128, UI_COLOR_BLACK);
    // 绘制内容...
}

static bool my_view_on_event(UI_Event_t* event) {
    if (event->type == UI_EVENT_BUTTON) {
        // 处理按钮事件
        return true;
    }
    return false;
}

// 创建视图实例
static UI_View_t my_view = {
    .name = "MyView",
    .on_draw = my_view_on_draw,
    .on_event = my_view_on_event,
    .visible = true
};

// 注册视图
UI_Core_RegisterView(UI_STATE_CUSTOM, &my_view);
```

### 4. 状态切换

```c
// 切换到菜单
UI_Core_SetState(UI_STATE_MENU);

// 切换到主界面
UI_Core_SetState(UI_STATE_IDLE);

// 显示弹窗
UI_Core_ShowPopup("提示", "操作成功", 2000, NULL);
```

### 5. 通过 Bridge 切换

```c
// 切换到 Page 系统控制
PageUI_Bridge_SwitchToPage(HOME_PAGE);

// 切换到 UI 系统控制
PageUI_Bridge_SwitchToUI(UI_STATE_MENU);
```

## 事件流

```
按钮按下
    │
    ▼
UI_Button_Scan() ──────► UI_ButtonEventData_t
    │
    ▼
PageUI_Bridge_Update()
    │
    ├── BRIDGE_MODE_PAGE ──► BG_page.Last/Next/Enter/Exit()
    │
    └── BRIDGE_MODE_UI ────► UI_Core_HandleButton()
                                    │
                                    ▼
                              UI_Event_t (type=UI_EVENT_BUTTON)
                                    │
                                    ▼
                              Event Queue
                                    │
                                    ▼
                              dispatch_to_handlers() (全局处理器)
                                    │
                                    ▼
                              dispatch_to_views() (视图处理器)
```

## 优势

1. **清晰的层次结构**：Core 负责状态管理，View 负责显示和交互
2. **可扩展性**：轻松添加新视图和新状态
3. **事件驱动**：统一的事件处理机制
4. **向后兼容**：保留旧版 ui_system 接口
5. **View 生命周期**：明确的创建/进入/退出/销毁流程

## 迁移指南

旧代码：
```c
UI_System_Init(NULL);
UI_System_Start();
UI_System_Update(20);
UI_System_SetState(UI_STATE_MENU);
```

新代码：
```c
UI_Core_Init();
UI_View_Home_Init();
UI_View_Menu_Init();
UI_Core_Start(UI_STATE_IDLE);

// 在主循环中
PageUI_Bridge_Update(20);  // 或 UI_Core_Update(20);

UI_Core_SetState(UI_STATE_MENU);
```
