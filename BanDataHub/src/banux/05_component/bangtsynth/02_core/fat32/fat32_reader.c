#include "bangtsynth_legacy.h"
#if BANGTSYNTH_LEGACY

/**
 * @file fat32_reader.c
 * @brief FAT32 文件系统最小读取器实现
 *
 * 实现 FAT32 引导扇区解析、目录遍历、文件读取等核心功能。
 * 专为嵌入式环境优化，内存占用小，功能精简。
 */

#include "product_def.h"

#if FAT32_EN && !defined(BANDATAHUB)

#include "fat32_reader.h"
#include "fat32_diskio.h"
#include <string.h>

/* 禁用日志以减小代码体积和避免 SDA 重定位溢出 */
#define BG_LOG_E(...)
#define BG_LOG_W(...)
#define BG_LOG_I(...)

/* ============================================
 * 内部常量定义
 * ============================================ */

/** 扇区缓存大小：只缓存 1 个扇区，避免大静态缓冲区 */
#define FAT32_CACHE_SIZE             (FAT32_SECTOR_SIZE)       /* 512B 缓存 */
#define FAT32_MAX_PATH_LENGTH        256
#define FAT32_MAX_LFN_ENTRIES        20   /* 最大长文件名目录项数 */

/** 错误重试次数 */
#define FAT32_RETRY_COUNT            3

/* ============================================
 * 内部数据结构
 * ============================================ */

/**
 * FAT32 读取器状态
 */
typedef struct {
    bool initialized;                    /* 初始化标志 */
    FAT32_FSInfo_t fs_info;              /* 文件系统信息 */
    uint8_t sector_cache[FAT32_CACHE_SIZE];  /* 扇区缓存 (512B，逐扇区处理) */
    uint32_t cached_sector;              /* 缓存的扇区号 */
    bool cache_valid;                    /* 缓存有效标志 */
} FAT32_ReaderState_t;

/* ============================================
 * 全局变量
 * ============================================ */

static FAT32_ReaderState_t g_fat32_state = {
    .initialized = false,
    .cached_sector = 0xFFFFFFFF,
    .cache_valid = false
};

/* ============================================
 * 内部函数声明
 * ============================================ */

static BG_ERR fat32_read_sector(uint32_t sector, uint8_t *buffer);
static BG_ERR fat32_parse_mbr(uint8_t *mbr_sector, uint32_t *partition_start);
static BG_ERR fat32_parse_bpb(uint8_t *boot_sector, FAT32_BPB_t *bpb);
static BG_ERR fat32_get_next_cluster(uint32_t cluster, uint32_t *next_cluster);
static BG_ERR fat32_traverse_directory(uint32_t dir_cluster, const char *filename,
                                      FAT32_FileInfo_t *file_info);
static BG_ERR fat32_parse_dir_entry(uint8_t *entry_data, FAT32_FileInfo_t *file_info,
                                   uint16_t *lfn_checksum, char *lfn_buffer);
static uint8_t fat32_lfn_checksum(const char *short_name);
static void fat32_lfn_to_utf8(uint16_t *lfn_unicode, uint8_t length, char *utf8_buffer);

/* 写相关内部函数 */
static BG_ERR fat32_write_sector(uint32_t sector, const uint8_t *buffer);
static BG_ERR fat32_set_fat_entry(uint32_t cluster, uint32_t value);
static BG_ERR fat32_alloc_cluster(uint32_t *out_cluster);
static void   fat32_make_83name(const char *filename, char out83[11]);
static BG_ERR fat32_find_free_dir_entry(uint32_t dir_cluster, uint32_t *out_sector, uint32_t *out_offset);
static BG_ERR fat32_find_dir_entry(uint32_t dir_cluster, const char *filename, uint32_t *out_sector, uint32_t *out_offset);

/* ============================================
 * 公共接口实现
 * ============================================ */

BG_ERR FAT32_Init(void)
{
    BG_ERR ret;
    uint32_t partition_start = 0;
    uint32_t total_sectors;
    uint32_t data_sectors;

    if (g_fat32_state.initialized) {
        BG_LOG_W(BG_LOG_TAG_FAT32, "FAT32 reader already initialized");
        return SUCCESS;
    }

    /* 分配扇区缓存，同时用作初始化时的临时读取缓冲区
     * （避免两次临近 malloc 导致 SDIO DMA 堆溢出） */
    /* sector_cache 已在全局状态结构体中静态分配 (2KB) */

    /* 读取扇区 0 (MBR 或超级软盘 BPB) - 通过 DiskIO 抽象层 */
    {
        const FAT32_DiskIO_t *dio = FAT32_DiskIO_GetCurrent();
        if (!dio) {
            ret = ENABLE_DEVICE_NOT_READY;
            goto cleanup;
        }
        if (dio->read_sectors(0, g_fat32_state.sector_cache, 1) != SUCCESS) {
            ret = ENABLE_IO_ERROR;
            goto cleanup;
        }
    }

    /* 解析 MBR，获取分区起始扇区号 */
    ret = fat32_parse_mbr(g_fat32_state.sector_cache, &partition_start);
    if (ret != SUCCESS) {
        goto cleanup;
    }

    /* 读取引导扇区 (partition_start == 0 则已就是就是已读入的内容) */
    if (partition_start != 0) {
        const FAT32_DiskIO_t *dio = FAT32_DiskIO_GetCurrent();
        if (!dio || dio->read_sectors(partition_start, g_fat32_state.sector_cache, 1) != SUCCESS) {
            ret = ENABLE_IO_ERROR;
            goto cleanup;
        }
    }

    /* 解析 BPB */
    ret = fat32_parse_bpb(g_fat32_state.sector_cache, &g_fat32_state.fs_info.bpb);
    if (ret != SUCCESS) {
        goto cleanup;
    }

    /* 检测 FAT 类型：sectors_per_fat_16 非零 = FAT12/FAT16 */
    g_fat32_state.fs_info.is_fat16 = (g_fat32_state.fs_info.bpb.sectors_per_fat_16 != 0);

    /* 计算文件系统参数 */
    g_fat32_state.fs_info.fat_start_sector = partition_start +
                                           g_fat32_state.fs_info.bpb.reserved_sectors;

    if (g_fat32_state.fs_info.is_fat16) {
        /* FAT16: 根目录在 FAT 表之后的固定区域，数据区在根目录之后 */
        uint32_t root_sects =
            ((uint32_t)g_fat32_state.fs_info.bpb.root_entries * 32u + FAT32_SECTOR_SIZE - 1u)
            / FAT32_SECTOR_SIZE;
        g_fat32_state.fs_info.root_dir_start_lba =
            g_fat32_state.fs_info.fat_start_sector +
            (uint32_t)g_fat32_state.fs_info.bpb.num_fats *
            (uint32_t)g_fat32_state.fs_info.bpb.sectors_per_fat_16;
        g_fat32_state.fs_info.root_dir_num_sectors = root_sects;
        g_fat32_state.fs_info.data_start_sector =
            g_fat32_state.fs_info.root_dir_start_lba + root_sects;
        g_fat32_state.fs_info.root_dir_sector = g_fat32_state.fs_info.root_dir_start_lba;
        /* FAT16 根目录不基于簇，用哨兵值让遍历函数走固定扇区路径 */
        g_fat32_state.fs_info.bpb.root_cluster = FAT16_ROOT_DIR_CLUSTER;
    } else {
        /* FAT32: 根目录基于 root_cluster，无固定根目录区 */
        g_fat32_state.fs_info.data_start_sector = g_fat32_state.fs_info.fat_start_sector +
            (g_fat32_state.fs_info.bpb.num_fats *
             g_fat32_state.fs_info.bpb.sectors_per_fat_32);
        g_fat32_state.fs_info.root_dir_sector = g_fat32_state.fs_info.data_start_sector +
            ((g_fat32_state.fs_info.bpb.root_cluster - 2u) *
             g_fat32_state.fs_info.bpb.sectors_per_cluster);
        g_fat32_state.fs_info.root_dir_start_lba = 0;
        g_fat32_state.fs_info.root_dir_num_sectors = 0;
    }

    /* 计算总簇数 */
    total_sectors = g_fat32_state.fs_info.bpb.total_sectors_32;
    if (total_sectors == 0) {
        total_sectors = g_fat32_state.fs_info.bpb.total_sectors_16;
    }
    data_sectors = total_sectors - g_fat32_state.fs_info.data_start_sector;
    g_fat32_state.fs_info.total_clusters = data_sectors / g_fat32_state.fs_info.bpb.sectors_per_cluster;

    /* 初始化成功，使缓存无效以便正常读扩可覆盖 */
    g_fat32_state.cache_valid = false;
    g_fat32_state.cached_sector = 0xFFFFFFFF;
    g_fat32_state.initialized = true;
    ret = SUCCESS;

cleanup:
    if (ret != SUCCESS) {
        /* 静态分配，无需 free */
        g_fat32_state.initialized = false;
    }
    return ret;
}

void FAT32_DeInit(void)
{
    if (!g_fat32_state.initialized) {
        return;
    }

    /* 静态分配的缓存，无需 free */
    g_fat32_state.initialized = false;
    g_fat32_state.cache_valid = false;

    BG_LOG_I(BG_LOG_TAG_FAT32, "FAT32 reader deinitialized");
}

BG_ERR FAT32_FindFile(const char *filename, FAT32_FileInfo_t *file_info)
{
    if (!g_fat32_state.initialized || !filename || !file_info) {
        return ENABLE_INVALID_INPUT;
    }

    /* 从根目录开始遍历 */
    return fat32_traverse_directory(g_fat32_state.fs_info.bpb.root_cluster,
                                   filename, file_info);
}

BG_ERR FAT32_OpenFile(const char *filename, FAT32_FileHandle_t *handle)
{
    BG_ERR ret;

    if (!g_fat32_state.initialized || !filename || !handle) {
        return ENABLE_INVALID_INPUT;
    }

    /* 查找文件 */
    ret = FAT32_FindFile(filename, &handle->info);
    if (ret != SUCCESS) {
        return ret;
    }

    /* 初始化句柄 */
    handle->current_cluster = handle->info.start_cluster;
    handle->current_sector = 0;
    handle->position = 0;
    handle->bytes_read = 0;

    BG_LOG_I(BG_LOG_TAG_FAT32, "File opened: %s (size=%u, cluster=%u)",
             handle->info.name, handle->info.size, handle->info.start_cluster);

    return SUCCESS;
}

int32_t FAT32_ReadFile(FAT32_FileHandle_t *handle, void *buffer, uint32_t size)
{
    uint8_t *buf = (uint8_t *)buffer;
    uint32_t bytes_read = 0;
    uint32_t spc;         /* sectors per cluster */
    uint32_t cluster_size;
    BG_ERR ret;

    if (!g_fat32_state.initialized || !handle || !buffer) {
        return -1;
    }

    /* 检查是否已读完 */
    if (handle->position >= handle->info.size) {
        return 0;
    }

    /* 限制读取大小 */
    if (handle->position + size > handle->info.size) {
        size = handle->info.size - handle->position;
    }

    spc          = g_fat32_state.fs_info.bpb.sectors_per_cluster;
    cluster_size = spc * FAT32_SECTOR_SIZE;

    /* 逐扇区读取，sector_cache 只需 512 字节，无簇大小限制 */
    while (bytes_read < size && handle->current_cluster < FAT32_CLUSTER_EOF_MIN) {
        uint32_t cluster_offset = handle->position % cluster_size;
        uint32_t sector_idx     = cluster_offset / FAT32_SECTOR_SIZE;   /* 簇内第几扇区 */
        uint32_t sector_off     = cluster_offset % FAT32_SECTOR_SIZE;   /* 扇区内字节偏移 */
        uint32_t lba;
        uint32_t available;
        uint32_t to_copy;

        lba = g_fat32_state.fs_info.data_start_sector +
              ((handle->current_cluster - 2u) * spc) + sector_idx;

        ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
        if (ret != SUCCESS) {
            return -1;
        }

        available = FAT32_SECTOR_SIZE - sector_off;
        to_copy   = size - bytes_read;
        if (to_copy > available) {
            to_copy = available;
        }

        memcpy(buf + bytes_read, g_fat32_state.sector_cache + sector_off, to_copy);
        bytes_read       += to_copy;
        handle->position += to_copy;

        /* 越过簇边界时切换到下一个簇 */
        if (handle->position % cluster_size == 0) {
            ret = fat32_get_next_cluster(handle->current_cluster, &handle->current_cluster);
            if (ret != SUCCESS) {
                return -1;
            }
        }
    }

    handle->bytes_read += bytes_read;
    return bytes_read;
}

void FAT32_CloseFile(FAT32_FileHandle_t *handle)
{
    if (handle) {
        memset(handle, 0, sizeof(FAT32_FileHandle_t));
    }
}

BG_ERR FAT32_GetFSInfo(FAT32_FSInfo_t *info)
{
    if (!g_fat32_state.initialized || !info) {
        return ENABLE_INVALID_INPUT;
    }

    memcpy(info, &g_fat32_state.fs_info, sizeof(FAT32_FSInfo_t));
    return SUCCESS;
}

bool FAT32_IsCardReady(void)
{
    const FAT32_DiskIO_t *dio = FAT32_DiskIO_GetCurrent();
    if (!dio || !dio->is_ready) {
        return false;
    }
    return dio->is_ready();
}

/* ============================================
 * 内部函数实现
 * ============================================ */

static BG_ERR fat32_read_sector(uint32_t sector, uint8_t *buffer)
{
    int retry_count = 0;
    BG_ERR ret;
    const FAT32_DiskIO_t *dio;

    /* 检查缓存 */
    if (g_fat32_state.cache_valid && g_fat32_state.cached_sector == sector) {
        memcpy(buffer, g_fat32_state.sector_cache, FAT32_SECTOR_SIZE);
        return SUCCESS;
    }

    dio = FAT32_DiskIO_GetCurrent();
    if (!dio) {
        return ENABLE_DEVICE_NOT_READY;
    }

    /* 通过 DiskIO 层读块 */
    while (retry_count < FAT32_RETRY_COUNT) {
        ret = dio->read_sectors(sector, buffer, 1);
        if (ret == SUCCESS) {
            /* 更新缓存 */
            memcpy(g_fat32_state.sector_cache, buffer, FAT32_SECTOR_SIZE);
            g_fat32_state.cached_sector = sector;
            g_fat32_state.cache_valid = true;
            return SUCCESS;
        }
        retry_count++;
    }

    return ENABLE_IO_ERROR;
}

static BG_ERR fat32_parse_mbr(uint8_t *mbr_sector, uint32_t *partition_start)
{
    MBR_PartitionEntry_t *partitions;
    int i;

    /* 检查扇区 0 开头是否就是 BPB (超级软盘格式，无 MBR) */
    if (mbr_sector[0] == 0xEB || mbr_sector[0] == 0xE9) {
        /* BPB 直接在扇区 0，分区起始 = 0 */
        *partition_start = 0;
        return SUCCESS;
    }

    /* 检查 MBR 签名 */
    if (mbr_sector[MBR_SIGNATURE_OFFSET] != 0x55 ||
        mbr_sector[MBR_SIGNATURE_OFFSET + 1] != 0xAA) {
        return ENABLE_INVALID_INPUT;
    }

    /* 查找 FAT 分区 (包括 FAT12/16/32 各分区类型) */
    partitions = (MBR_PartitionEntry_t *)(mbr_sector + MBR_PARTITION_TABLE_OFFSET);
    for (i = 0; i < 4; i++) {
        uint8_t type = partitions[i].type;
        /* 0x01/0x04/0x06: FAT12/16  0x0B/0x0C: FAT32  0x0E/0x0F: FAT16/32 LBA */
        if (type == 0x01 || type == 0x04 || type == 0x06 ||
            type == 0x0B || type == 0x0C ||
            type == 0x0E || type == 0x0F) {
            *partition_start = partitions[i].start_lba;
            return SUCCESS;
        }
    }

    /* 未找到已知分区类型，尝试第一个非空分区 */
    for (i = 0; i < 4; i++) {
        if (partitions[i].type != 0 && partitions[i].start_lba != 0) {
            *partition_start = partitions[i].start_lba;
            return SUCCESS;
        }
    }

    return ENABLE_INVALID_INPUT;
}

static BG_ERR fat32_parse_bpb(uint8_t *boot_sector, FAT32_BPB_t *bpb)
{
    /* 解析 BPB 字段 */
    bpb->bytes_per_sector = *(uint16_t *)(boot_sector + BPB_BYTES_PER_SECTOR);
    bpb->sectors_per_cluster = *(uint8_t *)(boot_sector + BPB_SECTORS_PER_CLUSTER);
    bpb->reserved_sectors = *(uint16_t *)(boot_sector + BPB_RESERVED_SECTORS);
    bpb->num_fats = *(uint8_t *)(boot_sector + BPB_NUM_FATS);
    bpb->root_entries = *(uint16_t *)(boot_sector + BPB_ROOT_ENTRIES);
    bpb->total_sectors_16 = *(uint16_t *)(boot_sector + BPB_TOTAL_SECTORS_16);
    bpb->media_type = *(uint8_t *)(boot_sector + BPB_MEDIA_TYPE);
    bpb->sectors_per_fat_16 = *(uint16_t *)(boot_sector + BPB_SECTORS_PER_FAT_16);
    bpb->sectors_per_track = *(uint16_t *)(boot_sector + BPB_SECTORS_PER_TRACK);
    bpb->num_heads = *(uint16_t *)(boot_sector + BPB_NUM_HEADS);
    bpb->hidden_sectors = *(uint32_t *)(boot_sector + BPB_HIDDEN_SECTORS);
    bpb->total_sectors_32 = *(uint32_t *)(boot_sector + BPB_TOTAL_SECTORS_32);
    bpb->sectors_per_fat_32 = *(uint32_t *)(boot_sector + BPB_SECTORS_PER_FAT_32);
    bpb->ext_flags = *(uint16_t *)(boot_sector + BPB_EXT_FLAGS);
    bpb->fs_version = *(uint16_t *)(boot_sector + BPB_FS_VERSION);
    bpb->root_cluster = *(uint32_t *)(boot_sector + BPB_ROOT_CLUSTER);
    bpb->fs_info_sector = *(uint16_t *)(boot_sector + BPB_FS_INFO_SECTOR);
    bpb->backup_boot_sector = *(uint16_t *)(boot_sector + BPB_BACKUP_BOOT_SECTOR);
    bpb->drive_number = *(uint8_t *)(boot_sector + BPB_DRIVE_NUMBER);
    bpb->boot_signature = *(uint8_t *)(boot_sector + BPB_BOOT_SIGNATURE);
    bpb->volume_id = *(uint32_t *)(boot_sector + BPB_VOLUME_ID);
    memcpy(bpb->volume_label, boot_sector + BPB_VOLUME_LABEL, 11);
    memcpy(bpb->fs_type, boot_sector + BPB_FS_TYPE, 8);

    /* 验证参数 */
    if (bpb->bytes_per_sector != FAT32_SECTOR_SIZE) {
        BG_LOG_E(BG_LOG_TAG_FAT32, "Unsupported sector size: %u", bpb->bytes_per_sector);
        return ENABLE_INVALID_INPUT;
    }

    /* fs_version 应为 0，但某些格式化工具会写入非 0 值，不强制失败 */

    return SUCCESS;
}

static BG_ERR fat32_get_next_cluster(uint32_t cluster, uint32_t *next_cluster)
{
    uint32_t fat_sector;
    uint32_t fat_offset;
    BG_ERR ret;

    if (g_fat32_state.fs_info.is_fat16) {
        /* FAT16: 每个条目 2 字节 */
        fat_sector = g_fat32_state.fs_info.fat_start_sector +
                    (cluster * 2u) / FAT32_SECTOR_SIZE;
        fat_offset = (cluster * 2u) % FAT32_SECTOR_SIZE;
        ret = fat32_read_sector(fat_sector, g_fat32_state.sector_cache);
        if (ret != SUCCESS) {
            return ret;
        }
        *next_cluster = *(uint16_t *)(g_fat32_state.sector_cache + fat_offset);
        if (*next_cluster >= 0xFFF8u) {
            *next_cluster = FAT32_CLUSTER_EOF_MIN;  /* FAT16 EOF */
        }
    } else {
        /* FAT32: 每个条目 4 字节 */
        fat_sector = g_fat32_state.fs_info.fat_start_sector +
                    (cluster * 4) / FAT32_SECTOR_SIZE;
        fat_offset = (cluster * 4) % FAT32_SECTOR_SIZE;
        ret = fat32_read_sector(fat_sector, g_fat32_state.sector_cache);
        if (ret != SUCCESS) {
            return ret;
        }
        *next_cluster = *(uint32_t *)(g_fat32_state.sector_cache + fat_offset) & 0x0FFFFFFF;
        if (*next_cluster >= FAT32_CLUSTER_BAD && *next_cluster <= FAT32_CLUSTER_EOF_MAX) {
            *next_cluster = FAT32_CLUSTER_EOF_MIN;
        }
    }

    return SUCCESS;
}

static int fat32_strcasecmp(const char *a, const char *b)
{
    int ca, cb;
    while (*a && *b) {
        ca = (unsigned char)*a;
        cb = (unsigned char)*b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static BG_ERR fat32_traverse_directory(uint32_t dir_cluster, const char *filename,
                                      FAT32_FileInfo_t *file_info)
{
    uint32_t current_cluster = dir_cluster;
    BG_ERR ret;
    uint8_t *entry_ptr;
    uint16_t lfn_checksum;
    char lfn_buffer[FAT32_MAX_PATH_LENGTH];
    bool has_lfn;
    uint32_t s;        /* 簇内扇区索引 */
    uint32_t i;        /* 扇区内入口偏移 */
    uint32_t spc;      /* sectors per cluster */
    uint32_t lba;
    BG_ERR parse_ret;

    spc = g_fat32_state.fs_info.bpb.sectors_per_cluster;
    lfn_checksum = 0;
    memset(lfn_buffer, 0, sizeof(lfn_buffer));
    has_lfn = false;

    /* FAT16 根目录：固定扇区范围，不使用簇链 */
    if (g_fat32_state.fs_info.is_fat16 && current_cluster == FAT16_ROOT_DIR_CLUSTER) {
        uint32_t end_lba = g_fat32_state.fs_info.root_dir_start_lba +
                           g_fat32_state.fs_info.root_dir_num_sectors;
        for (lba = g_fat32_state.fs_info.root_dir_start_lba; lba < end_lba; lba++) {
            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) { return ENABLE_INVALID_INPUT; } /* 目录结束 */
                if (*entry_ptr == 0xE5) { continue; }                    /* 已删除 */
                parse_ret = fat32_parse_dir_entry(entry_ptr, file_info,
                                                  &lfn_checksum, lfn_buffer);
                if (parse_ret == ENABLE_INVALID_INPUT) { has_lfn = true; continue; }
                if (parse_ret == SUCCESS) {
                    has_lfn = false;
                    if (fat32_strcasecmp(filename, file_info->name) == 0 ||
                        (strcmp(filename, "*.sf2") == 0 &&
                         strstr(file_info->name, ".sf2"))) {
                        return SUCCESS;
                    }
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                }
            }
        }
        return ENABLE_INVALID_INPUT;
    }

    while (current_cluster < FAT32_CLUSTER_EOF_MIN) {
        /* 逐扇区读取，无需簇大小的大缓冲区 */
        for (s = 0; s < spc; s++) {
            lba = g_fat32_state.fs_info.data_start_sector +
                  ((current_cluster - 2u) * spc) + s;

            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) {
                return ret;
            }

            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) {
                    return ENABLE_INVALID_INPUT;  /* 目录结束，未找到 */
                }
                if (*entry_ptr == 0xE5) {
                    continue;  /* 已删除 */
                }

                parse_ret = fat32_parse_dir_entry(entry_ptr, file_info, &lfn_checksum, lfn_buffer);
                if (parse_ret == ENABLE_INVALID_INPUT) {
                    has_lfn = true;
                    continue;
                }

                if (parse_ret == SUCCESS) {
                    if (has_lfn) {
                        uint8_t calc = fat32_lfn_checksum((const char *)((FAT32_DirEntry_t *)entry_ptr)->name);
                        (void)calc;  /* 仅记录校验和不匹配，不中止处理 */
                    }

                    if (fat32_strcasecmp(filename, file_info->name) == 0 ||
                        (strcmp(filename, "*.sf2") == 0 && strstr(file_info->name, ".sf2"))) {
                        return SUCCESS;
                    }

                    has_lfn = false;
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                }
            }
        }

        ret = fat32_get_next_cluster(current_cluster, &current_cluster);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return ENABLE_INVALID_INPUT;  /* 文件未找到 */
}

BG_ERR FAT32_ListDirByCluster(uint32_t dir_cluster, FAT32_ListCallback_t cb, void *user)
{
    BG_ERR ret;
    uint8_t *entry_ptr;
    uint16_t lfn_checksum;
    char lfn_buffer[FAT32_MAX_PATH_LENGTH];
    bool has_lfn;
    FAT32_FileInfo_t file_info;
    BG_ERR parse_ret;
    uint32_t spc;
    uint32_t s;
    uint32_t i;
    uint32_t lba;

    if (!g_fat32_state.initialized || !cb) {
        return ENABLE_INVALID_INPUT;
    }

    spc = g_fat32_state.fs_info.bpb.sectors_per_cluster;
    lfn_checksum = 0;
    memset(lfn_buffer, 0, sizeof(lfn_buffer));
    has_lfn = false;

    /* FAT16 根目录：固定扇区范围，不使用簇链 */
    if (g_fat32_state.fs_info.is_fat16 && dir_cluster == FAT16_ROOT_DIR_CLUSTER) {
        uint32_t end_lba = g_fat32_state.fs_info.root_dir_start_lba +
                           g_fat32_state.fs_info.root_dir_num_sectors;
        for (lba = g_fat32_state.fs_info.root_dir_start_lba; lba < end_lba; lba++) {
            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) { return SUCCESS; } /* 目录结束 */
                if (*entry_ptr == 0xE5) { continue; }       /* 已删除 */
                parse_ret = fat32_parse_dir_entry(entry_ptr, &file_info,
                                                  &lfn_checksum, lfn_buffer);
                if (parse_ret == ENABLE_INVALID_INPUT) { has_lfn = true; continue; }
                if (parse_ret == SUCCESS) {
                    if (cb(&file_info, user) != 0) { return SUCCESS; }
                    has_lfn = false;
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                }
            }
        }
        return SUCCESS;
    }

    while (dir_cluster < FAT32_CLUSTER_EOF_MIN) {
        for (s = 0; s < spc; s++) {
            lba = g_fat32_state.fs_info.data_start_sector +
                  ((dir_cluster - 2u) * spc) + s;

            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) {
                return ret;
            }

            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) {
                    return SUCCESS;  /* 目录结束 */
                }
                if (*entry_ptr == 0xE5) {
                    continue;
                }

                parse_ret = fat32_parse_dir_entry(entry_ptr, &file_info, &lfn_checksum, lfn_buffer);
                if (parse_ret == ENABLE_INVALID_INPUT) {
                    has_lfn = true;
                    continue;
                }

                if (parse_ret == SUCCESS) {
                    if (cb(&file_info, user) != 0) {
                        return SUCCESS;  /* 用户请求停止 */
                    }
                    has_lfn = false;
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                }
            }
        }

        ret = fat32_get_next_cluster(dir_cluster, &dir_cluster);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return SUCCESS;
}

/* FAT32_ListDir: 向后兼容包装，列根目录 */
BG_ERR FAT32_ListDir(const char *path, FAT32_ListCallback_t cb, void *user)
{
    (void)path;
    if (!g_fat32_state.initialized) {
        return ENABLE_INVALID_INPUT;
    }
    return FAT32_ListDirByCluster(g_fat32_state.fs_info.bpb.root_cluster, cb, user);
}

uint32_t FAT32_GetRootCluster(void)
{
    return g_fat32_state.fs_info.bpb.root_cluster;
}

BG_ERR FAT32_FindEntryInDir(uint32_t dir_cluster, const char *name, FAT32_FileInfo_t *info)
{
    if (!g_fat32_state.initialized || !name || !info) {
        return ENABLE_INVALID_INPUT;
    }
    return fat32_traverse_directory(dir_cluster, name, info);
}

BG_ERR FAT32_OpenFileInDir(uint32_t dir_cluster, const char *filename, FAT32_FileHandle_t *handle)
{
    BG_ERR ret;

    if (!g_fat32_state.initialized || !filename || !handle) {
        return ENABLE_INVALID_INPUT;
    }

    ret = fat32_traverse_directory(dir_cluster, filename, &handle->info);
    if (ret != SUCCESS) {
        return ret;
    }

    handle->current_cluster = handle->info.start_cluster;
    handle->current_sector  = 0;
    handle->position        = 0;
    handle->bytes_read      = 0;
    return SUCCESS;
}

int32_t FAT32_WriteFile(uint32_t dir_cluster, const char *filename, const void *data, uint32_t size)
{
    char short_name[11];
    FAT32_DirEntry_t dir_entry;
    uint32_t dir_sector, dir_offset;
    uint32_t first_cluster;
    uint32_t bytes_written = 0;
    BG_ERR ret;

    if (!g_fat32_state.initialized || !filename || !data) {
        return -1;
    }

    /* 生成 8.3 短文件名 */
    fat32_make_83name(filename, short_name);

    /* 查找空目录项 */
    ret = fat32_find_free_dir_entry(dir_cluster, &dir_sector, &dir_offset);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_FAT32, "No free directory entry");
        return -1;
    }

    /* 分配第一个簇 */
    ret = fat32_alloc_cluster(&first_cluster);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_FAT32, "Failed to allocate cluster");
        return -1;
    }

    /* 准备目录项 */
    memset(&dir_entry, 0, sizeof(dir_entry));
    memcpy(dir_entry.name, short_name, 11);
    dir_entry.attr = DIR_ATTR_ARCHIVE;
    dir_entry.cluster_high = (uint16_t)(first_cluster >> 16);
    dir_entry.cluster_low = (uint16_t)(first_cluster & 0xFFFF);
    dir_entry.size = size;

    /* 设置时间戳 (简化实现) */
    dir_entry.modify_time = 0x1234;
    dir_entry.modify_date = 0x5678;

    /* 写入目录项 */
    {
        uint8_t sector_buf[FAT32_SECTOR_SIZE];
        ret = fat32_read_sector(dir_sector, sector_buf);
        if (ret != SUCCESS) {
            return -1;
        }
        memcpy(sector_buf + dir_offset, &dir_entry, sizeof(dir_entry));
        ret = fat32_write_sector(dir_sector, sector_buf);
        if (ret != SUCCESS) {
            return -1;
        }
    }

    /* 写入数据：逐扇区写入，sector_cache 用作末尾不满扇区的填充缓冲区 */
    {
        uint32_t current_cluster = first_cluster;
        const uint8_t *data_ptr = (const uint8_t *)data;
        uint32_t spc = g_fat32_state.fs_info.bpb.sectors_per_cluster;
        uint32_t start_lba;
        uint32_t s;

        while (bytes_written < size) {
            start_lba = g_fat32_state.fs_info.data_start_sector +
                        ((current_cluster - 2u) * spc);

            for (s = 0; s < spc && bytes_written < size; s++) {
                uint32_t remaining   = size - bytes_written;
                uint32_t chunk       = (remaining >= FAT32_SECTOR_SIZE) ? FAT32_SECTOR_SIZE : remaining;

                if (chunk == FAT32_SECTOR_SIZE) {
                    /* 整扇区：直接从源数据写 */
                    ret = fat32_write_sector(start_lba + s, data_ptr + bytes_written);
                } else {
                    /* 末尾不足一扇区：零填充后再写 */
                    memset(g_fat32_state.sector_cache, 0, FAT32_SECTOR_SIZE);
                    memcpy(g_fat32_state.sector_cache, data_ptr + bytes_written, chunk);
                    g_fat32_state.cache_valid = false;  /* 写回前使缓存失效 */
                    ret = fat32_write_sector(start_lba + s, g_fat32_state.sector_cache);
                }

                if (ret != SUCCESS) {
                    return -1;
                }
                bytes_written += chunk;
            }

            /* 申请下一个簇或标记文件结束 */
            if (bytes_written < size) {
                uint32_t next_cluster;
                ret = fat32_alloc_cluster(&next_cluster);
                if (ret != SUCCESS) {
                    return -1;
                }
                ret = fat32_set_fat_entry(current_cluster, next_cluster);
                if (ret != SUCCESS) {
                    return -1;
                }
                current_cluster = next_cluster;
            } else {
                ret = fat32_set_fat_entry(current_cluster, FAT32_CLUSTER_EOF_MIN);
                if (ret != SUCCESS) {
                    return -1;
                }
            }
        }
    }

    return (int32_t)bytes_written;
}

int32_t FAT32_AppendFile(uint32_t dir_cluster, const char *filename, const void *data, uint32_t size)
{
    FAT32_FileInfo_t file_info;
    uint32_t dir_sector, dir_offset;
    uint32_t last_cluster, new_cluster;
    uint32_t old_size, new_size;
    uint32_t bytes_written = 0;
    const uint8_t *data_ptr = (const uint8_t *)data;
    BG_ERR ret;
    uint32_t spc;
    uint32_t bytes_in_last_cluster;
    uint32_t cluster_size_bytes;

    if (!g_fat32_state.initialized || !filename || !data || size == 0) {
        return -1;
    }

    spc = g_fat32_state.fs_info.bpb.sectors_per_cluster;
    cluster_size_bytes = spc * FAT32_SECTOR_SIZE;

    /* 查找文件是否存在 */
    ret = FAT32_FindEntryInDir(dir_cluster, filename, &file_info);
    if (ret != SUCCESS) {
        /* 文件不存在，创建新文件 */
        return FAT32_WriteFile(dir_cluster, filename, data, size);
    }

    /* 文件存在，追加数据 */
    old_size = file_info.size;
    new_size = old_size + size;

    /* 查找最后一个簇 */
    last_cluster = file_info.start_cluster;
    if (last_cluster == 0) {
        /* 空文件，分配第一个簇 */
        ret = fat32_alloc_cluster(&last_cluster);
        if (ret != SUCCESS) {
            return -1;
        }
        file_info.start_cluster = last_cluster;
    } else {
        /* 遍历簇链找到最后一个簇 */
        uint32_t current = last_cluster;
        uint32_t next;
        while (1) {
            ret = fat32_get_next_cluster(current, &next);
            if (ret != SUCCESS) {
                return -1;
            }
            if (next >= FAT32_CLUSTER_EOF_MIN) {
                last_cluster = current;
                break;
            }
            current = next;
        }
    }

    /* 计算最后一个簇中已有多少字节 */
    bytes_in_last_cluster = old_size % cluster_size_bytes;

    /* 如果最后一个簇还有剩余空间，先填满它 */
    if (bytes_in_last_cluster > 0 && bytes_in_last_cluster < cluster_size_bytes) {
        uint32_t space_left = cluster_size_bytes - bytes_in_last_cluster;
        uint32_t to_write = (size < space_left) ? size : space_left;
        uint32_t start_lba = g_fat32_state.fs_info.data_start_sector +
                             ((last_cluster - 2u) * spc);
        uint32_t sector_offset = bytes_in_last_cluster / FAT32_SECTOR_SIZE;
        uint32_t byte_offset = bytes_in_last_cluster % FAT32_SECTOR_SIZE;

        /* 读取部分填充的扇区 */
        ret = fat32_read_sector(start_lba + sector_offset, g_fat32_state.sector_cache);
        if (ret != SUCCESS) {
            return -1;
        }

        /* 填充数据 */
        {
            uint32_t written_in_sector = 0;
            while (written_in_sector < to_write && byte_offset < FAT32_SECTOR_SIZE) {
                uint32_t chunk = FAT32_SECTOR_SIZE - byte_offset;
                if (chunk > (to_write - written_in_sector)) {
                    chunk = to_write - written_in_sector;
                }
                memcpy(g_fat32_state.sector_cache + byte_offset, data_ptr + written_in_sector, chunk);
                written_in_sector += chunk;
                bytes_written += chunk;
                
                /* 写回扇区 */
                g_fat32_state.cache_valid = false;
                ret = fat32_write_sector(start_lba + sector_offset, g_fat32_state.sector_cache);
                if (ret != SUCCESS) {
                    return -1;
                }
                
                if (written_in_sector < to_write) {
                    sector_offset++;
                    byte_offset = 0;
                    if (sector_offset < spc) {
                        ret = fat32_read_sector(start_lba + sector_offset, g_fat32_state.sector_cache);
                        if (ret != SUCCESS) {
                            return -1;
                        }
                    }
                }
            }
        }
    }

    /* 写入剩余数据到新簇 */
    while (bytes_written < size) {
        uint32_t current_cluster;
        uint32_t start_lba;
        uint32_t s;
        
        /* 分配新簇 */
        ret = fat32_alloc_cluster(&new_cluster);
        if (ret != SUCCESS) {
            break;
        }
        
        /* 链接到簇链 */
        ret = fat32_set_fat_entry(last_cluster, new_cluster);
        if (ret != SUCCESS) {
            break;
        }
        
        current_cluster = new_cluster;
        last_cluster = new_cluster;
        
        /* 写入数据到新簇 */
        start_lba = g_fat32_state.fs_info.data_start_sector +
                    ((current_cluster - 2u) * spc);
        
        for (s = 0; s < spc && bytes_written < size; s++) {
            uint32_t remaining = size - bytes_written;
            uint32_t chunk = (remaining >= FAT32_SECTOR_SIZE) ? FAT32_SECTOR_SIZE : remaining;
            
            if (chunk == FAT32_SECTOR_SIZE) {
                ret = fat32_write_sector(start_lba + s, data_ptr + bytes_written);
            } else {
                memset(g_fat32_state.sector_cache, 0, FAT32_SECTOR_SIZE);
                memcpy(g_fat32_state.sector_cache, data_ptr + bytes_written, chunk);
                g_fat32_state.cache_valid = false;
                ret = fat32_write_sector(start_lba + s, g_fat32_state.sector_cache);
            }
            
            if (ret != SUCCESS) {
                return -1;
            }
            bytes_written += chunk;
        }
    }

    /* 标记最后一个簇为 EOF */
    ret = fat32_set_fat_entry(last_cluster, FAT32_CLUSTER_EOF_MIN);
    if (ret != SUCCESS) {
        return -1;
    }

    /* 更新目录项中的文件大小 */
    ret = fat32_find_dir_entry(dir_cluster, filename, &dir_sector, &dir_offset);
    if (ret == SUCCESS) {
        uint8_t sector_buf[FAT32_SECTOR_SIZE];
        FAT32_DirEntry_t *entry;
        
        ret = fat32_read_sector(dir_sector, sector_buf);
        if (ret == SUCCESS) {
            entry = (FAT32_DirEntry_t *)(sector_buf + dir_offset);
            entry->size = new_size;
            
            /* 如果是新创建的文件，更新起始簇 */
            if (old_size == 0 && file_info.start_cluster != 0) {
                entry->cluster_high = (uint16_t)(file_info.start_cluster >> 16);
                entry->cluster_low = (uint16_t)(file_info.start_cluster & 0xFFFF);
            }
            
            ret = fat32_write_sector(dir_sector, sector_buf);
        }
    }

    return (int32_t)bytes_written;
}

BG_ERR FAT32_DeleteFile(uint32_t dir_cluster, const char *filename)
{
    BG_ERR ret;
    uint8_t *entry_ptr;
    uint16_t lfn_checksum;
    char lfn_buffer[FAT32_MAX_PATH_LENGTH];
    bool has_lfn;
    FAT32_FileInfo_t file_info;
    BG_ERR parse_ret;
    uint32_t spc;
    uint32_t s;
    uint32_t i;
    uint32_t lba;

    if (!g_fat32_state.initialized || !filename) {
        return ENABLE_INVALID_INPUT;
    }

    spc = g_fat32_state.fs_info.bpb.sectors_per_cluster;
    lfn_checksum = 0;
    memset(lfn_buffer, 0, sizeof(lfn_buffer));
    has_lfn = false;

    /* FAT16 根目录：固定扇区范围 */
    if (g_fat32_state.fs_info.is_fat16 && dir_cluster == FAT16_ROOT_DIR_CLUSTER) {
        uint32_t end_lba = g_fat32_state.fs_info.root_dir_start_lba +
                           g_fat32_state.fs_info.root_dir_num_sectors;
        for (lba = g_fat32_state.fs_info.root_dir_start_lba; lba < end_lba; lba++) {
            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) { return ENABLE_NOT_FOUND; }
                if (*entry_ptr == 0xE5) { continue; }
                parse_ret = fat32_parse_dir_entry(entry_ptr, &file_info,
                                                  &lfn_checksum, lfn_buffer);
                if (parse_ret == ENABLE_INVALID_INPUT) { has_lfn = true; continue; }
                if (parse_ret == SUCCESS) {
                    const char *nm = (has_lfn && lfn_buffer[0]) ? lfn_buffer : file_info.name;
                    if (strcmp(filename, nm) == 0) {
                        *entry_ptr = 0xE5;
                        return fat32_write_sector(lba, g_fat32_state.sector_cache);
                    }
                    has_lfn = false;
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                }
            }
        }
        return ENABLE_NOT_FOUND;
    }

    /* FAT32: 按簇链遍历 */
    while (dir_cluster < FAT32_CLUSTER_EOF_MIN) {
        for (s = 0; s < spc; s++) {
            lba = g_fat32_state.fs_info.data_start_sector +
                  ((dir_cluster - 2u) * spc) + s;
            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) { return ENABLE_NOT_FOUND; }
                if (*entry_ptr == 0xE5) { continue; }
                parse_ret = fat32_parse_dir_entry(entry_ptr, &file_info,
                                                  &lfn_checksum, lfn_buffer);
                if (parse_ret == ENABLE_INVALID_INPUT) { has_lfn = true; continue; }
                if (parse_ret == SUCCESS) {
                    const char *nm = (has_lfn && lfn_buffer[0]) ? lfn_buffer : file_info.name;
                    if (strcmp(filename, nm) == 0) {
                        *entry_ptr = 0xE5;
                        return fat32_write_sector(lba, g_fat32_state.sector_cache);
                    }
                    has_lfn = false;
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                }
            }
        }
        ret = fat32_get_next_cluster(dir_cluster, &dir_cluster);
        if (ret != SUCCESS) { return ret; }
    }
    return ENABLE_NOT_FOUND;
}

BG_ERR FAT32_MkDir(uint32_t dir_cluster, const char *dirname)
{
    char short_name[11];
    FAT32_DirEntry_t dir_entry;
    uint32_t dir_sector, dir_offset;
    uint32_t new_cluster;
    uint32_t spc;
    BG_ERR ret;

    if (!g_fat32_state.initialized || !dirname || !*dirname) {
        return ENABLE_INVALID_INPUT;
    }

    fat32_make_83name(dirname, short_name);

    /* 在指定目录中找空闲目录项 */
    ret = fat32_find_free_dir_entry(dir_cluster,
                                    &dir_sector, &dir_offset);
    if (ret != SUCCESS) {
        return ret;
    }

    /* 分配一个簇存放新目录内容 */
    ret = fat32_alloc_cluster(&new_cluster);
    if (ret != SUCCESS) {
        return ret;
    }

    /* 零填充新目录的所有扇区 */
    spc = g_fat32_state.fs_info.bpb.sectors_per_cluster;
    {
        uint32_t s;
        uint32_t start_lba = g_fat32_state.fs_info.data_start_sector +
                             ((new_cluster - 2u) * spc);
        memset(g_fat32_state.sector_cache, 0, FAT32_SECTOR_SIZE);
        g_fat32_state.cache_valid = false;
        for (s = 0; s < spc; s++) {
            ret = fat32_write_sector(start_lba + s, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
        }
    }

    /* 写目录项 */
    memset(&dir_entry, 0, sizeof(dir_entry));
    memcpy(dir_entry.name, short_name, 11);
    dir_entry.attr         = DIR_ATTR_DIRECTORY;
    dir_entry.cluster_high = (uint16_t)(new_cluster >> 16);  /* FAT16 时始终为 0 */
    dir_entry.cluster_low  = (uint16_t)(new_cluster & 0xFFFFu);
    dir_entry.size         = 0;  /* 目录大小字段始终为 0 */

    {
        uint8_t sector_buf[FAT32_SECTOR_SIZE];
        ret = fat32_read_sector(dir_sector, sector_buf);
        if (ret != SUCCESS) { return ret; }
        memcpy(sector_buf + dir_offset, &dir_entry, sizeof(dir_entry));
        return fat32_write_sector(dir_sector, sector_buf);
    }
}

BG_ERR FAT32_RmDir(uint32_t dir_cluster, const char *dirname)
{
    BG_ERR ret;
    uint8_t *entry_ptr;
    uint16_t lfn_checksum;
    char lfn_buffer[FAT32_MAX_PATH_LENGTH];
    bool has_lfn;
    FAT32_FileInfo_t file_info;
    FAT32_DirEntry_t *raw_entry;
    BG_ERR parse_ret;
    uint32_t spc;
    uint32_t s;
    uint32_t i;
    uint32_t lba;

    if (!g_fat32_state.initialized || !dirname || !*dirname) {
        return ENABLE_INVALID_INPUT;
    }

    spc = g_fat32_state.fs_info.bpb.sectors_per_cluster;
    lfn_checksum = 0;
    memset(lfn_buffer, 0, sizeof(lfn_buffer));
    has_lfn = false;

    /* FAT16 根目录：固定扇区范围 */
    if (g_fat32_state.fs_info.is_fat16 && dir_cluster == FAT16_ROOT_DIR_CLUSTER) {
        uint32_t end_lba = g_fat32_state.fs_info.root_dir_start_lba +
                           g_fat32_state.fs_info.root_dir_num_sectors;
        for (lba = g_fat32_state.fs_info.root_dir_start_lba; lba < end_lba; lba++) {
            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) { return ENABLE_NOT_FOUND; }
                if (*entry_ptr == 0xE5) { continue; }
                raw_entry = (FAT32_DirEntry_t *)entry_ptr;
                parse_ret = fat32_parse_dir_entry(entry_ptr, &file_info,
                                                  &lfn_checksum, lfn_buffer);
                if (parse_ret == ENABLE_INVALID_INPUT) { has_lfn = true; continue; }
                if (parse_ret == SUCCESS) {
                    const char *nm = (has_lfn && lfn_buffer[0]) ? lfn_buffer : file_info.name;
                    if (strcmp(dirname, nm) == 0) {
                        uint32_t clust;
                        if (!(raw_entry->attr & DIR_ATTR_DIRECTORY)) {
                            return ENABLE_INVALID_INPUT;
                        }
                        /* 释放簇链 */
                        clust = ((uint32_t)raw_entry->cluster_high << 16) | raw_entry->cluster_low;
                        while (clust >= 2u && clust < FAT32_CLUSTER_EOF_MIN) {
                            uint32_t next_clust = FAT32_CLUSTER_EOF_MIN;
                            fat32_get_next_cluster(clust, &next_clust);
                            fat32_set_fat_entry(clust, 0u);
                            clust = next_clust;
                        }
                        *entry_ptr = 0xE5;
                        return fat32_write_sector(lba, g_fat32_state.sector_cache);
                    }
                    has_lfn = false;
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                }
            }
        }
        return ENABLE_NOT_FOUND;
    }

    /* FAT32: 按簇链遍历 */
    while (dir_cluster < FAT32_CLUSTER_EOF_MIN) {
        for (s = 0; s < spc; s++) {
            lba = g_fat32_state.fs_info.data_start_sector +
                  ((dir_cluster - 2u) * spc) + s;
            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) { return ENABLE_NOT_FOUND; }
                if (*entry_ptr == 0xE5) { continue; }
                raw_entry = (FAT32_DirEntry_t *)entry_ptr;
                parse_ret = fat32_parse_dir_entry(entry_ptr, &file_info,
                                                  &lfn_checksum, lfn_buffer);
                if (parse_ret == ENABLE_INVALID_INPUT) { has_lfn = true; continue; }
                if (parse_ret == SUCCESS) {
                    const char *nm = (has_lfn && lfn_buffer[0]) ? lfn_buffer : file_info.name;
                    if (strcmp(dirname, nm) == 0) {
                        uint32_t clust;
                        if (!(raw_entry->attr & DIR_ATTR_DIRECTORY)) {
                            return ENABLE_INVALID_INPUT;
                        }
                        /* 释放簇链 */
                        clust = ((uint32_t)raw_entry->cluster_high << 16) | raw_entry->cluster_low;
                        while (clust >= 2u && clust < FAT32_CLUSTER_EOF_MIN) {
                            uint32_t next_clust = FAT32_CLUSTER_EOF_MIN;
                            fat32_get_next_cluster(clust, &next_clust);
                            fat32_set_fat_entry(clust, 0u);
                            clust = next_clust;
                        }
                        *entry_ptr = 0xE5;
                        return fat32_write_sector(lba, g_fat32_state.sector_cache);
                    }
                    has_lfn = false;
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                }
            }
        }
        ret = fat32_get_next_cluster(dir_cluster, &dir_cluster);
        if (ret != SUCCESS) { return ret; }
    }
    return ENABLE_NOT_FOUND;
}

BG_ERR FAT32_Rename(uint32_t dir_cluster, const char *oldname, const char *newname)
{
    BG_ERR ret;
    uint8_t *entry_ptr;
    uint16_t lfn_checksum;
    char lfn_buffer[FAT32_MAX_PATH_LENGTH];
    bool has_lfn;
    FAT32_FileInfo_t file_info;
    FAT32_DirEntry_t *raw_entry;
    BG_ERR parse_ret;
    char new_83name[11];
    uint32_t spc;
    uint32_t s;
    uint32_t i;
    uint32_t lba;

    if (!g_fat32_state.initialized || !oldname || !*oldname || !newname || !*newname) {
        return ENABLE_INVALID_INPUT;
    }

    fat32_make_83name(newname, new_83name);
    spc = g_fat32_state.fs_info.bpb.sectors_per_cluster;
    lfn_checksum = 0;
    memset(lfn_buffer, 0, sizeof(lfn_buffer));
    has_lfn = false;

    /* FAT16 根目录：固定扇区范围 */
    if (g_fat32_state.fs_info.is_fat16 && dir_cluster == FAT16_ROOT_DIR_CLUSTER) {
        uint32_t end_lba = g_fat32_state.fs_info.root_dir_start_lba +
                           g_fat32_state.fs_info.root_dir_num_sectors;
        for (lba = g_fat32_state.fs_info.root_dir_start_lba; lba < end_lba; lba++) {
            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) { return ENABLE_NOT_FOUND; }
                if (*entry_ptr == 0xE5) { continue; }
                raw_entry = (FAT32_DirEntry_t *)entry_ptr;
                parse_ret = fat32_parse_dir_entry(entry_ptr, &file_info,
                                                  &lfn_checksum, lfn_buffer);
                if (parse_ret == ENABLE_INVALID_INPUT) { has_lfn = true; continue; }
                if (parse_ret == SUCCESS) {
                    const char *nm = (has_lfn && lfn_buffer[0]) ? lfn_buffer : file_info.name;
                    if (strcmp(oldname, nm) == 0) {
                        memcpy(raw_entry->name, new_83name, 11);
                        return fat32_write_sector(lba, g_fat32_state.sector_cache);
                    }
                    has_lfn = false;
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                }
            }
        }
        return ENABLE_NOT_FOUND;
    }

    /* FAT32: 按簇链遍历 */
    while (dir_cluster < FAT32_CLUSTER_EOF_MIN) {
        for (s = 0; s < spc; s++) {
            lba = g_fat32_state.fs_info.data_start_sector +
                  ((dir_cluster - 2u) * spc) + s;
            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) { return ENABLE_NOT_FOUND; }
                if (*entry_ptr == 0xE5) { continue; }
                raw_entry = (FAT32_DirEntry_t *)entry_ptr;
                parse_ret = fat32_parse_dir_entry(entry_ptr, &file_info,
                                                  &lfn_checksum, lfn_buffer);
                if (parse_ret == ENABLE_INVALID_INPUT) { has_lfn = true; continue; }
                if (parse_ret == SUCCESS) {
                    const char *nm = (has_lfn && lfn_buffer[0]) ? lfn_buffer : file_info.name;
                    if (strcmp(oldname, nm) == 0) {
                        memcpy(raw_entry->name, new_83name, 11);
                        return fat32_write_sector(lba, g_fat32_state.sector_cache);
                    }
                    has_lfn = false;
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                }
            }
        }
        ret = fat32_get_next_cluster(dir_cluster, &dir_cluster);
        if (ret != SUCCESS) { return ret; }
    }
    return ENABLE_NOT_FOUND;
}

/* ============================================
 * 写相关内部函数实现
 * ============================================ */

static BG_ERR fat32_write_sector(uint32_t sector, const uint8_t *buffer)
{
    int retry_count = 0;
    BG_ERR ret;
    const FAT32_DiskIO_t *dio;

    dio = FAT32_DiskIO_GetCurrent();
    if (!dio) {
        return ENABLE_DEVICE_NOT_READY;
    }

    while (retry_count < FAT32_RETRY_COUNT) {
        ret = dio->write_sectors(sector, buffer, 1);
        if (ret == SUCCESS) {
            memcpy(g_fat32_state.sector_cache, buffer, FAT32_SECTOR_SIZE);
            g_fat32_state.cached_sector = sector;
            g_fat32_state.cache_valid = true;
            return SUCCESS;
        }
        retry_count++;
    }
    return ENABLE_IO_ERROR;
}

static BG_ERR fat32_set_fat_entry(uint32_t cluster, uint32_t value)
{
    uint32_t fat_sector;
    uint32_t fat_offset;
    BG_ERR ret;
    uint8_t sector_buf[FAT32_SECTOR_SIZE];

    if (g_fat32_state.fs_info.is_fat16) {
        fat_sector = g_fat32_state.fs_info.fat_start_sector + (cluster * 2u) / FAT32_SECTOR_SIZE;
        fat_offset = (cluster * 2u) % FAT32_SECTOR_SIZE;
        ret = fat32_read_sector(fat_sector, sector_buf);
        if (ret != SUCCESS) { return ret; }
        *(uint16_t *)(sector_buf + fat_offset) =
            (value >= FAT32_CLUSTER_EOF_MIN) ? 0xFFFFu : (uint16_t)value;
    } else {
        fat_sector = g_fat32_state.fs_info.fat_start_sector + (cluster * 4) / FAT32_SECTOR_SIZE;
        fat_offset = (cluster * 4) % FAT32_SECTOR_SIZE;
        ret = fat32_read_sector(fat_sector, sector_buf);
        if (ret != SUCCESS) { return ret; }
        *(uint32_t *)(sector_buf + fat_offset) = value;
    }

    return fat32_write_sector(fat_sector, sector_buf);
}

static BG_ERR fat32_alloc_cluster(uint32_t *out_cluster)
{
    uint32_t cluster;

    /* 简化实现：从数据区起始簇开始查找空闲簇 */
    for (cluster = 2; cluster < g_fat32_state.fs_info.total_clusters + 2; cluster++) {
        uint32_t fat_value;
        BG_ERR ret = fat32_get_next_cluster(cluster, &fat_value);
        if (ret != SUCCESS) {
            return ret;
        }
        if (fat_value == FAT32_CLUSTER_FREE) {
            /* 找到空闲簇 */
            *out_cluster = cluster;
            return fat32_set_fat_entry(cluster, FAT32_CLUSTER_EOF_MIN); /* 暂时标记为 EOF */
        }
    }

    return ENABLE_OUT_OF_MEMORY; /* 无空闲簇 */
}

static void fat32_make_83name(const char *filename, char out83[11])
{
    char name[9] = {0};
    char ext[4] = {0};
    const char *dot;
    int i;

    /* 查找扩展名 */
    dot = strrchr(filename, '.');
    if (dot) {
        /* 有扩展名 */
        int name_len = dot - filename;
        int ext_len = strlen(dot + 1);

        if (name_len > 8) name_len = 8;
        if (ext_len > 3) ext_len = 3;

        memcpy(name, filename, name_len);
        memcpy(ext, dot + 1, ext_len);
    } else {
        /* 无扩展名 */
        int name_len = strlen(filename);
        if (name_len > 8) name_len = 8;
        memcpy(name, filename, name_len);
    }

    /* 转换为大写 */
    for (i = 0; i < 8 && name[i]; i++) {
        if (name[i] >= 'a' && name[i] <= 'z') {
            name[i] -= 32;
        }
    }
    for (i = 0; i < 3 && ext[i]; i++) {
        if (ext[i] >= 'a' && ext[i] <= 'z') {
            ext[i] -= 32;
        }
    }

    /* 填充输出缓冲区 */
    memset(out83, ' ', 11);
    memcpy(out83, name, strlen(name));
    memcpy(out83 + 8, ext, strlen(ext));
}

static BG_ERR fat32_find_free_dir_entry(uint32_t dir_cluster, uint32_t *out_sector, uint32_t *out_offset)
{
    BG_ERR ret;
    uint8_t *entry_ptr;
    uint32_t spc;
    uint32_t s;
    uint32_t i;
    uint32_t lba;

    /* FAT16 根目录：固定扇区范围 */
    if (g_fat32_state.fs_info.is_fat16 && dir_cluster == FAT16_ROOT_DIR_CLUSTER) {
        uint32_t end_lba = g_fat32_state.fs_info.root_dir_start_lba +
                           g_fat32_state.fs_info.root_dir_num_sectors;
        for (lba = g_fat32_state.fs_info.root_dir_start_lba; lba < end_lba; lba++) {
            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00 || *entry_ptr == 0xE5) {
                    *out_sector = lba;
                    *out_offset = i;
                    return SUCCESS;
                }
            }
        }
        return ENABLE_OUT_OF_MEMORY;  /* 根目录已满 */
    }

    spc = g_fat32_state.fs_info.bpb.sectors_per_cluster;

    while (dir_cluster < FAT32_CLUSTER_EOF_MIN) {
        for (s = 0; s < spc; s++) {
            lba = g_fat32_state.fs_info.data_start_sector +
                  ((dir_cluster - 2u) * spc) + s;

            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) {
                return ret;
            }

            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00 || *entry_ptr == 0xE5) {
                    *out_sector = lba;
                    *out_offset = i;
                    return SUCCESS;
                }
            }
        }

        ret = fat32_get_next_cluster(dir_cluster, &dir_cluster);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return ENABLE_OUT_OF_MEMORY;
}

static BG_ERR fat32_find_dir_entry(uint32_t dir_cluster, const char *filename, uint32_t *out_sector, uint32_t *out_offset)
{
    BG_ERR ret;
    uint8_t *entry_ptr;
    char short_name[11];
    uint32_t spc;
    uint32_t s;
    uint32_t i;
    uint32_t lba;

    /* 生成 8.3 短文件名用于比较 */
    fat32_make_83name(filename, short_name);

    /* FAT16 根目录：固定扇区范围 */
    if (g_fat32_state.fs_info.is_fat16 && dir_cluster == FAT16_ROOT_DIR_CLUSTER) {
        uint32_t end_lba = g_fat32_state.fs_info.root_dir_start_lba +
                           g_fat32_state.fs_info.root_dir_num_sectors;
        for (lba = g_fat32_state.fs_info.root_dir_start_lba; lba < end_lba; lba++) {
            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) { return ret; }
            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) {
                    return ENABLE_NOT_FOUND;  /* 目录结束 */
                }
                if (*entry_ptr == 0xE5) {
                    continue;  /* 已删除项 */
                }
                FAT32_DirEntry_t *entry = (FAT32_DirEntry_t *)entry_ptr;
                if (entry->attr == DIR_ATTR_LONG_NAME) {
                    continue;  /* 跳过长文件名项 */
                }
                if (memcmp(entry->name, short_name, 11) == 0) {
                    *out_sector = lba;
                    *out_offset = i;
                    return SUCCESS;
                }
            }
        }
        return ENABLE_NOT_FOUND;
    }

    spc = g_fat32_state.fs_info.bpb.sectors_per_cluster;

    while (dir_cluster < FAT32_CLUSTER_EOF_MIN) {
        for (s = 0; s < spc; s++) {
            lba = g_fat32_state.fs_info.data_start_sector +
                  ((dir_cluster - 2u) * spc) + s;

            ret = fat32_read_sector(lba, g_fat32_state.sector_cache);
            if (ret != SUCCESS) {
                return ret;
            }

            entry_ptr = g_fat32_state.sector_cache;
            for (i = 0; i < FAT32_SECTOR_SIZE; i += DIR_ENTRY_SIZE, entry_ptr += DIR_ENTRY_SIZE) {
                if (*entry_ptr == 0x00) {
                    return ENABLE_NOT_FOUND;  /* 目录结束 */
                }
                if (*entry_ptr == 0xE5) {
                    continue;  /* 已删除项 */
                }
                FAT32_DirEntry_t *entry = (FAT32_DirEntry_t *)entry_ptr;
                if (entry->attr == DIR_ATTR_LONG_NAME) {
                    continue;  /* 跳过长文件名项 */
                }
                if (memcmp(entry->name, short_name, 11) == 0) {
                    *out_sector = lba;
                    *out_offset = i;
                    return SUCCESS;
                }
            }
        }

        ret = fat32_get_next_cluster(dir_cluster, &dir_cluster);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return ENABLE_NOT_FOUND;
}

static BG_ERR fat32_parse_dir_entry(uint8_t *entry_data, FAT32_FileInfo_t *file_info,
                                   uint16_t *lfn_checksum, char *lfn_buffer)
{
    FAT32_DirEntry_t *dir_entry = (FAT32_DirEntry_t *)entry_data;

    /* 检查是否为长文件名项 */
    if (dir_entry->attr == DIR_ATTR_LONG_NAME) {
        FAT32_LFNEntry_t *lfn_entry = (FAT32_LFNEntry_t *)entry_data;

        /* 解析长文件名 */
        uint8_t sequence = lfn_entry->sequence & LFN_SEQUENCE_MASK;
        if (sequence > 0 && sequence <= FAT32_MAX_LFN_ENTRIES) {
            /* 计算在缓冲区中的位置 (注意：FAT32 LFN 是倒序存储的) */
            char *dest_ptr = lfn_buffer + (sequence - 1) * 13;

            /* 复制文件名部分 */
            uint16_t *unicode_ptr = lfn_entry->name1;
            fat32_lfn_to_utf8(unicode_ptr, 5, dest_ptr);
            unicode_ptr = lfn_entry->name2;
            fat32_lfn_to_utf8(unicode_ptr, 6, dest_ptr + 5);
            unicode_ptr = lfn_entry->name3;
            fat32_lfn_to_utf8(unicode_ptr, 2, dest_ptr + 11);

            *lfn_checksum = lfn_entry->checksum;
        }
        return ENABLE_INVALID_INPUT;  /* 长文件名项，继续处理 */
    }

    /* 短文件名项 */
    if (dir_entry->attr & DIR_ATTR_VOLUME_ID) {
        return ENABLE_INVALID_INPUT;  /* 卷标 */
    }

    /* 构造文件名 */
    if (lfn_buffer[0] != '\0') {
        /* 使用长文件名 */
        strcpy(file_info->name, lfn_buffer);
        memset(lfn_buffer, 0, FAT32_MAX_PATH_LENGTH);
    } else {
        /* 使用短文件名: 拆分 8 字节主名 + 3 字节扩展名，中间插点 */
        char base[9] = {0};
        char ext[4]  = {0};
        int  trim_i;

        memcpy(base, dir_entry->name,     8);
        memcpy(ext,  dir_entry->name + 8, 3);

        /* 移除尾部空格 */
        for (trim_i = 7; trim_i >= 0 && base[trim_i] == ' '; trim_i--) base[trim_i] = '\0';
        for (trim_i = 2; trim_i >= 0 && ext[trim_i]  == ' '; trim_i--) ext[trim_i]  = '\0';

        if (ext[0] != '\0') {
            strcpy(file_info->name, base);
            strcat(file_info->name, ".");
            strcat(file_info->name, ext);
        } else {
            strcpy(file_info->name, base);
        }
    }

    /* 填充文件信息 */
    file_info->size = dir_entry->size;
    file_info->start_cluster = (dir_entry->cluster_high << 16) | dir_entry->cluster_low;
    file_info->modify_time = dir_entry->modify_time;
    file_info->modify_date = dir_entry->modify_date;
    file_info->attr = dir_entry->attr;

    return SUCCESS;
}

static uint8_t fat32_lfn_checksum(const char *short_name)
{
    uint8_t sum = 0;
    int i;
    for (i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + short_name[i];
    }
    return sum;
}

static void fat32_lfn_to_utf8(uint16_t *lfn_unicode, uint8_t length, char *utf8_buffer)
{
    uint8_t i;
    for (i = 0; i < length; i++) {
        uint16_t unicode = lfn_unicode[i];
        if (unicode == 0 || unicode == 0xFFFF) {
            break;
        }
        /* 简单 Unicode 到 ASCII 转换 (仅支持基本 ASCII) */
        if (unicode < 128) {
            *utf8_buffer++ = (char)unicode;
        } else {
            *utf8_buffer++ = '?';  /* 不支持的字符 */
        }
    }
}

#endif /* FAT32_EN */
#endif /* BANGTSYNTH_LEGACY */
