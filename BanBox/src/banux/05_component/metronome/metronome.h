/**
 **************************************************************************************
 * @file    metronome.h
 * @brief   独立节拍器模块头文件
 *          从audio_looper模块独立出来，作为效果图的数据源节点
 *
 * @author  BanGO
 * @version V1.0.0
 *
 * @Copyright (C) 2025, Audio Looper Project. All rights reserved.
 **************************************************************************************
 */

#ifndef __METRONOME_H__
#define __METRONOME_H__

#include "type.h"
#include "stdint.h"
#include "audio_looper.h"  /* Include audio_looper for shared definitions */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 常量定义
 * ============================================================================ */
#define METRONOME_MIN_BPM                   60
#define METRONOME_MAX_BPM                   200
#define METRONOME_DEFAULT_BPM               80
#define METRONOME_DEFAULT_BEATS_PER_MEASURE 4
#define METRONOME_DEFAULT_DOWNBEAT_FREQ     1000
#define METRONOME_DEFAULT_REGULAR_BEAT_FREQ 800
#define METRONOME_DEFAULT_BEAT_DURATION     60
#define METRONOME_DEFAULT_VOLUME            0.03f
#define METRONOME_SAMPLE_RATE               48000  /* System sample rate */
#define METRONOME_MIN_BEATS_PER_MEASURE     2
#define METRONOME_MAX_BEATS_PER_MEASURE     8

/* Math constant */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ============================================================================
 * 共享枚举和结构体定义
 * ============================================================================ */
/* MetronomeState_t, BeatType_t 和 MetronomeConfig_t 定义在 audio_looper.h 中，
 * 以避免重复定义和编译冲突。这些类型在 metronome.h 中通过 #include "audio_looper.h" 获取。*/

/* 节拍器运行时状态结构体 */
typedef struct {
    MetronomeState_t state;             /* 开关状态 */
    MetronomeConfig_t config;           /* 配置参数 */
    uint32_t beat_interval_samples;     /* 节拍间隔（采样数）*/
    uint32_t beat_duration_samples;     /* 节拍音持续时间（采样数）*/
    uint32_t sample_counter;            /* 采样计数器 */
    uint32_t beat_sample_counter;       /* 节拍内采样计数器 */
    uint8_t current_beat;               /* 当前拍子索引 (0-based) */
    uint8_t is_beat_active;             /* 当前是否在发声 */
    BeatType_t current_beat_type;       /* 当前拍子类型 */
    float sine_phase;                   /* 正弦波相位累加器 */
} MetronomeRuntime_t;

/* ============================================================================
 * 节拍器模块接口结构体
 * ============================================================================ */
typedef struct {
    /* 初始化和控制 */
    void (*Init)(void);
    void (*Reset)(void);
    void (*Configure)(const MetronomeConfig_t* config);
    void (*Toggle)(void);
    void (*Enable)(void);
    void (*Disable)(void);
    uint8_t (*IsEnabled)(void);
    
    /* 参数设置 */
    void (*SetBPM)(uint16_t bpm);
    void (*SetBeatsPerMeasure)(uint8_t beats);
    void (*SetVolume)(float volume);
    void (*SetDownbeatFreq)(uint16_t freq);
    void (*SetRegularBeatFreq)(uint16_t freq);
    void (*SetBeatDuration)(uint16_t duration_ms);
    
    /* 参数获取 */
    uint16_t (*GetBPM)(void);
    uint8_t (*GetBeatsPerMeasure)(void);
    float (*GetVolume)(void);
    uint16_t (*GetDownbeatFreq)(void);
    uint16_t (*GetRegularBeatFreq)(void);
    uint16_t (*GetBeatDuration)(void);
    
    /* 状态查询 */
    uint8_t (*GetCurrentBeat)(void);
    BeatType_t (*GetCurrentBeatType)(void);
    uint8_t (*IsBeatActive)(void);
    
    /* 音频处理 - 作为效果图源节点使用 */
    uint16_t (*GenerateAudio)(uint32_t* output_data, uint16_t max_len);
    
    /* 查询可用数据量 - 节拍器始终有数据可用 */
    uint16_t (*GetAvailableData)(void);
} Metronome_Interface_t;

/* ============================================================================
 * 全局节拍器模块实例
 * ============================================================================ */
extern Metronome_Interface_t MetronomeModule;

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/* 初始化和控制 */
void Metronome_Init(void);
void Metronome_Reset(void);
void Metronome_Configure(const MetronomeConfig_t* config);
void Metronome_Toggle(void);
void Metronome_Enable(void);
void Metronome_Disable(void);
uint8_t Metronome_IsEnabled(void);

/* 参数设置 */
void Metronome_SetBPM(uint16_t bpm);
void Metronome_SetBeatsPerMeasure(uint8_t beats);
void Metronome_SetVolume(float volume);
void Metronome_SetDownbeatFreq(uint16_t freq);
void Metronome_SetRegularBeatFreq(uint16_t freq);
void Metronome_SetBeatDuration(uint16_t duration_ms);

/* 参数获取 */
uint16_t Metronome_GetBPM(void);
uint8_t Metronome_GetBeatsPerMeasure(void);
float Metronome_GetVolume(void);
uint16_t Metronome_GetDownbeatFreq(void);
uint16_t Metronome_GetRegularBeatFreq(void);
uint16_t Metronome_GetBeatDuration(void);

/* 状态查询 */
uint8_t Metronome_GetCurrentBeat(void);
BeatType_t Metronome_GetCurrentBeatType(void);
uint8_t Metronome_IsBeatActive(void);

/* 音频处理 - 核心功能 */
uint16_t Metronome_GenerateAudio(uint32_t* output_data, uint16_t max_len);
uint16_t Metronome_GetAvailableData(void);

/* 获取运行时状态指针（供外部访问）*/
MetronomeRuntime_t* Metronome_GetRuntime(void);

#ifdef __cplusplus
}
#endif

#endif /* __METRONOME_H__ */
