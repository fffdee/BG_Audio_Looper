/**
 **************************************************************************************
 * @file    audio_looper.c
 * @brief   Audio looper functions implementation
 *
 * @author  BanGO
 * @version V1.0.0
 *
 * @Copyright (C) 2025, Audio Looper Project. All rights reserved.
 ************************ *****************************************************/

#include "audio_looper.h"
#include "debug.h"
#include "flash_devices.h"  /* 直接使用底层Flash API */
#include "BG_FlashMgr.h"  /* 兼容性保留 */
#include "type.h"
#include <nds32_intrinsic.h>
#include <math.h>
#include <string.h>

/* 静态辅助函数前置声明 */
static void metronome_update_timing_params(void);
static float metronome_generate_sine_sample(float freq, float* phase);
static void metronome_advance_beat(void);

// 全局Loop管理器，归纳所有looper相关变量
LoopManager_t g_loop_manager = {
    .state = LOOP_STATE_IDLE,
    .flash_type = FLASH_TYPE_NOR,
    .sector_address = 0,
    .record_length = 0,
    .play_position = 0,
    .is_initialized = 0,
    .is_new_recording = 0,
    .current_segment = 0,
    .active_segments = 0,
    .page_size = 256,
    .boundary_samples_valid = 0
};

// 校验相关变量（归纳到结构体）
static int16_t ReadBuf[96];

// 录音/播放统计信息（归纳到结构体）
static struct {
    uint32_t recording_sample_count;
    uint32_t playback_sample_count;
    int16_t last_recorded_sample;
    int16_t first_playback_sample;
} g_loop_stats = {0};


void convertUint8ArrayToInt16Array(const uint8_t *input, int16_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; ++i) {
            // 假设系统是小端
            int16_t sample = (int16_t)(input[i * 2]) | ((int16_t)(input[i * 2 + 1]) << 8);
            output[i] = sample;
        }
}

void convertUint32ArrayToUint8Array(const uint32_t *input, uint8_t *output, size_t size) {
	size_t i;
	for (i = 0; i < size; i++) {
        // 每个uint32_t转换为4个uint8_t，保持双声道数据完整
        // 假设系统是小端，uint32_t格式为: [右声道低8位][右声道高8位][左声道低8位][左声道高8位]
        output[i * 4]     = (uint8_t)(input[i] & 0xFF);         // 右声道低8位
        output[i * 4 + 1] = (uint8_t)((input[i] >> 8) & 0xFF);  // 右声道高8位
        output[i * 4 + 2] = (uint8_t)((input[i] >> 16) & 0xFF); // 左声道低8位
        output[i * 4 + 3] = (uint8_t)((input[i] >> 24) & 0xFF); // 左声道高8位
    }
}

void convertUint8ArrayToUint32Array(const uint8_t *input, uint32_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; i++) {
        // 将4个uint8_t重新组合为1个uint32_t，恢复双声道数据
        output[i] = (uint32_t)input[i * 4] |
                    ((uint32_t)input[i * 4 + 1] << 8) |
                    ((uint32_t)input[i * 4 + 2] << 16) |
                    ((uint32_t)input[i * 4 + 3] << 24);
    }
}

void convertInt16ArrayToUint8Array(const int16_t *input, uint8_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; ++i) {
        // 假设系统是小端
        output[i * 2] = (uint8_t)(input[i] & 0xFF); // 低8位
        output[i * 2 + 1] = (uint8_t)((input[i] >> 8) & 0xFF); // 高8位
    }
}
/**
 * @brief 初始化Loop管理器
 */
void loop_init(void)
{
    memset(&g_loop_manager, 0, sizeof(LoopManager_t));
    
    g_loop_manager.state = LOOP_STATE_IDLE;
    g_loop_manager.flash_type = FLASH_TYPE_NOR;  // 改为默认使用NAND Flash
    g_loop_manager.mode = LOOP_MODE_FREE;        // 默认使用自由模式
    g_loop_manager.sector_address = 0;
    g_loop_manager.record_length = 0;
    g_loop_manager.play_position = 0;
    g_loop_manager.is_initialized = 1;
    g_loop_manager.is_new_recording = 0;
    
    // 初始化多段录音参数
    g_loop_manager.current_segment = 0;
    g_loop_manager.active_segments = 0;
    g_loop_manager.page_size = 256;  // Flash页大小

    // 初始化所有段信息
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address = 0;
        g_loop_manager.segments[i].length_pages = 0;
        g_loop_manager.segments[i].length_bytes = 0;
        g_loop_manager.segments[i].play_position = 0;
        g_loop_manager.segments[i].state = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].is_active = 0;
    }
    
    // 初始化统计信息
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count = 0;
    g_loop_stats.last_recorded_sample = 0;
    g_loop_stats.first_playback_sample = 0;
    
    // 初始化节拍器
    metronome_init();
    
    /* 不再在初始化时擦除Flash，由用户手动触发（编码器右转）*/
    DBG("Loop manager initialized with multi-segment support (Flash erase deferred)\n");
    //loop_handle_button_press();
}

/**
 * @brief 使用指定Flash类型初始化Loop管理器
 * @param flash_type 要使用的Flash类型
 */
void loop_init_with_flash_type(FlashType_t flash_type)
{
    memset(&g_loop_manager, 0, sizeof(LoopManager_t));
    
    g_loop_manager.state = LOOP_STATE_IDLE;
    g_loop_manager.flash_type = flash_type;  // 使用指定的Flash类型
    g_loop_manager.mode = LOOP_MODE_FREE;    // 默认使用自由模式
    g_loop_manager.sector_address = 0;
    g_loop_manager.record_length = 0;
    g_loop_manager.play_position = 0;
    g_loop_manager.is_initialized = 1;
    g_loop_manager.is_new_recording = 0;
    g_loop_manager.chip_erase_pending = 0;
    
    // 初始化多段录音参数
    g_loop_manager.current_segment = 0;
    g_loop_manager.active_segments = 0;
    g_loop_manager.page_size = 256;  // Flash页大小

    // 初始化所有段信息
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address = 0;
        g_loop_manager.segments[i].length_pages = 0;
        g_loop_manager.segments[i].length_bytes = 0;
        g_loop_manager.segments[i].play_position = 0;
        g_loop_manager.segments[i].state = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].is_active = 0;
    }
    
    // 初始化统计信息
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count = 0;
    g_loop_stats.last_recorded_sample = 0;
    g_loop_stats.first_playback_sample = 0;
    
    // 初始化节拍器
    metronome_init();
    
    /* 不再在初始化时擦除Flash，由用户手动触发（编码器右转）*/
    DBG("Loop manager initialized with %s Flash support (Flash erase deferred)\n",
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
}

/**
 * @brief 重置Loop管理器（包括擦除Flash数据）
 */
void loop_reset(void)
{
    /* 1. 将所有正在录制/播放的段重置为未激活状态 */
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address = 0;
        g_loop_manager.segments[i].length_pages  = 0;
        g_loop_manager.segments[i].length_bytes  = 0;
        g_loop_manager.segments[i].play_position = 0;
        g_loop_manager.segments[i].state         = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].is_active     = 0;
    }

    /* 2. 重置管理器状态变量 */
    g_loop_manager.state              = LOOP_STATE_IDLE;
    g_loop_manager.sector_address     = 0;
    g_loop_manager.record_length      = 0;
    g_loop_manager.play_position      = 0;
    g_loop_manager.is_new_recording   = 0;
    g_loop_manager.current_segment    = 0;
    g_loop_manager.active_segments    = 0;

    /* 3. 清空统计信息 */
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count  = 0;
    g_loop_stats.last_recorded_sample   = 0;
    g_loop_stats.first_playback_sample  = 0;

    /* 4. 异步全片擦除 Flash0，擦除完成前阻止录制和播放 */
    FlashStatus_t ret = FlashPartition_LooperEraseChipAsync();
    if (ret == FLASH_OK) {
        g_loop_manager.chip_erase_pending = 1;
        DBG("[Looper] Reset: async chip erase started, REC/PLAY blocked\n");
    } else {
        g_loop_manager.chip_erase_pending = 0;
        DBG("[Looper] Reset: Flash erase failed (%d), proceeding without erase\n", ret);
    }
}

/**
 * @brief Flash全片擦除初始化（清除所有Looper数据并重新准备录制）
 *
 * 调用后 chip_erase_pending 置 1，在 Flash 擦除完成前
 * 所有录制和播放操作均被阻塞。
 * 每个音频帧自动轮询 BUSY 位，擦除完成后自动解除阻塞。
 */
void loop_flash_erase_reinit(void)
{
    if (!g_loop_manager.is_initialized) {
        DBG("[Looper] Not initialized, call loop_init() first\n");
        return;
    }

    /* 1. 立即停止所有活动 */
    g_loop_manager.state = LOOP_STATE_IDLE;

    /* 2. 清空所有段信息 */
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address = 0;
        g_loop_manager.segments[i].length_pages  = 0;
        g_loop_manager.segments[i].length_bytes  = 0;
        g_loop_manager.segments[i].play_position = 0;
        g_loop_manager.segments[i].state         = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].is_active     = 0;
    }
    g_loop_manager.current_segment  = 0;
    g_loop_manager.active_segments  = 0;
    g_loop_manager.sector_address   = 0;
    g_loop_manager.record_length    = 0;
    g_loop_manager.play_position    = 0;
    g_loop_manager.is_new_recording = 0;

    /* 3. 发出异步全片擦除命令 */
    FlashStatus_t ret = FlashPartition_LooperEraseChipAsync();
    if (ret != FLASH_OK) {
        DBG("[Looper] Flash erase reinit failed: %d\n", ret);
        return;
    }

    g_loop_manager.chip_erase_pending = 1;
    DBG("[Looper] Flash chip erase started, REC/PLAY blocked until complete\n");
}

/**
 * @brief 处理按键按下事件，支持段选择
 * 当段未激活时，第一次按下进入录音模式，再次按下停止录音并且开始播放
 * 当段播放时，按下停止播放
 * 当段停止时，按下开始播放
 * @param segment_index 段索引 (0-3)，如果为-1则使用传统模式
 */
void loop_handle_button_press(int8_t segment_index)
{
    if (!g_loop_manager.is_initialized) {
        DBG("Loop manager not initialized\n");
        return;
    }

    /* Flash擦除：先主动 poll 一次 BUSY，已完成则清零放行 */
    if (g_loop_manager.chip_erase_pending) {
        if (FlashPartition_LooperIsErasing()) {
            DBG("[Looper] Flash erase in progress, input ignored\n");
            return;
        }
        g_loop_manager.chip_erase_pending = 0;
        DBG("[Looper] Chip erase done (detected on button), unblocked\n");
    }
    
    // 如果指定了段索引，使用新的段控制
    if (segment_index >= 0 && segment_index < MAX_SEGMENTS) {
        loop_handle_segment_button(segment_index);
        return;
    }
    
    // 传统模式：维持向后兼容性
    
    switch(g_loop_manager.state)
    {
        case LOOP_STATE_IDLE:
            // 空闲状态：开始录制新段
            if (g_loop_manager.active_segments < MAX_SEGMENTS) {
                loop_start_new_segment();
                DBG("Start recording segment %d using %s Flash\n",
                    g_loop_manager.current_segment + 1,
                    g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
            } else {
                // 已达到最大段数，清除所有段重新开始
                loop_clear_all_segments();
                loop_start_new_segment();
                DBG("Max segments reached, cleared all and start new recording using %s Flash\n",
                    g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
            }
            break;
            
        case LOOP_STATE_RECORDING:
            // 录制状态：停止当前段录制并开始混音播放
            // 查找正在录制的段
            {
                uint8_t recording_segment = MAX_SEGMENTS;
                uint8_t i;
                for (i = 0; i < MAX_SEGMENTS; i++) {
                    if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
                        recording_segment = i;
                        break;
                    }
                }
                
                if (recording_segment < MAX_SEGMENTS) {
                    loop_stop_current_segment(recording_segment);
                    DBG("Stop recording segment %d, start playing %d segments\n",
                        recording_segment + 1, g_loop_manager.active_segments);
                } else {
                    DBG("No recording segment found\n");
                    g_loop_manager.state = LOOP_STATE_PLAYING;
                }
            }
            break;
            
        case LOOP_STATE_PLAYING:
            // 播放状态：如果还可以录制更多段，则开始录制下一段
            if (g_loop_manager.active_segments < MAX_SEGMENTS) {
                loop_start_new_segment();
                DBG("Start recording segment %d while playing using %s Flash\n", 
                    g_loop_manager.current_segment + 1,
                    g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
            } else {
                // 已达到最大段数，停止播放回到空闲状态
                g_loop_manager.state = LOOP_STATE_IDLE;
                DBG("Max segments reached, stop playing\n");
            }
            break;
            
        default:
            g_loop_manager.state = LOOP_STATE_IDLE;
            break;
    }
}

/**
 * @brief 处理编码器左转事件：清除所有段
 */
void loop_handle_encoder_left(void)
{
    if (!g_loop_manager.is_initialized) {
        return;
    }
    
    DBG("Encoder left: clear all segments\n");
    loop_clear_all_segments();
    g_loop_manager.state = LOOP_STATE_IDLE;

}

/**
 * @brief 处理编码器右转事件：停止一切活动并擦除全片
 */
void loop_handle_encoder_right(void)
{
    if (!g_loop_manager.is_initialized) {
        DBG("Loop manager not initialized\n");
        return;
    }
    
    // 停止所有活动
    g_loop_manager.state = LOOP_STATE_IDLE;

    
    // 擦除Looper分区 (7MB) - 使用新API
    DBG("Encoder right: Erasing Looper partition (7MB)\n");
    int32_t erase_result = BG_FlashMgr.EraseLooperAll();
    
    if (erase_result < 0) {
        DBG("Flash erase failed: %ld\n", (long)erase_result);
        return;
    }
    
    // 重置所有变量
    g_loop_manager.record_length = 0;
    g_loop_manager.play_position = 0;
    g_loop_manager.sector_address = 0;
    g_loop_manager.is_new_recording = 1;
    
    DBG("Looper partition erased, system reset to idle\n");
}

/**
 * @brief 设置Flash类型
 * @param flash_type Flash类型 (FLASH_TYPE_NOR 或 FLASH_TYPE_NAND)
 */
void loop_set_flash_type(FlashType_t flash_type)
{
    if (flash_type != FLASH_TYPE_NOR && flash_type != FLASH_TYPE_NAND) {
        DBG("Invalid flash type: %d\n", flash_type);
        return;
    }
    
    g_loop_manager.flash_type = flash_type;
    DBG("Flash type set to: %s (value=%d)\n", flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND", flash_type);
    
    // 注：新flash方案不需要切换flash类型，Looper固定在Flash#0
}

/**
 * @brief 获取当前Flash类型
 * @return 当前Flash类型
 */
FlashType_t loop_get_flash_type(void)
{
    return g_loop_manager.flash_type;
}

/**
 * @brief 获取当前Flash设备ID (用于BG_flash_manager调用)
 * @note 硬件只使用NOR Flash，此函数已废弃，保留向后兼容
 * @return DEV_NOR
 */
uint8_t loop_get_flash_device_id(void)
{
    // 硬件只支持NOR Flash，始终返回DEV_NOR
    // 新API (BG_FlashMgr) 不需要device_id
    return 0;  // DEV_NOR
}

/**
 * @brief 停止录制并准备播放
 */
void loop_stop_recording(void)
{
    if (g_loop_manager.state == LOOP_STATE_RECORDING) {
        // 硬件只使用NOR Flash，无需特殊处理

        // 记录录制长度并重置播放位置
        g_loop_manager.record_length = g_loop_manager.sector_address;
        g_loop_manager.play_position = 0;
        g_loop_manager.state = LOOP_STATE_PLAYING;

        
        DBG("Recording stopped manually: total_samples=%lu, record_length=%lu bytes, last_sample=%d\n",
            (unsigned long)g_loop_stats.recording_sample_count, (unsigned long)g_loop_manager.record_length, g_loop_stats.last_recorded_sample);
        
        // 重置地址指针准备下次录制
        g_loop_manager.sector_address = 0;
        g_loop_stats.recording_sample_count = 0;
    }
}

/**
 * @brief 处理录制逻辑
 * @param audio_data 音频数据
 * @param buffer 缓冲区
 * @param length 数据长度
 */
void loop_process_recording(int16_t* audio_data, uint8_t* buffer, uint16_t length)
{
    if (g_loop_manager.state != LOOP_STATE_RECORDING) {
        return;  // 不在录制状态
    }

    // 移除record_flag依赖，直接处理音频数据
    // 录制应该基于音频数据可用性，而不是定时器

    // 只使用Flash录制模式
    {
        // Flash录制逻辑 - 确保长度参数正确
        
        // 数据校验：检查输入音频数据是否有效
        uint16_t non_zero_count = 0;
        int32_t amplitude_sum = 0;
        int16_t max_amplitude = 0;
        uint16_t i;
        for (i = 0; i < length; i++) {
            int16_t sample = audio_data[i];
            if (sample != 0) {
                non_zero_count++;
                amplitude_sum += (sample < 0) ? -sample : sample;  // 手动实现abs
                if ((sample < 0 ? -sample : sample) > (max_amplitude < 0 ? -max_amplitude : max_amplitude)) {
                    max_amplitude = sample;
                }
            }
        }
        
        // 记录统计信息
        g_loop_stats.recording_sample_count += length;
        if (length > 0) {
            g_loop_stats.last_recorded_sample = audio_data[length - 1];
        }
        
        // 如果输入信号太弱，提示调整增益
        if (g_loop_stats.recording_sample_count % 200 == 0 && non_zero_count > 0) {
            int32_t avg_amplitude = amplitude_sum / non_zero_count;
            if (avg_amplitude < 100) {  // 信号较弱
                DBG("WARNING: Input signal weak, avg_amp=%ld, max=%d, consider increasing gain\n",
                    (long)avg_amplitude, max_amplitude);
            }
        }
        
        convertInt16ArrayToUint8Array(audio_data, buffer, length);
        
        // Flash页面大小通常是256字节，我们写入length*2字节的数据
        uint32_t bytes_to_write = length * 2;  // 16位音频转8位需要*2
        
        // 直接使用底层Flash API
        FlashStatus_t write_result = FlashPartition_LooperWrite(g_loop_manager.sector_address, buffer, bytes_to_write);
        
        if (write_result != FLASH_OK) {
            DBG("Flash write error at offset %lu: %d\n", 
                (unsigned long)g_loop_manager.sector_address, write_result);
            // 写入失败，停止录音
            g_loop_manager.record_length = g_loop_manager.sector_address;
            g_loop_manager.play_position = 0;
            g_loop_manager.state = LOOP_STATE_PLAYING;
            return;
        }

        g_loop_stats.recording_sample_count++;
        g_loop_manager.sector_address += bytes_to_write;  // 按实际写入字节数递增

//        if (rec % 500 == 0) {  // 减少调试输出频率，避免影响实时性
//            //DBG("Flash recording: packets=%d, addr=%d, bytes=%d, nonzero=%d, avg_amp=%d, last_sample=%d\n",
//                rec, g_loop_manager.sector_address, bytes_to_write, non_zero_count,
//                non_zero_count > 0 ? amplitude_sum / non_zero_count : 0, last_recorded_sample);
//        }
        
        // 检查Flash存储空间 - Looper分区是7MB
        uint32_t looper_max_size = 7 * 1024 * 1024;  // 7MB Looper分区
        if (g_loop_manager.sector_address >= looper_max_size) {
            DBG("Looper partition full, stop recording. Offset: %lu, Max: %lu\n", 
                (unsigned long)g_loop_manager.sector_address, (unsigned long)looper_max_size);
            
            g_loop_manager.record_length = g_loop_manager.sector_address;  // 正确记录录制长度
            g_loop_manager.play_position = 0;  // 重置播放位置
            g_loop_manager.state = LOOP_STATE_PLAYING;

            DBG("Recording finished: total_samples=%lu, record_length=%lu, last_sample=%d\n", 
                (unsigned long)g_loop_stats.recording_sample_count, (unsigned long)g_loop_manager.record_length, g_loop_stats.last_recorded_sample);
            
            g_loop_stats.recording_sample_count = 0;
            g_loop_stats.playback_sample_count = 0;
        }
        
        // 移除外部变量依赖 - 不再需要同步sectorAddress
    }
}

/**
 * @brief 处理播放逻辑
 * @param output_data 输出音频数据
 * @param buffer 缓冲区
 * @param length 数据长度
 */
void loop_process_playback(int16_t* output_data, uint8_t* buffer, uint16_t length)
{
    if (g_loop_manager.state != LOOP_STATE_PLAYING) {
        return;  // 不在播放状态，保持原始音频数据不变
    }
    
    uint16_t i;
    
    // 只使用Flash播放模式
    {
        // Flash播放逻辑
        if (g_loop_manager.record_length == 0) {
            DBG("No recorded data in flash, record_length=0\n");
            return;  // 没有录制数据，保持原始音频
        }
        
        // 确保播放位置有效
        if (g_loop_manager.play_position >= g_loop_manager.record_length) {
            g_loop_manager.play_position = 0;
        }
        
        // 计算要读取的字节数
        uint32_t bytes_to_read = length * 2;  // 16位音频需要读取length*2字节

        // 确保不会超过录制长度
        if (g_loop_manager.play_position + bytes_to_read > g_loop_manager.record_length) {
            bytes_to_read = g_loop_manager.record_length - g_loop_manager.play_position;
            if (bytes_to_read == 0 || bytes_to_read % 2 != 0) {
                // 已到末尾或奇数字节，重新开始
                g_loop_manager.play_position = 0;
                bytes_to_read = (length * 2 > g_loop_manager.record_length) ?
                               g_loop_manager.record_length : length * 2;
                if (bytes_to_read % 2 != 0) bytes_to_read--;  // 确保偶数字节
                g_loop_stats.playback_sample_count++;
                DBG("Flash loop restart, count: %lu, length: %lu, reading: %lu\n",
                    (unsigned long)g_loop_stats.playback_sample_count, (unsigned long)g_loop_manager.record_length, (unsigned long)bytes_to_read);
            }
        }
        
        // 确保有效的读取长度
        if (bytes_to_read == 0) {
            DBG("Warning: bytes_to_read=0, skipping playback\n");
            return;
        }
        
        // 读取Flash数据 - 直接使用底层API
        FlashStatus_t read_result = FlashPartition_LooperRead(g_loop_manager.play_position, buffer, bytes_to_read);
        
        if (read_result != FLASH_OK) {
            DBG("Flash read error at offset %lu: %d\n", 
                (unsigned long)g_loop_manager.play_position, read_result);
            return;
        }
        
        convertUint8ArrayToInt16Array(buffer, ReadBuf, bytes_to_read/2);
        
        // 数据校验：检查读取的音频数据
        uint16_t valid_samples = bytes_to_read / 2;
        uint16_t non_zero_read = 0;
        int32_t read_amplitude_sum = 0;
        uint16_t j;
        for (j = 0; j < valid_samples; j++) {
            if (ReadBuf[j] != 0) {
                non_zero_read++;
                read_amplitude_sum += (ReadBuf[j] < 0) ? -ReadBuf[j] : ReadBuf[j];  // 手动实现abs
            }
        }
        
        // 记录第一个播放的样本用于校验
        if (g_loop_manager.play_position == 0 && valid_samples > 0) {
            g_loop_stats.first_playback_sample = ReadBuf[0];
            DBG("First playback sample: %d (should match last recorded: %d)\n",
                g_loop_stats.first_playback_sample, g_loop_stats.last_recorded_sample);
        }
        
        // 混合音频数据
        uint16_t samples_to_mix = (valid_samples < length) ? valid_samples : length;
        for (i = 0; i < samples_to_mix; i++) {
            int32_t mixed = (int32_t)output_data[i] + (int32_t)ReadBuf[i];
            output_data[i] = __nds32__clips(mixed, 15);  // 16位饱和限制
        }
        
        g_loop_stats.playback_sample_count += samples_to_mix;
        g_loop_manager.play_position += bytes_to_read;
        
        // 移除外部变量依赖
        // sectorAddress = g_loop_manager.play_position;
    }
}

/**
 * @brief 定时器更新函数，在1ms中断中调用
 * 处理所有需要实时更新的状态
 */
void loop_timer_update(void)
{
    if (!g_loop_manager.is_initialized) {
        return;
    }
    
    // 可以在这里添加需要定时更新的逻辑
    // 例如：LED指示、状态监控等

    // 移除外部变量同步
    // sectorAddress = (g_loop_manager.state == LOOP_STATE_PLAYING) ?
    //                g_loop_manager.play_position : g_loop_manager.sector_address;
}

/**
 * @brief 获取当前循环状态
 */
LoopState_t loop_get_state(void)
{
    return g_loop_manager.state;
}

/**
 * @brief 检查是否正在录制
 */
uint8_t loop_is_recording(void)
{
    return (g_loop_manager.state == LOOP_STATE_RECORDING || 
            g_loop_manager.state == LOOP_STATE_RECORDING_AND_PLAYING) ? 1 : 0;
}

/**
 * @brief 检查是否正在播放
 */
uint8_t loop_is_playing(void)
{
    return (g_loop_manager.state == LOOP_STATE_PLAYING ||
            g_loop_manager.state == LOOP_STATE_RECORDING_AND_PLAYING) ? 1 : 0;
}

/**
 * @brief 获取当前地址
 */
uint32_t loop_get_current_address(void)
{
    return (g_loop_manager.state == LOOP_STATE_PLAYING) ? 
           g_loop_manager.play_position : g_loop_manager.sector_address;
}

/**
 * @brief 获取录制长度
 */
uint32_t loop_get_record_length(void)
{
    return g_loop_manager.record_length;
}

// ============================================================================
// 循环模式控制函数实现
// ============================================================================

/**
 * @brief 设置循环模式
 * @param mode 要设置的循环模式
 */
void loop_set_mode(LoopMode_t mode)
{
    if (mode == LOOP_MODE_SONG || mode == LOOP_MODE_FREE) {
        g_loop_manager.mode = mode;
        DBG("Loop mode set to %s\n", mode == LOOP_MODE_SONG ? "SONG" : "FREE");
        
        // 如果切换到歌曲模式，需要重新计算主段信息
        if (mode == LOOP_MODE_SONG) {
            loop_update_master_segment_info();
        }
    }
}

/**
 * @brief 获取当前循环模式
 * @return 当前循环模式
 */
LoopMode_t loop_get_mode(void)
{
    return g_loop_manager.mode;
}

/**
 * @brief 检查是否为歌曲模式
 * @return 1如果是歌曲模式，0如果不是
 */
uint8_t loop_is_song_mode(void)
{
    return (g_loop_manager.mode == LOOP_MODE_SONG) ? 1 : 0;
}

/**
 * @brief 检查是否为自由模式
 * @return 1如果是自由模式，0如果不是
 */
uint8_t loop_is_free_mode(void)
{
    return (g_loop_manager.mode == LOOP_MODE_FREE) ? 1 : 0;
}

/**
 * @brief 更新主段信息（内部使用）
 * 在歌曲模式下，找到最长的段作为主段，用于循环基准
 */
void loop_update_master_segment_info(void)
{
    uint32_t max_length = 0;
    uint8_t master_index = 0;
    uint8_t i;
    
    // 找到最长的段
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].is_active && 
            g_loop_manager.segments[i].length_bytes > max_length) {
            max_length = g_loop_manager.segments[i].length_bytes;
            master_index = i;
        }
    }
    
    g_loop_manager.master_segment_length = max_length;
    g_loop_manager.master_segment_index = master_index;
    
    if (max_length > 0) {
        DBG("Master segment updated: index %u, length %u bytes\n", 
            (unsigned int)master_index, (unsigned int)max_length);
    }
}

/**
 * @brief 段录制处理函数 - 基于段实例
 * @param segment_index 要录制的段索引
 * @param audio_data uint32_t格式的音频数据
 * @param buffer 临时缓冲区用于数据转换
 * @param length uint32_t数据的数量
 */
void loop_process_segment_recording(uint8_t segment_index, uint32_t* audio_data, uint8_t* buffer, uint16_t length)
{
    static uint32_t rec_call_count = 0;
    
    if (segment_index >= MAX_SEGMENTS) {
        return;  // 无效段索引
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state != SEGMENT_RECORDING) {
        return;  // 段不在录制状态
    }
    
    /* 限制最大长度，避免缓冲区溢出 (192 bytes / 4 = 48 samples max) */
    if (length > 48) {
        length = 48;
    }
    
    uint32_t bytes_to_write = length * 4;  // 实际要写入的字节数
    uint8_t write_buffer[192];
    
    // 直接录制原始音频数据
    convertUint32ArrayToUint8Array(audio_data, write_buffer, length);
    uint32_t write_offset = segment->start_address + 
                            segment->length_pages * g_loop_manager.page_size;

    // 直接使用底层Flash API写入
    rec_call_count++;
    if (rec_call_count % 100 == 1) {
        // 检查音频数据是否有非零值
        uint16_t non_zero = 0;
        uint16_t k;
        for (k = 0; k < length && k < 48; k++) {
            if (audio_data[k] != 0) non_zero++;
        }
//        DBG("REC[%lu]: seg=%d offset=0x%lX pages=%lu nonzero=%d/%d sample0=0x%08lX\n",
//            (unsigned long)rec_call_count, segment_index,
//            (unsigned long)write_offset, (unsigned long)segment->length_pages,
//            non_zero, length, (unsigned long)audio_data[0]);
    }
    
    // 直接使用底层Flash API写入
    FlashStatus_t write_result = FlashPartition_LooperWrite(write_offset, write_buffer, bytes_to_write);
    
    if (write_result != FLASH_OK) {
        DBG("Flash write error at offset %lu: %d\n", 
            (unsigned long)write_offset, write_result);
        // 写入失败，停止录制该段
        segment->state = SEGMENT_INACTIVE;
        return;
    }
    
    segment->length_pages++;  // 更新段长度
    segment->length_bytes = segment->length_pages * g_loop_manager.page_size;  // 同步更新字节数
    // 只在段开始和结束时打印，避免频繁输出
//        if (segment->length_pages == 1 || segment->length_pages % 100 == 0) {
//            DBG("Segment %d recording: page %lu written\n", segment_index, (unsigned long)segment->length_pages);
//        }
}

/**
 * @brief 总录制处理函数 - 处理所有正在录制的段
 * @param audio_data uint32_t格式的音频数据
 * @param buffer 临时缓冲区用于数据转换
 * @param length uint32_t数据的数量
 */
void loop_process_recording_uint32(uint32_t* audio_data, uint8_t* buffer, uint16_t length)
{
    /* 全片擦除轮询：每帧检测一次 BUSY 位
     * BUSY=1 跳过本帧；BUSY=0 则解除阻塞并继续处理 */
    if (g_loop_manager.chip_erase_pending) {
        if (FlashPartition_LooperIsErasing()) {
            return;
        }
        g_loop_manager.chip_erase_pending = 0;
        DBG("[Looper] Chip erase complete, REC/PLAY unblocked\n");
    }

    // 处理所有正在录制的段
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
            loop_process_segment_recording(i, audio_data, buffer, length);
        }
    }
}

/**
 * @brief 段播放处理函数 - 基于段实例
 * @param segment_index 要播放的段索引
 * @param output_data uint32_t格式的输出音频数据（用于混音）
 * @param buffer 临时缓冲区用于数据转换
 * @param length uint32_t数据的数量
 * @return 1=成功播放, 0=无数据播放
 */
uint8_t loop_process_segment_playback(uint8_t segment_index, uint32_t* output_data, uint8_t* buffer, uint16_t length)
{
    static uint32_t play_call_count = 0;
    
    if (segment_index >= MAX_SEGMENTS) {
        return 0;  // 无效段索引
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state != SEGMENT_PLAYING || !segment->is_active) {
        return 0;  // 段不在播放状态或未激活
    }

    if (segment->length_pages == 0) {
        return 0;  // 段没有数据
    }
    
    // 检查播放位置是否需要循环
    if (segment->play_position >= segment->length_pages) {
        // 只在循环重置时打印一次
        if (segment->play_position == segment->length_pages) {
            DBG("Segment %d loop: reset position from %lu to 0 (length=%lu)\n",
                segment_index, (unsigned long)segment->play_position, (unsigned long)segment->length_pages);
        }
        segment->play_position = 0;  // 重置到段开头
    }
    
    // 读取段数据 - 直接使用底层Flash API
    uint32_t segment_offset = segment->start_address + segment->play_position * g_loop_manager.page_size;
    
    // 直接使用底层API
    FlashStatus_t read_result = FlashPartition_LooperRead(segment_offset, buffer, 192);
    
    if (read_result != FLASH_OK) {
        DBG("Flash read error at offset %lu: %d\n", 
            (unsigned long)segment_offset, read_result);
        return 0;  // 读取失败
    }
    
    uint32_t segment_data[48];
    uint32_t samples_to_read = (length < 48) ? length : 48;
    convertUint8ArrayToUint32Array(buffer, segment_data, samples_to_read);
    
    // Debug: 每100次打印一次播放状态
    play_call_count++;
    if (play_call_count % 100 == 1) {
        uint16_t non_zero = 0;
        uint16_t k;
        for (k = 0; k < samples_to_read; k++) {
            if (segment_data[k] != 0) non_zero++;
        }
//        DBG("PLAY[%lu]: seg=%d offset=0x%lX pos=%lu/%lu nonzero=%d/%lu data0=0x%08lX\n",
//            (unsigned long)play_call_count, segment_index,
//            (unsigned long)segment_offset, (unsigned long)segment->play_position,
//            (unsigned long)segment->length_pages, non_zero, (unsigned long)samples_to_read,
//            (unsigned long)segment_data[0]);
    }
    
    // 段开头淡入处理
    if (segment->play_position == 0) {
        uint16_t fade_samples = (samples_to_read < 16) ? samples_to_read : 16;
        uint16_t j;
        for (j = 0; j < fade_samples; j++) {
            int16_t left = (int16_t)(segment_data[j] & 0xFFFF);
            int16_t right = (int16_t)((segment_data[j] >> 16) & 0xFFFF);
            
            uint16_t fade_factor = (j * 100) / fade_samples;  // 0-100%淡入
            left = (int16_t)((int32_t)left * fade_factor / 100);
            right = (int16_t)((int32_t)right * fade_factor / 100);
            
            segment_data[j] = ((uint32_t)(uint16_t)right << 16) | ((uint32_t)(uint16_t)left & 0xFFFF);
        }
    }
    
    // 混音到输出数据
    uint16_t j;
    for (j = 0; j < samples_to_read; j++) {
        int16_t seg_left = (int16_t)(segment_data[j] & 0xFFFF);
        int16_t seg_right = (int16_t)((segment_data[j] >> 16) & 0xFFFF);
        int16_t out_left = (int16_t)(output_data[j] & 0xFFFF);
        int16_t out_right = (int16_t)((output_data[j] >> 16) & 0xFFFF);
        
        // 混音：每段贡献60%音量
        int32_t new_left = (int32_t)out_left + ((int32_t)seg_left * 6 / 10);
        int32_t new_right = (int32_t)out_right + ((int32_t)seg_right * 6 / 10);
        
        // 软限幅
        new_left = __nds32__clips(new_left, 15);
        new_right = __nds32__clips(new_right, 15);
        
        output_data[j] = ((uint32_t)(uint16_t)new_right << 16) | ((uint32_t)(uint16_t)new_left & 0xFFFF);
    }
    
    // 更新段播放位置
    segment->play_position++;

    // 不频繁打印播放状态，避免卡顿
    // DBG("Segment %d playback: position %lu/%lu\n",
    //     segment_index, (unsigned long)segment->play_position, (unsigned long)segment->length_pages);

    return 1;  // 成功播放
}

/**
 * @brief 总播放处理函数 - 混音所有正在播放的段
 * @param output_data uint32_t格式的输出音频数据
 * @param buffer 临时缓冲区用于数据转换
 * @param length uint32_t数据的数量
 */
void loop_process_playback_uint32(uint32_t* output_data, uint8_t* buffer, uint16_t length)
{
    /* 全片擦除轮询（与 recording 对称，以防只有播放任务在运行）
     * BUSY=0 时解除阻塞并正常播放（此时各段均为 INACTIVE，输出静音即可） */
    if (g_loop_manager.chip_erase_pending) {
        if (FlashPartition_LooperIsErasing()) {
            uint16_t k;
            for (k = 0; k < length; k++) output_data[k] = 0;
            return;
        }
        g_loop_manager.chip_erase_pending = 0;
        DBG("[Looper] Chip erase complete (detected in playback), REC/PLAY unblocked\n");
    }

    // 清零输出缓冲区
    uint16_t i;
    for (i = 0; i < length; i++) {
        output_data[i] = 0;
    }
    
    // 统计播放的段数
    uint8_t playing_count = 0;
    
    // 处理所有正在播放的段
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (loop_process_segment_playback(i, output_data, buffer, length)) {
            playing_count++;
        }
    }
    
    if (playing_count > 0) {
        // 增大整体播放音量
        for (i = 0; i < length; i++) {
            int16_t left = (int16_t)(output_data[i] & 0xFFFF);
            int16_t right = (int16_t)((output_data[i] >> 16) & 0xFFFF);
            
            // 增益1.5倍
            int32_t boosted_left = (int32_t)left * 3 / 2;
            int32_t boosted_right = (int32_t)right * 3 / 2;
            
            // 软限幅
            boosted_left = __nds32__clips(boosted_left, 15);
            boosted_right = __nds32__clips(boosted_right, 15);
            
            output_data[i] = ((uint32_t)(uint16_t)boosted_right << 16) | 
                            ((uint32_t)(uint16_t)boosted_left & 0xFFFF);
        }
        
        // 不频繁打印混音状态，避免卡顿
        // DBG("Mixed %d segments playback\n", playing_count);
    }
    
    /* 注意：Effect Graph 模式下节拍器由图的 Metronome 源节点单独处理，
     * 此处不再调用 metronome_process_audio，避免重复叠加。
     * 传统模式 (AudioLoopMinimal) 会在调用本函数后显式调用 metronome_process_audio。 */
}

/**
 * @brief 开始录制新段
 */
void loop_start_new_segment(void)
{
    // 找到第一个未激活的段
    uint8_t new_segment = MAX_SEGMENTS;
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (!g_loop_manager.segments[i].is_active) {
            new_segment = i;
            break;
        }
    }
    
    if (new_segment >= MAX_SEGMENTS) {
        DBG("Cannot start new segment: maximum segments reached\n");
        return;
    }
    
    // Flash0 全片归 Looper，段 0 固定从地址 0 开始，后续段紧接已用空间
    uint32_t max_end_address = 0x000000;  // 基地址：Flash0 起始
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].is_active && g_loop_manager.segments[i].length_pages > 0) {
            uint32_t end_addr = g_loop_manager.segments[i].start_address +
                               g_loop_manager.segments[i].length_pages * g_loop_manager.page_size;
            if (end_addr > max_end_address) {
                max_end_address = end_addr;
            }
        }
    }

    // 页对齐
    if (max_end_address % g_loop_manager.page_size != 0) {
        max_end_address = ((max_end_address / g_loop_manager.page_size) + 1) * g_loop_manager.page_size;
    }

    uint32_t start_address = max_end_address;

    DBG("Segment %d: start_address = 0x%08lX\n", new_segment, (unsigned long)start_address);

    // 擦除由 loop_flash_erase_reinit() 或 loop_reset() 在录制前显式触发，此处不再重复擦除

    // 初始化新段
    g_loop_manager.segments[new_segment].start_address = start_address;
    g_loop_manager.segments[new_segment].length_pages = 0;  // 从0开始，正常录制
    g_loop_manager.segments[new_segment].length_bytes = 0;  // 字节数也从0开始
    g_loop_manager.segments[new_segment].is_active = 1;
    g_loop_manager.segments[new_segment].state = SEGMENT_RECORDING;
    g_loop_manager.segments[new_segment].play_position = 0;
    
    // 更新活跃段计数
    g_loop_manager.active_segments++;
    
    // 设置全局状态为录制（兼容性）
    g_loop_manager.state = LOOP_STATE_RECORDING;
    g_loop_manager.is_new_recording = 1;
    g_loop_stats.recording_sample_count = 0;
    
    DBG("Started segment %d at address 0x%08lX using %s Flash\n",
        new_segment, (unsigned long)start_address,
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
}

/**
 * @brief 停止指定段录制
 * @param segment_index 要停止的段索引
 */
void loop_stop_current_segment(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        DBG("Invalid segment index: %d\n", segment_index);
        return;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state != SEGMENT_RECORDING) {
        DBG("Segment %d is not recording\n", segment_index);
        return;
    }
    
    // 在录制结尾复制前面的数据，确保循环平滑
    uint8_t copy_pages_to_add = 10;  // 复制前10页数据
    uint8_t copy_buffer[192];
    
    // 确保有数据可以复制
    if (segment->length_pages == 0) {
        DBG("Warning: No data recorded for segment %d, marking inactive\n", segment_index);
        segment->state = SEGMENT_INACTIVE;
        segment->is_active = 0;
        g_loop_manager.active_segments = (g_loop_manager.active_segments > 0)
                                         ? g_loop_manager.active_segments - 1 : 0;
        loop_update_global_state();
        return;
    }
    
    // 写入复制的数据页到当前段结尾
    uint8_t page_count;
    for (page_count = 0; page_count < copy_pages_to_add; page_count++) {
        // 计算要复制的源页地址（循环使用段开头的数据）
        uint32_t source_page_index = page_count % segment->length_pages;
        uint32_t source_offset = segment->start_address + source_page_index * g_loop_manager.page_size;
        
        // 读取源页数据 - 直接使用底层API
        FlashStatus_t read_result = FlashPartition_LooperRead(source_offset, copy_buffer, 192);
        if (read_result != FLASH_OK) {
            DBG("Flash read error at offset %lu: %d\n", 
                (unsigned long)source_offset, read_result);
            break;
        }
        
        // 写入到段结尾 - 直接使用底层API
        uint32_t dest_offset = segment->start_address + 
                               (segment->length_pages + page_count) * g_loop_manager.page_size;
        FlashStatus_t write_result = FlashPartition_LooperWrite(dest_offset, copy_buffer, 192);
        if (write_result != FLASH_OK) {
            DBG("Flash write error at offset %lu: %d\n", 
                (unsigned long)dest_offset, write_result);
            break;
        }
    }
    
    // 更新段长度（包含复制的页）
    segment->length_pages += copy_pages_to_add;
    segment->length_bytes = segment->length_pages * g_loop_manager.page_size;  // 同步更新字节数
    
    DBG("Stop segment %d: recorded %lu pages (%lu bytes) with end-copy (copied %d pages)\n", 
        segment_index, (unsigned long)(segment->length_pages - copy_pages_to_add),
        (unsigned long)segment->length_bytes, copy_pages_to_add);
    
    if (segment->length_pages == 0) {
        // 如果没有录制任何数据，标记段为无效
        segment->is_active = 0;
        segment->state = SEGMENT_INACTIVE;
        DBG("Segment %d has no data, marked as inactive\n", segment_index);
    } else {
        // 设置段为播放状态
        segment->state = SEGMENT_PLAYING;
        segment->play_position = 0;  // 重置播放位置

        DBG("Stopped segment %d: %lu pages, set to PLAYING state\n",
            segment_index, (unsigned long)segment->length_pages);
    }
    
    // 更新全局状态
    loop_update_global_state();
}

/**
 * @brief 获取已录制段数
 */
uint8_t loop_get_segment_count(void)
{
    return g_loop_manager.active_segments;
}

/**
 * @brief 清除所有段
 */
void loop_clear_all_segments(void)
{
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address = 0;
        g_loop_manager.segments[i].length_pages = 0;
        g_loop_manager.segments[i].length_bytes = 0;
        g_loop_manager.segments[i].is_active = 0;
        g_loop_manager.segments[i].state = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].play_position = 0;
    }
    
    g_loop_manager.active_segments = 0;
    g_loop_manager.current_segment = 0;
    g_loop_manager.sector_address = 0;
    g_loop_manager.play_position = 0;
    
    // 擦除Looper分区 - 使用新API
    DBG("Clearing all segments and erasing Looper partition...\n");
    int32_t erase_result = BG_FlashMgr.EraseLooperAll();
    if (erase_result < 0) {
        DBG("Flash erase failed: %ld\n", (long)erase_result);
    } else {
        DBG("All segments cleared\n");
    }
}

// ============================================================================
// 单段精细控制函数实现
// ============================================================================

/**
 * @brief 处理指定段的按键操作
 * @param segment_index 段索引 (0-3)
 */
void loop_handle_segment_button(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        DBG("Invalid segment index: %d\n", segment_index);
        return;
    }

    /* Flash擦除：先主动 poll 一次 BUSY，已完成则清零放行 */
    if (g_loop_manager.chip_erase_pending) {
        if (FlashPartition_LooperIsErasing()) {
            DBG("[Looper] Flash erase in progress, input ignored\n");
            return;
        }
        g_loop_manager.chip_erase_pending = 0;
        DBG("[Looper] Chip erase done (detected on segment button), unblocked\n");
    }

    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    switch (segment->state) {
        case SEGMENT_INACTIVE:
            // 段未激活：开始录制
            loop_set_segment_recording(segment_index);
            DBG("Segment %d: INACTIVE -> RECORDING\n", segment_index);
            break;
            
        case SEGMENT_RECORDING:
        {
            // 段录制中：停止录制并开始播放
            loop_stop_current_segment(segment_index);
            DBG("Segment %d: RECORDING -> PLAYING (stopped recording)\n", segment_index);
            break;
        }
            
        case SEGMENT_PLAYING:
            // 段播放中：停止播放
            loop_set_segment_stopped(segment_index);
            DBG("Segment %d: PLAYING -> STOPPED\n", segment_index);
            break;
            
        case SEGMENT_STOPPED:
            // 段已停止：开始播放
            loop_set_segment_playing(segment_index);
            DBG("Segment %d: STOPPED -> PLAYING\n", segment_index);
            break;
    }
}

/**
 * @brief 获取指定段的状态
 * @param segment_index 段索引 (0-3)
 * @return 段状态
 */
SegmentState_t loop_get_segment_state(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return SEGMENT_INACTIVE;
    }
    return g_loop_manager.segments[segment_index].state;
}

/**
 * @brief 获取指定段已录制的页数
 * @param segment_index 段索引 (0-3)
 * @return 已录制页数（0 表示没有数据）
 */
uint32_t loop_get_segment_length_pages(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return 0;
    }
    return g_loop_manager.segments[segment_index].length_pages;
}

/**
 * @brief 根据各段状态智能更新全局状态
 */
void loop_update_global_state(void)
{
    uint8_t has_recording = 0;
    uint8_t has_playing = 0;
    uint8_t i;
    
    // 统计各段状态
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
            has_recording = 1;
        }
        if (g_loop_manager.segments[i].state == SEGMENT_PLAYING) {
            has_playing = 1;
        }
    }
    
    // 根据段状态设置全局状态
    if (has_recording && has_playing) {
        g_loop_manager.state = LOOP_STATE_RECORDING_AND_PLAYING;
        DBG("Global state updated: RECORDING_AND_PLAYING (recording=%d, playing=%d)\n", has_recording, has_playing);
    } else if (has_recording) {
        g_loop_manager.state = LOOP_STATE_RECORDING;
        DBG("Global state updated: RECORDING\n");
    } else if (has_playing) {
        g_loop_manager.state = LOOP_STATE_PLAYING;
        DBG("Global state updated: PLAYING\n");
    } else {
        g_loop_manager.state = LOOP_STATE_IDLE;
        DBG("Global state updated: IDLE\n");
    }
}

/**
 * @brief 设置段进入录制状态
 * @param segment_index 段索引 (0-3)
 */
void loop_set_segment_recording(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    // 如果段未激活，需要先调用loop_start_new_segment()来初始化并擦除Flash
    if (segment->state == SEGMENT_INACTIVE) {
        // 调用loop_start_new_segment()来正确初始化段（包括Flash擦除）
        loop_start_new_segment();
        return;  // loop_start_new_segment()已经设置了状态
    }
    
    segment->state = SEGMENT_RECORDING;
    
    // 智能更新全局状态：不干扰其他段的播放
    loop_update_global_state();
    g_loop_manager.is_new_recording = 1;
    g_loop_stats.recording_sample_count = 0;
}

/**
 * @brief 设置段进入播放状态
 * @param segment_index 段索引 (0-3)
 */
void loop_set_segment_playing(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    // 检查段是否可以播放：必须是已激活的段，并且不是INACTIVE状态
    if (segment->state == SEGMENT_INACTIVE) {
        DBG("Cannot play segment %d: segment is inactive\n", segment_index);
        return;
    }
    
    // 如果段还没有数据，不能播放
    if (segment->length_pages == 0) {
        DBG("Cannot play segment %d: no recorded data\n", segment_index);
        return;
    }
    
    segment->state = SEGMENT_PLAYING;
    segment->play_position = 0;  // 重置播放位置

    // 智能更新全局状态：不干扰其他段
    loop_update_global_state();
    
    DBG("Segment %d set to PLAYING state\n", segment_index);
}

/**
 * @brief 设置段进入停止状态
 * @param segment_index 段索引 (0-3)
 */
void loop_set_segment_stopped(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state == SEGMENT_RECORDING) {
        // 如果是从录制状态停止，需要保存录制信息
        segment->length_bytes = segment->length_pages * g_loop_manager.page_size;
        
        DBG("Segment %d recording stopped: %lu pages\n", 
            segment_index, (unsigned long)segment->length_pages);
    }
    
    segment->state = SEGMENT_STOPPED;
    
    // 智能更新全局状态：不干扰其他段
    loop_update_global_state();
    
    DBG("Segment %d set to STOPPED state\n", segment_index);
}

/**
 * @brief 检查指定段是否在录制
 * @param segment_index 段索引 (0-3)
 * @return 1=正在录制, 0=未录制
 */
uint8_t loop_is_segment_recording(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return 0;
    }
    return (g_loop_manager.segments[segment_index].state == SEGMENT_RECORDING) ? 1 : 0;
}

/**
 * @brief 检查指定段是否在播放
 * @param segment_index 段索引 (0-3)
 * @return 1=正在播放, 0=未播放
 */
uint8_t loop_is_segment_playing(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return 0;
    }
    return (g_loop_manager.segments[segment_index].state == SEGMENT_PLAYING) ? 1 : 0;
}

// ============================================================================
// AudioLooper接口实现函数（内部实现）
// ============================================================================

/**
 * @brief AudioLooper接口：初始化
 */
static void AudioLooper_Init(void) {
    loop_init();
}

/**
 * @brief AudioLooper接口：使用指定Flash类型初始化
 */
static void AudioLooper_InitWithFlashType(FlashType_t flash_type) {
    loop_init_with_flash_type(flash_type);
}

/**
 * @brief AudioLooper接口：重置
 */
static void AudioLooper_Reset(void) {
    loop_reset();
}

/**
 * @brief AudioLooper接口：设置Flash类型
 */
static LoopResult_t AudioLooper_SetFlashType(FlashType_t flash_type) {
    loop_set_flash_type(flash_type);
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：处理按键按下
 */
static LoopResult_t AudioLooper_ButtonPress(void) {
    loop_handle_button_press(-1);  // 使用传统模式
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：处理指定段的按键按下
 * @param segment_index 段索引 (0-3)
 */
static LoopResult_t AudioLooper_SegmentButtonPress(uint8_t segment_index) {
    if (segment_index >= MAX_SEGMENTS) {
        return LOOP_RESULT_ERROR;
    }
    loop_handle_button_press(segment_index);
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：处理编码器左转
 */
static LoopResult_t AudioLooper_EncoderLeft(void) {
    loop_handle_encoder_left();
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：处理编码器右转
 */
static LoopResult_t AudioLooper_EncoderRight(void) {
    loop_handle_encoder_right();
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：停止录制
 */
static LoopResult_t AudioLooper_StopRecording(void) {
    loop_stop_recording();
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：处理录制（16位）
 */
static void AudioLooper_ProcessRecording(int16_t* audio_data, uint8_t* buffer, uint16_t length) {
    loop_process_recording(audio_data, buffer, length);
}

/**
 * @brief AudioLooper接口：处理播放（16位）
 */
static void AudioLooper_ProcessPlayback(int16_t* output_data, uint8_t* buffer, uint16_t length) {
    loop_process_playback(output_data, buffer, length);
}

/**
 * @brief AudioLooper接口：处理录制（32位）
 */
static void AudioLooper_ProcessRecording32(uint32_t* audio_data, uint8_t* buffer, uint16_t length) {
    loop_process_recording_uint32(audio_data, buffer, length);
}

/**
 * @brief AudioLooper接口：处理播放（32位）
 */
static void AudioLooper_ProcessPlayback32(uint32_t* output_data, uint8_t* buffer, uint16_t length) {
    loop_process_playback_uint32(output_data, buffer, length);
}

/**
 * @brief AudioLooper接口：获取状态
 */
static LoopStatus_t AudioLooper_GetStatus(void) {
    LoopStatus_t status;

    status.current_state = g_loop_manager.state;
    status.active_segments = g_loop_manager.active_segments;
    status.current_segment = g_loop_manager.current_segment;
    status.total_recorded_bytes = g_loop_manager.record_length;
    status.total_play_time_ms = 0; // 可根据需要计算
    status.flash_type = g_loop_manager.flash_type;
    status.is_recording = (g_loop_manager.state == LOOP_STATE_RECORDING) ? 1 : 0;
    status.is_playing = (g_loop_manager.state == LOOP_STATE_PLAYING) ? 1 : 0;

    return status;
}

/**
 * @brief AudioLooper接口：检查是否正在录制
 */
static uint8_t AudioLooper_IsRecording(void) {
    return loop_is_recording();
}

/**
 * @brief AudioLooper接口：检查是否正在播放
 */
static uint8_t AudioLooper_IsPlaying(void) {
    return loop_is_playing();
}

/**
 * @brief AudioLooper接口：获取当前地址
 */
static uint32_t AudioLooper_GetCurrentAddress(void) {
    return loop_get_current_address();
}

/**
 * @brief AudioLooper接口：获取录制长度
 */
static uint32_t AudioLooper_GetRecordLength(void) {
    return loop_get_record_length();
}

/**
 * @brief AudioLooper接口：定时器更新
 */
static void AudioLooper_TimerUpdate(void) {
    loop_timer_update();
}

// ============================================================================
// 节拍器模块实现
// ============================================================================

/**
 * @brief 初始化节拍器（使用默认设置）
 */
void metronome_init(void) {
    // 初始化节拍器配置为默认值
    g_loop_manager.metronome.state = METRONOME_OFF;
    g_loop_manager.metronome.config.bpm = METRONOME_DEFAULT_BPM;
    g_loop_manager.metronome.config.beats_per_measure = METRONOME_DEFAULT_BEATS_PER_MEASURE;
    g_loop_manager.metronome.config.downbeat_freq = METRONOME_DEFAULT_DOWNBEAT_FREQ;
    g_loop_manager.metronome.config.regular_beat_freq = METRONOME_DEFAULT_REGULAR_BEAT_FREQ;
    g_loop_manager.metronome.config.beat_duration_ms = METRONOME_DEFAULT_BEAT_DURATION;
    g_loop_manager.metronome.config.volume = METRONOME_DEFAULT_VOLUME;
    
    // 初始化运行时状态
    g_loop_manager.metronome.sample_counter = 0;
    g_loop_manager.metronome.beat_sample_counter = 0;
    g_loop_manager.metronome.current_beat = 0;
    g_loop_manager.metronome.is_beat_active = 0;
    g_loop_manager.metronome.current_beat_type = BEAT_TYPE_DOWNBEAT;
    g_loop_manager.metronome.sine_phase = 0.0f;
    
    // 更新计时参数
    metronome_update_timing_params();
    
    DBG("Metronome initialized: BPM=%d, beats_per_measure=%d\n", 
        g_loop_manager.metronome.config.bpm, 
        g_loop_manager.metronome.config.beats_per_measure);
}

/**
 * @brief 重置节拍器状态
 */
void metronome_reset(void) {
    g_loop_manager.metronome.sample_counter = 0;
    g_loop_manager.metronome.beat_sample_counter = 0;
    g_loop_manager.metronome.current_beat = 0;
    g_loop_manager.metronome.is_beat_active = 0;
    g_loop_manager.metronome.current_beat_type = BEAT_TYPE_DOWNBEAT;
    g_loop_manager.metronome.sine_phase = 0.0f;
}

/**
 * @brief 配置节拍器参数
 * @param config 节拍器配置结构体指针
 */
void metronome_configure(const MetronomeConfig_t* config) {
    if (config == NULL) return;
    
    // 验证并设置BPM
    if (config->bpm >= METRONOME_MIN_BPM && config->bpm <= METRONOME_MAX_BPM) {
        g_loop_manager.metronome.config.bpm = config->bpm;
    }
    
    // 验证并设置每小节拍数
    if (config->beats_per_measure >= METRONOME_MIN_BEATS_PER_MEASURE &&
        config->beats_per_measure <= METRONOME_MAX_BEATS_PER_MEASURE) {
        g_loop_manager.metronome.config.beats_per_measure = config->beats_per_measure;
    }
    
    // 设置频率参数
    g_loop_manager.metronome.config.downbeat_freq = config->downbeat_freq;
    g_loop_manager.metronome.config.regular_beat_freq = config->regular_beat_freq;
    g_loop_manager.metronome.config.beat_duration_ms = config->beat_duration_ms;
    
    // 验证并设置音量
    if (config->volume >= 0.0f && config->volume <= 1.0f) {
        g_loop_manager.metronome.config.volume = config->volume;
    }
    
    // 更新计时参数
    metronome_update_timing_params();
    
    DBG("Metronome configured: BPM=%d, beats=%d, vol=%.2f\n", 
        g_loop_manager.metronome.config.bpm,
        g_loop_manager.metronome.config.beats_per_measure,
        g_loop_manager.metronome.config.volume);
}

/**
 * @brief 切换节拍器开关状态
 */
void metronome_toggle(void) {
    if (g_loop_manager.metronome.state == METRONOME_OFF) {
        metronome_enable();
    } else {
        metronome_disable();
    }
}

/**
 * @brief 启用节拍器
 */
void metronome_enable(void) {
    g_loop_manager.metronome.state = METRONOME_ON;
    metronome_reset();  // 重置状态，从第一拍开始
    DBG("Metronome enabled\n");
}

/**
 * @brief 禁用节拍器
 */
void metronome_disable(void) {
    g_loop_manager.metronome.state = METRONOME_OFF;
    DBG("Metronome disabled\n");
}

/**
 * @brief 检查节拍器是否启用
 * @return 1如果启用，0如果禁用
 */
uint8_t metronome_is_enabled(void) {
    return (g_loop_manager.metronome.state == METRONOME_ON);
}

/**
 * @brief 设置BPM
 * @param bpm 节拍速度（60-200）
 */
void metronome_set_bpm(uint16_t bpm) {
    if (bpm >= METRONOME_MIN_BPM && bpm <= METRONOME_MAX_BPM) {
        g_loop_manager.metronome.config.bpm = bpm;
        metronome_update_timing_params();
        DBG("Metronome BPM set to %d\n", bpm);
    }
}

/**
 * @brief 设置每小节拍数
 * @param beats 每小节拍数（2-8）
 */
void metronome_set_beats_per_measure(uint8_t beats) {
    if (beats >= METRONOME_MIN_BEATS_PER_MEASURE && beats <= METRONOME_MAX_BEATS_PER_MEASURE) {
        g_loop_manager.metronome.config.beats_per_measure = beats;
        // 重置当前拍子以避免超出范围
        if (g_loop_manager.metronome.current_beat >= beats) {
            g_loop_manager.metronome.current_beat = 0;
        }
        DBG("Metronome beats per measure set to %d\n", beats);
    }
}

/**
 * @brief 设置音量
 * @param volume 音量系数（0.0-1.0）
 */
void metronome_set_volume(float volume) {
    if (volume >= 0.0f && volume <= 1.0f) {
        g_loop_manager.metronome.config.volume = volume;
        DBG("Metronome volume set to %.2f\n", volume);
    }
}

/**
 * @brief 设置下拍频率
 * @param freq 下拍频率（Hz）
 */
void metronome_set_downbeat_freq(uint16_t freq) {
    g_loop_manager.metronome.config.downbeat_freq = freq;
    DBG("Metronome downbeat frequency set to %d Hz\n", freq);
}

/**
 * @brief 设置普通拍频率
 * @param freq 普通拍频率（Hz）
 */
void metronome_set_regular_beat_freq(uint16_t freq) {
    g_loop_manager.metronome.config.regular_beat_freq = freq;
    DBG("Metronome regular beat frequency set to %d Hz\n", freq);
}

/**
 * @brief 设置节拍持续时间
 * @param duration_ms 节拍持续时间（毫秒）
 */
void metronome_set_beat_duration(uint16_t duration_ms) {
    g_loop_manager.metronome.config.beat_duration_ms = duration_ms;
    metronome_update_timing_params();
    DBG("Metronome beat duration set to %d ms\n", duration_ms);
}

/**
 * @brief 获取当前BPM
 * @return 当前BPM值
 */
uint16_t metronome_get_bpm(void) {
    return g_loop_manager.metronome.config.bpm;
}

/**
 * @brief 获取每小节拍数
 * @return 每小节拍数
 */
uint8_t metronome_get_beats_per_measure(void) {
    return g_loop_manager.metronome.config.beats_per_measure;
}

/**
 * @brief 获取当前音量
 * @return 当前音量系数
 */
float metronome_get_volume(void) {
    return g_loop_manager.metronome.config.volume;
}

/**
 * @brief 获取下拍频率
 * @return 下拍频率（Hz）
 */
uint16_t metronome_get_downbeat_freq(void) {
    return g_loop_manager.metronome.config.downbeat_freq;
}

/**
 * @brief 获取普通拍频率
 * @return 普通拍频率（Hz）
 */
uint16_t metronome_get_regular_beat_freq(void) {
    return g_loop_manager.metronome.config.regular_beat_freq;
}

/**
 * @brief 获取节拍持续时间
 * @return 节拍持续时间（毫秒）
 */
uint16_t metronome_get_beat_duration(void) {
    return g_loop_manager.metronome.config.beat_duration_ms;
}

/**
 * @brief 获取当前拍子索引
 * @return 当前拍子索引（0开始）
 */
uint8_t metronome_get_current_beat(void) {
    return g_loop_manager.metronome.current_beat;
}

/**
 * @brief 获取当前拍子类型
 * @return 当前拍子类型
 */
BeatType_t metronome_get_current_beat_type(void) {
    return g_loop_manager.metronome.current_beat_type;
}

/**
 * @brief 检查当前是否在播放节拍声音
 * @return 1如果正在播放节拍声音，0如果不是
 */
uint8_t metronome_is_beat_active(void) {
    return g_loop_manager.metronome.is_beat_active;
}

/**
 * @brief 处理节拍器音频输出（主要功能）
 * @param output_data 输出音频数据缓冲区（uint32_t格式，包含左右声道）
 * @param length 音频数据长度（样本数，不是字节数）
 */
void metronome_process_audio(uint32_t* output_data, uint16_t length) {
    if (!metronome_is_enabled() || output_data == NULL || length == 0) {
        return;
    }
    
    uint16_t i;
    for (i = 0; i < length; i++) {
        // 检查是否需要开始新的节拍
        if (g_loop_manager.metronome.sample_counter >= g_loop_manager.metronome.beat_interval_samples) {
            // 开始新的节拍
            g_loop_manager.metronome.is_beat_active = 1;
            g_loop_manager.metronome.beat_sample_counter = 0;
            g_loop_manager.metronome.sine_phase = 0.0f;
            
            // 确定拍子类型
            if (g_loop_manager.metronome.current_beat == 0) {
                g_loop_manager.metronome.current_beat_type = BEAT_TYPE_DOWNBEAT;
            } else {
                g_loop_manager.metronome.current_beat_type = BEAT_TYPE_REGULAR;
            }
            
            // 重置采样计数器
            g_loop_manager.metronome.sample_counter = 0;
            
            // 推进到下一拍
            metronome_advance_beat();
        }
        
        // 生成节拍声音样本
        int16_t sample = 0;
        if (g_loop_manager.metronome.is_beat_active) {
            // 选择频率
            float freq = (g_loop_manager.metronome.current_beat_type == BEAT_TYPE_DOWNBEAT) ? 
                         g_loop_manager.metronome.config.downbeat_freq : 
                         g_loop_manager.metronome.config.regular_beat_freq;
            
            // 生成正弦波样本
            float sine_sample = metronome_generate_sine_sample(freq, &g_loop_manager.metronome.sine_phase);
            
            // 应用音量和转换为16位整数
            sample = (int16_t)(sine_sample * g_loop_manager.metronome.config.volume * 32767.0f);
            
            // 更新节拍样本计数器
            g_loop_manager.metronome.beat_sample_counter++;
            
            // 检查节拍是否结束
            if (g_loop_manager.metronome.beat_sample_counter >= g_loop_manager.metronome.beat_duration_samples) {
                g_loop_manager.metronome.is_beat_active = 0;
            }
        }
        
        // 将样本混合到输出（假设uint32_t包含左右声道）
        // 提取当前左右声道
        int16_t left = (int16_t)(output_data[i] & 0xFFFF);
        int16_t right = (int16_t)((output_data[i] >> 16) & 0xFFFF);
        
        // 添加节拍器信号到两个声道
        left = (int16_t)(((int32_t)left + (int32_t)sample) / 2);  // 简单混合
        right = (int16_t)(((int32_t)right + (int32_t)sample) / 2);
        
        // 防止溢出
        if (left > 32767) left = 32767;
        if (left < -32768) left = -32768;
        if (right > 32767) right = 32767;
        if (right < -32768) right = -32768;
        
        // 重新打包
        output_data[i] = ((uint32_t)(uint16_t)right << 16) | (uint16_t)left;
        
        // 推进总采样计数器
        g_loop_manager.metronome.sample_counter++;
    }
}

/**
 * @brief 将节拍器音频混合到输出（替代接口）
 * @param output_data 输出音频数据缓冲区
 * @param length 音频数据长度
 */
void metronome_mix_audio(uint32_t* output_data, uint16_t length) {
    metronome_process_audio(output_data, length);
}

/**
 * @brief 更新计时参数（内部使用）
 */
static void metronome_update_timing_params(void) {
    // 计算节拍间隔（样本数）
    // BPM = 每分钟拍数，所以每拍间隔 = 60秒 / BPM
    // 样本数 = 间隔秒数 * 采样率
    uint32_t beat_interval_ms = (60000 / g_loop_manager.metronome.config.bpm);  // 毫秒
    g_loop_manager.metronome.beat_interval_samples = (beat_interval_ms * METRONOME_SAMPLE_RATE) / 1000;
    
    // 计算节拍持续时间（样本数）
    g_loop_manager.metronome.beat_duration_samples =
        (g_loop_manager.metronome.config.beat_duration_ms * METRONOME_SAMPLE_RATE) / 1000;
}

/**
 * @brief 生成正弦波样本（内部使用）
 * @param freq 频率（Hz）
 * @param phase 相位累积器指针
 * @return 正弦波样本值（-1.0到1.0）
 */
static float metronome_generate_sine_sample(float freq, float* phase) {
    float sample = sinf(*phase);
    *phase += 2.0f * M_PI * freq / METRONOME_SAMPLE_RATE;
    
    // 防止相位累积器溢出
    if (*phase >= 2.0f * M_PI) {
        *phase -= 2.0f * M_PI;
    }
    
    return sample;
}

/**
 * @brief 推进到下一拍（内部使用）
 */
static void metronome_advance_beat(void) {
    g_loop_manager.metronome.current_beat++;
    if (g_loop_manager.metronome.current_beat >= g_loop_manager.metronome.config.beats_per_measure) {
        g_loop_manager.metronome.current_beat = 0;
    }
}

// ============================================================================
// AudioLooper接口函数（模式控制）
// ============================================================================

/**
 * @brief AudioLooper接口：设置循环模式
 */
static void AudioLooper_SetMode(LoopMode_t mode) {
    loop_set_mode(mode);
}

/**
 * @brief AudioLooper接口：获取当前循环模式
 */
static LoopMode_t AudioLooper_GetMode(void) {
    return loop_get_mode();
}

/**
 * @brief AudioLooper接口：检查是否为歌曲模式
 */
static uint8_t AudioLooper_IsSongMode(void) {
    return loop_is_song_mode();
}

/**
 * @brief AudioLooper接口：检查是否为自由模式
 */
static uint8_t AudioLooper_IsFreeMode(void) {
    return loop_is_free_mode();
}

/**
 * @brief AudioLooper接口：切换节拍器开关
 */
static void AudioLooper_MetronomeToggle(void) {
    metronome_toggle();
}

/**
 * @brief AudioLooper接口：设置BPM
 */
static void AudioLooper_MetronomeSetBPM(uint16_t bpm) {
    metronome_set_bpm(bpm);
}

/**
 * @brief AudioLooper接口：设置每小节拍数
 */
static void AudioLooper_MetronomeSetBeatsPerMeasure(uint8_t beats) {
    metronome_set_beats_per_measure(beats);
}

/**
 * @brief AudioLooper接口：设置节拍器音量
 */
static void AudioLooper_MetronomeSetVolume(float volume) {
    metronome_set_volume(volume);
}

/**
 * @brief AudioLooper接口：检查节拍器是否开启
 */
static uint8_t AudioLooper_MetronomeIsEnabled(void) {
    return metronome_is_enabled();
}

/**
 * @brief AudioLooper接口：获取当前BPM
 */
static uint16_t AudioLooper_MetronomeGetBPM(void) {
    return metronome_get_bpm();
}

/**
 * @brief AudioLooper接口：获取每小节拍数
 */
static uint8_t AudioLooper_MetronomeGetBeatsPerMeasure(void) {
    return metronome_get_beats_per_measure();
}

// ============================================================================
// 全局AudioLooper接口实例（类似BG_flash_manager）
// ============================================================================
AudioLooper_t AudioLooper = {
    .Init = AudioLooper_Init,
    .InitWithFlashType = AudioLooper_InitWithFlashType,
    .Reset = AudioLooper_Reset,
    .SetFlashType = AudioLooper_SetFlashType,
    .ButtonPress = AudioLooper_ButtonPress,
    .SegmentButtonPress = AudioLooper_SegmentButtonPress,
    .EncoderLeft = AudioLooper_EncoderLeft,
    .EncoderRight = AudioLooper_EncoderRight,
    .StopRecording = AudioLooper_StopRecording,
    .ProcessRecording = AudioLooper_ProcessRecording,
    .ProcessPlayback = AudioLooper_ProcessPlayback,
    .ProcessRecording32 = AudioLooper_ProcessRecording32,
    .ProcessPlayback32 = AudioLooper_ProcessPlayback32,
    .GetStatus = AudioLooper_GetStatus,
    .IsRecording = AudioLooper_IsRecording,
    .IsPlaying = AudioLooper_IsPlaying,
    .GetCurrentAddress = AudioLooper_GetCurrentAddress,
    .GetRecordLength = AudioLooper_GetRecordLength,
    .TimerUpdate = AudioLooper_TimerUpdate,
    
    // 模式控制
    .SetMode = AudioLooper_SetMode,
    .GetMode = AudioLooper_GetMode,
    .IsSongMode = AudioLooper_IsSongMode,
    .IsFreeMode = AudioLooper_IsFreeMode,
    
    // 节拍器控制
    .MetronomeToggle = AudioLooper_MetronomeToggle,
    .MetronomeSetBPM = AudioLooper_MetronomeSetBPM,
    .MetronomeSetBeatsPerMeasure = AudioLooper_MetronomeSetBeatsPerMeasure,
    .MetronomeSetVolume = AudioLooper_MetronomeSetVolume,
    .MetronomeIsEnabled = AudioLooper_MetronomeIsEnabled,
    .MetronomeGetBPM = AudioLooper_MetronomeGetBPM,
    .MetronomeGetBeatsPerMeasure = AudioLooper_MetronomeGetBeatsPerMeasure
};
