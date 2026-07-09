/**
 **************************************************************************************
 * @file    pitch_shift.c
 * @brief   实时半音移调实现（WSOLA 颗粒重采样 + Hann 重叠相加）
 *          针对吉他→贝斯：默认降八度，并做低通塑形
 **************************************************************************************
 */

#include "pitch_shift.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* 默认实例：DAC 试听直接用，避免调用方再管内存 */
static PitchShiftContext_t s_pitch_shift_default;

static int16_t pitch_shift_clip16(int32_t x)
{
    if (x > 32767) {
        return 32767;
    }
    if (x < -32768) {
        return (int16_t)-32768;
    }
    return (int16_t)x;
}

static float pitch_shift_semitone_to_ratio(int32_t semitone)
{
    return powf(2.0f, (float)semitone / 12.0f);
}

static void pitch_shift_build_hann(int16_t *window, int32_t n)
{
    int32_t i;
    for (i = 0; i < n; i++) {
        float w = 0.5f * (1.0f - cosf(2.0f * M_PI * (float)i / (float)(n - 1)));
        int32_t q15 = (int32_t)(w * 32767.0f + 0.5f);
        if (q15 > 32767) {
            q15 = 32767;
        }
        window[i] = (int16_t)q15;
    }
}

static void pitch_shift_update_lpf(PitchShiftContext_t *ct)
{
    float fc;
    float a;
    int32_t a_q15;

    fc = (float)BG_PITCH_SHIFT_BASS_LPF_HZ;
    if (fc < 100.0f) {
        fc = 100.0f;
    }
    if (fc > (float)ct->sample_rate * 0.45f) {
        fc = (float)ct->sample_rate * 0.45f;
    }

    /* 一阶低通：y += a*(x-y)，a ≈ 1 - exp(-2π fc/fs) */
    a = 1.0f - expf(-2.0f * M_PI * fc / (float)ct->sample_rate);
    a_q15 = (int32_t)(a * 32768.0f + 0.5f);
    if (a_q15 < 1) {
        a_q15 = 1;
    }
    if (a_q15 > 32767) {
        a_q15 = 32767;
    }
    ct->lpf_a_q15 = a_q15;
}

static int16_t pitch_shift_bass_tone(PitchShiftContext_t *ct, int16_t x)
{
    int32_t diff;
    int32_t y;
    int32_t out;

    if (!ct->bass_tone_en) {
        return x;
    }

    /* 一阶 LPF */
    diff = ((int32_t)x << 15) - ct->lpf_y;
    ct->lpf_y += (diff * ct->lpf_a_q15) >> 15;
    y = ct->lpf_y >> 15;

    /* makeup + 轻度软限幅，增加一点贝斯“肉感” */
    out = (y * ct->bass_gain_q8) >> 8;
    if (out > 28000 || out < -28000) {
        /* 软拐点，避免硬削波刺耳 */
        if (out > 32767) {
            out = 32767;
        } else if (out < -32768) {
            out = -32768;
        } else {
            /* out' = out - out^3 / (3 * 32768^2) 的简化近似 */
            int32_t n = out >> 2;
            out = out - ((n * n * (out >> 8)) >> 20);
        }
    }
    return pitch_shift_clip16(out);
}

static int16_t pitch_shift_lerp(const int16_t *buf, uint16_t base, uint16_t buf_mask,
                                float pos, uint16_t valid_len)
{
    int32_t idx;
    float frac;
    int16_t s0;
    int16_t s1;

    if (pos < 0.0f) {
        pos = 0.0f;
    }
    if (pos > (float)(valid_len - 1)) {
        pos = (float)(valid_len - 1);
    }

    idx = (int32_t)pos;
    frac = pos - (float)idx;
    s0 = buf[(base + (uint16_t)idx) & buf_mask];
    if (idx + 1 < valid_len) {
        s1 = buf[(base + (uint16_t)(idx + 1)) & buf_mask];
    } else {
        s1 = s0;
    }
    return pitch_shift_clip16((int32_t)((1.0f - frac) * (float)s0 + frac * (float)s1));
}

static void pitch_shift_update_ratio(PitchShiftContext_t *ct)
{
    if (ct->semitone < PITCH_SHIFT_SEMITONE_MIN) {
        ct->semitone = PITCH_SHIFT_SEMITONE_MIN;
    } else if (ct->semitone > PITCH_SHIFT_SEMITONE_MAX) {
        ct->semitone = PITCH_SHIFT_SEMITONE_MAX;
    }
    ct->ratio = pitch_shift_semitone_to_ratio(ct->semitone);
    if (ct->ratio < 0.5f) {
        ct->ratio = 0.5f;
    } else if (ct->ratio > 2.0f) {
        ct->ratio = 2.0f;
    }
}

/**
 * @brief 从输入环缓生成一粒，重叠相加后推出 hop 点到输出环缓
 */
static void pitch_shift_synthesize_grain(PitchShiftContext_t *ct)
{
    const uint16_t in_mask = (uint16_t)(PITCH_SHIFT_IN_BUF_SIZE - 1);
    const uint16_t out_mask = (uint16_t)(PITCH_SHIFT_OUT_BUF_SIZE - 1);
    const int32_t frame = PITCH_SHIFT_FRAME_SIZE;
    const int32_t hop = PITCH_SHIFT_HOP_SIZE;
    uint16_t base;
    int32_t i;
    float inv_ratio;
    int32_t need_input_span;

    /* 颗粒在输入侧需要覆盖的跨度：frame / ratio（降调时更长） */
    inv_ratio = 1.0f / ct->ratio;
    need_input_span = (int32_t)((float)frame * inv_ratio) + 2;
    if (need_input_span < frame) {
        need_input_span = frame;
    }
    if (ct->in_count < (uint16_t)need_input_span) {
        return;
    }

    base = ct->in_r;

    /* 按 ratio 重采样颗粒：ratio<1 降调（吉他→贝斯） */
    for (i = 0; i < frame; i++) {
        float src_pos = (float)i * inv_ratio;
        ct->grain[i] = pitch_shift_lerp(ct->in_buf, base, in_mask, src_pos,
                                        (uint16_t)need_input_span);
    }

    /* Hann 窗重叠相加 */
    for (i = 0; i < frame; i++) {
        int32_t w = (int32_t)ct->window[i];
        int32_t s = (int32_t)ct->grain[i];
        ct->ola_acc[i] += (s * w) >> 15;
        ct->ola_norm[i] += (w * w) >> 15;
    }

    /* 推出前 hop 点到输出 */
    for (i = 0; i < hop; i++) {
        int32_t y;
        if (ct->ola_norm[i] > 0) {
            y = (ct->ola_acc[i] << 15) / ct->ola_norm[i];
        } else {
            y = ct->ola_acc[i];
        }
        ct->out_buf[ct->out_w] = pitch_shift_clip16(y);
        ct->out_w = (uint16_t)((ct->out_w + 1) & out_mask);
        if (ct->out_count < PITCH_SHIFT_OUT_BUF_SIZE) {
            ct->out_count++;
        } else {
            ct->out_r = (uint16_t)((ct->out_r + 1) & out_mask);
        }
    }

    /* 累加器左移 hop，为下一次重叠腾出空间 */
    memmove(&ct->ola_acc[0], &ct->ola_acc[hop],
            (size_t)(frame - hop) * sizeof(int32_t));
    memmove(&ct->ola_norm[0], &ct->ola_norm[hop],
            (size_t)(frame - hop) * sizeof(int32_t));
    memset(&ct->ola_acc[frame - hop], 0, (size_t)hop * sizeof(int32_t));
    memset(&ct->ola_norm[frame - hop], 0, (size_t)hop * sizeof(int32_t));

    /* 输入消耗 hop（时长保持 1:1） */
    ct->in_r = (uint16_t)((ct->in_r + hop) & in_mask);
    ct->in_count = (uint16_t)(ct->in_count - hop);
}

void PitchShift_Init(PitchShiftContext_t *ct, int32_t sample_rate, int32_t semitone)
{
    if (ct == NULL) {
        ct = &s_pitch_shift_default;
    }

    memset(ct, 0, sizeof(*ct));
    ct->sample_rate = (sample_rate > 0) ? sample_rate : PITCH_SHIFT_SAMPLE_RATE;
    ct->semitone = semitone;
    ct->enabled = 1;
    ct->bass_tone_en = BG_PITCH_SHIFT_BASS_TONE_EN ? 1 : 0;
    ct->bass_gain_q8 = BG_PITCH_SHIFT_BASS_GAIN_Q8;
    pitch_shift_update_ratio(ct);
    pitch_shift_update_lpf(ct);
    pitch_shift_build_hann(ct->window, PITCH_SHIFT_FRAME_SIZE);
}

void PitchShift_Reset(PitchShiftContext_t *ct)
{
    if (ct == NULL) {
        ct = &s_pitch_shift_default;
    }

    ct->in_w = 0;
    ct->in_r = 0;
    ct->in_count = 0;
    ct->out_w = 0;
    ct->out_r = 0;
    ct->out_count = 0;
    ct->primed = 0;
    ct->lpf_y = 0;
    memset(ct->ola_acc, 0, sizeof(ct->ola_acc));
    memset(ct->ola_norm, 0, sizeof(ct->ola_norm));
}

void PitchShift_SetEnabled(PitchShiftContext_t *ct, uint8_t enabled)
{
    if (ct == NULL) {
        ct = &s_pitch_shift_default;
    }
    ct->enabled = enabled ? 1 : 0;
}

uint8_t PitchShift_IsEnabled(PitchShiftContext_t *ct)
{
    if (ct == NULL) {
        ct = &s_pitch_shift_default;
    }
    return ct->enabled;
}

void PitchShift_SetSemitone(PitchShiftContext_t *ct, int32_t semitone)
{
    if (ct == NULL) {
        ct = &s_pitch_shift_default;
    }
    ct->semitone = semitone;
    pitch_shift_update_ratio(ct);
}

int32_t PitchShift_GetSemitone(PitchShiftContext_t *ct)
{
    if (ct == NULL) {
        ct = &s_pitch_shift_default;
    }
    return ct->semitone;
}

void PitchShift_SetBassTone(PitchShiftContext_t *ct, uint8_t enable)
{
    if (ct == NULL) {
        ct = &s_pitch_shift_default;
    }
    ct->bass_tone_en = enable ? 1 : 0;
    if (!ct->bass_tone_en) {
        ct->lpf_y = 0;
    }
}

PitchShiftContext_t *PitchShift_GetDefault(void)
{
    return &s_pitch_shift_default;
}

void PitchShift_Process(PitchShiftContext_t *ct,
                        const int16_t *pcm_in,
                        int16_t *pcm_out,
                        int32_t n)
{
    const uint16_t in_mask = (uint16_t)(PITCH_SHIFT_IN_BUF_SIZE - 1);
    const uint16_t out_mask = (uint16_t)(PITCH_SHIFT_OUT_BUF_SIZE - 1);
    int32_t i;
    int32_t need_span;

    if (ct == NULL) {
        ct = &s_pitch_shift_default;
    }
    if (pcm_in == NULL || pcm_out == NULL || n <= 0) {
        return;
    }

    if (!ct->enabled || ct->semitone == 0) {
        if (pcm_out != pcm_in) {
            memcpy(pcm_out, pcm_in, (size_t)n * sizeof(int16_t));
        }
        return;
    }

    /* 写入输入环缓 */
    for (i = 0; i < n; i++) {
        ct->in_buf[ct->in_w] = pcm_in[i];
        ct->in_w = (uint16_t)((ct->in_w + 1) & in_mask);
        if (ct->in_count < PITCH_SHIFT_IN_BUF_SIZE) {
            ct->in_count++;
        } else {
            ct->in_r = (uint16_t)((ct->in_r + 1) & in_mask);
        }
    }

    need_span = (int32_t)((float)PITCH_SHIFT_FRAME_SIZE / ct->ratio) + 2;
    if (need_span < PITCH_SHIFT_FRAME_SIZE) {
        need_span = PITCH_SHIFT_FRAME_SIZE;
    }

    /* 启动：先攒够一窗，避免开头咔哒声 */
    if (!ct->primed) {
        if (ct->in_count >= (uint16_t)need_span) {
            ct->primed = 1;
        } else {
            memset(pcm_out, 0, (size_t)n * sizeof(int16_t));
            return;
        }
    }

    while (ct->in_count >= (uint16_t)need_span) {
        uint16_t before = ct->in_count;
        pitch_shift_synthesize_grain(ct);
        if (ct->in_count == before) {
            break; /* 防御：未能推进则退出 */
        }
    }

    /* 读出与输入等长的输出，并做贝斯塑形 */
    for (i = 0; i < n; i++) {
        int16_t s;
        if (ct->out_count > 0) {
            s = ct->out_buf[ct->out_r];
            ct->out_r = (uint16_t)((ct->out_r + 1) & out_mask);
            ct->out_count--;
        } else {
            s = 0;
        }
        pcm_out[i] = pitch_shift_bass_tone(ct, s);
    }
}

void PitchShift_ProcessPackedStereo(PitchShiftContext_t *ct,
                                    uint32_t *packed,
                                    int32_t n)
{
    int16_t mono_in[128];
    int16_t mono_out[128];
    int32_t i;
    int32_t chunk;
    int32_t offset;

    if (ct == NULL) {
        ct = &s_pitch_shift_default;
    }
    if (packed == NULL || n <= 0) {
        return;
    }
    if (!ct->enabled || ct->semitone == 0) {
        return;
    }

    offset = 0;
    while (offset < n) {
        chunk = n - offset;
        if (chunk > 128) {
            chunk = 128;
        }

        for (i = 0; i < chunk; i++) {
            int16_t l = (int16_t)(packed[offset + i] & 0xFFFFU);
            int16_t r = (int16_t)((packed[offset + i] >> 16) & 0xFFFFU);
            mono_in[i] = (int16_t)(((int32_t)l + (int32_t)r) / 2);
        }

        PitchShift_Process(ct, mono_in, mono_out, chunk);

        for (i = 0; i < chunk; i++) {
            uint16_t m = (uint16_t)mono_out[i];
            packed[offset + i] = ((uint32_t)m << 16) | (uint32_t)m;
        }

        offset += chunk;
    }
}
