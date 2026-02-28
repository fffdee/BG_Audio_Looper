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

#ifdef __cplusplus
}
#endif

#endif /* __SF2_PARSER_H__ */
