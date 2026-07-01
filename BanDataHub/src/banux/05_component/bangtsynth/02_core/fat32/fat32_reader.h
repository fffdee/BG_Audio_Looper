/**
 * @file fat32_reader.h
 * @brief FAT32 文件系统最小读取器接口
 *
 * 专为 BanGTsynth SD卡+NAND+PSRAM 方案设计的最小 FAT32 读取器。
 * 只实现必要的读取功能，支持 SF2 文件定位和数据读取。
 *
 * 特性:
 * - MBR 分区表解析
 * - FAT32 BPB 解析
 * - 目录遍历 (支持长文件名)
 * - 文件读取 (簇链解析)
 * - 错误处理和重试机制
 * - 支持多种存储后端 (SD卡/NAND Flash)
 */

#ifndef __FAT32_READER_H__
#define __FAT32_READER_H__

#include "product_def.h"

#if FAT32_EN

#include <stdint.h>
#include <stdbool.h>
#include "err_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * FAT32 常量定义
 * ============================================ */

/** 扇区大小 (字节) */
#define FAT32_SECTOR_SIZE            512

/** MBR 分区表偏移 */
#define MBR_PARTITION_TABLE_OFFSET   0x1BE
#define MBR_PARTITION_ENTRY_SIZE     16
#define MBR_SIGNATURE_OFFSET         0x1FE
#define MBR_SIGNATURE                0xAA55

/** FAT32 BPB 偏移量 */
#define BPB_BYTES_PER_SECTOR         0x0B    /* 2 bytes */
#define BPB_SECTORS_PER_CLUSTER      0x0D    /* 1 byte */
#define BPB_RESERVED_SECTORS         0x0E    /* 2 bytes */
#define BPB_NUM_FATS                 0x10    /* 1 byte */
#define BPB_ROOT_ENTRIES             0x11    /* 2 bytes (FAT32=0) */
#define BPB_TOTAL_SECTORS_16         0x13    /* 2 bytes */
#define BPB_MEDIA_TYPE               0x15    /* 1 byte */
#define BPB_SECTORS_PER_FAT_16       0x16    /* 2 bytes */
#define BPB_SECTORS_PER_TRACK        0x18    /* 2 bytes */
#define BPB_NUM_HEADS                0x1A    /* 2 bytes */
#define BPB_HIDDEN_SECTORS           0x1C    /* 4 bytes */
#define BPB_TOTAL_SECTORS_32         0x20    /* 4 bytes */
#define BPB_SECTORS_PER_FAT_32       0x24    /* 4 bytes */
#define BPB_EXT_FLAGS                0x28    /* 2 bytes */
#define BPB_FS_VERSION               0x2A    /* 2 bytes */
#define BPB_ROOT_CLUSTER             0x2C    /* 4 bytes */
#define BPB_FS_INFO_SECTOR           0x30    /* 2 bytes */
#define BPB_BACKUP_BOOT_SECTOR       0x32    /* 2 bytes */
#define BPB_RESERVED                 0x34    /* 12 bytes */
#define BPB_DRIVE_NUMBER             0x40    /* 1 byte */
#define BPB_RESERVED1                0x41    /* 1 byte */
#define BPB_BOOT_SIGNATURE           0x42    /* 1 byte */
#define BPB_VOLUME_ID                0x43    /* 4 bytes */
#define BPB_VOLUME_LABEL             0x47    /* 11 bytes */
#define BPB_FS_TYPE                  0x52    /* 8 bytes */

/** 目录项常量 */
#define DIR_ENTRY_SIZE               32
#define DIR_NAME_LENGTH              11
#define DIR_ATTR_READ_ONLY           0x01
#define DIR_ATTR_HIDDEN              0x02
#define DIR_ATTR_SYSTEM              0x04
#define DIR_ATTR_VOLUME_ID           0x08
#define DIR_ATTR_DIRECTORY           0x10
#define DIR_ATTR_ARCHIVE             0x20
#define DIR_ATTR_LONG_NAME           (DIR_ATTR_READ_ONLY | DIR_ATTR_HIDDEN | DIR_ATTR_SYSTEM | DIR_ATTR_VOLUME_ID)
#define DIR_ATTR_LONG_NAME_MASK      0x3F

/** 长文件名常量 */
#define LFN_SEQUENCE_MASK            0x1F
#define LFN_SEQUENCE_LAST            0x40
#define LFN_NAME1_LENGTH             10
#define LFN_NAME2_LENGTH             12
#define LFN_NAME3_LENGTH             4

/** FAT 特殊值 */
#define FAT32_CLUSTER_FREE           0x00000000
#define FAT32_CLUSTER_RESERVED       0x00000001
#define FAT32_CLUSTER_BAD            0x0FFFFFF7
#define FAT32_CLUSTER_EOF_MIN        0x0FFFFFF8
#define FAT32_CLUSTER_EOF_MAX        0x0FFFFFFF

/** FAT16 根目录哨兵簇号（簇 1 在 FAT12/16/32 中均为保留值，不指向真实数据） */
#define FAT16_ROOT_DIR_CLUSTER       1u

/* ============================================
 * 数据结构定义
 * ============================================ */

/**
 * MBR 分区表项
 */
typedef struct {
    uint8_t  status;           /* 分区状态 */
    uint8_t  start_head;       /* 起始磁头 */
    uint8_t  start_sector;     /* 起始扇区 */
    uint8_t  start_cylinder;   /* 起始柱面 */
    uint8_t  type;             /* 分区类型 */
    uint8_t  end_head;         /* 结束磁头 */
    uint8_t  end_sector;       /* 结束扇区 */
    uint8_t  end_cylinder;     /* 结束柱面 */
    uint32_t start_lba;        /* 起始 LBA */
    uint32_t total_sectors;    /* 总扇区数 */
} __attribute__((packed)) MBR_PartitionEntry_t;

/**
 * FAT32 BPB (BIOS Parameter Block)
 */
typedef struct {
    uint16_t bytes_per_sector;      /* 每扇区字节数 */
    uint8_t  sectors_per_cluster;   /* 每簇扇区数 */
    uint16_t reserved_sectors;      /* 保留扇区数 */
    uint8_t  num_fats;              /* FAT 表数量 */
    uint16_t root_entries;          /* 根目录项数 (FAT32=0) */
    uint16_t total_sectors_16;      /* 总扇区数 (16位) */
    uint8_t  media_type;            /* 媒体类型 */
    uint16_t sectors_per_fat_16;    /* 每FAT扇区数 (16位) */
    uint16_t sectors_per_track;     /* 每磁道扇区数 */
    uint16_t num_heads;             /* 磁头数 */
    uint32_t hidden_sectors;        /* 隐藏扇区数 */
    uint32_t total_sectors_32;      /* 总扇区数 (32位) */
    uint32_t sectors_per_fat_32;    /* 每FAT扇区数 (32位) */
    uint16_t ext_flags;             /* 扩展标志 */
    uint16_t fs_version;            /* 文件系统版本 */
    uint32_t root_cluster;          /* 根目录簇号 */
    uint16_t fs_info_sector;        /* FSINFO 扇区号 */
    uint16_t backup_boot_sector;    /* 备份引导扇区 */
    uint8_t  drive_number;          /* 驱动器号 */
    uint8_t  reserved1;             /* 保留 */
    uint8_t  boot_signature;        /* 引导签名 */
    uint32_t volume_id;             /* 卷ID */
    char     volume_label[11];      /* 卷标 */
    char     fs_type[8];            /* 文件系统类型 */
} __attribute__((packed)) FAT32_BPB_t;

/**
 * 短文件名目录项 (32字节)
 */
typedef struct {
    char     name[DIR_NAME_LENGTH];     /* 文件名 (8.3格式) */
    uint8_t  attr;                      /* 属性 */
    uint8_t  nt_reserved;               /* NT保留 */
    uint8_t  create_time_tenth;         /* 创建时间 (10ms) */
    uint16_t create_time;               /* 创建时间 */
    uint16_t create_date;               /* 创建日期 */
    uint16_t access_date;               /* 访问日期 */
    uint16_t cluster_high;              /* 起始簇号高16位 */
    uint16_t modify_time;               /* 修改时间 */
    uint16_t modify_date;               /* 修改日期 */
    uint16_t cluster_low;               /* 起始簇号低16位 */
    uint32_t size;                      /* 文件大小 */
} __attribute__((packed)) FAT32_DirEntry_t;

/**
 * 长文件名目录项 (32字节)
 */
typedef struct {
    uint8_t  sequence;                  /* 序列号 */
    uint16_t name1[LFN_NAME1_LENGTH/2]; /* 文件名部分1 (5个字符) */
    uint8_t  attr;                      /* 属性 (必须是 0x0F) */
    uint8_t  type;                      /* 类型 (0) */
    uint8_t  checksum;                  /* 校验和 */
    uint16_t name2[LFN_NAME2_LENGTH/2]; /* 文件名部分2 (6个字符) */
    uint16_t cluster_low;               /* 簇号低16位 (0) */
    uint16_t name3[LFN_NAME3_LENGTH/2]; /* 文件名部分3 (2个字符) */
} __attribute__((packed)) FAT32_LFNEntry_t;

/**
 * FAT32 文件系统信息
 */
typedef struct {
    uint32_t fat_start_sector;      /* FAT表起始扇区 */
    uint32_t data_start_sector;     /* 数据区起始扇区 */
    uint32_t root_dir_sector;       /* 根目录起始扇区 */
    uint32_t total_clusters;        /* 总簇数 */
    bool     is_fat16;              /* true = FAT12/16，false = FAT32 */
    uint32_t root_dir_start_lba;    /* FAT16 根目录固定起始扇区 */
    uint32_t root_dir_num_sectors;  /* FAT16 根目录扇区数 */
    FAT32_BPB_t bpb;                /* BPB 信息 */
} FAT32_FSInfo_t;

/**
 * 文件信息
 */
typedef struct {
    char     name[256];             /* 文件名 (支持长文件名) */
    uint32_t size;                  /* 文件大小 */
    uint32_t start_cluster;         /* 起始簇号 */
    uint16_t modify_time;           /* 修改时间 */
    uint16_t modify_date;           /* 修改日期 */
    uint8_t  attr;                  /* 属性 */
} FAT32_FileInfo_t;

/**
 * 文件句柄
 */
typedef struct {
    FAT32_FileInfo_t info;          /* 文件信息 */
    uint32_t current_cluster;       /* 当前簇号 */
    uint32_t current_sector;        /* 当前扇区号 */
    uint32_t position;              /* 当前位置 (字节) */
    uint32_t bytes_read;            /* 已读取字节数 */
} FAT32_FileHandle_t;

/* ============================================
 * 接口函数声明
 * ============================================ */

/**
 * 初始化 FAT32 读取器
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_Init(void);

/**
 * 反初始化 FAT32 读取器
 */
void FAT32_DeInit(void);

/**
 * 查找文件
 * @param filename 要查找的文件名 (支持通配符)
 * @param file_info 输出文件信息
 * @return SUCCESS=找到, 其他=未找到或错误
 */
BG_ERR FAT32_FindFile(const char *filename, FAT32_FileInfo_t *file_info);

/**
 * 打开文件
 * @param filename 文件名
 * @param handle 输出文件句柄
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_OpenFile(const char *filename, FAT32_FileHandle_t *handle);

/**
 * 从文件读取数据
 * @param handle 文件句柄
 * @param buffer 输出缓冲区
 * @param size 要读取的字节数
 * @return 实际读取的字节数, <0 表示错误
 */
int32_t FAT32_ReadFile(FAT32_FileHandle_t *handle, void *buffer, uint32_t size);

/**
 * 关闭文件
 * @param handle 文件句柄
 */
void FAT32_CloseFile(FAT32_FileHandle_t *handle);

/**
 * 列出目录内容 (仅根目录，保留向后兼容)
 */
typedef int (*FAT32_ListCallback_t)(const FAT32_FileInfo_t *info, void *user);
BG_ERR FAT32_ListDir(const char *path, FAT32_ListCallback_t cb, void *user);

/**
 * 列出指定簇号目录的内容 (支持子目录)
 * @param dir_cluster  目录起始簇号 (用 FAT32_GetRootCluster() 获取根目录)
 */
BG_ERR FAT32_ListDirByCluster(uint32_t dir_cluster, FAT32_ListCallback_t cb, void *user);

/**
 * 在指定目录中查找条目 (文件或子目录)
 * @param dir_cluster  目录起始簇号
 * @param name         要查找的名称
 * @param info         输出文件信息
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_FindEntryInDir(uint32_t dir_cluster, const char *name, FAT32_FileInfo_t *info);

/**
 * 在指定目录中打开文件
 * @param dir_cluster  目录起始簇号
 * @param filename     文件名 (8.3 格式)
 * @param handle       输出文件句柄
 */
BG_ERR FAT32_OpenFileInDir(uint32_t dir_cluster, const char *filename, FAT32_FileHandle_t *handle);

/**
 * 返回根目录簇号
 */
uint32_t FAT32_GetRootCluster(void);

/**
 * 写入文件 (在指定目录中，不存在则创建，已存在则覆写)
 * @param dir_cluster  目标目录起始簇号
 * @param filename     文件名 (8.3 格式)
 * @param data         数据缓冲区
 * @param size         要写入的字节数
 * @return 实际写入字节数, <0 表示错误
 */
int32_t FAT32_WriteFile(uint32_t dir_cluster, const char *filename, const void *data, uint32_t size);

/**
 * 追加数据到现有文件末尾 (文件不存在则创建)
 * @param dir_cluster  目标目录起始簇号
 * @param filename     文件名 (8.3 格式)
 * @param data         数据缓冲区
 * @param size         要追加的字节数
 * @return 实际追加字节数, <0 表示错误
 */
int32_t FAT32_AppendFile(uint32_t dir_cluster, const char *filename, const void *data, uint32_t size);

/**
 * 删除文件 (标记首字节为 0xE5，不回收簇链)
 * @param dir_cluster  所在目录起始簇号
 * @param filename     文件名 (8.3 格式)
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_DeleteFile(uint32_t dir_cluster, const char *filename);

/**
 * 在指定目录中创建子目录
 * @param dir_cluster  父目录起始簇号
 * @param dirname      目录名 (8.3 格式)
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_MkDir(uint32_t dir_cluster, const char *dirname);

/**
 * 删除指定目录中的子目录 (目录须为空)
 * @param dir_cluster  父目录起始簇号
 * @param dirname      目录名 (8.3 格式)
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_RmDir(uint32_t dir_cluster, const char *dirname);

/**
 * 重命名指定目录中的文件或子目录
 * @param dir_cluster  所在目录起始簇号
 * @param oldname      原名称
 * @param newname      新名称
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_Rename(uint32_t dir_cluster, const char *oldname, const char *newname);

/**
 * 获取文件系统信息
 * @param info 输出文件系统信息
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_GetFSInfo(FAT32_FSInfo_t *info);

/**
 * 检查 SD 卡是否就绪
 * @return true=就绪, false=未就绪
 */
bool FAT32_IsCardReady(void);

#ifdef __cplusplus
}
#endif

#endif /* FAT32_EN */

#endif /* __FAT32_READER_H__ */