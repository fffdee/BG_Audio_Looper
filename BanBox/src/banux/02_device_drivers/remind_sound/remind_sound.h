/**
 ******************************************************************************
 * @file    remind_sound.h
 * @brief   提示音播放模块（非阻塞，通过 Effect Graph 音频系统输出）
 *
 * 架构说明:
 *   - 提示音数据（WAV/MP3 const 数组）注册在 s_remind_table[] 中
 *   - RemindSound_Start() 设置播放状态，立即返回（非阻塞）
 *   - RemindSound_GenerateAudio() 由 Effect Graph 的 REMIND 源节点调用，
 *     每次解码一小帧 PCM 数据，混入音频流输出到 DAC
 *   - 播放完成后自动停止，也可以手动 RemindSound_Stop()
 *
 * 新增提示音步骤：
 *   1. 用 mp3_to_c_array.py 生成 .h / .c
 *   2. 在 remind_sound.c 的 #include 区域引入新 .h
 *   3. 在 s_remind_table[] 末尾追加一条 { id, "name", data, size }
 ******************************************************************************
 */

#ifndef __REMIND_SOUND_H__
#define __REMIND_SOUND_H__

#include "typedefine.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 提示音条目结构体 ---- */
typedef struct {
    uint8_t         id;         /* 唯一索引 (从 0 开始) */
    const char     *name;       /* 名称字符串，用于按名播放 */
    const uint8_t  *data;       /* 音频数据指针 (WAV/MP3) */
    uint32_t        size;       /* 音频数据字节数 */
    uint8_t         vol_pct;    /* 播放音量百分比 (0-100) */
} RemindSoundItem_t;

/**
 * @brief  按名称启动提示音播放（非阻塞）
 * @param  name  对应 RemindSoundItem_t.name 字符串
 * @return 0=成功启动, -1=未找到/正在播放/参数无效
 */
int RemindSound_Start(const char *name);

/**
 * @brief  按索引启动提示音播放（非阻塞）
 * @param  id  s_remind_table 中的 id 字段
 * @return 0=成功启动, -1=索引越界/正在播放
 */
int RemindSound_StartById(uint8_t id);

/**
 * @brief  停止当前提示音播放
 */
void RemindSound_Stop(void);

/**
 * @brief  查询是否正在播放提示音
 * @return 1=正在播放, 0=未播放
 */
int RemindSound_IsPlaying(void);

/**
 * @brief  生成提示音音频数据（由 Effect Graph REMIND 源节点调用）
 * @param  out_buf   输出缓冲区，uint32_t 格式 [L|R] packed
 * @param  max_len   最大样本数（每个样本为一个 uint32_t 立体声帧）
 * @return 实际生成的样本数
 */
uint16_t RemindSound_GenerateAudio(uint32_t *out_buf, uint16_t max_len);

/**
 * @brief  查询可用数据量（Effect Graph REMIND 源节点可用数据回调）
 * @return 可用样本数，0=无数据/未播放
 */
uint16_t RemindSound_GetAvailableData(void);

/**
 * @brief  向串口打印当前提示音表
 */
void RemindSound_ListAll(void);

/**
 * @brief  返回提示音表中条目的总数
 */
int RemindSound_GetCount(void);

/**
 * @brief  初始化提示音模块（必须在 Effect Graph 初始化之前调用）
 */
void RemindSound_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __REMIND_SOUND_H__ */
