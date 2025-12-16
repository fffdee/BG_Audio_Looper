#ifndef __BG_LCD_H__
#define __BG_LCD_H__

#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

// 甯х紦鍐叉ā寮忔帶鍒跺畯 - 瀹氫箟杩欎釜瀹忓惎鐢ㄥ抚缂撳啿
//#define USE_FRAME_BUFFER 0

#define LCD_WIDTH 	160
#define LCD_HEIGHT 	128

// 甯х紦鍐茬浉鍏冲畾涔�#ifdef USE_FRAME_BUFFER
#define FRAME_BUFFER_SIZE (LCD_WIDTH * LCD_HEIGHT)
extern uint16_t frame_buffer[FRAME_BUFFER_SIZE];
extern uint8_t frame_buffer_dirty;


#define LCD_CS_INIT()        GPIO_RegOneBitClear(GPIO_A_IE, GPIOA20);\
		                       GPIO_RegOneBitSet(GPIO_A_OE, GPIOA20);\
		                       GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA20);

#define LCD_CS_ENABLE()     GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA20);
#define LCD_CS_DISABLE()    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA20);


#define LCD_DC_INIT()        GPIO_RegOneBitClear(GPIO_A_IE, GPIOA23);\
		                       GPIO_RegOneBitSet(GPIO_A_OE, GPIOA23);\
		                       GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA23);

#define LCD_DC_DISABLE()     GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA23);
#define LCD_DC_ENABLE()    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA23);

#define LCD_RST_INIT()        GPIO_RegOneBitClear(GPIO_A_IE, GPIOA24);\
		                       GPIO_RegOneBitSet(GPIO_A_OE, GPIOA24);\
		                       GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA24);

#define LCD_RST_DISABLE()     GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA24);
#define LCD_RST_ENABLE()    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA24);

typedef unsigned char u8;


#define RED  	0xf800
#define ORANGE 0xF882
#define GREEN	0x07e0
#define BLUE 	0x001f
#define WHITE	0xffff
#define BLACK	0x0000
#define YELLOW  0xFFE0
#define GRAY0   0xEF7D
#define GRAY1   0x8410
#define GRAY2   0x4208

typedef struct{

	void (*Init)(void);
	void (*Clear)(uint16_t Color);
	void (*DrawPoint)(uint16_t,uint16_t,uint16_t);
	void (*Circle)(uint16_t X,uint16_t Y,uint16_t R,uint16_t fc);
	void (*DrawLine)(uint16_t x0, uint16_t y0,uint16_t x1, uint16_t y1,uint16_t Color);
	void (*Box)(uint16_t x, uint16_t y, uint16_t w, uint16_t h,uint16_t bc);
	void (*ShowImage)(uint8_t x,uint8_t y,uint8_t width,uint8_t high,const uint8_t *p);
	void (*ButtonDown)(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
	void (*ButtonUp)(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
	void (*ShowChar)(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc);
#ifdef USE_FRAME_BUFFER
	void (*FlushFrameBuffer)(void);     // 鍒锋柊甯х紦鍐插埌灞忓箷
	void (*SetPixel)(uint16_t x, uint16_t y, uint16_t color);  // 璁剧疆甯х紦鍐插儚绱�#endif
#endif
}BG_Lcd;
void gui_DrawPoint(uint16_t x,uint16_t y,uint16_t Data);
extern BG_Lcd BG_lcd;

#endif
