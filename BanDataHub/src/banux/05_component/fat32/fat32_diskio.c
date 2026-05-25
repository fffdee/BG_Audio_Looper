/**
 * @file fat32_diskio.c
 * @brief BanDataHub FAT block device abstraction.
 */
#include "product_def.h"

#if FAT32_EN

#include "fat32_diskio.h"
#include "hal_sdio.h"
#include <string.h>

static const FAT32_DiskIO_t *s_diskio[FAT32_DISK_MAX];
static FAT32_DiskType_t s_current_type = FAT32_DISK_SDCARD;
static const FAT32_DiskIO_t *s_current = 0;

BG_ERR FAT32_DiskIO_Register(FAT32_DiskType_t type, const FAT32_DiskIO_t *diskio)
{
    if ((type >= FAT32_DISK_MAX) || !diskio) {
        return ENABLE_INVALID_INPUT;
    }
    s_diskio[type] = diskio;
    return SUCCESS;
}

BG_ERR FAT32_DiskIO_Select(FAT32_DiskType_t type)
{
    if (type >= FAT32_DISK_MAX) {
        return ENABLE_INVALID_INPUT;
    }
    if (!s_diskio[type]) {
        return ENABLE_DEVICE_NOT_READY;
    }
    s_current_type = type;
    s_current = s_diskio[type];
    return SUCCESS;
}

const FAT32_DiskIO_t* FAT32_DiskIO_GetCurrent(void)
{
    return s_current;
}

FAT32_DiskType_t FAT32_DiskIO_GetCurrentType(void)
{
    return s_current_type;
}

static BG_ERR sd_init(void)
{
    HAL_SD_CardInfo_t info;
    return (HAL_SD_GetInfo(&info) == HAL_SD_OK) ? SUCCESS : ENABLE_DEVICE_NOT_READY;
}

static void sd_deinit(void)
{
}

static BG_ERR sd_read(uint32_t sector, uint8_t *buffer, uint32_t count)
{
    HAL_SD_Error_t ret;
    if (!buffer || count == 0u) {
        return ENABLE_INVALID_INPUT;
    }
    ret = HAL_SD_ReadBlocks(sector, buffer, count);
    return (ret == HAL_SD_OK) ? SUCCESS : ENABLE_IO_ERROR;
}

static BG_ERR sd_write(uint32_t sector, const uint8_t *buffer, uint32_t count)
{
    HAL_SD_Error_t ret;
    if (!buffer || count == 0u) {
        return ENABLE_INVALID_INPUT;
    }
    ret = HAL_SD_WriteBlocks(sector, buffer, count);
    return (ret == HAL_SD_OK) ? SUCCESS : ENABLE_IO_ERROR;
}

static bool sd_ready(void)
{
    HAL_SD_CardInfo_t info;
    return (HAL_SD_GetInfo(&info) == HAL_SD_OK);
}

static uint32_t sd_sector_count(void)
{
    HAL_SD_CardInfo_t info;
    if (HAL_SD_GetInfo(&info) != HAL_SD_OK) {
        return 0;
    }
    return info.block_count;
}

const FAT32_DiskIO_t fat32_diskio_sdcard = {
    sd_init,
    sd_deinit,
    sd_read,
    sd_write,
    sd_ready,
    sd_sector_count
};

#endif /* FAT32_EN */
