/**
 * @file sd_card_driver.h
 * @brief SD卡驱动适配层
 */

#ifndef SD_CARD_DRIVER_H
#define SD_CARD_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "flash_bus.h"
#include "hal_sdio.h"

#define SD_CARD_BLOCK_SIZE  512

/**
 * @brief 创建SD卡设备
 * @param name 设备名称
 * @return 创建的FlashDevice_t指针，失败返回NULL
 */
FlashDevice_t* SDCard_Create(const char *name);

/**
 * @brief 销毁SD卡设备
 * @param dev 设备指针
 */
void SDCard_Destroy(FlashDevice_t *dev);

/**
 * @brief 获取SD卡操作接口
 * @return SD卡操作接口指针
 */
const FlashOps_t* SDCard_GetOps(void);

#ifdef __cplusplus
}
#endif

#endif /* SD_CARD_DRIVER_H */
