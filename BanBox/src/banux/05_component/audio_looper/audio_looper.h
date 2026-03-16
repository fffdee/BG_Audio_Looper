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
} Paly_Mode_t;

/* Multi-segment recording support */
#define MAX_SEGMENTS 4          /* Support up to 4 segments */

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
#define LOOPER_IO_BUFFER_ENABLE  1
#endif

#if LOOPER_IO_BUFFER_ENABLE

/* 每页数据大小 (48采样 × 4字节/采样 = 192字节) */
#define LOOPER_PAGE_DATA_SIZE      192

/* 录制写缓冲深度（页数）
 * 值越大可吸收越多的Flash写入延迟抖动（W25Q64页写最差可达3ms）
 * 每段 RAM 占用 = LOOPER_WRITE_BUF_PAGES × 192 字节
 * 推荐值: 8 (1.5KB/段), 最小: 2 */
#ifndef LOOPER_WRITE_BUF_PAGES
#define LOOPER_WRITE_BUF_PAGES    2
#endif

/* 播放读缓存深度（页数）
 * 值越大容许更长的Flash IO延迟而不产生播放缺数据
 * 每段 RAM 占用 = LOOPER_READ_CACHE_PAGES × 192 字节
 * 推荐值: 8 (1.5KB/段), 最小: 2 */
#ifndef LOOPER_READ_CACHE_PAGES
#define LOOPER_READ_CACHE_PAGES    2
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
#define METRONOME_SAMPLE_RATE 48000
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
    Paly_Mode_t play_mode;
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
 * 分段按时长初始化（局部擦除，取代全片擦除）
 *
 * Flash 固定平分两段：
 *   Seg0 → 0x000000 ~ 0x3FFFFF (4MB)
 *   Seg1 → 0x400000 ~ 0x7FFFFF (4MB)
 *
 * loop_init_segment_region(seg_idx, max_sec):
 *   1. 将该段 start_address 固定为 LOOPER_SEG_FLASH_START[seg_idx]
 *   2. 计算 max_sec 对应的字节数，向上对齐到 64KB Block
 *   3. 逐块异步擦除（在 looper_flush_io 里轮询推进）
 *   4. 擦除完成前置 partial_erase_pending，录制被阻塞
 *
 * 常量：
 *   LOOPER_AUDIO_BYTES_PER_SEC = 48000 * 4 = 192000 B/s (双声道 32-bit)
 *   LOOPER_SEG_FLASH_SIZE      = 4MB
 *   LOOPER_FLASH_BLOCK_SIZE    = 64KB
 * ============================================================================ */

#define LOOPER_SEG0_FLASH_START   0x000000UL  /* Seg0 固定起始地址 */
#define LOOPER_SEG1_FLASH_START   0x400000UL  /* Seg1 固定起始地址 */
#define LOOPER_SEG_FLASH_SIZE     0x400000UL  /* 每段 Flash 大小 (4MB) */
#define LOOPER_FLASH_BLOCK_SIZE   0x010000UL  /* 64KB 块擦除 */
#define LOOPER_AUDIO_BYTES_PER_SEC 192000UL   /* 48kHz × 4B/sample (双声道32bit) */

/**
 * @brief 对指定段执行局部块擦除并固定 start_address（在 looper_flush_io 里推进）
 * @param seg_idx  段索引 0 或 1
 * @param max_sec  最大录制秒数（10 / 30 / 60），决定需要擦除的块数
 */
void loop_init_segment_region(uint8_t seg_idx, uint16_t max_sec);

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
