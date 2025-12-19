# 自动滑动菜单演示使用说明

## 功能概述

本演示展示了一个基于定时器的自动滑动菜单系统，每2秒自动循环切换菜单项，展示流畅的滑动动画效果。

## 主要特性

### 1. 自动滑动
- **间隔时间**: 每2秒自动滑动一次
- **滑动方向**: 循环向右滑动
- **滑动效果**: 使用缓动动画，流畅自然

### 2. 菜单项
演示包含5个菜单项：
- Music (音乐)
- Settings (设置) 
- Games (游戏)
- Tools (工具)
- Photo (相册)

### 3. 视觉反馈
- 每个菜单项被选中时会显示不同颜色的背景
- 选中项有放大和偏移效果
- 实时显示当前选中的菜单名称

## 代码实现详解

### 定时器配置
```c
// Timer2中断，每20ms调用一次
void Timer2Interrupt(void) {
    // 自动滑动计时器
    if (demo_mode_enabled) {
        auto_slide_timer++;
        // 每2秒滑动一次 (2000ms / 20ms = 100次中断)
        if (auto_slide_timer >= 100) {
            auto_slide_timer = 0;
            IconMenuSlider_Right(&demo_slider); // 向右滑动
        }
    }
}
```

### 菜单初始化
```c
void init_demo_menu(void) {
    // 初始化菜单滑块
    IconMenuSlider_Init(&demo_slider, 80, 50, 0xFFFF, 0x07E0);
    
    // 添加菜单项
    IconMenuSlider_AddItem(&demo_slider, "Music", NULL, 0, 0, demo_music_callback);
    IconMenuSlider_AddItem(&demo_slider, "Settings", NULL, 0, 0, demo_settings_callback);
    // ... 更多菜单项
}
```

### 主循环更新
```c
while (1) {
    BG_AudioManager.Audio_Loop();
    BG_page.Loop(&BG_page);
    
    // 更新菜单动画
    if (demo_mode_enabled) {
        IconMenuSlider_Update(&demo_slider);  // 更新动画状态
        IconMenuSlider_Draw(&demo_slider);    // 绘制菜单
    }
}
```

## 关键变量说明

| 变量名 | 类型 | 功能 |
|--------|------|------|
| `demo_slider` | `IconMenuSlider` | 主菜单滑块对象 |
| `auto_slide_timer` | `uint32_t` | 自动滑动计时器 |
| `demo_mode_enabled` | `uint8_t` | 演示模式开关 |

## 性能参数

### 时间参数
- **滑动间隔**: 2000ms (2秒)
- **定时器频率**: 50Hz (20ms间隔)
- **动画帧率**: 取决于主循环频率

### 显示参数
- **菜单项宽度**: 64像素
- **菜单项高度**: 64像素
- **菜单中心位置**: (80, 50)
- **选中项放大**: 宽度+4px, 高度+2px

## 自定义配置

### 修改滑动间隔
```c
// 修改Timer2Interrupt中的判断条件
if (auto_slide_timer >= 100) {  // 100 = 2000ms / 20ms
    // 改为50表示1秒间隔: 50 = 1000ms / 20ms
    // 改为150表示3秒间隔: 150 = 3000ms / 20ms
}
```

### 修改滑动方向
```c
// 向左滑动
IconMenuSlider_Left(&demo_slider);

// 向右滑动  
IconMenuSlider_Right(&demo_slider);

// 随机方向
if (rand() % 2) {
    IconMenuSlider_Right(&demo_slider);
} else {
    IconMenuSlider_Left(&demo_slider);
}
```

### 添加更多菜单项
```c
IconMenuSlider_AddItem(&demo_slider, "Video", NULL, 0, 0, demo_video_callback);
IconMenuSlider_AddItem(&demo_slider, "Calendar", NULL, 0, 0, demo_calendar_callback);
```

## 扩展功能

### 1. 手动控制
可以添加按钮控制来暂停/恢复自动滑动：
```c
void toggle_demo_mode(void) {
    demo_mode_enabled = !demo_mode_enabled;
    auto_slide_timer = 0; // 重置计时器
}
```

### 2. 变速滑动
可以实现变速滑动效果：
```c
// 快速滑动阶段
if (slide_count < 3) {
    timer_threshold = 50;  // 1秒间隔
} else {
    timer_threshold = 150; // 3秒间隔
}
```

### 3. 方向切换
可以实现来回滑动效果：
```c
static uint8_t direction = 1;
static uint8_t slide_count = 0;

if (slide_count >= 5) {
    direction = !direction;
    slide_count = 0;
}

if (direction) {
    IconMenuSlider_Right(&demo_slider);
} else {
    IconMenuSlider_Left(&demo_slider);
}
slide_count++;
```

## 调试与优化

### 性能监控
```c
// 监控动画状态
if (IconMenuSlider_IsAnimating(&demo_slider)) {
    // 动画进行中
} else {
    // 动画完成
}
```

### 内存使用
- 菜单对象: ~100字节
- 帧缓冲(如果启用): LCD_WIDTH × LCD_HEIGHT × 2字节

### 优化建议
1. 在帧缓冲模式下运行以获得最佳性能
2. 根据实际硬件调整动画参数
3. 监控CPU使用率，避免主循环阻塞

## 常见问题

### Q: 滑动太快或太慢？
A: 修改Timer2Interrupt中的判断阈值，或调整MENU_SLIDER_SPEED常量

### Q: 动画不流畅？
A: 确保主循环频率足够高，建议启用帧缓冲模式

### Q: 如何停止自动滑动？
A: 设置 `demo_mode_enabled = 0`

### Q: 如何添加图标？
A: 将IconMenuSlider_AddItem中的NULL替换为图标数据指针
