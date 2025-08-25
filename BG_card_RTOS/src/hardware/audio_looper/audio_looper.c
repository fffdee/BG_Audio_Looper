/**
 **************************************************************************************
 * @file    audio_looper.c
 * @brief   Audio looper functions implementation
 *
 * @author  BanGO
 * @version V1.0.0
 *
 * @Copyright (C) 2025, Audio Looper Project. All rights reserved.
 ************************            
            // 混合音频数据
            uint16_t samples_to_mix = bytes_to_read / 2;
            for (i = 0; i < samples_to_mix && i < length; i++) {
                int32_t mixed = (int32_t)output_data[i] + (int32_t)ReadBuf[i];
                output_data[i] = __nds32__clips(mixed, 15);  // 16位饱和限制
            }
            
            g_loop_manager.play_position += bytes_to_read;****************************************************
 */

#include "audio_looper.h"
#include "debug.h"
#include "bg_flash_manager.h"
#include "type.h"
#include <nds32_intrinsic.h>
#include <math.h>
#include <string.h>

// 全局Loop管理器
LoopManager_t g_loop_manager = {0};

// 内存缓冲区用于调试 (约0.1秒的48KHz单声道音频，极小测试)
#define MEMORY_BUFFER_SIZE (4800)  // 0.1秒缓冲区，最小测试
static int16_t memory_buffer[MEMORY_BUFFER_SIZE];
static uint32_t memory_write_pos = 0;
static uint32_t memory_read_pos = 0;
static uint32_t memory_data_length = 0;

// 外部全局变量引用 - 为了兼容原有代码
extern uint32_t sectorAddress;
extern uint16_t rec, rea, play;
extern uint16_t time;
extern uint8_t record_flag;
extern uint8_t play_flag;
extern int16_t ReadBuf[96];
extern uint16_t read_write;

// 校验相关变量
static uint32_t recording_sample_count = 0;
static uint32_t playback_sample_count = 0;
static int16_t last_recorded_sample = 0;
static int16_t first_playback_sample = 0;

// 外部函数声明
extern void convertInt16ArrayToUint8Array(const int16_t *input, uint8_t *output, uint32_t size);
extern void convertUint8ArrayToInt16Array(const uint8_t *input, int16_t *output, uint32_t size);

/**
 * @brief 初始化Loop管理器
 */
void loop_init(void)
{
    memset(&g_loop_manager, 0, sizeof(LoopManager_t));
    
    g_loop_manager.state = LOOP_STATE_IDLE;
    g_loop_manager.flash_type = FLASH_TYPE_NAND;  // 改为默认使用NAND Flash
    g_loop_manager.sector_address = 0;
    g_loop_manager.record_length = 0;
    g_loop_manager.play_position = 0;
    g_loop_manager.is_initialized = 1;
    g_loop_manager.is_new_recording = 0;
    g_loop_manager.use_memory_buffer = 0;  // 暂时禁用内存缓冲区，使用Flash模式
    
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
    
    // 启动自动测试（初始化后自动录制10秒然后播放）
    //loop_start_auto_test();
    
    // 清空内存缓冲区
    memory_write_pos = 0;
    memory_read_pos = 0;
    memory_data_length = 0;
    memset(memory_buffer, 0, sizeof(memory_buffer));
    
    // 同步全局变量
    play_flag = 2; // 对应空闲状态
    sectorAddress = 0;
    rec = 0;
    rea = 0;
    play = 0;
    
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
    g_loop_manager.sector_address = 0;
    g_loop_manager.record_length = 0;
    g_loop_manager.play_position = 0;
    g_loop_manager.is_initialized = 1;
    g_loop_manager.is_new_recording = 0;
    g_loop_manager.use_memory_buffer = 0;  // 暂时禁用内存缓冲区，使用Flash模式
    
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
    
    // 清空内存缓冲区
    memory_write_pos = 0;
    memory_read_pos = 0;
    memory_data_length = 0;
    memset(memory_buffer, 0, sizeof(memory_buffer));
    
    // 同步全局变量
    play_flag = 2; // 对应空闲状态
    sectorAddress = 0;
    rec = 0;
    rea = 0;
    play = 0;
    
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
    
    // 重置内存缓冲区
    memory_write_pos = 0;
    memory_read_pos = 0;
    memory_data_length = 0;
    
    // 重置计数器
    rec = 0;
    rea = 0;
    play = 0;
    
    // 同步全局变量
    play_flag = 2; // 空闲状态
    sectorAddress = 0;
    
    DBG("Loop manager reset\n");
}

/**
 * @brief 处理按键按下事件
 * 实现简单的三步循环：空闲→录制→播放→（擦除回到空闲）
 */
void loop_handle_button_press(void)
{
    // 自动测试模式下不响应按键
    if (g_loop_manager.auto_test_mode) {
        return;
    }
    
    if (!g_loop_manager.is_initialized) {
        DBG("Loop manager not initialized\n");
        return;
    }
    
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
            loop_stop_current_segment();
            g_loop_manager.state = LOOP_STATE_PLAYING;
            play_flag = 1; // 播放状态
            DBG("Stop recording segment %d, start playing %d segments\n",
                g_loop_manager.current_segment + 1, g_loop_manager.active_segments);
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
                play_flag = 2; // 空闲状态
                DBG("Max segments reached, stop playing\n");
            }
            break;
            
        default:
            g_loop_manager.state = LOOP_STATE_IDLE;
            play_flag = 2;
            break;
    }
    
    // 同步全局变量
    sectorAddress = g_loop_manager.sector_address;
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
    play_flag = 2; // 空闲状态
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
    play_flag = 2; // 空闲状态
    
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
            nand_audio_flush_buffer(flash_device_id);
        }
        
        // 记录录制长度并重置播放位置
        g_loop_manager.record_length = g_loop_manager.sector_address;
        g_loop_manager.play_position = 0;
        g_loop_manager.state = LOOP_STATE_PLAYING;
        play_flag = 1;
        
        DBG("Recording stopped manually: total_samples=%d, record_length=%d bytes, last_sample=%d\n", 
            recording_sample_count, g_loop_manager.record_length, last_recorded_sample);
        
        // 重置地址指针准备下次录制
        g_loop_manager.sector_address = 0;
        read_write++;
        rec = 0;
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
    
    if (g_loop_manager.use_memory_buffer) {
        // 使用内存缓冲区录制
        uint16_t i;
        for (i = 0; i < length && memory_write_pos < MEMORY_BUFFER_SIZE; i++) {
            memory_buffer[memory_write_pos++] = audio_data[i];
        }
        rec++;
        
        // 检查缓冲区是否满
        if (memory_write_pos >= MEMORY_BUFFER_SIZE) {
            DBG("Memory buffer full, stop recording. Samples: %d\n", memory_write_pos);
            memory_data_length = memory_write_pos;
            memory_read_pos = 0;
            
            // 自动停止录制并开始播放
            g_loop_manager.state = LOOP_STATE_PLAYING;
            play_flag = 1;
        }
        
        // 调试信息
        if (rec % 100 == 0) {
           // DBG("Memory recording: %d samples, pos: %d\n", rec, memory_write_pos);
        }
    } else {
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
                amplitude_sum += abs(sample);
                if (abs(sample) > abs(max_amplitude)) {
                    max_amplitude = sample;
                }
            }
        }
        
        // 记录统计信息
        recording_sample_count += length;
        if (length > 0) {
            last_recorded_sample = audio_data[length - 1];
        }
        
        // 如果输入信号太弱，提示调整增益
        if (rec % 200 == 0 && non_zero_count > 0) {
            int32_t avg_amplitude = amplitude_sum / non_zero_count;
            if (avg_amplitude < 100) {  // 信号较弱
                DBG("WARNING: Input signal weak, avg_amp=%d, max=%d, consider increasing gain\n",
                    avg_amplitude, max_amplitude);
            }
        }
        
        convertInt16ArrayToUint8Array(audio_data, buffer, length);
        
        // Flash页面大小通常是256字节，我们写入length*2字节的数据
        uint32_t bytes_to_write = length * 2;  // 16位音频转8位需要*2
        uint8_t flash_device_id = loop_get_flash_device_id();
        
        uint8_t write_result;
        
        // 使用优化的音频写入函数
        if (flash_device_id == DEV_NAND) {
            // NAND Flash使用页面对齐缓冲写入
            write_result = nand_audio_write_buffered(g_loop_manager.sector_address, buffer, bytes_to_write, flash_device_id);
        } else {
            // NOR Flash直接写入
            write_result = BG_flash_manager.PageProgram(g_loop_manager.sector_address, buffer, bytes_to_write, flash_device_id);
        }
        
        // 检查写入是否成功
        if (write_result != FLASH_STATUS_OK) {
            // 写入失败，停止录制
            // DBG("Audio write failed, stopping recording\n");
            g_loop_manager.state = LOOP_STATE_PLAYING;
            play_flag = 1;
            return;
        }
        
        rec++;
        g_loop_manager.sector_address += bytes_to_write;  // 按实际写入字节数递增
        
//        if (rec % 500 == 0) {  // 减少调试输出频率，避免影响实时性
//            //DBG("Flash recording: packets=%d, addr=%d, bytes=%d, nonzero=%d, avg_amp=%d, last_sample=%d\n",
//                rec, g_loop_manager.sector_address, bytes_to_write, non_zero_count,
//                non_zero_count > 0 ? amplitude_sum / non_zero_count : 0, last_recorded_sample);
//        }
        
        // 检查Flash存储空间
        if (g_loop_manager.sector_address >= BG_flash_manager.GetTotalByte(flash_device_id)) {
            DBG("Flash full, stop recording. Address: %d, Total: %d (%s)\n", 
                g_loop_manager.sector_address, BG_flash_manager.GetTotalByte(flash_device_id),
                g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
            
            g_loop_manager.record_length = g_loop_manager.sector_address;  // 正确记录录制长度
            g_loop_manager.play_position = 0;  // 重置播放位置
            g_loop_manager.state = LOOP_STATE_PLAYING;
            play_flag = 1;
            
            DBG("Recording finished: total_samples=%d, record_length=%d, last_sample=%d\n", 
                recording_sample_count, g_loop_manager.record_length, last_recorded_sample);
            
            read_write++;
            rec = 0;
            rea = 0;
            play = 0;
        }
        
        sectorAddress = g_loop_manager.sector_address;
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
    
    if (g_loop_manager.use_memory_buffer) {
        // 使用内存缓冲区播放
        if (memory_data_length == 0) {
            DBG("No recorded data in memory buffer\n");
            return;  // 没有录制数据，保持原始音频
        }
        
        for (i = 0; i < length; i++) {
            if (memory_read_pos < memory_data_length) {
                // 混合音频数据
                int32_t mixed = (int32_t)output_data[i] + (int32_t)memory_buffer[memory_read_pos];
                output_data[i] = __nds32__clips(mixed, 15);  // 16位饱和限制
                memory_read_pos++;
            } else {
                // 循环播放
                memory_read_pos = 0;
                play++;
//                if (play % 10 == 0) {  // 减少调试输出频率
//                    DBG("Memory loop playback, count: %d, length: %d\n", play, memory_data_length);
//                }
                break;
            }
        }
    } else {
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
                play++;
                DBG("Flash loop restart, count: %d, length: %d, reading: %d\n",
                    play, g_loop_manager.record_length, bytes_to_read);
            }
        }
        
        // 确保有效的读取长度
        if (bytes_to_read == 0) {
            DBG("Warning: bytes_to_read=0, skipping playback\n");
            return;
        }
        
        // 读取Flash数据
        uint8_t flash_device_id = loop_get_flash_device_id();
        BG_flash_manager.ReadData(g_loop_manager.play_position, buffer, bytes_to_read, flash_device_id);
        convertUint8ArrayToInt16Array(buffer, ReadBuf, bytes_to_read/2);
        
        // 数据校验：检查读取的音频数据
        uint16_t valid_samples = bytes_to_read / 2;
        uint16_t non_zero_read = 0;
        int32_t read_amplitude_sum = 0;
        uint16_t j;
        for (j = 0; j < valid_samples; j++) {
            if (ReadBuf[j] != 0) {
                non_zero_read++;
                read_amplitude_sum += abs(ReadBuf[j]);
            }
        }
        
        // 记录第一个播放的样本用于校验
        if (g_loop_manager.play_position == 0 && valid_samples > 0) {
            first_playback_sample = ReadBuf[0];
            DBG("First playback sample: %d (should match last recorded: %d)\n",
                first_playback_sample, last_recorded_sample);
        }
        
        // 混合音频数据
        uint16_t samples_to_mix = (valid_samples < length) ? valid_samples : length;
        for (i = 0; i < samples_to_mix; i++) {
            int32_t mixed = (int32_t)output_data[i] + (int32_t)ReadBuf[i];
            output_data[i] = __nds32__clips(mixed, 15);  // 16位饱和限制
        }
        
        playback_sample_count += samples_to_mix;
        g_loop_manager.play_position += bytes_to_read;
        
        // 每播放50次打印一次调试信息，增加频率便于调试
        static uint32_t debug_counter = 0;
        debug_counter++;
//        if (debug_counter % 50 == 0) {
//            DBG("Playing: pos=%d/%d, read=%d, mix_samples=%d, nonzero=%d, avg_amp=%d, first=%d\n",
//                g_loop_manager.play_position, g_loop_manager.record_length, bytes_to_read,
//                samples_to_mix, non_zero_read,
//                non_zero_read > 0 ? read_amplitude_sum / non_zero_read : 0, ReadBuf[0]);
//        }
        
        sectorAddress = g_loop_manager.play_position;
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
    
    // 确保全局变量同步
    sectorAddress = (g_loop_manager.state == LOOP_STATE_PLAYING) ? 
                   g_loop_manager.play_position : g_loop_manager.sector_address;
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
    return (g_loop_manager.state == LOOP_STATE_RECORDING) ? 1 : 0;
}

/**
 * @brief 检查是否正在播放
 */
uint8_t loop_is_playing(void)
{
    return (g_loop_manager.state == LOOP_STATE_PLAYING) ? 1 : 0;
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

/**
 * @brief Flash数据完整性检查
 * @param test_length 要检查的数据长度（字节）
 * @return 1=数据完整，0=数据损坏
 */
uint8_t loop_verify_flash_data(uint32_t test_length)
{
    if (g_loop_manager.record_length == 0 || test_length == 0) {
        DBG("No data to verify or invalid length\n");
        return 0;
    }
    
    uint32_t check_length = (test_length > g_loop_manager.record_length) ? 
                           g_loop_manager.record_length : test_length;
    
    uint8_t test_buffer[96 * 2];  // 临时缓冲区
    int16_t test_samples[96];
    uint32_t total_samples = 0;
    uint32_t non_zero_samples = 0;
    uint32_t pos = 0;
    
    DBG("Verifying flash data: check_length=%d, record_length=%d\n", 
        check_length, g_loop_manager.record_length);
    
    // 分块读取并验证
    while (pos < check_length) {
        uint32_t read_size = (check_length - pos > sizeof(test_buffer)) ? 
                           sizeof(test_buffer) : (check_length - pos);
        
        // 确保读取偶数字节（因为每个样本2字节）
        if (read_size % 2 != 0) read_size--;
        
        uint8_t flash_device_id = loop_get_flash_device_id();
        BG_flash_manager.ReadData(pos, test_buffer, read_size, flash_device_id);
        convertUint8ArrayToInt16Array(test_buffer, test_samples, read_size/2);
        
        // 统计样本
        uint16_t samples_in_block = read_size / 2;
        total_samples += samples_in_block;
        
        uint16_t i;
        for (i = 0; i < samples_in_block; i++) {
            if (test_samples[i] != 0) {
                non_zero_samples++;
            }
        }
        
        pos += read_size;
    }
    
    float non_zero_ratio = (float)non_zero_samples / total_samples;
    DBG("Flash verification: total_samples=%d, non_zero=%d, ratio=%.3f\n", 
        total_samples, non_zero_samples, non_zero_ratio);
    
    // 如果非零样本比例太低，可能是数据损坏
    if (non_zero_ratio < 0.001f) {  // 少于0.1%的非零样本认为异常
        DBG("WARNING: Very low non-zero ratio, possible data corruption\n");
        return 0;
    }
    
    return 1;
}

/**
 * @brief 录制处理函数 - uint32_t版本
 * @param audio_data uint32_t格式的音频数据
 * @param buffer 临时缓冲区用于数据转换
 * @param length uint32_t数据的数量
 */
void loop_process_recording_uint32(uint32_t* audio_data, uint8_t* buffer, uint16_t length)
{
    // 更新自动测试状态
    loop_update_auto_test();
    
    if (g_loop_manager.state != LOOP_STATE_RECORDING) {
        return;  // 不在录制状态
    }
    
    if (g_loop_manager.use_memory_buffer) {
        // 内存录制：需要转换为int16_t格式存储
        uint16_t i;
        for (i = 0; i < length && memory_write_pos < MEMORY_BUFFER_SIZE - 1; i++) {
            // 提取左声道存储（简化处理）
            int16_t sample = (int16_t)(audio_data[i] & 0xFFFF);
            memory_buffer[memory_write_pos++] = sample;
        }
        rec++;
        
        if (memory_write_pos >= MEMORY_BUFFER_SIZE - 1) {
            DBG("Memory buffer full, stop recording. Samples: %lu\n", (unsigned long)memory_write_pos);
            memory_data_length = memory_write_pos;
            memory_read_pos = 0;
            g_loop_manager.state = LOOP_STATE_PLAYING;
            play_flag = 1;
        }
    } else {
        // Flash录制：直接录制新段
        uint32_t bytes_to_write = 192;  // 只存储192字节（48个uint32_t * 4字节）

        // 正常录制模式：直接写入新数据，但在录制结尾添加淡出处理
        uint8_t write_buffer[192];
        
        // 🔧 关键修复：录制结尾淡出处理，避免"哒"声
        uint8_t need_fadeout = 0;
        uint16_t fadeout_factor = 100;
        
        // 检查是否需要淡出（自动测试模式）
        if (g_loop_manager.auto_test_mode && g_loop_manager.auto_test_state == 0) {
            if (g_loop_manager.record_length >= 552) {  // 最后10页淡出(约50ms)
                need_fadeout = 1;
                uint32_t pages_remaining = 562 - g_loop_manager.record_length;
                // 使用指数淡出曲线，最后不是到0，而是到20%
                fadeout_factor = 20 + (pages_remaining * 8);  // 从20%到100%
                if (fadeout_factor > 100) fadeout_factor = 100;
            }
        }
        
        // 应用淡出到音频数据
        if (need_fadeout) {
            uint16_t fade_idx;
            for (fade_idx = 0; fade_idx < length; fade_idx++) {
                int16_t left = (int16_t)(audio_data[fade_idx] & 0xFFFF);
                int16_t right = (int16_t)((audio_data[fade_idx] >> 16) & 0xFFFF);
                
                left = (int16_t)((int32_t)left * fadeout_factor / 100);
                right = (int16_t)((int32_t)right * fadeout_factor / 100);
                
                audio_data[fade_idx] = ((uint32_t)(uint16_t)right << 16) | ((uint32_t)(uint16_t)left & 0xFFFF);
            }
        }
        
        convertUint32ArrayToUint8Array(audio_data, write_buffer, length);
        
        uint32_t write_address = g_loop_manager.sector_address + g_loop_manager.record_length * g_loop_manager.page_size;
        uint8_t flash_device_id = loop_get_flash_device_id();
        
        uint8_t result = BG_flash_manager.PageProgram(write_address, write_buffer, bytes_to_write, flash_device_id);
        
        if (result == FLASH_STATUS_OK) {
            g_loop_manager.record_length++;
            rec++;
            
            // 检查是否达到第一段的长度限制（确保所有段等长）
            if (g_loop_manager.current_segment > 0 && g_loop_manager.segments[0].is_active) {
                uint32_t first_segment_length = g_loop_manager.segments[0].length_pages;
                if (g_loop_manager.record_length >= first_segment_length - 10) {
                    // 接近第一段长度时开始淡出录制，但不要淡到0(提前50ms)
                    uint32_t fade_pages = first_segment_length - g_loop_manager.record_length;
                    uint16_t fade_factor = 20 + (fade_pages * 8);  // 从20%到100%，保持低音量而非静音
                    if (fade_factor > 100) fade_factor = 100;
                    
                    // 对录制的音频数据应用淡出
                    uint16_t fade_idx;
                    for (fade_idx = 0; fade_idx < length; fade_idx++) {
                        int16_t left = (int16_t)(audio_data[fade_idx] & 0xFFFF);
                        int16_t right = (int16_t)((audio_data[fade_idx] >> 16) & 0xFFFF);
                        
                        left = (int16_t)((int32_t)left * fade_factor / 100);
                        right = (int16_t)((int32_t)right * fade_factor / 100);
                        
                        audio_data[fade_idx] = ((uint32_t)(uint16_t)right << 16) | ((uint32_t)(uint16_t)left & 0xFFFF);
                    }
                    
                    // 重新转换已淡化的数据
                    convertUint32ArrayToUint8Array(audio_data, write_buffer, length);
                }
                
                if (g_loop_manager.record_length >= first_segment_length) {
                    // 达到第一段长度，自动停止录制
                    DBG("Segment %d reached first segment length (%lu pages), auto-stop\n",
                        g_loop_manager.current_segment + 1, (unsigned long)first_segment_length);
                    
                    loop_stop_current_segment();
                    g_loop_manager.state = LOOP_STATE_PLAYING;
                    rec = 0;
                }
            }
            
            // 调试信息
            if (rec % 100 == 0) {
                //DBG("Flash recording: page %lu, address 0x%lx\n", 
                //    (unsigned long)g_loop_manager.record_length, (unsigned long)write_address);
            }
        } else {
            DBG("Flash write error: %d at address 0x%lx\n", result, (unsigned long)write_address);
        }
    }
}

/**
 * @brief 播放处理函数 - uint32_t版本
 * @param output_data uint32_t格式的输出音频数据
 * @param buffer 临时缓冲区用于数据转换
 * @param length uint32_t数据的数量
 */
void loop_process_playback_uint32(uint32_t* output_data, uint8_t* buffer, uint16_t length)
{
    // 更新自动测试状态
    loop_update_auto_test();
    
    if (g_loop_manager.state != LOOP_STATE_PLAYING && g_loop_manager.state != LOOP_STATE_RECORDING) {
        return;  // 不在播放状态且不在录制状态，保持output_data为0
    }
    
    if (g_loop_manager.active_segments == 0) {
        // 没有录制段，清零输出缓冲区确保静音
        uint16_t i;
        for (i = 0; i < length; i++) {
            output_data[i] = 0;
        }
        return;
    }
    
    if (g_loop_manager.use_memory_buffer) {
        // 内存播放：从int16_t转换为uint32_t
        uint16_t i;
        for (i = 0; i < length; i++) {
            if (memory_read_pos < memory_data_length) {
                int16_t sample = memory_buffer[memory_read_pos];
                
                // 直接设置播放内容，不混合原始输入
                // 简化：双声道都播放同样内容
                output_data[i] = ((uint32_t)(uint16_t)sample << 16) | ((uint32_t)(uint16_t)sample & 0xFFFF);
                memory_read_pos++;
            } else {
                memory_read_pos = 0;
                play++;
                break;
            }
        }
    } else {
        // Flash多段混音播放
        // 找到最长的段作为播放循环的基准，但要减去结尾的问题区域
        uint32_t max_length = 0;
        uint8_t i;
        uint8_t total_active = 0;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            if (g_loop_manager.segments[i].is_active) {
                total_active++;
                if (g_loop_manager.segments[i].length_pages > max_length) {
                    max_length = g_loop_manager.segments[i].length_pages;
                }
            }
        }
        
        if (max_length == 0) {
            return;  // 没有有效段
        }
        
        // 🔧 关键修复：避开录制结尾的问题区域，提前100ms结束循环
        // 100ms约等于18页 (48kHz * 0.1s / 256bytes_per_page / 4bytes_per_sample)
        uint32_t early_end_pages = 5;  // 提前18页(约100ms)结束
        uint32_t effective_length = (max_length > early_end_pages) ? 
                                   (max_length - early_end_pages) : max_length;
        
        // 检查播放位置是否到达有效长度，提前重置循环
        uint8_t is_near_loop_end = 0;
        uint8_t is_at_loop_start = 0;
        uint16_t fade_factor = 100; // 默认100%音量
        
        if (g_loop_manager.play_position >= effective_length) {
            g_loop_manager.play_position = 0;
            play++;
            is_at_loop_start = 1;  // 刚刚重置到开头
            fade_factor = 100;  // 开头正常音量，因为避开了问题区域
        } else if (g_loop_manager.play_position >= effective_length - 15) {
            // 接近有效长度结尾时开始淡出
            is_near_loop_end = 1;
            uint32_t pages_from_end = effective_length - g_loop_manager.play_position;
            fade_factor = 60 + (pages_from_end * 2);  // 从60%到90%渐变
        } else if (g_loop_manager.play_position <= 5) {
            // 前5页淡入，简化处理
            is_at_loop_start = 1;
            fade_factor = 80 + (g_loop_manager.play_position * 4);  // 从80%到100%渐变
        }
        
        // 计算实际要读取的样本数
        uint32_t samples_per_page = 192 / 4;  // 每页192字节 = 48个uint32样本
        uint32_t page_offset = g_loop_manager.play_position;
        uint32_t remaining_in_page = samples_per_page;
        uint32_t samples_to_read = (length < remaining_in_page) ? length : remaining_in_page;
        
        // 混音所有活跃段的数据
        uint32_t mixed_samples[48];  // 临时混音缓冲区
        memset(mixed_samples, 0, sizeof(mixed_samples));
        
        uint8_t active_segment_count = 0;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            if (!g_loop_manager.segments[i].is_active) {
                continue;
            }
            
            // 计算当前段内的页偏移（统一按播放位置访问），并对每段应用提前结束保护
            uint32_t segment_page = page_offset;
            uint32_t segment_effective_length = g_loop_manager.segments[i].length_pages;
            
            // 🔧 关键修复：每个段都要避开录制结尾的问题区域
            if (segment_effective_length > 20) {
                segment_effective_length -= 18;  // 长段提前18页结束(约100ms)
            } else if (segment_effective_length > 10) {
                segment_effective_length -= 8;   // 中段提前8页结束(约40ms)
            } else if (segment_effective_length > 5) {
                segment_effective_length -= 3;   // 短段提前3页结束(约15ms)
            }
            // 如果段太短（<=5页），则不做提前结束处理
            
            if (segment_page >= segment_effective_length && segment_effective_length > 0) {
                // 如果超过该段的有效长度，则循环到该段的开头
                segment_page = segment_page % segment_effective_length;
            }
            
            uint32_t segment_address = g_loop_manager.segments[i].start_address +
                                     segment_page * g_loop_manager.page_size;

            // 读取段数据
            uint8_t flash_device_id = loop_get_flash_device_id();
            BG_flash_manager.ReadData(segment_address, buffer, 192, flash_device_id);
            uint32_t segment_data[48];
            convertUint8ArrayToUint32Array(buffer, segment_data, samples_to_read);

            // 检查并清理异常数据
            uint16_t data_check;
            for (data_check = 0; data_check < samples_to_read; data_check++) {
                int16_t seg_left = (int16_t)(segment_data[data_check] & 0xFFFF);
                int16_t seg_right = (int16_t)((segment_data[data_check] >> 16) & 0xFFFF);
                
                // 检查异常值
                if (seg_left < -32000 || seg_left > 32000 || seg_right < -32000 || seg_right > 32000) {
                    segment_data[data_check] = 0x80008000;  // 静音值
                }
            }

            // 混音到临时缓冲区（增大音量）
            uint16_t j;
            for (j = 0; j < samples_to_read; j++) {
                int16_t seg_left = (int16_t)(segment_data[j] & 0xFFFF);
                int16_t seg_right = (int16_t)((segment_data[j] >> 16) & 0xFFFF);
                int16_t mix_left = (int16_t)(mixed_samples[j] & 0xFFFF);
                    int16_t mix_right = (int16_t)((mixed_samples[j] >> 16) & 0xFFFF);

                    // 增大音量混音（每段贡献原音量的60%，提升播放音量）
                    int32_t new_left = (int32_t)mix_left + ((int32_t)seg_left * 5 / 5);
                    int32_t new_right = (int32_t)mix_right + ((int32_t)seg_right * 5 / 5);

                    // 立即进行软限幅
                    new_left = __nds32__clips(new_left, 15);
                    new_right = __nds32__clips(new_right, 15);

                    mixed_samples[j] = ((uint32_t)(uint16_t)new_right << 16) |
                                      ((uint32_t)(uint16_t)new_left & 0xFFFF);
                }
                active_segment_count++;
        }
        
        // 增大整体播放音量
        if (active_segment_count > 0) {
            uint16_t j;
            for (j = 0; j < samples_to_read; j++) {
                int16_t left = (int16_t)(mixed_samples[j] & 0xFFFF);
                int16_t right = (int16_t)((mixed_samples[j] >> 16) & 0xFFFF);
                
                // 增大音量输出，提升播放效果
                int32_t boosted_left = (int32_t)left * 3 / 2;   // 增益1.5倍
                int32_t boosted_right = (int32_t)right * 3 / 2; // 增益1.5倍

                // 软限幅防止溢出
                boosted_left = __nds32__clips(boosted_left, 15);
                boosted_right = __nds32__clips(boosted_right, 15);
                
                mixed_samples[j] = ((uint32_t)(uint16_t)boosted_right << 16) | 
                                  ((uint32_t)(uint16_t)boosted_left & 0xFFFF);
            }
        }
        
        // 复制混音结果到输出，应用简化的淡化处理
        uint16_t j;
        for (j = 0; j < samples_to_read && j < length; j++) {
            uint32_t sample = mixed_samples[j];
            
            // 简化的淡化处理，因为已经避开了问题区域
            if (fade_factor < 100) {
                int16_t left = (int16_t)(sample & 0xFFFF);
                int16_t right = (int16_t)((sample >> 16) & 0xFFFF);
                
                // 应用淡化系数
                int32_t fade_left = (int32_t)left * fade_factor / 100;
                int32_t fade_right = (int32_t)right * fade_factor / 100;
                
                // 软限幅
                fade_left = __nds32__clips(fade_left, 15);
                fade_right = __nds32__clips(fade_right, 15);
                
                sample = ((uint32_t)(uint16_t)fade_right << 16) | ((uint32_t)(uint16_t)fade_left & 0xFFFF);
            }
            
            output_data[j] = sample;
        }
        
        // 移动播放位置
        g_loop_manager.play_position++;
    }
}

/**
 * @brief 开始录制新段
 */
void loop_start_new_segment(void)
{
    if (g_loop_manager.active_segments >= MAX_SEGMENTS) {
        DBG("Cannot start new segment: maximum segments reached\n");
        return;
    }
    
    // 计算新段的起始地址（页对齐）
    uint32_t start_page = 0;
    if (g_loop_manager.active_segments > 0) {
        // 找到最后一个段的结束位置
        uint8_t i;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            if (g_loop_manager.segments[i].is_active) {
                uint32_t segment_end = g_loop_manager.segments[i].start_address + 
                                     g_loop_manager.segments[i].length_pages * g_loop_manager.page_size;
                if (segment_end > start_page * g_loop_manager.page_size) {
                    start_page = segment_end / g_loop_manager.page_size;
                }
            }
        }
    }
    
    // 设置新段参数
    g_loop_manager.current_segment = g_loop_manager.active_segments;
    g_loop_manager.segments[g_loop_manager.current_segment].start_address = start_page * g_loop_manager.page_size;
    g_loop_manager.segments[g_loop_manager.current_segment].length_pages = 0;
    g_loop_manager.segments[g_loop_manager.current_segment].is_active = 1;
    
    // 设置录制参数
    g_loop_manager.sector_address = g_loop_manager.segments[g_loop_manager.current_segment].start_address;
    g_loop_manager.record_length = 0;  // 重置录制长度计数器
    g_loop_manager.state = LOOP_STATE_RECORDING;
    g_loop_manager.is_new_recording = 1;
    play_flag = 0; // 录制状态
    rec = 0;
    
    DBG("Started segment %d at page %lu (address 0x%08lX) using %s Flash\n", 
        g_loop_manager.current_segment, (unsigned long)start_page, 
        (unsigned long)g_loop_manager.sector_address,
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
    
    // 额外调试信息：显示Flash类型数值
    DBG("DEBUG: Flash type value = %d (0=NOR, 1=NAND), device_id will be %d\n", 
        g_loop_manager.flash_type, loop_get_flash_device_id());
}

/**
 * @brief 停止当前段录制
 */
void loop_stop_current_segment(void)
{
    if (g_loop_manager.current_segment >= MAX_SEGMENTS || 
        !g_loop_manager.segments[g_loop_manager.current_segment].is_active) {
        DBG("No active segment to stop\n");
        return;
    }
    
    // 🔧 关键修复：在录制结尾写入少量静音页，确保平滑结束
    uint8_t silence_buffer[192];
    uint32_t silence_samples[48];
    uint16_t sil_idx;
    uint8_t silence_pages_to_add = 3;  // 只添加3页静音(约15ms)
    uint8_t page_count;
    
    // 生成正确的静音值 (0x80008000 = 两个16位的零点)
    for (sil_idx = 0; sil_idx < 48; sil_idx++) {
        silence_samples[sil_idx] = 0x80008000;
    }
    
    // 转换为字节数组
    convertUint32ArrayToUint8Array(silence_samples, silence_buffer, 48);
    
    // 写入少量静音页
    uint8_t flash_device_id = loop_get_flash_device_id();
    for (page_count = 0; page_count < silence_pages_to_add; page_count++) {
        uint32_t silence_address = g_loop_manager.sector_address + 
                                  (g_loop_manager.record_length + page_count) * g_loop_manager.page_size;
        BG_flash_manager.PageProgram(silence_address, silence_buffer, 192, flash_device_id);
    }
    
    g_loop_manager.record_length += silence_pages_to_add;  // 增加少量静音页数
    
    // 计算当前段的长度（页数）
    uint32_t start_address = g_loop_manager.segments[g_loop_manager.current_segment].start_address;
    uint32_t current_address = g_loop_manager.sector_address;
    uint32_t length_pages = g_loop_manager.record_length;  // 使用已录制的页数（包含3页静音）
    
    DBG("Stop segment %d: recorded %lu pages (including 3 silence pages), start=0x%08lX, current=0x%08lX\n", 
        g_loop_manager.current_segment, (unsigned long)length_pages,
        (unsigned long)start_address, (unsigned long)current_address);
    
    if (length_pages == 0) {
        // 如果没有录制任何数据，标记段为无效
        g_loop_manager.segments[g_loop_manager.current_segment].is_active = 0;
        DBG("Segment %d has no data, marked as inactive\n", g_loop_manager.current_segment);
    } else {
        // 保存段长度
        g_loop_manager.segments[g_loop_manager.current_segment].length_pages = length_pages;
        g_loop_manager.active_segments++;
        
        // 在自动测试模式下，停止录制后开始播放
        if (g_loop_manager.auto_test_mode) {
            g_loop_manager.state = LOOP_STATE_PLAYING;
            g_loop_manager.play_position = 0;  // 从头开始播放
            play_flag = 1; // 播放状态
        }
        
        DBG("Stopped segment %d: %lu pages, total segments: %d\n", 
            g_loop_manager.current_segment, (unsigned long)length_pages, g_loop_manager.active_segments);
    }
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
        g_loop_manager.segments[i].is_active = 0;
    }
    
    g_loop_manager.active_segments = 0;
    g_loop_manager.current_segment = 0;
    g_loop_manager.sector_address = 0;
    g_loop_manager.play_position = 0;
    
    // 擦除Flash
    uint8_t flash_device_id = loop_get_flash_device_id();
    DBG("Clearing all segments and erasing Flash (%s)...\n",
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
    BG_flash_manager.EraseAll(flash_device_id);
    DBG("All segments cleared\n");
}

/**
 * @brief 启动自动测试模式
 */
void loop_start_auto_test(void)
{
    g_loop_manager.auto_test_mode = 1;
    g_loop_manager.auto_test_timer = 0;
    g_loop_manager.auto_test_state = 0;  // 0=录制阶段
    
    DBG("Starting auto test mode - will record 3 seconds then play using %s Flash\n",
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
    
    // 开始第一段录音
    loop_start_new_segment();
    
    DBG("Auto test: started recording segment using %s Flash\n",
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
}

/**
 * @brief 更新自动测试状态（需要在主循环中调用）
 */
void loop_update_auto_test(void)
{
    if (!g_loop_manager.auto_test_mode) {
        return;
    }
    
    if (g_loop_manager.auto_test_state == 0) {
        // 录制阶段 - 检查是否录制了足够长的时间
        // 使用当前段的长度来判断是否达到3秒（约562页）
        if (g_loop_manager.active_segments > 0) {
            SegmentInfo_t* current_seg = &g_loop_manager.segments[0];
            if (current_seg->length_pages >= 562) {  // 3秒约562页
                DBG("Auto test: recorded %lu pages, switching to playback\n",
                    (unsigned long)current_seg->length_pages);
                // 停止录制，开始播放
                loop_stop_current_segment();
                g_loop_manager.auto_test_state = 1;  // 切换到播放阶段
            }
        } else {
            // 检查当前正在录制的段长度，添加淡出处理
            if (g_loop_manager.state == LOOP_STATE_RECORDING) {
                if (g_loop_manager.record_length >= 552) {  // 提前10页开始淡出(50ms)
                    // 标记即将停止，下次录制处理时应用淡出
                    g_loop_manager.auto_test_timer = 1;  // 用作淡出标记
                }
                if (g_loop_manager.record_length >= 562) {
                    DBG("Auto test: recorded %lu pages, switching to playback\n", 
                        (unsigned long)g_loop_manager.record_length);
                    // 停止录制，开始播放
                    loop_stop_current_segment();
                    g_loop_manager.auto_test_state = 1;  // 切换到播放阶段
                }
            }
        }
    } else if (g_loop_manager.auto_test_state == 1) {
        // 播放阶段 - 持续播放（不停止）
        // 播放状态已经在其他地方处理，这里只需要保持状态
    }
}

/**
 * @brief 停止自动测试模式
 */
void loop_stop_auto_test(void)
{
    g_loop_manager.auto_test_mode = 0;
    g_loop_manager.auto_test_timer = 0;
    g_loop_manager.auto_test_state = 0;
}
