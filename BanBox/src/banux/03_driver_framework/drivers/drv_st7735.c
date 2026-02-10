/**
 *****************************************************************************
 * @file     drv_st7735.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    ST7735 LCD driver framework adaptation layer
 *****************************************************************************
 * @attention
 *
 * Register ST7735 LCD driver to driver framework, providing:
 * 1. Driver registered to /driver/spi/st7735
 * 2. Parameter nodes: width/height/name etc.
 * 3. Shell command access: cat /driver/spi/st7735/width
 *
 *****************************************************************************
 */

#include "drv_st7735.h"
#include "drv_device.h"
#include "drv_fs.h"
#include "st7735.h"
#include "bg_lcd.h"
#include "debug.h"  /* For DBG macro */
#include <stdio.h>
#include <string.h>

/*******************************************************************************
 * Private data structures
 ******************************************************************************/
typedef struct {
    uint16_t width;
    uint16_t height;
    bool initialized;
    char name[32];
} St7735PrivData_t;

static St7735PrivData_t g_st7735_priv = {
    .width = LCD_WIDTH,
    .height = LCD_HEIGHT,
    .initialized = false,
    .name = "ST7735-128x160"
};

/*******************************************************************************
 * Parameter read/write callback functions
 ******************************************************************************/

static int param_get_name(char *buf, uint16_t maxLen, void *userData)
{
    St7735PrivData_t *priv = (St7735PrivData_t *)userData;
    snprintf(buf, maxLen, "%s", priv->name);
    return strlen(buf);
}

static int param_get_width(char *buf, uint16_t maxLen, void *userData)
{
    St7735PrivData_t *priv = (St7735PrivData_t *)userData;
    snprintf(buf, maxLen, "%u", priv->width);
    return strlen(buf);
}

static int param_get_height(char *buf, uint16_t maxLen, void *userData)
{
    St7735PrivData_t *priv = (St7735PrivData_t *)userData;
    snprintf(buf, maxLen, "%u", priv->height);
    return strlen(buf);
}

static int param_get_status(char *buf, uint16_t maxLen, void *userData)
{
    St7735PrivData_t *priv = (St7735PrivData_t *)userData;
    snprintf(buf, maxLen, "%s", priv->initialized ? "initialized" : "uninitialized");
    return strlen(buf);
}

static int param_set_brightness(const char *value, void *userData)
{
    uint32_t brightness = atoi(value);
    if (brightness > 100) {
        return -1;
    }
    // TODO: Set LCD brightness
    return 0;
}

/*******************************************************************************
 * Parameter definition table
 ******************************************************************************/
static const FsParamDef_t st7735_params[] = {
    {
        .name = "name",
        .desc = "LCD driver name",
        .get = param_get_name,
        .set = NULL,  // read-only
    },
    {
        .name = "width",
        .desc = "LCD width (pixels)",
        .get = param_get_width,
        .set = NULL,  // read-only
    },
    {
        .name = "height",
        .desc = "LCD height (pixels)",
        .get = param_get_height,
        .set = NULL,  // read-only
    },
    {
        .name = "status",
        .desc = "Initialization status",
        .get = param_get_status,
        .set = NULL,  // read-only
    },
    {
        .name = "brightness",
        .desc = "Screen brightness (0-100)",
        .get = NULL,
        .set = param_set_brightness,  // write-only
    },
    FS_PARAM_END
};

/*******************************************************************************
 * Driver operation functions
 ******************************************************************************/

static int st7735_drv_init(void *priv)
{
    St7735PrivData_t *st7735 = (St7735PrivData_t *)priv;
    
    if (st7735->initialized) {
        return 0;  // Already initialized
    }
    
    // Call underlying LCD initialization
    BG_lcd.Init();
    st7735->initialized = true;
    
    return 0;
}

static int st7735_drv_deinit(void *priv)
{
    St7735PrivData_t *st7735 = (St7735PrivData_t *)priv;
    st7735->initialized = false;
    return 0;
}

static int st7735_drv_open(void *priv)
{
    // LCD usually doesn't need open/close operations
    return 0;
}

static int st7735_drv_close(void *priv)
{
    return 0;
}

static int st7735_drv_read(void *priv, uint8_t *buf, uint32_t len)
{
    // ST7735 does not support reading screen content
    return -1;
}

static int st7735_drv_write(void *priv, const uint8_t *buf, uint32_t len)
{
    // Image data can be written through write interface
    // Simply return success here
    return len;
}

static int st7735_drv_ioctl(void *priv, uint32_t cmd, void *arg)
{
    St7735PrivData_t *st7735 = (St7735PrivData_t *)priv;
    
    switch (cmd) {
        case 0x01:  // Clear screen command
            // TODO: Call clear screen function
            break;
        case 0x02:  // Refresh command
            // TODO: Call refresh function
            break;
        default:
            return -1;
    }
    
    return 0;
}

/*******************************************************************************
 * Driver definition
 ******************************************************************************/
/* Note: Cannot use const because isRegistered/fsNode fields need to be modified at runtime */
static DrvDevice_t st7735_driver = {
    .name = "st7735",
    .bus = DRV_BUS_SPI,
    .init = st7735_drv_init,
    .deinit = st7735_drv_deinit,
    .open = st7735_drv_open,
    .close = st7735_drv_close,
    .read = st7735_drv_read,
    .write = st7735_drv_write,
    .ioctl = st7735_drv_ioctl,
    .params = st7735_params,
    .privData = &g_st7735_priv,
};

/*******************************************************************************
 * Driver registration function
 ******************************************************************************/
int St7735_DrvRegister(void)
{
    return DrvDevice_Register(&st7735_driver);
}
