/**
 * @file psram_buffer.h
 * @brief PSRAM 音符缓冲区管理器接口
 *
 * 管理 PSRAM 中的音符数据缓冲区，支持多音符并发播放。
 * 提供缓冲区分配、缓存管理、数据加载等功能。
 */

#ifndef __PSRAM_BUFFER_H__
#define __PSRAM_BUFFER_H__

#include "bg_config.h"

#if SYNTH_SD_NAND_PSRAM_EN

#include <stdint.h>
#include <stdbool.h>
#include "err_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * PSRAM 缓冲区常量定义
 * ============================================ */

/** PSRAM 总容量 (8MB) */
#define PSRAM_TOTAL_SIZE             (8u * 1024u * 1024u)

/** 缓冲区管理区域 */
#define PSRAM_MGMT_START             (6u * 1024u * 1024u)    /* 6MB */
#define PSRAM_MGMT_SIZE              (2u * 1024u * 1024u)    /* 2MB */

/** 音符缓冲区池 */
#define PSRAM_BUFFER_POOL_START      0x000000                /* 0MB */
#define PSRAM_BUFFER_POOL_SIZE       (6u * 1024u * 1024u)    /* 6MB */

/** 单个音符缓冲区大小 (64KB) */
#define PSRAM_NOTE_BUFFER_SIZE       (64u * 1024u)

/** 最大音符缓冲区数量 */
#define PSRAM_MAX_NOTE_BUFFERS       (PSRAM_BUFFER_POOL_SIZE / PSRAM_NOTE_BUFFER_SIZE)

/** 缓冲区对齐要求 (4KB) */
#define PSRAM_BUFFER_ALIGNMENT       4096

/** LRU 缓存参数 */
#define PSRAM_LRU_MAX_AGE            1000                    /* 最大年龄值 */

/* ============================================
 * 数据结构定义
 * ============================================ */

/**
 * 音符缓冲区状态
 */
typedef enum {
    PSRAM_BUFFER_FREE = 0,         /* 空闲 */
    PSRAM_BUFFER_LOADING,          /* 正在加载 */
    PSRAM_BUFFER_READY,            /* 数据就绪 */
    PSRAM_BUFFER_PLAYING,          /* 正在播放 */
    PSRAM_BUFFER_ERROR             /* 错误状态 */
} PSRAM_BufferState_t;

/**
 * 音符缓冲区信息
 */
typedef struct {
    uint32_t buffer_id;            /* 缓冲区ID (0-95) */
    uint32_t address;              /* PSRAM地址 */
    uint32_t size;                 /* 缓冲区大小 */
    PSRAM_BufferState_t state;     /* 缓冲区状态 */
    uint32_t note_number;          /* MIDI音符号 */
    uint32_t program;              /* MIDI程序号 */
    uint32_t sample_rate;          /* 采样率 */
    uint32_t data_size;            /* 实际数据大小 */
    uint32_t last_access;          /* 最后访问时间戳 */
    uint32_t access_count;         /* 访问计数 */
    uint32_t checksum;             /* 数据校验和 */
} PSRAM_BufferInfo_t;

/**
 * PSRAM 缓冲区管理器状态
 */
typedef struct {
    bool initialized;              /* 初始化标志 */
    PSRAM_BufferInfo_t *buffer_table; /* 缓冲区信息表 */
    uint32_t next_timestamp;       /* 时间戳计数器 */
    uint32_t total_buffers;        /* 总缓冲区数 */
    uint32_t free_buffers;         /* 空闲缓冲区数 */
    uint32_t loading_buffers;      /* 正在加载的缓冲区数 */
    uint32_t ready_buffers;        /* 数据就绪的缓冲区数 */
    uint32_t playing_buffers;      /* 正在播放的缓冲区数 */
} PSRAM_BufferManager_t;

/**
 * 音符数据请求
 */
typedef struct {
    uint8_t note;                  /* MIDI音符号 */
    uint8_t velocity;              /* 力度 */
    uint8_t program;               /* MIDI程序号 */
    uint32_t sample_rate;          /* 期望采样率 */
    bool high_priority;            /* 高优先级请求 */
} PSRAM_NoteRequest_t;

/**
 * 缓冲区分配结果
 */
typedef struct {
    uint32_t buffer_id;            /* 分配的缓冲区ID */
    uint32_t address;              /* PSRAM地址 */
    uint32_t size;                 /* 缓冲区大小 */
    bool from_cache;               /* 是否来自缓存 */
} PSRAM_BufferAlloc_t;

/* ============================================
 * 接口函数声明
 * ============================================ */

/**
 * 初始化 PSRAM 缓冲区管理器
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_BufferInit(void);

/**
 * 反初始化 PSRAM 缓冲区管理器
 */
void PSRAM_BufferDeInit(void);

/**
 * 请求音符缓冲区
 * @param request 音符数据请求
 * @param alloc_result 分配结果输出
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_RequestNoteBuffer(const PSRAM_NoteRequest_t *request,
                              PSRAM_BufferAlloc_t *alloc_result);

/**
 * 释放音符缓冲区
 * @param buffer_id 缓冲区ID
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_ReleaseNoteBuffer(uint32_t buffer_id);

/**
 * 从 NAND 异步加载音符数据到 PSRAM
 * @param buffer_id 目标缓冲区ID
 * @param nand_offset NAND中的数据偏移
 * @param data_size 数据大小
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_LoadNoteData(uint32_t buffer_id, uint32_t nand_offset, uint32_t data_size);

/**
 * 检查缓冲区数据是否就绪
 * @param buffer_id 缓冲区ID
 * @return true=就绪, false=未就绪
 */
bool PSRAM_IsBufferReady(uint32_t buffer_id);

/**
 * 获取缓冲区状态
 * @param buffer_id 缓冲区ID
 * @param info 缓冲区信息输出
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_GetBufferInfo(uint32_t buffer_id, PSRAM_BufferInfo_t *info);

/**
 * 读取缓冲区数据 (用于音频合成)
 * @param buffer_id 缓冲区ID
 * @param offset 读取偏移 (字节)
 * @param data 输出缓冲区
 * @param size 读取大小 (字节)
 * @return 实际读取的字节数
 */
int32_t PSRAM_ReadBufferData(uint32_t buffer_id, uint32_t offset,
                           void *data, uint32_t size);

/**
 * 更新缓冲区访问时间戳 (用于LRU)
 * @param buffer_id 缓冲区ID
 */
void PSRAM_UpdateAccessTime(uint32_t buffer_id);

/**
 * 执行垃圾回收 (释放最久未使用的缓冲区)
 * @param target_free_buffers 目标空闲缓冲区数量
 * @return 释放的缓冲区数量
 */
uint32_t PSRAM_GarbageCollect(uint32_t target_free_buffers);

/**
 * 获取缓冲区统计信息
 * @param total_buffers 总缓冲区数
 * @param free_buffers 空闲缓冲区数
 * @param ready_buffers 数据就绪缓冲区数
 * @param playing_buffers 正在播放缓冲区数
 */
void PSRAM_GetStats(uint32_t *total_buffers, uint32_t *free_buffers,
                   uint32_t *ready_buffers, uint32_t *playing_buffers);

/**
 * 预加载音符数据 (预测性缓存)
 * @param note MIDI音符号
 * @param program MIDI程序号
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_PrefetchNote(uint8_t note, uint8_t program);

/**
 * 设置缓冲区状态
 * @param buffer_id 缓冲区ID
 * @param state 新状态
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_SetBufferState(uint32_t buffer_id, PSRAM_BufferState_t state);

/**
 * 刷新所有缓冲区 (用于调试)
 */
void PSRAM_FlushAllBuffers(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_SD_NAND_PSRAM_EN */

#endif /* __PSRAM_BUFFER_H__ */