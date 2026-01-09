/**
 * @file    ui_icons.h
 * @brief   UI Icon Resources
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 图标资源定义
 * 
 * 图标格式: RGB565, 16位色深
 * 图标大小: 通常为 32x32 或 16x16 像素
 */

#ifndef __UI_ICONS_H__
#define __UI_ICONS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 图标尺寸定义
 *===========================================================================*/

#define ICON_SIZE_SMALL     16      /* 16x16 */
#define ICON_SIZE_MEDIUM    24      /* 24x24 */
#define ICON_SIZE_LARGE     32      /* 32x32 */

/*===========================================================================
 * 图标资源声明
 * 
 * 实际图标数据在 picture.h 中定义 (保持兼容性)
 *===========================================================================*/

/* 主界面图标 */
extern const unsigned char gImage_setting[];
extern const unsigned char gImage_hardware[];
extern const unsigned char gImage_music[];
extern const unsigned char gImage_looper[];

/* Banner 图像 */
extern const unsigned char gImage_BanBox[];

/*===========================================================================
 * 图标 ID 枚举 (用于通用图标获取接口)
 *===========================================================================*/

typedef enum {
    ICON_ID_SETTINGS = 0,
    ICON_ID_HARDWARE,
    ICON_ID_MUSIC,
    ICON_ID_LOOPER,
    ICON_ID_BLUETOOTH,
    ICON_ID_BATTERY,
    ICON_ID_VOLUME,
    ICON_ID_COUNT
} IconID_t;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 获取图标数据指针
 * @param id 图标 ID
 * @return 图标数据指针，失败返回 NULL
 */
const unsigned char* UI_Icons_Get(IconID_t id);

/**
 * @brief 获取图标尺寸
 * @param id 图标 ID
 * @param width 输出宽度
 * @param height 输出高度
 */
void UI_Icons_GetSize(IconID_t id, uint8_t* width, uint8_t* height);

#ifdef __cplusplus
}
#endif

#endif /* __UI_ICONS_H__ */
