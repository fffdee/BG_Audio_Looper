/**
 * @file looper_wav_export.c
 * @brief Audio Looper WAV 文件导出功能实现
 *
 * 功能: 将 PSRAM 中的 Looper 段录音导出为 WAV 文件保存到 NAND Flash FAT32 分区
 * 上位机通过 CDC 文件管理器读取和管理这些 WAV 文件
 */

#include "product_def.h"

#if FAT32_EN && HW_DRV_FLASH_NAND_EN

#include "looper_wav_export.h"
#include "audio_looper.h"
#include "fat32.h"
#include "fat32_nand.h"
#include "flash_devices.h"
#include "err_handle.h"
#include <string.h>
#include <stdio.h>

/* ============================================
 * 内部变量
 * ============================================ */

static bool g_wav_export_initialized = false;

/* WAV 导出缓冲区 (4KB = 1024 采样 × 4 字节) */
#define WAV_EXPORT_BUFFER_SIZE  4096
static uint8_t g_wav_buffer[WAV_EXPORT_BUFFER_SIZE];

/* ============================================
 * 内部辅助函数
 * ============================================ */

/**
 * @brief 创建 WAV 文件头
 */
static void create_wav_header(WAV_Header_t *header, uint32_t data_size)
{
    uint32_t byte_rate;
    uint16_t block_align;
    
    /* 计算参数 */
    block_align = (uint16_t)(LOOPER_WAV_CHANNELS * LOOPER_WAV_BIT_DEPTH / 8);
    byte_rate = LOOPER_WAV_SAMPLE_RATE * block_align;
    
    /* RIFF Chunk */
    memcpy(header->riff_id, "RIFF", 4);
    header->file_size = data_size + sizeof(WAV_Header_t) - 8;
    memcpy(header->wave_id, "WAVE", 4);
    
    /* fmt Chunk */
    memcpy(header->fmt_id, "fmt ", 4);
    header->fmt_size = 16;
    header->audio_format = 1;  /* PCM */
    header->num_channels = LOOPER_WAV_CHANNELS;
    header->sample_rate = LOOPER_WAV_SAMPLE_RATE;
    header->byte_rate = byte_rate;
    header->block_align = block_align;
    header->bits_per_sample = LOOPER_WAV_BIT_DEPTH;
    
    /* data Chunk */
    memcpy(header->data_id, "data", 4);
    header->data_size = data_size;
}

/**
 * @brief 生成自动文件名
 */
static int generate_filename(const char *prefix, char *buffer, uint32_t buflen)
{
    int i;
    BG_ERR ret;
    FAT32_FileInfo_t info;
    
    /* 尝试文件名: looper_segX_001.wav ~ looper_segX_999.wav */
    for (i = 1; i < 1000; i++) {
        int len = snprintf(buffer, buflen, "%s_%s_%03d.wav", LOOPER_WAV_PREFIX, prefix, i);
        if (len < 0 || (uint32_t)len >= buflen) {
            return -1;
        }
        
        /* 检查文件是否存在 */
        ret = FAT32_FindFile(buffer, &info);
        if (ret != SUCCESS) {
            /* 文件不存在，使用这个文件名 */
            return 0;
        }
    }
    
    return -1;  /* 无可用文件名 */
}

/**
 * @brief 转换 32-bit Q24 定点数音频为 16-bit PCM
 * @param src    源数据 (32-bit Q24 定点数)
 * @param dst    目标数据 (16-bit PCM)
 * @param count  采样数量
 */
static void convert_q24_to_pcm16(const uint32_t *src, int16_t *dst, uint32_t count)
{
    uint32_t i;
    int32_t sample;
    int32_t pcm;
    
    for (i = 0; i < count; i++) {
        /* Q24 格式: 32位整数，小数点在第24位之后
         * 转换为 16-bit PCM: 右移 8 位取高 24 位中的 16 位 */
        sample = (int32_t)src[i];
        pcm = sample >> 8;  /* 右移 8 位，保留符号 */
        
        /* 限幅到 16-bit 范围 */
        if (pcm > 32767) pcm = 32767;
        if (pcm < -32768) pcm = -32768;
        
        dst[i] = (int16_t)pcm;
    }
}

/* ============================================
 * 公共接口实现
 * ============================================ */

BG_ERR LooperWAV_Init(void)
{
    BG_ERR ret;
    
    if (g_wav_export_initialized) {
        return SUCCESS;
    }
    
    /* 确保 FAT32 NAND 已初始化 */
    ret = FAT32_NAND_Init();
    if (ret != SUCCESS) {
        return ret;
    }
    
    /* 创建录音目录 (如果不存在) */
    ret = FAT32_MkDir(0, LOOPER_WAV_DIR + 1);  /* 跳过开头的 '/' */
    /* 忽略目录已存在的错误 */
    
    g_wav_export_initialized = true;
    return SUCCESS;
}

BG_ERR LooperWAV_ExportSegment(uint8_t segment_index, const char *filename)
{
    BG_ERR ret;
    WAV_Header_t wav_header;
    char auto_filename[64];
    char full_path[96];
    const char *use_filename;
    uint32_t segment_length_samples;
    uint32_t segment_length_bytes;
    uint32_t flash_offset;
    uint32_t total_bytes_written;
    uint32_t remain_samples;
    uint32_t chunk_samples;
    uint32_t *flash_buf_32;
    int16_t *pcm_buf_16;
    
    if (!g_wav_export_initialized) {
        return -1;
    }
    
    if (segment_index >= MAX_SEGMENTS) {
        return -1;
    }
    
    /* 获取段长度 (从 AudioLooper 接口) */
    segment_length_samples = AudioLooper.GetRecordLength();
    if (segment_length_samples == 0) {
        return -1;  /* 段为空 */
    }
    
    /* 生成或使用指定文件名 */
    if (filename == NULL) {
        char prefix[16];
        snprintf(prefix, sizeof(prefix), "seg%d", segment_index);
        if (generate_filename(prefix, auto_filename, sizeof(auto_filename)) < 0) {
            return -1;
        }
        use_filename = auto_filename;
    } else {
        use_filename = filename;
    }
    
    /* 构建完整路径 */
    snprintf(full_path, sizeof(full_path), "%s/%s", LOOPER_WAV_DIR + 1, use_filename);
    
    /* 计算 WAV 数据大小 (立体声 × 16-bit) */
    segment_length_bytes = segment_length_samples * LOOPER_WAV_CHANNELS * (LOOPER_WAV_BIT_DEPTH / 8);
    
    /* 创建 WAV 头 */
    create_wav_header(&wav_header, segment_length_bytes);
    
    /* 写入 WAV 头到文件 */
    ret = FAT32_WriteFile(0, full_path, &wav_header, sizeof(wav_header));
    if (ret < 0) {
        return -1;
    }
    
    /* 逐块读取 Flash 并转换写入 WAV 数据 */
    flash_buf_32 = (uint32_t *)g_wav_buffer;
    pcm_buf_16 = (int16_t *)g_wav_buffer;
    chunk_samples = WAV_EXPORT_BUFFER_SIZE / sizeof(uint32_t);
    
    flash_offset = 0;
    total_bytes_written = sizeof(wav_header);
    remain_samples = segment_length_samples;
    
    while (remain_samples > 0) {
        uint32_t read_samples = (remain_samples < chunk_samples) ? remain_samples : chunk_samples;
        uint32_t read_bytes = read_samples * sizeof(uint32_t);
        uint32_t pcm_bytes;
        uint32_t i;
        
        /* 从 Flash 读取原始 Q24 数据 */
        ret = FlashPartition_LooperRead(flash_offset, (uint8_t *)flash_buf_32, read_bytes);
        if (ret != SUCCESS) {
            return -1;
        }
        
        /* 转换为 16-bit PCM */
        convert_q24_to_pcm16(flash_buf_32, pcm_buf_16, read_samples);
        
        /* 如果是立体声，复制左声道到右声道 */
        if (LOOPER_WAV_CHANNELS == 2) {
            for (i = read_samples; i > 0; i--) {
                pcm_buf_16[(i - 1) * 2 + 1] = pcm_buf_16[i - 1];
                pcm_buf_16[(i - 1) * 2] = pcm_buf_16[i - 1];
            }
        }
        
        /* 追加写入 WAV 文件 */
        pcm_bytes = read_samples * LOOPER_WAV_CHANNELS * sizeof(int16_t);
        ret = FAT32_AppendFile(0, full_path, pcm_buf_16, pcm_bytes);
        if (ret < 0) {
            return -1;
        }
        
        flash_offset += read_bytes;
        remain_samples -= read_samples;
        total_bytes_written += pcm_bytes;
    }
    
    return SUCCESS;
}

BG_ERR LooperWAV_ExportMix(const char *filename)
{
    /* TODO: 实现混音导出 */
    (void)filename;
    return -1;
}

BG_ERR LooperWAV_DeleteFile(const char *filename)
{
    char full_path[96];
    
    if (!g_wav_export_initialized || filename == NULL) {
        return -1;
    }
    
    snprintf(full_path, sizeof(full_path), "%s/%s", LOOPER_WAV_DIR + 1, filename);
    return FAT32_DeleteFile(0, full_path);
}

int LooperWAV_ListFiles(char *buffer, uint32_t buffer_len)
{
    if (!g_wav_export_initialized || buffer == NULL) {
        return -1;
    }
    
    buffer[0] = '\0';
    return 0;  /* TODO: 实现文件列表遍历 */
}

uint32_t LooperWAV_GetFreeSpace(void)
{
    if (!g_wav_export_initialized) {
        return 0;
    }
    
    return FAT32_NAND_GetFreeSpace();
}

BG_ERR LooperWAV_FormatNAND(void)
{
    BG_ERR ret;
    
    ret = FAT32_NAND_Format();
    if (ret != SUCCESS) {
        return ret;
    }
    
    /* 重新初始化 */
    g_wav_export_initialized = false;
    return LooperWAV_Init();
}

#endif /* FAT32_EN && HW_DRV_FLASH_NAND_EN */
