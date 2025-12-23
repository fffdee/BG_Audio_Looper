#ifndef __MENU_SLIDER_DEMO_H__
#define __MENU_SLIDER_DEMO_H__

#include "menu_slider_demo_config.h"

#if ENABLE_MENU_SLIDER_DEMO

#include <stdint.h>
#include "menu_slider.h"

// 滑动菜单演示状态
typedef struct {
    IconMenuSlider slider;          // 菜单滑块实例
    uint32_t auto_slide_timer;      // 自动滑动计时器
    uint8_t demo_enabled;           // 演示模式开关
    uint8_t demo_active;            // 演示是否激活
    uint8_t show_instructions;      // 是否显示说明
} MenuSliderDemo;

// 演示模块接口
void MenuSliderDemo_Init(void);
void MenuSliderDemo_Start(void);
void MenuSliderDemo_Stop(void);
void MenuSliderDemo_Update(void);
uint8_t MenuSliderDemo_IsActive(void);

// 菜单项回调函数
void MenuSliderDemo_MusicCallback(void);
void MenuSliderDemo_SettingsCallback(void);
void MenuSliderDemo_GameCallback(void);
void MenuSliderDemo_PhotoCallback(void);
void MenuSliderDemo_ToolsCallback(void);

// 演示控制
void MenuSliderDemo_Toggle(void);
void MenuSliderDemo_ShowInstructions(void);

#endif // ENABLE_MENU_SLIDER_DEMO

#endif // __MENU_SLIDER_DEMO_H__
