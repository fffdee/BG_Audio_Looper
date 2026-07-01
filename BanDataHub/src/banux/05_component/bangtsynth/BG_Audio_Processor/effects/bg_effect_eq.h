#ifndef _BG_EFFECT_EQ_H__
#define _BG_EFFECT_EQ_H__

#include <stdint.h>
#include "bg_config.h"

/**
 * EQ (均衡器) 音频效果
 * 支持多段参数化均衡 (Parametric EQ)
 */

/* 最大 EQ 段数 */
#define BG_EQ_MAX_BANDS 5

/* EQ 滤波器类型 */
typedef enum {
    BG_EQ_FILTER_PEAK = 0,      // 峰值滤波器 (中频)
    BG_EQ_FILTER_LOW_SHELF,     // 低频搁架
    BG_EQ_FILTER_HIGH_SHELF,    // 高频搁架
    BG_EQ_FILTER_LOW_PASS,      // 低通滤波器
    BG_EQ_FILTER_HIGH_PASS      // 高通滤波器
} BG_EQ_FilterType_t;

/* EQ 段参数 (单个频段) */
typedef struct {
    uint8_t enabled;            // 使能标志 (1=启用, 0=禁用)
    BG_EQ_FilterType_t type;    // 滤波器类型
    float freq;                 // 中心频率 (Hz)
    float gain;                 // 增益 (dB, -12 ~ +12)
    float q;                    // 品质因数 (0.1 ~ 10.0)
} BG_EQ_Band_t;

/* 双二阶滤波器系数 (Biquad) */
typedef struct {
    float b0, b1, b2;           // 前馈系数
    float a1, a2;               // 反馈系数
} BG_Biquad_Coef_t;

/* 双二阶滤波器状态 */
typedef struct {
    float x1, x2;               // 输入历史
    float y1, y2;               // 输出历史
} BG_Biquad_State_t;

/* EQ 效果完整结构 */
typedef struct {
    /* 参数表 */
    BG_EQ_Band_t bands[BG_EQ_MAX_BANDS];  // EQ 段参数表
    uint8_t band_count;                    // 实际使用的段数
    
    /* 内部状态 */
    BG_Biquad_Coef_t coefs[BG_EQ_MAX_BANDS];   // 滤波器系数
    BG_Biquad_State_t states[BG_EQ_MAX_BANDS]; // 滤波器状态
    
    /* 采样率 */
    uint32_t sample_rate;
} BG_EQ_Effect_t;

/**
 * EQ 效果初始化
 * @param user_data BG_EQ_Effect_t指针
 */
void bg_effect_eq_init(void *user_data);

/**
 * EQ 效果处理
 * @param input 输入音频缓冲区
 * @param output 输出音频缓冲区
 * @param samples 样本数量
 * @param user_data BG_EQ_Effect_t指针
 * @return 处理后的样本数
 */
uint16_t bg_effect_eq_process(const short *input, short *output, uint16_t samples, void *user_data);

/**
 * EQ 效果重置
 * @param user_data BG_EQ_Effect_t指针
 */
void bg_effect_eq_reset(void *user_data);

/**
 * 创建 EQ 效果实例 (辅助函数)
 * @return EQ 效果实例指针
 */
BG_EQ_Effect_t* bg_effect_eq_create(void);

/**
 * 设置 EQ 段参数
 * @param eq EQ 效果实例指针
 * @param band_index 段索引 (0 ~ BG_EQ_MAX_BANDS-1)
 * @param enabled 是否启用
 * @param type 滤波器类型
 * @param freq 中心频率 (Hz)
 * @param gain 增益 (dB)
 * @param q 品质因数
 * @return 0=成功, -1=失败
 */
int bg_effect_eq_set_band(BG_EQ_Effect_t *eq, uint8_t band_index,
                          uint8_t enabled, BG_EQ_FilterType_t type,
                          float freq, float gain, float q);

/**
 * 更新滤波器系数 (参数改变后需调用)
 * @param eq EQ 效果实例指针
 */
void bg_effect_eq_update_coefficients(BG_EQ_Effect_t *eq);

/**
 * EQ 预设: 流行音乐
 */
void bg_effect_eq_preset_pop(BG_EQ_Effect_t *eq);

/**
 * EQ 预设: 摇滚音乐
 */
void bg_effect_eq_preset_rock(BG_EQ_Effect_t *eq);

/**
 * EQ 预设: 古典音乐
 */
void bg_effect_eq_preset_classical(BG_EQ_Effect_t *eq);

/**
 * EQ 预设: 爵士音乐
 */
void bg_effect_eq_preset_jazz(BG_EQ_Effect_t *eq);

#endif /* _BG_EFFECT_EQ_H__ */
