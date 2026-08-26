/**
 * @file synth_sdnandpsram.h
 * @brief SD卡 + NAND + PSRAM 合成器集成模块接口
 *
 * 将三级存储架构 (SD→NAND→PSRAM) 集成到现有合成器框架。
 *
 * 工作流程:
 *   开机初始化:
 *     FAT32_Init() → 查找 .sf2 文件 → NAND_StoreInit()
 *     → (首次) 将 SF2 从 SD 拷贝到 NAND → PSRAM_BufferInit()
 *     → 安装 NAND 存储驱动 → soundbank_manager.Init()
 *
 *   音符播放:
 *     NoteOn → 分配 PSRAM 缓冲区 → 从 NAND 异步加载样本
 *     → 音频实时从 PSRAM 读取
 *     NoteOff → 释放 PSRAM 缓冲区
 */

#ifndef __SYNTH_SDNANDPSRAM_H__
#define __SYNTH_SDNANDPSRAM_H__

#include "bg_config.h"

#if SYNTH_SD_NAND_PSRAM_EN

#include <stdint.h>
#include <stdbool.h>
#include "err_handle.h"
#include "bg_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * NAND 中 SF2 文件布局常量
 * ============================================ */

/**
 * SF2 原始数据在 NAND 数据区中的位置
 *
 * NAND 数据区 (64MB-256MB):
 *   [64MB - 96MB]  SF2 原始数据 (本模块)
 *   [96MB - 256MB] 保留给 NAND_StoreProgram() (音色索引数据)
 */
#define SYNTH_SF2_NAND_BLOB_OFFSET   (64u * 1024u * 1024u)  /* 64MB, 即 NAND_DATA_START */
#define SYNTH_SF2_MAX_BLOB_SIZE      (32u * 1024u * 1024u)  /* 最大 32MB SF2 文件 */

/** NAND 中 SF2 头部魔数 */
#define SYNTH_SF2_HEADER_MAGIC       0x32465342UL  /* "BSF2" */

/** SF2 头部大小 (字节), 128 字节对齐 */
#define SYNTH_SF2_HEADER_SIZE        128

/** SF2 复制时的 NAND 写入块大小 (须为 NAND 页大小的倍数, 2048B) */
#define SYNTH_SF2_COPY_CHUNK_SIZE    (2u * 1024u)   /* 2KB / chunk */

/** 最大同时活跃声部数 (PSRAM 缓冲区资源限制) */
#define SYNTH_MAX_PSRAM_VOICES       16

/* ============================================
 * 数据结构
 * ============================================ */

/**
 * NAND 中 SF2 Blob 头部 (128 字节, packed)
 */
typedef struct {
    uint32_t magic;            /* SYNTH_SF2_HEADER_MAGIC */
    uint32_t sf2_size;         /* SF2 文件大小 (字节) */
    uint32_t checksum;         /* SF2 数据简单校验和 */
    uint32_t version;          /* 头部版本 (当前 1) */
    char     filename[64];     /* 原始文件名 */
    uint8_t  reserved[40];     /* 保留字段 */
} __attribute__((packed)) SYNTH_SF2NandHeader_t;

/**
 * NAND-backed 存储驱动私有状态
 */
typedef struct {
    uint32_t sf2_size;         /* NAND 中 SF2 文件大小 */
    uint32_t sf2_data_start;   /* NAND 中 SF2 数据起始地址 */
    bool     initialized;
} SYNTH_NandDriverState_t;

/**
 * 模块初始化状态
 */
typedef struct {
    bool    storage_ready;     /* NAND 存储驱动已安装 */
    bool    psram_ready;       /* PSRAM 缓冲区池已初始化 */
    bool    soundbank_ready;   /* soundbank_manager 已初始化 */
    uint32_t sf2_size;         /* 当前 SF2 文件大小 */
    char     sf2_filename[64]; /* 当前 SF2 文件名 */
} SYNTH_Status_t;

/* ============================================
 * NAND 存储驱动 (供 BG_Storage.SetDriver 使用)
 * ============================================ */

/** NAND-backed BG_Storage_Driver_t 实例 (在 .c 中定义) */
extern const BG_Storage_Driver_t synth_nand_storage_driver;

/* ============================================
 * 公开接口
 * ============================================ */

/**
 * 初始化 SD+NAND+PSRAM 合成器集成模块
 *
 * 按顺序执行:
 *   1. FAT32_Init()
 *   2. 在 SD 卡根目录查找第一个 .sf2 文件
 *   3. NAND_StoreInit()
 *   4. 检查 SF2 是否已缓存在 NAND; 如未缓存则执行拷贝
 *   5. PSRAM_BufferInit()
 *   6. 安装 NAND 存储驱动 (BG_Storage.SetDriver)
 *   7. soundbank_manager.Init()
 *
 * @return SUCCESS 或错误码
 */
BG_ERR SYNTH_SDNANDPSRAM_Init(void);

/**
 * 反初始化集成模块
 */
void SYNTH_SDNANDPSRAM_DeInit(void);

/**
 * 强制重新从 SD 卡拷贝 SF2 到 NAND (覆盖已有数据)
 *
 * 当 SD 卡上的 SF2 文件已更новый时调用。
 * 注意: 将阻塞较长时间 (取决于 SF2 大小和 NAND 写速)。
 *
 * @return SUCCESS 或错误码
 */
BG_ERR SYNTH_SDNANDPSRAM_ReloadFromSD(void);

/**
 * 获取模块状态
 * @param status 输出状态信息
 */
void SYNTH_SDNANDPSRAM_GetStatus(SYNTH_Status_t *status);

/**
 * 获取 NAND SF2 加载进度 (拷贝期间使用)
 * @param bytes_done  已拷贝字节数
 * @param bytes_total SF2 总字节数
 */
void SYNTH_SDNANDPSRAM_GetCopyProgress(uint32_t *bytes_done, uint32_t *bytes_total);

/**
 * 音符触发 (供 MIDI 控制器调用)
 *
 * 分配 PSRAM 缓冲区并触发从 NAND 的异步加载。
 * 同时调用 sf2_note_on() 激活合成器声部。
 *
 * @param note     MIDI 音符号 (0-127)
 * @param velocity 力度 (0-127)
 * @param program  MIDI 程序号 (0-127)
 */
void SYNTH_SDNANDPSRAM_NoteOn(uint8_t note, uint8_t velocity, uint8_t program);

/**
 * 音符释放 (供 MIDI 控制器调用)
 *
 * 释放该音符对应的 PSRAM 缓冲区。
 * 同时调用 sf2_note_off()。
 *
 * @param note     MIDI 音符号
 * @param program  MIDI 程序号
 */
void SYNTH_SDNANDPSRAM_NoteOff(uint8_t note, uint8_t program);

/* ============================================
 * Program Change 异步预热接口
 * ============================================ */

/**
 * 触发 PSRAM 预热 (供 MIDI ProgramChange 调用，非阻塞)
 *
 * 记录待预热的 program 号，实际预热由主循环中的 SYNTH_LoadTick() 驱动，
 * 不创建任何 RTOS 任务，不阻塞调用方。
 *
 * @param program  MIDI 程序号 (0-127)
 */
void SYNTH_LoadProgram(uint8_t program);

/**
 * 预热状态机驱动函数（在主循环 while(1) 中调用）
 *
 * 每次调用执行一步预热：向 PSRAM_PrefetchNote() 提交一个音符。
 * 当前 program 的全部音符预热完毕后自动停止，直到下次 SYNTH_LoadProgram()。
 * 不依赖 RTOS 任务或 vTaskDelay，仅依赖 bg_get_tick_ms()。
 */
void SYNTH_LoadTick(void);

/**
 * 执行完整的合成器启动序列 (实现见 synth_startup.c)
 * @return true=启动成功, false=启动失败
 */
bool SYNTH_StartupSequence(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_SD_NAND_PSRAM_EN */

#endif /* __SYNTH_SDNANDPSRAM_H__ */
