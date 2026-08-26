/**
 * @file bgs_parser.h
 * @brief BGS 自有音源格式解析器接口
 * 
 * 提供 BGS 格式音源的初始化、采样读取、音符控制等功能。
 * BGS 格式支持多 Program、力度层(velocity layer)。
 */
#ifndef __BGS_PARSER_H__
#define __BGS_PARSER_H__

#include <stdint.h>
#include "err_handle.h"
#include "hardware_interfance.h"  /* → bgs_types.h: BG_ReadData / BGS_Data / BG_Reader */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * BGS 解析器接口函数
 * ============================================ */

/**
 * 初始化 BGS 解析器
 * 必须在 soundbank_manager 初始化存储层后调用。
 * @return SUCCESS 或错误码
 */
BG_ERR bgs_init(void);

/**
 * 释放 BGS 解析器资源
 * @return SUCCESS 或错误码
 */
BG_ERR bgs_deinit(void);

/**
 * 读取 BGS 采样数据 (预选模式)
 * 在 NoteOn 时已通过 bgs_select_sample_by_velocity 确定采样索引，
 * 此函数根据索引读取 PCM 数据。
 * @param data     输出缓冲区 (int16_t PCM)
 * @param note     MIDI 音符号 (0-127)
 * @param count    请求的采样帧数
 * @param program  MIDI 程序号
 * @return 1=有数据, 0=播放结束
 */
uint8_t bgs_read_callback(short *data, uint32_t note, uint32_t count, uint8_t program);

/**
 * 获取 BGS 内部数据指针
 * @return BGS_Data 指针 (实际指向 BG_reader.Data)
 */
BGS_Data* bgs_get_data(void);

/**
 * 根据音符和力度选择最佳采样索引
 * @param note      MIDI 音符号
 * @param velocity  力度值 (0-127)
 * @param program   MIDI 程序号
 * @return 采样索引, -1 表示未找到匹配
 */
int bgs_select_sample_by_velocity(uint8_t note, uint8_t velocity, uint8_t program);

/* ============================================
 * 音符控制接口
 * ============================================ */

/**
 * 音符开启 — 选择力度层并激活采样
 * @param note      MIDI 音符号
 * @param velocity  力度值
 * @param program   MIDI 程序号
 */
void bgs_note_on(uint8_t note, uint8_t velocity, uint8_t program);

/**
 * 音符关闭 — 重置采样播放位置
 * @param note     MIDI 音符号
 * @param program  MIDI 程序号
 */
void bgs_note_off(uint8_t note, uint8_t program);

/**
 * 关闭指定程序的所有音符
 * @param program  MIDI 程序号
 */
void bgs_all_note_off(uint8_t program);

#ifdef __cplusplus
}
#endif

#endif /* __BGS_PARSER_H__ */
