#include "bg_lcd.h"
#include "spim.h"
#include "spi_flash.h"
#include "debug.h"
#include "spim_interface.h"
#include "dma.h"
#include "st7735.h"
#include "font.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>
#include <math.h>

#define _USE_MATH_DEFINES

// 甯х紦鍐茬浉鍏冲彉閲忓拰鍑芥暟
#ifdef USE_FRAME_BUFFER
uint16_t frame_buffer[FRAME_BUFFER_SIZE];
uint8_t frame_buffer_dirty = 0;

// 甯х紦鍐插嚱鏁板０鏄�void frame_buffer_flush(void);
void frame_buffer_set_pixel(uint16_t x, uint16_t y, uint16_t color);
void frame_buffer_draw_point(uint16_t x, uint16_t y, uint16_t color);
void frame_buffer_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void frame_buffer_clear(uint16_t color);
void frame_buffer_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t bc);
#endif

void lcd_init(void);
void lcd_write_byte(uint8_t data);
void Lcd_Clear(uint16_t Color);
void gui_DrawPoint(uint16_t x,uint16_t y,uint16_t Data);
void gui_Circle(uint16_t X,uint16_t Y,uint16_t R,uint16_t fc);
void gui_DrawLine(uint16_t x0, uint16_t y0,uint16_t x1, uint16_t y1,uint16_t Color);
void Gui_Speed_Radians(uint8_t X,uint8_t Y,uint8_t R,uint8_t angle,uint16_t fc, bool initFlag);
void Gui_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h,uint16_t bc);
void showimage(uint8_t x,uint8_t y,uint8_t width,uint8_t high,const uint8_t *p);

#ifdef USE_FRAME_BUFFER
extern void frame_buffer_flush(void);
extern void frame_buffer_set_pixel(uint16_t x, uint16_t y, uint16_t color);
extern void frame_buffer_draw_point(uint16_t x, uint16_t y, uint16_t color);
extern void frame_buffer_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
extern void frame_buffer_clear(uint16_t color);
extern void frame_buffer_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t bc);
#endif

void DisplayButtonDown(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
void DisplayButtonUp(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
void ShowChar(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc);
BG_Lcd BG_lcd = {
	.Init = lcd_init,
#ifdef USE_FRAME_BUFFER
	.Clear = frame_buffer_clear,
	.DrawPoint = frame_buffer_draw_point,
	.Circle = gui_Circle,  // 鍦嗗舰缁樺埗鏆傛椂淇濇寔鍘熸牱
	.DrawLine = frame_buffer_draw_line,
	.Box = frame_buffer_box,
	.FlushFrameBuffer = frame_buffer_flush,
	.SetPixel = frame_buffer_set_pixel,
#else
	.Clear = Lcd_Clear,
	.DrawPoint = gui_DrawPoint,
	.Circle = gui_Circle,
	.DrawLine = gui_DrawLine,
	.Box = Gui_box,
#endif
	.ShowImage = showimage,
	.ButtonUp = DisplayButtonUp,
	.ButtonDown = DisplayButtonDown,
	.ShowChar = ShowChar,
};
void lcd_init(void)
{

	LCD_RST_INIT();
	LCD_DC_INIT();
	LCD_CS_INIT();

	/* Hardware reset sequence for ST7735 */
	LCD_RST_DISABLE();       // Pull reset low
	vTaskDelay(10);          // Wait 10ms
	LCD_RST_ENABLE();        // Pull reset high
	vTaskDelay(120);         // Wait 120ms for LCD to stabilize

	st7735_init();


}
void lcd_write_byte(uint8_t data){

	SPIM_DMA_Send_Start(&data, 1);
	while(!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));

}

void Lcd_WriteIndex(uint8_t index)
{
	LCD_CS_ENABLE();
	LCD_DC_DISABLE();
	lcd_write_byte(index);
	LCD_CS_DISABLE();
}

void Lcd_WriteData(uint8_t data)
{
	LCD_CS_ENABLE();
	LCD_DC_ENABLE();
	lcd_write_byte(data);
	LCD_CS_DISABLE();
}

void Lcd_WriteReg(uint8_t index, uint8_t data)
{
		LCD_CS_ENABLE();
		Lcd_WriteIndex(index);
        Lcd_WriteData(data);
        LCD_CS_DISABLE();
}

/**
 * @brief 璁剧疆LCD灞忓箷鏃嬭浆鏂瑰悜
 * @param rotation 鏃嬭浆瑙掑害 (0=0掳, 1=90掳, 2=180掳, 3=270掳)
 */
void Lcd_SetRotation(uint8_t rotation)
{
	uint8_t madctl_value;
	
	switch (rotation % 4)
	{
		case 0:  // 0掳 - 绔栧睆
			madctl_value = 0xA0;  // MY=1, MX=0, MV=1, ML=0, RGB=0
			break;
		case 1:  // 90掳 - 妯睆
			madctl_value = 0x60;  // MY=0, MX=1, MV=1, ML=0, RGB=0
			break;
		case 2:  // 180掳 - 绔栧睆鍊掔疆
			madctl_value = 0x00;  // MY=0, MX=0, MV=0, ML=0, RGB=0
			break;
		case 3:  // 270掳 - 妯睆鍊掔疆
			madctl_value = 0xC0;  // MY=1, MX=1, MV=0, ML=0, RGB=0
			break;
		default:
			madctl_value = 0xA0;  // 榛樿0掳
			break;
	}
	
	Lcd_WriteIndex(0x36);  // Memory Data Access Control
	Lcd_WriteData(madctl_value);
}

void LCD_WriteData_16Bit(uint16_t Data)
{
	  LCD_CS_ENABLE();
	  LCD_DC_ENABLE();
	  lcd_write_byte(Data>>8);
	  lcd_write_byte(Data);
	  LCD_CS_DISABLE();

}

//void Lcd_SetRegion(uint16_t x_start,uint16_t y_start,uint16_t x_end,uint16_t y_end){
//	Lcd_WriteIndex(0x2a);
//	Lcd_WriteData(0x00);
//	Lcd_WriteData(x_start);//Lcd_WriteData(x_start+2);
//	Lcd_WriteData(0x00);
//	Lcd_WriteData(x_end+2);
//
//	Lcd_WriteIndex(0x2b);
//	Lcd_WriteData(0x00);
//	Lcd_WriteData(y_start+0);
//	Lcd_WriteData(0x00);
//	Lcd_WriteData(y_end+1);
//
//	Lcd_WriteIndex(0x2c);
//
//}
void Lcd_SetRegion(uint16_t x_start,uint16_t y_start,uint16_t x_end,uint16_t y_end){
    Lcd_WriteIndex(0x2a); // Column Address Set
    Lcd_WriteData(x_start >> 8);  // Start column high byte (always 0 for ST7735)
    Lcd_WriteData(x_start); // Start column low byte
    Lcd_WriteData(x_end >> 8);  // End column high byte (always 0 for ST7735)
    Lcd_WriteData(x_end); // End column low byte

    Lcd_WriteIndex(0x2b); // Page Address Set
    Lcd_WriteData(y_start>>8);  // Start page high byte (always 0 for ST7735)
    Lcd_WriteData(y_start); // Start page low byte
    Lcd_WriteData(y_end>>8);  // End page high byte (always 0 for ST7735)
    Lcd_WriteData(y_end); // End page low byte

    Lcd_WriteIndex(0x2c); // Memory Write


}

void Lcd_SetXY(uint16_t x,uint16_t y){
  	Lcd_SetRegion(x,y,x,y);
}



void gui_DrawPoint(uint16_t x,uint16_t y,uint16_t Data){

	Lcd_SetRegion(x,y,x+1,y+1);
	LCD_WriteData_16Bit(Data);

}


unsigned int Lcd_ReadPoint(uint16_t x,uint16_t y){
  unsigned int Data;
  Lcd_SetXY(x,y);
  Lcd_WriteData(Data);
  return Data;
}

void Lcd_Clear(uint16_t Color)               {
   unsigned int i,m;
   Lcd_SetRegion(0,0,LCD_WIDTH-1,LCD_HEIGHT-1);
   Lcd_WriteIndex(0x2C);
   for(i=0;i<LCD_WIDTH;i++)
    for(m=0;m<LCD_HEIGHT;m++)
    {
	  	  LCD_WriteData_16Bit(Color);
    }
}



uint16_t LCD_BGR2RGB(uint16_t c){
  uint16_t  r,g,b,rgb;
  b=(c>>0)&0x1f;
  g=(c>>5)&0x3f;
  r=(c>>11)&0x1f;
  rgb=(b<<11)+(g<<5)+(r<<0);
  return(rgb);
}
//鐢荤嚎鍑芥暟锛屼娇鐢˙resenham 鐢荤嚎绠楁硶
void gui_DrawLine(uint16_t x0, uint16_t y0,uint16_t x1, uint16_t y1,uint16_t Color){
int dx,             // difference in x's
    dy,             // difference in y's
    dx2,            // dx,dy * 2
    dy2,
    x_inc,          // amount in pixel space to move during drawing
    y_inc,          // amount in pixel space to move during drawing
    error,          // the discriminant i.e. error i.e. decision variable
    index;          // used for looping


	Lcd_SetXY(x0,y0);
	dx = x1-x0;//璁＄畻x璺濈
	dy = y1-y0;//璁＄畻y璺濈

	if (dx>=0)
	{
		x_inc = 1;
	}
	else
	{
		x_inc = -1;
		dx    = -dx;
	}

	if (dy>=0)
	{
		y_inc = 1;
	}
	else
	{
		y_inc = -1;
		dy    = -dy;
	}

	dx2 = dx << 1;
	dy2 = dy << 1;

	if (dx > dy){
		error = dy2 - dx;

		// draw the line
		for (index=0; index <= dx; index++)//瑕佺敾鐨勭偣鏁颁笉浼氳秴杩噚璺濈
		{
			//鐢荤偣
			Gui_DrawPoint(x0,y0,Color);

			// test if error has overflowed
			if (error >= 0)
				{
				error-=dx2;

				// move to next line
				y0+=y_inc;
				} // end if error overflowed

			// adjust the error term
			error+=dy2;

			// move to the next pixel
			x0+=x_inc;//x鍧愭爣鍊兼瘡娆＄敾鐐瑰悗閮介�澧�
		} // end for
	} // end if |slope| <= 1
	else{
		// initialize error term
		error = dx2 - dy;

		// draw the line
		for (index=0; index <= dy; index++)
		{
			// set the pixel
			Gui_DrawPoint(x0,y0,Color);

			// test if error overflowed
			if (error >= 0)
			{
				error-=dy2;

				// move to next line
				x0+=x_inc;
			} // end if error overflowed

			// adjust the error term
			error+=dx2;

			// move to the next pixel
			y0+=y_inc;
		} // end for
	}
}



void Gui_Speed_Radians(uint8_t X,uint8_t Y,uint8_t R,uint8_t angle,uint16_t fc, bool initFlag)
{
	unsigned char i;
  // 灏嗚搴﹁浆鎹负寮у害
  if(initFlag){
    for(i=0;i<180;i++){
      float angle_radians = radians(i+180);
      int outlineX = (int)(X + (R+2) * cos(angle_radians));
      int outlineY = (int)(Y + (R+2) * sin(angle_radians));
      Gui_DrawPoint(outlineX,outlineY,WHITE);
      int inlineX = (int)(X + (R-R/4-2) * cos(angle_radians));
      int inlineY = (int)(Y + (R-R/4-2) * sin(angle_radians));
      Gui_DrawPoint(inlineX,inlineY,WHITE);
      if((i==0)||(i%10==0)){
        int plineX = (int)(X + (R-R/4-10) * cos(angle_radians));
        int plineY = (int)(Y + (R-R/4-10) * sin(angle_radians));
        Gui_DrawLine(inlineX,inlineY,plineX,plineY,WHITE);
      }

    }

  }else {

      float angle_radians = radians(angle+180);
      int x = (int)(X + R * cos(angle_radians)); // 璁＄畻鍦嗕笂鐨勭偣鍧愭爣
      int y = (int)(Y + R * sin(angle_radians));

      int x2 = (int)(X + (R-R/4) * cos(angle_radians));
      int y2 = (int)(Y + (R-R/4) * sin(angle_radians));


      Gui_DrawLine(x,y,x2,y2,fc);
  }





}

void Gui_Circle2(uint16_t X,uint16_t Y,uint16_t R,uint16_t fc) {//Bresenham绠楁硶
    unsigned short  a,b;
    int c;
    a=0;
    b=R;
    c=3-2*R;
    while (a<b)
    {
        // Gui_DrawPoint(X+a,Y+b,fc);     //        7
        // Gui_DrawPoint(X-a,Y+b,fc);     //        6
        // Gui_DrawPoint(X+a,Y-b,fc);     //        2
        // Gui_DrawPoint(X-a,Y-b,fc);     //        3
        // Gui_DrawPoint(X+b,Y+a,fc);     //        8
        // Gui_DrawPoint(X-b,Y+a,fc);     //        5
        // Gui_DrawPoint(X+b,Y-a,fc);     //        1
        // Gui_DrawPoint(X-b,Y-a,fc);     //        4


        Gui_DrawLine(X+a,Y+b,X,Y,fc);     //        7
        Gui_DrawLine(X-a,Y+b,X,Y,fc);     //        6
        Gui_DrawLine(X+a,Y-b,X,Y,fc);     //        2
        Gui_DrawLine(X-a,Y-b,X,Y,fc);     //        3
        Gui_DrawLine(X+b,Y+a,X,Y,fc);     //        8
        Gui_DrawLine(X-b,Y+a,X,Y,fc);     //        5
        Gui_DrawLine(X+b,Y-a,X,Y,fc);     //        1
        Gui_DrawLine(X-b,Y-a,X,Y,fc);     //        4
        if(c<0) c=c+4*a+6;
        else
        {
            c=c+4*(a-b)+10;
            b-=1;
        }
       a+=1;
    }
    if (a==b)
    {
        // Gui_DrawPoint(X+a,Y+b,fc);
        // Gui_DrawPoint(X+a,Y+b,fc);
        // Gui_DrawPoint(X+a,Y-b,fc);
        // Gui_DrawPoint(X-a,Y-b,fc);
        // Gui_DrawPoint(X+b,Y+a,fc);
        // Gui_DrawPoint(X-b,Y+a,fc);
        // Gui_DrawPoint(X+b,Y-a,fc);
        // Gui_DrawPoint(X-b,Y-a,fc);

        Gui_DrawLine(X+a,Y+b,X,Y,fc);
        Gui_DrawLine(X+a,Y+b,X,Y,fc);
        Gui_DrawLine(X+a,Y-b,X,Y,fc);
        Gui_DrawLine(X-a,Y-b,X,Y,fc);
        Gui_DrawLine(X+b,Y+a,X,Y,fc);
        Gui_DrawLine(X-b,Y+a,X,Y,fc);
        Gui_DrawLine(X+b,Y-a,X,Y,fc);
        Gui_DrawLine(X-b,Y-a,X,Y,fc);
    }

}


// void Lcd_Clear(uint16_t Color)               {
//    unsigned int i,m;
//    Lcd_SetRegion(0,0,LCD_WIDTH-1,LCD_HEIGHT-1);
//    Lcd_WriteIndex(0x2C);
//    for(i=0;i<LCD_WIDTH;i++)
//     for(m=0;m<LCD_HEIGHT;m++)
//     {
// 	  	  LCD_WriteData_16Bit(Color);
//     }
// }
void Gui_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h,uint16_t bc){
	/* 濉厖鐭╁舰 */
	uint16_t i,m;
	uint16_t x_end, y_end;
	x_end = x + w - 1;
	y_end = y + h - 1;
	Lcd_SetRegion(x,y,x+w,y+h);
    Lcd_WriteIndex(0x2C);
    for(i=x;i<x_end;i++)
    for(m=y;m<y_end;m++)
    {
	  	  LCD_WriteData_16Bit(bc);
    }
	// for (i = 0; i < h; i++) {
	// 	Gui_DrawLine(x, y + i, x + w - 1, y + i, bc);
	// }
}

void Gui_box_border(uint16_t x, uint16_t y, uint16_t w, uint16_t h,uint16_t bc){
	/* 鍙敾杈规 (鍘熸潵鐨凣ui_box鍔熻兘) */
	Gui_DrawLine(x,y,x+w,y,0xEF7D);
	Gui_DrawLine(x+w-1,y+1,x+w-1,y+1+h,0x2965);
	Gui_DrawLine(x,y+h,x+w,y+h,0x2965);
	Gui_DrawLine(x,y,x,y+h,0xEF7D);
    Gui_DrawLine(x+1,y+1,x+1+w-2,y+1+h-2,bc);
}

void Gui_box2(uint16_t x,uint16_t y,uint16_t w,uint16_t h, u8 mode){
	if (mode==0)	{
		Gui_DrawLine(x,y,x+w,y,0xEF7D);
		Gui_DrawLine(x+w-1,y+1,x+w-1,y+1+h,0x2965);
		Gui_DrawLine(x,y+h,x+w,y+h,0x2965);
		Gui_DrawLine(x,y,x,y+h,0xEF7D);
		}
	if (mode==1)	{
		Gui_DrawLine(x,y,x+w,y,0x2965);
		Gui_DrawLine(x+w-1,y+1,x+w-1,y+1+h,0xEF7D);
		Gui_DrawLine(x,y+h,x+w,y+h,0xEF7D);
		Gui_DrawLine(x,y,x,y+h,0x2965);
	}
	if (mode==2)	{
		Gui_DrawLine(x,y,x+w,y,0xffff);
		Gui_DrawLine(x+w-1,y+1,x+w-1,y+1+h,0xffff);
		Gui_DrawLine(x,y+h,x+w,y+h,0xffff);
		Gui_DrawLine(x,y,x,y+h,0xffff);
	}
}




//鍙栨ā鏂瑰紡 姘村钩鎵弿 浠庡乏鍒板彸 浣庝綅鍦ㄥ墠
void showimage(uint8_t x,uint8_t y,uint8_t width,uint8_t high,const uint8_t *p){ //鏄剧ず40*40 QQ鍥剧墖
  	int i,j,k;
	unsigned char picH,picL;
	//Lcd_Clear(BLACK); //娓呭睆


			Lcd_SetRegion(x+2,y,x+width-1,y+high-1);		//鍧愭爣璁剧疆
		    for(i=0;i<width*high;i++)
			 {
			 	picL=*(p+i*2);	//鏁版嵁浣庝綅鍦ㄥ墠
				picH=*(p+i*2+1);
				LCD_WriteData_16Bit(picH<<8|picL);
        //delay(1);
			 }

}


/**************************************************************************************
鍔熻兘鎻忚堪: 鍦ㄥ睆骞曟樉绀轰竴鍑歌捣鐨勬寜閽
杈�   鍏� uint16_t x1,y1,x2,y2 鎸夐挳妗嗗乏涓婅鍜屽彸涓嬭鍧愭爣
杈�   鍑� 鏃�**************************************************************************************/
void DisplayButtonDown(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2){
	Gui_DrawLine(x1,  y1,  x2,y1, GRAY2);  //H
	Gui_DrawLine(x1+1,y1+1,x2,y1+1, GRAY1);  //H
	Gui_DrawLine(x1,  y1,  x1,y2, GRAY2);  //V
	Gui_DrawLine(x1+1,y1+1,x1+1,y2, GRAY1);  //V
	Gui_DrawLine(x1,  y2,  x2,y2, WHITE);  //H
	Gui_DrawLine(x2,  y1,  x2,y2, WHITE);  //V
}

/**************************************************************************************
鍔熻兘鎻忚堪: 鍦ㄥ睆骞曟樉绀轰竴鍑逛笅鐨勬寜閽
杈�   鍏� uint16_t x1,y1,x2,y2 鎸夐挳妗嗗乏涓婅鍜屽彸涓嬭鍧愭爣
杈�   鍑� 鏃�**************************************************************************************/
void DisplayButtonUp(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2){
	Gui_DrawLine(x1,  y1,  x2,y1, WHITE); //H
	Gui_DrawLine(x1,  y1,  x1,y2, WHITE); //V

	Gui_DrawLine(x1+1,y2-1,x2,y2-1, GRAY1);  //H
	Gui_DrawLine(x1,  y2,  x2,y2, GRAY2);  //H
	Gui_DrawLine(x2-1,y1+1,x2-1,y2, GRAY1);  //V
  Gui_DrawLine(x2  ,y1  ,x2,y2, GRAY2); //V
}

void Lcd_Clear_Section(uint8_t x0,uint8_t y0,uint8_t x1,uint8_t y1,uint16_t Color) {
   unsigned int i,m;
   Lcd_SetRegion(x0,y0,x0+x1,y0+y1);
   Lcd_WriteIndex(0x2C);
   for(i=0;i<x1;i++)
    for(m=0;m<y1;m++)
    {
	  	  LCD_WriteData_16Bit(Color);
    }
}

// gui_Circle鍑芥暟瀹炵幇 - 浣跨敤Bresenham绠楁硶鐢诲渾
void gui_Circle(uint16_t X,uint16_t Y,uint16_t R,uint16_t fc) {
    unsigned short  a,b;
    int c;
    a=0;
    b=R;
    c=3-2*R;
    while (a<b)
    {
        gui_DrawPoint(X+a,Y+b,fc);     //        7
        gui_DrawPoint(X-a,Y+b,fc);     //        6
        gui_DrawPoint(X+a,Y-b,fc);     //        2
        gui_DrawPoint(X-a,Y-b,fc);     //        3
        gui_DrawPoint(X+b,Y+a,fc);     //        8
        gui_DrawPoint(X-b,Y+a,fc);     //        5
        gui_DrawPoint(X+b,Y-a,fc);     //        1
        gui_DrawPoint(X-b,Y-a,fc);     //        4

        if(c<0) c=c+4*a+6;
        else
        {
            c=c+4*(a-b)+10;
            b-=1;
        }
       a+=1;
    }
    if (a==b)
    {
        gui_DrawPoint(X+a,Y+b,fc);
        gui_DrawPoint(X+a,Y+b,fc);
        gui_DrawPoint(X+a,Y-b,fc);
        gui_DrawPoint(X-a,Y-b,fc);
        gui_DrawPoint(X+b,Y+a,fc);
        gui_DrawPoint(X-b,Y+a,fc);
        gui_DrawPoint(X+b,Y-a,fc);
        gui_DrawPoint(X-b,Y-a,fc);
    }
}

// ShowChar鍑芥暟瀹炵幇 - 鏄剧ず鍗曚釜瀛楃 (8x16鍍忕礌)
void ShowChar(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc) {
    uint8_t i, j;
    uint8_t temp;
    
    // 瀛楃鑼冨洿妫�煡锛宎sc16瀛椾綋浠庣┖鏍�0x20)寮�
    if (chr < 0x20 || chr > 0x7E) {
        chr = '?';  // 涓嶆敮鎸佺殑瀛楃鏄剧ず闂彿
    }
    
    // 鑾峰彇瀛楃鍦ㄥ瓧浣撴暟鎹腑鐨勫亸绉�(姣忎釜瀛楃16瀛楄妭)
    uint16_t offset = (chr - 0x20) * 16;
    
    for(i = 0; i < 16; i++) {
        temp = asc16[offset + i];
        for(j = 0; j < 8; j++) {
            if(temp & (0x80 >> j)) {
#ifdef USE_FRAME_BUFFER
                frame_buffer_draw_point(x0 + j, y0 + i, fc);
#else
                gui_DrawPoint(x0 + j, y0 + i, fc);
#endif
            }
        }
    }
}







