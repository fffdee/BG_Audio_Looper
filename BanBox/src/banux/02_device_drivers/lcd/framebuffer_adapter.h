#ifndef __FRAMEBUFFER_ADAPTER_H__
#define __FRAMEBUFFER_ADAPTER_H__

#include "framebuffer.h"
#include "gui_tool.h"

// 帧缓冲适配器 - 将BGUI_tool接口重定向到帧缓冲
void FrameBuffer_Adapter_Init(void);

// 帧缓冲版本的绘制函数
void FB_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void FB_Circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color);
void FB_ShowChar(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc);
void FB_ShowNum(uint16_t x0, uint16_t y0, uint32_t num, uint16_t fc);
void FB_ShowString(uint16_t x0, uint16_t y0, uint8_t* str, uint16_t fc);
void FB_ShowImage(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height, const uint8_t* image);
void FB_DrawPoint(uint16_t x0, uint16_t y0, uint16_t fc);
void FB_Clear(uint16_t fc);
void FB_Update(void);

// 高性能菜单专用函数
void FB_DrawMenuBox(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void FB_ClearMenuArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

#endif
