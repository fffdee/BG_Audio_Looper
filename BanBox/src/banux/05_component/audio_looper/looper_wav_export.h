/**
 * @file looper_wav_export.h
 * @brief Audio Looper WAV 文件导出功能
 *
 * 将 PSRAM 中的录音数据导出为 WAV 格式保存到 NAND Flash FAT32 分区，
 * 然后通过 CDC 文件管理器传输到 PC。
 *
 * 支持功能：
 * - 单段导出 (segment 0-3)
 * - 混音导出 (所有启用段混合)
 * - 自动生成文件名 (looper_seg0_001.wav, looper_mix_001.wav)
 * - WAV 格式: PCM 16-bit, 44.1kHz/48kHz, 单声道/立体声
 *
 * 依赖：
 * - FAT32_EN && HW_DRV_FLASH_NAND_EN
 * - LOOPER_USE_STORAGE_ABSTRACTION
 */

#ifndef __LOOPER_WAV_EXPORT_H__
#define __LOOPER_WAV_EXPORT_H__

#include "product_def.h"
#include "audio_looper.h"

#if FAT32_EN && HW_DRV_FLASH_NAND_EN

#include <stdint.h>
#include <stdbool.h>
#include "err_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * WAV 导出配置
 * ============================================ */

/** WAV 文件采样率 (Hz) */
#ifndef LOOPER_WAV_SAMPLE_RATE
#define LOOPER_WAV_SAMPLE_RATE    LOOPER_SAMPLE_RATE
#endif

/** WAV 文件声道数 (1=单声道, 2=立体声) */
#ifndef LOOPER_WAV_CHANNELS
#define LOOPER_WAV_CHANNELS       2
#endif

/** WAV 文件位深度 (固定16-bit PCM) */
#define LOOPER_WAV_BIT_DEPTH      16

/** 导出文件名前缀 */
#define LOOPER_WAV_PREFIX         "looper"

/** 导出目录 (NAND FAT32 根目录下) */
#define LOOPER_WAV_DIR            "/recordings"

/* ============================================
 * WAV 导出接口
 * ============================================ */

/**
 * @brief 初始化 WAV 导出功能
 * @note  确保 FAT32_NAND_Init() 已调用
 * @return SUCCESS 或错误码
 */
BG_ERR LooperWAV_Init(void);

/**
 * @brief 导出单个段为 WAV 文件
 * @param segment_index 段索引 (0-3)
 * @param filename      输出文件名 (可选，NULL=自动生成)
 * @return SUCCESS 或错误码
 *
 * 示例：
 *   LooperWAV_ExportSegment(0, NULL);  // 导出为 /recordings/looper_seg0_001.wav
 *   LooperWAV_ExportSegment(1, "my_guitar.wav");  // 导出为 /recordings/my_guitar.wav
 */
BG_ERR LooperWAV_ExportSegment(uint8_t segment_index, const char *filename);

/**
 * @brief 导出所有段的混音为 WAV 文件
 * @param filename 输出文件名 (可选，NULL=自动生成)
 * @return SUCCESS 或错误码
 *
 * 示例：
 *   LooperWAV_ExportMix(NULL);  // 导出为 /recordings/looper_mix_001.wav
 */
BG_ERR LooperWAV_ExportMix(const char *filename);

/**
 * @brief 删除指定 WAV 文件
 * @param filename 文件名 (无需路径前缀)
 * @return SUCCESS 或错误码
 */
BG_ERR LooperWAV_DeleteFile(const char *filename);

/**
 * @brief 列出所有录音文件
 * @param buffer     输出缓冲区 (每行一个文件名)
 * @param buffer_len 缓冲区大小
 * @return 文件数量，或负数表示错误
 */
int LooperWAV_ListFiles(char *buffer, uint32_t buffer_len);

/**
 * @brief 获取 NAND FAT32 剩余空间 (字节)
 * @return 剩余空间 (字节)
 */
uint32_t LooperWAV_GetFreeSpace(void);

/**
 * @brief 格式化 NAND FAT32 分区 (警告: 删除所有文件!)
 * @return SUCCESS 或错误码
 */
BG_ERR LooperWAV_FormatNAND(void);

/* ============================================
 * WAV 文件格式定义 (内部使用)
 * ============================================ */

/** WAV RIFF 头 (44字节) */
typedef struct {
    /* RIFF Chunk */
    char     riff_id[4];        /* "RIFF" */
    uint32_t file_size;         /* 文件大小 - 8 */
    char     wave_id[4];        /* "WAVE" */
    
    /* fmt Chunk */
    char     fmt_id[4];         /* "fmt " */
    uint32_t fmt_size;          /* 16 */
    uint16_t audio_format;      /* 1 = PCM */
    uint16_t num_channels;      /* 1=单声道, 2=立体声 */
    uint32_t sample_rate;       /* 采样率 */
    uint32_t byte_rate;         /* 采样率 * 通道数 * 位深/8 */
    uint16_t block_align;       /* 通道数 * 位深/8 */
    uint16_t bits_per_sample;   /* 16 */
    
    /* data Chunk */
    char     data_id[4];        /* "data" */
    uint32_t data_size;         /* 音频数据字节数 */
} __attribute__((packed)) WAV_Header_t;

#ifdef __cplusplus
}
#endif

#endif /* FAT32_EN && HW_DRV_FLASH_NAND_EN */

#endif /* __LOOPER_WAV_EXPORT_H__ */
