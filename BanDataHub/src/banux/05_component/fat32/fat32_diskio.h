/**
 * @file fat32_diskio.h
 * @brief BanDataHub FAT block device abstraction.
 */
#ifndef __BANDATAHUB_FAT32_DISKIO_H__
#define __BANDATAHUB_FAT32_DISKIO_H__

#include "product_def.h"

#if FAT32_EN

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BG_ERR_DEFINED
#define BG_ERR_DEFINED
typedef enum {
    SUCCESS = 0,
    ENABLE_INVALID_INPUT,
    ENABLE_OUT_OF_MEMORY,
    ENABLE_NOT_FOUND,
    ENABLE_IO_ERROR,
    ENABLE_DEVICE_NOT_READY,
    ENABLE_OVERFLOW,
    ENABLE_ALREADY_EXISTS,
} BG_ERR;
#endif

typedef enum {
    FAT32_DISK_SDCARD = 0,
    FAT32_DISK_MAX
} FAT32_DiskType_t;

typedef struct {
    BG_ERR (*init)(void);
    void   (*deinit)(void);
    BG_ERR (*read_sectors)(uint32_t sector, uint8_t *buffer, uint32_t count);
    BG_ERR (*write_sectors)(uint32_t sector, const uint8_t *buffer, uint32_t count);
    bool   (*is_ready)(void);
    uint32_t (*get_sector_count)(void);
} FAT32_DiskIO_t;

BG_ERR FAT32_DiskIO_Register(FAT32_DiskType_t type, const FAT32_DiskIO_t *diskio);
BG_ERR FAT32_DiskIO_Select(FAT32_DiskType_t type);
const FAT32_DiskIO_t* FAT32_DiskIO_GetCurrent(void);
FAT32_DiskType_t FAT32_DiskIO_GetCurrentType(void);

extern const FAT32_DiskIO_t fat32_diskio_sdcard;

#ifdef __cplusplus
}
#endif

#endif /* FAT32_EN */

#endif /* __BANDATAHUB_FAT32_DISKIO_H__ */
