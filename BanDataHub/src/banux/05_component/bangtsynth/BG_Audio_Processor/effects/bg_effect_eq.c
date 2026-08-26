#include "bangtsynth_legacy.h"
#if BANGTSYNTH_LEGACY

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "bg_effect_eq.h"
#include "bg_log.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 全局 EQ 实例 (静态分配) */
static BG_EQ_Effect_t g_eq_instance;

/* 内部函数声明 */
static void calculate_biquad_coefficients(BG_Biquad_Coef_t *coef,
                                          BG_EQ_FilterType_t type,
                                          float freq, float gain, float q,
                                          uint32_t sample_rate);
static float process_biquad(float input, const BG_Biquad_Coef_t *coef, BG_Biquad_State_t *state);

/**
 * EQ 效果初始化
 */
void bg_effect_eq_init(void *user_data)
{
    BG_EQ_Effect_t *eq;
    uint8_t i;
    
    if (!user_data) return;
    
    eq = (BG_EQ_Effect_t *)user_data;
    
    /* 重置状态 */
    memset(eq->states, 0, sizeof(eq->states));
    
    /* 更新滤波器系数 */
    bg_effect_eq_update_coefficients(eq);
    
    BG_LOG_I(BG_LOG_TAG_EFFECT_EQ, "Initialized - Bands: %d, Sample Rate: %d Hz\n",
           eq->band_count, eq->sample_rate);
    
    /* 打印参数表 */
    for (i = 0; i < eq->band_count; i++) {
        if (eq->bands[i].enabled) {
            const char *type_str[] = {"Peak", "LowShelf", "HighShelf", "LowPass", "HighPass"};
            BG_LOG_D(BG_LOG_TAG_EFFECT_EQ, "  Band[%d]: %s, Freq=%.0fHz, Gain=%.1fdB, Q=%.2f\n",
                   i, type_str[eq->bands[i].type], eq->bands[i].freq, 
                   eq->bands[i].gain, eq->bands[i].q);
        }
    }
}

/**
 * EQ 效果处理
 */
uint16_t bg_effect_eq_process(const short *input, short *output, uint16_t samples, void *user_data)
{
    BG_EQ_Effect_t *eq;
    uint16_t i;
    uint8_t band;
    
    if (!input || !output || !user_data || samples == 0) {
        return 0;
    }
    
    eq = (BG_EQ_Effect_t *)user_data;
    
    for (i = 0; i < samples; i++) {
        /* 归一化到 -1.0 ~ +1.0 */
        float sample = (float)input[i] / 32768.0f;
        
        /* 依次通过所有启用的 EQ 段 */
        for (band = 0; band < eq->band_count; band++) {
            if (eq->bands[band].enabled) {
                sample = process_biquad(sample, &eq->coefs[band], &eq->states[band]);
            }
        }
        
        /* 限幅 */
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        
        /* 转换回 16 位整数 */
        output[i] = (short)(sample * 32767.0f);
    }
    
    return samples;
}

/**
 * EQ 效果重置
 */
void bg_effect_eq_reset(void *user_data)
{
    BG_EQ_Effect_t *eq;

    if (!user_data) return;
    
    eq = (BG_EQ_Effect_t *)user_data;
    
    /* 清空滤波器状态 */
    memset(eq->states, 0, sizeof(eq->states));
    
    BG_LOG_D(BG_LOG_TAG_EFFECT_EQ, "Reset\n");
}

/**
 * 创建 EQ 效果实例
 */
BG_EQ_Effect_t* bg_effect_eq_create(void)
{
    memset(&g_eq_instance, 0, sizeof(BG_EQ_Effect_t));
    
    g_eq_instance.sample_rate = BG_SAMPLE_RATE;
    g_eq_instance.band_count = 0;
    
    return &g_eq_instance;
}

/**
 * 设置 EQ 段参数
 */
int bg_effect_eq_set_band(BG_EQ_Effect_t *eq, uint8_t band_index,
                          uint8_t enabled, BG_EQ_FilterType_t type,
                          float freq, float gain, float q)
{
    if (!eq || band_index >= BG_EQ_MAX_BANDS) {
        return -1;
    }
    
    eq->bands[band_index].enabled = enabled;
    eq->bands[band_index].type = type;
    eq->bands[band_index].freq = freq;
    eq->bands[band_index].gain = gain;
    eq->bands[band_index].q = q;
    
    /* 更新段数 */
    if (band_index >= eq->band_count) {
        eq->band_count = band_index + 1;
    }
    
    return 0;
}

/**
 * 更新滤波器系数
 */
void bg_effect_eq_update_coefficients(BG_EQ_Effect_t *eq)
{
    uint8_t i;
    
    if (!eq) return;
    
    for (i = 0; i < eq->band_count; i++) {
        if (eq->bands[i].enabled) {
            calculate_biquad_coefficients(&eq->coefs[i],
                                          eq->bands[i].type,
                                          eq->bands[i].freq,
                                          eq->bands[i].gain,
                                          eq->bands[i].q,
                                          eq->sample_rate);
        }
    }
}

/**
 * 计算双二阶滤波器系数 (Biquad)
 */
static void calculate_biquad_coefficients(BG_Biquad_Coef_t *coef,
                                          BG_EQ_FilterType_t type,
                                          float freq, float gain, float q,
                                          uint32_t sample_rate)
{
    float w0 = 2.0f * M_PI * freq / sample_rate;
    float cos_w0 = cosf(w0);
    float sin_w0 = sinf(w0);
    float alpha = sin_w0 / (2.0f * q);
    float A = powf(10.0f, gain / 40.0f);  // dB 转增益
    
    float a0, a1, a2, b0, b1, b2;
    
    switch (type) {
        case BG_EQ_FILTER_PEAK:  // 峰值滤波器
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cos_w0;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cos_w0;
            a2 = 1.0f - alpha / A;
            break;
            
        case BG_EQ_FILTER_LOW_SHELF:  // 低频搁架
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrtf(A) * alpha);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0);
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrtf(A) * alpha);
            a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrtf(A) * alpha;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0);
            a2 = (A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sqrtf(A) * alpha;
            break;
            
        case BG_EQ_FILTER_HIGH_SHELF:  // 高频搁架
            b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrtf(A) * alpha);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0);
            b2 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sqrtf(A) * alpha);
            a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrtf(A) * alpha;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0);
            a2 = (A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrtf(A) * alpha;
            break;
            
        case BG_EQ_FILTER_LOW_PASS:  // 低通滤波器
            b0 = (1.0f - cos_w0) / 2.0f;
            b1 = 1.0f - cos_w0;
            b2 = (1.0f - cos_w0) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_w0;
            a2 = 1.0f - alpha;
            break;
            
        case BG_EQ_FILTER_HIGH_PASS:  // 高通滤波器
            b0 = (1.0f + cos_w0) / 2.0f;
            b1 = -(1.0f + cos_w0);
            b2 = (1.0f + cos_w0) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_w0;
            a2 = 1.0f - alpha;
            break;
            
        default:
            // 旁路: y = x
            b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
            a0 = 1.0f; a1 = 0.0f; a2 = 0.0f;
            break;
    }
    
    /* 归一化系数 */
    coef->b0 = b0 / a0;
    coef->b1 = b1 / a0;
    coef->b2 = b2 / a0;
    coef->a1 = a1 / a0;
    coef->a2 = a2 / a0;
}

/**
 * 双二阶滤波器处理单个样本
 */
static float process_biquad(float input, const BG_Biquad_Coef_t *coef, BG_Biquad_State_t *state)
{
    /* Direct Form II */
    float output = coef->b0 * input + state->x1;
    state->x1 = coef->b1 * input - coef->a1 * output + state->x2;
    state->x2 = coef->b2 * input - coef->a2 * output;
    
    return output;
}

/**
 * EQ 预设: 流行音乐
 * 提升低频和高频,略微削减中频
 */
void bg_effect_eq_preset_pop(BG_EQ_Effect_t *eq)
{
    if (!eq) return;
    
    bg_effect_eq_set_band(eq, 0, 1, BG_EQ_FILTER_LOW_SHELF,  100.0f,  3.0f, 0.7f);  // 低频 +3dB
    bg_effect_eq_set_band(eq, 1, 1, BG_EQ_FILTER_PEAK,       800.0f, -2.0f, 1.0f);  // 中频 -2dB
    bg_effect_eq_set_band(eq, 2, 1, BG_EQ_FILTER_PEAK,      3000.0f,  1.0f, 1.2f);  // 中高频 +1dB
    bg_effect_eq_set_band(eq, 3, 1, BG_EQ_FILTER_HIGH_SHELF, 8000.0f, 4.0f, 0.7f);  // 高频 +4dB
    
    bg_effect_eq_update_coefficients(eq);
    BG_LOG_I(BG_LOG_TAG_EFFECT_EQ, "Preset: Pop\n");
}

/**
 * EQ 预设: 摇滚音乐
 * 强化低频和高频,削减中低频
 */
void bg_effect_eq_preset_rock(BG_EQ_Effect_t *eq)
{
    if (!eq) return;
    
    bg_effect_eq_set_band(eq, 0, 1, BG_EQ_FILTER_LOW_SHELF,   80.0f,  5.0f, 0.7f);  // 低频 +5dB
    bg_effect_eq_set_band(eq, 1, 1, BG_EQ_FILTER_PEAK,       400.0f, -3.0f, 1.2f);  // 中低频 -3dB
    bg_effect_eq_set_band(eq, 2, 1, BG_EQ_FILTER_PEAK,      2500.0f,  2.0f, 1.0f);  // 中高频 +2dB
    bg_effect_eq_set_band(eq, 3, 1, BG_EQ_FILTER_HIGH_SHELF, 6000.0f, 5.0f, 0.7f);  // 高频 +5dB
    
    bg_effect_eq_update_coefficients(eq);
    BG_LOG_I(BG_LOG_TAG_EFFECT_EQ, "Preset: Rock\n");
}

/**
 * EQ 预设: 古典音乐
 * 平衡,略微提升低频和超高频
 */
void bg_effect_eq_preset_classical(BG_EQ_Effect_t *eq)
{
    if (!eq) return;
    
    bg_effect_eq_set_band(eq, 0, 1, BG_EQ_FILTER_LOW_SHELF,  100.0f,  2.0f, 0.7f);  // 低频 +2dB
    bg_effect_eq_set_band(eq, 1, 1, BG_EQ_FILTER_PEAK,      1000.0f,  0.0f, 1.0f);  // 中频 平坦
    bg_effect_eq_set_band(eq, 2, 1, BG_EQ_FILTER_PEAK,      4000.0f,  1.0f, 1.2f);  // 高频 +1dB
    bg_effect_eq_set_band(eq, 3, 1, BG_EQ_FILTER_HIGH_SHELF, 10000.0f, 2.0f, 0.7f); // 超高频 +2dB
    
    bg_effect_eq_update_coefficients(eq);
    BG_LOG_I(BG_LOG_TAG_EFFECT_EQ, "Preset: Classical\n");
}

/**
 * EQ 预设: 爵士音乐
 * 提升低频和中高频,保持温暖音色
 */
void bg_effect_eq_preset_jazz(BG_EQ_Effect_t *eq)
{
    if (!eq) return;
    
    bg_effect_eq_set_band(eq, 0, 1, BG_EQ_FILTER_LOW_SHELF,  120.0f,  3.0f, 0.7f);  // 低频 +3dB
    bg_effect_eq_set_band(eq, 1, 1, BG_EQ_FILTER_PEAK,       500.0f, -1.0f, 1.0f);  // 中低频 -1dB
    bg_effect_eq_set_band(eq, 2, 1, BG_EQ_FILTER_PEAK,      3500.0f,  3.0f, 1.2f);  // 中高频 +3dB
    bg_effect_eq_set_band(eq, 3, 1, BG_EQ_FILTER_HIGH_SHELF, 7000.0f, 1.0f, 0.7f);  // 高频 +1dB
    
    bg_effect_eq_update_coefficients(eq);
    BG_LOG_I(BG_LOG_TAG_EFFECT_EQ, "Preset: Jazz\n");
}

#endif /* BANGTSYNTH_EN */

#endif /* BANGTSYNTH_LEGACY */
