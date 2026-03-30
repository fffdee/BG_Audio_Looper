/**
 ******************************************************************************
 * @file    remind_sound.h
 * @brief   开机/事件提示音播放模块（基于 const 数组的 MP3 解码输出）
 *
 * 使用方式:
 *   1. 用 host_tool/mp3_to_c_array.py 将 MP3 文件转换为 C 数组
 *   2. 在 remind_sound.c 的 s_remind_table[] 中注册条目
 *   3. 调用 RemindSound_PlayByIndex(id) 或 RemindSound_PlayByName(name)
 ******************************************************************************
 */

#ifndef __REMIND_SOUND_H__
#define __REMIND_SOUND_H__

#include "typedefine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 提示音条目结构体 ---- */
typedef struct {
    uint8_t         id;         /* 唯一索引 (从 0 开始) */
    const char     *name;       /* 名称字符串，用于按名播放 */
    const uint8_t  *data;       /* MP3 数据指针 */
    uint32_t        size;       /* MP3 数据字节数 */
    uint8_t         vol_pct;    /* 播放音量百分比 (0-100)，相对于电位器当前音量 */
                                /* 100 = 跟随电位器; 50 = 电位器音量的一半 */
} RemindSoundItem_t;

/**
 * @brief  按索引播放提示音（阻塞，直到播完）
 * @param  id  s_remind_table 中的 id 字段
 * @return 0=成功, -1=索引越界/参数无效
 */
int RemindSound_PlayByIndex(uint8_t id);

/**
 * @brief  按名称播放提示音（阻塞，直到播完）
 * @param  name  对应 RemindSoundItem_t.name 字符串
 * @return 0=成功, -1=未找到
 */
int RemindSound_PlayByName(const char *name);

/**
 * @brief  直接播放原始 MP3 数据（底层接口）
 * @param  vol_pct  音量百分比 (0-100)，相对于电位器当前反馈音量
 */
void RemindSound_Play(const uint8_t *mp3_data, uint32_t mp3_size, uint8_t vol_pct);

/**
 * @brief  向串口打印当前提示音表
 */
void RemindSound_ListAll(void);

/**
 * @brief  返回提示音表中条目的总数
 */
int RemindSound_GetCount(void);

/**
 * @brief  提示音播放中标志（非零 = 正在播放）
 * @note   Effect Graph 的 DAC 输出节点检查此标志，
 *         播放期间停止向 DAC FIFO 写数据，避免冲突。
 */
extern volatile uint8_t g_remind_sound_active;

#ifdef __cplusplus
}
#endif

#endif /* __REMIND_SOUND_H__ */

