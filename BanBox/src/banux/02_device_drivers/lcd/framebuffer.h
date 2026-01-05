#ifndef __FRAMEBUFFER_H__
#define __FRAMEBUFFER_H__

#include <stdint.h>
#include "bg_lcd.h"

// 帧缓冲配置
#define FB_WIDTH  LCD_WIDTH   // 160
#define FB_HEIGHT LCD_HEIGHT  // 128
#define FB_SIZE   (FB_WIDTH * FB_HEIGHT)  // 20,480 像素

// 帧缓冲结构
typedef struct {
    uint16_t* buffer;           // 显存缓冲区指针
    uint8_t dirty;              // 脏标记：是否需要刷新
    uint16_t dirty_x1, dirty_y1; // 脏区域左上角
    uint16_t dirty_x2, dirty_y2; // 脏区域右下角
} FrameBuffer;

// 帧缓冲接口函数
void FrameBuffer_Init(void);
void FrameBuffer_Clear(uint16_t color);
void FrameBuffer_SetPixel(uint16_t x, uint16_t y, uint16_t color);
uint16_t FrameBuffer_GetPixel(uint16_t x, uint16_t y);
void FrameBuffer_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void FrameBuffer_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void FrameBuffer_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

// 刷新到LCD
void FrameBuffer_Flush(void);          // 刷新整个屏幕
void FrameBuffer_FlushDirty(void);     // 只刷新脏区域

// 脏区域管理
void FrameBuffer_MarkDirty(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void FrameBuffer_ClearDirty(void);

// 获取帧缓冲指针 (高级用法)
uint16_t* FrameBuffer_GetBuffer(void);

// 菜单专用绘制函数
void FrameBuffer_DrawImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* image_data);
void FrameBuffer_FillMenuRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void FrameBuffer_DrawMenuBorder(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color, uint16_t border_width);

// 文字绘制函数（简化版本）
void FrameBuffer_DrawChar(uint16_t x, uint16_t y, uint8_t ch, uint16_t color);
void FrameBuffer_DrawString(uint16_t x, uint16_t y, const uint8_t* str, uint16_t color);

#endif
