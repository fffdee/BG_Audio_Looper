#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "bg_effect_drc.h"
#include "bg_log.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* 采样率 (从bg_config.h获取) */
#define SAMPLE_RATE BG_SAMPLE_RATE

/**
 * DRC效果初始化
 */
void bg_effect_drc_init(void *user_data)
{
    if (!user_data) return;
    
    BG_DRC_Effect_t *drc = (BG_DRC_Effect_t *)user_data;
    
    /* 重置状态 */
    drc->state.envelope = 0.0f;
    drc->state.gain_reduction = 1.0f;
    
    BG_LOG_I(BG_LOG_TAG_EFFECT_DRC, "Initialized - Threshold:%.2f Ratio:%.1f Attack:%.1fms Release:%.1fms\n",
           drc->config.threshold, drc->config.ratio, 
           drc->config.attack_ms, drc->config.release_ms);
}

/**
 * DRC效果处理
 */
uint16_t bg_effect_drc_process(const short *input, short *output, uint16_t samples, void *user_data)
{
    BG_DRC_Effect_t *drc;
    float attack_coef, release_coef;
    uint16_t i;
    
    if (!input || !output || !user_data || samples == 0) {
        return 0;
    }
    
    drc = (BG_DRC_Effect_t *)user_data;
    
    /* 计算时间常数 (每次处理都计算,支持动态修改参数) */
    attack_coef = expf(-1.0f / (drc->config.attack_ms * SAMPLE_RATE / 1000.0f));
    release_coef = expf(-1.0f / (drc->config.release_ms * SAMPLE_RATE / 1000.0f));
    
    for (i = 0; i < samples; i++) {
        /* 归一化到 -1.0 ~ +1.0 */
        float normalized = (float)input[i] / 32768.0f;
        float input_level;
        float gain;
        float processed;
        float over_threshold;
        float compressed;
        float target_level;
        
        /* 获取输入电平 (取绝对值) */
        input_level = fabsf(normalized);
        
        /* 包络跟随 */
        if (input_level > drc->state.envelope) {
            /* 上升阶段 (attack) */
            drc->state.envelope = attack_coef * drc->state.envelope + 
                                  (1.0f - attack_coef) * input_level;
        } else {
            /* 下降阶段 (release) */
            drc->state.envelope = release_coef * drc->state.envelope + 
                                  (1.0f - release_coef) * input_level;
        }
        
        /* 计算增益衰减 */
        gain = 1.0f;
        
        if (drc->state.envelope > drc->config.threshold) {
            /* 超过阈值,计算压缩 */
            over_threshold = drc->state.envelope - drc->config.threshold;
            compressed = over_threshold / drc->config.ratio;
            
            /* 计算目标电平 */
            target_level = drc->config.threshold + compressed;
            
            /* 计算增益衰减 */
            if (drc->state.envelope > 0.0f) {
                gain = target_level / drc->state.envelope;
            }
        }
        
        /* 平滑增益变化 */
        drc->state.gain_reduction = gain;
        
        /* 应用增益 */
        processed = normalized * gain;
        
        /* 限幅到 -1.0 ~ +1.0 */
        if (processed > 1.0f) processed = 1.0f;
        if (processed < -1.0f) processed = -1.0f;
        
        /* 转换回16位整数 */
        output[i] = (short)(processed * 32767.0f);
    }
    
    return samples;
}

/**
 * DRC效果重置
 */
void bg_effect_drc_reset(void *user_data)
{
    if (!user_data) return;
    
    BG_DRC_Effect_t *drc = (BG_DRC_Effect_t *)user_data;
    
    drc->state.envelope = 0.0f;
    drc->state.gain_reduction = 1.0f;
    
    BG_LOG_D(BG_LOG_TAG_EFFECT_DRC, "Reset\n");
}

/**
 * 创建DRC效果实例 (辅助函数)
 */
BG_DRC_Effect_t* bg_effect_drc_create(float threshold, float ratio, 
                                       float attack_ms, float release_ms)
{
    static BG_DRC_Effect_t drc_instance;
    
    drc_instance.config.threshold = threshold;
    drc_instance.config.ratio = ratio;
    drc_instance.config.attack_ms = attack_ms;
    drc_instance.config.release_ms = release_ms;
    
    drc_instance.state.envelope = 0.0f;
    drc_instance.state.gain_reduction = 1.0f;
    
    return &drc_instance;
}

#endif /* BANGTSYNTH_EN */
