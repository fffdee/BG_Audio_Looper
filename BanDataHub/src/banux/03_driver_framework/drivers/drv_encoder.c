#include "drv_encoder.h"
#include "drv_device.h"
#include "drv_fs.h"
#include "rotary_encoder.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "product_def.h"

#if HW_DRV_ENCODER_EN

/**
 * @brief 编码器驱动私有数据
 */
typedef struct {
    int16_t  delta;         // 累计旋转增量
    uint8_t  last_event;    // 最近事件类型
    bool     btn_pressed;   // 短按标志
    bool     btn_long;      // 长按标志
    bool     initialized;   // 初始化标志
} EncoderPrivData_t;

// 全局私有数据
static EncoderPrivData_t g_encoder_priv = {
    .delta = 0,
    .last_event = ENCODER_EVT_NONE,
    .btn_pressed = false,
    .btn_long = false,
    .initialized = false,
};

/*****************************************************************************
 * 参数读取回调函数
 *****************************************************************************/

static int param_get_name(char *buf, uint16_t maxLen, void *userData)
{
    snprintf(buf, maxLen, "Rotary_Encoder");
    return strlen(buf);
}

static int param_get_delta(char *buf, uint16_t maxLen, void *userData)
{
    EncoderPrivData_t *priv = (EncoderPrivData_t *)userData;
    priv->delta = RotaryEncoder_GetDelta();
    snprintf(buf, maxLen, "%d", priv->delta);
    return strlen(buf);
}

static int param_get_event(char *buf, uint16_t maxLen, void *userData)
{
    EncoderPrivData_t *priv = (EncoderPrivData_t *)userData;
    const char *evt_str;
    switch (priv->last_event) {
        case ENCODER_EVT_CW:         evt_str = "CW"; break;
        case ENCODER_EVT_CCW:        evt_str = "CCW"; break;
        case ENCODER_EVT_CLICK:      evt_str = "CLICK"; break;
        case ENCODER_EVT_LONG_PRESS: evt_str = "LONG_PRESS"; break;
        default:                     evt_str = "NONE"; break;
    }
    snprintf(buf, maxLen, "%s", evt_str);
    return strlen(buf);
}

static int param_get_btn_pressed(char *buf, uint16_t maxLen, void *userData)
{
    EncoderPrivData_t *priv = (EncoderPrivData_t *)userData;
    priv->btn_pressed = RotaryEncoder_IsButtonPressed();
    snprintf(buf, maxLen, "%u", priv->btn_pressed ? 1 : 0);
    return strlen(buf);
}

static int param_get_btn_long(char *buf, uint16_t maxLen, void *userData)
{
    EncoderPrivData_t *priv = (EncoderPrivData_t *)userData;
    priv->btn_long = RotaryEncoder_IsButtonLongPressed();
    snprintf(buf, maxLen, "%u", priv->btn_long ? 1 : 0);
    return strlen(buf);
}

static int param_cmd_reset(const char *value, void *userData)
{
    EncoderPrivData_t *priv = (EncoderPrivData_t *)userData;
    priv->delta = 0;
    priv->last_event = ENCODER_EVT_NONE;
    priv->btn_pressed = false;
    priv->btn_long = false;
    return 0;
}

/*****************************************************************************
 * 参数定义表
 *****************************************************************************/
static const FsParamDef_t encoder_params[] = {
    {
        .name = "name",
        .desc = "设备名称",
        .get = param_get_name,
        .set = NULL,
    },
    {
        .name = "delta",
        .desc = "旋转增量",
        .get = param_get_delta,
        .set = NULL,
    },
    {
        .name = "event",
        .desc = "最近事件(NONE/CW/CCW/CLICK/LONG_PRESS)",
        .get = param_get_event,
        .set = NULL,
    },
    {
        .name = "btn_pressed",
        .desc = "按钮短按标志(0/1)",
        .get = param_get_btn_pressed,
        .set = NULL,
    },
    {
        .name = "btn_long",
        .desc = "按钮长按标志(0/1)",
        .get = param_get_btn_long,
        .set = NULL,
    },
    {
        .name = "reset",
        .desc = "重置状态(写入任意值触发)",
        .get = NULL,
        .set = param_cmd_reset,
    },
    FS_PARAM_END
};

/*****************************************************************************
 * 驱动操作函数
 *****************************************************************************/

static int encoder_drv_init(void *priv)
{
    EncoderPrivData_t *enc_priv = (EncoderPrivData_t *)priv;
    if (enc_priv->initialized) {
        return 0;
    }
    /* RotaryEncoder_Init() 已在 main.c power_on() 中调用，
       这里只标记框架层初始化完成 */
    enc_priv->initialized = true;
    return 0;
}

static int encoder_drv_deinit(void *priv)
{
    EncoderPrivData_t *enc_priv = (EncoderPrivData_t *)priv;
    enc_priv->initialized = false;
    return 0;
}

static int encoder_drv_open(void *priv)
{
    return 0;
}

static int encoder_drv_close(void *priv)
{
    return 0;
}

static int encoder_drv_read(void *priv, uint8_t *buf, uint32_t len)
{
    EncoderPrivData_t *enc_priv = (EncoderPrivData_t *)priv;
    int written;

    enc_priv->delta = RotaryEncoder_GetDelta();
    enc_priv->btn_pressed = RotaryEncoder_IsButtonPressed();
    enc_priv->btn_long = RotaryEncoder_IsButtonLongPressed();

    written = snprintf((char *)buf, len,
        "Encoder Info:\n"
        "  Delta: %d\n"
        "  Button: %s\n"
        "  Long Press: %s\n",
        enc_priv->delta,
        enc_priv->btn_pressed ? "PRESSED" : "RELEASED",
        enc_priv->btn_long ? "YES" : "NO"
    );

    return (written > 0) ? written : 0;
}

static int encoder_drv_ioctl(void *priv, uint32_t cmd, void *arg)
{
    EncoderPrivData_t *enc_priv = (EncoderPrivData_t *)priv;

    switch (cmd) {
        case 0x01:  // 获取旋转增量
            if (arg != NULL) {
                *(int16_t *)arg = RotaryEncoder_GetDelta();
                return 0;
            }
            return -1;

        case 0x02:  // 获取按钮短按标志
            if (arg != NULL) {
                *(uint8_t *)arg = RotaryEncoder_IsButtonPressed();
                return 0;
            }
            return -1;

        case 0x03:  // 获取按钮长按标志
            if (arg != NULL) {
                *(uint8_t *)arg = RotaryEncoder_IsButtonLongPressed();
                return 0;
            }
            return -1;

        case 0x04:  // 重置累计增量
            enc_priv->delta = 0;
            enc_priv->last_event = ENCODER_EVT_NONE;
            return 0;

        default:
            return -1;
    }
}

/*****************************************************************************
 * 驱动设备结构定义
 *****************************************************************************/
static DrvDevice_t encoder_driver = {
    .name = "encoder",
    .bus = DRV_BUS_GPIO,
    .init = encoder_drv_init,
    .deinit = encoder_drv_deinit,
    .open = encoder_drv_open,
    .close = encoder_drv_close,
    .read = encoder_drv_read,
    .write = NULL,
    .ioctl = encoder_drv_ioctl,
    .params = encoder_params,
    .privData = &g_encoder_priv,
};

/*****************************************************************************
 * 对外注册接口
 *****************************************************************************/

int Encoder_DrvRegister(void)
{
    return DrvDevice_Register(&encoder_driver);
}

#endif /* HW_DRV_ENCODER_EN */
