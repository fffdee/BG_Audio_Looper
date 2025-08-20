#include "gui_tool.h"
#include "bg_lcd.h"
#include "font.h"
#include <stdio.h>
#include <stdlib.h>

void Gui_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t Color);
void Gui_Circle(uint16_t X, uint16_t Y, uint16_t R, uint16_t fc);
void Gui_ShowChar(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc);
void Gui_ShowNum(uint16_t x0, uint16_t y0, uint32_t num, uint16_t fc);
void Gui_ShowString(uint16_t x0, uint16_t y0, uint8_t *chr, uint16_t fc);
void Gui_ShowImage(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint8_t *chr);
void Gui_DrawPoint(uint16_t x0, uint16_t y0, uint16_t fc);

BGUI_Tool BGUI_tool = {


	.DrawLine = Gui_DrawLine,
	.Circle = Gui_Circle,
	.ShowChar = Gui_ShowChar,
	.ShowNum = Gui_ShowNum,
	.ShowString = Gui_ShowString,
	.ShowImage = Gui_ShowImage,
	.DrawPoint = Gui_DrawPoint,
};


void Gui_DrawPoint(uint16_t x0, uint16_t y0, uint16_t fc)
{

	BG_lcd.DrawPoint(y0,x0,fc);
}

void Gui_Circle(uint16_t X, uint16_t Y, uint16_t R, uint16_t fc)
{
	BG_lcd.Circle(X,Y,R,fc);
}



void Gui_ShowChar(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc)
{

	unsigned char c = 0;
	uint16_t y;
	uint8_t ch,w,h;
	c = chr - ' '; // 寰楀埌鍋忕Щ鍚庣殑鍊�	if (x0 > LCD_WIDTH - 1)
	{
		x0 = 0;
		y0 = y0 + 16;
	}
	for (w = 0; w < 8; w++)
	{
		y = y0;
		// printf("in func %d\n",c);
		for (h = 0; h < 8; h++)
		{
			ch = F8x16[c * 16 + w];
			// printf("ch is %d\n",ch);
			if ((ch >> h & 0x01) == 1)
			{
				BG_lcd.DrawPoint(x0 + w, y, fc);
				// printf("drawpoint 1 %d %d \n", x0 + w,y); // 浣跨敤 %u 鎴�%d 鏉ユ牸寮忓寲 unsigned char
			}

			y++;
		}
	}

	for (w = 0; w < 8; w++)
	{
		y = y0 + 8;
		for (h = 0; h < 8; h++)
		{
			ch = F8x16[c * 16 + w + 8];
			if ((ch >> h & 0x01) == 1)
			{
				BG_lcd.DrawPoint(x0 + w, y, fc);

			}
			ch = ch >> 1;
			y++;
		}
	}
}


void Gui_ShowString(uint16_t x0, uint16_t y0, uint8_t *chr, uint16_t fc)
{
	unsigned char j = 0;
	while (chr[j] != '\0')
	{
		Gui_ShowChar(x0, y0, chr[j], fc);
		x0 += 8;
		if (x0 > LCD_WIDTH - 8)
		{
			x0 = 0;
			y0 += 16;
		}
		j++;
	}
}

void Gui_ShowNum(uint16_t x0, uint16_t y0, uint32_t num, uint16_t fc)
{
	uint8_t bit_count = 0;
	uint8_t i;
	if (num == 0)
	{ // 鐗规畩鎯呭喌锛�鏄竴浣嶆暟
		bit_count = 1;
	}
	else
	{
		uint32_t temp = num;
		while (temp != 0)
		{
			temp /= 10; // 鏁撮櫎10
			bit_count++;
		}
	}

	char char_num[bit_count+1]; // 浣跨敤char绫诲瀷鏁扮粍
	for (i = 0; i < bit_count; i++)
	{
		char_num[bit_count - i - 1] = (num % 10) + '0'; // 杞崲涓哄瓧绗﹀苟瀛樺偍
		num /= 10;										// 鏇存柊num涓轰笅涓�綅鏁板瓧

	}
	char_num[bit_count] = '\0';
	Gui_ShowString(x0, y0, char_num, fc);
}

void Gui_ShowImage(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint8_t *chr)
{
	uint8_t y ,x;
	for (y = 0; y < y1; y++)
		{
			for ( x = 0; x < x1; x++)
			{
				uint16_t color565 = 0x00;
				color565 = (chr[x * 2 + 1 + y * x1 * 2] << 8) | chr[x * 2 + y * x1 * 2];



				// 绘制点
				Gui_DrawPoint(y0 + y,x0 + x, color565);
			}
		}
}

void Gui_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t Color)
{
	BG_lcd.DrawLine(x0,y0,x1,y1,Color);
}
