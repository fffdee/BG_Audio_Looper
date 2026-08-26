#include "bangtsynth_legacy.h"
#if BANGTSYNTH_LEGACY

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "sf2_parser.h"
#include "soundbank_manager.h"  // 使用存储层接口
#include "bg_config.h"           // 配置宏 (SYNTH_MAX_VOICES等)
#include "bg_log.h"              // 日志接口
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/*
 * NDS32 数据同步屏障 (Data Synchronization Barrier)
 * FreeRTOS NDS32 port 的 vPortYield 只用了 ISB 没有 DSB,
 * 导致跨任务写入可能滞留在写缓冲区。必须手动插入 DSB。
 */
#define NDS32_DSB()  __asm__ volatile("dsb" ::: "memory")

/**
 * SF2 解析器实�?
 * 当前版本: 框架代码，待实现完整功能
 */

/* SF2 RIFF 标识�?*/
#define SF2_RIFF_ID     0x46464952  // "RIFF"
#define SF2_SFBK_ID     0x6B626673  // "sfbk"
#define SF2_LIST_ID     0x5453494C  // "LIST"

/* SF2 �?ID */
#define SF2_INFO_ID     0x4F464E49  // "INFO"
#define SF2_SDTA_ID     0x61746473  // "sdta"
#define SF2_PDTA_ID     0x61746470  // "pdta"
#define SF2_IFIL_ID     0x6C696669  // "ifil" - version
#define SF2_ISNG_ID     0x676E7369  // "isng" - sound engine
#define SF2_INAM_ID     0x6D616E49  // "INAM" - bank name

/* PDTA 子块 ID */
#define SF2_PHDR_ID     0x72646870  // "phdr" - preset headers
#define SF2_PBAG_ID     0x67616270  // "pbag" - preset bags
#define SF2_PMOD_ID     0x646F6D70  // "pmod" - preset modulators
#define SF2_PGEN_ID     0x6E656770  // "pgen" - preset generators
#define SF2_INST_ID     0x74736E69  // "inst" - instruments
#define SF2_IBAG_ID     0x67616269  // "ibag" - instrument bags
#define SF2_IMOD_ID     0x646F6D69  // "imod" - instrument modulators
#define SF2_IGEN_ID     0x6E656769  // "igen" - instrument generators
#define SF2_SHDR_ID     0x72646873  // "shdr" - sample headers
#define SF2_SMPL_ID     0x6C706D73  // "smpl" - sample data

/* SF2 Generator 类型 */
#define GEN_START_ADDRS_OFFSET      0
#define GEN_END_ADDRS_OFFSET        1
#define GEN_STARTLOOP_ADDRS_OFFSET  2
#define GEN_ENDLOOP_ADDRS_OFFSET    3
#define GEN_KEY_RANGE               43
#define GEN_VEL_RANGE               44
#define GEN_SAMPLE_ID               53
#define GEN_INSTRUMENT              41

/* 最大采样数 */
#define MAX_SAMPLES                 256
#define MAX_INSTRUMENTS             128
#define MAX_PRESETS                 128

/* SF2 RIFF 块头结构 */
typedef struct {
    uint32_t chunk_id;      // "RIFF"
    uint32_t chunk_size;    // 文件大小 - 8
    uint32_t format;        // "sfbk"
} __attribute__((packed)) SF2_RIFF_Header;

/* SF2 通用块头 */
typedef struct {
    uint32_t chunk_id;      // 块标�?
    uint32_t chunk_size;    // 块大�?
} __attribute__((packed)) SF2_Chunk_Header;

/* SF2 版本信息 */
typedef struct {
    uint16_t major;
    uint16_t minor;
} __attribute__((packed)) SF2_Version;

/* SF2 样本�?(46 bytes) */
typedef struct {
    char     name[20];          // 样本名称
    uint32_t start;             // 起始位置
    uint32_t end;               // 结束位置
    uint32_t start_loop;        // 循环起始
    uint32_t end_loop;          // 循环结束
    uint32_t sample_rate;       // 采样�?
    uint8_t  original_pitch;    // 原始音高 (MIDI note)
    int8_t   pitch_correction;  // 音高校正 (cents)
    uint16_t sample_link;       // 链接样本
    uint16_t sample_type;       // 样本类型
} __attribute__((packed)) SF2_Sample_Header;

/* SF2 预置�?(38 bytes) */
typedef struct {
    char     name[20];          // 预置名称
    uint16_t preset;            // 预置�?
    uint16_t bank;              // 银行�?
    uint16_t bag_index;         // Bag 索引
    uint32_t library;           // �?
    uint32_t genre;             // 流派
    uint32_t morphology;        // 形�?
} __attribute__((packed)) SF2_Preset_Header;

/* SF2 乐器�?(22 bytes) */
typedef struct {
    char     name[20];          // 乐器名称
    uint16_t bag_index;         // Bag 索引
} __attribute__((packed)) SF2_Instrument_Header;

/* SF2 Bag (4 bytes) */
typedef struct {
    uint16_t gen_index;         // Generator 索引
    uint16_t mod_index;         // Modulator 索引
} __attribute__((packed)) SF2_Bag;

/* SF2 Generator (4 bytes) */
typedef struct {
    uint16_t type;              // Generator 类型
    union {
        int16_t  sword;
        uint16_t uword;
        struct {
            uint8_t lo;
            uint8_t hi;
        } range;
    } amount;
} __attribute__((packed)) SF2_Generator;

/* 采样信息 (类似 bg_read �?Note_Info) */
typedef struct {
    uint8_t  note;              // 中心音符
    uint8_t  original_pitch;    // 原始音高 (MIDI note number)
    uint8_t  min_note;          // 最小音�?
    uint8_t  max_note;          // 最大音�?
    uint8_t  min_vel;           // 最小力�?
    uint8_t  max_vel;           // 最大力�?
    uint32_t start;             // 起始位置
    uint32_t end;               // 结束位置
    uint32_t start_loop;        // 循环起始
    uint32_t end_loop;          // 循环结束
    uint32_t sample_rate;       // 采样�?
} SF2_Sample_Info;

/* 播放状态 (每个声部独立) */
typedef struct {
    double   current_pos;       // 当前播放位置 (浮点用于重采样)
    uint8_t  target_note;       // 目标音符
    SF2_Sample_Info *sample;    // 对应的采样信息
} SF2_Playback_State;

/* 声部 (Voice) — 替代原 g_playback_states[128][128] 节省 ~256KB RAM */
typedef struct {
    volatile uint8_t  active;   // 是否活跃 (volatile: 跨任务可见)
    uint8_t  note;              // MIDI 音符号
    uint8_t  program;           // MIDI 程序号
    SF2_Playback_State state;   // 播放状态
} SF2_Voice;

/* 声部池 (静态数组, 大小由 SYNTH_MAX_VOICES 控制, 默认=BG_MAX_POLYPHONY=8) */
static SF2_Voice g_voices[SYNTH_MAX_VOICES];

/* SF2 Program 数据 */
typedef struct {
    char             name[20];         // 预置名称
    uint8_t          program_index;    // 程序�?
    uint8_t          bank_index;       // 银行�?
    uint16_t         sample_count;     // 采样数量
    SF2_Sample_Info  *samples;         // 采样信息数组
} SF2_Program_Data;

/* SF2 引擎类型 */
typedef enum {
    SF2_ENGINE_UNKNOWN = 0,
#if SYNTH_ENABLE_XFI_ENGINE
    SF2_ENGINE_X_FI,           // Creative X-Fi (29000Hz特殊编码)
#endif
    SF2_ENGINE_STANDARD        // 标准SF2引擎
} SF2_Engine_Type;

/* SF2 解析数据 — 使用 SYNTH_NAME_BUFFER_SIZE / SYNTH_MAX_PROGRAMS 控制大小 */
typedef struct {
    SF2_Version version;
    char bank_name[SYNTH_NAME_BUFFER_SIZE];       // 银行名 (默认32字节, 原256)
    char sound_engine[SYNTH_NAME_BUFFER_SIZE];     // 引擎名 (默认32字节, 原256)
    SF2_Engine_Type engine_type;  // 引擎类型
    uint32_t sdta_offset;           // 样本数据偏移
    uint32_t pdta_offset;           // 预置数据偏移
    uint32_t smpl_offset;           // PCM 数据偏移
    uint32_t smpl_size;             // PCM 数据大小
    
    /* PDTA 解析数据 */
    SF2_Sample_Header   *sample_headers;
    uint16_t            sample_count;
    SF2_Preset_Header   *preset_headers;
    uint16_t            preset_count;
    SF2_Instrument_Header *inst_headers;
    uint16_t            inst_count;
    SF2_Bag             *preset_bags;
    uint16_t            pbag_count;
    SF2_Bag             *inst_bags;
    uint16_t            ibag_count;
    SF2_Generator       *preset_gens;
    uint16_t            pgen_count;
    SF2_Generator       *inst_gens;
    uint16_t            igen_count;
    
    /* 程序映射 — SYNTH_MAX_PROGRAMS 控制上限 (默认16, 原128) */
    SF2_Program_Data    programs[SYNTH_MAX_PROGRAMS];
} SF2_Data;

/* 内部状态 */
static uint32_t g_read_offset = 0;  // 当前读取偏移(替代文件句柄)
static uint8_t g_initialized = 0;
static SF2_Data g_sf2_data;

/* 内部函数声明 */
static BG_ERR sf2_init(const char *filename);
static BG_ERR sf2_deinit(void);
static uint8_t sf2_callback(short *data, uint32_t note, uint32_t count, uint8_t program);

/* 辅助函数 - 使用存储层偏移访问 */
static int storage_read(void *buffer, size_t size);
static int storage_seek(uint32_t offset);
static uint32_t storage_tell(void);
static uint32_t read_fourcc(void);
static uint32_t read_uint32(void);
static uint16_t read_uint16(void);
static BG_ERR parse_riff_header(void);
static BG_ERR parse_info_chunk(uint32_t size);
static BG_ERR parse_sdta_chunk(uint32_t size);
static BG_ERR parse_pdta_chunk(uint32_t size);
static void print_fourcc(const char *name, uint32_t fourcc);
static void build_sample_map(void);
static SF2_Sample_Info* find_sample(uint8_t program, uint8_t note, uint8_t velocity);
static SF2_Engine_Type detect_engine_type(const char *engine_name);

/* 导出接口实例 */
SF2_Parser sf2_parser = {
    .Init = sf2_init,
    .DeInit = sf2_deinit,
    .Callback = sf2_callback
};

/**
 * 重置指定音符的播放状�?(�?MIDI 控制器调�?
 */
/* ============================================
 * 声部池辅助函数
 * ============================================ */

/* 查找已分配给指定 note+program 的声部 */
static SF2_Voice* find_voice(uint8_t note, uint8_t program)
{
    int i;
    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        if (g_voices[i].active && g_voices[i].note == note && g_voices[i].program == program) {
            return &g_voices[i];
        }
    }
    return NULL;
}

/* 分配一个空闲声部槽位，若满则偷用第一个 */
static SF2_Voice* alloc_voice(void)
{
    int i;
    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        if (!g_voices[i].active) {
            return &g_voices[i];
        }
    }
    /* 所有槽位都在用，偷用第 0 个（最早分配的） */
    return &g_voices[0];
}

/* 音符控制接口 */
void sf2_note_on(uint8_t note, uint8_t velocity, uint8_t program)
{
    SF2_Voice *v;
    int slot;
    if (note >= 128 || program >= SYNTH_MAX_PROGRAMS) return;

    /* 如果同一 note+program 已存在，重置播放位置 */
    v = find_voice(note, program);
    if (!v) {
        v = alloc_voice();
    }
    slot = (int)(v - g_voices);
    v->note = note;
    v->program = program;
    v->state.current_pos = 0.0;
    v->state.target_note = note;
    v->state.sample = NULL;  /* 下次回调时重新查找 */
    v->active = 1;

    printf("[SF2] NoteOn: slot=%d note=%u prog=%u active=%u\n",
           slot, note, program, v->active);
    (void)velocity;
}

void sf2_note_off(uint8_t note, uint8_t program)
{
    SF2_Voice *v;
    if (note >= 128 || program >= SYNTH_MAX_PROGRAMS) return;

    v = find_voice(note, program);
    if (v) {
        v->active = 0;
        v->state.current_pos = 0.0;
        v->state.sample = NULL;
    }
}

/**
 * 重置单个音符的播放状�?(保留兼容�?
 */
void sf2_reset_note(uint8_t note, uint8_t program)
{
    sf2_note_off(note, program);
}

/**
 * 重置所有音符的播放状�?
 */
void sf2_reset_all_notes(uint8_t program)
{
    int i;
    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        if (g_voices[i].program == program || program >= 128) {
            g_voices[i].active = 0;
            g_voices[i].state.current_pos = 0.0;
            g_voices[i].state.sample = NULL;
        }
    }
}

/**
 * 读取所有活跃声部的混合音频 (用于跨任务音频合成)
 *
 * 遍历 g_voices[], 对每个活跃声部调用 sf2_callback 读取采样,
 * 混合到 out_buf。此函数可以从不同编译单元调用,
 * 编译器无法缓存 g_voices 状态, 保证跨任务可见性。
 *
 * @param out_buf  输出缓冲区 (int16_t PCM, 将被清零后混合写入)
 * @param count    采样帧数 (建议 <= 48)
 * @return 活跃声部数量 (0 = 无活跃声部)
 */
uint8_t sf2_read_active_samples(short *out_buf, uint32_t count)
{
    uint8_t active_count = 0;
    int i;
    uint32_t j;
    short voice_buf[48];  /* 最大帧长度 */

    if (!g_initialized || !out_buf || count == 0) {
        if (out_buf) memset(out_buf, 0, count * sizeof(short));
        return 0;
    }

    /* 限制单次处理长度 */
    if (count > 48) count = 48;

    memset(out_buf, 0, count * sizeof(short));

    /* 不再需要跨任务内存屏障 — NoteOn/Off 现在在同一任务(主任务)执行 */

    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        if (!g_voices[i].active) continue;

        /* 诊断: 首次发现活跃声部 */
        {
            static uint32_t active_found_count = 0;
            if (active_found_count < 10) {
                printf("[SF2] RAS_ACTIVE: slot=%d note=%u prog=%u\n",
                       i, g_voices[i].note, g_voices[i].program);
                active_found_count++;
            }
        }

        memset(voice_buf, 0, count * sizeof(short));
        {
            uint8_t cb_result = sf2_callback(voice_buf, g_voices[i].note, count, g_voices[i].program);

            /* 诊断: 回调结果 */
            {
                static uint32_t ras_diag_count = 0;
                if (ras_diag_count < 10) {
                    printf("[SF2] RAS_CB: slot=%d note=%u cb_ret=%u buf[0]=%d\n",
                        i, g_voices[i].note, cb_result, (int)voice_buf[0]);
                    ras_diag_count++;
                }
            }

            if (cb_result) {
                /* 混入输出缓冲区 */
                for (j = 0; j < count; j++) {
                    int32_t mixed = (int32_t)out_buf[j] + (int32_t)voice_buf[j];
                    if (mixed > 32767)  mixed = 32767;
                    if (mixed < -32768) mixed = -32768;
                    out_buf[j] = (short)mixed;
                }
                active_count++;
            }
        }
    }

    return active_count;
}

/**
 * 初始化 SF2 解析器
 */
static BG_ERR sf2_init(const char *filename)
{
    (void)filename;  // filename 参数已废弃,从存储层读取
    
    /* 重置读取偏移 */
    g_read_offset = 0;
    
    /* 清空数据结构和声部池 */
    memset(&g_sf2_data, 0, sizeof(SF2_Data));
    memset(g_voices, 0, sizeof(g_voices));
    
    /* 解析 RIFF 头 */
    if (parse_riff_header() != SUCCESS) {
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Invalid RIFF header\n");
        return ENABLE_INVALID_INPUT;
    }
    
    /* 解析各个 LIST 块 */
    uint32_t file_size = soundbank_get_file_size();
    
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Starting chunk parsing, file_size=%u, offset=%u\n", 
             file_size, g_read_offset);
    
    while (g_read_offset < file_size) {
        SF2_Chunk_Header chunk;
        uint32_t chunk_start = storage_tell();
        
        if (storage_read(&chunk, sizeof(SF2_Chunk_Header)) != sizeof(SF2_Chunk_Header)) {
            BG_LOG_W(BG_LOG_TAG_SOUNDBANK, "Failed to read chunk header at offset %u\n", chunk_start);
            break;
        }
        
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Chunk at 0x%X: ID=0x%08X, size=%u\n", 
                 chunk_start, chunk.chunk_id, chunk.chunk_size);
        
        if (chunk.chunk_id == SF2_LIST_ID) {
            uint32_t list_type = read_fourcc();
            uint32_t list_size = chunk.chunk_size - 4;  // 减去 list_type 的4字节
            
            print_fourcc("LIST type", list_type);
            
            if (list_type == SF2_INFO_ID) {
                parse_info_chunk(list_size);
            } else if (list_type == SF2_SDTA_ID) {
                parse_sdta_chunk(list_size);
            } else if (list_type == SF2_PDTA_ID) {
                parse_pdta_chunk(list_size);
            } else {
                BG_LOG_W(BG_LOG_TAG_SOUNDBANK, "Unknown LIST type: 0x%08X\n", list_type);
            }
        }
        
        /* 跳到下一个块 */
        uint32_t next_offset = chunk_start + sizeof(SF2_Chunk_Header) + chunk.chunk_size;
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Seeking to next chunk: 0x%X\n", next_offset);
        storage_seek(next_offset);
        
        /* 对齐 */
        if (chunk.chunk_size & 1) {
            storage_seek(storage_tell() + 1);
        }
    }
    
    /* 构建采样映射表 */
    build_sample_map();
    
    /* 检测引擎类型 */
    detect_engine_type(g_sf2_data.sound_engine);
    
    g_initialized = 1;
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "SF2 parser initialized successfully\n");
    
    return SUCCESS;
}

/**
 * 释放 SF2 资源
 */
static BG_ERR sf2_deinit(void)
{
    /* 释放解析的数据结�?*/
    if (g_sf2_data.sample_headers) {
        free(g_sf2_data.sample_headers);
        g_sf2_data.sample_headers = NULL;
    }
    if (g_sf2_data.preset_headers) {
        free(g_sf2_data.preset_headers);
        g_sf2_data.preset_headers = NULL;
    }
    if (g_sf2_data.inst_headers) {
        free(g_sf2_data.inst_headers);
        g_sf2_data.inst_headers = NULL;
    }
    if (g_sf2_data.preset_bags) {
        free(g_sf2_data.preset_bags);
        g_sf2_data.preset_bags = NULL;
    }
    if (g_sf2_data.inst_bags) {
        free(g_sf2_data.inst_bags);
        g_sf2_data.inst_bags = NULL;
    }
    if (g_sf2_data.preset_gens) {
        free(g_sf2_data.preset_gens);
        g_sf2_data.preset_gens = NULL;
    }
    if (g_sf2_data.inst_gens) {
        free(g_sf2_data.inst_gens);
        g_sf2_data.inst_gens = NULL;
    }
    
    int i;
    
    /* 释放程序采样数据 */
    for (i = 0; i < SYNTH_MAX_PROGRAMS; i++) {
        if (g_sf2_data.programs[i].samples) {
            free(g_sf2_data.programs[i].samples);
            g_sf2_data.programs[i].samples = NULL;
        }
    }
    
    g_initialized = 0;
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Deinitialized\n");
    
    return SUCCESS;
}

/**
 * 读取音频数据回调 — 使用声部池 (voice pool) 模式
 * 不再使用 g_playback_states[128][128], 节省 ~256KB RAM
 */
static uint8_t sf2_callback(short *data, uint32_t note, uint32_t count, uint8_t program)
{
    uint32_t i;
    
    if (!g_initialized || !data) {
        printf("[SF2] cb: NOT_INIT g_init=%u data=%p\n", g_initialized, data);
        return 0;
    }
    
    /* 范围检查 */
    if (note >= 128 || program >= SYNTH_MAX_PROGRAMS) {
        memset(data, 0, count * sizeof(short));
        return 0;
    }
    
    /* 从声部池查找对应的活跃声部 */
    SF2_Voice *v = find_voice((uint8_t)note, program);
    if (!v) {
        /* 无活跃声部, 返回静音 */
        static uint32_t no_voice_count = 0;
        if (++no_voice_count <= 5) {
            BG_LOG_W(BG_LOG_TAG_SOUNDBANK, "sf2_callback: no active voice for note=%u prog=%u (#%u)\n",
                   (unsigned)note, (unsigned)program, no_voice_count);
        }
        /* 确保通过 printf 也能看到 (前10次) */
        if (no_voice_count <= 10) {
            printf("[SF2] NO_VOICE: note=%u prog=%u voices=[%u,%u,%u,%u,%u,%u,%u,%u]\n",
                (unsigned)note, (unsigned)program,
                g_voices[0].active, g_voices[1].active,
                g_voices[2].active, g_voices[3].active,
                g_voices[4].active, g_voices[5].active,
                g_voices[6].active, g_voices[7].active);
        }
        memset(data, 0, count * sizeof(short));
        return 0;
    }
    
    SF2_Playback_State *state = &v->state;
    
    /* 如果没有采样信息, 查找并初始化 */
    if (!state->sample) {
        state->sample = find_sample(program, note, 64);
        if (!state->sample) {
            BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "sf2_callback: find_sample FAILED for note=%u prog=%u, killing voice\n",
                   (unsigned)note, (unsigned)program);
            printf("[SF2] FIND_SAMPLE_FAIL: note=%u prog=%u, voice killed\n",
                (unsigned)note, (unsigned)program);
            memset(data, 0, count * sizeof(short));
            v->active = 0;
            return 0;
        }
        state->current_pos = (double)state->sample->start;
        state->target_note = note;
        
        printf("[SF2] SAMPLE_OK: note=%u prog=%u pitch=%d rate=%d start=%u end=%u\n",
               (unsigned)note, (unsigned)program,
               state->sample->original_pitch, state->sample->sample_rate,
               state->sample->start, state->sample->end);
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Note %d -> Sample: pitch=%d, rate=%d, range=[%d-%d], start=%u end=%u\n",
               note, state->sample->original_pitch, state->sample->sample_rate,
               state->sample->min_note, state->sample->max_note,
               state->sample->start, state->sample->end);
    }
    
    SF2_Sample_Info *sample = state->sample;
    
    /* 安全检查 */
    if (sample->start >= sample->end || sample->end == 0) {
        memset(data, 0, count * sizeof(short));
        return 0;
    }
    
    /* 计算音高比率 */
    double pitch_ratio;
    
#if SYNTH_ENABLE_XFI_ENGINE
    if (g_sf2_data.engine_type == SF2_ENGINE_X_FI && sample->sample_rate == 29000) {
        /* X-Fi引擎特殊处理: 使用range center计算 */
        int range_center = (sample->min_note + sample->max_note) / 2;
        double pitch_shift_from_rate = 12.0 * log2(48000.0 / 29000.0);
        double base_pitch_at_48k = (double)range_center + pitch_shift_from_rate;
        int pitch_diff = (int)state->target_note - (int)round(base_pitch_at_48k);
        pitch_ratio = pow(2.0, (double)pitch_diff / 12.0);
    } else
#endif /* SYNTH_ENABLE_XFI_ENGINE */
    {
        /* 标准SF2引擎: 使用original_pitch和sample_rate */
        int pitch_diff = (int)state->target_note - (int)sample->original_pitch;
        double pitch_shift_ratio = pow(2.0, (double)pitch_diff / 12.0);
        
        double sample_rate_ratio = 1.0;
        if (sample->sample_rate > 0 && sample->sample_rate != 48000) {
            sample_rate_ratio = (double)sample->sample_rate / 48000.0;
        }
        
        pitch_ratio = pitch_shift_ratio * sample_rate_ratio;
    }
    
    /* 读取采样数据并进行音高变换 */
    for (i = 0; i < count; i++) {
        uint32_t pos = (uint32_t)state->current_pos;
        
        if (pos >= sample->end) {
            /* 检查是否有循环 */
            if (sample->start_loop < sample->end_loop && 
                sample->start_loop >= sample->start &&
                sample->end_loop <= sample->end) {
                uint32_t loop_length = sample->end_loop - sample->start_loop;
                state->current_pos = sample->start_loop + 
                    fmod(state->current_pos - sample->start_loop, (double)loop_length);
                pos = (uint32_t)state->current_pos;
            } else {
                /* 播放结束, 释放声部 */
                memset(&data[i], 0, (count - i) * sizeof(short));
                state->current_pos = (double)sample->start;
                state->sample = NULL;
                v->active = 0;
                return 0;
            }
        }
        
        /* 计算文件位置 */
        uint32_t file_pos = g_sf2_data.smpl_offset + pos * 2;
        
        /* 边界检查 */
        if (file_pos + 2 > g_sf2_data.smpl_offset + g_sf2_data.smpl_size) {
            memset(&data[i], 0, (count - i) * sizeof(short));
            state->current_pos = (double)sample->start;
            state->sample = NULL;
            v->active = 0;
            return 0;
        }
        
        /* 读取采样 */
        int16_t sample1;
        if (soundbank_storage_read(file_pos, &sample1, sizeof(int16_t)) != sizeof(int16_t)) {
            data[i] = 0;
        } else {
            data[i] = sample1;
        }
        
        /* 按音高比率增加播放位置 */
        state->current_pos += pitch_ratio;
    }
    
    return 1;
}

/*============================================
 * 辅助函数实现
 *============================================*/

/**
 * 从存储层读取数据
 */
static int storage_read(void *buffer, size_t size)
{
    int result = soundbank_storage_read(g_read_offset, buffer, size);
    if (result > 0) {
        g_read_offset += result;
    }
    return result;
}

/**
 * 设置读取偏移
 */
static int storage_seek(uint32_t offset)
{
    g_read_offset = offset;
    return 0;
}

/**
 * 获取当前偏移
 */
static uint32_t storage_tell(void)
{
    return g_read_offset;
}

/**
 * 读取 FourCC (4字节标识符)
 */
static uint32_t read_fourcc(void)
{
    uint32_t value = 0;
    storage_read(&value, sizeof(uint32_t));
    return value;
}

/**
 * 读取 32位整数
 */
static uint32_t read_uint32(void)
{
    uint32_t value = 0;
    storage_read(&value, sizeof(uint32_t));
    return value;
}

/**
 * 读取 16位整数
 */
static uint16_t read_uint16(void)
{
    uint16_t value = 0;
    storage_read(&value, sizeof(uint16_t));
    return value;
}

/**
 * 打印 FourCC (调试�?
 */
static void print_fourcc(const char *name, uint32_t fourcc)
{
    char str[5];
    str[0] = (fourcc >> 0) & 0xFF;
    str[1] = (fourcc >> 8) & 0xFF;
    str[2] = (fourcc >> 16) & 0xFF;
    str[3] = (fourcc >> 24) & 0xFF;
    str[4] = '\0';
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "%s: '%s' (0x%08X)\n", name, str, fourcc);
}

/**
 * 解析 RIFF 头
 */
static BG_ERR parse_riff_header(void)
{
    SF2_RIFF_Header header;
    
    if (storage_read(&header, sizeof(SF2_RIFF_Header)) != sizeof(SF2_RIFF_Header)) {
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Failed to read RIFF header\n");
        return ENABLE_INVALID_INPUT;
    }
    
    /* 检查 RIFF 标识 */
    if (header.chunk_id != SF2_RIFF_ID) {
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Not a RIFF file (ID: 0x%08X)\n", header.chunk_id);
        return ENABLE_INVALID_INPUT;
    }
    
    /* 检查 sfbk 格式 */
    if (header.format != SF2_SFBK_ID) {
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Not a SoundFont file (format: 0x%08X)\n", header.format);
        return ENABLE_INVALID_INPUT;
    }
    
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "RIFF header OK, size: %u bytes\n", header.chunk_size);
    
    return SUCCESS;
}

/**
 * 解析 INFO 块(版本、银行名称等信息)
 */
static BG_ERR parse_info_chunk(uint32_t size)
{
    uint32_t bytes_read = 0;
    
    while (bytes_read < size) {
        SF2_Chunk_Header chunk;
        uint32_t chunk_start = storage_tell();
        
        if (storage_read(&chunk, sizeof(SF2_Chunk_Header)) != sizeof(SF2_Chunk_Header)) {
            break;
        }
        
        bytes_read += sizeof(SF2_Chunk_Header);
        
        if (chunk.chunk_id == SF2_IFIL_ID) {
            /* SF2 版本 */
            SF2_Version version;
            storage_read(&version, sizeof(SF2_Version));
            BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "SF2 Version: %d.%d\n", version.major, version.minor);
        } else if (chunk.chunk_id == SF2_INAM_ID) {
            /* 音色库名称 */
            if (chunk.chunk_size < sizeof(g_sf2_data.bank_name)) {
                storage_read(g_sf2_data.bank_name, chunk.chunk_size);
                g_sf2_data.bank_name[chunk.chunk_size] = '\0';
                BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Bank Name: %s\n", g_sf2_data.bank_name);
            }
        } else if (chunk.chunk_id == SF2_ISNG_ID) {
            /* 目标声音引擎 */
            if (chunk.chunk_size < sizeof(g_sf2_data.sound_engine)) {
                storage_read(g_sf2_data.sound_engine, chunk.chunk_size);
                g_sf2_data.sound_engine[chunk.chunk_size] = '\0';
                BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Sound Engine: %s\n", g_sf2_data.sound_engine);
            }
        }
        
        /* 跳到下一个子块 */
        storage_seek(chunk_start + sizeof(SF2_Chunk_Header) + chunk.chunk_size);
        bytes_read += chunk.chunk_size;
        
        /* 对齐 */
        if (chunk.chunk_size & 1) {
            storage_seek(storage_tell() + 1);
            bytes_read++;
        }
    }
    
    return SUCCESS;
}

/**
 * 解析 SDTA 块(样本数据)
 */
static BG_ERR parse_sdta_chunk(uint32_t size)
{
    uint32_t bytes_read = 0;
    
    while (bytes_read < size) {
        SF2_Chunk_Header chunk;
        uint32_t chunk_start = storage_tell();
        
        if (storage_read(&chunk, sizeof(SF2_Chunk_Header)) != sizeof(SF2_Chunk_Header)) {
            break;
        }
        
        bytes_read += sizeof(SF2_Chunk_Header);
        
        if (chunk.chunk_id == SF2_SMPL_ID) {
            /* 记录 PCM 样本数据位置和大小 */
            g_sf2_data.smpl_offset = storage_tell();
            g_sf2_data.smpl_size = chunk.chunk_size;
            BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Sample data: offset=0x%X, size=%u bytes\n", 
                   g_sf2_data.smpl_offset, g_sf2_data.smpl_size);
        }
        
        /* 跳到下一个子块 */
        storage_seek(chunk_start + sizeof(SF2_Chunk_Header) + chunk.chunk_size);
        bytes_read += chunk.chunk_size;
        
        /* 对齐 */
        if (chunk.chunk_size & 1) {
            storage_seek(storage_tell() + 1);
            bytes_read++;
        }
    }
    
    return SUCCESS;
}

/**
 * 解析 PDTA 块(预置数据)
 */
static BG_ERR parse_pdta_chunk(uint32_t size)
{
    uint32_t bytes_read = 0;
    
    while (bytes_read < size) {
        SF2_Chunk_Header chunk;
        uint32_t chunk_start = storage_tell();
        
        if (storage_read(&chunk, sizeof(SF2_Chunk_Header)) != sizeof(SF2_Chunk_Header)) {
            break;
        }
        
        bytes_read += sizeof(SF2_Chunk_Header);
        print_fourcc("PDTA sub-chunk", chunk.chunk_id);
        
        if (chunk.chunk_id == SF2_SHDR_ID) {
            /* 样本头 */
            g_sf2_data.sample_count = chunk.chunk_size / sizeof(SF2_Sample_Header);
            g_sf2_data.sample_headers = malloc(chunk.chunk_size);
            if (g_sf2_data.sample_headers) {
                storage_read(g_sf2_data.sample_headers, chunk.chunk_size);
                BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Loaded %d sample headers\n", g_sf2_data.sample_count);
            }
        } else if (chunk.chunk_id == SF2_PHDR_ID) {
            /* 预置头 */
            g_sf2_data.preset_count = chunk.chunk_size / sizeof(SF2_Preset_Header);
            g_sf2_data.preset_headers = malloc(chunk.chunk_size);
            if (g_sf2_data.preset_headers) {
                storage_read(g_sf2_data.preset_headers, chunk.chunk_size);
                BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Loaded %d preset headers\n", g_sf2_data.preset_count);
            }
        } else if (chunk.chunk_id == SF2_INST_ID) {
            /* 乐器头 */
            g_sf2_data.inst_count = chunk.chunk_size / sizeof(SF2_Instrument_Header);
            g_sf2_data.inst_headers = malloc(chunk.chunk_size);
            if (g_sf2_data.inst_headers) {
                storage_read(g_sf2_data.inst_headers, chunk.chunk_size);
                BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Loaded %d instrument headers\n", g_sf2_data.inst_count);
            }
        } else if (chunk.chunk_id == SF2_PBAG_ID) {
            /* 预置 Bags */
            g_sf2_data.pbag_count = chunk.chunk_size / sizeof(SF2_Bag);
            g_sf2_data.preset_bags = malloc(chunk.chunk_size);
            if (g_sf2_data.preset_bags) {
                storage_read(g_sf2_data.preset_bags, chunk.chunk_size);
            }
        } else if (chunk.chunk_id == SF2_IBAG_ID) {
            /* 乐器 Bags */
            g_sf2_data.ibag_count = chunk.chunk_size / sizeof(SF2_Bag);
            g_sf2_data.inst_bags = malloc(chunk.chunk_size);
            if (g_sf2_data.inst_bags) {
                storage_read(g_sf2_data.inst_bags, chunk.chunk_size);
            }
        } else if (chunk.chunk_id == SF2_PGEN_ID) {
            /* 预置 Generators */
            g_sf2_data.pgen_count = chunk.chunk_size / sizeof(SF2_Generator);
            g_sf2_data.preset_gens = malloc(chunk.chunk_size);
            if (g_sf2_data.preset_gens) {
                storage_read(g_sf2_data.preset_gens, chunk.chunk_size);
            }
        } else if (chunk.chunk_id == SF2_IGEN_ID) {
            /* 乐器 Generators */
            g_sf2_data.igen_count = chunk.chunk_size / sizeof(SF2_Generator);
            g_sf2_data.inst_gens = malloc(chunk.chunk_size);
            if (g_sf2_data.inst_gens) {
                storage_read(g_sf2_data.inst_gens, chunk.chunk_size);
            }
        }
        
        /* 跳到下一个子块 */
        storage_seek(chunk_start + sizeof(SF2_Chunk_Header) + chunk.chunk_size);
        bytes_read += chunk.chunk_size;
        
        /* 对齐 */
        if (chunk.chunk_size & 1) {
            storage_seek(storage_tell() + 1);
            bytes_read++;
        }
    }
    
    return SUCCESS;
}

/**
 * 构建采样映射表(类似 bg_read 的音符范围映射)
 */
static void build_sample_map(void)
{
    uint16_t p;
    uint16_t b;
    uint16_t g;
    uint16_t ib;
    uint16_t ig;
    uint16_t sample_idx;
    
    /* 初始化所有程序 */
    memset(g_sf2_data.programs, 0, sizeof(g_sf2_data.programs));
    
    if (!g_sf2_data.preset_headers || !g_sf2_data.sample_headers) {
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Warning: Missing preset or sample data\n");
        return;
    }
    
    /* 遍历所有预置(跳过最后一个终止符) */
    for (p = 0; p < g_sf2_data.preset_count - 1; p++) {
        SF2_Preset_Header *preset = &g_sf2_data.preset_headers[p];
        uint8_t program = preset->preset;
        
        if (program >= SYNTH_MAX_PROGRAMS) continue;
        
        /* 设置程序信息 */
        memcpy(g_sf2_data.programs[program].name, preset->name, 20);
        g_sf2_data.programs[program].program_index = preset->preset;
        g_sf2_data.programs[program].bank_index = preset->bank;
        
        /* 计算该预置的采样数量 */
        uint16_t bag_start = preset->bag_index;
        uint16_t bag_end = g_sf2_data.preset_headers[p + 1].bag_index;
        
        /* 为简化实现，预分配最大采样数 */
        g_sf2_data.programs[program].samples = malloc(sizeof(SF2_Sample_Info) * MAX_SAMPLES);
        if (!g_sf2_data.programs[program].samples) {
            continue;
        }
        
        sample_idx = 0;
        
        /* 遍历预置的所有bags */
        for (b = bag_start; b < bag_end && b < g_sf2_data.pbag_count; b++) {
            SF2_Bag *bag = &g_sf2_data.preset_bags[b];
            uint16_t gen_start = bag->gen_index;
            uint16_t gen_end = (b + 1 < g_sf2_data.pbag_count) ? 
                               g_sf2_data.preset_bags[b + 1].gen_index : g_sf2_data.pgen_count;
            
            /* 查找乐器 ID */
            uint16_t inst_id = 0xFFFF;
            uint8_t key_lo = 0, key_hi = 127;
            uint8_t vel_lo = 0, vel_hi = 127;
            
            for (g = gen_start; g < gen_end && g < g_sf2_data.pgen_count; g++) {
                SF2_Generator *gen = &g_sf2_data.preset_gens[g];
                
                if (gen->type == GEN_INSTRUMENT) {
                    inst_id = gen->amount.uword;
                } else if (gen->type == GEN_KEY_RANGE) {
                    key_lo = gen->amount.range.lo;
                    key_hi = gen->amount.range.hi;
                } else if (gen->type == GEN_VEL_RANGE) {
                    vel_lo = gen->amount.range.lo;
                    vel_hi = gen->amount.range.hi;
                }
            }
            
            /* 如果找到乐器，解析乐器的采样 */
            if (inst_id != 0xFFFF && inst_id < g_sf2_data.inst_count - 1) {
                SF2_Instrument_Header *inst = &g_sf2_data.inst_headers[inst_id];
                uint16_t ibag_start = inst->bag_index;
                uint16_t ibag_end = g_sf2_data.inst_headers[inst_id + 1].bag_index;
                
                /* 遍历乐器的bags */
                for (ib = ibag_start; ib < ibag_end && ib < g_sf2_data.ibag_count; ib++) {
                    SF2_Bag *ibag = &g_sf2_data.inst_bags[ib];
                    uint16_t igen_start = ibag->gen_index;
                    uint16_t igen_end = (ib + 1 < g_sf2_data.ibag_count) ? 
                                       g_sf2_data.inst_bags[ib + 1].gen_index : g_sf2_data.igen_count;
                    
                    uint16_t sample_id = 0xFFFF;
                    uint8_t ikey_lo = key_lo, ikey_hi = key_hi;
                    
                    for (ig = igen_start; ig < igen_end && ig < g_sf2_data.igen_count; ig++) {
                        SF2_Generator *igen = &g_sf2_data.inst_gens[ig];
                        
                        if (igen->type == GEN_SAMPLE_ID) {
                            sample_id = igen->amount.uword;
                        } else if (igen->type == GEN_KEY_RANGE) {
                            ikey_lo = igen->amount.range.lo;
                            ikey_hi = igen->amount.range.hi;
                        }
                    }
                    
                    /* 如果找到样本，添加到程序的采样列�?*/
                    if (sample_id != 0xFFFF && sample_id < g_sf2_data.sample_count && 
                        sample_idx < MAX_SAMPLES) {
                        SF2_Sample_Header *shdr = &g_sf2_data.sample_headers[sample_id];
                        SF2_Sample_Info *sinfo = &g_sf2_data.programs[program].samples[sample_idx];
                        
                        /* 使用SF2文件中的original_pitch，这是采样实际录制的音高
                         * 同时保存音符范围，用于查找时选择最佳采�?
                         */
                        sinfo->note = shdr->original_pitch;
                        sinfo->original_pitch = shdr->original_pitch;
                        sinfo->min_note = ikey_lo;
                        sinfo->max_note = ikey_hi;
                        sinfo->min_vel = vel_lo;
                        sinfo->max_vel = vel_hi;
                        sinfo->start = shdr->start;
                        sinfo->end = shdr->end;
                        sinfo->start_loop = shdr->start_loop;
                        sinfo->end_loop = shdr->end_loop;
                        sinfo->sample_rate = shdr->sample_rate;
                        
                        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "  Sample[%d]: id=%d pitch=%d range=[%d-%d] vel=[%d-%d] start=%u end=%u rate=%d\n",
                               sample_idx, sample_id, shdr->original_pitch,
                               ikey_lo, ikey_hi, vel_lo, vel_hi,
                               shdr->start, shdr->end, shdr->sample_rate);
                        
                        sample_idx++;
                    }
                }
            }
        }
        
        g_sf2_data.programs[program].sample_count = sample_idx;
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Program %d (%s): %d samples\n", 
               program, g_sf2_data.programs[program].name, sample_idx);
    }
}

/**
 * 查找匹配的采�?(类似 bg_read_callback 的查找逻辑)
 * 优先选择original_pitch最接近目标音符的采样，以减少变音幅�?
 */
static SF2_Sample_Info* find_sample(uint8_t program, uint8_t note, uint8_t velocity)
{
    uint16_t i;
    
    if (program >= SYNTH_MAX_PROGRAMS) {
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Invalid program: %d\n", program);
        return NULL;
    }
    
    SF2_Program_Data *prog = &g_sf2_data.programs[program];
    if (!prog->samples || prog->sample_count == 0) {
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Program %d has no samples (count=%d)\n", program, prog->sample_count);
        return NULL;
    }
    
    SF2_Sample_Info *best_match = NULL;
    int min_pitch_diff = 128;  // 最大音程差
    
    /* 遍历所有采样，查找音符和力度范围匹配的，优先选择音高最接近的 */
    for (i = 0; i < prog->sample_count; i++) {
        SF2_Sample_Info *sample = &prog->samples[i];
        
        if (note >= sample->min_note && note <= sample->max_note &&
            velocity >= sample->min_vel && velocity <= sample->max_vel) {
            
            /* 计算音高差距 */
            int pitch_diff = abs((int)note - (int)sample->original_pitch);
            
            /* 选择音高最接近的采�?*/
            if (pitch_diff < min_pitch_diff) {
                min_pitch_diff = pitch_diff;
                best_match = sample;
            }
        }
    }
    
    if (best_match) {
        return best_match;
    }
    
    /* 没有精确匹配，尝试找最接近的(忽略力度) */
    min_pitch_diff = 128;
    for (i = 0; i < prog->sample_count; i++) {
        SF2_Sample_Info *sample = &prog->samples[i];
        
        if (note >= sample->min_note && note <= sample->max_note) {
            int pitch_diff = abs((int)note - (int)sample->original_pitch);
            if (pitch_diff < min_pitch_diff) {
                min_pitch_diff = pitch_diff;
                best_match = sample;
            }
        }
    }
    
    if (best_match) {
        return best_match;
    }
    
    /* 第三轮: 忽略音符范围和力度，选择音高最接近的任意采样 */
    min_pitch_diff = 128;
    for (i = 0; i < prog->sample_count; i++) {
        SF2_Sample_Info *sample = &prog->samples[i];
        int pitch_diff = abs((int)note - (int)sample->original_pitch);
        if (pitch_diff < min_pitch_diff) {
            min_pitch_diff = pitch_diff;
            best_match = sample;
        }
    }
    
    if (best_match) {
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "find_sample fallback: prog=%d note=%d -> sample pitch=%d range=[%d-%d]\n",
               program, note, best_match->original_pitch, best_match->min_note, best_match->max_note);
        return best_match;
    }
    
    BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "No sample found for program %d, note %d, velocity %d (count=%d)\n", 
           program, note, velocity, prog->sample_count);
    return NULL;
}

/**
 * 检测SF2引擎类型
 */
static SF2_Engine_Type detect_engine_type(const char *engine_name)
{
    if (!engine_name || engine_name[0] == '\0') {
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Engine detection: empty name, using STANDARD\n");
        return SF2_ENGINE_STANDARD;
    }
    
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Engine detection: checking '%s'\n", engine_name);
    
#if SYNTH_ENABLE_XFI_ENGINE
    /* 检测X-Fi引擎 */
    if (strstr(engine_name, "X-Fi") != NULL || 
        strstr(engine_name, "x-fi") != NULL ||
        strstr(engine_name, "XFi") != NULL) {
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Engine detection: detected X-Fi!\n");
        return SF2_ENGINE_X_FI;
    }
#endif /* SYNTH_ENABLE_XFI_ENGINE */
    
    /* 默认使用标准SF2处理 */
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Engine detection: using STANDARD\n");
    return SF2_ENGINE_STANDARD;
}

#endif /* BANGTSYNTH_EN */

#endif /* BANGTSYNTH_LEGACY */
