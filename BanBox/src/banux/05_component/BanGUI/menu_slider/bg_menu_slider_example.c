#include "bg_menu_slider.h"
#include "../base_func/gui_tool.h"

// 示例：如何使用BG_MenuSlider控件
// 这个文件展示了控件的基本用法，可以作为参考

// 示例回调函数
void menu_callback_1(void) {
    // 菜单项1被选中时执行的操作
}

void menu_callback_2(void) {
    // 菜单项2被选中时执行的操作
}

void menu_callback_3(void) {
    // 菜单项3被选中时执行的操作
}

void menu_callback_4(void) {
    // 菜单项4被选中时执行的操作
}

// 示例图标数据（实际项目中应该是真实的图标数据）
extern const uint8_t icon_play[];
extern const uint8_t icon_pause[];
extern const uint8_t icon_stop[];
extern const uint8_t icon_settings[];

/**
 * @brief 创建菜单滑动控件示例
 * @return 创建的控件实例
 */
BG_MenuSlider create_example_menu_slider(void) {
    // 定义菜单项表
    static BG_MenuSlider_Item menu_items[] = {
        BG_MENU_SLIDER_ITEM("Play", icon_play, 32, 32, menu_callback_1, 1),
        BG_MENU_SLIDER_ITEM("Pause", icon_pause, 32, 32, menu_callback_2, 2),
        BG_MENU_SLIDER_ITEM("Stop", icon_stop, 32, 32, menu_callback_3, 3),
        BG_MENU_SLIDER_ITEM("Settings", icon_settings, 32, 32, menu_callback_4, 4),
    };
    
    static BG_MenuSlider_Table menu_table = BG_MENU_SLIDER_TABLE(menu_items);
    
    // 创建控件
    BG_MenuSlider slider = BG_MenuSlider_Create(
        &BG_MENU_SLIDER_REGION_CENTER,  // 使用预定义的中心区域
        &BG_MENU_SLIDER_STYLE_DEFAULT,  // 使用默认样式
        &menu_table                     // 传入菜单项表
    );
    
    // 设置动画配置
    slider.SetAnimation(&slider, &BG_MENU_SLIDER_ANIM_SMOOTH);
    
    // 设置帧缓冲配置（如果需要）
    slider.SetFrameBuffer(&slider, &BG_MENU_SLIDER_FB_DIRTY_ONLY);
    
    return slider;
}

/**
 * @brief 更简单的文字菜单示例
 * @return 创建的控件实例
 */
BG_MenuSlider create_text_menu_slider(void) {
    // 定义文字菜单项
    static BG_MenuSlider_Item text_items[] = {
        BG_MENU_SLIDER_TEXT_ITEM("Home", menu_callback_1),
        BG_MENU_SLIDER_TEXT_ITEM("Music", menu_callback_2),
        BG_MENU_SLIDER_TEXT_ITEM("Radio", menu_callback_3),
        BG_MENU_SLIDER_TEXT_ITEM("Settings", menu_callback_4),
    };
    
    static BG_MenuSlider_Table text_table = BG_MENU_SLIDER_TABLE(text_items);
    
    // 创建文字菜单
    BG_MenuSlider slider = BG_MenuSlider_Create(
        &BG_MENU_SLIDER_REGION_TOP,     // 使用顶部区域
        &BG_MENU_SLIDER_STYLE_MINIMAL,  // 使用简约样式
        &text_table                     // 传入菜单项表
    );
    
    // 启用自动滑动
    slider.SetAnimation(&slider, &BG_MENU_SLIDER_ANIM_AUTO);
    slider.StartAutoSlide(&slider);
    
    return slider;
}

/**
 * @brief 在主循环中使用控件的示例
 * @param slider 控件指针
 * @param delta_ms 距离上次调用的时间间隔(ms)
 */
void update_menu_slider_in_main_loop(BG_MenuSlider* slider, uint32_t delta_ms) {
    if (!slider || !slider->IsInitialized(slider)) {
        return;
    }
    
    // 1. 更新动画和自动滑动
    slider->Update(slider);
    slider->AutoSlideUpdate(slider, delta_ms);
    
    // 2. 绘制控件（只有需要重绘时才会真正绘制）
    slider->Draw(slider);
    
    // 3. 刷新到屏幕（帧缓冲模式下）
    slider->Refresh(slider);
}

/**
 * @brief 处理用户输入示例
 * @param slider 控件指针
 * @param input 输入类型（0=左滑, 1=右滑, 2=选择）
 */
void handle_menu_input(BG_MenuSlider* slider, uint8_t input) {
    if (!slider || !slider->IsInitialized(slider)) {
        return;
    }
    
    switch (input) {
        case 0: // 左滑
            slider->SlideLeft(slider);
            break;
            
        case 1: // 右滑
            slider->SlideRight(slider);
            break;
            
        case 2: // 选择当前项
            slider->Select(slider);
            break;
    }
}

/**
 * @brief 动态添加菜单项示例
 * @param slider 控件指针
 */
void add_dynamic_menu_items(BG_MenuSlider* slider) {
    if (!slider || !slider->IsInitialized(slider)) {
        return;
    }
    
    // 动态添加新菜单项
    BG_MenuSlider_Item new_item = BG_MENU_SLIDER_TEXT_ITEM("New Item", menu_callback_1);
    
    if (slider->AddItem(slider, &new_item)) {
        // 添加成功，立即重绘
        slider->Draw(slider);
        slider->Refresh(slider);
    }
}

/**
 * @brief 控件生命周期管理示例
 */
void menu_slider_lifecycle_example(void) {
    int i;
    
    // 1. 创建控件
    BG_MenuSlider slider = create_example_menu_slider();
    
    // 2. 在主循环中使用
    uint32_t last_time = 0;
    for (i = 0; i < 1000; i++) { /* 模拟主循环 */
        uint32_t current_time = i * 10; /* 假设每10ms一次循环 */
        uint32_t delta_ms = current_time - last_time;
        
        update_menu_slider_in_main_loop(&slider, delta_ms);
        
        /* 模拟用户输入 */
        if (i == 100) handle_menu_input(&slider, 1); /* 右滑 */
        if (i == 200) handle_menu_input(&slider, 2); /* 选择 */
        if (i == 300) handle_menu_input(&slider, 0); /* 左滑 */
        
        last_time = current_time;
    }
    
    // 3. 销毁控件
    BG_MenuSlider_DeInit(&slider);
}
