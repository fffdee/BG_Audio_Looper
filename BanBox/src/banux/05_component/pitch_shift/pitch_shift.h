/**
 **************************************************************************************
 * @file    pitch_shift.h
 * @brief   实时半音移调（Pitch Shift）组件
 *          单声道 / 16bit / 44100Hz
 *          基于 WSOLA 颗粒重采样 + 重叠相加，时长基本不变
 *
 * @note    当前试听目标：吉他输入 → 模拟贝斯输出（降八度 + 低通塑形）
 *          暂未接入音效图；通过 BG_PITCH_SHIFT_TEST_EN 在 DAC 输出试听
 **************************************************************************************
 */

#ifndef __PITCH_SHIFT_H__
#define __PITCH_SHIFT_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 试听宏（先不进音效图，在 DAC 输出处临时挂接）
 * ============================================================================ */
#ifndef BG_PITCH_SHIFT_TEST_EN
/* 默认关闭：挂在 DAC0 最终输出时，算法 priming/欠载会写零 → 整机无声 */
#define BG_PITCH_SHIFT_TEST_EN              0
#endif

/* 吉他→贝斯：降 1 个八度 = -12 半音 */
#ifndef BG_PITCH_SHIFT_TEST_SEMITONE
#define BG_PITCH_SHIFT_TEST_SEMITONE        (-12)
#endif

/* 贝斯塑形：移调后再做低通，去掉吉他高频弦噪 */
#ifndef BG_PITCH_SHIFT_BASS_TONE_EN
#define BG_PITCH_SHIFT_BASS_TONE_EN         1
#endif

/* 低通截止频率（Hz），约 800~1200 更像贝斯；可按听感微调 */
#ifndef BG_PITCH_SHIFT_BASS_LPF_HZ
#define BG_PITCH_SHIFT_BASS_LPF_HZ          900
#endif

/* 贝斯 makeup 增益（Q8，256=1.0）。降八度后能量常偏弱，默认约 +3dB */
#ifndef BG_PITCH_SHIFT_BASS_GAIN_Q8
#define BG_PITCH_SHIFT_BASS_GAIN_Q8         360
#endif

/* ============================================================================
 * 算法参数（降八度用更长窗，减少金属感）
 * ============================================================================ */
#define PITCH_SHIFT_SAMPLE_RATE             44100
#define PITCH_SHIFT_FRAME_SIZE              1024    /* 分析/合成窗长，必须为偶数 */
#define PITCH_SHIFT_HOP_SIZE                256     /* 输入/输出步进 */
#define PITCH_SHIFT_IN_BUF_SIZE             4096    /* 需 >= FRAME/ratio + margin（-12 时 ratio=0.5） */
#define PITCH_SHIFT_OUT_BUF_SIZE            4096
#define PITCH_SHIFT_SEMITONE_MIN            (-12)
#define PITCH_SHIFT_SEMITONE_MAX            (12)

typedef struct {
    int16_t  in_buf[PITCH_SHIFT_IN_BUF_SIZE];
    int16_t  out_buf[PITCH_SHIFT_OUT_BUF_SIZE];
    int16_t  grain[PITCH_SHIFT_FRAME_SIZE];
    int16_t  window[PITCH_SHIFT_FRAME_SIZE];        /* Hann 窗，Q15 */
    int32_t  ola_acc[PITCH_SHIFT_FRAME_SIZE];       /* 重叠相加累加器 */
    int32_t  ola_norm[PITCH_SHIFT_FRAME_SIZE];      /* 窗能量归一化 */

    uint16_t in_w;
    uint16_t in_r;
    uint16_t in_count;
    uint16_t out_w;
    uint16_t out_r;
    uint16_t out_count;

    int32_t  sample_rate;
    int32_t  semitone;          /* 整数半音，负=降调，正=升调 */
    float    ratio;             /* 2^(semitone/12) */

    /* 贝斯塑形：一阶低通状态 */
    int32_t  lpf_y;             /* 滤波状态，Q15 样本域 */
    int32_t  lpf_a_q15;         /* 系数 a，Q15；y += a*(x-y) */
    int32_t  bass_gain_q8;      /* makeup 增益，Q8 */
    uint8_t  bass_tone_en;

    uint8_t  enabled;
    uint8_t  primed;            /* 是否已填满启动延迟 */
} PitchShiftContext_t;

/**
 * @brief 初始化移调器
 * @param ct          上下文（可为 NULL，则使用内部静态实例）
 * @param sample_rate 采样率，建议 44100
 * @param semitone    半音步进，范围 [-12, +12]；吉他贝斯用 -12
 */
void PitchShift_Init(PitchShiftContext_t *ct, int32_t sample_rate, int32_t semitone);

/**
 * @brief 复位内部缓冲（切歌/断流时调用）
 */
void PitchShift_Reset(PitchShiftContext_t *ct);

/**
 * @brief 使能/旁路
 */
void PitchShift_SetEnabled(PitchShiftContext_t *ct, uint8_t enabled);
uint8_t PitchShift_IsEnabled(PitchShiftContext_t *ct);

/**
 * @brief 实时设置半音（下次处理帧生效）
 * @param semitone 整数半音，超出范围会被钳位到 [-12, +12]
 */
void PitchShift_SetSemitone(PitchShiftContext_t *ct, int32_t semitone);
int32_t PitchShift_GetSemitone(PitchShiftContext_t *ct);

/**
 * @brief 开关贝斯塑形（低通 + makeup）
 */
void PitchShift_SetBassTone(PitchShiftContext_t *ct, uint8_t enable);

/**
 * @brief 处理单声道 PCM（可 in-place：pcm_out == pcm_in）
 * @param pcm_in  输入，int16 mono
 * @param pcm_out 输出，int16 mono
 * @param n       采样点数
 */
void PitchShift_Process(PitchShiftContext_t *ct,
                        const int16_t *pcm_in,
                        int16_t *pcm_out,
                        int32_t n);

/**
 * @brief 处理 DAC 打包立体声：低16位=L，高16位=R
 *        取 (L+R)/2 做单声道移调，再写回 L/R
 * @param packed  uint32 打包缓冲（in-place）
 * @param n       帧数（每帧一个 uint32）
 */
void PitchShift_ProcessPackedStereo(PitchShiftContext_t *ct,
                                    uint32_t *packed,
                                    int32_t n);

/**
 * @brief 获取内部默认实例（DAC 试听宏使用）
 */
PitchShiftContext_t *PitchShift_GetDefault(void);

#ifdef __cplusplus
}
#endif

#endif /* __PITCH_SHIFT_H__ */
