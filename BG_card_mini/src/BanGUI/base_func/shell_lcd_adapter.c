/**
 *****************************************************************************
 * @file     shell_lcd_adapter.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     16-December-2025
 * @brief    Shell LCD Adapter - Connect Shell console with BG_lcd driver
 *****************************************************************************
 */

#include "shell_lcd_adapter.h"
#include "bg_shell.h"
#include "bg_lcd.h"
#include "gui_tool.h"

/* External 6x8 small font function declaration */
extern void Gui_ShowString6x8(uint16_t x0, uint16_t y0, uint8_t *chr, uint16_t fc);

/* External fill rectangle function declaration */
extern void Lcd_Clear_Section(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, uint16_t Color);

/*******************************************************************************
 * LCD Adapter Function Implementation (Adapt BG_lcd to Shell LCD interface)
 ******************************************************************************/

/**
 * @brief  Clear screen adapter function
 */
static void LCD_Adapter_Clear(uint16_t color)
{
    BG_lcd.Clear(color);
}

/**
 * @brief  Fill rectangle adapter function - use Lcd_Clear_Section for actual fill
 */
static void LCD_Adapter_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    /* Lcd_Clear_Section actually fills the rectangle, unlike BG_lcd.Box which only draws borders */
    Lcd_Clear_Section((uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)h, color);
}

/**
 * @brief  Draw string adapter function - use 6x8 small font for console
 */
static void LCD_Adapter_DrawString(uint16_t x, uint16_t y, const char* str, uint16_t color)
{
    Gui_ShowString6x8(x, y, (uint8_t*)str, color);
}

/**
 * @brief  Get screen size adapter function
 */
static void LCD_Adapter_GetSize(uint16_t* width, uint16_t* height)
{
    if (width) *width = LCD_WIDTH;
    if (height) *height = LCD_HEIGHT;
}

/*******************************************************************************
 * Shell LCD Interface Instance
 ******************************************************************************/
static const ShellLCD_t g_ShellLCD = {
    .clear      = LCD_Adapter_Clear,
    .fillRect   = LCD_Adapter_FillRect,
    .drawString = LCD_Adapter_DrawString,
    .getSize    = LCD_Adapter_GetSize
};

/*******************************************************************************
 * API Implementation
 ******************************************************************************/

bool ShellLCD_Adapter_Init(void)
{
    return Shell_SetLCD(&g_ShellLCD);
}

void ShellLCD_Console_Enable(bool enable)
{
    Shell_ConsoleEnable(enable);
}

bool ShellLCD_Console_IsEnabled(void)
{
    return Shell_ConsoleIsEnabled();
}
