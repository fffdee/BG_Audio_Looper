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

// 多段录音支持
#define MAX_SEGMENTS 4          // 最多支持4段录音

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

// 段信息结构体
typedef struct {
    uint32_t start_address;     // 段起始地址（页对齐）
    uint32_t length_pages;      // 段长度（页数）
    uint32_t length_bytes;      // 段长度（字节数）
    uint32_t play_position;     // 当前播放位置（字节）
    SegmentState_t state;       // 段当前状态
    uint8_t is_active;          // 段是否有效（保留兼容性）
} SegmentInfo_t;

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
    uint8_t use_memory_buffer;      // 是否使用内存缓冲区(用于调试)
    
    // 多段录音支持
    SegmentInfo_t segments[MAX_SEGMENTS];  // 段信息数组
    uint8_t current_segment;        // 当前录制段索引 (0-3)
    uint8_t active_segments;        // 已录制的段数量
    uint32_t page_size;             // Flash页大小 (256字节)
    
    // 歌曲模式相关
    uint32_t master_segment_length; // 主段（最长段）长度，用于歌曲模式循环基准
    uint8_t master_segment_index;   // 主段索引
    
    // 自动测试相关
    uint8_t auto_test_mode;         // 自动测试模式
    uint32_t auto_test_timer;       // 自动测试计时器
    uint8_t auto_test_state;        // 自动测试状态: 0=录制中, 1=播放中
    
    // 循环边界平滑处理
    uint32_t loop_boundary_samples[48];  // 存储循环边界的样本用于平滑处理
    uint8_t boundary_samples_valid;     // 边界样本是否有效
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
    uint8_t (*VerifyFlashData)(uint32_t test_length);
    void (*TimerUpdate)(void);
    
    // 模式控制
    void (*SetMode)(LoopMode_t mode);               // 设置循环模式
    LoopMode_t (*GetMode)(void);                    // 获取当前循环模式
    uint8_t (*IsSongMode)(void);                    // 检查是否为歌曲模式
    uint8_t (*IsFreeMode)(void);                    // 检查是否为自由模式
    
} AudioLooper_t;

// 全局Audio Looper模块实例
extern AudioLooper_t AudioLooper;

// 函数声明
void loop_init(void);
void loop_init_with_flash_type(FlashType_t flash_type);  // 新增：带Flash类型的初始化
void loop_reset(void);
void loop_handle_button_press(int8_t segment_index);
void loop_handle_encoder_left(void);    // 编码器左转处理

// Flash类型选择接口
void loop_set_flash_type(FlashType_t flash_type);
FlashType_t loop_get_flash_type(void);
uint8_t loop_get_flash_device_id(void); // 获取当前Flash设备ID (DEV_NOR或DEV_NAND)

// 自动测试函数
void loop_start_auto_test(void);        // 启动自动测试
void loop_update_auto_test(void);       // 更新自动测试状态
void loop_stop_auto_test(void);         // 停止自动测试

// 多段录音函数
void loop_start_new_segment(void);      // 开始录制新段
void loop_stop_current_segment(void);   // 停止当前段录制
uint8_t loop_get_segment_count(void);   // 获取已录制段数
void loop_clear_all_segments(void);     // 清除所有段
void loop_reset_playback_position(void); // 重置播放位置到段头

// 单段精细控制函数
void loop_handle_segment_button(uint8_t segment_index);    // 处理指定段的按键
SegmentState_t loop_get_segment_state(uint8_t segment_index);  // 获取段状态
void loop_set_segment_recording(uint8_t segment_index);    // 设置段进入录制状态
void loop_set_segment_playing(uint8_t segment_index);      // 设置段进入播放状态
void loop_set_segment_stopped(uint8_t segment_index);      // 设置段进入停止状态
uint8_t loop_is_segment_recording(uint8_t segment_index);  // 检查段是否在录制
uint8_t loop_is_segment_playing(uint8_t segment_index);    // 检查段是否在播放

// 循环模式控制接口
void loop_set_mode(LoopMode_t mode);                       // 设置循环模式
LoopMode_t loop_get_mode(void);                            // 获取当前循环模式
uint8_t loop_is_song_mode(void);                           // 检查是否为歌曲模式
uint8_t loop_is_free_mode(void);                           // 检查是否为自由模式
void loop_update_master_segment_info(void);               // 更新主段信息（内部使用）

// uint32_t版本的处理函数（主要使用）
void loop_process_recording_uint32(uint32_t* audio_data, uint8_t* buffer, uint16_t length);
void loop_process_playback_uint32(uint32_t* output_data, uint8_t* buffer, uint16_t length);

void loop_timer_update(void);

// 状态查询函数
LoopState_t loop_get_state(void);
uint8_t loop_is_recording(void);
uint8_t loop_is_playing(void);
uint32_t loop_get_current_address(void);
uint32_t loop_get_record_length(void);

// 数据校验函数
uint8_t loop_verify_flash_data(uint32_t test_length);

// 数据转换函数声明
void convertUint32ArrayToUint8Array(const uint32_t *input, uint8_t *output, size_t size);
void convertUint8ArrayToUint32Array(const uint8_t *input, uint32_t *output, size_t size);

// ============================================================================
// AudioLooper接口实例声明
// ============================================================================
extern AudioLooper_t AudioLooper;

#endif /* __AUDIO_LOOPER_H__ */
