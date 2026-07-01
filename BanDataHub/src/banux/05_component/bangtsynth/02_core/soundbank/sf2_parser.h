/**
 * @file sf2_parser.h
 * @brief SoundFont 2 (SF2) 格式解析器接口
 * 
 * 提供 SF2 格式音源的初始化、采样读取、音符控制等功能。
 */
#ifndef __SF2_PARSER_H__
#define __SF2_PARSER_H__

#include <stdint.h>
#include "err_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * SF2 解析器接口结构
 * ============================================ */
typedef struct {
    /**
     * 初始化 SF2 解析器
     * @param filename  (已废弃, 传 NULL) 从存储层读取
     * @return SUCCESS 或错误码
     */
    BG_ERR (*Init)(const char *filename);

    /**
     * 释放 SF2 解析器资源
     * @return SUCCESS 或错误码
     */
    BG_ERR (*DeInit)(void);

    /**
     * 读取 SF2 采样数据
     * @param data     输出缓冲区 (int16_t PCM)
     * @param note     MIDI 音符号
     * @param count    请求的采样帧数
     * @param program  MIDI 程序号
     * @return 1=有数据, 0=播放结束
     */
    uint8_t (*Callback)(short *data, uint32_t note, uint32_t count, uint8_t program);

} SF2_Parser;

/* ============================================
 * 全局实例
 * ============================================ */
/** SF2 解析器全局实例 (sf2_parser.c 中定义) */
extern SF2_Parser sf2_parser;

/* ============================================
 * 音符控制接口
 * ============================================ */

/**
 * SF2 音符开启
 * @param note      MIDI 音符号
 * @param velocity  力度值
 * @param program   MIDI 程序号
 */
void sf2_note_on(uint8_t note, uint8_t velocity, uint8_t program);

/**
 * SF2 音符关闭
 * @param note     MIDI 音符号
 * @param program  MIDI 程序号
 */
void sf2_note_off(uint8_t note, uint8_t program);

/**
 * 重置单个音符的播放状态 (兼容接口)
 * @param note     MIDI 音符号
 * @param program  MIDI 程序号
 */
void sf2_reset_note(uint8_t note, uint8_t program);

/**
 * 重置指定程序的所有音符播放状态
 * @param program  MIDI 程序号
 */
void sf2_reset_all_notes(uint8_t program);

/**
 * 读取所有活跃声部的混合音频
 * 遍历内部声部池, 为每个活跃声部读取采样并混合输出
 * @param out_buf  输出缓冲区 (int16_t PCM)
 * @param count    采样帧数 (建议 <= 48)
 * @return 活跃声部数量 (0 = 静音)
 */
uint8_t sf2_read_active_samples(short *out_buf, uint32_t count);

/* ============================================
 * MIDI CC / Pitch Bend 控制接口
 * ============================================ */

/**
 * 设置当前 MIDI 通道 (在 sf2_note_on 之前调用)
 * @param channel  MIDI 通道 (0-15)
 */
void sf2_set_current_channel(uint8_t channel);

/**
 * 设置通道弯音值 (Pitch Bend)
 * @param channel  MIDI 通道 (0-15)
 * @param value    14-bit 弯音值 (-8192..+8191, 0=中心)
 */
void sf2_pitch_bend(uint8_t channel, int16_t value);

/**
 * 处理通道 CC 消息
 * @param channel  MIDI 通道 (0-15)
 * @param cc_num   CC 编号
 * @param value    CC 值 (0-127)
 */
void sf2_channel_cc(uint8_t channel, uint8_t cc_num, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* __SF2_PARSER_H__ */
