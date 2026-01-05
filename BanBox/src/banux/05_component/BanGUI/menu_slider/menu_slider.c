#include "menu_slider.h"
#include "framebuffer.h"
#include <string.h>

// 使用帧缓冲的高效绘制填充矩形
static void draw_filled_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    FrameBuffer_FillMenuRect(x, y, width, height, color);
}

// 使用帧缓冲的高效绘制矩形边框
static void draw_rect_border(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color, uint8_t thickness) {
    FrameBuffer_DrawMenuBorder(x, y, width, height, color, thickness);
}

// 使用帧缓冲的高效清除指定区域
static void clear_menu_area(uint16_t center_x, uint16_t start_y, uint8_t item_count) {
    // 计算需要清除的区域 - 稍微扩大确保清除干净
    uint16_t total_width = (item_count + 1) * MENU_ITEM_WIDTH; 
    uint16_t clear_x = (center_x > total_width/2) ? (center_x - total_width/2) : 0;
    uint16_t clear_width = (clear_x + total_width > LCD_WIDTH) ? (LCD_WIDTH - clear_x) : total_width;
    
    // 使用帧缓冲清除背景
    FrameBuffer_FillRect(clear_x, start_y, clear_width, MENU_ITEM_HEIGHT, 0x0000);
}

// ��ʼ��ͼ��˵�
void IconMenuSlider_Init(IconMenuSlider* slider,
                        uint16_t center_x,
                        uint16_t start_y,
                        uint16_t selected_color,
                        uint16_t normal_color) {
    memset(slider, 0, sizeof(IconMenuSlider));
    slider->center_x = center_x;      // ѡ���������ʾ��X����
    slider->start_y = start_y;        // �˵���ʼY����
    slider->selected_color = selected_color;
    slider->normal_color = normal_color;
}

// ���Ӳ˵���
uint8_t IconMenuSlider_AddItem(IconMenuSlider* slider,
                              const char* name,
                              const unsigned char* icon,
                              uint16_t icon_width,
                              uint16_t icon_height,
                              void (*callback)(void)) {
    if (slider->item_count >= MAX_MENU_ITEMS) {
        return 0; // �����������
    }
    slider->items[slider->item_count].name = name;
    slider->items[slider->item_count].icon = icon;
    slider->items[slider->item_count].icon_width = icon_width;
    slider->items[slider->item_count].icon_height = icon_height;
    slider->items[slider->item_count].callback = callback;
    slider->item_count++;

    // ��ʼƫ��������(Ĭ��ѡ�е�0��)
    if (slider->item_count == 1) {
        slider->target_offset = slider->center_x - (MENU_ITEM_WIDTH / 2);
        slider->current_offset = slider->target_offset;
    }
    return 1;
}

// ����Ŀ��ƫ����(ȷ��ѡ�������)
static void calculate_target_offset(IconMenuSlider* slider) {
    // �������У�ÿ������ MENU_ITEM_WIDTH��ѡ���������ʾ
    slider->target_offset = slider->center_x -
                          (MENU_ITEM_WIDTH / 2) -
                          (slider->current_idx * MENU_ITEM_WIDTH);
}

// ����(ѭ��)
void IconMenuSlider_Left(IconMenuSlider* slider) {
    if (slider->is_sliding || slider->item_count <= 1) {
        return;
    }
    // ѭ���߼�������ʱ������1��С��0��ص����һ��
    slider->current_idx = (slider->current_idx - 1 + slider->item_count) % slider->item_count;
    calculate_target_offset(slider);
    slider->is_sliding = 1;
}

// ����(ѭ��)
void IconMenuSlider_Right(IconMenuSlider* slider) {
    if (slider->is_sliding || slider->item_count <= 1) {
        return;
    }
    // ѭ���߼�������ʱ������1��������ص���һ��
    slider->current_idx = (slider->current_idx + 1) % slider->item_count;
    calculate_target_offset(slider);
    slider->is_sliding = 1;
}

// ȷ��ѡ����
void IconMenuSlider_Enter(IconMenuSlider* slider) {
    if (slider->items[slider->current_idx].callback) {
        slider->items[slider->current_idx].callback();
    }
}

// 更新动画状态 - 流畅缓动算法
void IconMenuSlider_Update(IconMenuSlider* slider) {
    if (!slider->is_sliding) {
        return;
    }

    // 计算距离目标的差值
    int16_t diff = slider->target_offset - slider->current_offset;
    
    // 使用缓动算法实现流畅动画
    if (diff != 0) {
        // 缓动因子：距离越近，速度越慢
        float easing_factor = 0.2f; // 缓动强度（0.1-0.3之间）
        int16_t move_distance = (int16_t)(diff * easing_factor);
        
        // 确保最小移动距离，避免动画停滞
        if (move_distance == 0) {
            move_distance = (diff > 0) ? 1 : -1;
        }
        
        // 当距离很小时，直接跳到目标位置
        int16_t abs_diff = (diff > 0) ? diff : -diff;
        if (abs_diff <= 2) {
            slider->current_offset = slider->target_offset;
            slider->is_sliding = 0;
            return;
        }
        
        // 应用缓动移动
        slider->current_offset += move_distance;
        
        // 防止超调
        if ((diff > 0 && slider->current_offset >= slider->target_offset) ||
            (diff < 0 && slider->current_offset <= slider->target_offset)) {
            slider->current_offset = slider->target_offset;
            slider->is_sliding = 0; // 动画完成
        }
    } else {
        slider->is_sliding = 0; // 动画完成
    }
}

// 绘制菜单 - 流畅动画版本
void IconMenuSlider_Draw(IconMenuSlider* slider) {
	uint8_t i;
    uint16_t draw_x, draw_y = slider->start_y;
    uint8_t selected_idx = slider->current_idx;
    
#ifdef USE_FRAME_BUFFER
    // 帧缓冲模式：先清除菜单区域，然后绘制所有菜单项
    IconMenuSlider_ClearArea(slider);
#endif
    
    for (i = 0; i < slider->item_count; i++) {
        // 计算当前屏幕上的X坐标
        draw_x = slider->current_offset + (i * MENU_ITEM_WIDTH);

        // 只绘制可见的菜单项（扩大可见范围以支持流畅动画）
        if (draw_x + MENU_ITEM_WIDTH < -10 || draw_x > LCD_WIDTH + 10) {
            continue;
        }

        // 动态缩放效果 - 选中项稍大，其他项正常
        uint16_t item_width = MENU_ITEM_WIDTH;
        uint16_t item_height = MENU_ITEM_HEIGHT;
        uint16_t offset_y = 0;
        
        if (i == selected_idx) {
            // 选中项：稍微放大效果
            item_width += 4;
            item_height += 2;
            offset_y = 1; // 向上偏移1像素
            draw_x -= 2;  // 左偏移2像素居中
        }

        // 绘制菜单项背景（使用帧缓冲绘制边框，为图标留出空间）
        if (i == selected_idx) {
            // 选中项：绘制边框而不是填充
            // 上边框
            FrameBuffer_FillRect(draw_x, draw_y - offset_y, item_width, 2, slider->selected_color);
            // 下边框
            FrameBuffer_FillRect(draw_x, draw_y - offset_y + item_height - 2, item_width, 2, slider->selected_color);
            // 左边框
            FrameBuffer_FillRect(draw_x, draw_y - offset_y, 2, item_height, slider->selected_color);
            // 右边框
            FrameBuffer_FillRect(draw_x + item_width - 2, draw_y - offset_y, 2, item_height, slider->selected_color);
        } else {
            // 普通项：绘制边框
            // 上边框
            FrameBuffer_FillRect(draw_x, draw_y, item_width, 1, slider->normal_color);
            // 下边框
            FrameBuffer_FillRect(draw_x, draw_y + item_height - 1, item_width, 1, slider->normal_color);
            // 左边框
            FrameBuffer_FillRect(draw_x, draw_y, 1, item_height, slider->normal_color);
            // 右边框
            FrameBuffer_FillRect(draw_x + item_width - 1, draw_y, 1, item_height, slider->normal_color);
        }

        // 绘制图标(如果存在) - 使用帧缓冲
        if (slider->items[i].icon) {
            uint16_t icon_x = draw_x + (item_width - slider->items[i].icon_width) / 2;
            uint16_t icon_y = draw_y + 8 - offset_y;
            FrameBuffer_DrawImage(icon_x, icon_y,
                                slider->items[i].icon_width,
                                slider->items[i].icon_height,
                                slider->items[i].icon);
        }

        // 绘制文本 (暂时保持原有接口，因为字体绘制较复杂)
        if (slider->items[i].name) {
            uint16_t text_x = draw_x + (item_width - strlen(slider->items[i].name) * 8) / 2;
            uint16_t text_y = draw_y + item_height - 12 - offset_y;
            uint16_t text_color = (i == selected_idx) ? 0xFFFF : 0xC618;
            // 注意：文字绘制暂时保持原有接口，后续可优化为直接写入帧缓冲
            BGUI_tool.ShowString(text_x, text_y,
                               (uint8_t*)slider->items[i].name,
                               text_color);
        }
    }
}

// 高效清除菜单区域 - 使用帧缓冲优化
void IconMenuSlider_ClearArea(IconMenuSlider* slider) {
    // 使用帧缓冲高效清除菜单区域
    uint16_t margin = 20; // 额外清除边距
    uint16_t start_x = (slider->center_x > (slider->item_count * MENU_ITEM_WIDTH) / 2 + margin) ? 
                       (slider->center_x - (slider->item_count * MENU_ITEM_WIDTH) / 2 - margin) : 0;
    uint16_t total_width = slider->item_count * MENU_ITEM_WIDTH + 2 * margin;
    if (start_x + total_width > LCD_WIDTH) total_width = LCD_WIDTH - start_x;
    
    // 清除时扩大高度范围
    uint16_t start_y = (slider->start_y > 2) ? (slider->start_y - 2) : 0;
    uint16_t total_height = MENU_ITEM_HEIGHT + 4;
    if (start_y + total_height > LCD_HEIGHT) total_height = LCD_HEIGHT - start_y;
    
    // 一次性清除整个菜单区域
    FrameBuffer_FillRect(start_x, start_y, total_width, total_height, 0x0000);
}

// 快速跳转模式 - 无动画，直接跳转
void IconMenuSlider_SetInstant(IconMenuSlider* slider, uint8_t target_idx) {
    if (target_idx >= slider->item_count) return;
    
    slider->current_idx = target_idx;
    slider->target_offset = slider->center_x - (MENU_ITEM_WIDTH / 2) - (slider->current_idx * MENU_ITEM_WIDTH);
    slider->current_offset = slider->target_offset;
    slider->is_sliding = 0;
}

// 添加弹性动画效果
void IconMenuSlider_SetElasticMode(IconMenuSlider* slider, uint8_t enable) {
    // 此函数可以用来启用/禁用弹性动画效果
    // 实现预留接口，可以在后续扩展
    (void)slider; // 避免未使用警告
    (void)enable;
}

// 检查动画是否正在进行
uint8_t IconMenuSlider_IsAnimating(IconMenuSlider* slider) {
    return slider->is_sliding;
}

// 强制停止动画
void IconMenuSlider_StopAnimation(IconMenuSlider* slider) {
    slider->current_offset = slider->target_offset;
    slider->is_sliding = 0;
}
