/**
 *****************************************************************************
 * @file     drv_st7735.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    ST7735 LCD驱动框架适配层
 *****************************************************************************
 * @attention
 *
 * 将ST7735 LCD驱动注册到驱动框架，提供：
 * 1. 驱动注册到/driver/spi/st7735
 * 2. 参数节点：width/height/name等
 * 3. Shell命令访问: cat /driver/spi/st7735/width
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
 * 私有数据结构
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
 * 参数读写回调函数
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
    // TODO: 设置LCD亮度
    return 0;
}

/*******************************************************************************
 * 参数定义表
 ******************************************************************************/
static const FsParamDef_t st7735_params[] = {
    {
        .name = "name",
        .desc = "LCD驱动名称",
        .get = param_get_name,
        .set = NULL,  // 只读
    },
    {
        .name = "width",
        .desc = "LCD宽度(像素)",
        .get = param_get_width,
        .set = NULL,  // 只读
    },
    {
        .name = "height",
        .desc = "LCD高度(像素)",
        .get = param_get_height,
        .set = NULL,  // 只读
    },
    {
        .name = "status",
        .desc = "初始化状态",
        .get = param_get_status,
        .set = NULL,  // 只读
    },
    {
        .name = "brightness",
        .desc = "屏幕亮度(0-100)",
        .get = NULL,
        .set = param_set_brightness,  // 只写
    },
    FS_PARAM_END
};

/*******************************************************************************
 * 驱动操作函数
 ******************************************************************************/

static int st7735_drv_init(void *priv)
{
    St7735PrivData_t *st7735 = (St7735PrivData_t *)priv;
    
    if (st7735->initialized) {
        return 0;  // 已初始化
    }
    
    // 调用底层LCD初始化
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
    // LCD通常不需要open/close操作
    return 0;
}

static int st7735_drv_close(void *priv)
{
    return 0;
}

static int st7735_drv_read(void *priv, uint8_t *buf, uint32_t len)
{
    // ST7735不支持读取屏幕内容
    return -1;
}

static int st7735_drv_write(void *priv, const uint8_t *buf, uint32_t len)
{
    // 可以通过write接口写入图像数据
    // 这里简单返回成功
    return len;
}

static int st7735_drv_ioctl(void *priv, uint32_t cmd, void *arg)
{
    St7735PrivData_t *st7735 = (St7735PrivData_t *)priv;
    
    switch (cmd) {
        case 0x01:  // 清屏命令
            // TODO: 调用清屏函数
            break;
        case 0x02:  // 刷新命令
            // TODO: 调用刷新函数
            break;
        default:
            return -1;
    }
    
    return 0;
}

/*******************************************************************************
 * 驱动定义
 ******************************************************************************/
/* 注意：不能用const，因为需要在运行时修改isRegistered/fsNode等字段 */
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
 * 驱动注册函数
 ******************************************************************************/
int St7735_DrvRegister(void)
{
    return DrvDevice_Register(&st7735_driver);
}
