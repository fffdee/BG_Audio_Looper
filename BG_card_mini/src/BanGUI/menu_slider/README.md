# BG_MenuSlider 滑动菜单控件

## 概述

BG_MenuSlider 是一个高度模块化的滑动菜单控件，支持面向对象的操作方式。控件具有以下特性：

- 📱 支持左右滑动导航
- 🎨 丰富的样式配置选项
- 🎬 平滑的动画效果和多种缓动类型
- 🔄 自动滑动功能
- 🖼️ 支持图标和文字菜单项
- 📝 帧缓冲复用和脏区域优化
- 🎯 回调函数和用户数据支持

## 文件结构

```
menu_slider/
├── bg_menu_slider.h          # 主头文件
├── bg_menu_slider.c          # 主实现文件  
├── bg_menu_slider_example.h  # 使用示例头文件
├── bg_menu_slider_example.c  # 使用示例实现
└── README.md                 # 本说明文档
```

## 快速开始

### 1. 基本用法

```c
#include "bg_menu_slider.h"

// 定义菜单项
static BG_MenuSlider_Item menu_items[] = {
    BG_MENU_SLIDER_TEXT_ITEM("Home", home_callback),
    BG_MENU_SLIDER_TEXT_ITEM("Music", music_callback),
    BG_MENU_SLIDER_TEXT_ITEM("Settings", settings_callback),
};

// 创建菜单表
static BG_MenuSlider_Table menu_table = BG_MENU_SLIDER_TABLE(menu_items);

// 创建控件
BG_MenuSlider slider = BG_MenuSlider_Create(
    &BG_MENU_SLIDER_REGION_CENTER,  // 显示区域
    &BG_MENU_SLIDER_STYLE_DEFAULT,  // 样式
    &menu_table                     // 菜单项表
);
```

### 2. 在主循环中更新

```c
void main_loop(void) {
    uint32_t last_time = get_system_time();
    
    while (1) {
        uint32_t current_time = get_system_time();
        uint32_t delta_ms = current_time - last_time;
        
        // 更新控件动画
        slider.Update(&slider);
        slider.AutoSlideUpdate(&slider, delta_ms);
        
        // 绘制控件
        slider.Draw(&slider);
        slider.Refresh(&slider);
        
        last_time = current_time;
    }
}
```

### 3. 处理用户输入

```c
void handle_button_press(uint8_t button) {
    switch (button) {
        case BUTTON_LEFT:
            slider.SlideLeft(&slider);
            break;
        case BUTTON_RIGHT:
            slider.SlideRight(&slider);
            break;
        case BUTTON_SELECT:
            slider.Select(&slider);
            break;
    }
}
```

## 配置选项

### 样式配置

```c
BG_MenuSlider_Style custom_style = {
    .selected_color = 0xF800,      // 红色边框
    .normal_color = 0x8410,        // 灰色边框
    .background_color = 0x0000,    // 黑色背景
    .text_color_selected = 0xFFFF, // 白色选中文字
    .text_color_normal = 0x8410,   // 灰色普通文字
    .border_width = 2,             // 边框宽度
    .show_border = 1,              // 显示边框
    .show_background = 1,          // 显示背景
    .show_text = 1                 // 显示文字
};

slider.SetStyle(&slider, &custom_style);
```

### 动画配置

```c
BG_MenuSlider_Animation custom_anim = {
    .slide_duration = 300,              // 滑动持续300ms
    .ease_type = BG_MENU_SLIDER_EASE_IN_OUT, // 缓入缓出
    .auto_slide_enable = 1,             // 启用自动滑动
    .auto_slide_interval = 3000,        // 3秒间隔
    .scale_effect = 1                   // 启用缩放效果
};

slider.SetAnimation(&slider, &custom_anim);
```

### 区域配置

```c
BG_MenuSlider_Region custom_region = {
    .x = 20,              // 距离左边20像素
    .y = 30,              // 距离顶部30像素
    .width = 120,         // 宽度120像素
    .height = 60,         // 高度60像素
    .center_x = 80,       // 中心X坐标
    .item_width = 50,     // 菜单项宽度
    .item_height = 50     // 菜单项高度
};

slider.SetRegion(&slider, &custom_region);
```

## 接口说明

### 配置接口

- `SetRegion()` - 设置显示区域
- `SetFrameBuffer()` - 设置帧缓冲配置
- `SetStyle()` - 设置样式
- `SetAnimation()` - 设置动画参数

### 菜单操作接口

- `AddItem()` - 添加单个菜单项
- `ClearItems()` - 清空所有菜单项
- `LoadTable()` - 批量加载菜单项表
- `RemoveItem()` - 删除指定索引的菜单项

### 控制接口

- `SlideLeft()` - 向左滑动
- `SlideRight()` - 向右滑动
- `SlideTo()` - 滑动到指定索引
- `Select()` - 选择当前项（触发回调）

### 更新和绘制接口

- `Update()` - 更新动画状态
- `Draw()` - 绘制控件
- `Clear()` - 清除显示区域
- `Refresh()` - 刷新到屏幕
- `RefreshRegion()` - 刷新指定区域

### 自动滑动接口

- `StartAutoSlide()` - 开始自动滑动
- `StopAutoSlide()` - 停止自动滑动
- `ToggleAutoSlide()` - 切换自动滑动状态
- `AutoSlideUpdate()` - 更新自动滑动计时器

### 状态查询接口

- `GetSelectedIndex()` - 获取当前选中项索引
- `GetSelectedItem()` - 获取当前选中项指针
- `GetItemCount()` - 获取菜单项总数
- `IsSliding()` - 是否正在滑动动画中
- `IsAutoSliding()` - 是否正在自动滑动
- `IsInitialized()` - 是否已初始化

## 预定义常量

### 样式常量

- `BG_MENU_SLIDER_STYLE_DEFAULT` - 默认样式
- `BG_MENU_SLIDER_STYLE_DARK` - 深色主题
- `BG_MENU_SLIDER_STYLE_COLORFUL` - 彩色主题
- `BG_MENU_SLIDER_STYLE_MINIMAL` - 简约风格

### 动画常量

- `BG_MENU_SLIDER_ANIM_SMOOTH` - 平滑动画
- `BG_MENU_SLIDER_ANIM_FAST` - 快速动画
- `BG_MENU_SLIDER_ANIM_AUTO` - 自动滑动
- `BG_MENU_SLIDER_ANIM_INSTANT` - 无动画

### 区域常量

- `BG_MENU_SLIDER_REGION_CENTER` - 屏幕中央
- `BG_MENU_SLIDER_REGION_TOP` - 屏幕顶部
- `BG_MENU_SLIDER_REGION_BOTTOM` - 屏幕底部

### 帧缓冲常量

- `BG_MENU_SLIDER_FB_FULL_SCREEN` - 全屏帧缓冲
- `BG_MENU_SLIDER_FB_DIRTY_ONLY` - 脏区域优化

## 工具宏

```c
// 创建带图标的菜单项
BG_MENU_SLIDER_ITEM("Play", play_icon, 32, 32, play_callback, 1)

// 创建纯文字菜单项
BG_MENU_SLIDER_TEXT_ITEM("Settings", settings_callback)

// 创建菜单项表
BG_MENU_SLIDER_TABLE(menu_items_array)
```

## 高级用法

### 动态菜单管理

```c
// 运行时添加菜单项
BG_MenuSlider_Item new_item = BG_MENU_SLIDER_TEXT_ITEM("New", callback);
slider.AddItem(&slider, &new_item);

// 删除菜单项
slider.RemoveItem(&slider, 2);  // 删除索引为2的项

// 清空并重新加载
slider.ClearItems(&slider);
slider.LoadTable(&slider, &new_table);
```

### 帧缓冲优化

```c
// 配置帧缓冲以提高性能
BG_MenuSlider_FrameBuffer fb_config = {
    .buffer = shared_frame_buffer,     // 共享帧缓冲
    .buffer_width = 160,
    .buffer_height = 128,
    .use_dirty_region = 1,             // 启用脏区域优化
    .use_shared_buffer = 1,            // 使用共享缓冲
    .refresh_x = 0,
    .refresh_y = 32,
    .refresh_width = 160,
    .refresh_height = 64
};

slider.SetFrameBuffer(&slider, &fb_config);
```

### 自定义动画

```c
// 创建自定义动画配置
BG_MenuSlider_Animation anim = {
    .slide_duration = 500,                    // 较慢的滑动
    .ease_type = BG_MENU_SLIDER_EASE_OUT,     // 缓出效果
    .auto_slide_enable = 1,                   // 启用自动滑动
    .auto_slide_interval = 5000,              // 5秒间隔
    .scale_effect = 0                         // 禁用缩放
};

slider.SetAnimation(&slider, &anim);
```

## 注意事项

1. **内存管理**: 控件内部会复制菜单项数据，无需担心传入的临时数据被释放
2. **线程安全**: 控件不是线程安全的，需要在单线程环境中使用
3. **帧缓冲**: 使用帧缓冲模式时需要确保LCD驱动支持相应的接口
4. **图标数据**: 图标数据格式需要与LCD驱动的ShowImage函数兼容
5. **性能优化**: 启用脏区域刷新可以提高性能，特别是在频繁更新时

## 集成到现有项目

1. 将头文件和源文件复制到项目中
2. 确保包含路径正确设置
3. 根据需要调整TODO部分的具体实现
4. 在编译配置中添加源文件
5. 在主循环中调用Update、Draw、Refresh等函数

## 示例代码

详细的使用示例请参考 `bg_menu_slider_example.c` 文件，其中包含：

- 基本菜单创建
- 文字菜单创建  
- 主循环集成
- 用户输入处理
- 动态菜单管理
- 完整的生命周期示例

## 许可证

此控件遵循项目的整体许可证条款。
