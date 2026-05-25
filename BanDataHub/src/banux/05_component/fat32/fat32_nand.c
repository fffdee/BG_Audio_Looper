/**
 * @file fat32_nand.c
 * @brief NAND Flash FAT32 文件系统集成实现
 */

#include "product_def.h"

#if FAT32_EN && HW_NAND0_EN

#include "fat32_nand.h"
#include "fat32_reader.h"
#include "fat32_diskio.h"
#include "flash_devices.h"
#include <string.h>

/* ============================================
 * 内部状态
 * ============================================ */

static bool g_nand_fat32_initialized = false;

/* ============================================
 * 公开接口实现
 * ============================================ */

BG_ERR FAT32_NAND_Init(void)
{
    BG_ERR ret;
    FlashDevice_t *nand;

    if (g_nand_fat32_initialized) {
        return SUCCESS;
    }

    /* 检查 NAND 设备是否存在 */
    nand = FlashDevices_GetNandFlash();
    if (!nand) {
        return ENABLE_DEVICE_NOT_READY;
    }

    /* 注册 NAND 磁盘 IO 并选为当前后端 */
    ret = FAT32_DiskIO_Register(FAT32_DISK_NAND, &fat32_diskio_nand);
    if (ret != SUCCESS) {
        return ret;
    }

    ret = FAT32_DiskIO_Select(FAT32_DISK_NAND);
    if (ret != SUCCESS) {
        return ret;
    }

    /* 初始化 FAT32 文件系统 */
    ret = FAT32_Init();
    if (ret != SUCCESS) {
        /* FAT32 初始化失败，可能 NAND 未格式化 */
        return ret;
    }

    g_nand_fat32_initialized = true;
    return SUCCESS;
}

void FAT32_NAND_DeInit(void)
{
    if (g_nand_fat32_initialized) {
        FAT32_DeInit();
        g_nand_fat32_initialized = false;
    }
}

BG_ERR FAT32_NAND_Format(void)
{
    FlashDevice_t *nand;
    uint8_t sector_buf[512];
    uint32_t total_sectors;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t fat_size_sectors;
    uint32_t root_cluster;
    uint32_t data_start;
    uint32_t fat_entries;
    BG_ERR ret;
    const FAT32_DiskIO_t *dio;

    nand = FlashDevices_GetNandFlash();
    if (!nand) {
        return ENABLE_DEVICE_NOT_READY;
    }

    /* 确保 NAND 后端已注册 */
    FAT32_DiskIO_Register(FAT32_DISK_NAND, &fat32_diskio_nand);
    FAT32_DiskIO_Select(FAT32_DISK_NAND);
    dio = FAT32_DiskIO_GetCurrent();
    if (!dio) {
        return ENABLE_DEVICE_NOT_READY;
    }

    total_sectors = FAT32_NAND_PARTITION_SIZE / 512;
    sectors_per_cluster = 8;  /* 4KB 簇 */
    reserved_sectors = 32;
    root_cluster = 2;

    /* 计算 FAT 大小 */
    fat_entries = (total_sectors - reserved_sectors) / sectors_per_cluster + 2;
    fat_size_sectors = (fat_entries * 4 + 511) / 512;
    data_start = reserved_sectors + fat_size_sectors * 2;

    /* 1. 擦除 NAND FAT32 分区的前几个块 (用于 BPB + FAT 表) */
    {
        uint32_t erase_size;
        erase_size = (data_start + 256) * 512;  /* 擦除至少到数据区开头 */
        if (erase_size > FAT32_NAND_PARTITION_SIZE) {
            erase_size = FAT32_NAND_PARTITION_SIZE;
        }
        /* NAND 擦除以块为单位 (128KB) */
        {
            uint32_t block_size = 128 * 1024;
            uint32_t offset;
            for (offset = 0; offset < erase_size; offset += block_size) {
                /* 通过 NAND 驱动擦除块 */
                nand->ops->erase_block(nand, FAT32_NAND_PARTITION_OFFSET + offset);
            }
        }
    }

    /* 2. 写入 BPB (引导扇区) */
    memset(sector_buf, 0, 512);

    /* 跳转指令 */
    sector_buf[0] = 0xEB;
    sector_buf[1] = 0x58;
    sector_buf[2] = 0x90;

    /* OEM 名称 */
    memcpy(&sector_buf[3], "BANBOXFS", 8);

    /* BPB */
    sector_buf[0x0B] = 0x00; sector_buf[0x0C] = 0x02; /* 512 bytes/sector */
    sector_buf[0x0D] = (uint8_t)sectors_per_cluster;   /* sectors/cluster */
    sector_buf[0x0E] = (uint8_t)(reserved_sectors & 0xFF);
    sector_buf[0x0F] = (uint8_t)(reserved_sectors >> 8);
    sector_buf[0x10] = 2;   /* 2个FAT表 */
    /* total_sectors_16 = 0 (FAT32) */
    sector_buf[0x15] = 0xF8; /* 固定磁盘 */
    /* total_sectors_32 */
    sector_buf[0x20] = (uint8_t)(total_sectors);
    sector_buf[0x21] = (uint8_t)(total_sectors >> 8);
    sector_buf[0x22] = (uint8_t)(total_sectors >> 16);
    sector_buf[0x23] = (uint8_t)(total_sectors >> 24);
    /* FAT大小 */
    sector_buf[0x24] = (uint8_t)(fat_size_sectors);
    sector_buf[0x25] = (uint8_t)(fat_size_sectors >> 8);
    sector_buf[0x26] = (uint8_t)(fat_size_sectors >> 16);
    sector_buf[0x27] = (uint8_t)(fat_size_sectors >> 24);
    /* 根目录簇号 */
    sector_buf[0x2C] = (uint8_t)(root_cluster);
    sector_buf[0x2D] = (uint8_t)(root_cluster >> 8);
    sector_buf[0x2E] = (uint8_t)(root_cluster >> 16);
    sector_buf[0x2F] = (uint8_t)(root_cluster >> 24);
    /* FSInfo 扇区 */
    sector_buf[0x30] = 1;
    /* 备份引导扇区 */
    sector_buf[0x32] = 6;
    /* 驱动器号 */
    sector_buf[0x40] = 0x80;
    /* 引导签名 */
    sector_buf[0x42] = 0x29;
    /* 卷标 */
    memcpy(&sector_buf[0x47], "BANBOX NAND", 11);
    /* 文件系统类型 */
    memcpy(&sector_buf[0x52], "FAT32   ", 8);
    /* 分区签名 */
    sector_buf[0x1FE] = 0x55;
    sector_buf[0x1FF] = 0xAA;

    ret = dio->write_sectors(0, sector_buf, 1);
    if (ret != SUCCESS) {
        return ret;
    }

    /* 3. 写入 FSInfo 扇区 (扇区 1) */
    memset(sector_buf, 0, 512);
    sector_buf[0x00] = 0x52; sector_buf[0x01] = 0x52;
    sector_buf[0x02] = 0x61; sector_buf[0x03] = 0x41; /* RRaA */
    sector_buf[0x1E4] = 0x72; sector_buf[0x1E5] = 0x72;
    sector_buf[0x1E6] = 0x41; sector_buf[0x1E7] = 0x61; /* rrAa */
    /* 空闲簇数 (待计算) */
    {
        uint32_t free_clusters = (total_sectors - data_start) / sectors_per_cluster - 1;
        sector_buf[0x1E8] = (uint8_t)(free_clusters);
        sector_buf[0x1E9] = (uint8_t)(free_clusters >> 8);
        sector_buf[0x1EA] = (uint8_t)(free_clusters >> 16);
        sector_buf[0x1EB] = (uint8_t)(free_clusters >> 24);
    }
    /* 下一个空闲簇 */
    sector_buf[0x1EC] = 3;
    sector_buf[0x1FE] = 0x55;
    sector_buf[0x1FF] = 0xAA;

    ret = dio->write_sectors(1, sector_buf, 1);
    if (ret != SUCCESS) {
        return ret;
    }

    /* 4. 写入备份引导扇区 (扇区 6) - 回读扇区0并复制 */
    ret = dio->read_sectors(0, sector_buf, 1);
    if (ret == SUCCESS) {
        ret = dio->write_sectors(6, sector_buf, 1);
    }

    /* 5. 初始化 FAT 表 */
    memset(sector_buf, 0, 512);

    /* FAT 表第一个扇区: 簇 0,1 保留, 簇 2 = 根目录 (EOF) */
    {
        uint32_t *fat_entries_ptr = (uint32_t *)sector_buf;
        fat_entries_ptr[0] = 0x0FFFFFF8;  /* 媒体类型 */
        fat_entries_ptr[1] = 0x0FFFFFFF;  /* 保留 */
        fat_entries_ptr[2] = 0x0FFFFFFF;  /* 根目录 EOC */
    }

    /* FAT1 */
    ret = dio->write_sectors(reserved_sectors, sector_buf, 1);
    if (ret != SUCCESS) {
        return ret;
    }
    /* FAT2 */
    ret = dio->write_sectors(reserved_sectors + fat_size_sectors, sector_buf, 1);
    if (ret != SUCCESS) {
        return ret;
    }

    /* 6. 清空根目录簇 */
    memset(sector_buf, 0, 512);
    {
        uint32_t root_start = data_start;
        uint32_t i;
        for (i = 0; i < sectors_per_cluster; i++) {
            ret = dio->write_sectors(root_start + i, sector_buf, 1);
            if (ret != SUCCESS) {
                return ret;
            }
        }
    }

    return SUCCESS;
}

bool FAT32_NAND_IsFormatted(void)
{
    uint8_t sector_buf[512];
    const FAT32_DiskIO_t *dio;
    BG_ERR ret;

    /* 确保 NAND 后端就绪 */
    FAT32_DiskIO_Register(FAT32_DISK_NAND, &fat32_diskio_nand);
    FAT32_DiskIO_Select(FAT32_DISK_NAND);
    dio = FAT32_DiskIO_GetCurrent();
    if (!dio) {
        return false;
    }

    /* 读取扇区0, 检查 BPB 特征 */
    ret = dio->read_sectors(0, sector_buf, 1);
    if (ret != SUCCESS) {
        return false;
    }

    /* 检查跳转指令 */
    if (sector_buf[0] != 0xEB && sector_buf[0] != 0xE9) {
        return false;
    }

    /* 检查分区签名 */
    if (sector_buf[0x1FE] != 0x55 || sector_buf[0x1FF] != 0xAA) {
        return false;
    }

    /* 检查字节/扇区 = 512 */
    if (sector_buf[0x0B] != 0x00 || sector_buf[0x0C] != 0x02) {
        return false;
    }

    return true;
}

uint32_t FAT32_NAND_GetFreeSpace(void)
{
    FAT32_FSInfo_t fs_info;
    BG_ERR ret;
    uint32_t bytes_per_cluster;

    if (!g_nand_fat32_initialized) {
        return 0;
    }

    ret = FAT32_GetFSInfo(&fs_info);
    if (ret != SUCCESS) {
        return 0;
    }

    bytes_per_cluster = fs_info.bpb.bytes_per_sector * fs_info.bpb.sectors_per_cluster;

    /* 简化估算: 可用空间 ≈ 总簇数 × 簇大小 */
    return fs_info.total_clusters * bytes_per_cluster;
}

#endif /* FAT32_EN */
