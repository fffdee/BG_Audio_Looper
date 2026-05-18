/**
 * @file looper_wav_ble_export.c
 * @brief Looper 录音段通过 BLE 导出 WAV 功能实现
 *
 * 功能：
 * - 支持单段或多段混音导出
 * - 支持单声道/立体声转换
 * - 通过 BLE 逐包发送音频数据到 App
 * - App 端接收数据后生成 WAV 文件
 */

#include "product_def.h"
#include "looper_wav_ble_export.h"
#include "audio_looper.h"
#include "looper_storage.h"
#include "ble_protocol.h"
#include "sys_param.h"
#include "debug.h"
#include <string.h>
#include <stddef.h>

/* ============================================
 * WAV 文件头定义 (与 looper_wav_export.h 一致)
 * ============================================ */
typedef struct {
    /* RIFF Chunk */
    char     riff_id[4];        /* "RIFF" */
    uint32_t file_size;         /* 文件大小 - 8 */
    char     wave_id[4];        /* "WAVE" */
    
    /* fmt Chunk */
    char     fmt_id[4];         /* "fmt " */
    uint32_t fmt_size;          /* 16 */
    uint16_t audio_format;      /* 1 = PCM */
    uint16_t num_channels;      /* 1=mono, 2=stereo */
    uint32_t sample_rate;       /* 采样率 */
    uint32_t byte_rate;         /* sample_rate * channels * bits/8 */
    uint16_t block_align;       /* channels * bits/8 */
    uint16_t bits_per_sample;   /* 16 */
    
    /* data Chunk */
    char     data_id[4];        /* "data" */
    uint32_t data_size;         /* 音频数据字节数 */
} __attribute__((packed)) WAV_Header_t;

/* ============================================
 * 导出状态机
 * ============================================ */
typedef enum {
    WAV_BLE_STATE_IDLE         = 0,  /* 空闲 */
    WAV_BLE_STATE_EXPORTING    = 1,  /* 导出中 */
    WAV_BLE_STATE_COMPLETED    = 2,  /* 已完成 */
} WavBleState_t;

/* ============================================
 * 导出上下文
 * ============================================ */
typedef struct {
    WavBleState_t state;
    
    /* 导出配置 */
    uint8_t segment_mask;           /* bit0~3: 段掩码 */
    uint8_t output_channels;        /* 1=mono, 2=stereo */
    uint8_t  export_mono_mix;       /* 1=声道平衡(L=R=(L+R)/2) */
    uint16_t export_gain_pct;       /* 导出增益%: 100=原始, 400=+12dB */
    
    /* 导出统计 */
    uint32_t total_samples;         /* 总采样数（来自最长段或 seg0） */
    uint32_t total_data_bytes;      /* 总音频数据字节数 */
    uint32_t total_packets;         /* 总数据包数 */
    uint32_t current_packet_index;  /* 当前发送的包索引 */
    
    /* 运行时状态 */
    uint32_t current_offset_bytes;  /* 当前读取位置（字节） */
    WAV_Header_t wav_header;        /* WAV 头 */
    uint8_t start_sent;             /* 1=EXPORT_START 已发送 */
    uint8_t end_sent;               /* 1=EXPORT_END 已发送 */
    
    /* 读取缓冲 */
    uint8_t read_buffer[WAV_BLE_DATA_PER_PACKET * 2];  /* 临时读缓冲 */
} WavBleExportCtx_t;

static WavBleExportCtx_t g_export_ctx;

/* ============================================
 * 内部辅助函数
 * ============================================ */

/**
 * @brief 创建 WAV 文件头
 */
static void create_wav_header(WAV_Header_t *header, uint32_t data_size, uint8_t channels)
{
    uint32_t byte_rate;
    uint16_t block_align;
    
    /* 计算参数 */
    block_align = (uint16_t)(channels * 16 / 8);
    byte_rate = LOOPER_SAMPLE_RATE * block_align;
    
    /* RIFF Chunk */
    memcpy(header->riff_id, "RIFF", 4);
    header->file_size = data_size + sizeof(WAV_Header_t) - 8;
    memcpy(header->wave_id, "WAVE", 4);
    
    /* fmt Chunk */
    memcpy(header->fmt_id, "fmt ", 4);
    header->fmt_size = 16;
    header->audio_format = 1;  /* PCM */
    header->num_channels = channels;
    header->sample_rate = LOOPER_SAMPLE_RATE;
    header->byte_rate = byte_rate;
    header->block_align = block_align;
    header->bits_per_sample = 16;
    
    /* data Chunk */
    memcpy(header->data_id, "data", 4);
    header->data_size = data_size;
}

/**
 * @brief 检查导出段有效性并获取总长度
 * @return 总采样数，或 0 表示错误
 */
static uint32_t check_export_segments(uint8_t segment_mask, uint8_t *out_rec_source)
{
    uint32_t total_samples = 0;
    uint8_t first_valid_idx = 0xFF;
    uint8_t i;
    SegmentInfo_t *seg;
    
    if (segment_mask == 0) {
        DBG("[WAV_BLE] Error: segment_mask is 0\n");
        return 0;
    }
    
    /* 检查所有选中的段 */
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if ((segment_mask & (1 << i)) == 0) {
            continue;  /* 此段未选中 */
        }
        
        seg = &g_loop_manager.segments[i];
        
        if (seg->state == SEGMENT_INACTIVE || seg->length_bytes == 0) {
            DBG("[WAV_BLE] Error: segment %d is empty\n", i);
            return 0;
        }
        
        if (first_valid_idx == 0xFF) {
            first_valid_idx = i;
            total_samples = seg->length_bytes / (LOOP_REC_SRC_IS_MONO(seg->rec_source) ? 2 : 4);
            *out_rec_source = seg->rec_source;
        } else {
            /* 允许等长录制因 DSP 帧边界产生的微小偏差，取最短段截断导出 */
            uint32_t seg_samples = seg->length_bytes / (LOOP_REC_SRC_IS_MONO(seg->rec_source) ? 2 : 4);
            if (seg_samples != total_samples) {
                DBG("[WAV_BLE] Warning: segment length mismatch (base=%lu, seg%d=%lu), using min\n",
                    (unsigned long)total_samples, i, (unsigned long)seg_samples);
                if (seg_samples < total_samples) {
                    total_samples = seg_samples;
                }
            }
        }
    }
    
    return total_samples;
}

/**
 * @brief 读取指定段的原始音频数据
 * @param segment_idx 段索引
 * @param offset_bytes 相对于段起始的偏移（字节）
 * @param buf 输出缓冲
 * @param len 要读取的字节数
 * @return 实际读取的字节数
 */
static uint32_t read_segment_data(uint8_t segment_idx, uint32_t offset_bytes, 
                                   uint8_t *buf, uint32_t len)
{
    SegmentInfo_t *seg = &g_loop_manager.segments[segment_idx];
    uint32_t read_addr;
    LooperStorageStatus_t ret;
    
    if (offset_bytes >= seg->length_bytes) {
        return 0;
    }
    
    /* 计算实际地址 */
    read_addr = seg->start_address + offset_bytes;
    len = (offset_bytes + len > seg->length_bytes) ? 
          (seg->length_bytes - offset_bytes) : len;
    
    /* 通过存储抽象层读取 */
    ret = LooperStorage_Read(&g_looper_storage, read_addr, buf, len);
    
    return (ret == LOOPER_STORAGE_OK) ? len : 0;
}

/**
 * @brief 将单个立体声采样（4字节）转换为单声道（取左声道）
 * @param stereo_sample 立体声采样 (int16_t L, int16_t R)
 * @return 单声道采样 (左声道)
 */
static int16_t stereo_to_mono_left(uint32_t stereo_sample)
{
    /* stereo_sample = [L(16bit) | R(16bit)]
     * L = (int16_t)(stereo_sample & 0xFFFF)
     * 取低 16 位作为左声道 */
    return (int16_t)(stereo_sample & 0xFFFF);
}

/**
 * @brief 将单个单声道采样（2字节）扩展为立体声（4字节）
 * @param mono_sample 单声道采样 (int16_t)
 * @return 立体声采样 [mono | mono]
 */
static uint32_t mono_to_stereo(int16_t mono_sample)
{
    /* 将相同的样本复制到左右声道 */
    return ((uint32_t)mono_sample & 0xFFFF) | (((uint32_t)mono_sample & 0xFFFF) << 16);
}

/**
 * @brief 转换并混音多个段的数据
 *
 * 从各个选中的段读取数据，根据原始格式和目标格式进行转换：
 * - 立体声 → 单声道：取左声道
 * - 单声道 → 立体声：复制到两个声道
 * - 混音：将各段相加（定点混音）
 *
 * @param offset_bytes 相对于段起始的偏移（字节）
 * @param out_buf 输出缓冲
 * @param requested_bytes 请求的输出字节数
 * @param segments_mask 段掩码
 * @param output_channels 输出声道数
 * @return 实际转换的输出字节数
 */
static uint32_t convert_and_mix_segments(uint32_t offset_bytes, uint8_t *out_buf,
                                         uint32_t requested_bytes, uint8_t segments_mask,
                                         uint8_t output_channels)
{
    uint32_t out_idx = 0;
    uint32_t read_offset = offset_bytes;
    uint8_t i;
    uint32_t samples_to_read;
    uint8_t segment_count = 0;
    
    /* 计算要读取的采样数 */
    if (output_channels == 1) {
        samples_to_read = requested_bytes / 2;  /* 单声道：输出 2 字节/采样 */
    } else {
        samples_to_read = requested_bytes / 4;  /* 立体声：输出 4 字节/采样 */
    }
    
    if (samples_to_read == 0) {
        return 0;
    }
    
    /* 初始化输出缓冲为 0（用于多段混音累加） */
    memset(out_buf, 0, requested_bytes);
    
    /* 遍历所有选中的段 */
    for (i = 0; i < MAX_SEGMENTS; i++) {
        SegmentInfo_t *seg;
        uint8_t seg_is_mono;
        uint32_t seg_sample_count;
        uint32_t read_len;
        uint32_t sample_idx;
        int16_t mono_sample;
        uint32_t stereo_sample;
        uint32_t seg_byte_offset;
        uint32_t actual_read;
        uint32_t sample_count;
        
        if ((segments_mask & (1 << i)) == 0) {
            continue;
        }
        
        seg = &g_loop_manager.segments[i];
        seg_is_mono = LOOP_REC_SRC_IS_MONO(seg->rec_source);
        
        segment_count++;
        
        /* 计算此段的采样数 */
        seg_sample_count = seg->length_bytes / (seg_is_mono ? 2 : 4);

        /* 根据输出字节偏移还原已消费的采样数，再转换为源格式字节偏移。
         * read_offset 以输出字节计量（出口格式：output_channels * 2 字节/采样），
         * 源格式每采样可能是 2 字节（mono）或 4 字节（stereo），两者格式不同时不能
         * 直接把输出偏移当源偏移用，否则会导致读取位置 2x 偏差（双倍速 bug）。 */
        seg_byte_offset = (read_offset / (uint32_t)(output_channels * 2))
                          * (uint32_t)(seg_is_mono ? 2 : 4);

        if (seg_byte_offset >= seg->length_bytes) {
            continue;  /* 已读超过此段 */
        }

        /* 计算此段的读取长度 */
        read_len = samples_to_read * (seg_is_mono ? 2 : 4);
        
        if (seg_byte_offset + read_len > seg->length_bytes) {
            read_len = seg->length_bytes - seg_byte_offset;
        }
        
        /* 从此段读取数据 */
        actual_read = read_segment_data(i, seg_byte_offset, 
                                         g_export_ctx.read_buffer, read_len);
        
        if (actual_read == 0) {
            DBG("[WAV_BLE] Error: failed to read segment %d\n", i);
            continue;
        }
        
        /* 转换数据 */
        sample_count = actual_read / (seg_is_mono ? 2 : 4);
        out_idx = 0;
        
        if (output_channels == 1) {
            /* 输出单声道 */
            if (seg_is_mono) {
                if (segment_count == 1) {
                    memcpy(out_buf, g_export_ctx.read_buffer, sample_count * 2);
                } else {
                    for (sample_idx = 0; sample_idx < sample_count; sample_idx++) {
                        int16_t existing = *(int16_t *)(out_buf + sample_idx * 2);
                        int16_t new_val = *(int16_t *)(g_export_ctx.read_buffer + sample_idx * 2);
                        int32_t sum = (int32_t)existing + (int32_t)new_val;
                        if (sum > 32767) sum = 32767;
                        if (sum < -32768) sum = -32768;
                        *(int16_t *)(out_buf + sample_idx * 2) = (int16_t)sum;
                    }
                }
            } else {
                /* 立体声转单声道（取左声道） */
                for (sample_idx = 0; sample_idx < sample_count; sample_idx++) {
                    stereo_sample = *(uint32_t *)(g_export_ctx.read_buffer + sample_idx * 4);
                    mono_sample = stereo_to_mono_left(stereo_sample);
                    
                    if (segment_count == 1) {
                        *(int16_t *)(out_buf + sample_idx * 2) = mono_sample;
                    } else {
                        int16_t existing = *(int16_t *)(out_buf + sample_idx * 2);
                        int32_t sum = (int32_t)existing + (int32_t)mono_sample;
                        if (sum > 32767) sum = 32767;
                        if (sum < -32768) sum = -32768;
                        *(int16_t *)(out_buf + sample_idx * 2) = (int16_t)sum;
                    }
                }
            }
        } else {
            /* 输出立体声 */
            if (seg_is_mono) {
                /* 单声道转立体声 */
                for (sample_idx = 0; sample_idx < sample_count; sample_idx++) {
                    mono_sample = *(int16_t *)(g_export_ctx.read_buffer + sample_idx * 2);
                    stereo_sample = mono_to_stereo(mono_sample);
                    
                    if (segment_count == 1) {
                        *(uint32_t *)(out_buf + sample_idx * 4) = stereo_sample;
                    } else {
                        uint32_t existing = *(uint32_t *)(out_buf + sample_idx * 4);
                        int32_t l = (int16_t)(existing & 0xFFFF) + (int16_t)(stereo_sample & 0xFFFF);
                        int32_t r = (int16_t)((existing >> 16) & 0xFFFF) + 
                                    (int16_t)((stereo_sample >> 16) & 0xFFFF);
                        if (l > 32767) l = 32767;
                        if (l < -32768) l = -32768;
                        if (r > 32767) r = 32767;
                        if (r < -32768) r = -32768;
                        *(uint32_t *)(out_buf + sample_idx * 4) = 
                            ((uint32_t)(r & 0xFFFF) << 16) | (uint32_t)(l & 0xFFFF);
                    }
                }
            } else {
                if (segment_count == 1) {
                    memcpy(out_buf, g_export_ctx.read_buffer, sample_count * 4);
                } else {
                    for (sample_idx = 0; sample_idx < sample_count; sample_idx++) {
                        uint32_t existing = *(uint32_t *)(out_buf + sample_idx * 4);
                        uint32_t new_val = *(uint32_t *)(g_export_ctx.read_buffer + sample_idx * 4);
                        int32_t l = (int16_t)(existing & 0xFFFF) + (int16_t)(new_val & 0xFFFF);
                        int32_t r = (int16_t)((existing >> 16) & 0xFFFF) + 
                                    (int16_t)((new_val >> 16) & 0xFFFF);
                        if (l > 32767) l = 32767;
                        if (l < -32768) l = -32768;
                        if (r > 32767) r = 32767;
                        if (r < -32768) r = -32768;
                        *(uint32_t *)(out_buf + sample_idx * 4) = 
                            ((uint32_t)(r & 0xFFFF) << 16) | (uint32_t)(l & 0xFFFF);
                    }
                }
            }
        }
    }
    
    /* 计算实际输出字节数 */
    out_idx = samples_to_read * (output_channels == 1 ? 2 : 4);
    if (out_idx > requested_bytes) {
        out_idx = requested_bytes;
    }

    /* 后处理：声道平衡 + 增益 */
    if (out_idx > 0) {
        uint16_t gain_pct = g_export_ctx.export_gain_pct;
        uint8_t  mono_mix = g_export_ctx.export_mono_mix;
        uint32_t s;

        if (output_channels == 2) {
            /* 立体声：先声道平衡，再增益 */
            for (s = 0; s < out_idx / 4; s++) {
                uint32_t *p = (uint32_t *)(out_buf + s * 4);
                int32_t l = (int16_t)(*p & 0xFFFF);
                int32_t r = (int16_t)((*p >> 16) & 0xFFFF);
                int32_t avg;

                if (mono_mix) {
                    avg = (l + r) / 2;
                    l = avg;
                    r = avg;
                }

                if (gain_pct != 100) {
                    l = l * gain_pct / 100;
                    r = r * gain_pct / 100;
                    if (l >  32767) l =  32767;
                    if (l < -32768) l = -32768;
                    if (r >  32767) r =  32767;
                    if (r < -32768) r = -32768;
                }

                *p = ((uint32_t)((uint16_t)(int16_t)r) << 16) | (uint32_t)((uint16_t)(int16_t)l);
            }
        } else if (gain_pct != 100) {
            /* 单声道增益 */
            for (s = 0; s < out_idx / 2; s++) {
                int16_t *p = (int16_t *)(out_buf + s * 2);
                int32_t v = (int32_t)(*p) * gain_pct / 100;
                if (v >  32767) v =  32767;
                if (v < -32768) v = -32768;
                *p = (int16_t)v;
            }
        }
    }

    return out_idx;
}

/* ============================================
 * 公共接口实现
 * ============================================ */

void LooperWavBle_HandleCommand(const uint8_t *payload, uint8_t len)
{
    uint8_t subcmd;
    uint8_t segment_mask;
    uint8_t output_channels;
    uint8_t rec_source = 0;
    uint32_t total_samples;
    uint32_t total_data_bytes;
    uint32_t total_packets;
    uint8_t response_buf[206];
    uint8_t response_len;
    
    if (len < 1) {
        return;
    }
    
    subcmd = payload[0];
    
    switch (subcmd) {
        case WAV_BLE_SUBCMD_EXPORT_REQ:
            if (len < 3) {
                return;
            }
            
            if (g_export_ctx.state != WAV_BLE_STATE_IDLE) {
                DBG("[WAV_BLE] Error: already exporting or completed\n");
                return;
            }
            
            segment_mask = payload[1];
            output_channels = payload[2];
            
            if (output_channels != 1 && output_channels != 2) {
                DBG("[WAV_BLE] Error: invalid output_channels %d\n", output_channels);
                return;
            }
            
            /* 检查导出段有效性 */
            total_samples = check_export_segments(segment_mask, &rec_source);
            if (total_samples == 0) {
                DBG("[WAV_BLE] Error: invalid segments\n");
                return;
            }
            
            /* 计算总字节数 */
            total_data_bytes = total_samples * output_channels * 2;
            total_packets = (total_data_bytes + WAV_BLE_DATA_PER_PACKET - 1) / WAV_BLE_DATA_PER_PACKET;
            
            DBG("[WAV_BLE] Export request: segments=0x%02X, channels=%d, samples=%lu, bytes=%lu, packets=%lu\n",
                segment_mask, output_channels, (unsigned long)total_samples, 
                (unsigned long)total_data_bytes, (unsigned long)total_packets);
            
            /* 初始化导出上下文 */
            g_export_ctx.state = WAV_BLE_STATE_EXPORTING;
            g_export_ctx.segment_mask = segment_mask;
            g_export_ctx.output_channels = output_channels;
            g_export_ctx.export_mono_mix = SYSPARAM_LOOPER()->export_mono_mix;
            g_export_ctx.export_gain_pct = SYSPARAM_LOOPER()->export_gain_pct;
            if (g_export_ctx.export_gain_pct == 0) {
                g_export_ctx.export_gain_pct = 100;  /* 防御性处理：0 视为 100% */
            }
            g_export_ctx.total_samples = total_samples;
            g_export_ctx.total_data_bytes = total_data_bytes;
            g_export_ctx.total_packets = total_packets;
            g_export_ctx.current_packet_index = 0;
            g_export_ctx.current_offset_bytes = 0;
            g_export_ctx.start_sent = 0;
            g_export_ctx.end_sent = 0;
            
            /* 创建 WAV 头，ProcessTick 将发送 EXPORT_START */
            create_wav_header(&g_export_ctx.wav_header, total_data_bytes, output_channels);
            break;
            
        case WAV_BLE_SUBCMD_CANCEL:
            if (g_export_ctx.state == WAV_BLE_STATE_EXPORTING) {
                g_export_ctx.state = WAV_BLE_STATE_IDLE;
                DBG("[WAV_BLE] Export cancelled\n");
            }
            break;
            
        default:
            DBG("[WAV_BLE] Unknown subcmd: 0x%02X\n", subcmd);
            break;
    }
}

void LooperWavBle_ProcessTick(void)
{
    uint32_t data_len;
    uint32_t out_len;
    uint8_t response_buf[206];
    uint8_t response_len;
    
    if (g_export_ctx.state != WAV_BLE_STATE_EXPORTING) {
        return;
    }
    
    /* 1. 先发送 EXPORT_START（含 WAV 头），重试直到成功 */
    if (g_export_ctx.start_sent == 0) {
        response_buf[0] = WAV_BLE_SUBCMD_EXPORT_START;
        *(uint32_t *)&response_buf[1] = g_export_ctx.total_packets;
        *(uint32_t *)&response_buf[5] = g_export_ctx.total_data_bytes;
        memcpy(&response_buf[9], &g_export_ctx.wav_header, sizeof(WAV_Header_t));
        response_len = 1 + 4 + 4 + sizeof(WAV_Header_t);  /* 53 */
        if (BleProto_SendOnce(BLE_CMD_WAV_EXPORT, response_buf, response_len) == 0) {
            g_export_ctx.start_sent = 1;
            DBG("[WAV_BLE] EXPORT_START sent\n");
        }
        return;  /* 下一 tick 再发数据 */
    }
    
    /* 2. 逐帧发送数据：每 tick 发一包（send_frame 用 static buf，BLE_Send 异步不复制，
     * 不能 burst 连发，否则后一包会覆盖 static buf 导致前包数据损坏）*/
    {
        /* 所有包发完则发 END */
        if (g_export_ctx.current_packet_index >= g_export_ctx.total_packets) {
            if (g_export_ctx.end_sent == 0) {
                response_buf[0] = WAV_BLE_SUBCMD_EXPORT_END;
                response_buf[1] = WAV_BLE_RESULT_OK;
                if (BleProto_SendOnce(BLE_CMD_WAV_EXPORT, response_buf, 2) == 0) {
                    g_export_ctx.end_sent = 1;
                    DBG("[WAV_BLE] EXPORT_END sent\n");
                }
            } else {
                g_export_ctx.state = WAV_BLE_STATE_IDLE;
                DBG("[WAV_BLE] Export completed\n");
            }
            return;
        }

        /* 计算本包的数据大小 */
        data_len = g_export_ctx.total_data_bytes - g_export_ctx.current_offset_bytes;
        if (data_len > WAV_BLE_DATA_PER_PACKET) {
            data_len = WAV_BLE_DATA_PER_PACKET;
        }
        if (data_len == 0) return;

        /* 读取、转换并混音数据 */
        out_len = convert_and_mix_segments(g_export_ctx.current_offset_bytes,
                                            &response_buf[5], data_len,
                                            g_export_ctx.segment_mask,
                                            g_export_ctx.output_channels);
        if (out_len == 0) {
            DBG("[WAV_BLE] Error: failed to read data at offset %lu\n",
                (unsigned long)g_export_ctx.current_offset_bytes);
            g_export_ctx.state = WAV_BLE_STATE_IDLE;
            return;
        }

        /* 构造数据包响应 */
        response_buf[0] = WAV_BLE_SUBCMD_DATA_PACKET;
        *(uint32_t *)&response_buf[1] = g_export_ctx.current_packet_index;
        response_len = 1 + 4 + out_len;

        /* 用 SendOnce 发批量数据：不等 ACK，BLE 未就绪时只需下次 tick 重试。
         * 仅在成功发送后才推进包序号，确保数据不丢失。*/
        if (BleProto_SendOnce(BLE_CMD_WAV_EXPORT, response_buf, response_len) == 0) {
            g_export_ctx.current_packet_index++;
            g_export_ctx.current_offset_bytes += out_len;
        }
        /* 失败（BLE 未就绪）时不推进，下一 tick 自动重试 */
    }
}

bool LooperWavBle_IsBusy(void)
{
    return (g_export_ctx.state == WAV_BLE_STATE_EXPORTING);
}
