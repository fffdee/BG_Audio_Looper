#include "menu_slider_demo.h"

#if ENABLE_MENU_SLIDER_DEMO

#include "framebuffer.h"
#include "bg_lcd.h"
#include "gui_tool.h"
#include "picture.h"
#include <string.h>

// 全局演示实例
static MenuSliderDemo g_demo;

// 菜单项回调函数实现
void MenuSliderDemo_MusicCallback(void) {
    MenuSliderDemo_Stop(); // 停止自动演示
    FrameBuffer_Clear(0x001F); // 蓝色背景
    BGUI_tool.ShowString(50, 60, (uint8_t*)"Music Selected", 0xFFFF);
    FrameBuffer_Flush();
}

void MenuSliderDemo_SettingsCallback(void) {
    MenuSliderDemo_Stop(); // 停止自动演示
    FrameBuffer_Clear(0x07E0); // 绿色背景
    BGUI_tool.ShowString(50, 60, (uint8_t*)"Settings Selected", 0xFFFF);
    FrameBuffer_Flush();
}

void MenuSliderDemo_GameCallback(void) {
    MenuSliderDemo_Stop(); // 停止自动演示
    FrameBuffer_Clear(0xF800); // 红色背景
    BGUI_tool.ShowString(50, 60, (uint8_t*)"Game Selected", 0xFFFF);
    FrameBuffer_Flush();
}

void MenuSliderDemo_PhotoCallback(void) {
    MenuSliderDemo_Stop(); // 停止自动演示
    FrameBuffer_Clear(0xFFE0); // 黄色背景
    BGUI_tool.ShowString(50, 60, (uint8_t*)"Photo Selected", 0xFFFF);
    FrameBuffer_Flush();
}

void MenuSliderDemo_ToolsCallback(void) {
    MenuSliderDemo_Stop(); // 停止自动演示
    FrameBuffer_Clear(0xF81F); // 紫色背景
    BGUI_tool.ShowString(50, 60, (uint8_t*)"Tools Selected", 0xFFFF);
    FrameBuffer_Flush();
}

// 初始化演示模块
void MenuSliderDemo_Init(void) {
    memset(&g_demo, 0, sizeof(MenuSliderDemo));
    
    // 初始化菜单滑块
    IconMenuSlider_Init(&g_demo.slider, 
                       DEMO_MENU_CENTER_X, 
                       DEMO_MENU_START_Y, 
                       DEMO_SELECTED_COLOR, 
                       DEMO_NORMAL_COLOR);
    
#if USE_QQ_ICON
    // 添加带QQ图标的菜单项
    IconMenuSlider_AddItem(&g_demo.slider, "Music", gImage_qq, QQ_ICON_WIDTH, QQ_ICON_HEIGHT, MenuSliderDemo_MusicCallback);
    IconMenuSlider_AddItem(&g_demo.slider, "Settings", gImage_qq, QQ_ICON_WIDTH, QQ_ICON_HEIGHT, MenuSliderDemo_SettingsCallback);
    IconMenuSlider_AddItem(&g_demo.slider, "Games", gImage_qq, QQ_ICON_WIDTH, QQ_ICON_HEIGHT, MenuSliderDemo_GameCallback);
    IconMenuSlider_AddItem(&g_demo.slider, "Photo", gImage_qq, QQ_ICON_WIDTH, QQ_ICON_HEIGHT, MenuSliderDemo_PhotoCallback);
    IconMenuSlider_AddItem(&g_demo.slider, "Tools", gImage_qq, QQ_ICON_WIDTH, QQ_ICON_HEIGHT, MenuSliderDemo_ToolsCallback);
#else
    // 添加纯文字菜单项
    IconMenuSlider_AddItem(&g_demo.slider, "Music", NULL, 0, 0, MenuSliderDemo_MusicCallback);
    IconMenuSlider_AddItem(&g_demo.slider, "Settings", NULL, 0, 0, MenuSliderDemo_SettingsCallback);
    IconMenuSlider_AddItem(&g_demo.slider, "Games", NULL, 0, 0, MenuSliderDemo_GameCallback);
    IconMenuSlider_AddItem(&g_demo.slider, "Photo", NULL, 0, 0, MenuSliderDemo_PhotoCallback);
    IconMenuSlider_AddItem(&g_demo.slider, "Tools", NULL, 0, 0, MenuSliderDemo_ToolsCallback);
#endif

    g_demo.auto_slide_timer = 0;
    g_demo.demo_enabled = DEMO_AUTO_START;
    g_demo.demo_active = 0;
    g_demo.show_instructions = DEMO_SHOW_INSTRUCTIONS;
}

// 开始演示
void MenuSliderDemo_Start(void) {
    g_demo.demo_enabled = 1;
    g_demo.demo_active = 1;
    g_demo.auto_slide_timer = 0;
    
    if (g_demo.show_instructions) {
        MenuSliderDemo_ShowInstructions();
    }
}

// 停止演示
void MenuSliderDemo_Stop(void) {
    g_demo.demo_enabled = 0;
    g_demo.demo_active = 0;
}

// 切换演示模式
void MenuSliderDemo_Toggle(void) {
    if (g_demo.demo_enabled) {
        MenuSliderDemo_Stop();
    } else {
        MenuSliderDemo_Start();
    }
}

// 显示演示说明
void MenuSliderDemo_ShowInstructions(void) {
    FrameBuffer_Clear(0x0000);
    BGUI_tool.ShowString(10, 10, (uint8_t*)"QQ Icon Slide Demo", 0xFFFF);
    BGUI_tool.ShowString(15, 25, (uint8_t*)"Auto slide every 2s", 0x07E0);
    BGUI_tool.ShowString(20, 40, (uint8_t*)"FrameBuffer Mode", 0xF81F);
    FrameBuffer_Flush();
}

// 更新演示逻辑
void MenuSliderDemo_Update(void) {
    if (!g_demo.demo_enabled || !g_demo.demo_active) {
        return;
    }
    
    // 自动滑动逻辑 (基于20ms计时器中断)
    g_demo.auto_slide_timer++;
    if (g_demo.auto_slide_timer >= AUTO_SLIDE_TIMER_TICKS) { // 2000ms / 20ms = 100次
        g_demo.auto_slide_timer = 0;
        
        // 执行自动滑动 - 向右滑动，循环
        IconMenuSlider_Right(&g_demo.slider);
    }
    
    // 更新菜单动画
    IconMenuSlider_Update(&g_demo.slider);
    
    // 绘制菜单
    IconMenuSlider_Draw(&g_demo.slider);
    
    // 刷新帧缓冲到屏幕（脏区域优化）
    FrameBuffer_FlushDirty();
}

// 检查演示是否激活
uint8_t MenuSliderDemo_IsActive(void) {
    return g_demo.demo_enabled && g_demo.demo_active;
}

#endif // ENABLE_MENU_SLIDER_DEMO
