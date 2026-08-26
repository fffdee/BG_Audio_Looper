/**相机
 * @file sampler.c
 * @brief BanGTsynth 采样器模块实现
 *
 * 实现即兴音色合成功能：
 *   - ADC 录制采样
 *   - WAV 文件加载
 *   - 移调播放 (pitch shift)
 *   - 简易包络控制
 */

#include "bg_config.h"

#if SYNTH_SD_NAND_PSRAM_EN || BANGTSYNTH_EN

#include "sampler.h"
#include "../fat32/psram_heap.h"
#if FAT32_EN
#include "../fat32/fat32_reader.h"
#endif
#include "bg_log.h"
#include <string.h>

/* ============================================
 * 内部常量
 * ============================================ */

/** 简易包络参数 */
#define SAMPLER_ATTACK_TIME_MS   10    /* 起音时间 */
#define SAMPLER_RELEASE_TIME_MS  50    /* 释放时间 */

/** 移调计算: 12 个半音的 16.16 定点频率比率 (index 0-11) */
static const uint32_t semitone_ratio_table[12] = {
    65536,  /* 0:  2^(0/12)  = 1.00000 */
    69433,  /* 1:  2^(1/12)  = 1.05946 */
    73562,  /* 2:  2^(2/12)  = 1.12246 */
    77936,  /* 3:  2^(3/12)  = 1.18921 */
    82570,  /* 4:  2^(4/12)  = 1.25992 */
    87480,  /* 5:  2^(5/12)  = 1.33484 */
    92682,  /* 6:  2^(6/12)  = 1.41421 */
    98193,  /* 7:  2^(7/12)  = 1.49831 */
    104032, /* 8:  2^(8/12)  = 1.58740 */
    110218, /* 9:  2^(9/12)  = 1.68179 */
    116772, /* 10: 2^(10/12) = 1.78180 */
    123715  /* 11: 2^(11/12) = 1.88775 */
};

/** PSRAM 预取缓冲区大小 (采样点数, 共享给所有声部, 逐声部使用) */
#define SAMPLER_PREFETCH_SAMPLES  512

/* ============================================
 * 全局状态
 * ============================================ */

static Sampler_State_t g_sampler_state = {0};

/** PSRAM 预取缓冲区 (SRAM, 声部处理时共享复用) */
static int16_t g_prefetch_buf[SAMPLER_PREFETCH_SAMPLES];
static uint32_t g_prefetch_start;   /* 缓冲区对应的起始采样点索引 */
static uint32_t g_prefetch_count;   /* 缓冲区中有效采样点数 */
static psram_ptr_t g_prefetch_addr; /* 缓冲区对应的 PSRAM 基地址 */

/** WAV 原始数据读取缓冲区 (加载时使用, 最大 bytes_per_frame×512 = 2048B) */
#if FAT32_EN && (SAMPLER_WAV_STEREO_EN || SAMPLER_WAV_8BIT_EN)
static uint8_t g_wav_raw_buf[SAMPLER_PREFETCH_SAMPLES * 4]; /* 支持 stereo16 最大帧 */
#endif

/* ============================================
 * 内部函数声明
 * ============================================ */

#if FAT32_EN
static BG_ERR sampler_load_wav_from_file(uint8_t slot, const char *filename, uint8_t root_key);
static BG_ERR sampler_parse_wav_header(const uint8_t *header, uint32_t header_size,
                                       uint32_t *data_offset, uint32_t *data_size,
                                       uint32_t *sample_rate, uint16_t *bit_depth,
                                       uint8_t *channels);
static void   sampler_convert_frames(const uint8_t *raw_in, int16_t *pcm_out,
                                     uint32_t num_frames,
                                     uint8_t channels, uint8_t bit_depth);
#endif /* FAT32_EN */
static BG_ERR sampler_allocate_psram(uint32_t size, uint32_t *psram_addr);
static void sampler_free_psram(uint32_t psram_addr, uint32_t size);
static uint32_t sampler_calculate_increment(uint8_t root_key, uint8_t target_note);
static int16_t sampler_interpolate_sample(psram_ptr_t data_addr, uint32_t num_samples,
                                          uint32_t position_fixed);
static void sampler_update_envelope(Sampler_Voice_t *voice);
static Sampler_Voice_t* sampler_find_free_voice(void);
static Sampler_Voice_t* sampler_find_voice_by_note(uint8_t note);
static void sampler_prefetch_load(psram_ptr_t base_addr, uint32_t start_sample,
                                  uint32_t total_samples);

#if FAT32_EN
/* ============================================
 * WAV 文件格式结构
 * ============================================ */

typedef struct {
    char     riff_id[4];        /* "RIFF" */
    uint32_t riff_size;         /* 文件大小 - 8 */
    char     wave_id[4];        /* "WAVE" */
} __attribute__((packed)) WAV_RIFF_Header_t;

typedef struct {
    char     chunk_id[4];       /* "fmt " */
    uint32_t chunk_size;        /* 格式块大小 */
    uint16_t audio_format;      /* 音频格式 (1=PCM) */
    uint16_t num_channels;      /* 通道数 */
    uint32_t sample_rate;       /* 采样率 */
    uint32_t byte_rate;         /* 字节率 */
    uint16_t block_align;       /* 块对齐 */
    uint16_t bits_per_sample;   /* 位深 */
} __attribute__((packed)) WAV_FMT_Chunk_t;
#endif /* FAT32_EN */

/* ============================================
 * 公开接口实现
 * ============================================ */

BG_ERR Sampler_Init(void)
{
    BG_ERR ret;

    if (g_sampler_state.initialized) {
        return SUCCESS;
    }

    memset(&g_sampler_state, 0, sizeof(Sampler_State_t));

    /* 初始化 PSRAM 堆管理器 (如果未初始化) */
    if (!PSRAM_HeapIsInitialized()) {
        ret = PSRAM_HeapInit();
        if (ret != SUCCESS) {
            BG_LOG_E(BG_LOG_TAG_SYNTH, "Failed to init PSRAM heap for sampler");
            return ret;
        }
    }

    g_sampler_state.initialized = true;
    BG_LOG_I(BG_LOG_TAG_SYNTH, "Sampler initialized");

    return SUCCESS;
}

void Sampler_DeInit(void)
{
    uint8_t i;

    if (!g_sampler_state.initialized) {
        return;
    }

    /* 停止录制 */
    if (g_sampler_state.rec_state == SAMPLER_REC_RECORDING) {
        Sampler_StopRecord();
    }

    /* 释放所有采样数据 */
    for (i = 0; i < SAMPLER_MAX_TIMBRES; i++) {
        Sampler_ClearSlot(i);
    }

    /* 停止所有声部 */
    Sampler_AllNoteOff();

    g_sampler_state.initialized = false;
    BG_LOG_I(BG_LOG_TAG_SYNTH, "Sampler deinitialized");
}

/* ============================================
 * 采样加载
 * ============================================ */

#if FAT32_EN
BG_ERR Sampler_LoadWAV(uint8_t slot, const char *filename, uint8_t root_key)
{
    if (!g_sampler_state.initialized) {
        return ENABLE_DEVICE_NOT_READY;
    }

    if (slot >= SAMPLER_MAX_TIMBRES) {
        return ENABLE_INVALID_INPUT;
    }

    return sampler_load_wav_from_file(slot, filename, root_key);
}
#endif /* FAT32_EN */

BG_ERR Sampler_LoadFromMemory(uint8_t slot, const int16_t *pcm_data,
                              uint32_t num_samples, uint32_t sample_rate,
                              uint8_t root_key)
{
    Sampler_SampleDesc_t *desc;
    BG_ERR ret;
    uint32_t psram_addr;
    uint32_t data_size;

    if (!g_sampler_state.initialized) {
        return ENABLE_DEVICE_NOT_READY;
    }

    if (slot >= SAMPLER_MAX_TIMBRES || !pcm_data || num_samples == 0) {
        return ENABLE_INVALID_INPUT;
    }

    /* 先清空现有数据 */
    Sampler_ClearSlot(slot);

    desc = &g_sampler_state.timbres[slot];
    data_size = num_samples * sizeof(int16_t);

    /* 分配 PSRAM 空间 */
    ret = sampler_allocate_psram(data_size, &psram_addr);
    if (ret != SUCCESS) {
        return ret;
    }

    /* 复制数据到 PSRAM */
    ret = PSRAM_HeapWrite(psram_addr, (const uint8_t *)pcm_data, data_size);
    if (ret != SUCCESS) {
        sampler_free_psram(psram_addr, data_size);
        return ret;
    }

    /* 填充描述符 */
    desc->source = SAMPLER_SRC_MEMORY;
    desc->root_key = root_key;
    desc->sample_rate = sample_rate;
    desc->bit_depth = 16;
    desc->channels = 1;
    desc->num_samples = num_samples;
    desc->data_size = data_size;
    desc->loop_start = 0;
    desc->loop_end = 0;
    desc->psram_addr = psram_addr;
    strcpy(desc->name, "Memory Sample");

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Loaded memory sample to slot %u: %u samples", slot, num_samples);
    return SUCCESS;
}

void Sampler_ClearSlot(uint8_t slot)
{
    Sampler_SampleDesc_t *desc;

    if (slot >= SAMPLER_MAX_TIMBRES) {
        return;
    }

    desc = &g_sampler_state.timbres[slot];

    if (desc->psram_addr != 0) {
        sampler_free_psram(desc->psram_addr, desc->data_size);
    }

    memset(desc, 0, sizeof(Sampler_SampleDesc_t));
}

/* ============================================
 * ADC 录制
 * ============================================ */

BG_ERR Sampler_StartRecord(uint8_t slot, uint8_t root_key)
{
    BG_ERR ret;
    uint32_t psram_addr;

    if (!g_sampler_state.initialized) {
        return ENABLE_DEVICE_NOT_READY;
    }

    if (slot >= SAMPLER_MAX_TIMBRES) {
        return ENABLE_INVALID_INPUT;
    }

    if (g_sampler_state.rec_state != SAMPLER_REC_IDLE) {
        return ENABLE_BUSY;
    }

    /* 清空目标 slot */
    Sampler_ClearSlot(slot);

    /* 分配录制缓冲区 */
    ret = sampler_allocate_psram(SAMPLER_MAX_SAMPLE_SIZE, &psram_addr);
    if (ret != SUCCESS) {
        return ret;
    }

    /* 初始化录制状态 */
    g_sampler_state.rec_state = SAMPLER_REC_RECORDING;
    g_sampler_state.rec_target_slot = slot;
    g_sampler_state.rec_position = 0;
    g_sampler_state.rec_psram_addr = psram_addr;

    /* 填充采样描述符 (录制完成后更新) */
    g_sampler_state.timbres[slot].source = SAMPLER_SRC_ADC;
    g_sampler_state.timbres[slot].root_key = root_key;
    g_sampler_state.timbres[slot].sample_rate = SAMPLER_SAMPLE_RATE;
    g_sampler_state.timbres[slot].bit_depth = SAMPLER_BIT_DEPTH;
    g_sampler_state.timbres[slot].channels = SAMPLER_CHANNELS;
    g_sampler_state.timbres[slot].psram_addr = psram_addr;
    strcpy(g_sampler_state.timbres[slot].name, "ADC Recording");

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Started ADC recording to slot %u", slot);
    return SUCCESS;
}

BG_ERR Sampler_StopRecord(void)
{
    Sampler_SampleDesc_t *desc;
    uint32_t actual_samples;

    if (g_sampler_state.rec_state != SAMPLER_REC_RECORDING) {
        return ENABLE_INVALID_INPUT;
    }

    /* 完成录制 */
    g_sampler_state.rec_state = SAMPLER_REC_DONE;

    desc = &g_sampler_state.timbres[g_sampler_state.rec_target_slot];
    actual_samples = g_sampler_state.rec_position;

    if (actual_samples == 0) {
        /* 无数据，释放资源 */
        Sampler_ClearSlot(g_sampler_state.rec_target_slot);
        BG_LOG_W(BG_LOG_TAG_SYNTH, "Recording stopped with no data");
        return ENABLE_OPERATION_FAILED;
    }

    /* 更新描述符 */
    desc->num_samples = actual_samples;
    desc->data_size = actual_samples * sizeof(int16_t);
    desc->loop_start = 0;
    desc->loop_end = 0;

    /* 释放多余的 PSRAM 空间 (简化实现，实际保持分配) */

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Recording completed: %u samples", actual_samples);
    return SUCCESS;
}

void Sampler_FeedADC(const int16_t *samples, uint32_t count)
{
    BG_ERR ret;
    uint32_t write_addr;
    uint32_t remaining_space;

    if (g_sampler_state.rec_state != SAMPLER_REC_RECORDING) {
        return;
    }

    /* 检查空间是否足够 */
    remaining_space = SAMPLER_MAX_SAMPLE_SIZE - (g_sampler_state.rec_position * sizeof(int16_t));
    if (remaining_space < count * sizeof(int16_t)) {
        count = remaining_space / sizeof(int16_t);
        if (count == 0) {
            /* 空间不足，自动停止录制 */
            Sampler_StopRecord();
            return;
        }
    }

    /* 写入 PSRAM */
    write_addr = g_sampler_state.rec_psram_addr + (g_sampler_state.rec_position * sizeof(int16_t));
    ret = PSRAM_HeapWrite(write_addr, (const uint8_t *)samples, count * sizeof(int16_t));
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "Failed to write ADC data to PSRAM");
        Sampler_StopRecord();
        return;
    }

    g_sampler_state.rec_position += count;

    /* 检查是否达到最大时长 */
    if (g_sampler_state.rec_position >= SAMPLER_MAX_DURATION_SEC * SAMPLER_SAMPLE_RATE) {
        Sampler_StopRecord();
    }
}

Sampler_RecState_t Sampler_GetRecordState(void)
{
    return g_sampler_state.rec_state;
}

uint8_t Sampler_GetRecordProgress(void)
{
    uint32_t max_samples;
    uint32_t progress;

    if (g_sampler_state.rec_state == SAMPLER_REC_IDLE) {
        return 0;
    }

    max_samples = SAMPLER_MAX_DURATION_SEC * SAMPLER_SAMPLE_RATE;
    progress = (g_sampler_state.rec_position * 100) / max_samples;

    return (progress > 100) ? 100 : (uint8_t)progress;
}

/* ============================================
 * 音符控制
 * ============================================ */

void Sampler_SelectTimbre(uint8_t slot)
{
    if (slot < SAMPLER_MAX_TIMBRES) {
        g_sampler_state.active_timbre = slot;
    }
}

void Sampler_NoteOn(uint8_t note, uint8_t velocity)
{
    Sampler_Voice_t *voice;
    Sampler_SampleDesc_t *timbre;
    uint32_t increment;

    if (!g_sampler_state.initialized || velocity == 0) {
        return;
    }

    timbre = &g_sampler_state.timbres[g_sampler_state.active_timbre];
    if (timbre->source == SAMPLER_SRC_NONE || timbre->num_samples == 0) {
        return;
    }

    /* 查找空闲声部 */
    voice = sampler_find_free_voice();
    if (!voice) {
        BG_LOG_W(BG_LOG_TAG_SYNTH, "No free voice for note %u", note);
        return;
    }

    /* 计算移调比率 */
    increment = sampler_calculate_increment(timbre->root_key, note);

#if SAMPLER_WAV_RESAMPLE_EN
    /* 采样率补偿: increment × (wav_sr / playback_sr)
     * 例: 22050Hz WAV → increment/2, 48000Hz WAV → increment×48000/44100 */
    if (timbre->sample_rate != 0 && timbre->sample_rate != SAMPLER_SAMPLE_RATE) {
        unsigned long long scaled =
            (unsigned long long)increment * timbre->sample_rate
            + SAMPLER_SAMPLE_RATE / 2U;
        increment = (uint32_t)(scaled / SAMPLER_SAMPLE_RATE);
    }
#endif

    /* 初始化声部 */
    voice->state = SAMPLER_VOICE_ATTACK;
    voice->note = note;
    voice->velocity = velocity;
    voice->timbre_slot = g_sampler_state.active_timbre;
    voice->position_fixed = 0;  /* 从头开始播放 */
    voice->increment_fixed = increment;
    voice->envelope = 0;  /* 起音从 0 开始 */
    voice->release_rate = 65535 / ((SAMPLER_RELEASE_TIME_MS * SAMPLER_SAMPLE_RATE) / 1000);

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Note ON: note=%u, timbre=%u, increment=0x%08X",
             note, g_sampler_state.active_timbre, increment);
}

void Sampler_NoteOff(uint8_t note)
{
    Sampler_Voice_t *voice;

    voice = sampler_find_voice_by_note(note);
    if (voice && voice->state != SAMPLER_VOICE_FREE) {
        voice->state = SAMPLER_VOICE_RELEASE;
        BG_LOG_I(BG_LOG_TAG_SYNTH, "Note OFF: note=%u", note);
    }
}

void Sampler_AllNoteOff(void)
{
    uint8_t i;

    for (i = 0; i < SAMPLER_MAX_VOICES; i++) {
        g_sampler_state.voices[i].state = SAMPLER_VOICE_FREE;
    }
}

/* ============================================
 * 音频输出
 * ============================================ */

uint8_t Sampler_ReadSamples(int16_t *out_buf, uint32_t count)
{
    uint8_t active_voices = 0;
    uint32_t i, j;
    int32_t mixed_sample;

    if (!g_sampler_state.initialized || !out_buf) {
        return 0;
    }

    /* 清空输出缓冲区 */
    memset(out_buf, 0, count * sizeof(int16_t));

    /* 遍历所有声部 */
    for (i = 0; i < SAMPLER_MAX_VOICES; i++) {
        Sampler_Voice_t *voice = &g_sampler_state.voices[i];
        Sampler_SampleDesc_t *timbre;

        if (voice->state == SAMPLER_VOICE_FREE) {
            continue;
        }

        timbre = &g_sampler_state.timbres[voice->timbre_slot];
        if (timbre->source == SAMPLER_SRC_NONE) {
            continue;
        }

        active_voices++;

        /* 预取: 加载当前播放位置附近的采样到 SRAM */
        sampler_prefetch_load((psram_ptr_t)timbre->psram_addr,
                              voice->position_fixed >> 16, timbre->num_samples);

        /* 为每个采样帧生成音频 */
        for (j = 0; j < count; j++) {
            int16_t sample;
            int32_t envelope_sample;

            /* 获取采样点 (带插值) */
            sample = sampler_interpolate_sample((psram_ptr_t)timbre->psram_addr,
                                               timbre->num_samples,
                                               voice->position_fixed);

            /* 应用包络 */
            sampler_update_envelope(voice);
            envelope_sample = (int32_t)sample * voice->envelope / 65535;

            /* 应用力度 */
            envelope_sample = envelope_sample * voice->velocity / 127;

            /* 混合到输出 */
            mixed_sample = out_buf[j] + envelope_sample;

            /* 防止溢出 */
            if (mixed_sample > 32767) mixed_sample = 32767;
            if (mixed_sample < -32768) mixed_sample = -32768;

            out_buf[j] = (int16_t)mixed_sample;

            /* 前进播放位置 */
            voice->position_fixed += voice->increment_fixed;

            /* 检查循环或结束 */
            {
            uint32_t position_int = voice->position_fixed >> 16;
            if (timbre->loop_end > 0 && position_int >= timbre->loop_end) {
                /* 循环 */
                uint32_t loop_length = timbre->loop_end - timbre->loop_start;
                if (loop_length > 0) {
                    position_int = timbre->loop_start +
                                   ((position_int - timbre->loop_start) % loop_length);
                    voice->position_fixed = position_int << 16;
                }
            } else if (position_int >= timbre->num_samples) {
                /* 播放结束 */
                voice->state = SAMPLER_VOICE_FREE;
                break;
            }
            } /* end C89 block for position_int */
        }
    }

    return active_voices;
}

/* ============================================
 * 循环点设置
 * ============================================ */

void Sampler_SetLoop(uint8_t slot, uint32_t loop_start, uint32_t loop_end)
{
    Sampler_SampleDesc_t *desc;

    if (slot >= SAMPLER_MAX_TIMBRES) {
        return;
    }

    desc = &g_sampler_state.timbres[slot];

    if (loop_start >= loop_end || loop_end > desc->num_samples) {
        return;
    }

    desc->loop_start = loop_start;
    desc->loop_end = loop_end;

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Set loop for slot %u: %u-%u", slot, loop_start, loop_end);
}

/* ============================================
 * 查询接口
 * ============================================ */

BG_ERR Sampler_GetSampleDesc(uint8_t slot, Sampler_SampleDesc_t *desc)
{
    if (slot >= SAMPLER_MAX_TIMBRES || !desc) {
        return ENABLE_INVALID_INPUT;
    }

    memcpy(desc, &g_sampler_state.timbres[slot], sizeof(Sampler_SampleDesc_t));
    return SUCCESS;
}

bool Sampler_IsActive(void)
{
    uint8_t i;

    for (i = 0; i < SAMPLER_MAX_VOICES; i++) {
        if (g_sampler_state.voices[i].state != SAMPLER_VOICE_FREE) {
            return true;
        }
    }

    return false;
}

/* ============================================
 * 内部函数实现
 * ============================================ */

#if FAT32_EN
static BG_ERR sampler_load_wav_from_file(uint8_t slot, const char *filename, uint8_t root_key)
{
    FAT32_FileHandle_t file_handle;
    BG_ERR ret;
    uint8_t  header[256];        /* WAV 头部缓冲, 覆盖绝大多数格式 */
    uint32_t data_offset, data_size, sample_rate;
    uint16_t bit_depth;
    uint8_t  channels;
    uint32_t bytes_per_frame;    /* 原始文件每帧字节数 */
    uint32_t num_frames;         /* 总帧数 (= 输出 mono 采样点数) */
    uint32_t out_size;           /* PSRAM 占用字节 (mono 16-bit) */
    uint32_t psram_addr;
    uint32_t bytes_read, header_read;
    uint32_t psram_out_offset;
    uint32_t frames_remaining;
    uint32_t frames_this_chunk;
    uint32_t raw_per_chunk;
    uint32_t avail_raw, avail_frames;
    uint32_t max_frames;
    uint8_t  *raw_input;
    Sampler_SampleDesc_t *desc;

    /* 打开 WAV 文件 */
    ret = FAT32_OpenFile(filename, &file_handle);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "Failed to open WAV: %s", filename);
        return ret;
    }

    /* 读取头部 (256B 覆盖绝大多数 WAV 扩展头) */
    header_read = FAT32_ReadFile(&file_handle, header, sizeof(header));
    if (header_read < 44) {
        FAT32_CloseFile(&file_handle);
        return ENABLE_FORMAT_ERROR;
    }

    /* 解析 WAV 头部 */
    ret = sampler_parse_wav_header(header, header_read, &data_offset, &data_size,
                                   &sample_rate, &bit_depth, &channels);
    if (ret != SUCCESS) {
        FAT32_CloseFile(&file_handle);
        return ret;
    }

    /* ---- 格式兼容性检查 (受宏控制) ---- */
    if (bit_depth != 16 && bit_depth != 8) {
        FAT32_CloseFile(&file_handle);
        BG_LOG_E(BG_LOG_TAG_SYNTH, "WAV: unsupported bit depth %u", (unsigned)bit_depth);
        return ENABLE_FORMAT_ERROR;
    }
#if !SAMPLER_WAV_STEREO_EN
    if (channels != 1) {
        FAT32_CloseFile(&file_handle);
        BG_LOG_E(BG_LOG_TAG_SYNTH, "WAV: stereo not enabled (SAMPLER_WAV_STEREO_EN=0)");
        return ENABLE_FORMAT_ERROR;
    }
#endif
#if !SAMPLER_WAV_8BIT_EN
    if (bit_depth != 16) {
        FAT32_CloseFile(&file_handle);
        BG_LOG_E(BG_LOG_TAG_SYNTH, "WAV: 8-bit not enabled (SAMPLER_WAV_8BIT_EN=0)");
        return ENABLE_FORMAT_ERROR;
    }
#endif

    /* 计算输出大小 (内部统一: mono 16-bit) */
    bytes_per_frame = (uint32_t)channels * ((uint32_t)bit_depth / 8U);
    num_frames      = data_size / bytes_per_frame;
    out_size        = num_frames * sizeof(int16_t);

    if (out_size > SAMPLER_MAX_SAMPLE_SIZE) {
        FAT32_CloseFile(&file_handle);
        BG_LOG_E(BG_LOG_TAG_SYNTH, "WAV too large: %u frames", num_frames);
        return ENABLE_INVALID_INPUT;
    }

    /* 分配 PSRAM 空间 */
    ret = sampler_allocate_psram(out_size, &psram_addr);
    if (ret != SUCCESS) {
        FAT32_CloseFile(&file_handle);
        return ret;
    }

    psram_out_offset = 0;
    frames_remaining = num_frames;

    /* ---- 处理 header 缓冲区中已包含的 PCM 数据 ---- */
    if (data_offset < header_read) {
        avail_raw    = header_read - data_offset;
        avail_frames = avail_raw / bytes_per_frame;
        if (avail_frames > num_frames) avail_frames = num_frames;

        if (avail_frames > 0) {
            sampler_convert_frames(&header[data_offset], g_prefetch_buf,
                                   avail_frames, channels, bit_depth);
            ret = PSRAM_HeapWrite(psram_addr,
                                  (const uint8_t *)g_prefetch_buf,
                                  avail_frames * sizeof(int16_t));
            if (ret != SUCCESS) {
                sampler_free_psram(psram_addr, out_size);
                FAT32_CloseFile(&file_handle);
                return ret;
            }
            psram_out_offset  = avail_frames * sizeof(int16_t);
            frames_remaining -= avail_frames;
        }
    } else if (data_offset > header_read) {
        /* 跳过中间部分 (data chunk 在 256B 以后, 罕见情况) */
        uint32_t skip = data_offset - header_read;
        while (skip > 0) {
            uint32_t n = (skip > (uint32_t)sizeof(header))
                         ? (uint32_t)sizeof(header) : skip;
            bytes_read = FAT32_ReadFile(&file_handle, header, n);
            if (bytes_read == 0) break;
            skip -= bytes_read;
        }
    }

    /* ---- 逐块读取剩余 PCM 数据并转换 ---- */
    while (frames_remaining > 0) {
#if SAMPLER_WAV_STEREO_EN || SAMPLER_WAV_8BIT_EN
        if (channels == 1 && bit_depth == 16) {
            /* 快速路径: 直接读入输出缓冲区, 无需转换 */
            frames_this_chunk = (frames_remaining < SAMPLER_PREFETCH_SAMPLES)
                                 ? frames_remaining : SAMPLER_PREFETCH_SAMPLES;
            raw_per_chunk = frames_this_chunk << 1U;
            raw_input = (uint8_t *)g_prefetch_buf;
        } else {
            /* 转换路径: 先读入原始缓冲区 */
            max_frames = (uint32_t)sizeof(g_wav_raw_buf) / bytes_per_frame;
            if (max_frames > SAMPLER_PREFETCH_SAMPLES) max_frames = SAMPLER_PREFETCH_SAMPLES;
            frames_this_chunk = (frames_remaining < max_frames)
                                 ? frames_remaining : max_frames;
            raw_per_chunk = frames_this_chunk * bytes_per_frame;
            raw_input = g_wav_raw_buf;
        }
#else
        frames_this_chunk = (frames_remaining < SAMPLER_PREFETCH_SAMPLES)
                             ? frames_remaining : SAMPLER_PREFETCH_SAMPLES;
        raw_per_chunk = frames_this_chunk << 1U;
        raw_input = (uint8_t *)g_prefetch_buf;
#endif

        bytes_read = FAT32_ReadFile(&file_handle, raw_input, raw_per_chunk);
        if (bytes_read == 0) break;

        frames_this_chunk = bytes_read / bytes_per_frame;

#if SAMPLER_WAV_STEREO_EN || SAMPLER_WAV_8BIT_EN
        if (channels != 1 || bit_depth != 16) {
            sampler_convert_frames(raw_input, g_prefetch_buf,
                                   frames_this_chunk, channels, bit_depth);
        }
#endif

        ret = PSRAM_HeapWrite(psram_addr + psram_out_offset,
                              (const uint8_t *)g_prefetch_buf,
                              frames_this_chunk * sizeof(int16_t));
        if (ret != SUCCESS) {
            sampler_free_psram(psram_addr, out_size);
            FAT32_CloseFile(&file_handle);
            return ret;
        }

        psram_out_offset  += frames_this_chunk * sizeof(int16_t);
        frames_remaining  -= frames_this_chunk;
    }

    FAT32_CloseFile(&file_handle);

    /* 清空目标 slot, 填充描述符 */
    Sampler_ClearSlot(slot);

    desc = &g_sampler_state.timbres[slot];
    desc->source      = SAMPLER_SRC_WAV_FILE;
    desc->root_key    = root_key;
    desc->sample_rate = sample_rate;    /* 保留原始采样率用于播放速率补偿 */
    desc->bit_depth   = 16;             /* PSRAM 内统一 16-bit */
    desc->channels    = 1;             /* PSRAM 内统一 mono */
    desc->num_samples = num_frames;
    desc->data_size   = out_size;
    desc->loop_start  = 0;
    desc->loop_end    = 0;
    desc->psram_addr  = psram_addr;
    strncpy(desc->name, filename, sizeof(desc->name) - 1);
    desc->name[sizeof(desc->name) - 1] = '\0';

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Loaded WAV: %s [%uch %ubit %uHz] -> %u mono16 frames",
             filename, (unsigned)channels, (unsigned)bit_depth,
             (unsigned)sample_rate, (unsigned)num_frames);

    return SUCCESS;
}

static BG_ERR sampler_parse_wav_header(const uint8_t *header, uint32_t header_size,
                                       uint32_t *data_offset, uint32_t *data_size,
                                       uint32_t *sample_rate, uint16_t *bit_depth,
                                       uint8_t *channels)
{
    const WAV_RIFF_Header_t *riff = (const WAV_RIFF_Header_t *)header;
    const WAV_FMT_Chunk_t *fmt;
    uint32_t offset;
    int found_fmt = 0;

    if (header_size < sizeof(WAV_RIFF_Header_t)) {
        return ENABLE_FORMAT_ERROR;
    }

    /* 检查 RIFF 头部 */
    if (memcmp(riff->riff_id, "RIFF", 4) != 0 ||
        memcmp(riff->wave_id, "WAVE", 4) != 0) {
        return ENABLE_FORMAT_ERROR;
    }

    offset = sizeof(WAV_RIFF_Header_t);

    /* 遍历 chunks, 带边界检查 */
    while (offset + 8 <= header_size) {
        const char *chunk_id = (const char *)&header[offset];
        uint32_t chunk_size = *(const uint32_t *)&header[offset + 4];

        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            if (offset + 8 + sizeof(WAV_FMT_Chunk_t) - 8 > header_size) {
                return ENABLE_FORMAT_ERROR;  /* fmt 块超出缓冲区 */
            }
            fmt = (const WAV_FMT_Chunk_t *)&header[offset];
            if (fmt->audio_format != 1) {  /* 只支持 PCM */
                return ENABLE_FORMAT_ERROR;
            }
            *sample_rate = fmt->sample_rate;
            *bit_depth = fmt->bits_per_sample;
            *channels = fmt->num_channels;
            found_fmt = 1;
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            if (!found_fmt) {
                return ENABLE_FORMAT_ERROR;  /* data 在 fmt 之前 */
            }
            *data_offset = offset + 8;  /* chunk header (id+size) 后就是数据 */
            *data_size = chunk_size;
            return SUCCESS;
        }

        /* 跳到下一个 chunk (chunk_size 需 word 对齐) */
        offset += 8 + chunk_size;
        if (chunk_size & 1) offset++;  /* WAV chunk 按 2 字节对齐 */
    }

    return ENABLE_FORMAT_ERROR;  /* 未找到 data chunk */
}

/**
 * 将原始 PCM 帧 (任意 ch/bit_depth) 转换为 mono int16_t 输出
 * bit_depth=8 : uint8 无符号 (0-255, 中点128)
 * bit_depth=16: int16 有符号小端
 * channels=2  : 取 (L+R)/2 混为单声道
 */
static void sampler_convert_frames(const uint8_t *raw_in, int16_t *pcm_out,
                                   uint32_t num_frames,
                                   uint8_t channels, uint8_t bit_depth)
{
    uint32_t bpf = (uint32_t)channels * ((uint32_t)bit_depth / 8U);
    uint32_t fi;
    int32_t L, R;

    for (fi = 0; fi < num_frames; fi++) {
        const uint8_t *f = raw_in + fi * bpf;

        if (bit_depth == 8) {
            L = ((int32_t)(f[0]) - 128) << 8;
            R = (channels >= 2) ? (((int32_t)(f[1]) - 128) << 8) : L;
        } else {
            /* 16-bit 小端 */
            L = (int16_t)((uint16_t)f[0] | ((uint16_t)f[1] << 8));
            R = (channels >= 2)
                ? (int16_t)((uint16_t)f[2] | ((uint16_t)f[3] << 8))
                : L;
        }

        pcm_out[fi] = (channels >= 2) ? (int16_t)((L + R) >> 1) : (int16_t)L;
    }
}
#endif /* FAT32_EN */

static BG_ERR sampler_allocate_psram(uint32_t size, uint32_t *psram_addr){
    psram_ptr_t addr;

    if (!PSRAM_HeapIsInitialized()) {
        BG_ERR ret = PSRAM_HeapInit();
        if (ret != SUCCESS) {
            return ret;
        }
    }

    addr = PSRAM_HeapAllocTagged(size, "sampler");
    if (addr == PSRAM_HEAP_NULL) {
        return ENABLE_OUT_OF_MEMORY;
    }

    *psram_addr = addr;
    return SUCCESS;
}

static void sampler_free_psram(uint32_t psram_addr, uint32_t size)
{
    PSRAM_HeapFree((psram_ptr_t)psram_addr, size);
}

static uint32_t sampler_calculate_increment(uint8_t root_key, uint8_t target_note)
{
    int semitones;
    int abs_semi, octaves, remainder;
    uint32_t ratio;

    semitones = (int)target_note - (int)root_key;

    /* 限制移调范围 */
    if (semitones > SAMPLER_PITCH_RANGE) semitones = SAMPLER_PITCH_RANGE;
    if (semitones < -SAMPLER_PITCH_RANGE) semitones = -SAMPLER_PITCH_RANGE;

    if (semitones >= 0) {
        octaves = semitones / 12;
        remainder = semitones % 12;
        ratio = semitone_ratio_table[remainder];
        ratio <<= octaves;  /* ×2^octaves */
    } else {
        abs_semi = -semitones;
        octaves = abs_semi / 12;
        remainder = abs_semi % 12;
        if (remainder > 0) {
            /* 例: -5半音 = 下一个八度 + 上7半音 */
            ratio = semitone_ratio_table[12 - remainder];
            octaves++;
        } else {
            ratio = semitone_ratio_table[0];  /* 65536 */
        }
        ratio >>= octaves;  /* ÷2^octaves */
    }

    return ratio;
}

/**
 * 加载预取缓冲区 (从 PSRAM 读取一块采样到 SRAM)
 */
static void sampler_prefetch_load(psram_ptr_t base_addr, uint32_t start_sample,
                                  uint32_t total_samples)
{
    uint32_t avail;
    uint32_t read_count;

    g_prefetch_addr = base_addr;
    g_prefetch_start = start_sample;

    avail = (start_sample < total_samples) ? (total_samples - start_sample) : 0;
    read_count = (avail < SAMPLER_PREFETCH_SAMPLES) ? avail : SAMPLER_PREFETCH_SAMPLES;

    if (read_count > 0) {
        PSRAM_HeapRead(base_addr + start_sample * sizeof(int16_t),
                       g_prefetch_buf, read_count * sizeof(int16_t));
    }
    g_prefetch_count = read_count;
}

/**
 * 从预取缓冲区线性插值读取采样 (纯定点运算, 无 float)
 */
static int16_t sampler_interpolate_sample(psram_ptr_t data_addr, uint32_t num_samples,
                                          uint32_t position_fixed)
{
    uint32_t pos_int = position_fixed >> 16;
    uint32_t fraction = position_fixed & 0xFFFF;
    int16_t s1, s2;
    int32_t result;
    uint32_t local_idx;

    if (pos_int >= num_samples) {
        return 0;
    }

    /* 检查预取缓冲区是否命中 */
    if (data_addr != g_prefetch_addr ||
        pos_int < g_prefetch_start ||
        pos_int + 1 >= g_prefetch_start + g_prefetch_count) {
        /* 缓冲区未命中，重新加载 */
        sampler_prefetch_load(data_addr, pos_int, num_samples);
    }

    local_idx = pos_int - g_prefetch_start;
    s1 = g_prefetch_buf[local_idx];

    if (pos_int + 1 < num_samples && local_idx + 1 < g_prefetch_count) {
        s2 = g_prefetch_buf[local_idx + 1];
    } else {
        s2 = s1;  /* 末尾不插值 */
    }

    /* 16.16 定点线性插值: result = s1 + (s2 - s1) * fraction / 65536 */
    result = (int32_t)s1 + (((int32_t)(s2 - s1) * (int32_t)fraction) >> 16);
    return (int16_t)result;
}

static void sampler_update_envelope(Sampler_Voice_t *voice)
{
    switch (voice->state) {
        case SAMPLER_VOICE_ATTACK:
            /* 快速起音 */
            voice->envelope += 65535 / ((SAMPLER_ATTACK_TIME_MS * SAMPLER_SAMPLE_RATE) / 1000);
            if (voice->envelope >= 65535) {
                voice->envelope = 65535;
                voice->state = SAMPLER_VOICE_SUSTAIN;
            }
            break;

        case SAMPLER_VOICE_SUSTAIN:
            /* 保持最大值 */
            voice->envelope = 65535;
            break;

        case SAMPLER_VOICE_RELEASE:
            /* 释放 */
            if (voice->envelope > voice->release_rate) {
                voice->envelope -= voice->release_rate;
            } else {
                voice->envelope = 0;
                voice->state = SAMPLER_VOICE_FREE;
            }
            break;

        default:
            voice->envelope = 0;
            break;
    }
}

static Sampler_Voice_t* sampler_find_free_voice(void)
{
    uint8_t i;

    for (i = 0; i < SAMPLER_MAX_VOICES; i++) {
        if (g_sampler_state.voices[i].state == SAMPLER_VOICE_FREE) {
            return &g_sampler_state.voices[i];
        }
    }

    return NULL;
}

static Sampler_Voice_t* sampler_find_voice_by_note(uint8_t note)
{
    uint8_t i;

    for (i = 0; i < SAMPLER_MAX_VOICES; i++) {
        if (g_sampler_state.voices[i].state != SAMPLER_VOICE_FREE &&
            g_sampler_state.voices[i].note == note) {
            return &g_sampler_state.voices[i];
        }
    }

    return NULL;
}

#endif /* SYNTH_SD_NAND_PSRAM_EN || BANGTSYNTH_EN */
