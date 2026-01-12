# BanGUI UI 系统重构文档

## 📋 概述

本次重构将 UI 系统抽象为一个统一的对象 (`BG_UI`)，并重新组织文件结构为清晰的分层架构。
页面管理器已迁移到 core 层，与 UI 系统深度集成。

## 🏗️ 新架构

### 目录结构

```
ui/
├── bangui.h                 # 主入口头文件 (只需包含这一个)
├── core/                    # 核心层
│   ├── bg_ui.h/.c          # UI 主对象
│   ├── ui_page.h/.c        # 页面管理器 (新)
│   └── bg_page_compat.h/.c # 旧 BG_Page API 兼容层
├── components/              # 组件层
│   ├── comp_statusbar.h/.c # 状态栏组件
│   └── comp_popup.h/.c     # 弹窗组件
├── views/                   # 视图层
│   ├── view_home.h/.c      # 主界面视图
│   ├── view_menu.h/.c      # 菜单视图
│   ├── view_looper.h/.c    # Looper 视图
│   └── app_pages.h/.c      # 应用页面定义 (兼容旧 API)
└── resources/               # 资源层
    ├── ui_icons.h          # 图标资源
    └── ui_fonts.h          # 字体资源
```

### 使用方法

```c
#include "bangui.h"

void app_init() {
    // 方法1: 快速初始化
    BANGUI_QUICK_INIT();
    BANGUI_START(UI_STATE_IDLE);
    
    // 方法2: 手动初始化
    BG_UI.Init();
    View_Home_Create();
    View_Menu_Create();
    BG_UI.Start();
}

void app_loop() {
    BG_UI.Update(20);  // 20ms 周期
}
```

### BG_UI 对象接口

| 方法 | 说明 |
|------|------|
| `BG_UI.Init()` | 初始化 UI 系统 |
| `BG_UI.Start()` | 启动 UI 系统 |
| `BG_UI.Update(delta_ms)` | 主循环更新 |
| `BG_UI.SetState(state)` | 切换 UI 状态 |
| `BG_UI.GetState()` | 获取当前状态 |
| `BG_UI.RegisterView(state, view)` | 注册视图到状态 |
| `BG_UI.HandleButton(event)` | 处理按钮事件 |
| `BG_UI.ShowPopup(title, msg, duration)` | 显示弹窗 |
| `BG_UI.Invalidate()` | 标记需要重绘 |

## 📁 文件迁移计划

### 保留文件 (迁移到新位置)

| 原文件 | 新位置 | 说明 |
|--------|--------|------|
| `ui_system/ui_menu.c/h` | `ui_system/` | 暂保留,后续迁移到 components |
| `ui_system/ui_statusbar.c/h` | `ui_system/` | 暂保留,后续迁移到 components |
| `ui_system/ui_button.c/h` | `ui_system/` | 暂保留,按钮扫描基础功能 |
| `ui_system/ui_config.h` | `ui_system/` | 配置常量 |
| `ui_system/picture.h` | `resources/` | 图标资源 |

### 待删除文件 (未使用或仅测试用)

| 文件 | 原因 |
|------|------|
| `ui_system/audio_spectrum_simple.c/h` | 简化版频谱,未使用 |
| `ui_system/vacal_setting.c` | 测试代码,未完成 |
| `ui_system/ui_view_home.c/h` | 旧版视图,已迁移到 ui/views/ |
| `ui_system/ui_view_menu.c/h` | 旧版视图,已迁移到 ui/views/ |
| `ui_system/ui_core.c/h` | 旧版核心,已迁移到 ui/core/bg_ui |
| `menu_slider/menu_slider_demo.c` | Demo 代码 |

### 待整理文件

| 目录 | 说明 |
|------|------|
| `base_func/` | 基础绘图函数,可整合到 core |
| `BG_List/` | 列表组件,可迁移到 components |
| `menu_slider/` | 滑块菜单,保留核心文件 |

## 🔄 迁移步骤

### 第一阶段: 新建架构 ✅

1. ✅ 创建 `ui/core/bg_ui.h/.c` - UI 主对象
2. ✅ 创建 `ui/views/view_home.h/.c` - 主界面视图
3. ✅ 创建 `ui/views/view_menu.h/.c` - 菜单视图
4. ✅ 创建 `ui/views/view_looper.h/.c` - Looper 视图
5. ✅ 创建 `ui/components/comp_statusbar.h/.c` - 状态栏
6. ✅ 创建 `ui/components/comp_popup.h/.c` - 弹窗
7. ✅ 创建 `ui/bangui.h` - 统一入口

### 第二阶段: 更新 main.c

修改 `main.c` 使用新的 BG_UI 接口:

```c
// 旧代码
#include "ui_system.h"
#include "ui_core.h"
#include "ui_view_home.h"
#include "ui_view_menu.h"

// 新代码
#include "bangui.h"
```

```c
// 旧代码
UI_Core_Init();
UI_View_Home_Init();
UI_View_Menu_Init();
UI_System_Init(NULL);
UI_Menu_InitDefault();
UI_Core_Start(UI_STATE_IDLE);

// 新代码
BANGUI_QUICK_INIT();
BANGUI_START(UI_STATE_IDLE);
```

### 第三阶段: 清理旧文件

1. 删除 `ui_system/audio_spectrum_simple.c/h`
2. 删除 `ui_system/vacal_setting.c`
3. 删除旧版视图文件 (确认新版工作后)
4. 更新 makefile 编译文件列表

## 📊 状态机

```
                ┌─────────────────────────────────────┐
                │                                     │
                ▼                                     │
    ┌────────────────────┐                           │
    │    UI_STATE_BOOT   │──────────────────────────►│
    └────────────────────┘                           │
                │                                     │
                │ (Boot complete)                     │
                ▼                                     │
    ┌────────────────────┐     ┌──────────────────┐  │
    │    UI_STATE_IDLE   │◄───►│  UI_STATE_MENU   │  │
    │      (Home)        │     │    (Navigation)  │  │
    └────────────────────┘     └──────────────────┘  │
                │                       │            │
                │                       │            │
                ▼                       ▼            │
    ┌────────────────────┐     ┌──────────────────┐  │
    │  UI_STATE_LOOPER   │     │ UI_STATE_SETTINGS│──┘
    │   (Looper View)    │     │  (Settings View) │
    └────────────────────┘     └──────────────────┘
                │
                │
                ▼
    ┌────────────────────┐
    │   UI_STATE_POPUP   │ (Modal overlay on any state)
    │    (Popup Dialog)  │
    └────────────────────┘
```

## 🔧 编译配置

### 新增编译文件

在 `subdir.mk` 中添加:

```makefile
# New UI Architecture
C_SRCS += \
    ../src/banux/05_component/BanGUI/ui/core/bg_ui.c \
    ../src/banux/05_component/BanGUI/ui/views/view_home.c \
    ../src/banux/05_component/BanGUI/ui/views/view_menu.c \
    ../src/banux/05_component/BanGUI/ui/views/view_looper.c \
    ../src/banux/05_component/BanGUI/ui/components/comp_statusbar.c \
    ../src/banux/05_component/BanGUI/ui/components/comp_popup.c
```

### 包含路径

```makefile
INCLUDES += \
    -I../src/banux/05_component/BanGUI/ui \
    -I../src/banux/05_component/BanGUI/ui/core \
    -I../src/banux/05_component/BanGUI/ui/views \
    -I../src/banux/05_component/BanGUI/ui/components \
    -I../src/banux/05_component/BanGUI/ui/resources
```

## 📝 待办事项

- [ ] 更新 main.c 使用新架构
- [ ] 测试新 UI 系统
- [ ] 删除旧的未使用文件
- [ ] 更新 makefile
- [ ] 完善 View_Looper 与 AudioLooper 的集成
- [ ] 添加 Settings 视图
- [ ] 迁移 menu_slider 到 components
- [ ] 迁移 BG_List 到 components

## 📅 更新记录

| 日期 | 更新内容 |
|------|----------|
| 2025-01-08 | 创建新 UI 架构，完成核心层和视图层 |
