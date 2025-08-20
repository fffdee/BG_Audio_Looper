#ifndef __BG_LCD_H__
#define __BG_LCD_H__

#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

#define LCD_WIDTH 	160
#define LCD_HEIGHT 	128

#define LCD_CS_INIT()        GPIO_RegOneBitClear(GPIO_A_IE, GPIOA20);\
		                       GPIO_RegOneBitSet(GPIO_A_OE, GPIOA20);\
		                       GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA20);

#define LCD_CS_ENABLE()     GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA20);
#define LCD_CS_DISABLE()    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA20);


#define LCD_DC_INIT()        GPIO_RegOneBitClear(GPIO_A_IE, GPIOA16);\
		                       GPIO_RegOneBitSet(GPIO_A_OE, GPIOA16);\
		                       GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA16);

#define LCD_DC_DISABLE()     GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA16);
#define LCD_DC_ENABLE()    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA16);

#define LCD_RST_INIT()        GPIO_RegOneBitClear(GPIO_A_IE, GPIOA15);\
		                       GPIO_RegOneBitSet(GPIO_A_OE, GPIOA15);\
		                       GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA15);

#define LCD_RST_DISABLE()     GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA15);
#define LCD_RST_ENABLE()    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA15);

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

}BG_Lcd;
void gui_DrawPoint(uint16_t x,uint16_t y,uint16_t Data);
extern BG_Lcd BG_lcd;

#endif
