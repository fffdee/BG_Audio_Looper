#ifndef __MENU_SLIDER_DEMO_CONFIG_H__
#define __MENU_SLIDER_DEMO_CONFIG_H__

// 滑动菜单演示模块配置
#define ENABLE_MENU_SLIDER_DEMO    1    // 1=启用演示, 0=禁用演示

#if ENABLE_MENU_SLIDER_DEMO

// 演示模式配置
#define AUTO_SLIDE_INTERVAL_MS     2000 // 自动滑动间隔（毫秒）
#define AUTO_SLIDE_TIMER_TICKS     100  // 自动滑动计时器节拍（2000ms / 20ms = 100）
#define DEMO_AUTO_START           1     // 1=自动开始演示, 0=手动启动
#define DEMO_SHOW_INSTRUCTIONS    1     // 1=显示演示说明, 0=直接开始

// 菜单演示配置
#define DEMO_MENU_CENTER_X        80    // 菜单中心X坐标
#define DEMO_MENU_START_Y         50    // 菜单起始Y坐标
#define DEMO_SELECTED_COLOR       0xFFFF // 选中项颜色（白色）
#define DEMO_NORMAL_COLOR         0x07E0 // 普通项颜色（绿色）

// QQ图标配置
#define USE_QQ_ICON               1     // 1=使用QQ图标, 0=不使用图标
#define QQ_ICON_WIDTH             40    // QQ图标宽度
#define QQ_ICON_HEIGHT            40    // QQ图标高度

#endif // ENABLE_MENU_SLIDER_DEMO

#endif // __MENU_SLIDER_DEMO_CONFIG_H__
