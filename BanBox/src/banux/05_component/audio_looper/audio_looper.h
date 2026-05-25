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
#include "usb_audio_api.h"

/* ============================================================================
 * Looper 采样率定义
 *
 * 必须与 DAC/ADC 实际硬件采样率一致 (CFG_PARA_SAMPLE_RATE)，
 * 否则录制/播放会产生采样率不匹配失真。
 * 当前 DAC 初始化为 44100 Hz，looper 也必须使用 44100 Hz。
 * ============================================================================ */
#ifndef LOOPER_SAMPLE_RATE
#define LOOPER_SAMPLE_RATE  CFG_PARA_SAMPLE_RATE
#endif

/* Manually define size_t type (if compiler does not have stddef.h) */
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

/* Loop state enumeration */
typedef enum {
    LOOP_STATE_IDLE = 0,               /* Idle state, waiting for recording */
    LOOP_STATE_RECORDING = 1,          /* Recording only (first segment) */
    LOOP_STATE_PLAYING = 2,            /* Playback only */
    LOOP_STATE_RECORDING_AND_PLAYING = 3 /* Recording and playback simultaneously */
} LoopState_t;

/* Playback mode enumeration */
typedef enum {
    SONG_MODE = 0,
    FREE_STYLE
} Play_Mode_t;

/* Multi-segment recording support */
#define MAX_SEGMENTS 4          /* Support up to 4 segments */

/* ============================================================================
 * 存储类型选择常量 (配合 product_def.h 中的 LOOPER_STORAGE_TYPE 使用)
 *   0 = 自动检测 (根据 HW_PSRAM0_EN / HW_NAND0_EN)
 *   1 = 强制 PSRAM
 *   2 = 强制 NAND Flash
 *   3 = 强制 NOR Flash
 * ============================================================================ */
#define LOOPER_STORAGE_TYPE_AUTO   0
#define LOOPER_STORAGE_TYPE_PSRAM  1
#define LOOPER_STORAGE_TYPE_NAND   2
#define LOOPER_STORAGE_TYPE_NOR    3

/* 若 product_def.h 未定义，默认自动检测 */
#ifndef LOOPER_STORAGE_TYPE
#define LOOPER_STORAGE_TYPE  LOOPER_STORAGE_TYPE_AUTO
#endif

/* ============================================================================
 * 多Flash支持宏开关
 *   1 = 启用多Flash：每段绑定独立CS引脚/Flash芯片，播放段A(读Flash#0)
 *       的同时可录制段B(写Flash#1)，彻底解决W25Q64不能边读边写的问题
 *   0 = 单Flash模式（向后兼容旧代码，所有段共用Flash#0）
 * ============================================================================ */
#ifndef LOOPER_MULTI_FLASH_ENABLE
#define LOOPER_MULTI_FLASH_ENABLE  0
#endif

/* ============================================================================
 * IO缓冲区宏开关 (解决 SPI Flash 页写入忙等待导致音频帧超时的问题)
 *
 * 原理：将Flash读写与音频回调解耦
 *   录制 → 音频回调仅向 RAM 写缓冲区追加数据 (~1μs)
 *   播放 → 音频回调仅从 RAM 读缓存取数据 (~1μs)
 *   Flash IO → 在 DAC 输出之后调用 looper_flush_io() 统一执行
 *
 *   1 = 启用：音频回调只操作RAM，Flash IO延迟到帧末尾
 *   0 = 禁用：录制/播放直接操作Flash（旧行为）
 * ============================================================================ */
#ifndef LOOPER_IO_BUFFER_ENABLE
#define LOOPER_IO_BUFFER_ENABLE  0

#endif

/* ============================================================================
 * 存储抽象层宏开关 (启用统一的存储接口，支持 NOR/NAND/PSRAM 自动切换)
 *
 * 原理：使用 looper_storage.h 提供的抽象层接口访问存储
 *   - 录制/播放通过 LooperStorage_Read/Write 访问
 *   - 自动适配当前选择的存储介质（NOR Flash / NAND Flash / PSRAM）
 *   - 支持首次启动带宽测试和性能优化
 *   - PSRAM 模式下支持叠录功能
 *
 *   1 = 启用：使用抽象层接口（推荐，支持多种存储介质）
 *   0 = 禁用：使用传统 FlashPartition_Looper* API（向后兼容）
 * ============================================================================ */
#ifndef LOOPER_USE_STORAGE_ABSTRACTION
#define LOOPER_USE_STORAGE_ABSTRACTION  1
#endif

/* PSRAM 页大小 = 256 字节
 * 每个采样 = 4 字节 (双声道16-bit打包在uint32_t)
 * 每页采样数 = 256 / 4 = 64
 *
 * 录制：凑满 64 采样 (256 字节) 才写一页 PSRAM
 * 播放：每次读一整页 256 字节，消费 64 采样
 */
#define LOOPER_PSRAM_PAGE_SIZE      256
#define LOOPER_SAMPLES_PER_PAGE     (LOOPER_PSRAM_PAGE_SIZE / 4)

/* 单声道模式：每采样 2 字节 (int16_t)，每页 128 采样 */
#define LOOPER_SAMPLES_PER_PAGE_MONO (LOOPER_PSRAM_PAGE_SIZE / 2)

/* ============================================================================
 * 录制源选择枚举 (每段可独立选择录制源)
 *
 * MIC_L / MIC_R / LINEIN_L / LINEIN_R 为单声道录制，
 * 存储占用减半 (2字节/采样)，播放时扩展为双声道输出。
 * ALL_MIX 为所有 ADC 混合信号，双声道 (4字节/采样)。
 * ============================================================================ */
typedef enum {
    LOOP_REC_SRC_MIC_L    = 0,   /* Mic 左声道 (mono) */
    LOOP_REC_SRC_MIC_R    = 1,   /* Mic 右声道 (mono) */
    LOOP_REC_SRC_LINEIN_L = 2,   /* LineIn 左声道 (mono) */
    LOOP_REC_SRC_LINEIN_R = 3,   /* LineIn 右声道 (mono) */
    LOOP_REC_SRC_ALL_MIX  = 4    /* 所有输入混音 (stereo) */
} LoopRecSource_t;

#define LOOP_REC_SRC_DEFAULT     LOOP_REC_SRC_ALL_MIX
#define LOOP_REC_SRC_IS_MONO(s)  ((s) != LOOP_REC_SRC_ALL_MIX)

/* 录制源缓冲区指针 (由 bg_audio_io_manager 在每帧录制前设置) */
extern uint32_t *g_looper_src_mic;     /* ADC1 原始麦克风立体声数据 */
extern uint32_t *g_looper_src_linein;  /* ADC0 原始 LineIn 立体声数据 */

#if LOOPER_IO_BUFFER_ENABLE

#define LOOPER_PAGE_DATA_SIZE      LOOPER_PSRAM_PAGE_SIZE

/* 录制写缓冲深度（页数）
 * 值越大可吸收越多的Flash写入延迟抖动（W25Q64页写最差可达3ms）
 * 每段 RAM 占用 = LOOPER_WRITE_BUF_PAGES × LOOPER_PSRAM_PAGE_SIZE 字节
 * 推荐值: 8 (2KB/段 × 4段 = 8KB), 最小: 2
 * 设为 2 时实际只有 1 个可用槽，任何单帧延迟即溢出，不可用于多段录制 */
#ifndef LOOPER_WRITE_BUF_PAGES
#define LOOPER_WRITE_BUF_PAGES    8
#endif

/* 播放读缓存深度（页数）
 * 值越大容许更长的Flash IO延迟而不产生播放缺数据
 * 每段 RAM 占用 = LOOPER_READ_CACHE_PAGES × LOOPER_PSRAM_PAGE_SIZE 字节
 * 推荐值: 8 (2KB/段), 最小: 2
 * 注意: 2 页时实际只有 1 个可用槽，多段录制时 Flash IO 延迟
 *       可能超过 1 帧，导致播放缺数据 → 底噪/咔嗒声 */
#ifndef LOOPER_READ_CACHE_PAGES
#define LOOPER_READ_CACHE_PAGES    8
#endif

/* ----- 录制写环形缓冲区 (每段一个) ----- */
typedef struct {
    uint8_t  buf[LOOPER_WRITE_BUF_PAGES][LOOPER_PAGE_DATA_SIZE];
    uint8_t  head;              /* 下一个写入槽位 (音频回调生产) */
    uint8_t  tail;              /* 下一个刷出槽位 (Flash消费)   */
    uint32_t flush_page;        /* Flash端已刷出页计数          */
} LooperWriteRing_t;

/* ----- 播放读环形缓存 (每段一个) ----- */
typedef struct {
    uint8_t  buf[LOOPER_READ_CACHE_PAGES][LOOPER_PAGE_DATA_SIZE];
    uint8_t  head;              /* 下一个填入槽位 (从Flash预读) */
    uint8_t  tail;              /* 下一个消费槽位 (音频回调读取) */
    uint32_t prefetch_page;     /* Flash端下一个待预读的页号     */
    uint8_t  active;            /* 1 = 缓存已初始化并追踪本段   */
} LooperReadCache_t;

#endif /* LOOPER_IO_BUFFER_ENABLE */

/* Metronome constant definitions */
#define METRONOME_MIN_BPM 60
#define METRONOME_MAX_BPM 200
#define METRONOME_DEFAULT_BPM 80
#define METRONOME_DEFAULT_BEATS_PER_MEASURE 4
#define METRONOME_DEFAULT_DOWNBEAT_FREQ 1000
#define METRONOME_DEFAULT_REGULAR_BEAT_FREQ 800
#define METRONOME_DEFAULT_BEAT_DURATION 60
#define METRONOME_DEFAULT_VOLUME 0.03f
#define METRONOME_SAMPLE_RATE LOOPER_SAMPLE_RATE
#define METRONOME_MIN_BEATS_PER_MEASURE 2
#define METRONOME_MAX_BEATS_PER_MEASURE 8

/* Math constant */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Flash type enumeration */
#ifndef FLASH_TYPE_DEFINED
#define FLASH_TYPE_DEFINED
typedef enum {
    FLASH_TYPE_NOR = 0,
    FLASH_TYPE_NAND = 1
} FlashType_t;
#endif /* FLASH_TYPE_DEFINED */

/* Segment state enumeration */
typedef enum {
    SEGMENT_INACTIVE = 0,
    SEGMENT_RECORDING = 1,
    SEGMENT_PLAYING = 2,
    SEGMENT_STOPPED = 3
} SegmentState_t;

/* Loop mode enumeration */
typedef enum {
    LOOP_MODE_SONG = 0,
    LOOP_MODE_FREE = 1
} LoopMode_t;

/* Metronome state enumeration */
typedef enum {
    METRONOME_OFF = 0,
    METRONOME_ON = 1
} MetronomeState_t;

/* Metronome beat type enumeration */
typedef enum {
    BEAT_TYPE_DOWNBEAT = 0,
    BEAT_TYPE_REGULAR = 1
} BeatType_t;

/* Segment info structure */
typedef struct {
    uint32_t start_address;  /* 相对于所绑定Flash起始地址的偏移 */
    /* PSRAM 分裂分配支持（NOR Flash 下三字段全为 0）：
     *   start_address2 = 0 表示单区域；非 0 表示有第二非连续区域
     *   region1_pages  = region1 最大页容量（= 分裂边界；0 = 无限，NOR Flash）
     *   region2_pages  = region2 最大页容量（0 = 无第二区域）               */
    uint32_t start_address2; /* 第二区域起始地址（0=单区域）*/
    uint32_t region1_pages;  /* 第一区域最大容量（页数，0=NOR Flash 无限制）*/
    uint32_t region2_pages;  /* 第二区域最大容量（页数，0=无第二区域）*/
    uint32_t length_pages;
    uint32_t length_bytes;
    uint32_t play_position;
    SegmentState_t state;
    uint8_t  is_active;
#if LOOPER_MULTI_FLASH_ENABLE
    uint8_t  flash_dev_id;   /* 绑定的Flash设备号 (0=Flash#0, 1=Flash#1, ...) */
#endif
    uint32_t trim_start_page; /* 循环起始页（0=从头播放） */
    uint32_t trim_end_page;   /* 循环终止页（0=播放至末尾） */
    
    /* 新增：叠录相关字段 */
    uint8_t  overdub_enabled; /* 叠录模式使能 (1=启用, 0=禁用) */
    uint8_t  rec_source;      /* 录制源 (LoopRecSource_t)，决定单/双声道存储 */

    uint16_t rec_partial_count;                                    /* rec_partial_buf中的字节数 (0-255) */
    uint8_t  rec_partial_buf[LOOPER_PSRAM_PAGE_SIZE];              /* 录制部分页缓冲：凑满256字节写一页 */
    uint16_t play_page_offset;                                     /* play_page_buf中的采样偏移 (0-63) */
    uint8_t  play_page_valid;                                      /* 1=play_page_buf有有效数据 */
    uint8_t  play_page_buf[LOOPER_PSRAM_PAGE_SIZE];                /* 播放页缓冲：读256字节整页，逐帧消费 */
} SegmentInfo_t;

/* Metronome config structure */
typedef struct {
    uint16_t bpm;
    uint8_t beats_per_measure;
    uint16_t downbeat_freq;
    uint16_t regular_beat_freq;
    uint16_t beat_duration_ms;
    float volume;
} MetronomeConfig_t;

/* Metronome runtime state structure */
typedef struct {
    MetronomeState_t state;
    MetronomeConfig_t config;
    uint32_t beat_interval_samples;
    uint32_t beat_duration_samples;
    uint32_t sample_counter;
    uint32_t beat_sample_counter;
    uint8_t current_beat;
    uint8_t is_beat_active;
    BeatType_t current_beat_type;
    float sine_phase;
} MetronomeState_Runtime_t;

/* Loop manager structure */
typedef struct {
    LoopState_t state;
    LoopMode_t mode;
    FlashType_t flash_type;
    uint32_t sector_address;
    uint32_t record_length;
    uint32_t play_position;
    uint8_t is_initialized;
    uint8_t is_new_recording;
#if LOOPER_MULTI_FLASH_ENABLE
    volatile uint8_t chip_erase_pending_mask; /* 位掩码：bit N=1 表示 Flash#N 全片擦除进行中
                                               * 允许对未擦除的Flash独立读写，实现边播边录 */
#else
    volatile uint8_t chip_erase_pending;    /* 1=全片擦除进行中，写入须等待 */
#endif
    Play_Mode_t play_mode;
    SegmentInfo_t segments[MAX_SEGMENTS];
    uint8_t current_segment;
    uint8_t active_segments;
    uint32_t page_size;
    uint32_t master_segment_length;
    uint8_t master_segment_index;
    uint32_t loop_boundary_samples[48];
    uint8_t boundary_samples_valid;
    MetronomeState_Runtime_t metronome;
    uint8_t segment_volume[MAX_SEGMENTS]; /* 各段播放音量 0-100，默认100 */
    
    /* 新增：存储抽象层相关字段 */
    uint8_t max_concurrent_segments;      /* 硬件支持的最大同时段数 */
    uint8_t support_overdub;              /* 是否支持叠录 */
    uint8_t overdub_mix_mode;             /* 叠录混音模式 (0=替换, 1=相加, 2=平均) */
    uint8_t storage_ready;               /* 存储后端就绪标志 (0=正在准备/擦除, 1=可录制) */
} LoopManager_t;

/* Global Loop manager */
extern LoopManager_t g_loop_manager;

/* Loop operation result enumeration */
typedef enum {
    LOOP_RESULT_OK = 0,
    LOOP_RESULT_ERROR = 1,
    LOOP_RESULT_BUSY = 2,
    LOOP_RESULT_FULL = 3
} LoopResult_t;

/* Loop status info structure */
typedef struct {
    LoopState_t current_state;
    uint8_t active_segments;
    uint8_t current_segment;
    uint32_t total_recorded_bytes;
    uint32_t total_play_time_ms;
    FlashType_t flash_type;
    uint8_t is_recording;
    uint8_t is_playing;
} LoopStatus_t;

/* Audio Looper module interface structure */
typedef struct {
    void (*Init)(void);
    void (*InitWithFlashType)(FlashType_t flash_type);
    void (*Reset)(void);
    LoopResult_t (*SetFlashType)(FlashType_t flash_type);
    LoopResult_t (*ButtonPress)(void);
    LoopResult_t (*SegmentButtonPress)(uint8_t segment_index);
    LoopResult_t (*EncoderLeft)(void);
    LoopResult_t (*EncoderRight)(void);
    LoopResult_t (*StopRecording)(void);
    void (*ProcessRecording)(int16_t* audio_data, uint8_t* buffer, uint16_t length);
    void (*ProcessPlayback)(int16_t* output_data, uint8_t* buffer, uint16_t length);
    void (*ProcessRecording32)(uint32_t* audio_data, uint8_t* buffer, uint16_t length);
    void (*ProcessPlayback32)(uint32_t* output_data, uint8_t* buffer, uint16_t length);
    LoopStatus_t (*GetStatus)(void);
    uint8_t (*IsRecording)(void);
    uint8_t (*IsPlaying)(void);
    uint32_t (*GetCurrentAddress)(void);
    uint32_t (*GetRecordLength)(void);
    void (*TimerUpdate)(void);
    void (*SetMode)(LoopMode_t mode);
    LoopMode_t (*GetMode)(void);
    uint8_t (*IsSongMode)(void);
    uint8_t (*IsFreeMode)(void);
    void (*MetronomeToggle)(void);
    void (*MetronomeSetBPM)(uint16_t bpm);
    void (*MetronomeSetBeatsPerMeasure)(uint8_t beats);
    void (*MetronomeSetVolume)(float volume);
    uint8_t (*MetronomeIsEnabled)(void);
    uint16_t (*MetronomeGetBPM)(void);
    uint8_t (*MetronomeGetBeatsPerMeasure)(void);
    /* 段音量控制 */
    void (*SetSegmentVolume)(uint8_t segment_index, uint8_t volume); /* 设置指定段音量 0-100 */
    uint8_t (*GetSegmentVolume)(uint8_t segment_index);              /* 获取指定段音量 */
    /* 段裁剪控制：设置/获取循环起止页，不修改Flash数据，仅影响播放范围 */
    void (*SetSegmentTrim)(uint8_t segment_index, uint32_t start_page, uint32_t end_page);
    void (*GetSegmentTrim)(uint8_t segment_index, uint32_t *start_page, uint32_t *end_page);
    /* 段录制源控制 */
    void (*SetSegmentRecSource)(uint8_t segment_index, uint8_t source);
    uint8_t (*GetSegmentRecSource)(uint8_t segment_index);
#if LOOPER_MULTI_FLASH_ENABLE
    /* 段Flash绑定控制 (仅多Flash模式) */
    void (*SetSegmentFlash)(uint8_t segment_index, uint8_t flash_dev_id); /* 绑定段到指定Flash */
    uint8_t (*GetSegmentFlash)(uint8_t segment_index);                    /* 获取段绑定的Flash */
#endif /* LOOPER_MULTI_FLASH_ENABLE */
    /* Looper Flash 生命周期 */
    void (*CheckFlashInitOnBoot)(void); /* 开机检查Flash是否已初始化 */
    void (*OnAppExit)(void);            /* 退出Looper界面时调用 */
#if LOOPER_IO_BUFFER_ENABLE
    /* IO缓冲区刷新 (每帧DAC输出后调用) */
    void (*FlushIO)(void);              /* 将写缓冲刷入Flash + 预读数据填充读缓存 */
#endif
    
    /* 新增：叠录功能支持 */
    uint8_t (*IsOverdubSupported)(void); /* 检查当前存储是否支持叠录 */
    void (*SetOverdubMode)(uint8_t segment_index, uint8_t enabled); /* 设置段的叠录模式 */
    uint8_t (*GetOverdubMode)(uint8_t segment_index); /* 获取段的叠录模式 */
    void (*SetOverdubMixMode)(uint8_t mix_mode); /* 设置叠录混音模式 (0=替换, 1=相加, 2=平均) */
    uint8_t (*GetOverdubMixMode)(void); /* 获取叠录混音模式 */
} AudioLooper_t;

/* Global Audio Looper module instance */
extern AudioLooper_t AudioLooper;

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/* 初始化函数 */
void loop_init(void);
void loop_init_with_flash_type(FlashType_t flash_type);
void loop_reset(void);
void loop_flash_erase_reinit(void);  /* 全片擦除初始化：清除所有数据，擦除完成前阻塞录制/播放 */

/* 按键处理函数 - 4个按键控制 */
void loop_handle_button_press(int8_t segment_index);
void loop_handle_segment_button(uint8_t segment_index);
void loop_handle_encoder_left(void);
void loop_handle_encoder_right(void);

/* Flash类型选择接口 */
void loop_set_flash_type(FlashType_t flash_type);
FlashType_t loop_get_flash_type(void);
uint8_t loop_get_flash_device_id(void);

/* 多段录音函数 */
void loop_start_new_segment(void);
void loop_stop_current_segment(uint8_t segment_index);
void loop_update_global_state(void);
uint8_t loop_get_segment_count(void);
void loop_clear_all_segments(void);
void loop_clear_segment(uint8_t segment_index);  /* 仅重置单段状态为INACTIVE（不擦Flash） */
void loop_reset_playback_position(void);

/* 单段精细控制函数 */
SegmentState_t loop_get_segment_state(uint8_t segment_index);
uint32_t loop_get_segment_length_pages(uint8_t segment_index);
void loop_set_segment_recording(uint8_t segment_index);
void loop_set_segment_playing(uint8_t segment_index);
void loop_set_segment_stopped(uint8_t segment_index);
uint8_t loop_is_segment_recording(uint8_t segment_index);
uint8_t loop_is_segment_playing(uint8_t segment_index);

/* 循环模式控制接口 */
void loop_set_mode(LoopMode_t mode);
LoopMode_t loop_get_mode(void);
uint8_t loop_is_song_mode(void);
uint8_t loop_is_free_mode(void);
void loop_update_master_segment_info(void);

/* 音频处理函数 */
void loop_process_recording_uint32(uint32_t* audio_data, uint8_t* buffer, uint16_t length);
void loop_process_playback_uint32(uint32_t* output_data, uint8_t* buffer, uint16_t length);
void loop_process_segment_recording(uint8_t segment_index, uint32_t* audio_data, uint8_t* buffer, uint16_t length);
uint8_t loop_process_segment_playback(uint8_t segment_index, uint32_t* output_data, uint8_t* buffer, uint16_t length);

void loop_timer_update(void);
void loop_stop_recording(void);

/* 状态查询函数 */
LoopState_t loop_get_state(void);
uint8_t loop_is_recording(void);
uint8_t loop_is_playing(void);
uint32_t loop_get_current_address(void);
uint32_t loop_get_record_length(void);

/* 数据转换函数 */
void convertUint32ArrayToUint8Array(const uint32_t *input, uint8_t *output, size_t size);
void convertUint8ArrayToUint32Array(const uint8_t *input, uint32_t *output, size_t size);
void convertUint8ArrayToInt16Array(const uint8_t *input, int16_t *output, size_t size);
void convertInt16ArrayToUint8Array(const int16_t *input, uint8_t *output, size_t size);

/* ============================================================================
 * 节拍器模块函数声明
 * ============================================================================ */
void metronome_init(void);
void metronome_reset(void);
void metronome_configure(const MetronomeConfig_t* config);
void metronome_toggle(void);
void metronome_enable(void);
void metronome_disable(void);
uint8_t metronome_is_enabled(void);
void metronome_set_bpm(uint16_t bpm);
void metronome_set_beats_per_measure(uint8_t beats);
void metronome_set_volume(float volume);
void metronome_set_downbeat_freq(uint16_t freq);
void metronome_set_regular_beat_freq(uint16_t freq);
void metronome_set_beat_duration(uint16_t duration_ms);
uint16_t metronome_get_bpm(void);
uint8_t metronome_get_beats_per_measure(void);
float metronome_get_volume(void);
uint16_t metronome_get_downbeat_freq(void);
uint16_t metronome_get_regular_beat_freq(void);
uint16_t metronome_get_beat_duration(void);
uint8_t metronome_get_current_beat(void);
BeatType_t metronome_get_current_beat_type(void);
uint8_t metronome_is_beat_active(void);
void metronome_process_audio(uint32_t* output_data, uint16_t length);
void metronome_mix_audio(uint32_t* output_data, uint16_t length);

/* ============================================================================
 * Looper 段音量控制
 * ============================================================================ */
void loop_set_segment_volume(uint8_t segment_index, uint8_t volume); /* 设置段播放音量 0-100 */
uint8_t loop_get_segment_volume(uint8_t segment_index);              /* 获取段播放音量 */

/* ============================================================================
 * Looper 段裁剪控制
 * ============================================================================ */
/**
 * @brief 设置段的循环裁剪起止页（仅影响播放范围，不修改Flash数据）
 * @param segment_index 段索引
 * @param start_page    循环起始页（0=从头）
 * @param end_page      循环终止页（0=到录制末尾）
 */
void loop_set_segment_trim(uint8_t segment_index, uint32_t start_page, uint32_t end_page);

/**
 * @brief 获取段当前的裁剪起止页
 * @param segment_index 段索引
 * @param start_page    输出：循环起始页
 * @param end_page      输出：循环终止页（0=到录制末尾）
 */
void loop_get_segment_trim(uint8_t segment_index, uint32_t *start_page, uint32_t *end_page);

/* ============================================================================
 * 段录制源控制
 * ============================================================================ */
void loop_set_segment_rec_source(uint8_t segment_index, uint8_t source);
uint8_t loop_get_segment_rec_source(uint8_t segment_index);
uint8_t loop_is_segment_mono(uint8_t segment_index);

/* ============================================================================
 * 叠录功能支持 (仅当存储设备支持时可用)
 * ============================================================================ */

/**
 * @brief 检查当前存储设备是否支持叠录
 * @return 1=支持叠录, 0=不支持
 */
uint8_t loop_is_overdub_supported(void);

/**
 * @brief 设置指定段的叠录模式
 * @param segment_index 段索引 (0-3)
 * @param enabled 1=启用叠录, 0=禁用叠录
 */
void loop_set_overdub_mode(uint8_t segment_index, uint8_t enabled);

/**
 * @brief 获取指定段的叠录模式
 * @param segment_index 段索引 (0-3)
 * @return 1=叠录模式启用, 0=正常录制模式
 */
uint8_t loop_get_overdub_mode(uint8_t segment_index);

/**
 * @brief 设置叠录混音模式
 * @param mix_mode 混音模式: 0=替换, 1=相加, 2=平均
 */
void loop_set_overdub_mix_mode(uint8_t mix_mode);

/**
 * @brief 获取叠录混音模式
 * @return 混音模式: 0=替换, 1=相加, 2=平均
 */
uint8_t loop_get_overdub_mix_mode(void);

/* 段与Flash绑定控制 (仅多Flash模式) */
#if LOOPER_MULTI_FLASH_ENABLE
void    loop_set_segment_flash(uint8_t segment_index, uint8_t flash_dev_id); /* 将指定段绑定到某颗Flash (须在录制前设置) */
uint8_t loop_get_segment_flash(uint8_t segment_index);                       /* 获取指定段当前绑定的Flash设备号 */
#endif /* LOOPER_MULTI_FLASH_ENABLE */

/* ============================================================================
 * Looper Flash 状态管理
 * ============================================================================ */
void loop_check_flash_init_on_boot(void); /* 开机检查Flash是否已初始化，否则触发全片擦除 */
void loop_on_app_exit(void);              /* 退出Looper界面时调用，如果Flash已使用则触发擦除 */

/* ============================================================================
 * IO缓冲区管理 (仅 LOOPER_IO_BUFFER_ENABLE=1 时可用)
 * ============================================================================ */
#if LOOPER_IO_BUFFER_ENABLE
void looper_flush_io(void);                            /* 每帧调用：刷写缓冲 + 填读缓存 */
void looper_init_read_cache(uint8_t segment_index);    /* 初始化/重填段的播放读缓存 */
void looper_flush_write_all(uint8_t segment_index);    /* 将段的写缓冲全部刷入Flash（阻塞） */
#endif /* LOOPER_IO_BUFFER_ENABLE */

/* ============================================================================
 * 动态 Flash 分配 + 边录边擦（erase-ahead）
 *
 * 不再固定分区，所有段共享整片 Flash（8MB），按实际录制长度占用：
 *   - 段 N 的 start_address = 前 N-1 段末尾的最大地址（64KB 对齐）
 *   - 录制开始时擦除首块；写指针距下一块边界 ≤ LOOPER_ERASE_AHEAD_PAGES 时
 *     异步触发下一块擦除（looper_flush_io 里轮询推进）
 *   - 若擦除尚未完成而写指针已到达新块，则暂停写入直到擦除完成
 *
 * loop_init_segment_region(seg_idx):
 *   重置段状态，计算 start_address，触发首块擦除。
 *   无需提前指定 max_sec，用多少擦多少。
 *
 * 常量：
 *   LOOPER_FLASH_TOTAL_SIZE    = 8MB (整片可用)
 *   LOOPER_FLASH_BLOCK_SIZE    = 64KB
 *   LOOPER_ERASE_AHEAD_PAGES   = 提前几页触发下一块擦除（256B/页 → 16页=4KB 提前量）
 *   LOOPER_AUDIO_BYTES_PER_SEC      = LOOPER_SAMPLE_RATE * 4 B/s (双声道)
 *   LOOPER_AUDIO_BYTES_PER_SEC_MONO = LOOPER_SAMPLE_RATE * 2 B/s (单声道)
 * ============================================================================ */

#define LOOPER_FLASH_TOTAL_SIZE   LOOPER_FLASH_DEV_SIZE   /* 8MB 整片 */
#define LOOPER_FLASH_BLOCK_SIZE   0x010000UL              /* 64KB 块擦除粒度 */
#define LOOPER_ERASE_AHEAD_PAGES  16u                     /* 距块末 16 页时触发下一块擦除 */
#define LOOPER_AUDIO_BYTES_PER_SEC      ((uint32_t)LOOPER_SAMPLE_RATE * 4u)
#define LOOPER_AUDIO_BYTES_PER_SEC_MONO ((uint32_t)LOOPER_SAMPLE_RATE * 2u)

/**
 * @brief 重置指定段并触发首块擦除（动态起始地址，无需预先指定时长）
 * @param seg_idx  段索引 0-3
 */
void loop_init_segment_region(uint8_t seg_idx);

/**
 * @brief 查询指定段的局部擦除是否仍在进行
 * @return 1 = 正在擦除（录制被阻塞）；0 = 擦除完成或未启动
 */
uint8_t loop_segment_partial_erase_pending(uint8_t seg_idx);

/* ============================================================================
 * Looper 下位机定时操作（衔接 / 接入 / 等待本轮播完再停止）
 *
 * 原理：音频回调 loop_process_segment_playback 检测到段回绕（循环到起点）时，
 *       仅设置 pending_* 标志位（无 I/O）；主循环调用
 *       Looper_TimedOps_Process() 消费标志并执行真正的状态切换 + BLE 回包。
 *
 * 精度：回绕检测在音频帧级（48 samples = 1ms @ 48kHz），对 Looper 场景完全够用。
 * ============================================================================ */

/**
 * 定时操作状态结构体
 *
 * chain_armed  : 1 = 衔接等待中；下次 chain_stop_seg 回绕时，停止它并启动 chain_start_seg
 * join_armed   : 1 = 接入等待中；下次任意 PLAYING 段回绕时，同时启动 join_start_seg
 * wait_finish_mask : bit N = 1 → 停止 seg N 时等到本轮结束（用户持久偏好）
 * deferred_stop_mask : bit N = 1 → 已收到 stop 请求，等待 seg N 下次回绕后再真正停止
 *
 * pending_*    : 由音频线程在回绕点置 1（仅设标志，不执行任何 I/O），
 *                主循环 Looper_TimedOps_Process() 读标志并执行操作后清零。
 */
typedef struct {
    /* --- 衔接：chain_stop_seg 本轮结束 → 停止它，同时启动 chain_start_seg --- */
    uint8_t chain_armed;        /* 1 = 衔接已激活 */
    uint8_t chain_stop_seg;     /* 触发回绕时将被停止的段 */
    uint8_t chain_start_seg;    /* 触发回绕时将被启动的段 */
    uint8_t pending_chain;      /* 音频线程置 1，主循环执行后清 0 */

    /* --- 接入：下次任意 PLAYING 段回绕 → 额外启动 join_start_seg（原播放段继续） --- */
    uint8_t join_armed;         /* 1 = 接入已激活 */
    uint8_t join_start_seg;     /* 触发回绕时将被启动的段 */
    uint8_t pending_join;       /* 音频线程置 1，主循环执行后清 0 */

    /* --- 等待播完再停止 --- */
    uint8_t wait_finish_mask;       /* 持久偏好 bit mask (bit N = seg N 开启了 wait-finish) */
    uint8_t deferred_stop_mask;     /* 待延迟停止 bit mask (stop 命令已收到，等回绕) */
    uint8_t pending_wait_finish;    /* 音频线程置位，主循环执行停止后清 0 */

    /* --- 同步录制：trigger_seg 回绕时触发 rec_seg 开始录制 --- */
    uint8_t sr_armed;           /* 1 = 同步录制已激活 */
    uint8_t sr_trigger_seg;     /* 触发回绕的段（通常 seg0） */
    uint8_t sr_record_seg;      /* 回绕后开始录制的段（通常 seg1） */
    uint8_t pending_sr;         /* 音频线程置 1，主循环执行后清 0 */
    uint8_t sr_match;           /* 1 = SR 触发录制后，在 trigger_seg 下次回绕时自动停止 rec_seg（等长模式） */
    uint8_t sr_autostop_armed;  /* 1 = 等待 trigger_seg 再次回绕时停止 rec_seg */
    uint8_t pending_sr_stop;    /* 音频线程置 1，主循环执行自动停止后清 0 */
} LooperTimedOps_t;

/** 全局定时操作状态（audio_looper.c 定义） */
extern LooperTimedOps_t g_looper_timed_ops;

/**
 * @brief 清除所有定时操作标志（通常在段清除/重置时调用）
 */
void Looper_TimedOps_Reset(void);

/**
 * @brief 主循环中调用：消费 pending 标志，执行 stop/start 状态切换，
 *        并通过 Shell_WriteRaw 发送 AA55 23 型 BLE 通知包告知 App。
 *        （勿在中断上下文调用：内部可能触发 Flash I/O）
 */
void Looper_TimedOps_Process(void);

#endif /* __AUDIO_LOOPER_H__ */
