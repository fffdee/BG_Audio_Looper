#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_envelope.h"
#include <math.h>
#include <string.h>

/* 最小增量阈值(避免除零) */
#define MIN_INCREMENT 0.00001f

/* 指数曲线系数(控制曲线形状) */
#define EXP_CURVE_COEFF 4.0f

/**
 * 计算线性增量
 */
static float calculate_linear_increment(float start, float end, uint32_t samples) {
    if (samples == 0) return 0.0f;
    return (end - start) / (float)samples;
}

/**
 * 计算指数曲线增量
 * 使用简化的指数近似: level = start + (end - start) * (1 - e^(-t/tau))
 */
static float calculate_exp_increment(float delta, uint32_t samples) {
    if (samples == 0) return 0.0f;
    // 使用时间常数 tau = samples / 4
    return delta * EXP_CURVE_COEFF / (float)samples;
}

/**
 * 初始化包络生成器
 */
void BG_Envelope_Init(BG_Envelope_t *env, const BG_EnvParams_t *params, uint32_t sample_rate) {
    memset(env, 0, sizeof(BG_Envelope_t));
    env->sample_rate = sample_rate;
    env->params = *params;
    env->stage = BG_ENV_IDLE;
    env->is_active = false;
}

/**
 * 切换到下一个阶段
 */
static void switch_to_stage(BG_Envelope_t *env, BG_EnvStage_t new_stage) {
    env->stage = new_stage;
    env->samples_count = 0;
    
    switch (new_stage) {
        case BG_ENV_ATTACK:
            env->current_level = 0.0f;
            env->target_level = 1.0f;
            env->stage_samples = (uint32_t)(env->params.attack_time * env->sample_rate);
            if (env->params.curve == BG_ENV_CURVE_LINEAR) {
                env->increment = calculate_linear_increment(0.0f, 1.0f, env->stage_samples);
            } else {
                env->increment = calculate_exp_increment(1.0f, env->stage_samples);
            }
            env->is_active = true;
            break;
            
        case BG_ENV_DECAY:
            env->current_level = 1.0f;
            env->target_level = env->params.sustain_level;
            env->stage_samples = (uint32_t)(env->params.decay_time * env->sample_rate);
            if (env->params.curve == BG_ENV_CURVE_LINEAR) {
                env->increment = calculate_linear_increment(1.0f, env->params.sustain_level, env->stage_samples);
            } else {
                env->increment = -calculate_exp_increment(1.0f - env->params.sustain_level, env->stage_samples);
            }
            break;
            
        case BG_ENV_SUSTAIN:
            env->current_level = env->params.sustain_level;
            env->target_level = env->params.sustain_level;
            env->increment = 0.0f;
            env->stage_samples = 0xFFFFFFFF; // 无限持续
            break;
            
        case BG_ENV_RELEASE:
            // current_level 保持当前值
            env->target_level = 0.0f;
            env->stage_samples = (uint32_t)(env->params.release_time * env->sample_rate);
            if (env->params.curve == BG_ENV_CURVE_LINEAR) {
                env->increment = calculate_linear_increment(env->current_level, 0.0f, env->stage_samples);
            } else {
                env->increment = -calculate_exp_increment(env->current_level, env->stage_samples);
            }
            break;
            
        case BG_ENV_IDLE:
        default:
            env->current_level = 0.0f;
            env->target_level = 0.0f;
            env->increment = 0.0f;
            env->is_active = false;
            break;
    }
}

/**
 * 触发包络
 */
void BG_Envelope_Trigger(BG_Envelope_t *env) {
    switch_to_stage(env, BG_ENV_ATTACK);
}

/**
 * 释放包络
 */
void BG_Envelope_Release(BG_Envelope_t *env) {
    if (env->stage != BG_ENV_IDLE && env->stage != BG_ENV_RELEASE) {
        switch_to_stage(env, BG_ENV_RELEASE);
    }
}

/**
 * 处理一个采样
 */
float BG_Envelope_Process(BG_Envelope_t *env) {
    float output;

    if (!env->is_active) {
        return 0.0f;
    }
    
    output = env->current_level;
    
    // 更新电平
    if (env->params.curve == BG_ENV_CURVE_LINEAR) {
        env->current_level += env->increment;
    } else {
        // 指数曲线: 每次递增逐渐减小
        float delta = env->target_level - env->current_level;
        env->current_level += delta * env->increment;
    }
    
    env->samples_count++;
    
    // 检查阶段是否完成
    if (env->samples_count >= env->stage_samples) {
        switch (env->stage) {
            case BG_ENV_ATTACK:
                env->current_level = 1.0f; // 确保精确到达峰值
                switch_to_stage(env, BG_ENV_DECAY);
                break;
                
            case BG_ENV_DECAY:
                env->current_level = env->params.sustain_level; // 确保精确到达持续电平
                switch_to_stage(env, BG_ENV_SUSTAIN);
                break;
                
            case BG_ENV_RELEASE:
                env->current_level = 0.0f;
                switch_to_stage(env, BG_ENV_IDLE);
                break;
                
            case BG_ENV_SUSTAIN:
            case BG_ENV_IDLE:
            default:
                // 保持当前状态
                break;
        }
    }
    
    // 限幅保护
    if (env->current_level < 0.0f) env->current_level = 0.0f;
    if (env->current_level > 1.0f) env->current_level = 1.0f;
    
    return output;
}

/**
 * 批量处理采样
 */
void BG_Envelope_ProcessBlock(BG_Envelope_t *env, float *output, uint32_t count) {
    uint32_t i;
    
    for (i = 0; i < count; i++) {
        output[i] = BG_Envelope_Process(env);
    }
}

/**
 * 重置包络
 */
void BG_Envelope_Reset(BG_Envelope_t *env) {
    switch_to_stage(env, BG_ENV_IDLE);
}

/**
 * 更新包络参数
 */
void BG_Envelope_SetParams(BG_Envelope_t *env, const BG_EnvParams_t *params) {
    BG_EnvStage_t current_stage = env->stage;
    env->params = *params;
    
    // 如果正在运行,重新计算当前阶段的增量
    if (env->is_active && current_stage != BG_ENV_IDLE) {
        switch_to_stage(env, current_stage);
    }
}

#endif /* BANGTSYNTH_EN */
