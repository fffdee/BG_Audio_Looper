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
/* Large font function (8x16) */
void Gui_ShowCharLarge(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc);
void Gui_ShowStringLarge(uint16_t x0, uint16_t y0, uint8_t *chr, uint16_t fc);

BGUI_Tool BGUI_tool __attribute__((section(".data"))) = {


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
	{ // Bresenham algorithm
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
	c = chr - ' '; // Get the offset value after subtracting ' '
	if (x0 > LCD_WIDTH - 1)
	{
		x0 = 0;
		y0 = y0 + 8;  // Change to 8-pixel height
	}
	
	// Use 6x8 font
	for (w = 0; w < 6; w++)  // Change to 6-pixel width
	{
		y = y0;
		ch = F6x8[c][w];  // Use F6x8 font array
		for (h = 0; h < 8; h++)  // 8-pixel height
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
		x0 += 6;  // Change to 6-pixel character width
		if (x0 > LCD_WIDTH - 6)  // Change to 6-pixel boundary check
		{
			x0 = 0;
			y0 += 8;  // Change to 8-pixel line height
		}
		j++;
	}
}

void Gui_ShowNum(uint16_t x0, uint16_t y0, uint32_t num, uint16_t fc)
{
	uint8_t bit_count = 0;
	uint8_t i;
	if (num == 0)
	{ // Special case: single digit
		bit_count = 1;
	}
	else
	{
		uint32_t temp = num;
		while (temp != 0)
		{
			temp /= 10; // Divide by 10
			bit_count++;
		}
	}

	char char_num[bit_count+1]; // Use char type array
	for (i = 0; i < bit_count; i++)
	{
		char_num[bit_count - i - 1] = (num % 10) + '0'; // Convert to character and store
		num /= 10;										// Update num for next digit

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

/* Large font display character (8x16) */
void Gui_ShowCharLarge(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc)
{
	unsigned char c = 0, i = 0;
	uint16_t y;
	uint8_t ch, w, h;
	c = chr - ' '; // Get the offset value after subtracting ' '
	if (x0 > LCD_WIDTH - 1)
	{
		x0 = 0;
		y0 = y0 + 16;
	}
	
	// Use 8x16 font - upper part
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

	// Use 8x16 font - lower part
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

/* Large font display string (8x16) */
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
	BG_lcd.ShowImage(x0,y0,x1,y1,chr);
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

		dx = x1 - x0; // Calculate x distance
		dy = y1 - y0; // Calculate y distance

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

		if (dx > dy) // If x distance is greater than y distance, then there is only one point on each x axis, and several points on each y axis
		{           // The number of points on the line is equal to the x distance, draw points by incrementing x axis
			// initialize error term
			error = dy2 - dx;

			// draw the line
			for (index = 0; index <= dx; index++) // The number of points to be drawn will not exceed the x distance
			{
				// Draw point
				Gui_DrawPoint(x0, y0, Color);

				// test if error has overflowed
				if (error >= 0) // Whether to increase y coordinate
				{
					error -= dx2;

					// move to next line
					y0 += y_inc; // Increase y coordinate
				} // end if error overflowed

				// adjust the error term
				error += dy2;

				// move to the next pixel
				x0 += x_inc; // x coordinate increases by 1 after each point is drawn
			} // end for
		} // end if |slope| <= 1
		else // If y axis is greater than x axis, then there is only one point on each y axis, and several points on each x axis
		{    // Draw points by incrementing y axis
			// initialize error term
			error = dx2 - dy;

			// draw the line
			for (index = 0; index <= dy; index++)
			{
				// Set the pixel
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
