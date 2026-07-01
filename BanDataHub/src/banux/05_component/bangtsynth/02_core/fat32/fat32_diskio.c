/**
 * @file fat32_diskio.c
 * @brief FAT32 底层磁盘IO抽象层实现
 *
 * 提供存储后端注册/选择功能，以及 SD卡和 NAND Flash 的后端实现。
 */

#include "product_def.h"

#if FAT32_EN

#include "fat32_diskio.h"
#include "hal_sdio.h"
#include "flash_devices.h"
#include <string.h>

/* ============================================
 * 内部状态
 * ============================================ */

static const FAT32_DiskIO_t *g_diskio_drivers[FAT32_DISK_MAX] = { 0 };
static FAT32_DiskType_t g_current_disk = FAT32_DISK_SDCARD;
static const FAT32_DiskIO_t *g_current_driver = 0;

/* ============================================
 * 磁盘IO管理实现
 * ============================================ */

BG_ERR FAT32_DiskIO_Register(FAT32_DiskType_t type, const FAT32_DiskIO_t *diskio)
{
    if (type >= FAT32_DISK_MAX || !diskio) {
        return ENABLE_INVALID_INPUT;
    }
    g_diskio_drivers[type] = diskio;
    return SUCCESS;
}

BG_ERR FAT32_DiskIO_Select(FAT32_DiskType_t type)
{
    if (type >= FAT32_DISK_MAX) {
        return ENABLE_INVALID_INPUT;
    }
    if (!g_diskio_drivers[type]) {
        return ENABLE_DEVICE_NOT_READY;
    }
    g_current_disk = type;
    g_current_driver = g_diskio_drivers[type];
    return SUCCESS;
}

const FAT32_DiskIO_t* FAT32_DiskIO_GetCurrent(void)
{
    return g_current_driver;
}

FAT32_DiskType_t FAT32_DiskIO_GetCurrentType(void)
{
    return g_current_disk;
}

/* ============================================
 * SD卡 (SDIO) 后端实现
 * ============================================ */

static BG_ERR sdcard_init(void)
{
    /* SD卡初始化由 HAL_SDIO 层完成，此处仅验证就绪 */
    uint8_t probe[512];
    if (HAL_SD_ReadBlocks(0, probe, 1) == HAL_SD_OK) {
        return SUCCESS;
    }
    return ENABLE_DEVICE_NOT_READY;
}

static void sdcard_deinit(void)
{
    /* SD卡无需特殊反初始化 */
}

static BG_ERR sdcard_read_sectors(uint32_t sector, uint8_t *buffer, uint32_t count)
{
    HAL_SD_Error_t ret;
    ret = HAL_SD_ReadBlocks(sector, buffer, count);
    return (ret == HAL_SD_OK) ? SUCCESS : ENABLE_IO_ERROR;
}

static BG_ERR sdcard_write_sectors(uint32_t sector, const uint8_t *buffer, uint32_t count)
{
    HAL_SD_Error_t ret;
    ret = HAL_SD_WriteBlocks(sector, buffer, count);
    return (ret == HAL_SD_OK) ? SUCCESS : ENABLE_IO_ERROR;
}

static bool sdcard_is_ready(void)
{
    uint8_t probe[512];
    return (HAL_SD_ReadBlocks(0, probe, 1) == HAL_SD_OK);
}

static uint32_t sdcard_get_sector_count(void)
{
    /* SD卡容量由CSD寄存器决定, 此处返回常见值 */
    /* 实际应从HAL层获取 */
    return 0;  /* 0 = 未知, FAT32会从BPB获取 */
}

const FAT32_DiskIO_t fat32_diskio_sdcard = {
    .init           = sdcard_init,
    .deinit         = sdcard_deinit,
    .read_sectors   = sdcard_read_sectors,
    .write_sectors  = sdcard_write_sectors,
    .is_ready       = sdcard_is_ready,
    .get_sector_count = sdcard_get_sector_count
};

/* ============================================
 * NAND Flash 后端实现 (W25N02, FAT32格式)
 *
 * NAND Flash 布局 (格式化为 FAT32 时):
 *   扇区大小: 512 字节
 *   页大小:   2048 字节 = 4个扇区
 *   块大小:   128KB = 256个扇区
 *
 * NAND Flash FAT32 专用数据区偏移:
 *   从 NAND 的 0MB 开始 (如果整片用于FAT32)
 *   或从指定偏移开始 (与 synth 音色数据共存)
 * ============================================ */

/** NAND FAT32 数据区在 NAND 中的起始偏移 (可配置) */
#ifndef NAND_FAT32_BASE_OFFSET
#define NAND_FAT32_BASE_OFFSET   0  /* 默认从 NAND 起始处开始 */
#endif

/** NAND FAT32 分区最大大小 */
#ifndef NAND_FAT32_MAX_SIZE
#define NAND_FAT32_MAX_SIZE      (32u * 1024u * 1024u)  /* 默认 32MB */
#endif

/** NAND 页大小 */
#define NAND_PAGE_SIZE           2048
/** NAND 每页包含的 FAT32 扇区数 */
#define NAND_SECTORS_PER_PAGE    (NAND_PAGE_SIZE / 512)

static BG_ERR nand_fat32_init(void)
{
    FlashDevice_t *nand;
    nand = FlashDevices_GetNandFlash();
    if (!nand) {
        return ENABLE_DEVICE_NOT_READY;
    }
    return SUCCESS;
}

static void nand_fat32_deinit(void)
{
    /* NAND 设备由 flash_devices 统一管理 */
}

static BG_ERR nand_fat32_read_sectors(uint32_t sector, uint8_t *buffer, uint32_t count)
{
    FlashDevice_t *nand;
    uint32_t nand_addr;
    BG_ERR ret;
    uint32_t i;

    nand = FlashDevices_GetNandFlash();
    if (!nand) {
        return ENABLE_DEVICE_NOT_READY;
    }

    for (i = 0; i < count; i++) {
        nand_addr = NAND_FAT32_BASE_OFFSET + (sector + i) * 512;
        if (nand_addr + 512 > NAND_FAT32_BASE_OFFSET + NAND_FAT32_MAX_SIZE) {
            return ENABLE_OVERFLOW;
        }
        ret = nand->ops->read(nand, nand_addr, buffer + i * 512, 512);
        if (ret != SUCCESS) {
            return ENABLE_IO_ERROR;
        }
    }
    return SUCCESS;
}

static BG_ERR nand_fat32_write_sectors(uint32_t sector, const uint8_t *buffer, uint32_t count)
{
    FlashDevice_t *nand;
    uint32_t nand_addr;
    BG_ERR ret;
    uint32_t i;

    nand = FlashDevices_GetNandFlash();
    if (!nand) {
        return ENABLE_DEVICE_NOT_READY;
    }

    for (i = 0; i < count; i++) {
        nand_addr = NAND_FAT32_BASE_OFFSET + (sector + i) * 512;
        if (nand_addr + 512 > NAND_FAT32_BASE_OFFSET + NAND_FAT32_MAX_SIZE) {
            return ENABLE_OVERFLOW;
        }
        ret = nand->ops->write(nand, nand_addr, (uint8_t *)(buffer + i * 512), 512);
        if (ret != SUCCESS) {
            return ENABLE_IO_ERROR;
        }
    }
    return SUCCESS;
}

static bool nand_fat32_is_ready(void)
{
    return (FlashDevices_GetNandFlash() != 0);
}

static uint32_t nand_fat32_get_sector_count(void)
{
    return NAND_FAT32_MAX_SIZE / 512;
}

const FAT32_DiskIO_t fat32_diskio_nand = {
    .init           = nand_fat32_init,
    .deinit         = nand_fat32_deinit,
    .read_sectors   = nand_fat32_read_sectors,
    .write_sectors  = nand_fat32_write_sectors,
    .is_ready       = nand_fat32_is_ready,
    .get_sector_count = nand_fat32_get_sector_count
};

#endif /* FAT32_EN */
