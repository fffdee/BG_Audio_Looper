/**
 * @file nand_store.h
 * @brief NAND Flash 音色存储管理器接口
 *
 * 管理 NAND Flash 中的音色数据存储和检索。
 * 提供音色数据的写入、读取、索引管理功能。
 */

#ifndef __NAND_STORE_H__
#define __NAND_STORE_H__

#include "product_def.h"

#if SYNTH_SD_NAND_PSRAM_EN

#include <stdint.h>
#include <stdbool.h>
#include "err_handle.h"

/**
 * 启用此宏：索引表缓存分配在 PSRAM，节省约 8192 字节 SRAM
 * 注释掉此宏：索引表缓存分配在 SRAM（malloc）
 */
#define NAND_STORE_USE_PSRAM_INDEX

#ifdef NAND_STORE_USE_PSRAM_INDEX
#include "psram_heap.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * NAND 存储布局常量
 * ============================================ */

/** NAND Flash 总容量 (256MB) */
#define NAND_TOTAL_SIZE             (256u * 1024u * 1024u)

/** 音色索引表区域 */
#define NAND_INDEX_START            (32u * 1024u * 1024u)    /* 32MB */
#define NAND_INDEX_SIZE             (32u * 1024u * 1024u)    /* 32MB */

/** 音色数据区域 */
#define NAND_DATA_START             (64u * 1024u * 1024u)    /* 64MB */
#define NAND_DATA_SIZE              (192u * 1024u * 1024u)   /* 192MB */

/** 索引表结构 */
#define NAND_INDEX_ENTRY_SIZE       64                        /* 每个索引项64字节 */
#define NAND_MAX_PROGRAMS           128                       /* 最大音色数 */
#define NAND_INDEX_TABLE_SIZE       (NAND_MAX_PROGRAMS * NAND_INDEX_ENTRY_SIZE)

/** 校验和 */
#define NAND_CHECKSUM_SEED          0x12345678

/* ============================================
 * 数据结构定义
 * ============================================ */

/**
 * 音色索引项
 */
typedef struct {
    uint8_t  program;              /* MIDI 程序号 (0-127) */
    uint8_t  bank_msb;             /* Bank MSB */
    uint8_t  bank_lsb;             /* Bank LSB */
    uint8_t  reserved1;            /* 保留 */
    uint32_t data_offset;          /* 数据在 NAND 中的偏移 */
    uint32_t data_size;            /* 数据大小 (字节) */
    uint32_t checksum;             /* 数据校验和 */
    uint16_t format;               /* 音源格式 (0=SF2, 1=BGS) */
    uint16_t flags;                /* 标志位 */
    char     name[32];             /* 音色名称 */
    uint32_t timestamp;            /* 创建时间戳 */
    uint32_t reserved2[2];         /* 保留 */
} __attribute__((packed)) NAND_ProgramIndex_t;

/**
 * NAND 存储状态
 */
typedef struct {
    bool initialized;              /* 初始化标志 */
    uint32_t next_data_offset;     /* 下个数据写入偏移 */
    uint32_t used_space;           /* 已使用空间 */
#ifdef NAND_STORE_USE_PSRAM_INDEX
    psram_ptr_t index_cache_addr;  /* 索引表在 PSRAM 中的地址（节省约 8192 字节 SRAM） */
#else
    NAND_ProgramIndex_t *index_cache;  /* 索引表 SRAM 缓存指针 */
#endif
    bool index_dirty;              /* 索引表是否需要写入 */
} NAND_StoreState_t;

/**
 * 音色数据块头
 */
typedef struct {
    uint32_t magic;                /* 魔数 0x53464E44 ("SFND") */
    uint32_t size;                 /* 数据块大小 */
    uint32_t checksum;             /* 数据校验和 */
    uint16_t format;               /* 格式标识 */
    uint16_t version;              /* 版本号 */
    uint32_t program;              /* MIDI 程序号 */
    char     name[32];             /* 音色名称 */
} __attribute__((packed)) NAND_DataHeader_t;

/* ============================================
 * 接口函数声明
 * ============================================ */

/**
 * 初始化 NAND 存储管理器
 * @return SUCCESS 或错误码
 */
BG_ERR NAND_StoreInit(void);

/**
 * 反初始化 NAND 存储管理器
 */
void NAND_StoreDeInit(void);

/**
 * 存储音色数据到 NAND
 * @param program MIDI 程序号
 * @param data 音色数据缓冲区
 * @param size 数据大小
 * @param name 音色名称
 * @param format 音源格式 (0=SF2, 1=BGS)
 * @return SUCCESS 或错误码
 */
BG_ERR NAND_StoreProgram(uint8_t program, const void *data, uint32_t size,
                        const char *name, uint16_t format);

/**
 * 从 NAND 读取音色数据
 * @param program MIDI 程序号
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @param actual_size 输出实际读取大小
 * @return SUCCESS 或错误码
 */
BG_ERR NAND_LoadProgram(uint8_t program, void *buffer, uint32_t size,
                       uint32_t *actual_size);

/**
 * 删除音色数据
 * @param program MIDI 程序号
 * @return SUCCESS 或错误码
 */
BG_ERR NAND_DeleteProgram(uint8_t program);

/**
 * 检查音色是否存在
 * @param program MIDI 程序号
 * @return true=存在, false=不存在
 */
bool NAND_ProgramExists(uint8_t program);

/**
 * 获取音色信息
 * @param program MIDI 程序号
 * @param index 输出索引信息
 * @return SUCCESS 或错误码
 */
BG_ERR NAND_GetProgramInfo(uint8_t program, NAND_ProgramIndex_t *index);

/**
 * 获取存储使用统计
 * @param total_space 总空间
 * @param used_space 已使用空间
 * @param program_count 音色数量
 */
void NAND_GetStats(uint32_t *total_space, uint32_t *used_space, uint32_t *program_count);

/**
 * 格式化 NAND 存储区域 (清除所有数据)
 * @return SUCCESS 或错误码
 */
BG_ERR NAND_Format(void);

/**
 * 验证 NAND 数据完整性
 * @param program MIDI 程序号 (0xFF=验证所有)
 * @return SUCCESS 或错误码
 */
BG_ERR NAND_VerifyIntegrity(uint8_t program);

/**
 * 整理存储空间 (垃圾回收)
 * @return SUCCESS 或错误码
 */
BG_ERR NAND_Compact(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_SD_NAND_PSRAM_EN */

#endif /* __NAND_STORE_H__ */