#include "gui_tool.h"
#include "bg_lcd.h"

#include <stdio.h>
#include <stdlib.h>
#include "font.h"
void Gui_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t Color);
void Gui_Circle(uint16_t X, uint16_t Y, uint16_t R, uint16_t fc);
void Gui_ShowChar(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc);
void Gui_ShowNum(uint16_t x0, uint16_t y0, uint32_t num, uint16_t fc);
void Gui_ShowString(uint16_t x0, uint16_t y0, uint8_t *chr, uint16_t fc);
void Gui_ShowImage(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint8_t *chr);
void Gui_DrawPoint(uint16_t x0, uint16_t y0, uint16_t fc);
void Gui_Clear(uint16_t fc);
void Gui_update();
/* 大字体函数 (8x16) */
void Gui_ShowCharLarge(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc);
void Gui_ShowStringLarge(uint16_t x0, uint16_t y0, uint8_t *chr, uint16_t fc);

BGUI_Tool BGUI_tool = {


	.DrawLine = Gui_DrawLine,
	.Circle = Gui_Circle,
	.ShowChar = Gui_ShowChar,
	.ShowNum = Gui_ShowNum,
	.ShowString = Gui_ShowString,
	.ShowImage = Gui_ShowImage,
	.DrawPoint = Gui_DrawPoint,
	.Clear = Gui_Clear,
	.Update = Gui_update,
	.ShowCharLarge = Gui_ShowCharLarge,
	.ShowStringLarge = Gui_ShowStringLarge,
};

void Gui_Clear(uint16_t fc){
	BG_lcd.Clear(fc);
}

void Gui_update()
{

}

void Gui_DrawPoint(uint16_t x0, uint16_t y0, uint16_t fc)
{

	BG_lcd.DrawPoint(x0,y0,fc);
}

void Gui_Circle(uint16_t X, uint16_t Y, uint16_t R, uint16_t fc)
{
	void Gui_Circle(uint16_t X, uint16_t Y, uint16_t R, uint32_t fc)
	{ // Bresenham算法
		unsigned short a, b;
		int c;
		a = 0;
		b = R;
		c = 3 - 2 * R;
		while (a < b)
		{
			Gui_DrawPoint(X + a, Y + b, fc); //        7
			Gui_DrawPoint(X - a, Y + b, fc); //        6
			Gui_DrawPoint(X + a, Y - b, fc); //        2
			Gui_DrawPoint(X - a, Y - b, fc); //        3
			Gui_DrawPoint(X + b, Y + a, fc); //        8
			Gui_DrawPoint(X - b, Y + a, fc); //        5
			Gui_DrawPoint(X + b, Y - a, fc); //        1
			Gui_DrawPoint(X - b, Y - a, fc); //        4

			if (c < 0)
				c = c + 4 * a + 6;
			else
			{
				c = c + 4 * (a - b) + 10;
				b -= 1;
			}
			a += 1;
		}
		if (a == b)
		{
			Gui_DrawPoint(X + a, Y + b, fc);
			Gui_DrawPoint(X + a, Y + b, fc);
			Gui_DrawPoint(X + a, Y - b, fc);
			Gui_DrawPoint(X - a, Y - b, fc);
			Gui_DrawPoint(X + b, Y + a, fc);
			Gui_DrawPoint(X - b, Y + a, fc);
			Gui_DrawPoint(X + b, Y - a, fc);
			Gui_DrawPoint(X - b, Y - a, fc);
		}
	}
;
}



void Gui_ShowChar(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc)
{
	unsigned char c = 0, i = 0;
	uint16_t y;
	uint8_t ch, w, h;
	c = chr - ' '; // 得到偏移后的值
	if (x0 > LCD_WIDTH - 1)
	{
		x0 = 0;
		y0 = y0 + 8;  // 改为8像素高度
	}
	
	// 使用6x8字体
	for (w = 0; w < 6; w++)  // 改为6像素宽度
	{
		y = y0;
		ch = F6x8[c][w];  // 使用F6x8字体数组
		for (h = 0; h < 8; h++)  // 8像素高度
		{
			if ((ch >> h & 0x01) == 1)
			{
				Gui_DrawPoint(x0 + w, y, fc);
			}
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
		x0 += 6;  // 改为6像素字符宽度
		if (x0 > LCD_WIDTH - 6)  // 改为6像素边界检查
		{
			x0 = 0;
			y0 += 8;  // 改为8像素行高
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

/* 6x8 small font display - single character */
void Gui_ShowChar6x8(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc)
{
	uint8_t c, w, h, ch;
	
	if (chr < ' ' || chr > '~') chr = ' ';
	c = chr - ' ';
	
	for (w = 0; w < 6; w++) {
		ch = F6x8[c][w];
		for (h = 0; h < 8; h++) {
			if (ch & (1 << h)) {
				Gui_DrawPoint(x0 + w, y0 + h, fc);
			}
		}
	}
}

/* 6x8 small font display - string */
void Gui_ShowString6x8(uint16_t x0, uint16_t y0, uint8_t *chr, uint16_t fc)
{
	while (*chr != '\0') {
		Gui_ShowChar6x8(x0, y0, *chr, fc);
		x0 += 6;
		if (x0 > LCD_WIDTH - 6) {
			x0 = 0;
			y0 += 8;
		}
		chr++;
	}
}

/* 大字体显示字符 (8x16) */
void Gui_ShowCharLarge(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc)
{
	unsigned char c = 0, i = 0;
	uint16_t y;
	uint8_t ch, w, h;
	c = chr - ' '; // 得到偏移后的值
	if (x0 > LCD_WIDTH - 1)
	{
		x0 = 0;
		y0 = y0 + 16;
	}
	
	// 使用8x16字体 - 上半部分
	for (w = 0; w < 8; w++)
	{
		y = y0;
		ch = F8x16[c * 16 + w];
		for (h = 0; h < 8; h++)
		{
			if ((ch >> h & 0x01) == 1)
			{
				Gui_DrawPoint(x0 + w, y, fc);
			}
			y++;
		}
	}

	// 使用8x16字体 - 下半部分
	for (w = 0; w < 8; w++)
	{
		y = y0 + 8;
		ch = F8x16[c * 16 + w + 8];
		for (h = 0; h < 8; h++)
		{
			if ((ch >> h & 0x01) == 1)
			{
				Gui_DrawPoint(x0 + w, y, fc);
			}
			y++;
		}
	}
}

/* 大字体显示字符串 (8x16) */
void Gui_ShowStringLarge(uint16_t x0, uint16_t y0, uint8_t *chr, uint16_t fc)
{
	unsigned char j = 0;
	while (chr[j] != '\0')
	{
		Gui_ShowCharLarge(x0, y0, chr[j], fc);
		x0 += 8;
		if (x0 > LCD_WIDTH - 8)
		{
			x0 = 0;
			y0 += 16;
		}
		j++;
	}
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
				Gui_DrawPoint(x0 + x,y0 + y, color565);
			}
		}
}

void Gui_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t Color)
{

		int dx,	 // difference in x's
			dy,	 // difference in y's
			dx2, // dx,dy * 2
			dy2,
			x_inc, // amount in pixel space to move during drawing
			y_inc, // amount in pixel space to move during drawing
			error, // the discriminant i.e. error i.e. decision variable
			index; // used for looping

		dx = x1 - x0; // 计算x距离
		dy = y1 - y0; // 计算y距离

		if (dx >= 0)
		{
			x_inc = 1;
		}
		else
		{
			x_inc = -1;
			dx = -dx;
		}

		if (dy >= 0)
		{
			y_inc = 1;
		}
		else
		{
			y_inc = -1;
			dy = -dy;
		}

		dx2 = dx << 1;
		dy2 = dy << 1;

		if (dx > dy) // x距离大于y距离，那么每个x轴上只有一个点，每个y轴上有若干个点
		{			 // 且线的点数等于x距离，以x轴递增画点
			// initialize error term
			error = dy2 - dx;

			// draw the line
			for (index = 0; index <= dx; index++) // 要画的点数不会超过x距离
			{
				// 画点
				Gui_DrawPoint(x0, y0, Color);

				// test if error has overflowed
				if (error >= 0) // 是否需要增加y坐标值
				{
					error -= dx2;

					// move to next line
					y0 += y_inc; // 增加y坐标值
				} // end if error overflowed

				// adjust the error term
				error += dy2;

				// move to the next pixel
				x0 += x_inc; // x坐标值每次画点后都递增1
			} // end for
		} // end if |slope| <= 1
		else // y轴大于x轴，则每个y轴上只有一个点，x轴若干个点
		{	 // 以y轴为递增画点
			// initialize error term
			error = dx2 - dy;

			// draw the line
			for (index = 0; index <= dy; index++)
			{
				// set the pixel
				Gui_DrawPoint(x0, y0, Color);

				// test if error overflowed
				if (error >= 0)
				{
					error -= dy2;

					// move to next line
					x0 += x_inc;
				} // end if error overflowed

				// adjust the error term
				error += dx2;

				// move to the next pixel
				y0 += y_inc;
			} // end for
		} // end else |slope| > 1

}
