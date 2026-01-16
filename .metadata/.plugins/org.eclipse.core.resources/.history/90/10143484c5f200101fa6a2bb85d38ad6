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
    uint32_t start_address;
    uint32_t length_pages;
    uint32_t length_bytes;
    uint32_t play_position;
    SegmentState_t state;
    uint8_t is_active;
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
void loop_reset_playback_position(void);

/* 单段精细控制函数 */
SegmentState_t loop_get_segment_state(uint8_t segment_index);
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

#endif /* __AUDIO_LOOPER_H__ */
