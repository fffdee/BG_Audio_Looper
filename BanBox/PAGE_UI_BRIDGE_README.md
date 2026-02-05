# Page-UI Bridge 页面与按钮事件融合模块

## 概述

`page_ui_bridge` 模块实现了 BG_Page 页面系统与 UI_System UI系统的统一事件管理。它提供了一个中间层来处理四个物理按钮的事件，并根据当前状态将事件路由到正确的处理系统。

## 按钮映射

| GPIO 引脚 | 按钮 ID | UI 系统功能 | Page 系统功能 |
|-----------|---------|-------------|---------------|
| GPIO_A0   | UI_BTN_UP | 向上导航 | `BG_page.Last()` |
| GPIO_B5   | UI_BTN_DOWN | 向下导航 | `BG_page.Next()` |
| GPIO_A15  | UI_BTN_ENTER | 确认/进入 | `BG_page.Enter()` |
| GPIO_A16  | UI_BTN_BACK | 返回/取消 | `BG_page.Exit()` |

## 工作模式

### BRIDGE_MODE_AUTO（默认）
自动根据 UI 系统状态决定事件路由：
- `UI_STATE_BOOT` / `UI_STATE_POPUP` → 事件路由到 UI 系统
- `UI_STATE_MENU` → 事件路由到 UI 系统
- `UI_STATE_IDLE` / 其他 → 事件路由到 Page 系统

### BRIDGE_MODE_PAGE
所有事件强制路由到 Page 系统。

### BRIDGE_MODE_UI
所有事件强制路由到 UI 系统。

### BRIDGE_MODE_DISABLED
忽略所有按钮事件。

## 使用方法

### 1. 初始化

在 `main.c` 中的初始化代码：

```c
#include "page_ui_bridge.h"

// 初始化 Page 系统
BG_page = BG_Page_Init(table, MAX_PAGE);

// 配置 Bridge
PageUI_BridgeConfig_t bridge_config = {
    .mode = BRIDGE_MODE_AUTO,           // 自动路由模式
    .enable_page_callbacks = true,
    .enable_ui_callbacks = true,
    .long_press_to_exit = true,         // 长按 BACK 显示菜单
    .update_interval_ms = 20
};
PageUI_Bridge_Init(&BG_page, &bridge_config);
```

### 2. 主循环更新

```c
while (1) {
    // 其他任务...
    
    if (UI_flag == 1) {
        UI_flag = 0;
        
        // 更新 Bridge（处理按钮事件并路由）
        PageUI_Bridge_Update(20);
        
        // 刷新显示
        BG_lcd.FlushFrameBuffer();
    }
}
```

### 3. 手动切换模式

```c
// 切换到指定页面
PageUI_Bridge_SwitchToPage(HOME_PAGE);

// 切换到 UI 菜单
PageUI_Bridge_SwitchToUI(UI_STATE_MENU);

// 检查当前活跃系统
if (PageUI_Bridge_IsPageActive()) {
    // Page 系统正在处理事件
}
```

### 4. 自定义事件处理器

```c
// 自定义处理器函数
bool my_custom_handler(UI_ButtonEventData_t* event) {
    if (event->id == UI_BTN_ENTER && event->event == UI_BTN_EVENT_LONG_PRESS) {
        // 处理长按确认的特殊逻辑
        return true;  // 返回 true 表示事件已消费
    }
    return false;  // 返回 false 继续传递给默认处理器
}

// 注册处理器（高优先级）
PageUI_Bridge_RegisterHandler(my_custom_handler, BRIDGE_PRIORITY_HIGH);

// 移除处理器
PageUI_Bridge_UnregisterHandler(my_custom_handler);
```

## 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                     Physical Buttons                        │
│   GPIO_A0 (UP)   GPIO_B5 (DOWN)   GPIO_A15 (ENTER)   A16   │
└─────────────────────────────┬───────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     UI_Button Module                        │
│   - GPIO Scan                                               │
│   - Debounce                                                │
│   - Event Generation (Press/Release/Click/LongPress)        │
│   - Event Queue                                             │
└─────────────────────────────┬───────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Page-UI Bridge Module                     │
│   - Custom Handler Priority Chain                           │
│   - Mode-based Event Routing                                │
│   - Statistics Tracking                                     │
└────────────┬───────────────────────────────┬────────────────┘
             │                               │
             ▼                               ▼
┌────────────────────────┐     ┌────────────────────────────┐
│     BG_Page System     │     │       UI_System            │
│  - Page State Machine  │     │  - Menu Navigation         │
│  - Page Callbacks      │     │  - Popup Management        │
│  - last/next/enter/exit│     │  - Status Bar              │
└────────────────────────┘     └────────────────────────────┘
```

## Page 系统与 UI 系统协作示例

### 从 Page 页面进入 UI 菜单

```c
// 在页面操作函数中
void my_page_operation() {
    if (BG_page.Data.enter_pressed) {
        // 长按 ENTER 进入设置菜单
        PageUI_Bridge_SwitchToUI(UI_STATE_MENU);
        BG_page.Data.enter_pressed = 0;
    }
}
```

### 从 UI 菜单返回 Page 页面

```c
// 在菜单回调中
void menu_back_to_page_callback(void) {
    PageUI_Bridge_SwitchToPage(HOME_PAGE);
}
```

## 调试与统计

```c
// 获取统计信息
const PageUI_BridgeStats_t* stats = PageUI_Bridge_GetStats();
DBG("Total events: %d\n", stats->total_events);
DBG("Page events: %d\n", stats->page_events);
DBG("UI events: %d\n", stats->ui_events);
DBG("Ignored events: %d\n", stats->ignored_events);

// 重置统计
PageUI_Bridge_ResetStats();
```

## 注意事项

1. **初始化顺序**：先初始化 UI_System，再初始化 BG_Page，最后初始化 Bridge。

2. **按钮扫描**：Bridge 内部调用 `UI_Button_Scan()`，不需要在主循环中重复调用。

3. **UI 更新**：Bridge 内部调用 `UI_System_UpdateStateOnly()`，不需要额外调用 `UI_System_Update()`。

4. **长按检测**：Bridge 支持长按 BACK 键自动切换到 UI 菜单（可配置）。

5. **自定义处理器**：最多支持 4 个自定义事件处理器，按优先级顺序调用。

## 文件列表

| 文件 | 说明 |
|------|------|
| `page_ui_bridge.h` | Bridge 模块头文件，包含 API 声明 |
| `page_ui_bridge.c` | Bridge 模块实现 |
| `page_manager.h` | 已更新，包含 bridge 头文件 |
| `ui_system.h` | 已更新，添加 `UI_System_UpdateStateOnly()` |
| `ui_system.c` | 已更新，实现 `UI_System_UpdateStateOnly()` |
| `main.c` | 已更新，集成 Bridge 初始化和更新 |

## 更新日志

### 2025-06-21
- 创建 Page-UI Bridge 模块
- 实现统一的按钮事件分发机制
- 添加自动模式路由
- 集成到主循环
