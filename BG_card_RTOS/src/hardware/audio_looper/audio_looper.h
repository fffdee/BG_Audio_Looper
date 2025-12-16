/**
 **************************************************************************************
 * @file    audio_looper.h
 * @brief   Audio looper header file
 *
 * @author  BanGO
 * @version V1.0.0
 *
 * @Copyright (C) 2025, Audio Looper Project. All rights reserved.
 **************************************************************************************
 */

#ifndef __AUDIO_LOOPER_H__
#define __AUDIO_LOOPER_H__

#include "type.h"
#include "stdint.h"

// 手动定义size_t类型（如果编译器没有stddef.h）
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

// Loop状态枚举
typedef enum {
    LOOP_STATE_IDLE = 0,               // 空闲状态，等待录制
    LOOP_STATE_RECORDING = 1,          // 只录制状态（第一段）
    LOOP_STATE_PLAYING = 2,            // 只播放状态
    LOOP_STATE_RECORDING_AND_PLAYING = 3 // 边录制边播放状态
} LoopState_t;


// Loop状态枚举
typedef enum {
	SONG_MODE = 0,
	FREE_STYLE,
} Paly_Mode_t;

// 多段录音支持
#define MAX_SEGMENTS 4          // 最多支持4段录音

// 节拍器常量定义
#define METRONOME_MIN_BPM 60        // 最小BPM
#define METRONOME_MAX_BPM 200       // 最大BPM
#define METRONOME_DEFAULT_BPM 80   // 默认BPM
#define METRONOME_DEFAULT_BEATS_PER_MEASURE 4   // 默认每小节拍数
#define METRONOME_DEFAULT_DOWNBEAT_FREQ 1000    // 默认下拍频率（Hz）
#define METRONOME_DEFAULT_REGULAR_BEAT_FREQ 800 // 默认普通拍频率（Hz）
#define METRONOME_DEFAULT_BEAT_DURATION 60     // 默认节拍持续时间（ms）
#define METRONOME_DEFAULT_VOLUME 0.01f           // 默认音量
#define METRONOME_DEFAULT_SOUND_ENABLED 1       // 默认开启节拍器声音
#define METRONOME_SAMPLE_RATE 48000             // 音频采样率
#define METRONOME_MIN_BEATS_PER_MEASURE 2       // 最小每小节拍数
#define METRONOME_MAX_BEATS_PER_MEASURE 8       // 最大每小节拍数

// 数学常量
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Flash类型枚举
typedef enum {
    FLASH_TYPE_NOR = 0,         // NOR Flash (DEV_NOR)
    FLASH_TYPE_NAND = 1         // NAND Flash (DEV_NAND)
} FlashType_t;

// 段状态枚举
typedef enum {
    SEGMENT_INACTIVE = 0,      // 段未激活
    SEGMENT_RECORDING = 1,     // 段正在录制
    SEGMENT_PLAYING = 2,       // 段正在播放
    SEGMENT_STOPPED = 3        // 段已停止（已录制但未播放）
} SegmentState_t;

// 循环模式枚举
typedef enum {
    LOOP_MODE_SONG = 0,        // 歌曲模式：所有段等待最长段完成循环，有长度限制
    LOOP_MODE_FREE = 1         // 自由模式：各段独立循环，无长度限制
} LoopMode_t;

// 节拍器状态枚举
typedef enum {
    METRONOME_OFF = 0,         // 节拍器关闭
    METRONOME_ON = 1           // 节拍器开启
} MetronomeState_t;

// 节拍器拍子类型枚举
typedef enum {
    BEAT_TYPE_DOWNBEAT = 0,    // 下拍（强拍，每小节第一拍）
    BEAT_TYPE_REGULAR = 1      // 普通拍
} BeatType_t;

typedef enum {
    NONE = 0,      // 无属性
    N_TYPE = 1,      // N属性
    S_TYPE = 2      // S属性
} MUTEX_t;

// 段信息结构体
typedef struct {

    uint32_t start_address;     // 段起始地址（页对齐）
    uint32_t length_pages;      // 段长度（页数）
    uint32_t length_bytes;      // 段长度（字节数）
    uint32_t play_position;     // 当前播放位置（字节）
    uint32_t over_time;
    SegmentState_t state;       // 段当前状态
    uint8_t is_active;          // 段是否有效（保留兼容性）
    MUTEX_t mutex;              // 段互斥
    uint8_t is_first_loop;      // 是否为第一次循环


} SegmentInfo_t;

// 节拍器配置结构体
typedef struct {
    uint16_t bpm;               // 节拍速度（每分钟拍数），范围60-200
    uint8_t beats_per_measure;  // 每小节拍数，通常为4
    uint16_t downbeat_freq;     // 下拍频率（Hz），建议1000Hz
    uint16_t regular_beat_freq; // 普通拍频率（Hz），建议800Hz
    uint16_t beat_duration_ms;  // 节拍声音持续时间（毫秒），建议100ms
    float volume;               // 音量系数，范围0.0-1.0
    uint8_t sound_enabled;      // 是否开启节拍器声音（1=开启，0=关闭，只计数）
} MetronomeConfig_t;

// 小节信息结构体
typedef struct {
    uint32_t measure_number;    // 当前小节编号（从1开始）
    uint8_t beat_in_measure;    // 当前小节内的拍子（从1开始）
    uint32_t total_beats;       // 总拍数（跨所有小节）
    uint32_t total_measures;    // 总小节数
    uint8_t measure_complete;   // 当前小节是否完成（刚完成一个小节时为1）
} MeasureInfo_t;

// 节拍器状态结构体
typedef struct {
    MetronomeState_t state;     // 节拍器开关状态
    MetronomeConfig_t config;   // 节拍器配置
    MeasureInfo_t measure_info; // 小节信息
    uint32_t beat_interval_samples;  // 节拍间隔（样本数）
    uint32_t beat_duration_samples;  // 节拍持续时间（样本数）
    uint32_t sample_counter;    // 样本计数器
    uint32_t beat_sample_counter;    // 当前拍子样本计数器
    uint8_t current_beat;       // 当前拍子索引（0-beats_per_measure-1）
    uint8_t is_beat_active;     // 当前是否在播放节拍声音
    BeatType_t current_beat_type;    // 当前拍子类型
    float sine_phase;           // 正弦波相位累积器
    uint8_t loop_reset_flag;    // 循环重置标志，用于检测循环重新开始
} MetronomeState_Runtime_t;

// Loop管理器结构体
typedef struct {
    LoopState_t state;              // 当前状态
    LoopMode_t mode;                // 循环模式（歌曲/自由）
    FlashType_t flash_type;         // Flash类型选择（NOR或NAND）
    uint32_t sector_address;        // 当前Flash地址
    uint32_t record_length;         // 录制数据长度
    uint32_t play_position;         // 播放位置
    uint8_t is_initialized;         // 是否已初始化
    uint8_t is_new_recording;       // 是否为新录制

    // 模式选择
    Paly_Mode_t play_mode;
    
    // 多段录音支持
    SegmentInfo_t segments[MAX_SEGMENTS];  // 段信息数组
    uint8_t current_segment;        // 当前录制段索引 (0-3)
    uint8_t active_segments;        // 已录制的段数量
    uint32_t page_size;             // Flash页大小 (256字节)
    
    // 歌曲模式相关
    uint32_t master_segment_length; // 主段（最长段）长度，用于歌曲模式循环基准
    uint8_t master_segment_index;   // 主段索引
    
    // 循环边界平滑处理
    uint32_t loop_boundary_samples[48];  // 存储循环边界的样本用于平滑处理
    uint8_t boundary_samples_valid;     // 边界样本是否有效
    
    // 节拍器支持
    MetronomeState_Runtime_t metronome; // 节拍器运行时状态
} LoopManager_t;

// 全局Loop管理器
extern LoopManager_t g_loop_manager;

// Loop操作结果枚举
typedef enum {
    LOOP_RESULT_OK = 0,         // 操作成功
    LOOP_RESULT_ERROR = 1,      // 操作失败
    LOOP_RESULT_BUSY = 2,       // 系统忙碌
    LOOP_RESULT_FULL = 3        // 存储已满
} LoopResult_t;

// Loop统计信息结构体
typedef struct {
    LoopState_t current_state;          // 当前状态
    uint8_t active_segments;            // 活跃段数
    uint8_t current_segment;            // 当前段索引
    uint32_t total_recorded_bytes;      // 总录制字节数
    uint32_t total_play_time_ms;        // 总播放时间（毫秒）
    FlashType_t flash_type;             // Flash类型
    uint8_t is_recording;               // 是否正在录制
    uint8_t is_playing;                 // 是否正在播放
} LoopStatus_t;

// Audio Looper模块接口结构体
typedef struct {
    // 初始化和配置
    void (*Init)(void);
    void (*InitWithFlashType)(FlashType_t flash_type);
    void (*Reset)(void);
    LoopResult_t (*SetFlashType)(FlashType_t flash_type);
    
    // 控制操作
    LoopResult_t (*ButtonPress)(void);              // 主按键处理
    LoopResult_t (*SegmentButtonPress)(uint8_t segment_index);  // 指定段按键处理
    LoopResult_t (*EncoderLeft)(void);              // 左编码器（清除段）
    LoopResult_t (*EncoderRight)(void);             // 右编码器（全擦除）
    LoopResult_t (*StopRecording)(void);            // 停止录制
    
    // 音频处理
    void (*ProcessRecording)(int16_t* audio_data, uint8_t* buffer, uint16_t length);
    void (*ProcessPlayback)(int16_t* output_data, uint8_t* buffer, uint16_t length);
    void (*ProcessRecording32)(uint32_t* audio_data, uint8_t* buffer, uint16_t length);
    void (*ProcessPlayback32)(uint32_t* output_data, uint8_t* buffer, uint16_t length);
    
    // 状态查询
    LoopStatus_t (*GetStatus)(void);
    uint8_t (*IsRecording)(void);
    uint8_t (*IsPlaying)(void);
    uint32_t (*GetCurrentAddress)(void);
    uint32_t (*GetRecordLength)(void);
    
    // 测试和调试
    void (*TimerUpdate)(void);
    
    // 模式控制
    void (*SetMode)(LoopMode_t mode);               // 设置循环模式
    LoopMode_t (*GetMode)(void);                    // 获取当前循环模式
    uint8_t (*IsSongMode)(void);                    // 检查是否为歌曲模式
    uint8_t (*IsFreeMode)(void);                    // 检查是否为自由模式
    
    // 节拍器控制
    void (*MetronomeToggle)(void);                  // 切换节拍器开关
    void (*MetronomeSetBPM)(uint16_t bpm);         // 设置BPM
    void (*MetronomeSetBeatsPerMeasure)(uint8_t beats); // 设置每小节拍数
    void (*MetronomeSetVolume)(float volume);       // 设置节拍器音量
    void (*MetronomeSetSoundEnabled)(uint8_t enabled); // 设置是否开启节拍器声音
    uint8_t (*MetronomeIsEnabled)(void);           // 检查节拍器是否开启
    uint8_t (*MetronomeIsSoundEnabled)(void);      // 检查节拍器声音是否开启
    uint16_t (*MetronomeGetBPM)(void);             // 获取当前BPM
    uint8_t (*MetronomeGetBeatsPerMeasure)(void);  // 获取每小节拍数
    
    // 小节和拍子信息查询
    uint32_t (*MetronomeGetCurrentMeasure)(void);  // 获取当前小节号
    uint8_t (*MetronomeGetCurrentBeat)(void);      // 获取当前小节内拍子号
    uint32_t (*MetronomeGetTotalBeats)(void);      // 获取总拍数
    uint32_t (*MetronomeGetTotalMeasures)(void);   // 获取总小节数
    uint8_t (*MetronomeIsMeasureComplete)(void);   // 检查当前小节是否刚完成
    void (*MetronomeResetCounts)(void);            // 重置小节和拍子计数
    void (*MetronomeOnLoopReset)(void);            // 循环重置时调用，增加小节数
    
} AudioLooper_t;

// 全局Audio Looper模块实例
extern AudioLooper_t AudioLooper;

// ============================================================================
// 节拍器模块功能说明和使用示例
// ============================================================================
/**
 * 节拍器模块新功能：
 * 
 * 1. 小节概念：
 *    - 每个小节包含指定数量的拍子（默认4拍）
 *    - 小节从1开始编号，拍子从1开始编号
 *    - 每次循环重置时，小节数会增加
 * 
 * 2. 声音控制：
 *    - 可以独立控制节拍器是否发出声音
 *    - sound_enabled=1：正常节拍器声音
 *    - sound_enabled=0：静音模式，只计数不发声
 * 
 * 3. 状态查询：
 *    - 当前小节号：AudioLooper.MetronomeGetCurrentMeasure()
 *    - 当前拍子号：AudioLooper.MetronomeGetCurrentBeat()
 *    - 总拍数：AudioLooper.MetronomeGetTotalBeats()
 *    - 总小节数：AudioLooper.MetronomeGetTotalMeasures()
 * 
 * 使用示例：
 * ```c
 * // 设置节拍器参数
 * AudioLooper.MetronomeSetBPM(120);                    // 设置120 BPM
 * AudioLooper.MetronomeSetBeatsPerMeasure(4);          // 4/4拍
 * AudioLooper.MetronomeSetSoundEnabled(1);             // 开启声音
 * AudioLooper.MetronomeToggle();                       // 启动节拍器
 * 
 * // 查询当前状态
 * uint32_t measure = AudioLooper.MetronomeGetCurrentMeasure();  // 当前小节
 * uint8_t beat = AudioLooper.MetronomeGetCurrentBeat();        // 当前拍子
 * 
 * // 在循环重置时调用
 * AudioLooper.MetronomeOnLoopReset();                  // 增加小节计数
 * 
 * // 静音模式（只计数，不发声）
 * AudioLooper.MetronomeSetSoundEnabled(0);             // 关闭声音
 * ```
 */

// ============================================================================
// 节拍器相关函数声明（供外部调用）
// ============================================================================

// 节拍器初始化和配置
void metronome_init(void);
void metronome_reset(void);
void metronome_configure(const MetronomeConfig_t* config);

// 节拍器控制
void metronome_toggle(void);
void metronome_enable(void);
void metronome_disable(void);
uint8_t metronome_is_enabled(void);

// 声音控制
void metronome_set_sound_enabled(uint8_t enabled);
uint8_t metronome_is_sound_enabled(void);

// 参数设置
void metronome_set_bpm(uint16_t bpm);
void metronome_set_beats_per_measure(uint8_t beats);
void metronome_set_volume(float volume);
void metronome_set_downbeat_freq(uint16_t freq);
void metronome_set_regular_beat_freq(uint16_t freq);
void metronome_set_beat_duration(uint16_t duration_ms);

// 参数获取
uint16_t metronome_get_bpm(void);
uint8_t metronome_get_beats_per_measure(void);
float metronome_get_volume(void);
uint16_t metronome_get_downbeat_freq(void);
uint16_t metronome_get_regular_beat_freq(void);
uint16_t metronome_get_beat_duration(void);

// 状态查询
uint8_t metronome_get_current_beat(void);
BeatType_t metronome_get_current_beat_type(void);
uint8_t metronome_is_beat_active(void);

// 小节和计数相关
uint32_t metronome_get_current_measure(void);
uint8_t metronome_get_current_beat_in_measure(void);
uint32_t metronome_get_total_beats(void);
uint32_t metronome_get_total_measures(void);
uint8_t metronome_is_measure_complete(void);
void metronome_reset_counts(void);
void metronome_on_loop_reset(void);

// 音频处理
void metronome_process_audio(uint32_t* output_data, uint16_t length);
void metronome_mix_audio(uint32_t* output_data, uint16_t length);



#endif /* __AUDIO_LOOPER_H__ */
