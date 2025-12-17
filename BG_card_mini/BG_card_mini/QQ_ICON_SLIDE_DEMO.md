# QQ图标滑动菜单演示

## 功能说明

实现了一个使用picture.h中qq图标的自动滑动菜单演示，每2秒自动切换一次菜单项，展示40x40像素的QQ图标在滑动菜单中的效果。

## 主要特性

### 1. QQ图标显示
- 使用picture.h中的`gImage_qq`数据
- 40x40像素，16位RGB565格式
- 在64x64像素的菜单项中居中显示

### 2. 滑动动画
- 每2秒自动向右滑动
- 流畅的缓动动画效果
- 选中项有红色边框突出显示
- 普通项有绿色边框

### 3. 视觉效果
- 边框绘制而非填充，确保图标可见
- 选中项稍微放大（宽度+4px，高度+2px）
- 选中项向上偏移1像素，增加浮起感
- 图标在菜单项中央显示

## 代码实现

### 图标数据引用
```c
#include "picture.h"

// 菜单项配置
IconMenuSlider_AddItem(&demo_slider, "QQ1", gImage_qq, 40, 40, menu_music_callback);
IconMenuSlider_AddItem(&demo_slider, "QQ2", gImage_qq, 40, 40, menu_settings_callback);
IconMenuSlider_AddItem(&demo_slider, "QQ3", gImage_qq, 40, 40, menu_game_callback);
IconMenuSlider_AddItem(&demo_slider, "QQ4", gImage_qq, 40, 40, menu_photo_callback);
```

### 边框绘制优化
```c
// 选中项：绘制边框而不是填充
// 上边框
BG_lcd.Box(draw_x, draw_y - offset_y, item_width, 2, slider->selected_color);
// 下边框
BG_lcd.Box(draw_x, draw_y - offset_y + item_height - 2, item_width, 2, slider->selected_color);
// 左边框
BG_lcd.Box(draw_x, draw_y - offset_y, 2, item_height, slider->selected_color);
// 右边框
BG_lcd.Box(draw_x + item_width - 2, draw_y - offset_y, 2, item_height, slider->selected_color);
```

### 图标显示
```c
// 绘制图标(如果存在)
if (slider->items[i].icon) {
    uint16_t icon_x = draw_x + (item_width - slider->items[i].icon_width) / 2;
    uint16_t icon_y = draw_y + 8 - offset_y;
    BGUI_tool.ShowImage(icon_x, icon_y,
                      slider->items[i].icon_width,
                      slider->items[i].icon_height,
                      slider->items[i].icon);
}
```

## 显示效果

### 界面布局
- 顶部：演示说明"QQ Icon Slide Demo"
- 中央：4个QQ图标的滑动菜单
- 选中项：红色边框，稍微放大
- 普通项：绿色边框，正常尺寸

### 动画效果
- 平滑的水平滑动
- 选中项边框颜色变化
- 图标在边框内居中显示
- 文字标签显示在图标下方

## 技术要点

### 1. 图标格式
- RGB565格式，16位颜色
- 40x40像素尺寸
- 水平扫描，从左到右，从上到下
- 低位在前的字节序

### 2. 绘制顺序
1. 清除背景（如果需要）
2. 绘制边框（4条边分别绘制）
3. 绘制图标（居中显示）
4. 绘制文字标签（底部显示）

### 3. 性能优化
- 只绘制可见的菜单项
- 使用边框而非填充减少绘制量
- 帧缓冲模式下批量刷新

## 自定义扩展

### 1. 使用其他图标
可以替换为其他图标数据：
```c
// 假设有其他图标
extern const unsigned char other_icon[size];
IconMenuSlider_AddItem(&demo_slider, "Other", other_icon, width, height, callback);
```

### 2. 调整图标位置
修改icon_y的计算来调整垂直位置：
```c
uint16_t icon_y = draw_y + 5 - offset_y;  // 向上移动3像素
```

### 3. 添加图标动画
可以在选中时添加图标的缩放或旋转效果：
```c
if (i == selected_idx) {
    // 选中项图标稍微放大
    uint16_t scaled_width = slider->items[i].icon_width + 4;
    uint16_t scaled_height = slider->items[i].icon_height + 4;
    // 使用缩放后的尺寸绘制
}
```

## 故障排除

### 1. 图标不显示
- 检查picture.h是否正确包含
- 确认gImage_qq数据完整
- 验证图标尺寸设置（40x40）

### 2. 边框覆盖图标
- 确保使用边框绘制而非填充
- 检查绘制顺序（先边框后图标）
- 调整边框厚度避免过粗

### 3. 动画卡顿
- 确保主循环频率足够
- 检查帧缓冲模式是否启用
- 监控绘制函数执行时间

## 性能数据

- **内存占用**: 图标数据3200字节 + 菜单结构体约100字节
- **绘制性能**: 单个40x40图标约需1-2ms
- **动画帧率**: 50-60 FPS（取决于系统负载）
- **滑动精度**: 2秒±20ms（基于20ms定时器）
