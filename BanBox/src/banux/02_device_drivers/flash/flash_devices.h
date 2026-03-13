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

/* Flash #0 - Looper专用Flash (全部8MB均给Looper使用，无系统分区)
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

/* Flash #0 分区布局 (总共8MB，全部给Looper使用) */
/* 系统参数已迁移到 Flash#1，Flash#0 整片专用于 Looper 录音 */
#define FLASH0_PARTITION_SYSTEM_START   0x000000    /* 保留定义，不再使用 */
#define FLASH0_PARTITION_SYSTEM_SIZE    0x000000    /* 0 - 无系统分区 */

#define FLASH0_PARTITION_LOOPER_START   0x000000    /* Looper从Flash0起始地址开始 */
#define FLASH0_PARTITION_LOOPER_SIZE    0x800000    /* 8MB 全部给Looper */

/* Flash #1 分区布局 (总共8MB) */
#define FLASH1_PARTITION_STORAGE_START  0x000000    /* 存储分区起始 */
#define FLASH1_PARTITION_STORAGE_SIZE   0x800000    /* 8MB */

/*===========================================================================
 * 设备ID定义
 *===========================================================================*/

#define FLASH_DEV_ID_SYSTEM     0   /* Flash #0 */
#define FLASH_DEV_ID_STORAGE    1   /* Flash #1 */

/*===========================================================================
 * 多Flash Looper支持
 *===========================================================================
 * 每颗Flash均可独立承载Looper段，段与Flash绑定后可实现：
 *   播放段A (读Flash#0) + 录制段B (写Flash#1) 同时进行，互不干扰
 *===========================================================================*/

/* 系统中供Looper使用的Flash总数，与硬件CS引脚数一致 */
#define LOOPER_FLASH_DEV_COUNT      2

/* 每颗Flash供Looper使用的地址空间大小 (8MB) */
#define LOOPER_FLASH_DEV_SIZE       0x800000UL

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
 * @brief 按dev_id获取Flash设备指针
 * @param dev_id 设备号 (0 = Flash#0, 1 = Flash#1, ...)
 * @return 设备指针，设备不存在则返回NULL
 */
FlashDevice_t* FlashDevices_GetDevice(uint8_t dev_id);

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

/* Looper分区 (Flash#0 全部8MB) */
FlashStatus_t FlashPartition_LooperRead(uint32_t offset, uint8_t *buf, uint32_t len);
FlashStatus_t FlashPartition_LooperWrite(uint32_t offset, const uint8_t *buf, uint32_t len);
FlashStatus_t FlashPartition_LooperEraseSector(uint32_t offset);
FlashStatus_t FlashPartition_LooperEraseBlock(uint32_t offset);
FlashStatus_t FlashPartition_LooperEraseChip(void);         /* 整片擦除（阻塞，录制前调用） */
FlashStatus_t FlashPartition_LooperEraseChipAsync(void);    /* 整片擦除命令发出后立即返回（非阻塞） */
uint8_t       FlashPartition_LooperIsErasing(void);         /* 轮询 BUSY 位：1=仍在擦除，0=完成 */

/* 存储分区 (Flash#1 全部8MB) */
FlashStatus_t FlashPartition_StorageRead(uint32_t offset, uint8_t *buf, uint32_t len);
FlashStatus_t FlashPartition_StorageWrite(uint32_t offset, const uint8_t *buf, uint32_t len);
FlashStatus_t FlashPartition_StorageEraseSector(uint32_t offset);
FlashStatus_t FlashPartition_StorageEraseBlock(uint32_t offset);

/*===========================================================================
 * 多Flash Looper分区操作API (按dev_id选择Flash)
 *===========================================================================
 * 使用这组API可以让不同段绑定到不同Flash：
 *   段0 → dev_id=0 (Flash#0, CS=GPIOA21)
 *   段1 → dev_id=1 (Flash#1, CS=GPIOA22)
 * 从而实现播放段0(读Flash#0)的同时录制段1(写Flash#1)。
 *===========================================================================*/

/**
 * @brief 从指定Flash的Looper区域读取数据
 * @param dev_id   Flash设备号 (0 ~ LOOPER_FLASH_DEV_COUNT-1)
 * @param offset   相对于该Flash起始地址的偏移 (0 ~ LOOPER_FLASH_DEV_SIZE-1)
 * @param buf      输出缓冲区
 * @param len      读取字节数
 */
FlashStatus_t FlashPartition_LooperReadByDev(uint8_t dev_id, uint32_t offset, uint8_t *buf, uint32_t len);

/**
 * @brief 向指定Flash的Looper区域写入数据
 */
FlashStatus_t FlashPartition_LooperWriteByDev(uint8_t dev_id, uint32_t offset, const uint8_t *buf, uint32_t len);

/**
 * @brief 擦除指定Flash Looper区域的一个扇区
 */
FlashStatus_t FlashPartition_LooperEraseSectorByDev(uint8_t dev_id, uint32_t offset);

/**
 * @brief 对指定Flash执行全片阻塞擦除
 */
FlashStatus_t FlashPartition_LooperEraseChipByDev(uint8_t dev_id);

/**
 * @brief 对指定Flash发出全片擦除命令后立即返回（非阻塞）
 * 调用后需轮询 FlashPartition_LooperIsErasingByDev() 等待完成
 */
FlashStatus_t FlashPartition_LooperEraseChipAsyncByDev(uint8_t dev_id);

/**
 * @brief 查询指定Flash是否仍在执行擦除
 * @return 1=仍在擦除，0=空闲（或设备无效）
 */
uint8_t FlashPartition_LooperIsErasingByDev(uint8_t dev_id);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_DEVICES_H__ */
