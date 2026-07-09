#include "product_def.h"

#if BANGTSYNTH_EN

#include "sf2_parser.h"
#include "soundbank_manager.h"  // 使用存储层接口
#include "bg_config.h"           // 配置宏 (SYNTH_MAX_VOICES等)
#include "bg_log.h"              // 日志接口
#include "bg_envelope.h"         // ADSR 包络
#include "bg_osal.h"             // 内存屏障, 时间等
#include "midi_info.h"           // BG_MIDI_data (pitch_bend / CC 状态)
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/*
 * 数据同步屏障 — 通过 OSAL 抽象 (替代硬编码 NDS32 DSB)
 */
#define NDS32_DSB()  bg_memory_barrier()

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

/* SF2 Generator 类型 (SF2 spec §8.1.2) */
#define GEN_START_ADDRS_OFFSET      0
#define GEN_END_ADDRS_OFFSET        1
#define GEN_STARTLOOP_ADDRS_OFFSET  2
#define GEN_ENDLOOP_ADDRS_OFFSET    3
#define GEN_START_ADDRS_COARSE      4
#define GEN_MOD_LFO_TO_PITCH       5
#define GEN_VIB_LFO_TO_PITCH       6
#define GEN_MOD_ENV_TO_PITCH        7
#define GEN_INITIAL_FILTER_FC       8
#define GEN_INITIAL_FILTER_Q        9
#define GEN_END_ADDRS_COARSE        12
#define GEN_PAN                     17
#define GEN_DELAY_VOL_ENV           33
#define GEN_ATTACK_VOL_ENV          34
#define GEN_HOLD_VOL_ENV            35
#define GEN_DECAY_VOL_ENV           36
#define GEN_SUSTAIN_VOL_ENV         37
#define GEN_RELEASE_VOL_ENV         38
#define GEN_INSTRUMENT              41
#define GEN_KEY_RANGE               43
#define GEN_VEL_RANGE               44
#define GEN_STARTLOOP_ADDRS_COARSE  45
#define GEN_INITIAL_ATTENUATION     48
#define GEN_ENDLOOP_ADDRS_COARSE    50
#define GEN_COARSE_TUNE             51
#define GEN_FINE_TUNE               52
#define GEN_SAMPLE_ID               53
#define GEN_SAMPLE_MODES            54
#define GEN_SCALE_TUNING            56
#define GEN_EXCLUSIVE_CLASS         57
#define GEN_OVERRIDING_ROOT_KEY     58

/* SF2 sample loop modes (sampleModes generator) */
#define SF2_LOOP_NONE               0
#define SF2_LOOP_CONTINUOUS         1
#define SF2_LOOP_UNUSED             2
#define SF2_LOOP_UNTIL_RELEASE      3

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

/* SF2 Sample Header (shdr, 46 bytes per SF2 spec §7.10) */
typedef struct {
    char     name[20];
    uint32_t start;
    uint32_t end;
    uint32_t start_loop;
    uint32_t end_loop;
    uint32_t sample_rate;
    uint8_t  original_pitch;
    int8_t   pitch_correction;
    uint16_t sample_link;
    uint16_t sample_type;
} __attribute__((packed)) SF2_Sample_Header;

/* SF2 Preset Header (phdr, 38 bytes per SF2 spec §7.2) */
typedef struct {
    char     name[20];
    uint16_t preset;
    uint16_t bank;
    uint16_t bag_index;
    uint32_t library;
    uint32_t genre;
    uint32_t morphology;
} __attribute__((packed)) SF2_Preset_Header;

/* SF2 Instrument Header (inst, 22 bytes per SF2 spec §7.6) */
typedef struct {
    char     name[20];
    uint16_t bag_index;
} __attribute__((packed)) SF2_Instrument_Header;

/* SF2 Bag (pbag/ibag, 4 bytes per SF2 spec §7.3/§7.7) */
typedef struct {
    uint16_t gen_index;
    uint16_t mod_index;
} __attribute__((packed)) SF2_Bag;

/* SF2 Generator Amount (union) */
typedef union {
    struct { uint8_t lo; uint8_t hi; } range;
    int16_t  sword;
    uint16_t uword;
} SF2_GenAmount;

/* SF2 Generator (pgen/igen, 4 bytes per SF2 spec §7.5/§7.9) */
typedef struct {
    uint16_t     type;
    SF2_GenAmount amount;
} __attribute__((packed)) SF2_Generator;

/* SF2 内部采样信息 (从 shdr + generators 合并) */
typedef struct {
    uint8_t  note;              // 音符 (用于查找)
    uint8_t  original_pitch;    // 原始录制音高 (from shdr)
    uint8_t  min_note;          // 最低音符 (key range)
    uint8_t  max_note;          // 最高音符 (key range)
    uint8_t  min_vel;           // 最低力度 (vel range)
    uint8_t  max_vel;           // 最高力度 (vel range)
    uint32_t start;             // 起始位置
    uint32_t end;               // 结束位置
    uint32_t start_loop;        // 循环起始
    uint32_t end_loop;          // 循环结束
    uint32_t sample_rate;       // 采样率
    int8_t   pitch_correction;  // 音高校正 (cents, from shdr)

    /* SF2 Generator 参数 */
    int16_t  attenuation;       // 初始衰减 (centibels, gen 48, 0=full vol)
    int16_t  coarse_tune;       // 粗调 (semitones, gen 51)
    int16_t  fine_tune;         // 微调 (cents, gen 52)
    int16_t  pan;               // 声像 (-500..+500, gen 17, 0=center)
    int16_t  scale_tuning;      // 音阶调律 (cents/key, gen 56, default 100)
    uint16_t sample_modes;      // 循环模式 (gen 54: 0=none, 1=loop, 3=loop+release)
    int16_t  exclusive_class;   // 排他类 (gen 57, 用于 hi-hat choke)
    uint8_t  override_root_key; // 覆盖根键 (gen 58, 255=不覆盖)

    /* SF2 Volume Envelope (timecents: -12000=1ms, 0=1s) */
    int16_t  vol_env_delay;     // 延迟 (gen 33)
    int16_t  vol_env_attack;    // 攻击 (gen 34)
    int16_t  vol_env_hold;      // 保持 (gen 35)
    int16_t  vol_env_decay;     // 衰减 (gen 36)
    int16_t  vol_env_sustain;   // 持续衰减 (gen 37, centibels)
    int16_t  vol_env_release;   // 释放 (gen 38)
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
    uint8_t  velocity;          // 触发力度 (0-127)
    uint8_t  channel;           // MIDI 通道 (0-15)
    uint8_t  sustained;         // 1=延音踏板保持中 (note_off 被延迟)
    float    gain;              // 最终增益 = velocity * attenuation
    BG_Envelope_t envelope;     // ADSR 音量包络
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

/* 当前 MIDI 通道 (由 sf2_set_current_channel 设置, sf2_note_on 读取) */
static uint8_t g_current_channel = 0;

/* BG_MIDI_data 外部引用 (volatile, 定义在 midi_controller.c) */
extern volatile BG_MIDI_Data BG_MIDI_data;

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

/* ============================================
 * SF2 参数转换辅助函数
 * ============================================ */

/**
 * SF2 timecents -> seconds
 * SF2 spec: timecents = 1200 * log2(seconds)
 * -> seconds = 2^(timecents / 1200)
 */
static float sf2_timecents_to_seconds(int16_t tc)
{
    if (tc <= -32768) return 0.0f;
    if (tc <= -12000) return 0.001f;
    if (tc >= 8000)   return 20.0f;
    return powf(2.0f, (float)tc / 1200.0f);
}

/**
 * SF2 centibels -> linear gain
 * gain = 10^(-cb / 200)
 */
static float sf2_cb_to_gain(int16_t cb)
{
    if (cb <= 0) return 1.0f;
    if (cb >= 1440) return 0.0f;
    return powf(10.0f, -(float)cb / 200.0f);
}

/**
 * MIDI velocity -> linear gain (square curve)
 */
static float sf2_velocity_to_gain(uint8_t velocity)
{
    float v;
    if (velocity == 0) return 0.0f;
    if (velocity >= 127) return 1.0f;
    v = (float)velocity / 127.0f;
    return v * v;
}

/**
 * Initialize voice envelope from SF2_Sample_Info parameters
 */
static void sf2_init_voice_envelope(SF2_Voice *v)
{
    BG_EnvParams_t params;
    SF2_Sample_Info *s = v->state.sample;

    if (s && (s->vol_env_attack != 0 || s->vol_env_decay != 0 ||
              s->vol_env_release != 0 || s->vol_env_sustain != 0)) {
        params.attack_time  = sf2_timecents_to_seconds(s->vol_env_attack);
        params.decay_time   = sf2_timecents_to_seconds(s->vol_env_decay);
        params.release_time = sf2_timecents_to_seconds(s->vol_env_release);
        params.sustain_level = sf2_cb_to_gain(s->vol_env_sustain);
    } else {
        params.attack_time  = 0.005f;
        params.decay_time   = 0.1f;
        params.sustain_level = 1.0f;
        params.release_time = 0.2f;
    }

    if (params.attack_time < 0.001f) params.attack_time = 0.001f;
    if (params.decay_time < 0.001f)  params.decay_time = 0.001f;
    if (params.release_time < 0.005f) params.release_time = 0.005f;

    params.curve = BG_ENV_CURVE_EXPONENTIAL;
    BG_Envelope_Init(&v->envelope, &params, BG_SAMPLE_RATE);
    BG_Envelope_Trigger(&v->envelope);
}

/**
 * Handle exclusive class: kill other voices in same class
 */
static void sf2_handle_exclusive_class(uint8_t program, int16_t exc_class)
{
    int i;
    if (exc_class <= 0) return;
    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        if (g_voices[i].active && g_voices[i].program == program &&
            g_voices[i].state.sample &&
            g_voices[i].state.sample->exclusive_class == exc_class) {
            BG_Envelope_Release(&g_voices[i].envelope);
        }
    }
}

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
    SF2_Sample_Info *sample;
    int slot;
    if (note >= 128 || program >= SYNTH_MAX_PROGRAMS || velocity == 0) return;

    /* 预查找采样，用于排他类和增益计算 */
    sample = find_sample(program, note, velocity);

    /* 排他类处理 (如 hi-hat choke) */
    if (sample && sample->exclusive_class > 0) {
        sf2_handle_exclusive_class(program, sample->exclusive_class);
    }

    /* 如果同一 note+program 已存在，重置播放位置 */
    v = find_voice(note, program);
    if (!v) {
        v = alloc_voice();
    }
    slot = (int)(v - g_voices);
    v->note = note;
    v->program = program;
    v->velocity = velocity;
    v->channel = g_current_channel;
    v->sustained = 0;
    v->state.current_pos = 0.0;
    v->state.target_note = note;
    v->state.sample = sample;  /* 预分配采样 */
    v->active = 1;

    /* 计算增益: 力度曲线 * 衰减 */
    v->gain = sf2_velocity_to_gain(velocity);
    if (sample) {
        v->gain *= sf2_cb_to_gain(sample->attenuation);

        /* 初始化采样位置 */
        v->state.current_pos = (double)sample->start;

        /* 初始化 ADSR 包络 */
        sf2_init_voice_envelope(v);
    }

    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "[SF2] NoteOn: slot=%d note=%u vel=%u prog=%u gain=%.3f\n",
           slot, note, velocity, program, v->gain);
}

void sf2_note_off(uint8_t note, uint8_t program)
{
    SF2_Voice *v;
    if (note >= 128 || program >= SYNTH_MAX_PROGRAMS) return;

    v = find_voice(note, program);
    if (v) {
        /* 延音踏板检查: 如果延音踏板开启, 延迟释放 */
        if (BG_MIDI_data.sustain_pedal[v->channel]) {
            v->sustained = 1;
            return;
        }
        /* 进入包络释放阶段，声部保持 active 直到包络完成 */
        BG_Envelope_Release(&v->envelope);
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
        if (g_voices[i].active &&
            (g_voices[i].program == program || program >= 128)) {
            g_voices[i].active = 0;
            BG_Envelope_Reset(&g_voices[i].envelope);
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

        memset(voice_buf, 0, count * sizeof(short));
        {
            uint8_t cb_result = sf2_callback(voice_buf, g_voices[i].note, count, g_voices[i].program);

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
 * 读取音频数据回调 — 声部池模式
 * 支持: 线性插值、ADSR包络、力度增益、sampleModes循环、音高校正
 */
static uint8_t sf2_callback(short *data, uint32_t note, uint32_t count, uint8_t program)
{
    uint32_t i;

    if (!g_initialized || !data) return 0;

    /* 范围检查 */
    if (note >= 128 || program >= SYNTH_MAX_PROGRAMS) {
        memset(data, 0, count * sizeof(short));
        return 0;
    }

    /* 从声部池查找对应的活跃声部 */
    SF2_Voice *v = find_voice((uint8_t)note, program);
    if (!v) {
        memset(data, 0, count * sizeof(short));
        return 0;
    }

    SF2_Playback_State *state = &v->state;

    /* 如果没有采样信息, 查找并初始化 */
    if (!state->sample) {
        state->sample = find_sample(program, note, v->velocity ? v->velocity : 64);
        if (!state->sample) {
            BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "sf2_callback: find_sample FAILED note=%u prog=%u\n",
                   (unsigned)note, (unsigned)program);
            memset(data, 0, count * sizeof(short));
            v->active = 0;
            return 0;
        }
        state->current_pos = (double)state->sample->start;
        state->target_note = note;

        /* 计算增益和初始化包络 (如果 note_on 时没有预查找到) */
        if (v->gain == 0.0f) {
            v->gain = sf2_velocity_to_gain(v->velocity) * sf2_cb_to_gain(state->sample->attenuation);
        }
        sf2_init_voice_envelope(v);

        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Note %d -> pitch=%d rate=%d range=[%d-%d]\n",
               note, state->sample->original_pitch, state->sample->sample_rate,
               state->sample->min_note, state->sample->max_note);
    }

    SF2_Sample_Info *sample = state->sample;

    /* 安全检查 */
    if (sample->start >= sample->end || sample->end == 0) {
        memset(data, 0, count * sizeof(short));
        return 0;
    }

    /* 计算音高比率 (含 coarse_tune, fine_tune, pitch_correction, scale_tuning, pitch_bend) */
    double pitch_ratio;
    {
        /* 音高偏移 (cents) */
        double cents_offset = 0.0;
        int st = (sample->scale_tuning != 0) ? sample->scale_tuning : 100;
        cents_offset += (double)((int)state->target_note - (int)sample->original_pitch) * (double)st;
        cents_offset += (double)sample->coarse_tune * 100.0;
        cents_offset += (double)sample->fine_tune;
        cents_offset += (double)sample->pitch_correction;

        /* Pitch Bend: bend_value * bend_range_semitones * 100 / 8192 */
        {
            int16_t pb = BG_MIDI_data.pitch_bend[v->channel];
            uint8_t br = BG_MIDI_data.bend_range;
            if (br == 0) br = 2;
            cents_offset += (double)pb * (double)(br * 100) / 8192.0;
        }

        double pitch_shift_ratio = pow(2.0, cents_offset / 1200.0);

        double sample_rate_ratio = 1.0;
        if (sample->sample_rate > 0 && sample->sample_rate != BG_SAMPLE_RATE) {
            sample_rate_ratio = (double)sample->sample_rate / (double)BG_SAMPLE_RATE;
        }

        pitch_ratio = pitch_shift_ratio * sample_rate_ratio;
    }

    /* 通道音量和表情 -> 增益系数 */
    float channel_gain;
    {
        uint8_t ch_vol = BG_MIDI_data.Channel_volume[v->channel];
        uint8_t ch_exp = BG_MIDI_data.expression[v->channel];
        channel_gain = ((float)ch_vol / 127.0f) * ((float)ch_exp / 127.0f);
    }

    /* 判断循环是否可用 */
    int has_loop = (sample->start_loop < sample->end_loop &&
                    sample->start_loop >= sample->start &&
                    sample->end_loop <= sample->end);
    int loop_active = has_loop && (sample->sample_modes == SF2_LOOP_CONTINUOUS ||
                     (sample->sample_modes == SF2_LOOP_UNTIL_RELEASE &&
                      v->envelope.stage != BG_ENV_RELEASE && v->envelope.stage != BG_ENV_IDLE));

    /* 读取采样数据：线性插值 + 包络 + 增益 */
    for (i = 0; i < count; i++) {
        /* 包络处理 */
        float env_val = BG_Envelope_Process(&v->envelope);

        /* 包络结束 -> 声部释放 */
        if (!BG_Envelope_IsActive(&v->envelope)) {
            memset(&data[i], 0, (count - i) * sizeof(short));
            v->active = 0;
            v->state.sample = NULL;
            return 0;
        }

        uint32_t pos = (uint32_t)state->current_pos;
        double frac = state->current_pos - (double)pos;

        /* 到达采样结尾 */
        if (pos >= sample->end) {
            if (loop_active) {
                uint32_t loop_length = sample->end_loop - sample->start_loop;
                state->current_pos = (double)sample->start_loop +
                    fmod(state->current_pos - (double)sample->start_loop, (double)loop_length);
                pos = (uint32_t)state->current_pos;
                frac = state->current_pos - (double)pos;
            } else {
                /* 播放结束 */
                memset(&data[i], 0, (count - i) * sizeof(short));
                v->active = 0;
                v->state.sample = NULL;
                return 0;
            }
        }

        /* 循环边界检查 (loop_active 时在 endloop 处跳回) */
        if (loop_active && pos >= sample->end_loop) {
            uint32_t loop_length = sample->end_loop - sample->start_loop;
            state->current_pos = (double)sample->start_loop +
                fmod(state->current_pos - (double)sample->start_loop, (double)loop_length);
            pos = (uint32_t)state->current_pos;
            frac = state->current_pos - (double)pos;
        }

        /* 线性插值: 读取两个相邻采样 */
        uint32_t file_pos1 = g_sf2_data.smpl_offset + pos * 2;
        uint32_t file_pos2 = g_sf2_data.smpl_offset + (pos + 1) * 2;
        uint32_t smpl_end_pos = g_sf2_data.smpl_offset + g_sf2_data.smpl_size;

        if (file_pos1 + 2 > smpl_end_pos) {
            memset(&data[i], 0, (count - i) * sizeof(short));
            v->active = 0;
            v->state.sample = NULL;
            return 0;
        }

        int16_t s1 = 0, s2 = 0;
        soundbank_storage_read(file_pos1, &s1, sizeof(int16_t));
        if (file_pos2 + 2 <= smpl_end_pos) {
            soundbank_storage_read(file_pos2, &s2, sizeof(int16_t));
        } else {
            s2 = s1;
        }

        /* 线性插值 */
        float interpolated = (float)s1 + (float)(s2 - s1) * (float)frac;

        /* 应用包络、力度增益和通道增益 */
        float out_sample = interpolated * env_val * v->gain * channel_gain;

        /* 钳位到 int16 范围 */
        if (out_sample > 32767.0f) out_sample = 32767.0f;
        if (out_sample < -32768.0f) out_sample = -32768.0f;
        data[i] = (short)out_sample;

        /* 推进播放位置 */
        state->current_pos += pitch_ratio;

        /* 动态更新循环状态 (release 阶段关闭 loop-until-release) */
        if (sample->sample_modes == SF2_LOOP_UNTIL_RELEASE &&
            (v->envelope.stage == BG_ENV_RELEASE || v->envelope.stage == BG_ENV_IDLE)) {
            loop_active = 0;
        }
    }

    return 1;
}

/*============================================
 * MIDI CC / Pitch Bend 控制接口
 *============================================*/

/**
 * 设置当前 MIDI 通道 (在 sf2_note_on 之前调用)
 * sf2_note_on 会将此值存入声部的 channel 字段
 */
void sf2_set_current_channel(uint8_t channel)
{
    if (channel < 16)
        g_current_channel = channel;
}

/**
 * 设置通道弯音值
 * @param channel  MIDI 通道 (0-15)
 * @param value    14-bit 弯音值 (-8192..+8191, 0=中心)
 */
void sf2_pitch_bend(uint8_t channel, int16_t value)
{
    if (channel < 16)
        BG_MIDI_data.pitch_bend[channel] = value;
}

/**
 * 处理通道 CC 消息
 * @param channel  MIDI 通道 (0-15)
 * @param cc_num   CC 编号
 * @param value    CC 值 (0-127)
 */
void sf2_channel_cc(uint8_t channel, uint8_t cc_num, uint8_t value)
{
    int i;
    if (channel >= 16) return;

    switch (cc_num) {
        case 0x01:  /* CC1: 调制轮 (Modulation Wheel) */
            BG_MIDI_data.modulation[channel] = value;
            break;

        case 0x07:  /* CC7: 通道音量 (Channel Volume) */
            BG_MIDI_data.Channel_volume[channel] = value;
            break;

        case 0x0B:  /* CC11: 表情控制 (Expression) */
            BG_MIDI_data.expression[channel] = value;
            break;

        case 0x40:  /* CC64: 延音踏板 (Sustain Pedal) */
            if (value >= 64) {
                BG_MIDI_data.sustain_pedal[channel] = 1;
            } else {
                BG_MIDI_data.sustain_pedal[channel] = 0;
                /* 踏板松开: 释放该通道所有被延音保持的声部 */
                for (i = 0; i < SYNTH_MAX_VOICES; i++) {
                    if (g_voices[i].active &&
                        g_voices[i].channel == channel &&
                        g_voices[i].sustained) {
                        g_voices[i].sustained = 0;
                        BG_Envelope_Release(&g_voices[i].envelope);
                    }
                }
            }
            break;

        case 0x78:  /* CC120: All Sound Off — 立即静音 */
            for (i = 0; i < SYNTH_MAX_VOICES; i++) {
                if (g_voices[i].active && g_voices[i].channel == channel) {
                    g_voices[i].active = 0;
                    g_voices[i].sustained = 0;
                    BG_Envelope_Reset(&g_voices[i].envelope);
                    g_voices[i].state.sample = NULL;
                }
            }
            break;

        case 0x79:  /* CC121: Reset All Controllers */
            BG_MIDI_data.pitch_bend[channel]    = 0;
            BG_MIDI_data.sustain_pedal[channel]  = 0;
            BG_MIDI_data.expression[channel]     = 127;
            BG_MIDI_data.modulation[channel]     = 0;
            /* 释放所有被延音保持的声部 */
            for (i = 0; i < SYNTH_MAX_VOICES; i++) {
                if (g_voices[i].active &&
                    g_voices[i].channel == channel &&
                    g_voices[i].sustained) {
                    g_voices[i].sustained = 0;
                    BG_Envelope_Release(&g_voices[i].envelope);
                }
            }
            break;

        default:
            break;
    }
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
                    uint8_t ivel_lo = vel_lo, ivel_hi = vel_hi;

                    /* 临时存储 instrument-level generator 值 */
                    int16_t  i_attenuation = 0;
                    int16_t  i_coarse_tune = 0;
                    int16_t  i_fine_tune = 0;
                    int16_t  i_pan = 0;
                    int16_t  i_scale_tuning = 100;  /* SF2 default */
                    uint16_t i_sample_modes = 0;
                    int16_t  i_exclusive_class = 0;
                    uint8_t  i_override_root_key = 255;
                    int16_t  i_vol_env_delay = -12000;
                    int16_t  i_vol_env_attack = -12000;
                    int16_t  i_vol_env_hold = -12000;
                    int16_t  i_vol_env_decay = -12000;
                    int16_t  i_vol_env_sustain = 0;
                    int16_t  i_vol_env_release = -12000;

                    for (ig = igen_start; ig < igen_end && ig < g_sf2_data.igen_count; ig++) {
                        SF2_Generator *igen = &g_sf2_data.inst_gens[ig];
                        
                        if (igen->type == GEN_SAMPLE_ID) {
                            sample_id = igen->amount.uword;
                        } else if (igen->type == GEN_KEY_RANGE) {
                            ikey_lo = igen->amount.range.lo;
                            ikey_hi = igen->amount.range.hi;
                        } else if (igen->type == GEN_VEL_RANGE) {
                            ivel_lo = igen->amount.range.lo;
                            ivel_hi = igen->amount.range.hi;
                        } else if (igen->type == GEN_INITIAL_ATTENUATION) {
                            i_attenuation = igen->amount.sword;
                        } else if (igen->type == GEN_COARSE_TUNE) {
                            i_coarse_tune = igen->amount.sword;
                        } else if (igen->type == GEN_FINE_TUNE) {
                            i_fine_tune = igen->amount.sword;
                        } else if (igen->type == GEN_PAN) {
                            i_pan = igen->amount.sword;
                        } else if (igen->type == GEN_SCALE_TUNING) {
                            i_scale_tuning = igen->amount.sword;
                        } else if (igen->type == GEN_SAMPLE_MODES) {
                            i_sample_modes = igen->amount.uword;
                        } else if (igen->type == GEN_EXCLUSIVE_CLASS) {
                            i_exclusive_class = igen->amount.sword;
                        } else if (igen->type == GEN_OVERRIDING_ROOT_KEY) {
                            i_override_root_key = (uint8_t)igen->amount.uword;
                        } else if (igen->type == GEN_DELAY_VOL_ENV) {
                            i_vol_env_delay = igen->amount.sword;
                        } else if (igen->type == GEN_ATTACK_VOL_ENV) {
                            i_vol_env_attack = igen->amount.sword;
                        } else if (igen->type == GEN_HOLD_VOL_ENV) {
                            i_vol_env_hold = igen->amount.sword;
                        } else if (igen->type == GEN_DECAY_VOL_ENV) {
                            i_vol_env_decay = igen->amount.sword;
                        } else if (igen->type == GEN_SUSTAIN_VOL_ENV) {
                            i_vol_env_sustain = igen->amount.sword;
                        } else if (igen->type == GEN_RELEASE_VOL_ENV) {
                            i_vol_env_release = igen->amount.sword;
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
                        sinfo->min_vel = ivel_lo;
                        sinfo->max_vel = ivel_hi;
                        sinfo->start = shdr->start;
                        sinfo->end = shdr->end;
                        sinfo->start_loop = shdr->start_loop;
                        sinfo->end_loop = shdr->end_loop;
                        sinfo->sample_rate = shdr->sample_rate;
                        sinfo->pitch_correction = shdr->pitch_correction;

                        /* Generator 参数 */
                        sinfo->attenuation = i_attenuation;
                        sinfo->coarse_tune = i_coarse_tune;
                        sinfo->fine_tune = i_fine_tune;
                        sinfo->pan = i_pan;
                        sinfo->scale_tuning = i_scale_tuning;
                        sinfo->sample_modes = i_sample_modes;
                        sinfo->exclusive_class = i_exclusive_class;
                        sinfo->override_root_key = i_override_root_key;
                        sinfo->vol_env_delay = i_vol_env_delay;
                        sinfo->vol_env_attack = i_vol_env_attack;
                        sinfo->vol_env_hold = i_vol_env_hold;
                        sinfo->vol_env_decay = i_vol_env_decay;
                        sinfo->vol_env_sustain = i_vol_env_sustain;
                        sinfo->vol_env_release = i_vol_env_release;

                        /* override_root_key 覆盖 original_pitch */
                        if (i_override_root_key != 255) {
                            sinfo->original_pitch = i_override_root_key;
                        }
                        
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
