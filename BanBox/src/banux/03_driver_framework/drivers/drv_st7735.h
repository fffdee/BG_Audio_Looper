/**
 *****************************************************************************
 * @file     drv_st7735.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    ST7735 LCD driver framework adaptation layer header file
 *****************************************************************************
 */

#ifndef __DRV_ST7735_H__
#define __DRV_ST7735_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

/*******************************************************************************
 * Function declarations
 ******************************************************************************/

/**
 * @brief  Register ST7735 driver to driver framework
 * @retval 0-success, <0-failure
 * 
 * @note   After registration, will create in file system:
 *         /driver/spi/st7735/
 *         ├── name       (driver name)
 *         ├── width      (LCD width)
 *         ├── height     (LCD height)
 *         ├── status     (initialization status)
 *         └── brightness (brightness control)
 */
int St7735_DrvRegister(void);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_ST7735_H__ */
