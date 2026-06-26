/**
 * @file fat32_reader.c
 * @brief BanDataHub FAT16/FAT32 file operations.
 */
#include "product_def.h"

#if FAT32_EN

#include "fat32_reader.h"
<<<<<<< Updated upstream
#include <string.h>
=======
<<<<<<< HEAD
#include <string.h>
=======
<<<<<<< HEAD
#include "fat32_diskio.h"
#include "bg_shell.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>

/* ---------- Debug macros ---------- */
#ifndef FAT32_DEBUG
#define FAT32_DEBUG  0   /* 0=off, 1=on */
#endif

#if FAT32_DEBUG
  /* Frequent logs go to UART (DBG/printf), not USB shell */
  #define FAT32_DBG(fmt, ...)  DBG("[FAT32] " fmt, ##__VA_ARGS__)
  #define FAT32_LOGW(fmt, ...) DBG("[FAT32-W] " fmt, ##__VA_ARGS__)
  #define FAT32_LOGE(fmt, ...) DBG("[FAT32-E] " fmt, ##__VA_ARGS__)
#else
  #define FAT32_DBG(fmt, ...)  ((void)0)
  #define FAT32_LOGW(fmt, ...) DBG("[FAT32-W] " fmt, ##__VA_ARGS__)
  #define FAT32_LOGE(fmt, ...) DBG("[FAT32-E] " fmt, ##__VA_ARGS__)
#endif
=======
#include <string.h>
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes

#define FAT_EOC32              0x0FFFFFF8u
#define FAT_EOC16              0xFFF8u
#define FAT_FREE               0x00000000u
#define FAT_MAX_OPEN_FILES     4
#define FAT_OPEN_READ          0x01
#define FAT_OPEN_WRITE         0x02
#define FAT_OPEN_CREATE        0x04

typedef struct {
    bool mounted;
    uint32_t part_lba;
    uint32_t fat_size;
    FAT32_FSInfo_t info;
    uint8_t sector[FAT32_SECTOR_SIZE];
} FatState_t;

static FatState_t s_fat;
static FAT32_FileHandle_t s_open_files[FAT_MAX_OPEN_FILES];
static uint8_t s_open_used[FAT_MAX_OPEN_FILES];

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static char upcase(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

static int name_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (upcase(*a) != upcase(*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
static int is_fat_partition_type(uint8_t type)
{
    return type == 0x01u || type == 0x04u || type == 0x06u ||
           type == 0x0Bu || type == 0x0Cu || type == 0x0Eu;
}

static void copy_fs_type(const uint8_t *b, uint16_t off, char out[9])
{
    uint8_t i;

    for (i = 0; i < 8u; i++) {
        uint8_t c = b[off + i];
        out[i] = (c >= 0x20u && c < 0x7Fu) ? (char)c : '.';
    }
    out[8] = '\0';
}

static int is_fat_boot_sector(const uint8_t *b)
{
    uint8_t spc = b[0x0D];
    uint8_t fats = b[0x10];
    uint16_t rsvd = rd16(&b[0x0E]);
    uint16_t fatsz16 = rd16(&b[0x16]);
    uint32_t fatsz32 = rd32(&b[0x24]);
    uint16_t total16 = rd16(&b[0x13]);
    uint32_t total32 = rd32(&b[0x20]);

    return b[510] == 0x55 && b[511] == 0xAA &&
           (b[0] == 0xEB || b[0] == 0xE9) &&
           rd16(&b[0x0B]) == FAT32_SECTOR_SIZE &&
           (spc == 1 || spc == 2 || spc == 4 || spc == 8 ||
            spc == 16 || spc == 32 || spc == 64 || spc == 128) &&
           (fats == 1 || fats == 2) &&
           rsvd != 0 &&
           (total16 != 0 || total32 != 0) &&
           (fatsz16 != 0 || fatsz32 != 0) &&
           (b[0x15] == 0xF8 || b[0x15] == 0xF0);
}

=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
static BG_ERR disk_read(uint32_t lba, uint8_t *buf, uint32_t count)
{
    const FAT32_DiskIO_t *dio = FAT32_DiskIO_GetCurrent();
    if (!dio || !dio->read_sectors) {
        return ENABLE_DEVICE_NOT_READY;
    }
    return dio->read_sectors(lba, buf, count);
}

static BG_ERR disk_write(uint32_t lba, const uint8_t *buf, uint32_t count)
{
    const FAT32_DiskIO_t *dio = FAT32_DiskIO_GetCurrent();
    if (!dio || !dio->write_sectors) {
        return ENABLE_DEVICE_NOT_READY;
    }
    return dio->write_sectors(lba, buf, count);
}

static uint32_t cluster_to_lba(uint32_t cluster)
{
    return s_fat.info.data_start_sector +
           (cluster - 2u) * s_fat.info.bpb.sectors_per_cluster;
}

static BG_ERR read_fat_entry(uint32_t cluster, uint32_t *value)
{
    uint32_t offset;
    uint32_t sector;
    uint32_t pos;
    BG_ERR ret;

    if (!value) {
        return ENABLE_INVALID_INPUT;
    }

    offset = s_fat.info.is_fat16 ? (cluster * 2u) : (cluster * 4u);
    sector = s_fat.info.fat_start_sector + (offset / FAT32_SECTOR_SIZE);
    pos = offset % FAT32_SECTOR_SIZE;

    ret = disk_read(sector, s_fat.sector, 1);
    if (ret != SUCCESS) {
        return ret;
    }

    if (s_fat.info.is_fat16) {
        *value = rd16(&s_fat.sector[pos]);
    } else {
        *value = rd32(&s_fat.sector[pos]) & 0x0FFFFFFFu;
    }
    return SUCCESS;
}

static BG_ERR write_fat_entry(uint32_t cluster, uint32_t value)
{
    uint32_t offset;
    uint32_t fat;
    uint32_t sector;
    uint32_t pos;
    BG_ERR ret;

    offset = s_fat.info.is_fat16 ? (cluster * 2u) : (cluster * 4u);

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
#if FAT32_DEBUG
    FAT32_DBG("write_fat: cluster=%lu value=0x%08lX\n",
              (unsigned long)cluster, (unsigned long)value);
#endif

=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    for (fat = 0; fat < s_fat.info.bpb.num_fats; fat++) {
        sector = s_fat.info.fat_start_sector + fat * s_fat.fat_size + (offset / FAT32_SECTOR_SIZE);
        pos = offset % FAT32_SECTOR_SIZE;
        ret = disk_read(sector, s_fat.sector, 1);
        if (ret != SUCCESS) {
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
            FAT32_LOGE("write_fat: disk_read sector=%lu failed: %d\n",
                       (unsigned long)sector, ret);
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
            return ret;
        }
        if (s_fat.info.is_fat16) {
            wr16(&s_fat.sector[pos], (uint16_t)value);
        } else {
            wr32(&s_fat.sector[pos], value & 0x0FFFFFFFu);
        }
        ret = disk_write(sector, s_fat.sector, 1);
        if (ret != SUCCESS) {
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
            FAT32_LOGE("write_fat: disk_write sector=%lu failed: %d\n",
                       (unsigned long)sector, ret);
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
            return ret;
        }
    }
    return SUCCESS;
}

static bool is_eoc(uint32_t value)
{
    return s_fat.info.is_fat16 ? (value >= FAT_EOC16) : (value >= FAT_EOC32);
}

static BG_ERR alloc_cluster(uint32_t *cluster)
{
    uint32_t c;
    uint32_t v;
    BG_ERR ret;

    if (!cluster) {
        return ENABLE_INVALID_INPUT;
    }

    for (c = 2; c < s_fat.info.total_clusters + 2u; c++) {
        ret = read_fat_entry(c, &v);
        if (ret != SUCCESS) {
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
            FAT32_LOGE("alloc_cluster: read_fat(%lu) failed: %d\n",
                       (unsigned long)c, ret);
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
            return ret;
        }
        if (v == FAT_FREE) {
            ret = write_fat_entry(c, s_fat.info.is_fat16 ? 0xFFFFu : 0x0FFFFFFFu);
            if (ret != SUCCESS) {
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
                FAT32_LOGE("alloc_cluster: write_fat(%lu) failed: %d\n",
                           (unsigned long)c, ret);
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
                return ret;
            }
            memset(s_fat.sector, 0, FAT32_SECTOR_SIZE);
            ret = disk_write(cluster_to_lba(c), s_fat.sector, s_fat.info.bpb.sectors_per_cluster);
            if (ret != SUCCESS) {
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
                FAT32_LOGE("alloc_cluster: zero-fill cluster %lu LBA=%lu failed: %d\n",
                           (unsigned long)c, (unsigned long)cluster_to_lba(c), ret);
                return ret;
            }
            *cluster = c;
            FAT32_DBG("alloc_cluster: allocated cluster %lu\n", (unsigned long)c);
            return SUCCESS;
        }
    }
    FAT32_LOGE("alloc_cluster: disk full (total_clusters=%lu)\n",
               (unsigned long)s_fat.info.total_clusters);
=======
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
                return ret;
            }
            *cluster = c;
            return SUCCESS;
        }
    }
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    return ENABLE_OUT_OF_MEMORY;
}

static void make_short_name(const char *name, char out[11])
{
    uint8_t i;
    uint8_t j;
    const char *dot;

    memset(out, ' ', 11);
    dot = 0;
    for (i = 0; name[i]; i++) {
        if (name[i] == '.') {
            dot = &name[i];
            break;
        }
    }

    for (i = 0; i < 8 && name[i] && name[i] != '.'; i++) {
        out[i] = upcase(name[i]);
    }
    if (dot) {
        dot++;
        for (j = 0; j < 3 && dot[j]; j++) {
            out[8 + j] = upcase(dot[j]);
        }
    }
}

static void short_name_to_text(const uint8_t in[11], char *out, uint16_t max_len)
{
    uint8_t i;
    uint8_t pos = 0;

    if (max_len == 0) {
        return;
    }
    for (i = 0; i < 8 && in[i] != ' ' && pos < max_len - 1; i++) {
        out[pos++] = (char)in[i];
    }
    if (in[8] != ' ' && pos < max_len - 1) {
        out[pos++] = '.';
        for (i = 8; i < 11 && in[i] != ' ' && pos < max_len - 1; i++) {
            out[pos++] = (char)in[i];
        }
    }
    out[pos] = '\0';
}

static BG_ERR parse_dir_entry(const uint8_t *e, FAT32_FileInfo_t *info)
{
    uint32_t hi;
    uint32_t lo;

    if (!e || !info) {
        return ENABLE_INVALID_INPUT;
    }
    memset(info, 0, sizeof(FAT32_FileInfo_t));
    short_name_to_text(e, info->name, sizeof(info->name));
    info->attr = e[11];
    info->modify_time = rd16(&e[22]);
    info->modify_date = rd16(&e[24]);
    hi = rd16(&e[20]);
    lo = rd16(&e[26]);
    info->start_cluster = (hi << 16) | lo;
    info->size = rd32(&e[28]);
    return SUCCESS;
}

static BG_ERR find_dir_entry(uint32_t dir_cluster, const char *name,
                             FAT32_FileInfo_t *info, uint32_t *entry_sector,
                             uint16_t *entry_offset, uint8_t find_free)
{
    char sfn[11];
    uint32_t sector;
    uint32_t i;
    uint32_t c;
    uint32_t next;
    uint16_t off;
    BG_ERR ret;

    make_short_name(name, sfn);

    if (s_fat.info.is_fat16 && dir_cluster == 0) {
        for (i = 0; i < s_fat.info.root_dir_num_sectors; i++) {
            sector = s_fat.info.root_dir_start_lba + i;
            ret = disk_read(sector, s_fat.sector, 1);
            if (ret != SUCCESS) {
                return ret;
            }
            for (off = 0; off < FAT32_SECTOR_SIZE; off += 32) {
                uint8_t *e = &s_fat.sector[off];
                if (find_free && (e[0] == 0x00 || e[0] == 0xE5)) {
                    if (entry_sector) *entry_sector = sector;
                    if (entry_offset) *entry_offset = off;
                    return SUCCESS;
                }
                if (e[0] == 0x00) break;
                if (e[0] == 0xE5 || e[11] == DIR_ATTR_LONG_NAME || (e[11] & DIR_ATTR_VOLUME_ID)) continue;
                if (memcmp(e, sfn, 11) == 0) {
                    if (info) parse_dir_entry(e, info);
                    if (entry_sector) *entry_sector = sector;
                    if (entry_offset) *entry_offset = off;
                    return SUCCESS;
                }
            }
        }
        return ENABLE_NOT_FOUND;
    }

    c = (dir_cluster == 0) ? s_fat.info.bpb.root_cluster : dir_cluster;
    while (c >= 2) {
        for (i = 0; i < s_fat.info.bpb.sectors_per_cluster; i++) {
            sector = cluster_to_lba(c) + i;
            ret = disk_read(sector, s_fat.sector, 1);
            if (ret != SUCCESS) {
                return ret;
            }
            for (off = 0; off < FAT32_SECTOR_SIZE; off += 32) {
                uint8_t *e = &s_fat.sector[off];
                if (find_free && (e[0] == 0x00 || e[0] == 0xE5)) {
                    if (entry_sector) *entry_sector = sector;
                    if (entry_offset) *entry_offset = off;
                    return SUCCESS;
                }
                if (e[0] == 0x00) break;
                if (e[0] == 0xE5 || e[11] == DIR_ATTR_LONG_NAME || (e[11] & DIR_ATTR_VOLUME_ID)) continue;
                if (memcmp(e, sfn, 11) == 0) {
                    if (info) parse_dir_entry(e, info);
                    if (entry_sector) *entry_sector = sector;
                    if (entry_offset) *entry_offset = off;
                    return SUCCESS;
                }
            }
        }
        ret = read_fat_entry(c, &next);
        if (ret != SUCCESS || is_eoc(next)) {
            break;
        }
        c = next;
    }

    return ENABLE_NOT_FOUND;
}

BG_ERR FAT32_Init(void)
{
    const FAT32_DiskIO_t *dio;
    uint32_t p_lba;
    uint32_t total_sec;
    uint32_t root_secs;
    uint32_t data_secs;
    uint8_t *b;
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
    uint8_t part_type = 0;
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    BG_ERR ret;

    if (s_fat.mounted) {
        return SUCCESS;
    }

    FAT32_DiskIO_Register(FAT32_DISK_SDCARD, &fat32_diskio_sdcard);
    FAT32_DiskIO_Select(FAT32_DISK_SDCARD);
    dio = FAT32_DiskIO_GetCurrent();
    if (!dio || dio->init() != SUCCESS) {
        return ENABLE_DEVICE_NOT_READY;
    }

    ret = disk_read(0, s_fat.sector, 1);
    if (ret != SUCCESS) {
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
        FAT32_DBG("disk_read(0) failed: %d\n", ret);
        return ret;
    }

    FAT32_DBG("LBA0 sig=%02X %02X first=%02X\n", s_fat.sector[510], s_fat.sector[511], s_fat.sector[0]);

    p_lba = 0;
    if (s_fat.sector[510] == 0x55 && s_fat.sector[511] == 0xAA &&
        s_fat.sector[0] != 0xEB && s_fat.sector[0] != 0xE9) {
        uint8_t i;
        uint32_t fallback_lba = 0;
        uint8_t fallback_type = 0;

        for (i = 0; i < 4u; i++) {
            uint16_t off = (uint16_t)(0x1BEu + i * 16u);
            uint8_t type = s_fat.sector[off + 4u];
            uint32_t lba = rd32(&s_fat.sector[off + 8u]);
            uint32_t nsec = rd32(&s_fat.sector[off + 12u]);

            FAT32_DBG("MBR part%u: type=0x%02X lba=%lu secs=%lu\n",
                      (unsigned)i, (unsigned)type,
                      (unsigned long)lba, (unsigned long)nsec);

            if (fallback_lba == 0u && type != 0u && lba != 0u && nsec != 0u) {
                fallback_lba = lba;
                fallback_type = type;
            }
            if (is_fat_partition_type(type) && lba != 0u && nsec != 0u) {
                p_lba = lba;
                part_type = type;
                break;
            }
        }

        if (p_lba == 0u && fallback_lba != 0u) {
            p_lba = fallback_lba;
            part_type = fallback_type;
            FAT32_LOGE("No FAT partition type found, trying part type=0x%02X LBA=%lu\n",
                       (unsigned)part_type, (unsigned long)p_lba);
        }

        FAT32_DBG("MBR detected, partition type=0x%02X LBA=%lu\n",
                  (unsigned)part_type, (unsigned long)p_lba);
        if (p_lba != 0) {
            ret = disk_read(p_lba, s_fat.sector, 1);
            if (ret != SUCCESS) {
                FAT32_DBG("disk_read(%lu) failed: %d\n", (unsigned long)p_lba, ret);
                return ret;
            }
            FAT32_DBG("LBA%lu sig=%02X%02X\n",
                       (unsigned long)p_lba, s_fat.sector[510], s_fat.sector[511]);
=======
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
        return ret;
    }

    p_lba = 0;
    if (s_fat.sector[510] == 0x55 && s_fat.sector[511] == 0xAA &&
        s_fat.sector[0] != 0xEB && s_fat.sector[0] != 0xE9) {
        p_lba = rd32(&s_fat.sector[0x1BE + 8]);
        if (p_lba != 0) {
            ret = disk_read(p_lba, s_fat.sector, 1);
            if (ret != SUCCESS) {
                return ret;
            }
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
        }
    }

    b = s_fat.sector;
<<<<<<< Updated upstream
    if (b[510] != 0x55 || b[511] != 0xAA) {
=======
<<<<<<< HEAD
    if (b[510] != 0x55 || b[511] != 0xAA) {
=======
<<<<<<< HEAD
    if (!is_fat_boot_sector(b)) {
        static const uint32_t common_offsets[] = {0x2000u};
        uint8_t i;

        for (i = 0; i < (uint8_t)(sizeof(common_offsets) / sizeof(common_offsets[0])); i++) {
            ret = disk_read(common_offsets[i], s_fat.sector, 1);
            if (ret == SUCCESS && is_fat_boot_sector(s_fat.sector)) {
                p_lba = common_offsets[i];
                part_type = 0;
                FAT32_LOGW("No MBR/BPB at LBA0, using FAT boot sector at LBA=%lu\n",
                           (unsigned long)p_lba);
                b = s_fat.sector;
                break;
            }
        }
    }

    if (b[510] != 0x55 || b[511] != 0xAA) {
        FAT32_LOGE("No valid FAT boot signature: LBA=%lu sig=%02X %02X first=%02X part=0x%02X\n",
                   (unsigned long)p_lba, b[510], b[511], b[0], part_type);
=======
    if (b[510] != 0x55 || b[511] != 0xAA) {
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
        return ENABLE_INVALID_INPUT;
    }

    memset(&s_fat.info, 0, sizeof(s_fat.info));
    s_fat.part_lba = p_lba;
    s_fat.info.bpb.bytes_per_sector = rd16(&b[0x0B]);
    s_fat.info.bpb.sectors_per_cluster = b[0x0D];
    s_fat.info.bpb.reserved_sectors = rd16(&b[0x0E]);
    s_fat.info.bpb.num_fats = b[0x10];
    s_fat.info.bpb.root_entries = rd16(&b[0x11]);
    s_fat.info.bpb.total_sectors_16 = rd16(&b[0x13]);
    s_fat.info.bpb.media_type = b[0x15];
    s_fat.info.bpb.sectors_per_fat_16 = rd16(&b[0x16]);
    s_fat.info.bpb.hidden_sectors = rd32(&b[0x1C]);
    s_fat.info.bpb.total_sectors_32 = rd32(&b[0x20]);
    s_fat.info.bpb.sectors_per_fat_32 = rd32(&b[0x24]);
    s_fat.info.bpb.root_cluster = rd32(&b[0x2C]);

    if (s_fat.info.bpb.bytes_per_sector != FAT32_SECTOR_SIZE ||
        s_fat.info.bpb.sectors_per_cluster == 0 ||
        s_fat.info.bpb.num_fats == 0) {
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
        char fs16[9];
        char fs32[9];
        copy_fs_type(b, 0x36, fs16);
        copy_fs_type(b, 0x52, fs32);
        FAT32_LOGE("Invalid BPB: LBA=%lu part=0x%02X bps=%u spc=%u nf=%u fs16='%s' fs32='%s'\n",
                   (unsigned long)p_lba, (unsigned)part_type,
                   s_fat.info.bpb.bytes_per_sector,
                   s_fat.info.bpb.sectors_per_cluster,
                   s_fat.info.bpb.num_fats,
                   fs16, fs32);
        if (part_type == 0x07u) {
            FAT32_LOGE("Partition type 0x07 is usually exFAT/NTFS, not supported by this FAT writer\n");
        }
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
        return ENABLE_INVALID_INPUT;
    }

    total_sec = s_fat.info.bpb.total_sectors_16 ?
                s_fat.info.bpb.total_sectors_16 : s_fat.info.bpb.total_sectors_32;
    s_fat.info.is_fat16 = (s_fat.info.bpb.sectors_per_fat_16 != 0);
    s_fat.fat_size = s_fat.info.is_fat16 ?
                     s_fat.info.bpb.sectors_per_fat_16 : s_fat.info.bpb.sectors_per_fat_32;
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
    if (total_sec == 0 || s_fat.fat_size == 0) {
        FAT32_LOGE("Invalid FAT size: LBA=%lu part=0x%02X total=%lu fat_size=%lu root_cluster=%lu\n",
                   (unsigned long)p_lba, (unsigned)part_type,
                   (unsigned long)total_sec, (unsigned long)s_fat.fat_size,
                   (unsigned long)s_fat.info.bpb.root_cluster);
        return ENABLE_INVALID_INPUT;
    }
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    root_secs = ((uint32_t)s_fat.info.bpb.root_entries * 32u + FAT32_SECTOR_SIZE - 1u) / FAT32_SECTOR_SIZE;
    s_fat.info.fat_start_sector = p_lba + s_fat.info.bpb.reserved_sectors;
    s_fat.info.root_dir_start_lba = s_fat.info.fat_start_sector +
                                    (uint32_t)s_fat.info.bpb.num_fats * s_fat.fat_size;
    s_fat.info.root_dir_num_sectors = root_secs;
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
    if (s_fat.info.is_fat16) {
        s_fat.info.data_start_sector = s_fat.info.root_dir_start_lba + root_secs;
    } else {
        s_fat.info.data_start_sector = s_fat.info.root_dir_start_lba;
    }
    data_secs = total_sec - (s_fat.info.data_start_sector - p_lba);
    s_fat.info.total_clusters = data_secs / s_fat.info.bpb.sectors_per_cluster;
    if (!s_fat.info.is_fat16 &&
        (s_fat.info.bpb.root_cluster < 2 ||
         s_fat.info.bpb.root_cluster >= s_fat.info.total_clusters + 2u)) {
        FAT32_LOGW("Invalid FAT32 root_cluster=%lu total_clusters=%lu, using 2\n",
                   (unsigned long)s_fat.info.bpb.root_cluster,
                   (unsigned long)s_fat.info.total_clusters);
        s_fat.info.bpb.root_cluster = 2;
    }
    s_fat.info.root_dir_sector = s_fat.info.is_fat16 ?
                                 s_fat.info.root_dir_start_lba :
                                 cluster_to_lba(s_fat.info.bpb.root_cluster);
=======
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    s_fat.info.data_start_sector = s_fat.info.root_dir_start_lba + root_secs;
    data_secs = total_sec - (s_fat.info.data_start_sector - p_lba);
    s_fat.info.total_clusters = data_secs / s_fat.info.bpb.sectors_per_cluster;
    s_fat.info.root_dir_sector = s_fat.info.is_fat16 ?
                                 s_fat.info.root_dir_start_lba :
                                 cluster_to_lba(s_fat.info.bpb.root_cluster);
    if (!s_fat.info.is_fat16 && s_fat.info.bpb.root_cluster < 2) {
        s_fat.info.bpb.root_cluster = 2;
    }
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes

    s_fat.mounted = true;
    memset(s_open_used, 0, sizeof(s_open_used));
    return SUCCESS;
}

void FAT32_DeInit(void)
{
    s_fat.mounted = false;
    memset(s_open_used, 0, sizeof(s_open_used));
}

BG_ERR FAT32_GetFSInfo(FAT32_FSInfo_t *info)
{
    if (!s_fat.mounted || !info) {
        return ENABLE_INVALID_INPUT;
    }
    *info = s_fat.info;
    return SUCCESS;
}

uint32_t FAT32_GetRootCluster(void)
{
    if (!s_fat.mounted) {
        return 0;
    }
    return s_fat.info.is_fat16 ? 0 : s_fat.info.bpb.root_cluster;
}

bool FAT32_IsCardReady(void)
{
    const FAT32_DiskIO_t *dio = FAT32_DiskIO_GetCurrent();
    return (dio && dio->is_ready && dio->is_ready());
}

BG_ERR FAT32_FindEntryInDir(uint32_t dir_cluster, const char *name, FAT32_FileInfo_t *info)
{
    if (!s_fat.mounted || !name || !info) {
        return ENABLE_INVALID_INPUT;
    }
    return find_dir_entry(dir_cluster, name, info, 0, 0, 0);
}

BG_ERR FAT32_ListDirByCluster(uint32_t dir_cluster, FAT32_ListCallback_t cb, void *user)
{
    uint32_t c;
    uint32_t next;
    uint32_t i;
    uint32_t sector;
    uint16_t off;
    FAT32_FileInfo_t info;
    BG_ERR ret;

    if (!s_fat.mounted || !cb) {
        return ENABLE_INVALID_INPUT;
    }

    if (s_fat.info.is_fat16 && dir_cluster == 0) {
        for (i = 0; i < s_fat.info.root_dir_num_sectors; i++) {
            ret = disk_read(s_fat.info.root_dir_start_lba + i, s_fat.sector, 1);
            if (ret != SUCCESS) return ret;
            for (off = 0; off < FAT32_SECTOR_SIZE; off += 32) {
                uint8_t *e = &s_fat.sector[off];
                if (e[0] == 0x00) return SUCCESS;
                if (e[0] == 0xE5 || e[11] == DIR_ATTR_LONG_NAME || (e[11] & DIR_ATTR_VOLUME_ID)) continue;
                parse_dir_entry(e, &info);
                if (cb(&info, user) != 0) return SUCCESS;
            }
        }
        return SUCCESS;
    }

    c = (dir_cluster == 0) ? s_fat.info.bpb.root_cluster : dir_cluster;
    while (c >= 2) {
        for (i = 0; i < s_fat.info.bpb.sectors_per_cluster; i++) {
            sector = cluster_to_lba(c) + i;
            ret = disk_read(sector, s_fat.sector, 1);
            if (ret != SUCCESS) return ret;
            for (off = 0; off < FAT32_SECTOR_SIZE; off += 32) {
                uint8_t *e = &s_fat.sector[off];
                if (e[0] == 0x00) return SUCCESS;
                if (e[0] == 0xE5 || e[11] == DIR_ATTR_LONG_NAME || (e[11] & DIR_ATTR_VOLUME_ID)) continue;
                parse_dir_entry(e, &info);
                if (cb(&info, user) != 0) return SUCCESS;
            }
        }
        ret = read_fat_entry(c, &next);
        if (ret != SUCCESS || is_eoc(next)) break;
        c = next;
    }
    return SUCCESS;
}

BG_ERR FAT32_ListDir(const char *path, FAT32_ListCallback_t cb, void *user)
{
    (void)path;
    return FAT32_ListDirByCluster(FAT32_GetRootCluster(), cb, user);
}

BG_ERR FAT32_OpenFileInDir(uint32_t dir_cluster, const char *filename, FAT32_FileHandle_t *handle)
{
    BG_ERR ret;
    if (!handle) return ENABLE_INVALID_INPUT;
    memset(handle, 0, sizeof(FAT32_FileHandle_t));
    ret = find_dir_entry(dir_cluster, filename, &handle->info,
                         &handle->dir_sector, &handle->dir_offset, 0);
    if (ret != SUCCESS) return ret;
    if (handle->info.attr & DIR_ATTR_DIRECTORY) return ENABLE_INVALID_INPUT;
    handle->current_cluster = handle->info.start_cluster;
    handle->mode = FAT_OPEN_READ;
    return SUCCESS;
}

BG_ERR FAT32_OpenFile(const char *filename, FAT32_FileHandle_t *handle)
{
    return FAT32_OpenFileInDir(FAT32_GetRootCluster(), filename, handle);
}

int32_t FAT32_ReadFile(FAT32_FileHandle_t *handle, void *buffer, uint32_t size)
{
    uint8_t *out = (uint8_t*)buffer;
    uint32_t read = 0;
    uint32_t cluster_size;
    uint32_t sector_in_cluster;
    uint32_t offset_in_sector;
    uint32_t to_copy;
    uint32_t next;
    BG_ERR ret;

    if (!handle || !buffer || !(handle->mode & FAT_OPEN_READ)) return -1;
    if (handle->position >= handle->info.size) return 0;
    if (handle->position + size > handle->info.size) size = handle->info.size - handle->position;

    cluster_size = (uint32_t)s_fat.info.bpb.sectors_per_cluster * FAT32_SECTOR_SIZE;
    while (read < size && handle->current_cluster >= 2) {
        sector_in_cluster = (handle->position % cluster_size) / FAT32_SECTOR_SIZE;
        offset_in_sector = handle->position % FAT32_SECTOR_SIZE;
        ret = disk_read(cluster_to_lba(handle->current_cluster) + sector_in_cluster, s_fat.sector, 1);
        if (ret != SUCCESS) return -1;
        to_copy = FAT32_SECTOR_SIZE - offset_in_sector;
        if (to_copy > size - read) to_copy = size - read;
        memcpy(out + read, &s_fat.sector[offset_in_sector], to_copy);
        read += to_copy;
        handle->position += to_copy;
        if ((handle->position % cluster_size) == 0 && read < size) {
            ret = read_fat_entry(handle->current_cluster, &next);
            if (ret != SUCCESS || is_eoc(next)) break;
            handle->current_cluster = next;
        }
    }
    return (int32_t)read;
}

void FAT32_CloseFile(FAT32_FileHandle_t *handle)
{
    if (handle) memset(handle, 0, sizeof(FAT32_FileHandle_t));
}

static BG_ERR update_dir_entry(uint32_t sector, uint16_t off, const char sfn[11],
                               uint8_t attr, uint32_t cluster, uint32_t size)
{
    BG_ERR ret;
<<<<<<< Updated upstream
    ret = disk_read(sector, s_fat.sector, 1);
    if (ret != SUCCESS) return ret;
=======
<<<<<<< HEAD
    ret = disk_read(sector, s_fat.sector, 1);
    if (ret != SUCCESS) return ret;
=======
<<<<<<< HEAD
#if FAT32_DEBUG
    FAT32_DBG("update_dir_entry: LBA=%lu off=%u attr=0x%02X cluster=%lu size=%lu\n",
              (unsigned long)sector, (unsigned)off, attr,
              (unsigned long)cluster, (unsigned long)size);
#endif
    ret = disk_read(sector, s_fat.sector, 1);
    if (ret != SUCCESS) {
        FAT32_LOGE("update_dir_entry: disk_read LBA=%lu failed: %d\n",
                   (unsigned long)sector, ret);
        return ret;
    }
=======
    ret = disk_read(sector, s_fat.sector, 1);
    if (ret != SUCCESS) return ret;
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    memcpy(&s_fat.sector[off], sfn, 11);
    s_fat.sector[off + 11] = attr;
    memset(&s_fat.sector[off + 12], 0, 20);
    wr16(&s_fat.sector[off + 20], (uint16_t)(cluster >> 16));
    wr16(&s_fat.sector[off + 26], (uint16_t)cluster);
    wr32(&s_fat.sector[off + 28], size);
<<<<<<< Updated upstream
    return disk_write(sector, s_fat.sector, 1);
=======
<<<<<<< HEAD
    return disk_write(sector, s_fat.sector, 1);
=======
<<<<<<< HEAD
    ret = disk_write(sector, s_fat.sector, 1);
    if (ret != SUCCESS) {
        FAT32_LOGE("update_dir_entry: disk_write LBA=%lu failed: %d\n",
                   (unsigned long)sector, ret);
    }
    return ret;
=======
    return disk_write(sector, s_fat.sector, 1);
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}

int32_t FAT32_WriteFile(uint32_t dir_cluster, const char *filename, const void *data, uint32_t size)
{
    char sfn[11];
    uint32_t entry_sector;
    uint16_t entry_off;
    uint32_t first = 0;
    uint32_t prev = 0;
    uint32_t cur = 0;
    uint32_t written = 0;
    uint32_t cluster_size;
    BG_ERR ret;

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
    if (!s_fat.mounted || !filename || (!data && size)) {
        FAT32_LOGE("WriteFile: invalid args (mounted=%d filename=%p data=%p size=%lu)\n",
                   s_fat.mounted, filename, data, (unsigned long)size);
        return -1;
    }

    FAT32_DBG("WriteFile: '%s' size=%lu dir_cluster=%lu\n",
              filename, (unsigned long)size, (unsigned long)dir_cluster);

    ret = find_dir_entry(dir_cluster, filename, 0, &entry_sector, &entry_off, 1);
    if (ret != SUCCESS) {
        FAT32_LOGE("WriteFile: find_dir_entry(free) failed: %d\n", ret);
        return -1;
    }
    make_short_name(filename, sfn);

    FAT32_DBG("WriteFile: entry_sector=%lu entry_off=%u sfn=%.11s\n",
              (unsigned long)entry_sector, (unsigned)entry_off, sfn);

    cluster_size = (uint32_t)s_fat.info.bpb.sectors_per_cluster * FAT32_SECTOR_SIZE;
    while (written < size || first == 0) {
        ret = alloc_cluster(&cur);
        if (ret != SUCCESS) {
            FAT32_LOGE("WriteFile: alloc_cluster failed at written=%lu\n", (unsigned long)written);
            return -1;
        }
        if (first == 0) first = cur;
        if (prev != 0) {
            ret = write_fat_entry(prev, cur);
            if (ret != SUCCESS) {
                FAT32_LOGE("WriteFile: write_fat(prev=%lu,cur=%lu) failed\n",
                           (unsigned long)prev, (unsigned long)cur);
                return -1;
            }
=======
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    if (!s_fat.mounted || !filename || (!data && size)) return -1;
    ret = find_dir_entry(dir_cluster, filename, 0, &entry_sector, &entry_off, 1);
    if (ret != SUCCESS) return -1;
    make_short_name(filename, sfn);

    cluster_size = (uint32_t)s_fat.info.bpb.sectors_per_cluster * FAT32_SECTOR_SIZE;
    while (written < size || first == 0) {
        ret = alloc_cluster(&cur);
        if (ret != SUCCESS) return -1;
        if (first == 0) first = cur;
        if (prev != 0) {
            ret = write_fat_entry(prev, cur);
            if (ret != SUCCESS) return -1;
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
        }
        memset(s_fat.sector, 0, FAT32_SECTOR_SIZE);
        {
            uint32_t remain = size - written;
            uint32_t chunk = (remain > cluster_size) ? cluster_size : remain;
            uint32_t sec;
            uint32_t sec_count = s_fat.info.bpb.sectors_per_cluster;
            for (sec = 0; sec < sec_count; sec++) {
                uint32_t byte_off = sec * FAT32_SECTOR_SIZE;
                uint32_t n = 0;
                memset(s_fat.sector, 0, FAT32_SECTOR_SIZE);
                if (byte_off < chunk) {
                    n = chunk - byte_off;
                    if (n > FAT32_SECTOR_SIZE) n = FAT32_SECTOR_SIZE;
                    memcpy(s_fat.sector, ((const uint8_t*)data) + written + byte_off, n);
                }
                ret = disk_write(cluster_to_lba(cur) + sec, s_fat.sector, 1);
<<<<<<< Updated upstream
                if (ret != SUCCESS) return -1;
=======
<<<<<<< HEAD
                if (ret != SUCCESS) return -1;
=======
<<<<<<< HEAD
                if (ret != SUCCESS) {
                    FAT32_LOGE("WriteFile: disk_write LBA=%lu failed: %d\n",
                               (unsigned long)(cluster_to_lba(cur) + sec), ret);
                    return -1;
                }
=======
                if (ret != SUCCESS) return -1;
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
            }
            written += chunk;
        }
        prev = cur;
        if (size == 0) break;
    }
<<<<<<< Updated upstream
    write_fat_entry(prev, s_fat.info.is_fat16 ? 0xFFFFu : 0x0FFFFFFFu);
    ret = update_dir_entry(entry_sector, entry_off, sfn, DIR_ATTR_ARCHIVE, first, size);
    return (ret == SUCCESS) ? (int32_t)size : -1;
=======
<<<<<<< HEAD
    write_fat_entry(prev, s_fat.info.is_fat16 ? 0xFFFFu : 0x0FFFFFFFu);
    ret = update_dir_entry(entry_sector, entry_off, sfn, DIR_ATTR_ARCHIVE, first, size);
    return (ret == SUCCESS) ? (int32_t)size : -1;
=======
<<<<<<< HEAD
    ret = write_fat_entry(prev, s_fat.info.is_fat16 ? 0xFFFFu : 0x0FFFFFFFu);
    if (ret != SUCCESS) {
        FAT32_LOGE("WriteFile: write_fat EOC for cluster %lu failed\n", (unsigned long)prev);
        return -1;
    }

    FAT32_DBG("WriteFile: updating dir entry sector=%lu off=%u cluster=%lu size=%lu\n",
              (unsigned long)entry_sector, (unsigned)entry_off,
              (unsigned long)first, (unsigned long)size);
    ret = update_dir_entry(entry_sector, entry_off, sfn, DIR_ATTR_ARCHIVE, first, size);
    if (ret != SUCCESS) {
        FAT32_LOGE("WriteFile: update_dir_entry failed: %d\n", ret);
        return -1;
    }

    FAT32_DBG("WriteFile: done, first_cluster=%lu total_written=%lu\n",
              (unsigned long)first, (unsigned long)written);
    return (int32_t)written;
=======
    write_fat_entry(prev, s_fat.info.is_fat16 ? 0xFFFFu : 0x0FFFFFFFu);
    ret = update_dir_entry(entry_sector, entry_off, sfn, DIR_ATTR_ARCHIVE, first, size);
    return (ret == SUCCESS) ? (int32_t)size : -1;
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}

int32_t FAT32_AppendFile(uint32_t dir_cluster, const char *filename, const void *data, uint32_t size)
{
<<<<<<< Updated upstream
    return FAT32_WriteFile(dir_cluster, filename, data, size);
=======
<<<<<<< HEAD
    return FAT32_WriteFile(dir_cluster, filename, data, size);
=======
<<<<<<< HEAD
    FAT32_FileInfo_t finfo;
    uint32_t first_cluster, last_cluster, file_size;
    uint32_t cluster_size, offset_in_cluster;
    uint32_t written = 0;
    uint32_t cur, prev;
    BG_ERR ret;

    if (!s_fat.mounted || !filename || (!data && size)) {
        FAT32_LOGE("AppendFile: invalid args\n");
        return -1;
    }

    FAT32_DBG("AppendFile: '%s' size=%lu dir_cluster=%lu\n",
              filename, (unsigned long)size, (unsigned long)dir_cluster);

    /* 查找文件目录项 */
    ret = FAT32_FindEntryInDir(dir_cluster, filename, &finfo);
    if (ret != SUCCESS) {
        /* 文件不存在，直接用 WriteFile 创建 */
        FAT32_LOGW("AppendFile: '%s' not found, creating via WriteFile\n", filename);
        return FAT32_WriteFile(dir_cluster, filename, data, size);
    }

    first_cluster = finfo.start_cluster;
    file_size     = finfo.size;
    cluster_size  = (uint32_t)s_fat.info.bpb.sectors_per_cluster * FAT32_SECTOR_SIZE;

    FAT32_DBG("AppendFile: found file cluster=%lu size=%lu cluster_size=%lu\n",
              (unsigned long)first_cluster, (unsigned long)file_size,
              (unsigned long)cluster_size);

    if (first_cluster == 0 || file_size == 0) {
        /* 空文件，直接覆盖写 */
        FAT32_LOGW("AppendFile: empty file, using WriteFile\n");
        return FAT32_WriteFile(dir_cluster, filename, data, size);
    }

    /* 找到文件最后一个簇 */
    last_cluster = first_cluster;
    {
        uint32_t chain_len = 0;
        while (1) {
            uint32_t next;
            read_fat_entry(last_cluster, &next);
            chain_len++;
            if (next >= (s_fat.info.is_fat16 ? 0xFFF8u : 0x0FFFFFF8u) || next == 0) break;
            last_cluster = next;
        }
        FAT32_DBG("AppendFile: chain_len=%lu last_cluster=%lu\n",
                  (unsigned long)chain_len, (unsigned long)last_cluster);
    }

    /* 计算最后一簇内的偏移 */
    offset_in_cluster = file_size % cluster_size;
    if (offset_in_cluster == 0 && file_size > 0) {
        offset_in_cluster = cluster_size;  /* 最后一簇刚好写满 */
    }

    FAT32_DBG("AppendFile: offset_in_last_cluster=%lu\n", (unsigned long)offset_in_cluster);

    /* 如果最后一簇有剩余空间，先填充 */
    if (offset_in_cluster < cluster_size) {
        uint32_t remain_in_cluster = cluster_size - offset_in_cluster;
        uint32_t chunk = (size < remain_in_cluster) ? size : remain_in_cluster;
        uint32_t sector_offset = offset_in_cluster / FAT32_SECTOR_SIZE;
        uint32_t byte_offset   = offset_in_cluster % FAT32_SECTOR_SIZE;
        uint32_t sec;

        FAT32_DBG("AppendFile: fill last cluster, chunk=%lu sec_off=%lu byte_off=%lu\n",
                  (unsigned long)chunk, (unsigned long)sector_offset,
                  (unsigned long)byte_offset);

        for (sec = sector_offset; sec < s_fat.info.bpb.sectors_per_cluster && chunk > 0; sec++) {
            uint32_t lba = cluster_to_lba(last_cluster) + sec;
            uint32_t n;

            ret = disk_read(lba, s_fat.sector, 1);
            if (ret != SUCCESS) {
                FAT32_LOGE("AppendFile: disk_read LBA=%lu failed: %d\n",
                           (unsigned long)lba, ret);
                return -1;
            }

            n = FAT32_SECTOR_SIZE - byte_offset;
            if (n > chunk) n = chunk;
            memcpy(&s_fat.sector[byte_offset], ((const uint8_t*)data) + written, n);
            ret = disk_write(lba, s_fat.sector, 1);
            if (ret != SUCCESS) {
                FAT32_LOGE("AppendFile: disk_write LBA=%lu failed: %d\n",
                           (unsigned long)lba, ret);
                return -1;
            }

            written += n;
            chunk   -= n;
            byte_offset = 0;
        }
    }

    /* 分配新簇写入剩余数据 */
    prev = last_cluster;
    while (written < size) {
        uint32_t chunk = size - written;
        uint32_t sec;

        ret = alloc_cluster(&cur);
        if (ret != SUCCESS) {
            FAT32_LOGE("AppendFile: alloc_cluster failed at written=%lu\n", (unsigned long)written);
            return -1;
        }
        ret = write_fat_entry(prev, cur);
        if (ret != SUCCESS) {
            FAT32_LOGE("AppendFile: write_fat(prev=%lu,cur=%lu) failed\n",
                       (unsigned long)prev, (unsigned long)cur);
            return -1;
        }

        if (chunk > cluster_size) chunk = cluster_size;

        for (sec = 0; sec < s_fat.info.bpb.sectors_per_cluster; sec++) {
            uint32_t byte_off = sec * FAT32_SECTOR_SIZE;
            uint32_t n = 0;
            memset(s_fat.sector, 0, FAT32_SECTOR_SIZE);
            if (byte_off < chunk) {
                n = chunk - byte_off;
                if (n > FAT32_SECTOR_SIZE) n = FAT32_SECTOR_SIZE;
                memcpy(s_fat.sector, ((const uint8_t*)data) + written + byte_off, n);
            }
            ret = disk_write(cluster_to_lba(cur) + sec, s_fat.sector, 1);
            if (ret != SUCCESS) {
                FAT32_LOGE("AppendFile: disk_write LBA=%lu failed: %d\n",
                           (unsigned long)(cluster_to_lba(cur) + sec), ret);
                return -1;
            }
        }
        written += chunk;
        prev = cur;
    }

    /* 标记最后一簇为 EOC */
    ret = write_fat_entry(prev, s_fat.info.is_fat16 ? 0xFFFFu : 0x0FFFFFFFu);
    if (ret != SUCCESS) {
        FAT32_LOGE("AppendFile: write_fat EOC for cluster %lu failed\n", (unsigned long)prev);
        return -1;
    }

    /* 更新目录项中的文件大小 */
    FAT32_DBG("AppendFile: updating dir entry, old_size=%lu + %lu = %lu\n",
              (unsigned long)file_size, (unsigned long)size,
              (unsigned long)(file_size + size));
    {
        uint32_t entry_sector;
        uint16_t entry_off;
        ret = find_dir_entry(dir_cluster, filename, 0, &entry_sector, &entry_off, 0);
        if (ret == SUCCESS) {
            ret = disk_read(entry_sector, s_fat.sector, 1);
            if (ret == SUCCESS) {
                wr32(&s_fat.sector[entry_off + 28], file_size + size);
                ret = disk_write(entry_sector, s_fat.sector, 1);
                if (ret != SUCCESS) {
                    FAT32_LOGE("AppendFile: dir entry write failed: %d\n", ret);
                }
            } else {
                FAT32_LOGE("AppendFile: dir entry read failed: %d\n", ret);
            }
        } else {
            FAT32_LOGE("AppendFile: find_dir_entry for update failed: %d\n", ret);
        }
    }

    FAT32_DBG("AppendFile: done, written=%lu new_size=%lu\n",
              (unsigned long)written, (unsigned long)(file_size + size));
    return (int32_t)size;
=======
    return FAT32_WriteFile(dir_cluster, filename, data, size);
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}

BG_ERR FAT32_DeleteFile(uint32_t dir_cluster, const char *filename)
{
    uint32_t sector;
    uint16_t off;
    BG_ERR ret;
    ret = find_dir_entry(dir_cluster, filename, 0, &sector, &off, 0);
    if (ret != SUCCESS) return ret;
    ret = disk_read(sector, s_fat.sector, 1);
    if (ret != SUCCESS) return ret;
    s_fat.sector[off] = 0xE5;
    return disk_write(sector, s_fat.sector, 1);
}

BG_ERR FAT32_MkDir(uint32_t dir_cluster, const char *dirname)
{
    (void)dir_cluster; (void)dirname;
    return ENABLE_INVALID_INPUT;
}

BG_ERR FAT32_RmDir(uint32_t dir_cluster, const char *dirname)
{
    (void)dir_cluster; (void)dirname;
    return ENABLE_INVALID_INPUT;
}

BG_ERR FAT32_Rename(uint32_t dir_cluster, const char *oldname, const char *newname)
{
    (void)dir_cluster; (void)oldname; (void)newname;
    return ENABLE_INVALID_INPUT;
}

int fat_open(const char *path, int flags)
{
    uint8_t i;
    BG_ERR ret;
    if (!s_fat.mounted && FAT32_Init() != SUCCESS) return -1;
    for (i = 0; i < FAT_MAX_OPEN_FILES; i++) {
        if (!s_open_used[i]) {
            memset(&s_open_files[i], 0, sizeof(FAT32_FileHandle_t));
            if (flags & FAT_OPEN_WRITE) {
                s_open_files[i].mode = FAT_OPEN_WRITE;
                strncpy(s_open_files[i].info.name, path, sizeof(s_open_files[i].info.name) - 1);
                s_open_used[i] = 1;
                return (int)i;
            }
            ret = FAT32_OpenFile(path, &s_open_files[i]);
            if (ret != SUCCESS) return -1;
            s_open_used[i] = 1;
            return (int)i;
        }
    }
    return -1;
}

int fat_close(int fd)
{
    if (fd < 0 || fd >= FAT_MAX_OPEN_FILES || !s_open_used[fd]) return -1;
    s_open_used[fd] = 0;
    FAT32_CloseFile(&s_open_files[fd]);
    return 0;
}

int fat_read(int fd, void *buf, uint32_t len)
{
    if (fd < 0 || fd >= FAT_MAX_OPEN_FILES || !s_open_used[fd]) return -1;
    return FAT32_ReadFile(&s_open_files[fd], buf, len);
}

int fat_write(int fd, const void *buf, uint32_t len)
{
    if (fd < 0 || fd >= FAT_MAX_OPEN_FILES || !s_open_used[fd]) return -1;
    return FAT32_WriteFile(FAT32_GetRootCluster(), s_open_files[fd].info.name, buf, len);
}

#endif /* FAT32_EN */
