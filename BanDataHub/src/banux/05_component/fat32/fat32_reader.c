﻿﻿﻿﻿﻿﻿/**
 * @file fat32_reader.c
 * @brief BanDataHub FAT16/FAT32 file operations.
 */
#include "product_def.h"

#if FAT32_EN

#include "fat32_reader.h"
#include <string.h>

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

    for (fat = 0; fat < s_fat.info.bpb.num_fats; fat++) {
        sector = s_fat.info.fat_start_sector + fat * s_fat.fat_size + (offset / FAT32_SECTOR_SIZE);
        pos = offset % FAT32_SECTOR_SIZE;
        ret = disk_read(sector, s_fat.sector, 1);
        if (ret != SUCCESS) {
            return ret;
        }
        if (s_fat.info.is_fat16) {
            wr16(&s_fat.sector[pos], (uint16_t)value);
        } else {
            wr32(&s_fat.sector[pos], value & 0x0FFFFFFFu);
        }
        ret = disk_write(sector, s_fat.sector, 1);
        if (ret != SUCCESS) {
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
            return ret;
        }
        if (v == FAT_FREE) {
            ret = write_fat_entry(c, s_fat.info.is_fat16 ? 0xFFFFu : 0x0FFFFFFFu);
            if (ret != SUCCESS) {
                return ret;
            }
            memset(s_fat.sector, 0, FAT32_SECTOR_SIZE);
            ret = disk_write(cluster_to_lba(c), s_fat.sector, s_fat.info.bpb.sectors_per_cluster);
            if (ret != SUCCESS) {
                return ret;
            }
            *cluster = c;
            return SUCCESS;
        }
    }
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
        }
    }

    b = s_fat.sector;
    if (b[510] != 0x55 || b[511] != 0xAA) {
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
        return ENABLE_INVALID_INPUT;
    }

    total_sec = s_fat.info.bpb.total_sectors_16 ?
                s_fat.info.bpb.total_sectors_16 : s_fat.info.bpb.total_sectors_32;
    s_fat.info.is_fat16 = (s_fat.info.bpb.sectors_per_fat_16 != 0);
    s_fat.fat_size = s_fat.info.is_fat16 ?
                     s_fat.info.bpb.sectors_per_fat_16 : s_fat.info.bpb.sectors_per_fat_32;
    root_secs = ((uint32_t)s_fat.info.bpb.root_entries * 32u + FAT32_SECTOR_SIZE - 1u) / FAT32_SECTOR_SIZE;
    s_fat.info.fat_start_sector = p_lba + s_fat.info.bpb.reserved_sectors;
    s_fat.info.root_dir_start_lba = s_fat.info.fat_start_sector +
                                    (uint32_t)s_fat.info.bpb.num_fats * s_fat.fat_size;
    s_fat.info.root_dir_num_sectors = root_secs;
    s_fat.info.data_start_sector = s_fat.info.root_dir_start_lba + root_secs;
    data_secs = total_sec - (s_fat.info.data_start_sector - p_lba);
    s_fat.info.total_clusters = data_secs / s_fat.info.bpb.sectors_per_cluster;
    s_fat.info.root_dir_sector = s_fat.info.is_fat16 ?
                                 s_fat.info.root_dir_start_lba :
                                 cluster_to_lba(s_fat.info.bpb.root_cluster);
    if (!s_fat.info.is_fat16 && s_fat.info.bpb.root_cluster < 2) {
        s_fat.info.bpb.root_cluster = 2;
    }

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
    ret = disk_read(sector, s_fat.sector, 1);
    if (ret != SUCCESS) return ret;
    memcpy(&s_fat.sector[off], sfn, 11);
    s_fat.sector[off + 11] = attr;
    memset(&s_fat.sector[off + 12], 0, 20);
    wr16(&s_fat.sector[off + 20], (uint16_t)(cluster >> 16));
    wr16(&s_fat.sector[off + 26], (uint16_t)cluster);
    wr32(&s_fat.sector[off + 28], size);
    return disk_write(sector, s_fat.sector, 1);
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
                if (ret != SUCCESS) return -1;
            }
            written += chunk;
        }
        prev = cur;
        if (size == 0) break;
    }
    write_fat_entry(prev, s_fat.info.is_fat16 ? 0xFFFFu : 0x0FFFFFFFu);
    ret = update_dir_entry(entry_sector, entry_off, sfn, DIR_ATTR_ARCHIVE, first, size);
    return (ret == SUCCESS) ? (int32_t)size : -1;
}

int32_t FAT32_AppendFile(uint32_t dir_cluster, const char *filename, const void *data, uint32_t size)
{
    return FAT32_WriteFile(dir_cluster, filename, data, size);
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
