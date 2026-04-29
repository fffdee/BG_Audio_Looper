/**
 * @file fat32_diskio.h
 * @brief FAT32 底层磁盘IO抽象层
 *
 * 将 FAT32 文件系统与具体存储设备解耦。
 * 支持多种存储后端: SD卡(SDIO), NAND Flash, NOR Flash 等。
 * FAT32 读取器通过本接口访问存储设备，不再直接调用 HAL_SD_xxx。
 */

#ifndef __FAT32_DISKIO_H__
#define __FAT32_DISKIO_H__

#include "product_def.h"

#if FAT32_EN

#include <stdint.h>
#include <stdbool.h>
#include "err_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 存储后端类型枚举
 * ============================================ */
typedef enum {
    FAT32_DISK_SDCARD = 0,     /* SD卡 (SDIO接口) */
    FAT32_DISK_NAND,           /* NAND Flash (SPI接口, W25N02) */
    FAT32_DISK_NOR,            /* NOR Flash (SPI接口, W25Qxx) */
    FAT32_DISK_MAX
} FAT32_DiskType_t;

/* ============================================
 * 磁盘IO操作接口 (函数指针表)
 * ============================================ */
typedef struct {
    /**
     * 初始化存储设备
     * @return SUCCESS 或错误码
     */
    BG_ERR (*init)(void);

    /**
     * 反初始化存储设备
     */
    void (*deinit)(void);

    /**
     * 读取一个扇区 (512字节)
     * @param sector  逻辑扇区号 (LBA)
     * @param buffer  输出缓冲区 (至少512字节)
     * @param count   扇区数量
     * @return SUCCESS 或错误码
     */
    BG_ERR (*read_sectors)(uint32_t sector, uint8_t *buffer, uint32_t count);

    /**
     * 写入一个扇区 (512字节)
     * @param sector  逻辑扇区号 (LBA)
     * @param buffer  数据缓冲区 (512字节)
     * @param count   扇区数量
     * @return SUCCESS 或错误码
     */
    BG_ERR (*write_sectors)(uint32_t sector, const uint8_t *buffer, uint32_t count);

    /**
     * 检查设备是否就绪
     * @return true=就绪, false=未就绪
     */
    bool (*is_ready)(void);

    /**
     * 获取设备总扇区数
     * @return 总扇区数 (每扇区512字节)
     */
    uint32_t (*get_sector_count)(void);

} FAT32_DiskIO_t;

/* ============================================
 * 全局磁盘IO管理
 * ============================================ */

/**
 * 注册磁盘IO驱动
 * @param type    存储后端类型
 * @param diskio  驱动函数指针表
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_DiskIO_Register(FAT32_DiskType_t type, const FAT32_DiskIO_t *diskio);

/**
 * 选择当前活跃的磁盘IO驱动
 * @param type  存储后端类型
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_DiskIO_Select(FAT32_DiskType_t type);

/**
 * 获取当前活跃的磁盘IO驱动
 * @return 当前驱动指针, NULL表示未设置
 */
const FAT32_DiskIO_t* FAT32_DiskIO_GetCurrent(void);

/**
 * 获取当前活跃的磁盘类型
 * @return 当前磁盘类型
 */
FAT32_DiskType_t FAT32_DiskIO_GetCurrentType(void);

/* ============================================
 * 预置存储后端驱动 (各平台提供)
 * ============================================ */

/** SD卡 (SDIO) 驱动 */
extern const FAT32_DiskIO_t fat32_diskio_sdcard;

/** NAND Flash 驱动 (W25N02, 需格式化为FAT32) */
extern const FAT32_DiskIO_t fat32_diskio_nand;

#ifdef __cplusplus
}
#endif

#endif /* FAT32_EN */

#endif /* __FAT32_DISKIO_H__ */
