/**
 * @file bg_synth.h
 * @brief BanGTsynth 最小公开 API（移植只需实现 HAL，然后调这组函数）
 *
 * 使用顺序:
 *   1. 平台把存储/PSRAM/FS 等硬件准备好
 *   2. bg_synth_init()
 *   3. 音频回调里 bg_synth_render()
 *   4. 任意任务 bg_synth_note_on/off 或 bg_synth_midi()
 */
#ifndef BG_SYNTH_H
#define BG_SYNTH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化合成器：加载音源 + 节点（队列/MIDI）
 * @return 0=成功, <0=失败（音源失败时仍可能完成节点初始化，render 输出静音）
 */
int bg_synth_init(void);

/** 反初始化 */
void bg_synth_deinit(void);

/**
 * 在音频线程/ISR 中填充 PCM（int16，单声道交错由 BG_CHANNELS 决定，当前为单声道）
 * @param out    输出缓冲
 * @param frames 帧数
 */
void bg_synth_render(int16_t *out, uint32_t frames);

void bg_synth_note_on(uint8_t channel, uint8_t note, uint8_t velocity, uint8_t program);
void bg_synth_note_off(uint8_t channel, uint8_t note, uint8_t program);
void bg_synth_midi(const uint8_t *data, uint8_t len);

void bg_synth_set_volume(uint8_t volume_0_100);
uint8_t bg_synth_get_volume(void);

uint8_t bg_synth_is_ready(void);

/** 从 TF 指定 .sf2 重新加载到 PSRAM 并解析（阻塞，勿在 ISR 调） */
int bg_synth_load_file(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* BG_SYNTH_H */
