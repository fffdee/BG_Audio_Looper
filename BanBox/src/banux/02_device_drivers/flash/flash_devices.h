/**
 * flash_devices.h - Flash设备定义和注册
 * 
 * 定义系统中所有Flash设备的CS引脚配置和设备实例
 */

#ifndef __FLASH_DEVICES_H__
#define __FLASH_DEVICES_H__

#include "flash_bus.h"
#include "flash_nor_w25qxx.h"
#include "gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 硬件配置 - CS引脚定义
 *===========================================================================*/

/* Flash #0 - 系统Flash (1MB系统 + 7MB Looper)
 * CS引脚: GPIOA21
 */
#define FLASH0_CS_GPIO_INDEX    GPIO_A_IN
#define FLASH0_CS_GPIO_MASK     GPIO_INDEX21
#define FLASH0_CS_PIN           21

/* Flash #1 - 存储Flash (8MB存储)
 * CS引脚: GPIOA22
 */
#define FLASH1_CS_GPIO_INDEX    GPIO_A_IN
#define FLASH1_CS_GPIO_MASK     GPIO_INDEX22
#define FLASH1_CS_PIN           22

/*===========================================================================
 * 分区定义
 *===========================================================================*/

/* Flash #0 分区布局 (总共8MB) */
#define FLASH0_PARTITION_SYSTEM_START   0x000000    /* 系统分区起始 */
#define FLASH0_PARTITION_SYSTEM_SIZE    0x100000    /* 1MB */

#define FLASH0_PARTITION_LOOPER_START   0x100000    /* Looper分区起始 */
#define FLASH0_PARTITION_LOOPER_SIZE    0x700000    /* 7MB */

/* Flash #1 分区布局 (总共8MB) */
#define FLASH1_PARTITION_STORAGE_START  0x000000    /* 存储分区起始 */
#define FLASH1_PARTITION_STORAGE_SIZE   0x800000    /* 8MB */

/*===========================================================================
 * 设备ID定义
 *===========================================================================*/

#define FLASH_DEV_ID_SYSTEM     0   /* Flash #0 */
#define FLASH_DEV_ID_STORAGE    1   /* Flash #1 */

/*===========================================================================
 * API
 *===========================================================================*/

/**
 * @brief 初始化所有Flash设备
 * @return FLASH_OK成功
 */
FlashStatus_t FlashDevices_Init(void);

/**
 * @brief 反初始化所有Flash设备
 */
void FlashDevices_DeInit(void);

/**
 * @brief 获取系统Flash设备
 * @return 设备指针
 */
FlashDevice_t* FlashDevices_GetSystemFlash(void);

/**
 * @brief 获取存储Flash设备
 * @return 设备指针
 */
FlashDevice_t* FlashDevices_GetStorageFlash(void);

/**
 * @brief 注册Shell命令
 */
void FlashDevices_RegisterShellCommands(void);

/*===========================================================================
 * 便捷分区操作API
 *===========================================================================*/

/* 系统分区 (Flash#0 前1MB) */
FlashStatus_t FlashPartition_SystemRead(uint32_t offset, uint8_t *buf, uint32_t len);
FlashStatus_t FlashPartition_SystemWrite(uint32_t offset, const uint8_t *buf, uint32_t len);
FlashStatus_t FlashPartition_SystemEraseSector(uint32_t offset);

/* Looper分区 (Flash#0 后7MB) */
FlashStatus_t FlashPartition_LooperRead(uint32_t offset, uint8_t *buf, uint32_t len);
FlashStatus_t FlashPartition_LooperWrite(uint32_t offset, const uint8_t *buf, uint32_t len);
FlashStatus_t FlashPartition_LooperEraseSector(uint32_t offset);
FlashStatus_t FlashPartition_LooperEraseBlock(uint32_t offset);

/* 存储分区 (Flash#1 全部8MB) */
FlashStatus_t FlashPartition_StorageRead(uint32_t offset, uint8_t *buf, uint32_t len);
FlashStatus_t FlashPartition_StorageWrite(uint32_t offset, const uint8_t *buf, uint32_t len);
FlashStatus_t FlashPartition_StorageEraseSector(uint32_t offset);
FlashStatus_t FlashPartition_StorageEraseBlock(uint32_t offset);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_DEVICES_H__ */
