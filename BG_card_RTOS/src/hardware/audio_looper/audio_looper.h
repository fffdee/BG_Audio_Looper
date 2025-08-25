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
    LOOP_STATE_IDLE = 0,        // 空闲状态，等待录制
    LOOP_STATE_RECORDING = 1,   // 录制状态
    LOOP_STATE_PLAYING = 2      // 播放状态
} LoopState_t;

// 多段录音支持
#define MAX_SEGMENTS 4          // 最多支持4段录音

// Flash类型枚举
typedef enum {
    FLASH_TYPE_NOR = 0,         // NOR Flash (DEV_NOR)
    FLASH_TYPE_NAND = 1         // NAND Flash (DEV_NAND)
} FlashType_t;

// 段信息结构体
typedef struct {
    uint32_t start_address;     // 段起始地址（页对齐）
    uint32_t length_pages;      // 段长度（页数）
    uint8_t is_active;          // 段是否有效
} SegmentInfo_t;

// Loop管理器结构体
typedef struct {
    LoopState_t state;              // 当前状态
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

// 函数声明
void loop_init(void);
void loop_init_with_flash_type(FlashType_t flash_type);  // 新增：带Flash类型的初始化
void loop_reset(void);
void loop_handle_button_press(void);
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

#endif /* __AUDIO_LOOPER_H__ */
