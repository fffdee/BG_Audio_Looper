#include "bg_lcd.h"
#include "spim.h"
#include "spi_flash.h"
#include "debug.h"
#include "spim_interface.h"
#include "dma.h"
#include "st7735.h"
#include <stdlib.h>
#include <math.h>
#define _USE_MATH_DEFINES

void lcd_init(void);
void lcd_write_byte(uint8_t data);
void Lcd_Clear(uint16_t Color);
void gui_DrawPoint(uint16_t x,uint16_t y,uint16_t Data);
void gui_Circle(uint16_t X,uint16_t Y,uint16_t R,uint16_t fc);
void gui_DrawLine(uint16_t x0, uint16_t y0,uint16_t x1, uint16_t y1,uint16_t Color);
void Gui_Speed_Radians(uint8_t X,uint8_t Y,uint8_t R,uint8_t angle,uint16_t fc, bool initFlag);
void Gui_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h,uint16_t bc);
void showimage(uint8_t x,uint8_t y,uint8_t width,uint8_t high,const uint8_t *p);

void DisplayButtonDown(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
void DisplayButtonUp(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);

BG_Lcd BG_lcd = {

	.Init = lcd_init,
	.Clear = Lcd_Clear,
	.DrawPoint = gui_DrawPoint,
	.Circle = gui_Circle,
	.DrawLine = gui_DrawLine,
	.Box = Gui_box,
	.ShowImage = showimage,
	.ButtonUp = DisplayButtonUp,
	.ButtonDown = DisplayButtonDown,
};
void lcd_init(void)
{

	LCD_RST_INIT();
	LCD_DC_INIT();
	LCD_CS_INIT();

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


void gui_Circle(uint16_t X,uint16_t Y,uint16_t R,uint16_t fc) {//Bresenham算法
    unsigned short  a,b;
    int c;
    a=0;
    b=R;
    c=3-2*R;
    while (a<b)
    {
        Gui_DrawPoint(X+a,Y+b,fc);     //        7
        Gui_DrawPoint(X-a,Y+b,fc);     //        6
        Gui_DrawPoint(X+a,Y-b,fc);     //        2
        Gui_DrawPoint(X-a,Y-b,fc);     //        3
        Gui_DrawPoint(X+b,Y+a,fc);     //        8
        Gui_DrawPoint(X-b,Y+a,fc);     //        5
        Gui_DrawPoint(X+b,Y-a,fc);     //        1
        Gui_DrawPoint(X-b,Y-a,fc);     //        4

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
        Gui_DrawPoint(X+a,Y+b,fc);
        Gui_DrawPoint(X+a,Y+b,fc);
        Gui_DrawPoint(X+a,Y-b,fc);
        Gui_DrawPoint(X-a,Y-b,fc);
        Gui_DrawPoint(X+b,Y+a,fc);
        Gui_DrawPoint(X-b,Y+a,fc);
        Gui_DrawPoint(X+b,Y-a,fc);
        Gui_DrawPoint(X-b,Y-a,fc);
    }

}
//画线函数，使用Bresenham 画线算法
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
	dx = x1-x0;//计算x距离
	dy = y1-y0;//计算y距离

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

	if (dx > dy)//x距离大于y距离，那么每个x轴上只有一个点，每个y轴上有若干个点
	{//且线的点数等于x距离，以x轴递增画点
		// initialize error term
		error = dy2 - dx;

		// draw the line
		for (index=0; index <= dx; index++)//要画的点数不会超过x距离
		{
			//画点
			Gui_DrawPoint(x0,y0,Color);

			// test if error has overflowed
			if (error >= 0) //是否需要增加y坐标值
			{
				error-=dx2;

				// move to next line
				y0+=y_inc;//增加y坐标值
			} // end if error overflowed

			// adjust the error term
			error+=dy2;

			// move to the next pixel
			x0+=x_inc;//x坐标值每次画点后都递增1
		} // end for
	} // end if |slope| <= 1
	else//y轴大于x轴，则每个y轴上只有一个点，x轴若干个点
	{//以y轴为递增画点
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
	} // end else |slope| > 1
}



void Gui_Speed_Radians(uint8_t X,uint8_t Y,uint8_t R,uint8_t angle,uint16_t fc, bool initFlag)
{
	unsigned char i;
  // 将角度转换为弧度
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
      int x = (int)(X + R * cos(angle_radians)); // 计算圆上的点坐标
      int y = (int)(Y + R * sin(angle_radians));

      int x2 = (int)(X + (R-R/4) * cos(angle_radians));
      int y2 = (int)(Y + (R-R/4) * sin(angle_radians));


      Gui_DrawLine(x,y,x2,y2,fc);
  }





}

void Gui_Circle2(uint16_t X,uint16_t Y,uint16_t R,uint16_t fc) {//Bresenham算法
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

void Gui_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h,uint16_t bc){
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




//取模方式 水平扫描 从左到右 低位在前
void showimage(uint8_t x,uint8_t y,uint8_t width,uint8_t high,const uint8_t *p){ //显示40*40 QQ图片
  	int i,j,k;
	unsigned char picH,picL;
	//Lcd_Clear(BLACK); //清屏


			Lcd_SetRegion(x+2,y,x+width-1,y+high-1);		//坐标设置
		    for(i=0;i<width*high;i++)
			 {
			 	picL=*(p+i*2);	//数据低位在前
				picH=*(p+i*2+1);
				LCD_WriteData_16Bit(picH<<8|picL);
        //delay(1);
			 }

}


/**************************************************************************************
功能描述: 在屏幕显示一凸起的按钮框
输    入: uint16_t x1,y1,x2,y2 按钮框左上角和右下角坐标
输    出: 无
**************************************************************************************/
void DisplayButtonDown(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2){
	Gui_DrawLine(x1,  y1,  x2,y1, GRAY2);  //H
	Gui_DrawLine(x1+1,y1+1,x2,y1+1, GRAY1);  //H
	Gui_DrawLine(x1,  y1,  x1,y2, GRAY2);  //V
	Gui_DrawLine(x1+1,y1+1,x1+1,y2, GRAY1);  //V
	Gui_DrawLine(x1,  y2,  x2,y2, WHITE);  //H
	Gui_DrawLine(x2,  y1,  x2,y2, WHITE);  //V
}

/**************************************************************************************
功能描述: 在屏幕显示一凹下的按钮框
输    入: uint16_t x1,y1,x2,y2 按钮框左上角和右下角坐标
输    出: 无
**************************************************************************************/
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







