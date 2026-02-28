/**
 * @file    bangtsynth_node.c
 * @brief   BanGTsynth 合成器 Effect Graph 源节点桥接层实现
 * @author  BanGO
 * @date    2026-02-27
 *
 * 将 BanGTsynth MIDI 合成器封装为 Effect Graph 的 SOURCE 节点。
 * 合成器在每次回调中:
 *   1. 遍历所有 MIDI 通道的活动音符
 *   2. 从 soundbank 读取对应采样
 *   3. 应用力度和包络控制
 *   4. 混合所有复音
 *   5. 输出 uint32_t 立体声格式 (高16位=R, 低16位=L)
 *
 * 宏控制: BANGTSYNTH_EN
 */

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "bangtsynth_node.h"
#include "midi_controller.h"
#include "midi_soundbank_bridge.h"
#include "soundbank_manager.h"
#include "bg_audio_processor.h"
#include "bg_config.h"
#include "debug.h"
#include <string.h>

/*============================================================================
 * 内部状态
 *===========================================================================*/
static uint8_t g_synth_initialized = 0;

/* 合成器每次处理的帧长 */
#define SYNTH_FRAME_SIZE    48

/* 最大同时复音数 (限制在嵌入式平台可接受范围) */
#define SYNTH_MAX_VOICES    8

/* 中间缓冲区 */
static int16_t g_synth_mix_buf[SYNTH_FRAME_SIZE];       /* 最终混音缓冲区 */
static int16_t g_synth_voice_buf[SYNTH_FRAME_SIZE];     /* 单个voice临时缓冲区 */
static int16_t g_synth_processed_buf[SYNTH_FRAME_SIZE]; /* 效果处理后缓冲区 */

/*============================================================================
 * 初始化 / 反初始化
 *===========================================================================*/

int BanGTsynth_Node_Init(void)
{
    if (g_synth_initialized) {
        DBG("[Synth] Already initialized\n");
        return 0;
    }

    DBG("[Synth] Initializing BanGTsynth node...\n");

    /* 初始化 MIDI 控制器 (包含音频处理流水线) */
    BG_MIDI_controller.Init();

    /* 注意: soundbank 需要在外部单独加载 (通过 soundbank_manager.Init)
     * 因为音源数据存储在外部 Flash, 需要先完成 Flash 初始化 */

    g_synth_initialized = 1;
    DBG("[Synth] BanGTsynth node initialized OK\n");

    return 0;
}

void BanGTsynth_Node_DeInit(void)
{
    if (!g_synth_initialized) {
        return;
    }

    /* 释放音源资源 */
    midi_soundbank_bridge.DeInit();

    g_synth_initialized = 0;
    DBG("[Synth] BanGTsynth node deinitialized\n");
}

/*============================================================================
 * Effect Graph 源节点回调
 *===========================================================================*/

uint16_t BanGTsynth_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
    uint16_t i;
    uint16_t process_len;
    uint8_t polyphony_count = 0;
    uint8_t has_audio = 0;
    uint8_t ch, note;
    uint8_t result;
    uint8_t velocity;
    int32_t vel_scale;
    uint16_t sample_u16;

    (void)node;

    /* 限制帧长 */
    process_len = (max_len > SYNTH_FRAME_SIZE) ? SYNTH_FRAME_SIZE : max_len;

    /* 清零输出 */
    for (i = 0; i < max_len; i++) {
        out_buf[i] = 0;
    }

    if (!g_synth_initialized) {
        return max_len;
    }

    /* 清零混音缓冲区 */
    memset(g_synth_mix_buf, 0, sizeof(int16_t) * process_len);

    /* 遍历所有 MIDI 通道, 收集活动音符的音频数据 */
    for (ch = 0; ch < 16; ch++) {
        if (BG_MIDI_data.BG_channel_info[ch].NoteOn_count == 0) {
            continue;
        }

        for (note = 0; note < 128; note++) {
            if (BG_MIDI_data.BG_channel_info[ch].Note_Map[note] == 0) {
                continue;
            }

            if (polyphony_count >= SYNTH_MAX_VOICES) {
                break;  /* 达到最大复音数限制 */
            }

            /* 从 soundbank 读取采样数据 */
            memset(g_synth_voice_buf, 0, sizeof(int16_t) * process_len);
            result = soundbank_manager.ReadSamples(
                g_synth_voice_buf, note, process_len,
                BG_MIDI_data.BG_channel_info[ch].program_index
            );

            if (!result) {
                /* 采样播放完成, 关闭音符 */
                BG_MIDI_data.BG_channel_info[ch].Note_Map[note] = 0;
                if (BG_MIDI_data.BG_channel_info[ch].NoteOn_count > 0) {
                    BG_MIDI_data.BG_channel_info[ch].NoteOn_count--;
                }
                continue;
            }

            /* 应用力度后混入总缓冲区 */
            velocity = BG_MIDI_data.BG_channel_info[ch].Note_Map[note];
            vel_scale = (int32_t)velocity;  /* 0-127 */

            for (i = 0; i < process_len; i++) {
                int32_t sample = ((int32_t)g_synth_voice_buf[i] * vel_scale) >> 7;
                /* 累加混音 (用 int32_t 防溢出) */
                int32_t mixed = (int32_t)g_synth_mix_buf[i] + sample;
                /* 软限幅 */
                if (mixed > 32767) mixed = 32767;
                if (mixed < -32768) mixed = -32768;
                g_synth_mix_buf[i] = (int16_t)mixed;
            }

            polyphony_count++;
            has_audio = 1;
        }

        if (polyphony_count >= SYNTH_MAX_VOICES) {
            break;
        }
    }

    /* 如果有音频数据, 经过音频处理流水线 (EQ + DRC) */
    if (has_audio) {
        BG_AudioProcessor.Process(g_synth_mix_buf, g_synth_processed_buf, process_len);

        /* 转换为 uint32_t 立体声格式: 高16位=R, 低16位=L (mono复制到双声道) */
        for (i = 0; i < process_len; i++) {
            sample_u16 = (uint16_t)g_synth_processed_buf[i];
            out_buf[i] = ((uint32_t)sample_u16 << 16) | (uint32_t)sample_u16;
        }
    }

    return max_len;
}

uint16_t BanGTsynth_GetAvailCallback(EffectNode_t *node)
{
    (void)node;
    /* 合成器是软件引擎, 始终可以产生数据 */
    return SYNTH_FRAME_SIZE;
}

/*============================================================================
 * MIDI 消息接口
 *===========================================================================*/

void BanGTsynth_SendMIDI(uint8_t *data, uint8_t len)
{
    if (!g_synth_initialized) {
        return;
    }
    BG_MIDI_controller.MIDI_Handle(data, len);
}

uint8_t BanGTsynth_IsInitialized(void)
{
    return g_synth_initialized;
}

#endif /* BANGTSYNTH_EN */
