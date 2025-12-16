#include "bg_menu_slider.h"
#include "../base_func/gui_tool.h"
#include "../../hardware/BG_Lcd/bg_lcd.h"
#include <string.h>

// 外部变量声明
#ifdef USE_FRAME_BUFFER
extern uint16_t frame_buffer[];
extern uint8_t frame_buffer_dirty;
#endif

//=============================================================================
// 私有函数声明
//=============================================================================
static void BG_MenuSlider_SetRegion(struct BG_MenuSlider* self, const BG_MenuSlider_Region* region);
static void BG_MenuSlider_SetFrameBuffer(struct BG_MenuSlider* self, const BG_MenuSlider_FrameBuffer* fb);
static void BG_MenuSlider_SetStyle(struct BG_MenuSlider* self, const BG_MenuSlider_Style* style);
static void BG_MenuSlider_SetAnimation(struct BG_MenuSlider* self, const BG_MenuSlider_Animation* anim);

// 菜单操作接口
static uint8_t BG_MenuSlider_AddItem(struct BG_MenuSlider* self, const BG_MenuSlider_Item* item);
static void BG_MenuSlider_ClearItems(struct BG_MenuSlider* self);
static void BG_MenuSlider_LoadTable(struct BG_MenuSlider* self, const BG_MenuSlider_Table* table);
static uint8_t BG_MenuSlider_RemoveItem(struct BG_MenuSlider* self, uint8_t index);

// 控制接口
static void BG_MenuSlider_SlideLeft(struct BG_MenuSlider* self);
static void BG_MenuSlider_SlideRight(struct BG_MenuSlider* self);
static void BG_MenuSlider_SlideTo(struct BG_MenuSlider* self, uint8_t index);
static void BG_MenuSlider_Select(struct BG_MenuSlider* self);

// 更新和绘制接口
static void BG_MenuSlider_Update(struct BG_MenuSlider* self);
static void BG_MenuSlider_Draw(struct BG_MenuSlider* self);
static void BG_MenuSlider_Clear(struct BG_MenuSlider* self);
static void BG_MenuSlider_Refresh(struct BG_MenuSlider* self);
static void BG_MenuSlider_RefreshRegion(struct BG_MenuSlider* self, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// 自动滑动接口
static void BG_MenuSlider_StartAutoSlide(struct BG_MenuSlider* self);
static void BG_MenuSlider_StopAutoSlide(struct BG_MenuSlider* self);
static void BG_MenuSlider_ToggleAutoSlide(struct BG_MenuSlider* self);
static void BG_MenuSlider_AutoSlideUpdate(struct BG_MenuSlider* self, uint32_t delta_ms);

// 状态查询接口
static uint8_t BG_MenuSlider_GetSelectedIndex(struct BG_MenuSlider* self);
static const BG_MenuSlider_Item* BG_MenuSlider_GetSelectedItem(struct BG_MenuSlider* self);
static uint8_t BG_MenuSlider_GetItemCount(struct BG_MenuSlider* self);
static uint8_t BG_MenuSlider_IsSliding(struct BG_MenuSlider* self);
static uint8_t BG_MenuSlider_IsAutoSliding(struct BG_MenuSlider* self);
static uint8_t BG_MenuSlider_IsInitialized(struct BG_MenuSlider* self);

//=============================================================================
// 配置接口实现
//=============================================================================

/**
 * @brief 设置显示区域配置
 * @param self 控件指针
 * @param region 区域配置指针
 */
static void BG_MenuSlider_SetRegion(struct BG_MenuSlider* self, const BG_MenuSlider_Region* region) {
    if (!self || !region) {
        return;
    }
    
    // 复制区域配置
    memcpy(&self->region, region, sizeof(BG_MenuSlider_Region));
    
    // 标记需要重绘
    self->data.need_redraw = 1;
    
    // 如果已初始化，重新计算偏移量
    if (self->data.initialized) {
        // 重新计算当前选中项的目标偏移量
        self->data.target_offset = self->region.center_x - 
                                  (self->data.selected_index * self->region.item_width) - 
                                  (self->region.item_width / 2);
        self->data.current_offset = self->data.target_offset;
    }
}

/**
 * @brief 设置帧缓冲配置
 * @param self 控件指针
 * @param fb 帧缓冲配置指针
 */
static void BG_MenuSlider_SetFrameBuffer(struct BG_MenuSlider* self, const BG_MenuSlider_FrameBuffer* fb) {
    if (!self || !fb) {
        return;
    }
    
    // 复制帧缓冲配置
    memcpy(&self->framebuffer, fb, sizeof(BG_MenuSlider_FrameBuffer));
    
    // 验证帧缓冲参数
    if (fb->buffer == NULL) {
        // 如果缓冲区为空，禁用帧缓冲功能
        self->framebuffer.use_shared_buffer = 0;
        return;
    }
    
    // 验证刷新区域是否在缓冲区范围内
    if (fb->refresh_x + fb->refresh_width > fb->buffer_width) {
        self->framebuffer.refresh_width = fb->buffer_width - fb->refresh_x;
    }
    
    if (fb->refresh_y + fb->refresh_height > fb->buffer_height) {
        self->framebuffer.refresh_height = fb->buffer_height - fb->refresh_y;
    }
    
    // 标记需要重绘
    self->data.need_redraw = 1;
}

/**
 * @brief 设置样式配置
 * @param self 控件指针
 * @param style 样式配置指针
 */
static void BG_MenuSlider_SetStyle(struct BG_MenuSlider* self, const BG_MenuSlider_Style* style) {
    if (!self || !style) {
        return;
    }
    
    // 复制样式配置
    memcpy(&self->style, style, sizeof(BG_MenuSlider_Style));
    
    // 验证边框宽度
    if (style->border_width > self->region.item_width / 4 || 
        style->border_width > self->region.item_height / 4) {
        // 边框太宽，限制在项目尺寸的1/4
        self->style.border_width = (self->region.item_width < self->region.item_height) ? 
                                   self->region.item_width / 4 : self->region.item_height / 4;
    }
    
    // 标记需要重绘
    self->data.need_redraw = 1;
}

/**
 * @brief 设置动画配置
 * @param self 控件指针
 * @param anim 动画配置指针
 */
static void BG_MenuSlider_SetAnimation(struct BG_MenuSlider* self, const BG_MenuSlider_Animation* anim) {
    if (!self || !anim) {
        return;
    }
    
    // 保存旧的自动滑动状态
    uint8_t old_auto_slide = self->animation.auto_slide_enable;
    
    // 复制动画配置
    memcpy(&self->animation, anim, sizeof(BG_MenuSlider_Animation));
    
    // 验证动画参数
    if (anim->slide_duration == 0) {
        // 如果持续时间为0，设置为即时切换
        self->animation.slide_duration = 1;
    }
    
    if (anim->auto_slide_interval < 100) {
        // 自动滑动间隔不能太短，最少100ms
        self->animation.auto_slide_interval = 100;
    }
    
    // 如果自动滑动状态改变，重置计时器
    if (old_auto_slide != anim->auto_slide_enable) {
        self->data.auto_slide_timer = 0;
        if (anim->auto_slide_enable) {
            self->data.auto_slide_active = 1;
        } else {
            self->data.auto_slide_active = 0;
        }
    }
    
    // 重置动画状态
    if (self->data.state == BG_MENU_SLIDER_SLIDING) {
        self->data.animation_timer = 0;
    }
}

//=============================================================================
// 菜单操作接口实现
//=============================================================================

/**
 * @brief 添加菜单项
 * @param self 控件指针
 * @param item 菜单项指针
 * @return 成功返回1，失败返回0
 */
static uint8_t BG_MenuSlider_AddItem(struct BG_MenuSlider* self, const BG_MenuSlider_Item* item) {
    if (!self || !item || self->data.item_count >= BG_MENU_SLIDER_MAX_ITEMS) {
        return 0;
    }
    
    // 复制菜单项数据
    memcpy(&self->data.items[self->data.item_count], item, sizeof(BG_MenuSlider_Item));
    self->data.item_count++;
    
    // 如果是第一个项目，设置为选中
    if (self->data.item_count == 1) {
        self->data.selected_index = 0;
        // 重新计算偏移量
        self->data.target_offset = self->region.center_x - (self->region.item_width / 2);
        self->data.current_offset = self->data.target_offset;
    }
    
    // 标记需要重绘
    self->data.need_redraw = 1;
    
    return 1;
}

/**
 * @brief 清除所有菜单项
 * @param self 控件指针
 */
static void BG_MenuSlider_ClearItems(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    // 清零菜单项数据
    memset(self->data.items, 0, sizeof(self->data.items));
    self->data.item_count = 0;
    self->data.selected_index = 0;
    
    // 重置偏移量
    self->data.current_offset = 0;
    self->data.target_offset = 0;
    
    // 重置动画状态
    self->data.state = BG_MENU_SLIDER_IDLE;
    self->data.animation_timer = 0;
    
    // 停止自动滑动
    self->data.auto_slide_active = 0;
    self->data.auto_slide_timer = 0;
    
    // 标记需要重绘
    self->data.need_redraw = 1;
}

/**
 * @brief 加载菜单项表
 * @param self 控件指针
 * @param table 菜单项表指针
 */
static void BG_MenuSlider_LoadTable(struct BG_MenuSlider* self, const BG_MenuSlider_Table* table) {
    if (!self || !table || !table->items || table->item_count == 0) {
        return;
    }
    
    // 先清除现有项目
    BG_MenuSlider_ClearItems(self);
    
    // 限制项目数量
    uint8_t count = (table->item_count > BG_MENU_SLIDER_MAX_ITEMS) ? 
                    BG_MENU_SLIDER_MAX_ITEMS : table->item_count;
    uint8_t i;
    
    // 复制菜单项
    for (i = 0; i < count; i++) {
        memcpy(&self->data.items[i], &table->items[i], sizeof(BG_MenuSlider_Item));
    }
    
    self->data.item_count = count;
    
    // 设置第一个为选中项
    if (count > 0) {
        self->data.selected_index = 0;
        // 计算初始偏移量
        self->data.target_offset = self->region.center_x - (self->region.item_width / 2);
        self->data.current_offset = self->data.target_offset;
    }
    
    // 标记需要重绘
    self->data.need_redraw = 1;
}

/**
 * @brief 移除指定索引的菜单项
 * @param self 控件指针
 * @param index 要移除的项目索引
 * @return 成功返回1，失败返回0
 */
static uint8_t BG_MenuSlider_RemoveItem(struct BG_MenuSlider* self, uint8_t index) {
    uint8_t i;
    if (!self || index >= self->data.item_count || self->data.item_count == 0) {
        return 0;
    }
    
    // 移动后续项目前移
    for (i = index; i < self->data.item_count - 1; i++) {
        memcpy(&self->data.items[i], &self->data.items[i + 1], sizeof(BG_MenuSlider_Item));
    }
    
    // 清除最后一个项目
    memset(&self->data.items[self->data.item_count - 1], 0, sizeof(BG_MenuSlider_Item));
    self->data.item_count--;
    
    // 调整选中索引
    if (self->data.selected_index >= self->data.item_count && self->data.item_count > 0) {
        self->data.selected_index = self->data.item_count - 1;
    } else if (self->data.item_count == 0) {
        self->data.selected_index = 0;
        self->data.current_offset = 0;
        self->data.target_offset = 0;
    }
    
    // 重新计算偏移量
    if (self->data.item_count > 0) {
        self->data.target_offset = self->region.center_x - 
                                  (self->data.selected_index * self->region.item_width) - 
                                  (self->region.item_width / 2);
        self->data.current_offset = self->data.target_offset;
    }
    
    // 标记需要重绘
    self->data.need_redraw = 1;
    
    return 1;
}

//=============================================================================
// 控制接口实现
//=============================================================================

/**
 * @brief 向左滑动
 * @param self 控件指针
 */
static void BG_MenuSlider_SlideLeft(struct BG_MenuSlider* self) {
    if (!self || self->data.item_count == 0) {
        return;
    }
    
    // 计算新的选中索引（向左滑动，索引减少）
    uint8_t new_index;
    if (self->data.selected_index == 0) {
        // 如果已经是第一个，循环到最后一个
        new_index = self->data.item_count - 1;
    } else {
        new_index = self->data.selected_index - 1;
    }
    
    // 调用SlideTo实现滑动
    BG_MenuSlider_SlideTo(self, new_index);
}

/**
 * @brief 向右滑动
 * @param self 控件指针
 */
static void BG_MenuSlider_SlideRight(struct BG_MenuSlider* self) {
    if (!self || self->data.item_count == 0) {
        return;
    }
    
    // 计算新的选中索引（向右滑动，索引增加）
    uint8_t new_index;
    if (self->data.selected_index >= self->data.item_count - 1) {
        // 如果已经是最后一个，循环到第一个
        new_index = 0;
    } else {
        new_index = self->data.selected_index + 1;
    }
    
    // 调用SlideTo实现滑动
    BG_MenuSlider_SlideTo(self, new_index);
}

/**
 * @brief 滑动到指定索引
 * @param self 控件指针
 * @param index 目标索引
 */
static void BG_MenuSlider_SlideTo(struct BG_MenuSlider* self, uint8_t index) {
    if (!self || index >= self->data.item_count || self->data.item_count == 0) {
        return;
    }
    
    // 如果已经是目标索引，直接返回
    if (index == self->data.selected_index) {
        return;
    }
    
    // 更新选中索引
    self->data.selected_index = index;
    
    // 计算目标偏移量
    self->data.target_offset = self->region.center_x - 
                              (index * self->region.item_width) - 
                              (self->region.item_width / 2);
    
    // 如果动画持续时间为0或1，立即完成滑动
    if (self->animation.slide_duration <= 1) {
        self->data.current_offset = self->data.target_offset;
        self->data.state = BG_MENU_SLIDER_IDLE;
        self->data.animation_timer = 0;
    } else {
        // 开始滑动动画
        self->data.state = BG_MENU_SLIDER_SLIDING;
        self->data.animation_timer = 0;
    }
    
    // 标记需要重绘
    self->data.need_redraw = 1;
}

/**
 * @brief 选中当前项（调用回调函数）
 * @param self 控件指针
 */
static void BG_MenuSlider_Select(struct BG_MenuSlider* self) {
    if (!self || self->data.item_count == 0 || 
        self->data.selected_index >= self->data.item_count) {
        return;
    }
    
    // 获取当前选中项
    const BG_MenuSlider_Item* selected_item = &self->data.items[self->data.selected_index];
    
    // 如果有回调函数，执行它
    if (selected_item->callback) {
        selected_item->callback();
    }
}

//=============================================================================
// 更新和绘制接口实现
//=============================================================================

/**
 * @brief 更新控件状态（动画、自动滑动等）
 * @param self 控件指针
 */
static void BG_MenuSlider_Update(struct BG_MenuSlider* self) {
    if (!self || !self->data.initialized) {
        return;
    }
    
    // 更新滑动动画
    if (self->data.state == BG_MENU_SLIDER_SLIDING) {
        self->data.animation_timer++;
        
        if (self->data.animation_timer >= self->animation.slide_duration) {
            // 动画完成
            self->data.current_offset = self->data.target_offset;
            self->data.state = BG_MENU_SLIDER_IDLE;
            self->data.animation_timer = 0;
            self->data.need_redraw = 1;
        } else {
            // 计算当前偏移量（缓动效果）
            float progress = (float)self->data.animation_timer / self->animation.slide_duration;
            int16_t start_offset = self->data.current_offset;
            int16_t diff = self->data.target_offset - start_offset;
            
            // 应用缓动函数
            switch (self->animation.ease_type) {
                case BG_MENU_SLIDER_EASE_IN_OUT:
                    progress = progress < 0.5f ? 2 * progress * progress : 1 - 2 * (1 - progress) * (1 - progress);
                    break;
                case BG_MENU_SLIDER_EASE_OUT:
                    progress = 1 - (1 - progress) * (1 - progress);
                    break;
                case BG_MENU_SLIDER_EASE_LINEAR:
                default:
                    // 线性，不需要调整
                    break;
            }
            
            self->data.current_offset = start_offset + (int16_t)(diff * progress);
            self->data.need_redraw = 1;
        }
    }
}

/**
 * @brief 绘制控件
 * @param self 控件指针
 */
static void BG_MenuSlider_Draw(struct BG_MenuSlider* self) {
    if (!self || !self->data.initialized || self->data.item_count == 0) {
        return;
    }
    
    // 如果不需要重绘，直接返回
    if (!self->data.need_redraw) {
        return;
    }
    
    // 绘制背景（如果需要）
    if (self->style.show_background) {
        BG_MenuSlider_Clear(self);
    }
    
    // 绘制所有可见的菜单项
    {
        uint8_t i;
        for (i = 0; i < self->data.item_count; i++) {
            // 计算当前项目的X坐标
            int16_t item_x = self->data.current_offset + (i * self->region.item_width);
            
            // 只绘制可见的项目（包含缓冲区域）
            if (item_x + self->region.item_width < -20 || item_x > self->region.width + 20) {
                continue;
            }
        
        // 确定项目尺寸（选中项可能有缩放效果）
        uint16_t item_width = self->region.item_width;
        uint16_t item_height = self->region.item_height;
        int16_t item_y = self->region.y;
        
        if (i == self->data.selected_index && self->animation.scale_effect) {
            // 选中项稍微放大
            item_width += 4;
            item_height += 2;
            item_x -= 2;
            item_y -= 1;
        }
        
        // 绘制边框（如果需要）
        if (self->style.show_border) {
            uint16_t border_color = (i == self->data.selected_index) ? 
                                   self->style.selected_color : self->style.normal_color;
            
            // 使用帧缓冲或直接绘制
            if (self->framebuffer.buffer && self->framebuffer.use_shared_buffer) {
                // 帧缓冲模式：绘制边框矩形
#ifdef USE_FRAME_BUFFER
                // 绘制边框的四条边
                {
                    uint16_t x, y;
                    for (x = item_x; x < item_x + item_width; x++) {
                        BG_lcd.SetPixel(x, item_y, border_color);  // 上边
                        BG_lcd.SetPixel(x, item_y + item_height - 1, border_color);  // 下边
                    }
                    for (y = item_y; y < item_y + item_height; y++) {
                        BG_lcd.SetPixel(item_x, y, border_color);  // 左边
                        BG_lcd.SetPixel(item_x + item_width - 1, y, border_color);  // 右边
                    }
                }
#endif
            } else {
                // 直接绘制边框（4条边）
                BGUI_tool.DrawLine(item_x, item_y, item_x + item_width - 1, item_y, border_color);  // 上边
                BGUI_tool.DrawLine(item_x, item_y + item_height - 1, item_x + item_width - 1, item_y + item_height - 1, border_color);  // 下边
                BGUI_tool.DrawLine(item_x, item_y, item_x, item_y + item_height - 1, border_color);  // 左边
                BGUI_tool.DrawLine(item_x + item_width - 1, item_y, item_x + item_width - 1, item_y + item_height - 1, border_color);  // 右边
            }
        }
        
        // 绘制图标（如果有）
        const BG_MenuSlider_Item* item = &self->data.items[i];
        if (item->icon_data && item->icon_width > 0 && item->icon_height > 0) {
            // 计算图标居中位置
            uint16_t icon_x = item_x + (item_width - item->icon_width) / 2;
            uint16_t icon_y = item_y + 8;
            
            // 使用帧缓冲或直接绘制
            if (self->framebuffer.buffer && self->framebuffer.use_shared_buffer) {
                // 帧缓冲模式：此模式下通常使用LCD的ShowImage接口直接写入帧缓冲
                // 注意：实际项目中可能需要手动像素复制到帧缓冲
                BG_lcd.ShowImage(icon_x, icon_y, item->icon_width, item->icon_height, item->icon_data);
            } else {
                // 直接绘制模式
                BGUI_tool.ShowImage(icon_x, icon_y, item->icon_width, item->icon_height, item->icon_data);
            }
        }
        
        // 绘制文字（如果需要且有名称）
        if (self->style.show_text && item->name) {
            // 计算文字居中位置
            uint16_t text_len = 0;
            while (item->name[text_len] != '\0') text_len++; // 简单字符串长度计算
            uint16_t text_x = item_x + (item_width - text_len * 8) / 2; // 假设字符宽度为8
            uint16_t text_y = item_y + item_height - 12;
            
            uint16_t text_color = (i == self->data.selected_index) ? 
                                 self->style.text_color_selected : self->style.text_color_normal;
            
            // 绘制文字
            BGUI_tool.ShowString(text_x, text_y, (uint8_t*)item->name, text_color);
        }
        }
    }
    
    // 清除重绘标记
    self->data.need_redraw = 0;
}

/**
 * @brief 清除控件显示区域
 * @param self 控件指针
 */
static void BG_MenuSlider_Clear(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    // 使用帧缓冲或直接清除
    if (self->framebuffer.buffer && self->framebuffer.use_shared_buffer) {
        // 帧缓冲模式：使用填充矩形清除区域
#ifdef USE_FRAME_BUFFER
        // 逐像素清除指定区域
        {
            uint16_t x, y;
            for (y = self->region.y; y < self->region.y + self->region.height; y++) {
                for (x = self->region.x; x < self->region.x + self->region.width; x++) {
                    BG_lcd.SetPixel(x, y, self->style.background_color);
                }
            }
        }
#endif
    } else {
        // 直接绘制模式：使用GUI工具的Box函数填充背景
        BG_lcd.Box(self->region.x, self->region.y, self->region.width, self->region.height, self->style.background_color);
    }
}

/**
 * @brief 刷新显示（将帧缓冲内容输出到屏幕）
 * @param self 控件指针
 */
static void BG_MenuSlider_Refresh(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    if (self->framebuffer.buffer && self->framebuffer.use_shared_buffer) {
        if (self->framebuffer.use_dirty_region) {
            // 刷新脏区域：只刷新已更改的区域
#ifdef USE_FRAME_BUFFER
            if (frame_buffer_dirty) {
                BG_lcd.FlushFrameBuffer();
                frame_buffer_dirty = 0;
            }
#endif
        } else {
            // 刷新整个区域
#ifdef USE_FRAME_BUFFER
            BG_lcd.FlushFrameBuffer();
#endif
        }
    }
}

/**
 * @brief 刷新指定区域
 * @param self 控件指针
 * @param x X坐标
 * @param y Y坐标
 * @param w 宽度
 * @param h 高度
 */
static void BG_MenuSlider_RefreshRegion(struct BG_MenuSlider* self, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!self) {
        return;
    }
    
    // 实现指定区域刷新
    // 注意：这里简化实现，实际项目中可能需要更精细的区域刷新控制
#ifdef USE_FRAME_BUFFER
    if (self->framebuffer.buffer && self->framebuffer.use_shared_buffer) {
        // 在帧缓冲模式下，标记脏区域并刷新整个帧缓冲
        // 实际项目中可以实现更精细的区域刷新
        frame_buffer_dirty = 1;
        BG_lcd.FlushFrameBuffer();
    }
#endif
}

//=============================================================================
// 自动滑动接口实现
//=============================================================================

/**
 * @brief 开始自动滑动
 * @param self 控件指针
 */
static void BG_MenuSlider_StartAutoSlide(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    self->data.auto_slide_active = 1;
    self->data.auto_slide_timer = 0;
    self->animation.auto_slide_enable = 1;
}

/**
 * @brief 停止自动滑动
 * @param self 控件指针
 */
static void BG_MenuSlider_StopAutoSlide(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    self->data.auto_slide_active = 0;
    self->data.auto_slide_timer = 0;
    self->animation.auto_slide_enable = 0;
}

/**
 * @brief 切换自动滑动状态
 * @param self 控件指针
 */
static void BG_MenuSlider_ToggleAutoSlide(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    if (self->data.auto_slide_active) {
        BG_MenuSlider_StopAutoSlide(self);
    } else {
        BG_MenuSlider_StartAutoSlide(self);
    }
}

/**
 * @brief 自动滑动更新（基于时间增量）
 * @param self 控件指针
 * @param delta_ms 时间增量（毫秒）
 */
static void BG_MenuSlider_AutoSlideUpdate(struct BG_MenuSlider* self, uint32_t delta_ms) {
    if (!self || !self->data.auto_slide_active || 
        !self->animation.auto_slide_enable || self->data.item_count <= 1) {
        return;
    }
    
    self->data.auto_slide_timer += delta_ms;
    
    if (self->data.auto_slide_timer >= self->animation.auto_slide_interval) {
        self->data.auto_slide_timer = 0;
        
        // 执行自动滑动 - 向右滑动
        BG_MenuSlider_SlideRight(self);
    }
}

//=============================================================================
// 状态查询接口实现
//=============================================================================

/**
 * @brief 获取当前选中项索引
 * @param self 控件指针
 * @return 选中项索引
 */
static uint8_t BG_MenuSlider_GetSelectedIndex(struct BG_MenuSlider* self) {
    if (!self) {
        return 0;
    }
    return self->data.selected_index;
}

/**
 * @brief 获取当前选中项
 * @param self 控件指针
 * @return 选中项指针，失败返回NULL
 */
static const BG_MenuSlider_Item* BG_MenuSlider_GetSelectedItem(struct BG_MenuSlider* self) {
    if (!self || self->data.selected_index >= self->data.item_count) {
        return NULL;
    }
    return &self->data.items[self->data.selected_index];
}

/**
 * @brief 获取菜单项总数
 * @param self 控件指针
 * @return 菜单项总数
 */
static uint8_t BG_MenuSlider_GetItemCount(struct BG_MenuSlider* self) {
    if (!self) {
        return 0;
    }
    return self->data.item_count;
}

/**
 * @brief 检查是否正在滑动
 * @param self 控件指针
 * @return 1表示正在滑动，0表示静止
 */
static uint8_t BG_MenuSlider_IsSliding(struct BG_MenuSlider* self) {
    if (!self) {
        return 0;
    }
    return (self->data.state == BG_MENU_SLIDER_SLIDING) ? 1 : 0;
}

/**
 * @brief 检查自动滑动是否激活
 * @param self 控件指针
 * @return 1表示激活，0表示未激活
 */
static uint8_t BG_MenuSlider_IsAutoSliding(struct BG_MenuSlider* self) {
    if (!self) {
        return 0;
    }
    return self->data.auto_slide_active;
}

/**
 * @brief 检查控件是否已初始化
 * @param self 控件指针
 * @return 1表示已初始化，0表示未初始化
 */
static uint8_t BG_MenuSlider_IsInitialized(struct BG_MenuSlider* self) {
    if (!self) {
        return 0;
    }
    return self->data.initialized;
}

//=============================================================================
// 初始化和销毁接口实现
//=============================================================================

/**
 * @brief 初始化控件
 * @param region 显示区域配置
 * @param style 样式配置
 * @return 初始化后的控件实例
 */
BG_MenuSlider BG_MenuSlider_Init(const BG_MenuSlider_Region* region, 
                                 const BG_MenuSlider_Style* style) {
    BG_MenuSlider slider;
    
    // 清零整个结构体
    memset(&slider, 0, sizeof(BG_MenuSlider));
    
    // 设置函数指针
    slider.SetRegion = BG_MenuSlider_SetRegion;
    slider.SetFrameBuffer = BG_MenuSlider_SetFrameBuffer;
    slider.SetStyle = BG_MenuSlider_SetStyle;
    slider.SetAnimation = BG_MenuSlider_SetAnimation;
    
    slider.AddItem = BG_MenuSlider_AddItem;
    slider.ClearItems = BG_MenuSlider_ClearItems;
    slider.LoadTable = BG_MenuSlider_LoadTable;
    slider.RemoveItem = BG_MenuSlider_RemoveItem;
    
    slider.SlideLeft = BG_MenuSlider_SlideLeft;
    slider.SlideRight = BG_MenuSlider_SlideRight;
    slider.SlideTo = BG_MenuSlider_SlideTo;
    slider.Select = BG_MenuSlider_Select;
    
    slider.Update = BG_MenuSlider_Update;
    slider.Draw = BG_MenuSlider_Draw;
    slider.Clear = BG_MenuSlider_Clear;
    slider.Refresh = BG_MenuSlider_Refresh;
    slider.RefreshRegion = BG_MenuSlider_RefreshRegion;
    
    slider.StartAutoSlide = BG_MenuSlider_StartAutoSlide;
    slider.StopAutoSlide = BG_MenuSlider_StopAutoSlide;
    slider.ToggleAutoSlide = BG_MenuSlider_ToggleAutoSlide;
    slider.AutoSlideUpdate = BG_MenuSlider_AutoSlideUpdate;
    
    slider.GetSelectedIndex = BG_MenuSlider_GetSelectedIndex;
    slider.GetSelectedItem = BG_MenuSlider_GetSelectedItem;
    slider.GetItemCount = BG_MenuSlider_GetItemCount;
    slider.IsSliding = BG_MenuSlider_IsSliding;
    slider.IsAutoSliding = BG_MenuSlider_IsAutoSliding;
    slider.IsInitialized = BG_MenuSlider_IsInitialized;
    
    // 设置默认配置
    if (region) {
        memcpy(&slider.region, region, sizeof(BG_MenuSlider_Region));
    } else {
        // 使用默认区域配置
        slider.region.x = 0;
        slider.region.y = 50;
        slider.region.width = 160;
        slider.region.height = 64;
        slider.region.center_x = 80;
        slider.region.item_width = BG_MENU_SLIDER_ITEM_WIDTH;
        slider.region.item_height = BG_MENU_SLIDER_ITEM_HEIGHT;
    }
    
    if (style) {
        memcpy(&slider.style, style, sizeof(BG_MenuSlider_Style));
    } else {
        // 使用默认样式配置
        slider.style.selected_color = 0xFFFF;      // 白色
        slider.style.normal_color = 0x7BEF;        // 灰色
        slider.style.background_color = 0x0000;    // 黑色
        slider.style.text_color_selected = 0xFFFF; // 白色
        slider.style.text_color_normal = 0xC618;   // 暗灰色
        slider.style.border_width = BG_MENU_SLIDER_BORDER_WIDTH;
        slider.style.show_border = 1;
        slider.style.show_background = 1;
        slider.style.show_text = 1;
    }
    
    // 设置默认动画配置
    slider.animation.slide_duration = 300;
    slider.animation.ease_type = BG_MENU_SLIDER_EASE_IN_OUT;
    slider.animation.auto_slide_enable = 0;
    slider.animation.auto_slide_interval = 2000;
    slider.animation.scale_effect = 1;
    
    // 初始化数据
    slider.data.item_count = 0;
    slider.data.selected_index = 0;
    slider.data.current_offset = 0;
    slider.data.target_offset = 0;
    slider.data.state = BG_MENU_SLIDER_IDLE;
    slider.data.animation_timer = 0;
    slider.data.auto_slide_timer = 0;
    slider.data.need_redraw = 1;
    slider.data.initialized = 1;
    slider.data.auto_slide_active = 0;
    
    return slider;
}

/**
 * @brief 销毁控件
 * @param slider 控件指针
 */
void BG_MenuSlider_DeInit(BG_MenuSlider* slider) {
    if (!slider) {
        return;
    }
    
    // 停止自动滑动
    slider->StopAutoSlide(slider);
    
    // 清除所有菜单项
    slider->ClearItems(slider);
    
    // 清零所有数据
    memset(slider, 0, sizeof(BG_MenuSlider));
}

/**
 * @brief 快速创建控件（包含菜单项表）
 * @param region 显示区域配置
 * @param style 样式配置
 * @param table 菜单项表
 * @return 创建的控件实例
 */
BG_MenuSlider BG_MenuSlider_Create(const BG_MenuSlider_Region* region,
                                   const BG_MenuSlider_Style* style,
                                   const BG_MenuSlider_Table* table) {
    // 先初始化控件
    BG_MenuSlider slider = BG_MenuSlider_Init(region, style);
    
    // 如果提供了菜单项表，加载它
    if (table) {
        slider.LoadTable(&slider, table);
    }
    
    return slider;
}

//=============================================================================
// 预定义配置常量
//=============================================================================

// 预定义样式常量
const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_DEFAULT = {
    0xFFFF,  /* selected_color - 白色 */
    0x7BEF,  /* normal_color - 灰色 */
    0x0000,  /* background_color - 黑色 */
    0xFFFF,  /* text_color_selected - 白色 */
    0xC618,  /* text_color_normal - 暗灰色 */
    2,       /* border_width */
    1,       /* show_border */
    1,       /* show_background */
    1        /* show_text */
};

const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_DARK = {
    0x07E0,  /* selected_color - 绿色 */
    0x4208,  /* normal_color - 深灰色 */
    0x0000,  /* background_color - 黑色 */
    0x07E0,  /* text_color_selected - 绿色 */
    0x8410,  /* text_color_normal - 暗灰色 */
    1,       /* border_width */
    1,       /* show_border */
    1,       /* show_background */
    1        /* show_text */
};

const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_COLORFUL = {
    0xF81F,  /* selected_color - 紫色 */
    0x07FF,  /* normal_color - 青色 */
    0x001F,  /* background_color - 深蓝色 */
    0xFFE0,  /* text_color_selected - 黄色 */
    0xFFFF,  /* text_color_normal - 白色 */
    3,       /* border_width */
    1,       /* show_border */
    1,       /* show_background */
    1        /* show_text */
};

const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_MINIMAL = {
    0xFFFF,  /* selected_color - 白色 */
    0x8410,  /* normal_color - 暗灰色 */
    0x0000,  /* background_color - 黑色 */
    0xFFFF,  /* text_color_selected - 白色 */
    0x8410,  /* text_color_normal - 暗灰色 */
    1,       /* border_width */
    1,       /* show_border */
    0,       /* show_background */
    1        /* show_text */
};

// 预定义动画配置常量
const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_SMOOTH = {
    300,                        /* slide_duration */
    BG_MENU_SLIDER_EASE_IN_OUT, /* ease_type */
    0,                          /* auto_slide_enable */
    2000,                       /* auto_slide_interval */
    1                           /* scale_effect */
};

const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_FAST = {
    150,                       /* slide_duration */
    BG_MENU_SLIDER_EASE_OUT,   /* ease_type */
    0,                         /* auto_slide_enable */
    1000,                      /* auto_slide_interval */
    0                          /* scale_effect */
};

const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_AUTO = {
    500,                        /* slide_duration */
    BG_MENU_SLIDER_EASE_IN_OUT, /* ease_type */
    1,                          /* auto_slide_enable */
    2000,                       /* auto_slide_interval */
    1                           /* scale_effect */
};

const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_INSTANT = {
    1,                           /* slide_duration */
    BG_MENU_SLIDER_EASE_LINEAR,  /* ease_type */
    0,                           /* auto_slide_enable */
    1000,                        /* auto_slide_interval */
    0                            /* scale_effect */
};

// 预定义区域配置常量（假设LCD为160x128）
const BG_MenuSlider_Region BG_MENU_SLIDER_REGION_CENTER = {
    0,   /* x */
    32,  /* y */
    160, /* width */
    64,  /* height */
    80,  /* center_x */
    64,  /* item_width */
    64   /* item_height */
};

const BG_MenuSlider_Region BG_MENU_SLIDER_REGION_TOP = {
    0,   /* x */
    0,   /* y */
    160, /* width */
    64,  /* height */
    80,  /* center_x */
    64,  /* item_width */
    64   /* item_height */
};

const BG_MenuSlider_Region BG_MENU_SLIDER_REGION_BOTTOM = {
    0,   /* x */
    64,  /* y */
    160, /* width */
    64,  /* height */
    80,  /* center_x */
    64,  /* item_width */
    64   /* item_height */
};

// 预定义帧缓冲配置常量
const BG_MenuSlider_FrameBuffer BG_MENU_SLIDER_FB_FULL_SCREEN = {
    NULL, /* buffer - 需要用户设置 */
    160,  /* buffer_width */
    128,  /* buffer_height */
    0,    /* use_dirty_region */
    1,    /* use_shared_buffer */
    0,    /* refresh_x */
    0,    /* refresh_y */
    160,  /* refresh_width */
    128   /* refresh_height */
};

const BG_MenuSlider_FrameBuffer BG_MENU_SLIDER_FB_DIRTY_ONLY = {
    NULL, /* buffer - 需要用户设置 */
    160,  /* buffer_width */
    128,  /* buffer_height */
    1,    /* use_dirty_region */
    1,    /* use_shared_buffer */
    0,    /* refresh_x */
    32,   /* refresh_y */
    160,  /* refresh_width */
    64    /* refresh_height */
};
