/**
 * @file sampler.h
 * @brief BanGTsynth 采样器模块接口
 *
 * 即兴音色合成功能：
 *   1. 通过 ADC 录制单个音高的采样 (单采样)
 *   2. 从存储芯片加载 WAV 文件作为采样
 *   3. 对单采样进行移调 (pitch shift)，拓展成完整音色（覆盖 MIDI 0-127）
 *
 * 移调原理：
 *   - 基准音高 (root key)：采样录制/加载时的原始音高
 *   - 目标音高：MIDI NoteOn 的音符号
 *   - 播放速率 = 2^((target_note - root_key) / 12)
 *   - 线性插值保证音质，避免锯齿
 *
 * 采样来源：
 *   - ADC 录制：通过麦克风/线路输入实时录制
 *   - WAV 加载：从 SD卡/NAND Flash 的 FAT32 文件系统加载 .wav 文件
 *   - PSRAM 存储：大采样数据存放在 PSRAM，不占 SRAM
 *
 * 内存布局 (PSRAM):
 *   采样数据存放在 PSRAM 的指定区域，通过 psram_buffer 管理。
 *   每个采样最大 512KB (约 5.8 秒 @ 44100Hz/16bit/mono)。
 */

#ifndef __SAMPLER_H__
#define __SAMPLER_H__

#include "product_def.h"

#if SYNTH_SD_NAND_PSRAM_EN || BANGTSYNTH_EN

#include <stdint.h>
#include <stdbool.h>
#include "err_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 采样器配置常量
 * ============================================ */

/** 最大采样时长 (秒) */
#define SAMPLER_MAX_DURATION_SEC     6

/** 采样率 (Hz) */
#define SAMPLER_SAMPLE_RATE          44100

/** 采样位深 (bits) */
#define SAMPLER_BIT_DEPTH            16

/** 通道数 */
#define SAMPLER_CHANNELS             1

/** 单采样最大大小 (字节) */
#define SAMPLER_MAX_SAMPLE_SIZE      (SAMPLER_MAX_DURATION_SEC * SAMPLER_SAMPLE_RATE * \
                                      (SAMPLER_BIT_DEPTH / 8) * SAMPLER_CHANNELS)

/** 采样器最大音色 (slot) 数量 */
#define SAMPLER_MAX_TIMBRES          4

/** 移调范围: 基准音 ± SAMPLER_PITCH_RANGE 半音 */
#define SAMPLER_PITCH_RANGE          48

/** 最大同时发声数 */
#define SAMPLER_MAX_VOICES           8

/* ============================================
 * 功能开关宏 (可在 product_def.h 中提前定义覆盖默认值)
 * ============================================ */

/** 支持双声道 WAV 输入 (自动混为单声道) */
#ifndef SAMPLER_WAV_STEREO_EN
#define SAMPLER_WAV_STEREO_EN    1
#endif

/** 支持 8-bit PCM WAV (自动转为 16-bit) */
#ifndef SAMPLER_WAV_8BIT_EN
#define SAMPLER_WAV_8BIT_EN      1
#endif

/** 支持非标准采样率 WAV (自动调整播放速率以音高不变) */
#ifndef SAMPLER_WAV_RESAMPLE_EN
#define SAMPLER_WAV_RESAMPLE_EN  1
#endif

/* ============================================
 * 数据结构
 * ============================================ */

/** 采样来源枚举 */
typedef enum {
    SAMPLER_SRC_NONE = 0,      /* 无采样 */
    SAMPLER_SRC_ADC,           /* ADC 录制 */
    SAMPLER_SRC_WAV_FILE,      /* WAV 文件 (SD/NAND) */
    SAMPLER_SRC_MEMORY         /* 内存数据 (调试用) */
} Sampler_Source_t;

/** 采样数据描述符 */
typedef struct {
    Sampler_Source_t source;    /* 采样来源 */
    uint8_t  root_key;         /* 基准音高 (MIDI note, 0-127, 默认60=C4) */
    uint32_t sample_rate;      /* 采样率 (Hz) */
    uint16_t bit_depth;        /* 位深 (8/16) */
    uint8_t  channels;         /* 通道数 (1=mono, 2=stereo) */
    uint32_t num_samples;      /* 总采样点数 */
    uint32_t data_size;        /* 数据大小 (字节) */
    uint32_t loop_start;       /* 循环起始采样点 (0=无循环) */
    uint32_t loop_end;         /* 循环结束采样点 (0=无循环) */
    uint32_t psram_addr;       /* 采样数据在 PSRAM 中的地址 */
    char     name[32];         /* 采样/音色名称 */
} Sampler_SampleDesc_t;

/** 采样器声部状态 */
typedef enum {
    SAMPLER_VOICE_FREE = 0,    /* 空闲 */
    SAMPLER_VOICE_ATTACK,      /* 起音阶段 */
    SAMPLER_VOICE_SUSTAIN,     /* 持续阶段 */
    SAMPLER_VOICE_RELEASE,     /* 释放阶段 */
    SAMPLER_VOICE_FINISHED     /* 已结束 */
} Sampler_VoiceState_t;

/** 声部实例 (运行时) */
typedef struct {
    Sampler_VoiceState_t state;
    uint8_t  note;              /* MIDI 音符号 */
    uint8_t  velocity;          /* 力度 */
    uint8_t  timbre_slot;       /* 音色 slot 索引 */
    uint32_t position_fixed;    /* 当前播放位置 (16.16 定点数) */
    uint32_t increment_fixed;   /* 每采样点步进 (16.16 定点数, 由移调决定) */
    uint16_t envelope;          /* 简易包络值 (0-65535) */
    uint16_t release_rate;      /* 释放速率 */
} Sampler_Voice_t;

/** ADC 录制状态 */
typedef enum {
    SAMPLER_REC_IDLE = 0,      /* 未录制 */
    SAMPLER_REC_ARMED,         /* 已装备，等待信号触发 */
    SAMPLER_REC_RECORDING,     /* 录制中 */
    SAMPLER_REC_DONE           /* 录制完成 */
} Sampler_RecState_t;

/** 采样器模块状态 */
typedef struct {
    bool initialized;
    Sampler_SampleDesc_t timbres[SAMPLER_MAX_TIMBRES];
    Sampler_Voice_t voices[SAMPLER_MAX_VOICES];
    Sampler_RecState_t rec_state;
    uint8_t  rec_target_slot;   /* 录制目标 slot */
    uint32_t rec_position;      /* 录制当前位置 (采样点) */
    uint32_t rec_psram_addr;    /* 录制缓冲区 PSRAM 地址 */
    uint8_t  active_timbre;     /* 当前活跃音色 slot */
} Sampler_State_t;

/* ============================================
 * 公开接口
 * ============================================ */

/**
 * 初始化采样器模块
 * @return SUCCESS 或错误码
 */
BG_ERR Sampler_Init(void);

/**
 * 反初始化采样器
 */
void Sampler_DeInit(void);

/* ---------- 采样加载 ---------- */

#if FAT32_EN
/**
 * 从 WAV 文件加载采样到指定 slot
 * @param slot      音色 slot (0 ~ SAMPLER_MAX_TIMBRES-1)
 * @param filename  WAV 文件路径 (在 FAT32 文件系统中)
 * @param root_key  基准音高 (MIDI note, 60=C4)
 * @return SUCCESS 或错误码
 */
BG_ERR Sampler_LoadWAV(uint8_t slot, const char *filename, uint8_t root_key);
#endif /* FAT32_EN */

/**
 * 从内存数据加载采样到指定 slot
 * @param slot       音色 slot
 * @param pcm_data   PCM 数据指针 (int16_t mono)
 * @param num_samples 采样点数
 * @param sample_rate 采样率
 * @param root_key   基准音高
 * @return SUCCESS 或错误码
 */
BG_ERR Sampler_LoadFromMemory(uint8_t slot, const int16_t *pcm_data,
                              uint32_t num_samples, uint32_t sample_rate,
                              uint8_t root_key);

/**
 * 清空指定 slot 的采样数据
 * @param slot  音色 slot
 */
void Sampler_ClearSlot(uint8_t slot);

/* ---------- ADC 录制 ---------- */

/**
 * 开始 ADC 录制采样
 * @param slot      录制目标 slot
 * @param root_key  录制音高 (建议吹/弹该音高的音)
 * @return SUCCESS 或错误码
 */
BG_ERR Sampler_StartRecord(uint8_t slot, uint8_t root_key);

/**
 * 停止 ADC 录制
 * 录制完成后自动将数据整理到 PSRAM
 * @return SUCCESS 或错误码
 */
BG_ERR Sampler_StopRecord(void);

/**
 * ADC 采样数据输入回调 (在音频中断中调用)
 * @param samples   ADC 采样数据 (int16_t)
 * @param count     采样点数
 */
void Sampler_FeedADC(const int16_t *samples, uint32_t count);

/**
 * 获取录制状态
 * @return 当前录制状态
 */
Sampler_RecState_t Sampler_GetRecordState(void);

/**
 * 获取录制进度 (0-100%)
 * @return 百分比
 */
uint8_t Sampler_GetRecordProgress(void);

/* ---------- 音符控制 (MIDI 兼容) ---------- */

/**
 * 选择当前活跃音色 slot
 * @param slot  音色 slot
 */
void Sampler_SelectTimbre(uint8_t slot);

/**
 * 音符开启 (采样器声部)
 * 根据 root_key 与 note 的差异自动计算移调比率
 * @param note      MIDI 音符号 (0-127)
 * @param velocity  力度 (0-127)
 */
void Sampler_NoteOn(uint8_t note, uint8_t velocity);

/**
 * 音符关闭
 * @param note  MIDI 音符号
 */
void Sampler_NoteOff(uint8_t note);

/**
 * 关闭所有音符
 */
void Sampler_AllNoteOff(void);

/* ---------- 音频输出 ---------- */

/**
 * 读取采样器混合输出
 * 遍历所有活跃声部，移调+混合输出 PCM 数据
 * @param out_buf  输出缓冲区 (int16_t)
 * @param count    采样帧数
 * @return 活跃声部数量 (0=静音)
 */
uint8_t Sampler_ReadSamples(int16_t *out_buf, uint32_t count);

/* ---------- 循环点设置 ---------- */

/**
 * 设置采样循环点
 * @param slot        音色 slot
 * @param loop_start  循环起始采样点
 * @param loop_end    循环结束采样点
 */
void Sampler_SetLoop(uint8_t slot, uint32_t loop_start, uint32_t loop_end);

/* ---------- 查询接口 ---------- */

/**
 * 获取指定 slot 的采样描述
 * @param slot  音色 slot
 * @param desc  输出描述符
 * @return SUCCESS 或错误码
 */
BG_ERR Sampler_GetSampleDesc(uint8_t slot, Sampler_SampleDesc_t *desc);

/**
 * 检查采样器是否有活跃声部
 * @return true=有声部在播放
 */
bool Sampler_IsActive(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_SD_NAND_PSRAM_EN || BANGTSYNTH_EN */

#endif /* __SAMPLER_H__ */
