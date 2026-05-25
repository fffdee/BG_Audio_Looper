#ifndef DRV_ENCODER_H
#define DRV_ENCODER_H

#include <stdint.h>

/**
 * @file drv_encoder.h
 * @brief 编码器驱动框架适配层
 *
 * 将底层旋转编码器驱动(rotary_encoder.c)适配到驱动框架，提供统一的访问接口
 *
 * 注册后的文件系统路径:
 *   /driver/gpio/encoder/
 *   ├── name           (只读) - 设备名称 "Rotary_Encoder"
 *   ├── delta          (只读) - 旋转增量
 *   ├── event          (只读) - 最近事件类型
 *   ├── btn_pressed    (只读) - 按钮短按标志
 *   ├── btn_long       (只读) - 按钮长按标志
 *   └── reset          (只写) - 重置状态(写入任意值触发)
 *
 * IOCTL命令:
 *   0x01 - 获取旋转增量到arg指向的int16_t*
 *   0x02 - 获取按钮短按标志到arg指向的uint8_t*
 *   0x03 - 获取按钮长按标志到arg指向的uint8_t*
 *   0x04 - 重置累计增量
 */

/**
 * @brief 注册编码器驱动到驱动框架
 * @return 0成功, 负值失败
 */
int Encoder_DrvRegister(void);

#endif /* DRV_ENCODER_H */
