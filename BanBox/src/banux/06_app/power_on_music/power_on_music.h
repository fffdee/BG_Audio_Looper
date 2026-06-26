/**
 ******************************************************************************
 * @file    power_on_music.h
 * @brief   开机音乐播放模块（基于 const WAV 数组，复用 RemindSound 解码器）
 *
 * 使用方式:
 *   1. 将 WAV 文件用 host_tool/mp3_to_c_array.py 转换为 C 数组
 *   2. 替换 g_power_on_wav.c 中的数组数据
 *   3. 开机时自动调用 PowerOnMusic_Play() 播放
 *   4. Shell 命令行可用 `pwr_music -p` 手动测试
 ******************************************************************************
 */

#ifndef __POWER_ON_MUSIC_H__
#define __POWER_ON_MUSIC_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  播放开机音乐（阻塞，直到播完）
 * @note   底层复用 RemindSound_Play()，自动检测 WAV/MP3 格式
 *         - 播放期间会设置 g_remind_sound_active 标志，Effect Graph DAC 输出静默
 *         - 需要在 DAC/ADC 初始化完成后、InitAudioEffects 之前调用（堆空间约束）
 */
void PowerOnMusic_Play(void);

/**
 * @brief  注册开机音乐的 Shell 测试命令（pwr_music）
 */
void ShellCmdPowerOnMusic_Register(void);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_ON_MUSIC_H__ */
