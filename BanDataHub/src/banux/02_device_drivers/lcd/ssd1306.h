#ifndef __SSD1306_H__
#define __SSD1306_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"
#include "product_def.h"

#if HW_DRV_SSD1306_EN

#define SSD1306_WIDTH           128
#define SSD1306_HEIGHT          64
#define SSD1306_PAGES           8

#define SSD1306_CMD             0x00
#define SSD1306_DATA            0x40

#ifndef HW_SSD1306_I2C_ADDR
#define HW_SSD1306_I2C_ADDR     0x3C
#endif

#ifndef HW_SSD1306_SCL_PIN
#define HW_SSD1306_SCL_PIN      20
#endif

#ifndef HW_SSD1306_SDA_PIN
#define HW_SSD1306_SDA_PIN      21
#endif

typedef struct {
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES];
    uint8_t initialized;
} SSD1306_t;

void SSD1306_Init(void);
void SSD1306_DeInit(void);
void SSD1306_Clear(void);
void SSD1306_Update(void);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void SSD1306_DrawChar(uint8_t x, uint8_t y, char ch, uint8_t size, uint8_t color);
void SSD1306_DrawString(uint8_t x, uint8_t y, const char *str, uint8_t size, uint8_t color);
void SSD1306_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void SSD1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void SSD1306_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void SSD1306_SetContrast(uint8_t contrast);
void SSD1306_DisplayOn(void);
void SSD1306_DisplayOff(void);
void SSD1306_InvertDisplay(uint8_t invert);

#endif /* HW_DRV_SSD1306_EN */

#ifdef __cplusplus
}
#endif

#endif /* __SSD1306_H__ */
