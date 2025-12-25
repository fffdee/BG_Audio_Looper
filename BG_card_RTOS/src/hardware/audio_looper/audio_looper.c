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
#include "bg_flash_manager.h"
#include "type.h"
#include <nds32_intrinsic.h>
#include <math.h>
#include <string.h>


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

// 前向声明（静态辅助函数）
static void metronome_advance_beat(void);
static void metronome_update_timing_params(void);
static float metronome_generate_sine_sample(float freq, float* phase);

// 前向声明（非静态辅助函数）
uint8_t loop_get_flash_device_id(void);

static void convertUint8ArrayToInt16Array(const uint8_t *input, int16_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; ++i) {
            // 假设系统是小端
            int16_t sample = (int16_t)(input[i * 2]) | ((int16_t)(input[i * 2 + 1]) << 8);
            output[i] = sample;
        }
}

static void convertUint32ArrayToUint8Array(const uint32_t *input, uint8_t *output, size_t size) {
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

static void convertUint8ArrayToUint32Array(const uint8_t *input, uint32_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; i++) {
        // 将4个uint8_t重新组合为1个uint32_t，恢复双声道数据
        output[i] = (uint32_t)input[i * 4] |
                    ((uint32_t)input[i * 4 + 1] << 8) |
                    ((uint32_t)input[i * 4 + 2] << 16) |
                    ((uint32_t)input[i * 4 + 3] << 24);
    }
}

static void convertInt16ArrayToUint8Array(const int16_t *input, uint8_t *output, size_t size) {
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
        g_loop_manager.segments[i].is_active = 0;
    }
    
    // 初始化统计信息
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count = 0;
    g_loop_stats.last_recorded_sample = 0;
    g_loop_stats.first_playback_sample = 0;
    
    // 初始化节拍器
    metronome_init();
    
    // 开机全片擦除Flash
    uint8_t flash_device_id = loop_get_flash_device_id();
    DBG("Erasing entire Flash on startup (device: %s)...\n", 
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
    BG_flash_manager.EraseAll(flash_device_id);
    DBG("Flash erased, Loop manager initialized with multi-segment support\n");
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
    
    // 初始化多段录音参数
    g_loop_manager.current_segment = 0;
    g_loop_manager.active_segments = 0;
    g_loop_manager.page_size = 256;  // Flash页大小

    // 初始化所有段信息
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address = 0;
        g_loop_manager.segments[i].length_pages = 0;
        g_loop_manager.segments[i].is_active = 0;
    }
    
    // 初始化统计信息
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count = 0;
    g_loop_stats.last_recorded_sample = 0;
    g_loop_stats.first_playback_sample = 0;
    
    // 初始化节拍器
    metronome_init();
    
    // 开机全片擦除Flash（使用指定的Flash类型）
    uint8_t flash_device_id = loop_get_flash_device_id();
    DBG("Erasing entire Flash on startup (device: %s)...\n", 
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
    BG_flash_manager.EraseAll(flash_device_id);
    DBG("Flash erased, Loop manager initialized with %s Flash support\n",
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
}

/**
 * @brief 重置Loop管理器
 */
void loop_reset(void)
{
    g_loop_manager.state = LOOP_STATE_IDLE;
    g_loop_manager.sector_address = 0;
    g_loop_manager.record_length = 0;
    g_loop_manager.play_position = 0;
    g_loop_manager.is_new_recording = 0;
    
    // 重置统计信息
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count = 0;
    g_loop_stats.last_recorded_sample = 0;
    g_loop_stats.first_playback_sample = 0;
    
    DBG("Loop manager reset\n");
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

    
    // 擦除全片Flash
    uint8_t flash_device_id = loop_get_flash_device_id();
    DBG("Encoder right: Erasing all flash memory (device: %s)\n",
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
    BG_flash_manager.EraseAll(flash_device_id);
    
    // 重置所有变量
    g_loop_manager.record_length = 0;
    g_loop_manager.play_position = 0;
    g_loop_manager.sector_address = 0;
    g_loop_manager.is_new_recording = 1;
    
    DBG("Flash erased, system reset to idle\n");
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
    
    // 如果系统已初始化，重新擦除Flash以确保使用正确的Flash设备
    if (g_loop_manager.is_initialized) {
        uint8_t flash_device_id = loop_get_flash_device_id();
        DBG("Re-erasing Flash after type change (device: %s, device_id=%d)...\n", 
            flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND", flash_device_id);
        BG_flash_manager.EraseAll(flash_device_id);
        
        // 重置状态
        loop_reset();
        g_loop_manager.flash_type = flash_type; // 保持Flash类型设置
        DBG("Flash type change completed\n");
    }
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
 * @return DEV_NOR 或 DEV_NAND
 */
uint8_t loop_get_flash_device_id(void)
{
    uint8_t device_id = (g_loop_manager.flash_type == FLASH_TYPE_NOR) ? DEV_NOR : DEV_NAND;
    // 临时调试信息
    //DBG("DEBUG: loop_get_flash_device_id() - flash_type=%d, returning device_id=%d\n",
    //    g_loop_manager.flash_type, device_id);
    return device_id;
}

/**
 * @brief 停止录制并准备播放
 */
void loop_stop_recording(void)
{
    if (g_loop_manager.state == LOOP_STATE_RECORDING) {
        // 如果使用NAND Flash，刷新音频缓冲区
        uint8_t flash_device_id = loop_get_flash_device_id();
        if (flash_device_id == DEV_NAND) {
            // NAND音频缓冲区刷新 - 暂时注释，因为函数未定义
            // nand_audio_flush_buffer(flash_device_id);
        }
        
        // Record the recording length and reset the playback position
        g_loop_manager.record_length = g_loop_manager.sector_address;
        g_loop_manager.play_position = 0;
        g_loop_manager.state = LOOP_STATE_PLAYING;

        
        DBG("Recording stopped manually: total_samples=%lu, record_length=%lu bytes, last_sample=%d\n",
            (unsigned long)g_loop_stats.recording_sample_count, (unsigned long)g_loop_manager.record_length, g_loop_stats.last_recorded_sample);
        
        // Reset address pointer for next recording
        g_loop_manager.sector_address = 0;
        g_loop_stats.recording_sample_count = 0;
    }
}

/**
 * @brief Handle recording logic
 * @param audio_data Audio data
 * @param buffer Buffer
 * @param length Data length
 */
void loop_process_recording(int16_t* audio_data, uint8_t* buffer, uint16_t length)
{
    if (g_loop_manager.state != LOOP_STATE_RECORDING) {
        return;  // Not in recording state
    }
    
    // Remove record_flag dependency, process audio data directly
    // Recording should be based on audio data availability, not timer
    
    // Only use Flash recording mode
    {
        // Flash recording logic - ensure length parameter is correct
        
        // Data validation: check if input audio data is valid
        uint16_t non_zero_count = 0;
        int32_t amplitude_sum = 0;
        int16_t max_amplitude = 0;
        uint16_t i;
        for (i = 0; i < length; i++) {
            int16_t sample = audio_data[i];
            if (sample != 0) {
                non_zero_count++;
                amplitude_sum += (sample < 0) ? -sample : sample;  // Manually implement abs
                if ((sample < 0 ? -sample : sample) > (max_amplitude < 0 ? -max_amplitude : max_amplitude)) {
                    max_amplitude = sample;
                }
            }
        }
        
        // Record statistics
        g_loop_stats.recording_sample_count += length;
        if (length > 0) {
            g_loop_stats.last_recorded_sample = audio_data[length - 1];
        }
        
        // If input signal is too weak, prompt to adjust gain
        if (g_loop_stats.recording_sample_count % 200 == 0 && non_zero_count > 0) {
            int32_t avg_amplitude = amplitude_sum / non_zero_count;
            if (avg_amplitude < 100) {  // Signal is weak
                DBG("WARNING: Input signal weak, avg_amp=%ld, max=%d, consider increasing gain\n",
                    (long)avg_amplitude, max_amplitude);
            }
        }
        
        convertInt16ArrayToUint8Array(audio_data, buffer, length);
        
        // Flash page size is usually 256 bytes, we write length*2 bytes of data
        uint32_t bytes_to_write = length * 2;  // 16-bit audio to 8-bit needs *2
        uint8_t flash_device_id = loop_get_flash_device_id();
        
        uint8_t write_result;
        
        // Use optimized audio write function
        if (flash_device_id == DEV_NAND) {
            // NAND Flash uses page-aligned buffer write - temporarily use standard write
            write_result = BG_flash_manager.PageProgram(g_loop_manager.sector_address, buffer, bytes_to_write, flash_device_id);
        } else {
            // NOR Flash direct write
            write_result = BG_flash_manager.PageProgram(g_loop_manager.sector_address, buffer, bytes_to_write, flash_device_id);
        }
        
        // Check if write is successful
        if (write_result != FLASH_STATUS_OK) {
            // Write failed, stop recording
            // DBG("Audio write failed, stopping recording\n");
            g_loop_manager.state = LOOP_STATE_PLAYING;

            return;
        }
        
        g_loop_stats.recording_sample_count++;
        g_loop_manager.sector_address += bytes_to_write;  // Increment by actual written bytes
        
//        if (rec % 500 == 0) {  // Reduce debug output frequency to avoid affecting real-time performance
//            //DBG("Flash recording: packets=%d, addr=%d, bytes=%d, nonzero=%d, avg_amp=%d, last_sample=%d\n",
//                rec, g_loop_manager.sector_address, bytes_to_write, non_zero_count,
//                non_zero_count > 0 ? amplitude_sum / non_zero_count : 0, last_recorded_sample);
//        }
        
        // Check Flash storage space
        if (g_loop_manager.sector_address >= BG_flash_manager.GetTotalByte(flash_device_id)) {
            DBG("Flash full, stop recording. Address: %lu, Total: %lu (%s)\n", 
                (unsigned long)g_loop_manager.sector_address, (unsigned long)BG_flash_manager.GetTotalByte(flash_device_id),
                g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
            
            g_loop_manager.record_length = g_loop_manager.sector_address;  // Correctly record recording length
            g_loop_manager.play_position = 0;  // Reset playback position
            g_loop_manager.state = LOOP_STATE_PLAYING;

            DBG("Recording finished: total_samples=%lu, record_length=%lu, last_sample=%d\n", 
                (unsigned long)g_loop_stats.recording_sample_count, (unsigned long)g_loop_manager.record_length, g_loop_stats.last_recorded_sample);
            
            g_loop_stats.recording_sample_count = 0;
            g_loop_stats.playback_sample_count = 0;
        }
        
        // Remove external variable dependency - no need to sync sectorAddress anymore
    }
}

/**
 * @brief Handle playback logic
 * @param output_data Output audio data
 * @param buffer Buffer
 * @param length Data length
 */
void loop_process_playback(int16_t* output_data, uint8_t* buffer, uint16_t length)
{
    if (g_loop_manager.state != LOOP_STATE_PLAYING) {
        return;  // Not in playback state, keep original audio data unchanged
    }
    
    uint16_t i;
    
    // Only use Flash playback mode
    {
        // Flash playback logic
        if (g_loop_manager.record_length == 0) {
            DBG("No recorded data in flash, record_length=0\n");
            return;  // No recorded data, keep original audio
        }
        
        // Ensure playback position is valid
        if (g_loop_manager.play_position >= g_loop_manager.record_length) {
            g_loop_manager.play_position = 0;
        }
        
        // Calculate bytes to read
        uint32_t bytes_to_read = length * 2;  // 16-bit audio needs to read length*2 bytes
        
        // Ensure not to exceed recording length
        if (g_loop_manager.play_position + bytes_to_read > g_loop_manager.record_length) {
            bytes_to_read = g_loop_manager.record_length - g_loop_manager.play_position;
            if (bytes_to_read == 0 || bytes_to_read % 2 != 0) {
                // Reached the end or odd bytes, restart
                g_loop_manager.play_position = 0;
                bytes_to_read = (length * 2 > g_loop_manager.record_length) ?
                               g_loop_manager.record_length : length * 2;
                if (bytes_to_read % 2 != 0) bytes_to_read--;  // Ensure even bytes
                g_loop_stats.playback_sample_count++;
                DBG("Flash loop restart, count: %lu, length: %lu, reading: %lu\n",
                    (unsigned long)g_loop_stats.playback_sample_count, (unsigned long)g_loop_manager.record_length, (unsigned long)bytes_to_read);
            }
        }
        
        // Ensure valid read length
        if (bytes_to_read == 0) {
            DBG("Warning: bytes_to_read=0, skipping playback\n");
            return;
        }
        
        // Read Flash data
        uint8_t flash_device_id = loop_get_flash_device_id();
        BG_flash_manager.ReadData(g_loop_manager.play_position, buffer, bytes_to_read, flash_device_id);
        convertUint8ArrayToInt16Array(buffer, ReadBuf, bytes_to_read/2);
        
        // Data validation: check the read audio data
        uint16_t valid_samples = bytes_to_read / 2;
        uint16_t non_zero_read = 0;
        int32_t read_amplitude_sum = 0;
        uint16_t j;
        for (j = 0; j < valid_samples; j++) {
            if (ReadBuf[j] != 0) {
                non_zero_read++;
                read_amplitude_sum += (ReadBuf[j] < 0) ? -ReadBuf[j] : ReadBuf[j];  // Manually implement abs
            }
        }
        
        // Record the first played sample for validation
        if (g_loop_manager.play_position == 0 && valid_samples > 0) {
            g_loop_stats.first_playback_sample = ReadBuf[0];
            DBG("First playback sample: %d (should match last recorded: %d)\n",
                g_loop_stats.first_playback_sample, g_loop_stats.last_recorded_sample);
        }
        
        // Mix audio data
        uint16_t samples_to_mix = (valid_samples < length) ? valid_samples : length;
        for (i = 0; i < samples_to_mix; i++) {
            int32_t mixed = (int32_t)output_data[i] + (int32_t)ReadBuf[i];
            output_data[i] = __nds32__clips(mixed, 15);  // 16-bit saturation limit
        }
        
        g_loop_stats.playback_sample_count += samples_to_mix;
        g_loop_manager.play_position += bytes_to_read;
        
        // Remove external variable dependency
        // sectorAddress = g_loop_manager.play_position;
    }
}

/**
 * @brief Timer update function, called in 1ms interrupt
 * Handles all states that need real-time update
 */
void loop_timer_update(void)
{
    if (!g_loop_manager.is_initialized) {
        return;
    }
    
    // Add logic that needs periodic update here
    // For example: LED indication, status monitoring, etc.
    
    // Remove external variable sync
    // sectorAddress = (g_loop_manager.state == LOOP_STATE_PLAYING) ?
    //                g_loop_manager.play_position : g_loop_manager.sector_address;
}

/**
 * @brief Get current loop state
 */
LoopState_t loop_get_state(void)
{
    return g_loop_manager.state;
}

/**
 * @brief Check if recording
 */
uint8_t loop_is_recording(void)
{
    return (g_loop_manager.state == LOOP_STATE_RECORDING || 
            g_loop_manager.state == LOOP_STATE_RECORDING_AND_PLAYING) ? 1 : 0;
}

/**
 * @brief Check if playing
 */
uint8_t loop_is_playing(void)
{
    return (g_loop_manager.state == LOOP_STATE_PLAYING || 
            g_loop_manager.state == LOOP_STATE_RECORDING_AND_PLAYING) ? 1 : 0;
}

/**
 * @brief Get current address
 */
uint32_t loop_get_current_address(void)
{
    return (g_loop_manager.state == LOOP_STATE_PLAYING) ? 
           g_loop_manager.play_position : g_loop_manager.sector_address;
}

/**
 * @brief Get recording length
 */
uint32_t loop_get_record_length(void)
{
    return g_loop_manager.record_length;
}

// ============================================================================
// Loop mode control function implementation
// ============================================================================

/**
 * @brief Set loop mode
 * @param mode Loop mode to set
 */
void loop_set_mode(LoopMode_t mode)
{
    if (mode == LOOP_MODE_SONG || mode == LOOP_MODE_FREE) {
        g_loop_manager.mode = mode;
        DBG("Loop mode set to %s\n", mode == LOOP_MODE_SONG ? "SONG" : "FREE");
        
        // If switching to song mode, need to recalculate master segment info
        if (mode == LOOP_MODE_SONG) {
            loop_update_master_segment_info();
        }
    }
}

/**
 * @brief Get current loop mode
 * @return Current loop mode
 */
LoopMode_t loop_get_mode(void)
{
    return g_loop_manager.mode;
}

/**
 * @brief Check if song mode
 * @return 1 if song mode, 0 otherwise
 */
uint8_t loop_is_song_mode(void)
{
    return (g_loop_manager.mode == LOOP_MODE_SONG) ? 1 : 0;
}

/**
 * @brief Check if free mode
 * @return 1 if free mode, 0 otherwise
 */
uint8_t loop_is_free_mode(void)
{
    return (g_loop_manager.mode == LOOP_MODE_FREE) ? 1 : 0;
}

/**
 * @brief Update master segment info (internal use)
 * In song mode, find the longest segment as the master segment for loop reference
 */
void loop_update_master_segment_info(void)
{
    uint32_t max_length = 0;
    uint8_t master_index = 0;
    uint8_t i;
    
    // Find the longest segment
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
 * @brief Segment recording handler - based on segment instance
 * @param segment_index Index of the segment to record
 * @param audio_data Audio data in uint32_t format
 * @param buffer Temporary buffer for data conversion
 * @param length Number of uint32_t data
 */
void loop_process_segment_recording(uint8_t segment_index, uint32_t* audio_data, uint8_t* buffer, uint16_t length)
{
    if (segment_index >= MAX_SEGMENTS) {
        return;  // Invalid segment index
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state != SEGMENT_RECORDING) {
        return;  // Segment not in recording state
    }
    
    uint32_t bytes_to_write = 192;  // Only store 192 bytes (48 uint32_t * 4 bytes)
    uint8_t write_buffer[192];
    
    // Directly record raw audio data
    convertUint32ArrayToUint8Array(audio_data, write_buffer, length);
    uint32_t write_address = segment->start_address + 
                            segment->length_pages * g_loop_manager.page_size;
    uint8_t flash_device_id = loop_get_flash_device_id();
    
    uint8_t result = BG_flash_manager.PageProgram(write_address, write_buffer, bytes_to_write, flash_device_id);
    
    if (result == FLASH_STATUS_OK) {
        segment->length_pages++;  // Update segment length
        // Only print at segment start and end to avoid frequent output
//        if (segment->length_pages == 1 || segment->length_pages % 100 == 0) {
//            DBG("Segment %d recording: page %lu written\n", segment_index, (unsigned long)segment->length_pages);
//        }
    } else {
        // Error message retained as it should not occur frequently
//        DBG("Segment %d recording error: %d at address 0x%lx\n",
//            segment_index, result, (unsigned long)write_address);
    }
}

/**
 * @brief Total recording handler - handle all segments being recorded
 * @param audio_data Audio data in uint32_t format
 * @param buffer Temporary buffer for data conversion
 * @param length Number of uint32_t data
 */
void loop_process_recording_uint32(uint32_t* audio_data, uint8_t* buffer, uint16_t length)
{
    // 处理所有正在录制的段
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
            loop_process_segment_recording(i, audio_data, buffer, length);
        }
    }
}

/**
 * @brief Segment playback handler - based on segment instance
 * @param segment_index Index of the segment to play
 * @param output_data Output audio data in uint32_t format (for mixing)
 * @param buffer Temporary buffer for data conversion
 * @param length Number of uint32_t data
 * @return 1=successfully played, 0=no data played
 */
uint8_t loop_process_segment_playback(uint8_t segment_index, uint32_t* output_data, uint8_t* buffer, uint16_t length)
{
    if (segment_index >= MAX_SEGMENTS) {
        return 0;  // Invalid segment index
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state != SEGMENT_PLAYING || !segment->is_active) {
        return 0;  // Segment not in playing state or not active
    }

    if (segment->length_pages == 0) {
        return 0;  // Segment has no data
    }
    
    // Check if playback position needs to loop
    if (segment->play_position >= segment->length_pages) {
        // Only print once when loop resets
        if (segment->play_position == segment->length_pages) {
            DBG("Segment %d loop: reset position from %lu to 0 (length=%lu)\n",
                segment_index, (unsigned long)segment->play_position, (unsigned long)segment->length_pages);
        }
        segment->play_position = 0;  // Reset to segment start
    }
    
    // Read segment data
    uint32_t segment_address = segment->start_address + segment->play_position * g_loop_manager.page_size;
    uint8_t flash_device_id = loop_get_flash_device_id();
    
    BG_flash_manager.ReadData(segment_address, buffer, 192, flash_device_id);
    uint32_t segment_data[48];
    uint32_t samples_to_read = (length < 48) ? length : 48;
    convertUint8ArrayToUint32Array(buffer, segment_data, samples_to_read);
    
    // Segment fade-in at the beginning
    if (segment->play_position == 0) {
        uint16_t fade_samples = (samples_to_read < 16) ? samples_to_read : 16;
        uint16_t j;
        for (j = 0; j < fade_samples; j++) {
            int16_t left = (int16_t)(segment_data[j] & 0xFFFF);
            int16_t right = (int16_t)((segment_data[j] >> 16) & 0xFFFF);
            
            uint16_t fade_factor = (j * 100) / fade_samples;  // 0-100% fade-in
            left = (int16_t)((int32_t)left * fade_factor / 100);
            right = (int16_t)((int32_t)right * fade_factor / 100);
            
            segment_data[j] = ((uint32_t)(uint16_t)right << 16) | ((uint32_t)(uint16_t)left & 0xFFFF);
        }
    }
    
    // Mix to output data
    uint16_t j;
    for (j = 0; j < samples_to_read; j++) {
        int16_t seg_left = (int16_t)(segment_data[j] & 0xFFFF);
        int16_t seg_right = (int16_t)((segment_data[j] >> 16) & 0xFFFF);
        int16_t out_left = (int16_t)(output_data[j] & 0xFFFF);
        int16_t out_right = (int16_t)((output_data[j] >> 16) & 0xFFFF);
        
        // Mixing: each segment contributes 60% volume
        int32_t new_left = (int32_t)out_left + ((int32_t)seg_left * 6 / 10);
        int32_t new_right = (int32_t)out_right + ((int32_t)seg_right * 6 / 10);
        
        // Soft limiting
        new_left = __nds32__clips(new_left, 15);
        new_right = __nds32__clips(new_right, 15);
        
        output_data[j] = ((uint32_t)(uint16_t)new_right << 16) | ((uint32_t)(uint16_t)new_left & 0xFFFF);
    }
    
    // Update segment playback position
    segment->play_position++;

    // Do not print playback status frequently to avoid stutter
    // DBG("Segment %d playback: position %lu/%lu\n",
    //     segment_index, (unsigned long)segment->play_position, (unsigned long)segment->length_pages);

    return 1;  // Successfully played
}

/**
 * @brief Total playback handler - mix all segments being played
 * @param output_data Output audio data in uint32_t format
 * @param buffer Temporary buffer for data conversion
 * @param length Number of uint32_t data
 */
void loop_process_playback_uint32(uint32_t* output_data, uint8_t* buffer, uint16_t length)
{
    // Clear output buffer
    uint16_t i;
    for (i = 0; i < length; i++) {
        output_data[i] = 0;
    }
    
    // Count number of playing segments
    uint8_t playing_count = 0;
    
    // Handle all segments being played
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (loop_process_segment_playback(i, output_data, buffer, length)) {
            playing_count++;
        }
    }
    
    if (playing_count > 0) {
        // Increase overall playback volume
        for (i = 0; i < length; i++) {
            int16_t left = (int16_t)(output_data[i] & 0xFFFF);
            int16_t right = (int16_t)((output_data[i] >> 16) & 0xFFFF);
            
            // Gain 1.5x
            int32_t boosted_left = (int32_t)left * 3 / 2;
            int32_t boosted_right = (int32_t)right * 3 / 2;
            
            // Soft limiting
            boosted_left = __nds32__clips(boosted_left, 15);
            boosted_right = __nds32__clips(boosted_right, 15);
            
            output_data[i] = ((uint32_t)(uint16_t)boosted_right << 16) | 
                            ((uint32_t)(uint16_t)boosted_left & 0xFFFF);
        }
        
        // Do not print mixing status frequently to avoid stutter
        // DBG("Mixed %d segments playback\n", playing_count);
    }
    
    // Process metronome audio (regardless of whether any segment is playing)
    metronome_process_audio(output_data, length);
}

/**
 * @brief Start recording a new segment
 */
void loop_start_new_segment(void)
{
    // Find the first inactive segment
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
    
    // Calculate the start address of the new segment
    uint32_t start_address;
    if (new_segment == 0) {
        // The first segment starts from a fixed address
        start_address = 0x40000;  // Avoid low addresses in Flash
    } else {
        // Subsequent segments are calculated based on the size of the previous segment
        start_address = g_loop_manager.segments[new_segment - 1].start_address + 
                       g_loop_manager.segments[new_segment - 1].length_pages * g_loop_manager.page_size;
        
        // Ensure the address is page-aligned
        if (start_address % g_loop_manager.page_size != 0) {
            start_address = ((start_address / g_loop_manager.page_size) + 1) * g_loop_manager.page_size;
        }
    }
    
    // Initialize new segment
    g_loop_manager.segments[new_segment].start_address = start_address;
    g_loop_manager.segments[new_segment].length_pages = 0;  // Start from 0, normal recording
    g_loop_manager.segments[new_segment].is_active = 1;
    g_loop_manager.segments[new_segment].state = SEGMENT_RECORDING;
    g_loop_manager.segments[new_segment].play_position = 0;
    
    // Update active segment count
    g_loop_manager.active_segments++;
    
    // Set global state to recording (for compatibility)
    g_loop_manager.state = LOOP_STATE_RECORDING;
    g_loop_manager.is_new_recording = 1;
    g_loop_stats.recording_sample_count = 0;
    
    DBG("Started segment %d at address 0x%08lX using %s Flash\n",
        new_segment, (unsigned long)start_address,
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
}

/**
 * @brief Stop recording the specified segment
 * @param segment_index Index of the segment to stop
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
    
    // Copy previous data at the end of recording to ensure smooth looping
    uint8_t copy_pages_to_add = 10;  // Copy the first 10 pages of data
    uint8_t copy_buffer[192];
    
    // Ensure there is data to copy
    if (segment->length_pages == 0) {
        DBG("Warning: No data to copy for segment %d\n", segment_index);
        return;
    }
    
    // Write copied pages to the end of the segment
    uint8_t flash_device_id = loop_get_flash_device_id();
    uint8_t page_count;
    for (page_count = 0; page_count < copy_pages_to_add; page_count++) {
        // Calculate the source page address to copy (loop using the beginning data of the segment)
        uint32_t source_page_index = page_count % segment->length_pages;
        uint32_t source_address = segment->start_address + source_page_index * g_loop_manager.page_size;
        
        // Read source page data
        BG_flash_manager.ReadData(source_address, copy_buffer, 192, flash_device_id);
        
        // Write to the end of the segment
        uint32_t dest_address = segment->start_address + 
                               (segment->length_pages + page_count) * g_loop_manager.page_size;
        uint8_t write_result = BG_flash_manager.PageProgram(dest_address, copy_buffer, 192, flash_device_id);
        if (write_result != FLASH_STATUS_OK) {
            DBG("Warning: Failed to copy page %d to end\n", page_count);
        }
    }
    
    // Update segment length (including copied pages)
    segment->length_pages += copy_pages_to_add;
    
    DBG("Stop segment %d: recorded %lu pages with end-copy (copied %d pages)\n", 
        segment_index, (unsigned long)(segment->length_pages - copy_pages_to_add), copy_pages_to_add);
    
    if (segment->length_pages == 0) {
        // If no data was recorded, mark segment as invalid
        segment->is_active = 0;
        segment->state = SEGMENT_INACTIVE;
        DBG("Segment %d has no data, marked as inactive\n", segment_index);
    } else {
        // Set segment to playing state
        segment->state = SEGMENT_PLAYING;
        segment->play_position = 0;  // Reset playback position
        
        DBG("Stopped segment %d: %lu pages, set to PLAYING state\n",
            segment_index, (unsigned long)segment->length_pages);
    }
    
    // Update global state
    loop_update_global_state();
}

/**
 * @brief Get the number of recorded segments
 */
uint8_t loop_get_segment_count(void)
{
    return g_loop_manager.active_segments;
}

/**
 * @brief Clear all segments
 */
void loop_clear_all_segments(void)
{
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address = 0;
        g_loop_manager.segments[i].length_pages = 0;
        g_loop_manager.segments[i].is_active = 0;
        g_loop_manager.segments[i].state = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].play_position = 0;
    }
    
    g_loop_manager.active_segments = 0;
    g_loop_manager.current_segment = 0;
    g_loop_manager.sector_address = 0;
    g_loop_manager.play_position = 0;
    
    // Erase Flash
    uint8_t flash_device_id = loop_get_flash_device_id();
    DBG("Clearing all segments and erasing Flash (%s)...\n",
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
    BG_flash_manager.EraseAll(flash_device_id);
    DBG("All segments cleared\n");
}

// ============================================================================
// Single segment fine control function implementation
// ============================================================================

/**
 * @brief Handle button operation for the specified segment
 * @param segment_index Segment index (0-3)
 */
void loop_handle_segment_button(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        DBG("Invalid segment index: %d\n", segment_index);
        return;
    }

    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    switch (segment->state) {
        case SEGMENT_INACTIVE:
            // Segment not active: start recording
            loop_set_segment_recording(segment_index);
            DBG("Segment %d: INACTIVE -> RECORDING\n", segment_index);
            break;
            
        case SEGMENT_RECORDING:
        {
            // Segment recording: stop recording and start playback
            loop_stop_current_segment(segment_index);
            DBG("Segment %d: RECORDING -> PLAYING (stopped recording)\n", segment_index);
            break;
        }
            
        case SEGMENT_PLAYING:
            // Segment playing: stop playback
            loop_set_segment_stopped(segment_index);
            DBG("Segment %d: PLAYING -> STOPPED\n", segment_index);
            break;
            
        case SEGMENT_STOPPED:
            // Segment stopped: start playback
            loop_set_segment_playing(segment_index);
            DBG("Segment %d: STOPPED -> PLAYING\n", segment_index);
            break;
    }
}

/**
 * @brief Get the state of the specified segment
 * @param segment_index Segment index (0-3)
 * @return Segment state
 */
SegmentState_t loop_get_segment_state(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return SEGMENT_INACTIVE;
    }
    return g_loop_manager.segments[segment_index].state;
}

/**
 * @brief Intelligently update global state based on the state of each segment
 */
void loop_update_global_state(void)
{
    uint8_t has_recording = 0;
    uint8_t has_playing = 0;
    uint8_t i;
    
    // Count the state of each segment
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
            has_recording = 1;
        }
        if (g_loop_manager.segments[i].state == SEGMENT_PLAYING) {
            has_playing = 1;
        }
    }
    
    // Set global state according to segment states
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
    
    // 如果段未激活，初始化段信息
    if (segment->state == SEGMENT_INACTIVE) {
        // 计算段起始地址 - 与loop_start_new_segment保持一致
        uint32_t start_address;
        if (segment_index == 0) {
            // 第一段从固定地址开始
            start_address = 0x40000;  // 避开Flash低地址
        } else {
            // 后续段根据前面段的大小来计算
            start_address = g_loop_manager.segments[segment_index - 1].start_address + 
                           g_loop_manager.segments[segment_index - 1].length_pages * g_loop_manager.page_size;
            
            // 确保地址是页对齐的
            if (start_address % g_loop_manager.page_size != 0) {
                start_address = ((start_address / g_loop_manager.page_size) + 1) * g_loop_manager.page_size;
            }
        }
        
        segment->start_address = start_address;
        segment->length_pages = 0;
        segment->play_position = 0;
        segment->is_active = 1;
        g_loop_manager.active_segments++;
        
        DBG("Initialized segment %d at address 0x%08lX\n", 
            segment_index, (unsigned long)segment->start_address);
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
// AudioLooper接口实现函数（基础功能）
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
 * @brief AudioLooper接口：节拍器开关
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
 * @brief AudioLooper接口：设置音量
 */
static void AudioLooper_MetronomeSetVolume(float volume) {
    metronome_set_volume(volume);
}

/**
 * @brief AudioLooper接口：设置声音开关
 */
static void AudioLooper_MetronomeSetSoundEnabled(uint8_t enabled) {
    metronome_set_sound_enabled(enabled);
}

/**
 * @brief AudioLooper接口：检查节拍器是否启用
 */
static uint8_t AudioLooper_MetronomeIsEnabled(void) {
    return metronome_is_enabled();
}

/**
 * @brief AudioLooper接口：检查节拍器声音是否启用
 */
static uint8_t AudioLooper_MetronomeIsSoundEnabled(void) {
    return metronome_is_sound_enabled();
}

/**
 * @brief AudioLooper接口：获取BPM
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

/**
 * @brief AudioLooper接口：获取当前小节号
 */
static uint32_t AudioLooper_MetronomeGetCurrentMeasure(void) {
    return metronome_get_current_measure();
}

/**
 * @brief AudioLooper接口：获取当前拍子号
 */
static uint8_t AudioLooper_MetronomeGetCurrentBeat(void) {
    return metronome_get_current_beat();
}

/**
 * @brief AudioLooper接口：获取总拍数
 */
static uint32_t AudioLooper_MetronomeGetTotalBeats(void) {
    return metronome_get_total_beats();
}

/**
 * @brief AudioLooper接口：获取总小节数
 */
static uint32_t AudioLooper_MetronomeGetTotalMeasures(void) {
    return metronome_get_total_measures();
}

/**
 * @brief AudioLooper接口：检查当前小节是否刚完成
 */
static uint8_t AudioLooper_MetronomeIsMeasureComplete(void) {
    return metronome_is_measure_complete();
}

/**
 * @brief AudioLooper接口：重置小节和拍子计数
 */
static void AudioLooper_MetronomeResetCounts(void) {
    metronome_reset_counts();
}

/**
 * @brief AudioLooper接口：循环重置时调用
 */
static void AudioLooper_MetronomeOnLoopReset(void) {
    metronome_on_loop_reset();
}

// ============================================================================
// 节拍器模块内部函数实现
// ============================================================================

/**
 * @brief 初始化节拍器模块
 */
void metronome_init(void) {
    // 初始化节拍器配置
    g_loop_manager.metronome.state = METRONOME_OFF;
    g_loop_manager.metronome.config.bpm = 120;
    g_loop_manager.metronome.config.beats_per_measure = 4;
    g_loop_manager.metronome.config.volume = 80;
    g_loop_manager.metronome.config.sound_enabled = 1;
    
    // 初始化小节信息
    g_loop_manager.metronome.measure_info.measure_number = 1;
    g_loop_manager.metronome.measure_info.beat_in_measure = 1;
    g_loop_manager.metronome.measure_info.total_beats = 0;
    g_loop_manager.metronome.measure_info.total_measures = 0;
    g_loop_manager.metronome.measure_info.measure_complete = 0;
    
    // 初始化运行状态
    g_loop_manager.metronome.current_beat = 0;
    g_loop_manager.metronome.current_beat_type = BEAT_TYPE_DOWNBEAT;
    g_loop_manager.metronome.beat_interval_samples = (METRONOME_SAMPLE_RATE * 60) / g_loop_manager.metronome.config.bpm;
    g_loop_manager.metronome.sample_counter = 0;
    g_loop_manager.metronome.loop_reset_flag = 0;
    
    DBG("Metronome initialized: BPM=%d, BeatsPerMeasure=%d\n", 
        g_loop_manager.metronome.config.bpm,
        g_loop_manager.metronome.config.beats_per_measure);
}

/**
 * @brief 切换节拍器开关状态
 */
void metronome_toggle(void) {
    g_loop_manager.metronome.state = (g_loop_manager.metronome.state == METRONOME_OFF) ? METRONOME_ON : METRONOME_OFF;
    
    if (g_loop_manager.metronome.state == METRONOME_ON) {
        // 开启时重置状态
        g_loop_manager.metronome.sample_counter = 0;
        g_loop_manager.metronome.current_beat = 0;
        g_loop_manager.metronome.current_beat_type = BEAT_TYPE_DOWNBEAT;
        DBG("Metronome enabled\n");
    } else {
        DBG("Metronome disabled\n");
    }
}

/**
 * @brief 检查节拍器是否开启
 * @return 1=开启，0=关闭
 */
uint8_t metronome_is_enabled(void) {
    return (g_loop_manager.metronome.state == METRONOME_ON) ? 1 : 0;
}

/**
 * @brief 设置节拍器BPM
 * @param bpm BPM值（建议范围：60-200）
 */
void metronome_set_bpm(uint16_t bpm) {
    if (bpm >= 60 && bpm <= 200) {
        g_loop_manager.metronome.config.bpm = bpm;
        g_loop_manager.metronome.beat_interval_samples = (METRONOME_SAMPLE_RATE * 60) / bpm;
        DBG("Metronome BPM set to %d\n", bpm);
    }
}

/**
 * @brief 获取节拍器BPM
 * @return 当前BPM值
 */
uint16_t metronome_get_bpm(void) {
    return g_loop_manager.metronome.config.bpm;
}

/**
 * @brief 设置每小节拍数
 * @param beats 每小节拍数（建议范围：2-8）
 */
void metronome_set_beats_per_measure(uint8_t beats) {
    if (beats >= 2 && beats <= 8) {
        g_loop_manager.metronome.config.beats_per_measure = beats;
        DBG("Metronome beats per measure set to %d\n", beats);
    }
}

/**
 * @brief 获取每小节拍数
 * @return 当前每小节拍数
 */
uint8_t metronome_get_beats_per_measure(void) {
    return g_loop_manager.metronome.config.beats_per_measure;
}

/**
 * @brief 设置节拍器音量
 * @param volume 音量值（0.0-1.0）
 */
void metronome_set_volume(float volume) {
    if (volume >= 0.0f && volume <= 1.0f) {
        g_loop_manager.metronome.config.volume = volume;
        DBG("Metronome volume set to %.2f\n", volume);
    }
}

/**
 * @brief 获取当前拍子号（在整个序列中）
 * @return 当前拍子号
 */
uint8_t metronome_get_current_beat(void) {
    return g_loop_manager.metronome.measure_info.beat_in_measure;
}

/**
 * @brief 节拍器音频处理
 * @param output_data 输出音频数据缓冲区
 * @param length 数据长度（采样点数）
 */
void metronome_process_audio(uint32_t* output_data, uint16_t length) {
    uint32_t i;
    
    if (!metronome_is_enabled() || !g_loop_manager.metronome.config.sound_enabled) {
        return;  // 节拍器关闭或静音模式
    }
    
    for (i = 0; i < length; i++) {
        g_loop_manager.metronome.sample_counter++;
        
        // 检查是否到达下一拍
        if (g_loop_manager.metronome.sample_counter >= g_loop_manager.metronome.beat_interval_samples) {
            metronome_advance_beat();
            g_loop_manager.metronome.sample_counter = 0;
        }
        
        // 生成节拍器音频信号（简单的点击声）
        if (g_loop_manager.metronome.sample_counter < 1000) {  // 前1000个采样点播放点击声
            uint32_t click_amplitude = (uint32_t)(0x7FFF * g_loop_manager.metronome.config.volume);  // 音量调节
            
            // 强拍（第一拍）音调更高
            if (g_loop_manager.metronome.current_beat_type == BEAT_TYPE_DOWNBEAT) {
                click_amplitude = (uint32_t)(click_amplitude * 1.5);
                if (click_amplitude > 0x7FFF) click_amplitude = 0x7FFF;
            }
            
            // 混合到输出（假设output_data是32位数据）
            output_data[i] |= (click_amplitude << 16) | click_amplitude;  // 双声道
        }
    }
}

/**
 * @brief 推进到下一拍
 * @note 内部函数，用于处理拍子推进和小节切换逻辑
 */
static void metronome_advance_beat(void) {
    // 增加当前拍子
    g_loop_manager.metronome.current_beat++;
    g_loop_manager.metronome.measure_info.beat_in_measure++;
    g_loop_manager.metronome.measure_info.total_beats++;
    
    // 检查是否完成一个小节
    if (g_loop_manager.metronome.measure_info.beat_in_measure > g_loop_manager.metronome.config.beats_per_measure) {
        // 完成小节，开始新小节
        g_loop_manager.metronome.measure_info.measure_number++;
        g_loop_manager.metronome.measure_info.total_measures++;
        g_loop_manager.metronome.measure_info.beat_in_measure = 1;
        g_loop_manager.metronome.measure_info.measure_complete = 1;
        g_loop_manager.metronome.current_beat_type = BEAT_TYPE_DOWNBEAT;
        
        DBG("Metronome: Completed measure %lu, starting measure %lu\n",
            (unsigned long)(g_loop_manager.metronome.measure_info.measure_number - 1),
            (unsigned long)g_loop_manager.metronome.measure_info.measure_number);
    } else {
        // 普通拍子
        g_loop_manager.metronome.current_beat_type = BEAT_TYPE_REGULAR;
    }
    
    DBG("Metronome: Beat %d/%d in measure %lu (total beats: %lu)\n",
        g_loop_manager.metronome.measure_info.beat_in_measure,
        g_loop_manager.metronome.config.beats_per_measure,
        (unsigned long)g_loop_manager.metronome.measure_info.measure_number,
        (unsigned long)g_loop_manager.metronome.measure_info.total_beats);
    }

// ============================================================================
// AudioLooper模块实例定义
// ============================================================================

/**
 * @brief AudioLooper模块全局实例
 */
AudioLooper_t AudioLooper = {
    // 初始化和配置
    .Init = AudioLooper_Init,
    .InitWithFlashType = AudioLooper_InitWithFlashType,
    .Reset = AudioLooper_Reset,
    .SetFlashType = AudioLooper_SetFlashType,
    
    // 控制操作
    .ButtonPress = AudioLooper_ButtonPress,
    .SegmentButtonPress = AudioLooper_SegmentButtonPress,
    .EncoderLeft = AudioLooper_EncoderLeft,
    .EncoderRight = AudioLooper_EncoderRight,
    .StopRecording = AudioLooper_StopRecording,
    
    // 音频处理
    .ProcessRecording = AudioLooper_ProcessRecording,
    .ProcessPlayback = AudioLooper_ProcessPlayback,
    .ProcessRecording32 = AudioLooper_ProcessRecording32,
    .ProcessPlayback32 = AudioLooper_ProcessPlayback32,
    
    // 状态查询
    .GetStatus = AudioLooper_GetStatus,
    .IsRecording = AudioLooper_IsRecording,
    .IsPlaying = AudioLooper_IsPlaying,
    .GetCurrentAddress = AudioLooper_GetCurrentAddress,
    .GetRecordLength = AudioLooper_GetRecordLength,
    
    // 测试和调试
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
    .MetronomeSetSoundEnabled = AudioLooper_MetronomeSetSoundEnabled,
    .MetronomeIsEnabled = AudioLooper_MetronomeIsEnabled,
    .MetronomeIsSoundEnabled = AudioLooper_MetronomeIsSoundEnabled,
    .MetronomeGetBPM = AudioLooper_MetronomeGetBPM,
    .MetronomeGetBeatsPerMeasure = AudioLooper_MetronomeGetBeatsPerMeasure,
    
    // 小节和拍子信息查询
    .MetronomeGetCurrentMeasure = AudioLooper_MetronomeGetCurrentMeasure,
    .MetronomeGetCurrentBeat = AudioLooper_MetronomeGetCurrentBeat,
    .MetronomeGetTotalBeats = AudioLooper_MetronomeGetTotalBeats,
    .MetronomeGetTotalMeasures = AudioLooper_MetronomeGetTotalMeasures,
    .MetronomeIsMeasureComplete = AudioLooper_MetronomeIsMeasureComplete,
    .MetronomeResetCounts = AudioLooper_MetronomeResetCounts,
    .MetronomeOnLoopReset = AudioLooper_MetronomeOnLoopReset
};
