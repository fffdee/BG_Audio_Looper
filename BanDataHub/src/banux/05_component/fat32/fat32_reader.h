/**
 * @file fat32_reader.h
 * @brief BanDataHub FAT16/FAT32 file operations.
 */
#ifndef __BANDATAHUB_FAT32_READER_H__
#define __BANDATAHUB_FAT32_READER_H__

#include "product_def.h"

#if FAT32_EN

#include <stdint.h>
#include <stdbool.h>
#include "fat32_diskio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FAT32_SECTOR_SIZE       512u
#define DIR_ATTR_READ_ONLY      0x01u
#define DIR_ATTR_HIDDEN         0x02u
#define DIR_ATTR_SYSTEM         0x04u
#define DIR_ATTR_VOLUME_ID      0x08u
#define DIR_ATTR_DIRECTORY      0x10u
#define DIR_ATTR_ARCHIVE        0x20u
#define DIR_ATTR_LONG_NAME      0x0Fu

typedef struct {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
} FAT32_BPB_t;

typedef struct {
    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t root_dir_sector;
    uint32_t total_clusters;
    bool     is_fat16;
    uint32_t root_dir_start_lba;
    uint32_t root_dir_num_sectors;
    FAT32_BPB_t bpb;
} FAT32_FSInfo_t;

typedef struct {
    char     name[256];
    uint32_t size;
    uint32_t start_cluster;
    uint16_t modify_time;
    uint16_t modify_date;
    uint8_t  attr;
} FAT32_FileInfo_t;

typedef struct {
    FAT32_FileInfo_t info;
    uint32_t current_cluster;
    uint32_t position;
    uint8_t  mode;
    uint32_t dir_sector;
    uint16_t dir_offset;
} FAT32_FileHandle_t;

typedef int (*FAT32_ListCallback_t)(const FAT32_FileInfo_t *info, void *user);

BG_ERR FAT32_Init(void);
void FAT32_DeInit(void);
BG_ERR FAT32_GetFSInfo(FAT32_FSInfo_t *info);
uint32_t FAT32_GetRootCluster(void);
bool FAT32_IsCardReady(void);

BG_ERR FAT32_ListDir(const char *path, FAT32_ListCallback_t cb, void *user);
BG_ERR FAT32_ListDirByCluster(uint32_t dir_cluster, FAT32_ListCallback_t cb, void *user);
BG_ERR FAT32_FindEntryInDir(uint32_t dir_cluster, const char *name, FAT32_FileInfo_t *info);

BG_ERR FAT32_OpenFile(const char *filename, FAT32_FileHandle_t *handle);
BG_ERR FAT32_OpenFileInDir(uint32_t dir_cluster, const char *filename, FAT32_FileHandle_t *handle);
int32_t FAT32_ReadFile(FAT32_FileHandle_t *handle, void *buffer, uint32_t size);
int32_t FAT32_WriteFile(uint32_t dir_cluster, const char *filename, const void *data, uint32_t size);
int32_t FAT32_AppendFile(uint32_t dir_cluster, const char *filename, const void *data, uint32_t size);
void FAT32_CloseFile(FAT32_FileHandle_t *handle);

BG_ERR FAT32_DeleteFile(uint32_t dir_cluster, const char *filename);
BG_ERR FAT32_MkDir(uint32_t dir_cluster, const char *dirname);
BG_ERR FAT32_RmDir(uint32_t dir_cluster, const char *dirname);
BG_ERR FAT32_Rename(uint32_t dir_cluster, const char *oldname, const char *newname);

/* Standard-like file API used by VFS/shell adapters. */
int fat_open(const char *path, int flags);
int fat_close(int fd);
int fat_read(int fd, void *buf, uint32_t len);
int fat_write(int fd, const void *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* FAT32_EN */

#endif /* __BANDATAHUB_FAT32_READER_H__ */
