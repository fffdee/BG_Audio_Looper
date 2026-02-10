/**
 *****************************************************************************
 * @file     drv_w25qxx.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    W25Qxx Flash driver framework adaptation layer header file
 *****************************************************************************
 */

#ifndef __DRV_W25QXX_H__
#define __DRV_W25QXX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

/*******************************************************************************
 * Function declarations
 ******************************************************************************/

/**
 * @brief  Register W25Qxx driver to driver framework
 * @retval 0-success, <0-failure
 * 
 * @note   After registration, will create in file system:
 *         /driver/spi/w25qxx/
 *         ├── name          (driver name)
 *         ├── capacity      (Flash capacity)
 *         ├── page_size     (page size)
 *         ├── sector_size   (sector size)
 *         ├── status        (initialization status)
 *         ├── device_id     (device ID)
 *         └── erase_chip    (chip erase command)
 */
int W25qxx_DrvRegister(void);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_W25QXX_H__ */
