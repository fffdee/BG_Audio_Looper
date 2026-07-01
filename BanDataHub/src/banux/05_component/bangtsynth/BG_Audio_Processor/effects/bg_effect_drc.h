#ifndef _BG_EFFECT_DRC_H__
#define _BG_EFFECT_DRC_H__

#include <stdint.h>
#include "bg_config.h"

/**
 * DRC (动态范围压缩) 音频效果
 * 用途: 防止复音叠加导致失真,平滑动态范围
 */

/* DRC配置参数 */
typedef struct {
    float threshold;         // 压缩阈值 (0.0 ~ 1.0, 建议0.7)
    float ratio;             // 压缩比 (1.0 ~ 20.0, 建议4.0)
    float attack_ms;         // 起音时间 (ms, 建议1.0)
    float release_ms;        // 释音时间 (ms, 建议50.0)
} BG_DRC_Config_t;

/* DRC内部状态 */
typedef struct {
    float envelope;          // 包络跟随器
    float gain_reduction;    // 当前增益衰减
} BG_DRC_State_t;

/* DRC效果完整结构 */
typedef struct {
    BG_DRC_Config_t config;  // 配置参数
    BG_DRC_State_t state;    // 运行状态
} BG_DRC_Effect_t;

/**
 * DRC效果初始化
 * @param user_data BG_DRC_Effect_t指针
 */
void bg_effect_drc_init(void *user_data);

/**
 * DRC效果处理
 * @param input 输入音频缓冲区
 * @param output 输出音频缓冲区
 * @param samples 样本数量
 * @param user_data BG_DRC_Effect_t指针
 * @return 处理后的样本数
 */
uint16_t bg_effect_drc_process(const short *input, short *output, uint16_t samples, void *user_data);

/**
 * DRC效果重置
 * @param user_data BG_DRC_Effect_t指针
 */
void bg_effect_drc_reset(void *user_data);

/**
 * 创建DRC效果实例 (辅助函数)
 * @param threshold 压缩阈值
 * @param ratio 压缩比
 * @param attack_ms 起音时间
 * @param release_ms 释音时间
 * @return DRC效果实例指针
 */
BG_DRC_Effect_t* bg_effect_drc_create(float threshold, float ratio, 
                                       float attack_ms, float release_ms);

#endif /* _BG_EFFECT_DRC_H__ */
