#include "bg_lcd.h"
#include "spim.h"
#include "spi_flash.h"
#include "debug.h"
#include "spim_interface.h"
#include "dma.h"
#include "st7735.h"
#include "font.h"
#include "delay.h"
#include <stdlib.h>
#include <math.h>

#define _USE_MATH_DEFINES

// 鐢呯处閸愯尙娴夐崗鍐插綁闁插繐鎷伴崙鑺ユ殶
#ifdef USE_FRAME_BUFFER
uint16_t frame_buffer[FRAME_BUFFER_SIZE];
uint8_t frame_buffer_dirty = 0;

// 鐢呯处閸愭彃鍤遍弫鏉匡紣閺勶拷void frame_buffer_flush(void);
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
	.Circle = gui_Circle,  // 閸﹀棗鑸扮紒妯哄煑閺嗗倹妞傛穱婵囧瘮閸樼喐鐗�	.DrawLine = frame_buffer_draw_line,
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
	DelayMs(10);             // Wait 10ms
	LCD_RST_ENABLE();        // Pull reset high
	DelayMs(120);            // Wait 120ms for LCD to stabilize

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
 * @brief 鐠佸墽鐤哃CD鐏炲繐绠烽弮瀣祮閺傜懓鎮� * @param rotation 閺冨娴嗙憴鎺戝 (0=0鎺� 1=90鎺� 2=180鎺� 3=270鎺�
 */
void Lcd_SetRotation(uint8_t rotation)
{
	uint8_t madctl_value;
	
	switch (rotation % 4)
	{
		case 0:  // 0鎺�- 缁旀牕鐫�			madctl_value = 0xA0;  // MY=1, MX=0, MV=1, ML=0, RGB=0
			break;
		case 1:  // 90鎺�- 濡亜鐫�			madctl_value = 0x60;  // MY=0, MX=1, MV=1, ML=0, RGB=0
			break;
		case 2:  // 180鎺�- 缁旀牕鐫嗛崐鎺旂枂
			madctl_value = 0x00;  // MY=0, MX=0, MV=0, ML=0, RGB=0
			break;
		case 3:  // 270鎺�- 濡亜鐫嗛崐鎺旂枂
			madctl_value = 0xC0;  // MY=1, MX=1, MV=0, ML=0, RGB=0
			break;
		default:
			madctl_value = 0xA0;  // 姒涙顓�鎺�			break;
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
//閻㈣崵鍤庨崙鑺ユ殶閿涘奔濞囬悽藱resenham 閻㈣崵鍤庣粻妤佺《
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
	dx = x1-x0;//鐠侊紕鐣粁鐠烘繄顬�	dy = y1-y0;//鐠侊紕鐣粂鐠烘繄顬�
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
		for (index=0; index <= dx; index++)//鐟曚胶鏁鹃惃鍕仯閺侀绗夋导姘崇Т鏉╁櫄鐠烘繄顬�
			{
			//閻㈣崵鍋�
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
			x0+=x_inc;//x閸ф劖鐖ｉ崐鍏肩槨濞嗭紕鏁鹃悙鐟版倵闁粙锟芥晶锟�
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
  // 鐏忓棜顫楁惔锕佹祮閹诡澀璐熷褍瀹�
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
      int x = (int)(X + R * cos(angle_radians)); // 鐠侊紕鐣婚崷鍡曠瑐閻ㄥ嫮鍋ｉ崸鎰垼
      int y = (int)(Y + R * sin(angle_radians));

      int x2 = (int)(X + (R-R/4) * cos(angle_radians));
      int y2 = (int)(Y + (R-R/4) * sin(angle_radians));


      Gui_DrawLine(x,y,x2,y2,fc);
  }





}

void Gui_Circle2(uint16_t X,uint16_t Y,uint16_t R,uint16_t fc) {//Bresenham缁犳纭�
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

//
// void Lcd_Clear(uint16_t x, uint16_t y, uint16_t w, uint16_t h,uint16_t bc)
// {
//    unsigned int i,m;
//	uint16_t x_end, y_end;
//	x_end = x + w;
//	y_end = y + h;
//	Lcd_SetRegion(x,y,x+w,y+h);
//    Lcd_WriteIndex(0x2C);
//    for(i=x;i<x_end;i++)
//    for(m=y;m<y_end;m++)
//    {
//	  	  LCD_WriteData_16Bit(bc);
//    }
// }
//void Gui_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h,uint16_t bc){
//	/* 婵夘偄鍘栭惌鈺佽埌 */
//	uint16_t i,;
//
//	 for (i = 0; i < h; i++) {
//	 	Gui_DrawLine(x, y + i, x + w - 1, y + i, bc);
//	 }
//}

 void Gui_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t bc) {
     uint32_t total = w * h;
     uint32_t i;

     Lcd_SetRegion(x, y, x + w - 1, y + h - 1);
     Lcd_WriteIndex(0x2C);
     for (i = 0; i < total; i++) {
         LCD_WriteData_16Bit(bc);
     }
 }

void Gui_box_border(uint16_t x, uint16_t y, uint16_t w, uint16_t h,uint16_t bc){
	/* 閸欘亞鏁炬潏瑙勵攱 (閸樼喐娼甸惃鍑i_box閸旂喕鍏� */
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




//閸欐牗膩閺傜懓绱�濮樻潙閽╅幍顐ｅ伎 娴犲骸涔忛崚鏉垮礁 娴ｅ簼缍呴崷銊ュ
void showimage(uint8_t x,uint8_t y,uint8_t width,uint8_t high,const uint8_t *p){ //鏄剧ず40*40 QQ鍥惧儚
  	int row, col;  // 鏀逛负琛屻�鍒楀懡鍚嶏紝鏇存竻鏅�
  	unsigned char picH,picL;
	//Lcd_Clear(BLACK); //娓呭睆

	// 淇鍚庣殑鏄剧ず鍖哄煙璁剧疆
	Lcd_SetRegion(x+2, y, (x+2)+width-1, y+high-1);		//鍧愭爣璁剧疆

	// 鍙屽眰寰幆锛氬厛琛屽悗鍒楋紝璐村悎ST7735 GRAM濉厖閫昏緫
	for(row=0; row<high; row++)  // 閬嶅巻姣忎竴琛�
		{
	    for(col=0; col<width; col++)  // 閬嶅巻褰撳墠琛岀殑姣忎竴鍒�
	    	{
	        // 鎸夎浼樺厛璇诲彇鏁版嵁锛屼笌鏄剧ず椤哄簭瀹屽叏鍖归厤
	        picL=*(p + row*width*2 + col*2);	//鏁版嵁浣庝綅鍦ㄥ墠
	        picH=*(p + row*width*2 + col*2 + 1);
	        LCD_WriteData_16Bit(picH<<8|picL);
	        //delay(1);
	    }
	}
}


/**************************************************************************************
閸旂喕鍏橀幓蹇氬牚: 閸︺劌鐫嗛獮鏇熸▔缁�桨绔撮崙姝屾崳閻ㄥ嫭瀵滈柦顔筋攱
鏉堬拷   閸忥拷 uint16_t x1,y1,x2,y2 閹稿鎸冲鍡椾箯娑撳﹨顫楅崪灞藉礁娑撳顫楅崸鎰垼
鏉堬拷   閸戯拷 閺冿拷**************************************************************************************/
void DisplayButtonDown(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2){
	Gui_DrawLine(x1,  y1,  x2,y1, GRAY2);  //H
	Gui_DrawLine(x1+1,y1+1,x2,y1+1, GRAY1);  //H
	Gui_DrawLine(x1,  y1,  x1,y2, GRAY2);  //V
	Gui_DrawLine(x1+1,y1+1,x1+1,y2, GRAY1);  //V
	Gui_DrawLine(x1,  y2,  x2,y2, WHITE);  //H
	Gui_DrawLine(x2,  y1,  x2,y2, WHITE);  //V
}

/**************************************************************************************
閸旂喕鍏橀幓蹇氬牚: 閸︺劌鐫嗛獮鏇熸▔缁�桨绔撮崙閫涚瑓閻ㄥ嫭瀵滈柦顔筋攱
鏉堬拷   閸忥拷 uint16_t x1,y1,x2,y2 閹稿鎸冲鍡椾箯娑撳﹨顫楅崪灞藉礁娑撳顫楅崸鎰垼
鏉堬拷   閸戯拷 閺冿拷**************************************************************************************/
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

// gui_Circle閸戣姤鏆熺�鐐靛箛 - 娴ｈ法鏁resenham缁犳纭堕悽璇叉妇
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

// ShowChar閸戣姤鏆熺�鐐靛箛 - 閺勫墽銇氶崡鏇氶嚋鐎涙顑�(8x16閸嶅繒绀�
void ShowChar(uint16_t x0, uint16_t y0, uint8_t chr, uint16_t fc) {
    uint8_t i, j;
    uint8_t temp;

    // 鐎涙顑侀懠鍐ㄦ纯濡拷鐓￠敍瀹巗c16鐎涙ぞ缍嬫禒搴ｂ敄閺嶏拷0x20)瀵拷顫�    if (chr < 0x20 || chr > 0x7E) {
        chr = '?';  // 娑撳秵鏁幐浣烘畱鐎涙顑侀弰鍓с仛闂傤喖褰�    }

    // 閼惧嘲褰囩�妤冾儊閸︺劌鐡ф担鎾存殶閹诡喕鑵戦惃鍕焊缁夛拷(濮ｅ繋閲滅�妤冾儊16鐎涙濡�
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







