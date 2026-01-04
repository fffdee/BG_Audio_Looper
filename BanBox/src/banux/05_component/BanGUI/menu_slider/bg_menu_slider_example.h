#ifndef _BG_MENU_SLIDER_EXAMPLE_H__
#define _BG_MENU_SLIDER_EXAMPLE_H__

#include "bg_menu_slider.h"

/**
 * @file bg_menu_slider_example.h
 * @brief BG_MenuSlider控件使用示例
 * 
 * 这个文件展示了如何使用BG_MenuSlider控件的各种功能：
 * - 创建基本的图标菜单
 * - 创建文字菜单
 * - 在主循环中更新控件
 * - 处理用户输入
 * - 动态添加菜单项
 * - 控件生命周期管理
 */

// 示例函数声明
BG_MenuSlider create_example_menu_slider(void);
BG_MenuSlider create_text_menu_slider(void);
void update_menu_slider_in_main_loop(BG_MenuSlider* slider, uint32_t delta_ms);
void handle_menu_input(BG_MenuSlider* slider, uint8_t input);
void add_dynamic_menu_items(BG_MenuSlider* slider);
void menu_slider_lifecycle_example(void);

#endif // _BG_MENU_SLIDER_EXAMPLE_H__
