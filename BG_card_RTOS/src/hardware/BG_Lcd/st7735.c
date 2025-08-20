#include "st7735.h"
#include "bg_lcd.h"

extern void Lcd_WriteData(uint8_t data);
extern void Lcd_WriteIndex(uint8_t index);

void st7735_init(void)
{
	//LCD Init For 1.44Inch LCD Panel with ST7735R.

		LCD_RST_ENABLE();
		Lcd_WriteIndex(0x11);//Sleep exit


	    //ST7735R Frame Rate
	    Lcd_WriteIndex(0xB1);
	    Lcd_WriteData(0x01);
	    Lcd_WriteData(0x2C);
	    Lcd_WriteData(0x2D);

	    Lcd_WriteIndex(0xB2);
	    Lcd_WriteData(0x01);
	    Lcd_WriteData(0x2C);
	    Lcd_WriteData(0x2D);

	    Lcd_WriteIndex(0xB3);
	    Lcd_WriteData(0x01);
	    Lcd_WriteData(0x2C);
	    Lcd_WriteData(0x2D);
	    Lcd_WriteData(0x01);
	    Lcd_WriteData(0x2C);
	    Lcd_WriteData(0x2D);

	    Lcd_WriteIndex(0xB4); //Column inversion
	    Lcd_WriteData(0x07);

	    //ST7735R Power Sequence
	    Lcd_WriteIndex(0xC0);
	    Lcd_WriteData(0xA2);
	    Lcd_WriteData(0x02);
	    Lcd_WriteData(0x84);
	    Lcd_WriteIndex(0xC1);
	    Lcd_WriteData(0xC5);

	    Lcd_WriteIndex(0xC2);
	    Lcd_WriteData(0x0A);
	    Lcd_WriteData(0x00);

	    Lcd_WriteIndex(0xC3);
	    Lcd_WriteData(0x8A);
	    Lcd_WriteData(0x2A);
	    Lcd_WriteIndex(0xC4);
	    Lcd_WriteData(0x8A);
	    Lcd_WriteData(0xEE);

	    Lcd_WriteIndex(0xC5); //VCOM
	    Lcd_WriteData(0x0E);

	    Lcd_WriteIndex(0x36); //MX, MY, RGB mode
	    //Lcd_WriteData(0x20);
	    Lcd_WriteData(0x60);
	    //Lcd_WriteData(0x28);
	    //ST7735R Gamma Sequence
	    Lcd_WriteIndex(0xe0);
	    Lcd_WriteData(0x0f);
	    Lcd_WriteData(0x1a);
	    Lcd_WriteData(0x0f);
	    Lcd_WriteData(0x18);
	    Lcd_WriteData(0x2f);
	    Lcd_WriteData(0x28);
	    Lcd_WriteData(0x20);
	    Lcd_WriteData(0x22);
	    Lcd_WriteData(0x1f);
	    Lcd_WriteData(0x1b);
	    Lcd_WriteData(0x23);
	    Lcd_WriteData(0x37);
	    Lcd_WriteData(0x00);
	    Lcd_WriteData(0x07);
	    Lcd_WriteData(0x02);
	    Lcd_WriteData(0x10);

	    Lcd_WriteIndex(0xe1);
	    Lcd_WriteData(0x0f);
	    Lcd_WriteData(0x1b);
	    Lcd_WriteData(0x0f);
	    Lcd_WriteData(0x17);
	    Lcd_WriteData(0x33);
	    Lcd_WriteData(0x2c);
	    Lcd_WriteData(0x29);
	    Lcd_WriteData(0x2e);
	    Lcd_WriteData(0x30);
	    Lcd_WriteData(0x30);
	    Lcd_WriteData(0x39);
	    Lcd_WriteData(0x3f);
	    Lcd_WriteData(0x00);
	    Lcd_WriteData(0x07);
	    Lcd_WriteData(0x03);
	    Lcd_WriteData(0x10);

	    Lcd_WriteIndex(0x2a);
	    Lcd_WriteData(0x00);
	    Lcd_WriteData(0x00);
	    Lcd_WriteData(0x00);
	    Lcd_WriteData(0x7f);

	    Lcd_WriteIndex(0x2b);
	    Lcd_WriteData(0x00);
	    Lcd_WriteData(0x00);
	    Lcd_WriteData(0x00);
	    Lcd_WriteData(0x9f);

	    Lcd_WriteIndex(0xF0); //Enable test command
	    Lcd_WriteData(0x01);
	    Lcd_WriteIndex(0xF6); //Disable ram power save mode
	    Lcd_WriteData(0x00);

	    Lcd_WriteIndex(0x3A); //65k mode
	    Lcd_WriteData(0x05);

	    Lcd_WriteIndex(0x29);//Display on


}
