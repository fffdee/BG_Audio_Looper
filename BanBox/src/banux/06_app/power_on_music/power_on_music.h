/**
 ******************************************************************************
 * @file    power_on_music.h
 * @brief   开机音乐播放模块（直接 PCM 播放，无需解码器）
 *
 * 使用方式:
 *   1. 将 WAV 文件用 wav2array 工具转换为 raw PCM int16_t C 数组
 *   2. 放入 power_on.h（const int16_t power_on[]）
 *   3. 开机时自动调用 PowerOnMusic_Play() 播放
 *   4. Shell 命令行可用 `pwr_music -p` 手动测试
 *
 * 数据参数 (由 power_on.h 注释标定):
 *   - Sample Rate: 44100 Hz
 *   - Bit Depth:   16-bit
 *   - Channels:    2 (stereo, L/R 交错)
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
