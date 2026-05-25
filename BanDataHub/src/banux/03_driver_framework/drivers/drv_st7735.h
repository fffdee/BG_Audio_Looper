/**
 *****************************************************************************
 * @file     drv_st7735.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    ST7735 LCD驱动框架适配层头文件
 *****************************************************************************
 */

#ifndef __DRV_ST7735_H__
#define __DRV_ST7735_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

/*******************************************************************************
 * 函数声明
 ******************************************************************************/

/**
 * @brief  注册ST7735驱动到驱动框架
 * @retval 0-成功, <0-失败
 * 
 * @note   注册后将在文件系统创建:
 *         /driver/spi/st7735/
 *         ├── name       (驱动名称)
 *         ├── width      (LCD宽度)
 *         ├── height     (LCD高度)
 *         ├── status     (初始化状态)
 *         └── brightness (亮度控制)
 */
#ifndef BANDATAHUB
int St7735_DrvRegister(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __DRV_ST7735_H__ */
