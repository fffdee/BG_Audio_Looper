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
    .use_memory_buffer = 0,
    .current_segment = 0,
    .active_segments = 0,
    .page_size = 256,
    .auto_test_mode = 0,
    .auto_test_timer = 0,
    .auto_test_state = 0,
    .boundary_samples_valid = 0
};

// 内存缓冲区用于调试 (约0.1秒的48KHz单声道音频，极小测试)
#define MEMORY_BUFFER_SIZE (4800)  // 0.1秒缓冲区，最小测试
static int16_t memory_buffer[MEMORY_BUFFER_SIZE];
static uint32_t memory_write_pos = 0;
static uint32_t memory_read_pos = 0;
static uint32_t memory_data_length = 0;

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
    
    // 初始化统计信息
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count = 0;
    g_loop_stats.last_recorded_sample = 0;
    g_loop_stats.first_playback_sample = 0;
    
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
    
    // 初始化统计信息
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count = 0;
    g_loop_stats.last_recorded_sample = 0;
    g_loop_stats.first_playback_sample = 0;
    
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
    // 自动测试模式下不响应按键
    if (g_loop_manager.auto_test_mode) {
        return;
    }
    
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
            loop_stop_current_segment();
            g_loop_manager.state = LOOP_STATE_PLAYING;
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
    
    if (g_loop_manager.use_memory_buffer) {
        // 使用内存缓冲区录制
        uint16_t i;
        for (i = 0; i < length && memory_write_pos < MEMORY_BUFFER_SIZE; i++) {
            memory_buffer[memory_write_pos++] = audio_data[i];
        }
        g_loop_stats.recording_sample_count++;
        
        // 检查缓冲区是否满
        if (memory_write_pos >= MEMORY_BUFFER_SIZE) {
            DBG("Memory buffer full, stop recording. Samples: %lu\n", (unsigned long)memory_write_pos);
            memory_data_length = memory_write_pos;
            memory_read_pos = 0;
            
            // 自动停止录制并开始播放
            g_loop_manager.state = LOOP_STATE_PLAYING;

        }
        
        // 调试信息（使用统计计数器）
        if (g_loop_stats.recording_sample_count % 100 == 0) {
           // DBG("Memory recording: %lu samples, pos: %lu\n", (unsigned long)g_loop_stats.recording_sample_count, (unsigned long)memory_write_pos);
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
        uint8_t flash_device_id = loop_get_flash_device_id();
        
        uint8_t write_result;
        
        // 使用优化的音频写入函数
        if (flash_device_id == DEV_NAND) {
            // NAND Flash使用页面对齐缓冲写入 - 暂时使用标准写入
            write_result = BG_flash_manager.PageProgram(g_loop_manager.sector_address, buffer, bytes_to_write, flash_device_id);
        } else {
            // NOR Flash直接写入
            write_result = BG_flash_manager.PageProgram(g_loop_manager.sector_address, buffer, bytes_to_write, flash_device_id);
        }
        
        // 检查写入是否成功
        if (write_result != FLASH_STATUS_OK) {
            // 写入失败，停止录制
            // DBG("Audio write failed, stopping recording\n");
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
        
        // 检查Flash存储空间
        if (g_loop_manager.sector_address >= BG_flash_manager.GetTotalByte(flash_device_id)) {
            DBG("Flash full, stop recording. Address: %lu, Total: %lu (%s)\n", 
                (unsigned long)g_loop_manager.sector_address, (unsigned long)BG_flash_manager.GetTotalByte(flash_device_id),
                g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
            
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
                g_loop_stats.playback_sample_count++;
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
        
        // 每播放50次打印一次调试信息，增加频率便于调试
        static uint32_t debug_counter = 0;
        debug_counter++;
//        if (debug_counter % 50 == 0) {
//            DBG("Playing: pos=%d/%d, read=%d, mix_samples=%d, nonzero=%d, avg_amp=%d, first=%d\n",
//                g_loop_manager.play_position, g_loop_manager.record_length, bytes_to_read,
//                samples_to_mix, non_zero_read,
//                non_zero_read > 0 ? read_amplitude_sum / non_zero_read : 0, ReadBuf[0]);
//        }
        
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
    
    DBG("Verifying flash data: check_length=%lu, record_length=%lu\n", 
        (unsigned long)check_length, (unsigned long)g_loop_manager.record_length);
    
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
    DBG("Flash verification: total_samples=%lu, non_zero=%lu, ratio=%.3f\n", 
        (unsigned long)total_samples, (unsigned long)non_zero_samples, non_zero_ratio);
    
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
        g_loop_stats.recording_sample_count++;
        
        if (memory_write_pos >= MEMORY_BUFFER_SIZE - 1) {
            DBG("Memory buffer full, stop recording. Samples: %lu\n", (unsigned long)memory_write_pos);
            memory_data_length = memory_write_pos;
            memory_read_pos = 0;
            g_loop_manager.state = LOOP_STATE_PLAYING;

        }
    } else {
        // Flash录制：检查当前录制段
        uint8_t recording_segment = MAX_SEGMENTS;  // 初始化为无效值

        // 查找正在录制的段
        uint8_t i;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
                recording_segment = i;
                break;
            }
        }
        
        if (recording_segment >= MAX_SEGMENTS) {
            return;  // 没有段在录制
        }
        
        uint32_t bytes_to_write = 192;  // 只存储192字节（48个uint32_t * 4字节）

        // 正常录制模式：在录制过程中检测是否需要淡出（避免突然停止的"哒"声）
        uint8_t write_buffer[192];
        uint8_t need_fadeout = 0;
        uint16_t fadeout_factor = 100;
        
        // 使用当前录制段的地址信息
        SegmentInfo_t* current_segment = &g_loop_manager.segments[recording_segment];
        
        // 🔧 智能淡出：检测按键状态或长度，在即将停止录制时淡出
        // 这里可以添加检测逻辑，比如检测按键状态或录制时长
        // 目前先保持简单，依赖外部调用停止时的淡出
        
        convertUint32ArrayToUint8Array(audio_data, write_buffer, length);
        uint32_t write_address = current_segment->start_address + 
                                current_segment->length_pages * g_loop_manager.page_size;
        uint8_t flash_device_id = loop_get_flash_device_id();
        
        uint8_t result = BG_flash_manager.PageProgram(write_address, write_buffer, bytes_to_write, flash_device_id);
        
        if (result == FLASH_STATUS_OK) {
            current_segment->length_pages++;  // 更新段长度
            g_loop_stats.recording_sample_count++;
            
            // 只在自动测试模式下进行长度限制
            if (g_loop_manager.auto_test_mode && g_loop_manager.auto_test_state == 0) {
                if (current_segment->length_pages >= 562) {  // 562页约3秒
                    DBG("Auto-test: Segment %d reached length limit, auto-stop\n", recording_segment);
                    loop_set_segment_stopped(recording_segment);
                    g_loop_manager.state = LOOP_STATE_PLAYING;
                }
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
                g_loop_stats.playback_sample_count++;
                break;
            }
        }
    } else {
        // Flash多段混音播放
        // 找到最长的正在播放的段作为播放循环的基准
        uint32_t max_length = 0;
        uint8_t i;
        uint8_t total_playing = 0;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            if (g_loop_manager.segments[i].is_active && 
                g_loop_manager.segments[i].state == SEGMENT_PLAYING) {
                total_playing++;
                if (g_loop_manager.segments[i].length_pages > max_length) {
                    max_length = g_loop_manager.segments[i].length_pages;
                }
            }
        }
        
        if (max_length == 0) {
            return;  // 没有正在播放的段
        }
        
        // 🔧 新逻辑：每个段使用独立的播放位置，不使用全局播放位置
        uint32_t effective_length = max_length;  // 使用完整长度
        
        // 计算实际要读取的样本数
        uint32_t samples_per_page = 192 / 4;  // 每页192字节 = 48个uint32样本
        uint32_t samples_to_read = (length < samples_per_page) ? length : samples_per_page;
        
        // 混音所有正在播放的段的数据
        uint32_t mixed_samples[48];  // 临时混音缓冲区
        memset(mixed_samples, 0, sizeof(mixed_samples));
        
        uint8_t playing_segment_count = 0;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            if (!g_loop_manager.segments[i].is_active || 
                g_loop_manager.segments[i].state != SEGMENT_PLAYING) {
                continue;
            }
            
            SegmentInfo_t* current_seg = &g_loop_manager.segments[i];
            
            // 🔧 关键修复：每个段使用自己的播放位置
            uint32_t segment_page = current_seg->play_position;
            uint32_t segment_effective_length = current_seg->length_pages;
            
            // 检查段播放位置是否需要循环
            if (segment_page >= segment_effective_length && segment_effective_length > 0) {
                segment_page = 0;  // 重置到段开头
                current_seg->play_position = 0;
            }
            
            uint32_t segment_address = current_seg->start_address +
                                     segment_page * g_loop_manager.page_size;

            // 读取段数据
            uint8_t flash_device_id = loop_get_flash_device_id();
            BG_flash_manager.ReadData(segment_address, buffer, 192, flash_device_id);
            uint32_t segment_data[48];
            convertUint8ArrayToUint32Array(buffer, segment_data, samples_to_read);

            // 🔧 额外数据验证：段开头特殊处理
            if (current_seg->play_position == 0) {
                // 段开头：检查前几个样本是否有效
                uint16_t head_check;
                for (head_check = 0; head_check < 8 && head_check < samples_to_read; head_check++) {
                    uint32_t sample = segment_data[head_check];
                    // 如果前几个样本看起来像垃圾数据，用静音替换
                    if (sample == 0xFFFFFFFF || sample == 0x80008000 ||
                        (sample & 0xFFFF) == 0xFFFF || ((sample >> 16) & 0xFFFF) == 0xFFFF) {
                        segment_data[head_check] = 0x00000000;
                    }
                }
            }

            // 检查并清理异常数据
            uint16_t data_check;
            for (data_check = 0; data_check < samples_to_read; data_check++) {
                int16_t seg_left = (int16_t)(segment_data[data_check] & 0xFFFF);
                int16_t seg_right = (int16_t)((segment_data[data_check] >> 16) & 0xFFFF);
                
                // 检查异常值
                if (seg_left < -32000 || seg_left > 32000 || seg_right < -32000 || seg_right > 32000) {
                    segment_data[data_check] = 0x00000000;  // 🔧 修复：使用真正的静音值
                }
            }

            // 混音到临时缓冲区（增大音量）
            uint16_t j;
            for (j = 0; j < samples_to_read; j++) {
                int16_t seg_left = (int16_t)(segment_data[j] & 0xFFFF);
                int16_t seg_right = (int16_t)((segment_data[j] >> 16) & 0xFFFF);
                
                // 🔧 段开头淡入处理：防止突然的音频跳跃
                if (current_seg->play_position == 0 && j < 16) {
                    // 在段开头的前16个样本做淡入
                    uint16_t fade_factor = (j * 100) / 16;  // 0-100%淡入
                    seg_left = (int16_t)((int32_t)seg_left * fade_factor / 100);
                    seg_right = (int16_t)((int32_t)seg_right * fade_factor / 100);
                }

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
                playing_segment_count++;
        }
        
        // 增大整体播放音量
        if (playing_segment_count > 0) {
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

        // 复制混音结果到输出，不再需要淡化处理（因为结尾已经用前面数据替换）
        uint16_t j;
        for (j = 0; j < samples_to_read && j < length; j++) {
            output_data[j] = mixed_samples[j];
        }
        
        // 🔧 关键修复：递增每个正在播放段的播放位置
        for (i = 0; i < MAX_SEGMENTS; i++) {
            if (g_loop_manager.segments[i].is_active && 
                g_loop_manager.segments[i].state == SEGMENT_PLAYING) {
                g_loop_manager.segments[i].play_position++;
            }
        }
    }
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
    
    // 计算新段的起始地址
    uint32_t start_address;
    if (new_segment == 0) {
        // 第一段从固定地址开始
        start_address = 0x40000;  // 避开Flash低地址
    } else {
        // 后续段根据前面段的大小来计算
        start_address = g_loop_manager.segments[new_segment - 1].start_address + 
                       g_loop_manager.segments[new_segment - 1].length_pages * g_loop_manager.page_size;
        
        // 确保地址是页对齐的
        if (start_address % g_loop_manager.page_size != 0) {
            start_address = ((start_address / g_loop_manager.page_size) + 1) * g_loop_manager.page_size;
        }
    }
    
    // 初始化新段
    g_loop_manager.segments[new_segment].start_address = start_address;
    g_loop_manager.segments[new_segment].length_pages = 0;
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
 * @brief 停止当前段录制
 */
void loop_stop_current_segment(void)
{
    // 查找正在录制的段
    uint8_t recording_segment = MAX_SEGMENTS;
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
            recording_segment = i;
            break;
        }
    }
    
    if (recording_segment >= MAX_SEGMENTS) {
        DBG("No segment currently recording\n");
        return;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[recording_segment];
    
    // 🔧 新方案：在录制结尾写入真正的静音数据，确保平滑结束
    // 添加18页真正的静音数据（100ms）来替换可能的噪声结尾
    uint8_t silence_pages_to_add = 18;  // 100ms的静音
    uint8_t silence_buffer[192];
    uint32_t silence_samples[48];
    uint16_t sil_idx;
    
    // 生成真正的静音值 (0x00000000)
    for (sil_idx = 0; sil_idx < 48; sil_idx++) {
        silence_samples[sil_idx] = 0x00000000;
    }
    
    convertUint32ArrayToUint8Array(silence_samples, silence_buffer, 48);
    
    // 写入静音页到当前段结尾
    uint8_t flash_device_id = loop_get_flash_device_id();
    uint8_t page_count;
    for (page_count = 0; page_count < silence_pages_to_add; page_count++) {
        uint32_t silence_address = segment->start_address + 
                                  (segment->length_pages + page_count) * g_loop_manager.page_size;
        uint8_t write_result = BG_flash_manager.PageProgram(silence_address, silence_buffer, 192, flash_device_id);
        if (write_result != FLASH_STATUS_OK) {
            DBG("Warning: Failed to write silence page %d\n", page_count);
        }
    }
    
    // 更新段长度（包含静音页）
    segment->length_pages += silence_pages_to_add;
    
    DBG("Stop segment %d: recorded %lu pages with end-replacement\n", 
        recording_segment, (unsigned long)segment->length_pages);
    
    if (segment->length_pages == 0) {
        // 如果没有录制任何数据，标记段为无效
        segment->is_active = 0;
        segment->state = SEGMENT_INACTIVE;
        DBG("Segment %d has no data, marked as inactive\n", recording_segment);
    } else {
        // 设置段为播放状态
        segment->state = SEGMENT_PLAYING;
        segment->play_position = 0;  // 重置播放位置
        
        DBG("Stopped segment %d: %lu pages, set to PLAYING state\n",
            recording_segment, (unsigned long)segment->length_pages);
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
        g_loop_manager.segments[i].state = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].play_position = 0;
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
            // 查找正在录制的段并停止（不依赖全局变量）
            uint8_t found_recording = 0;
            uint8_t i;
            for (i = 0; i < MAX_SEGMENTS; i++) {
                if (g_loop_manager.segments[i].state == SEGMENT_RECORDING && i == segment_index) {
                    found_recording = 1;
                    break;
                }
            }
            
            if (found_recording) {
                // 如果确实是正在录制的段，停止录制
                loop_stop_current_segment();
                DBG("Segment %d: RECORDING -> PLAYING (stopped recording)\n", segment_index);
            } else {
                // 状态不一致，直接设置为播放状态
                segment->state = SEGMENT_PLAYING;
                DBG("Segment %d: RECORDING -> PLAYING (state correction)\n", segment_index);
            }
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
    
    // 更新全局状态（兼容性）
    g_loop_manager.state = LOOP_STATE_RECORDING;
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
    
    // 如果段正在录制，需要先更新其长度信息
    if (segment->state == SEGMENT_RECORDING && segment_index == g_loop_manager.current_segment) {
        // 更新当前录制段的长度
        segment->length_pages = g_loop_manager.record_length;
        DBG("Updated recording segment %d length to %lu pages\n", segment_index, (unsigned long)segment->length_pages);
    }
    
    // 如果段还没有数据，不能播放
    if (segment->length_pages == 0) {
        DBG("Cannot play segment %d: no recorded data\n", segment_index);
        return;
    }
    
    segment->state = SEGMENT_PLAYING;
    segment->play_position = 0;  // 重置播放位置
    
    // 更新全局状态
    g_loop_manager.state = LOOP_STATE_PLAYING;
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
        segment->length_pages = g_loop_manager.record_length;
        segment->length_bytes = segment->length_pages * g_loop_manager.page_size;
        
        // 在录制结尾写入静音页，确保平滑结束
        uint8_t silence_buffer[192];
        uint32_t silence_samples[48];
        uint16_t sil_idx;
        
        // 生成正确的静音值
        for (sil_idx = 0; sil_idx < 48; sil_idx++) {
            silence_samples[sil_idx] = 0x00000000;
        }
        
        convertUint32ArrayToUint8Array(silence_samples, silence_buffer, 48);
        
        // 写入少量静音页
        uint8_t flash_device_id = loop_get_flash_device_id();
        uint32_t silence_address = segment->start_address + 
                                  segment->length_pages * g_loop_manager.page_size;
        BG_flash_manager.PageProgram(silence_address, silence_buffer, 192, flash_device_id);
        
        segment->length_pages++;  // 增加静音页

        DBG("Segment %d recording stopped: %lu pages\n", 
            segment_index, (unsigned long)segment->length_pages);
    }
    
    segment->state = SEGMENT_STOPPED;
    
    // 检查是否还有其他段在录制或播放
    uint8_t has_recording = 0, has_playing = 0;
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
            has_recording = 1;
        }
        if (g_loop_manager.segments[i].state == SEGMENT_PLAYING) {
            has_playing = 1;
        }
    }
    
    // 更新全局状态
    if (has_recording && has_playing) {
        g_loop_manager.state = LOOP_STATE_RECORDING_AND_PLAYING;
    } else if (has_recording) {
        g_loop_manager.state = LOOP_STATE_RECORDING;
    } else if (has_playing) {
        g_loop_manager.state = LOOP_STATE_PLAYING;
    } else {
        g_loop_manager.state = LOOP_STATE_IDLE;
    }
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
 * @brief AudioLooper接口：验证Flash数据
 */
static uint8_t AudioLooper_VerifyFlashData(uint32_t test_length) {
    return loop_verify_flash_data(test_length);
}

/**
 * @brief AudioLooper接口：定时器更新
 */
static void AudioLooper_TimerUpdate(void) {
    loop_timer_update();
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
    .VerifyFlashData = AudioLooper_VerifyFlashData,
    .TimerUpdate = AudioLooper_TimerUpdate
};
