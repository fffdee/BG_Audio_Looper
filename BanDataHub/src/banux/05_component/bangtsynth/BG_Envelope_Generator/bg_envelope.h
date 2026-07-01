#ifndef BG_ENVELOPE_H
#define BG_ENVELOPE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * BG 包络生成器 (ADSR Envelope Generator)
 * 
 * 通用包络模块,适用于:
 * - SF2 音源合成
 * - BGS 音源合成
 * - 任何需要包络控制的音频处理
 * 
 * 特性:
 * - 标准 ADSR (Attack-Decay-Sustain-Release) 包络
 * - 支持线性和指数曲线
 * - 可配置采样率
 * - 低 CPU 开销的整数运算
 */

#ifdef __cplusplus
extern "C" {
#endif

/* 包络阶段枚举 */
typedef enum {
    BG_ENV_IDLE,      // 空闲状态(包络未触发)
    BG_ENV_ATTACK,    // 攻击阶段(从0上升到峰值)
    BG_ENV_DECAY,     // 衰减阶段(从峰值下降到持续电平)
    BG_ENV_SUSTAIN,   // 持续阶段(保持在持续电平)
    BG_ENV_RELEASE    // 释放阶段(从当前电平下降到0)
} BG_EnvStage_t;

/* 包络曲线类型 */
typedef enum {
    BG_ENV_CURVE_LINEAR,      // 线性曲线
    BG_ENV_CURVE_EXPONENTIAL  // 指数曲线(更自然)
} BG_EnvCurve_t;

/* 包络参数结构 */
typedef struct {
    float attack_time;    // 攻击时间(秒), 0.001 ~ 10.0
    float decay_time;     // 衰减时间(秒), 0.001 ~ 10.0
    float sustain_level;  // 持续电平(0.0 ~ 1.0)
    float release_time;   // 释放时间(秒), 0.001 ~ 10.0
    BG_EnvCurve_t curve;  // 曲线类型
} BG_EnvParams_t;

/* 包络生成器实例 */
typedef struct {
    // 参数配置
    BG_EnvParams_t params;
    uint32_t sample_rate;
    
    // 运行时状态
    BG_EnvStage_t stage;
    float current_level;     // 当前电平 (0.0 ~ 1.0)
    float target_level;      // 目标电平
    float increment;         // 每次采样的增量
    uint32_t samples_count;  // 当前阶段已处理的采样数
    uint32_t stage_samples;  // 当前阶段总采样数
    
    // 速度优化
    bool is_active;          // 包络是否激活(快速判断)
    
} BG_Envelope_t;

/**
 * 初始化包络生成器
 * 
 * @param env 包络实例指针
 * @param params 包络参数
 * @param sample_rate 采样率 (Hz)
 */
void BG_Envelope_Init(BG_Envelope_t *env, const BG_EnvParams_t *params, uint32_t sample_rate);

/**
 * 触发包络 (Note On)
 * 
 * 启动包络从 Attack 阶段开始
 * 
 * @param env 包络实例指针
 */
void BG_Envelope_Trigger(BG_Envelope_t *env);

/**
 * 释放包络 (Note Off)
 * 
 * 进入 Release 阶段,从当前电平下降到0
 * 
 * @param env 包络实例指针
 */
void BG_Envelope_Release(BG_Envelope_t *env);

/**
 * 处理一个采样
 * 
 * @param env 包络实例指针
 * @return 当前包络值 (0.0 ~ 1.0)
 */
float BG_Envelope_Process(BG_Envelope_t *env);

/**
 * 批量处理采样(优化版本)
 * 
 * @param env 包络实例指针
 * @param output 输出缓冲区
 * @param count 采样数量
 */
void BG_Envelope_ProcessBlock(BG_Envelope_t *env, float *output, uint32_t count);

/**
 * 重置包络到空闲状态
 * 
 * @param env 包络实例指针
 */
void BG_Envelope_Reset(BG_Envelope_t *env);

/**
 * 检查包络是否处于激活状态
 * 
 * @param env 包络实例指针
 * @return true=包络正在运行, false=已结束
 */
static inline bool BG_Envelope_IsActive(const BG_Envelope_t *env) {
    return env->is_active;
}

/**
 * 获取当前包络电平
 * 
 * @param env 包络实例指针
 * @return 当前电平 (0.0 ~ 1.0)
 */
static inline float BG_Envelope_GetLevel(const BG_Envelope_t *env) {
    return env->current_level;
}

/**
 * 获取当前包络阶段
 * 
 * @param env 包络实例指针
 * @return 当前阶段
 */
static inline BG_EnvStage_t BG_Envelope_GetStage(const BG_Envelope_t *env) {
    return env->stage;
}

/**
 * 更新包络参数(动态调整)
 * 
 * @param env 包络实例指针
 * @param params 新参数
 */
void BG_Envelope_SetParams(BG_Envelope_t *env, const BG_EnvParams_t *params);

/**
 * 创建默认包络参数
 * 
 * @param attack_ms 攻击时间(毫秒)
 * @param decay_ms 衰减时间(毫秒)
 * @param sustain_level 持续电平(0.0~1.0)
 * @param release_ms 释放时间(毫秒)
 * @return 包络参数结构
 */
static inline BG_EnvParams_t BG_Envelope_CreateParams(
    float attack_ms, 
    float decay_ms, 
    float sustain_level, 
    float release_ms
) {
    BG_EnvParams_t params = {
        .attack_time = attack_ms / 1000.0f,
        .decay_time = decay_ms / 1000.0f,
        .sustain_level = sustain_level,
        .release_time = release_ms / 1000.0f,
        .curve = BG_ENV_CURVE_EXPONENTIAL
    };
    return params;
}

#ifdef __cplusplus
}
#endif

#endif /* BG_ENVELOPE_H */
