#ifndef _BG_AUDIO_PROCESSOR_H__
#define _BG_AUDIO_PROCESSOR_H__

#include <stdint.h>
#include "bg_config.h"

/**
 * 音频处理器模块 (流水线架构)
 * 功能: 提供可扩展的音频效果处理流水线
 * 架构: 效果节点可动态注册，支持bypass控制
 */

/* 最大效果节点数量 */
#define BG_AUDIO_MAX_EFFECTS 8

/* 前置声明 */
struct BG_AudioEffect_Node;

/**
 * 音频效果处理函数类型
 * @param input 输入音频缓冲区
 * @param output 输出音频缓冲区
 * @param samples 样本数量
 * @param user_data 效果私有数据指针
 * @return 处理后的样本数
 */
typedef uint16_t (*BG_EffectProcessFunc)(const short *input, short *output, uint16_t samples, void *user_data);

/**
 * 音频效果初始化函数类型
 * @param user_data 效果私有数据指针
 */
typedef void (*BG_EffectInitFunc)(void *user_data);

/**
 * 音频效果重置函数类型
 * @param user_data 效果私有数据指针
 */
typedef void (*BG_EffectResetFunc)(void *user_data);

/**
 * 音频效果节点结构 (流水线工位)
 */
typedef struct BG_AudioEffect_Node {
    const char *name;                    // 效果名称
    uint8_t bypass;                      // bypass标志 (1=跳过, 0=处理)
    uint8_t enabled;                     // 使能标志 (1=已注册, 0=未使用)
    
    BG_EffectInitFunc init;              // 初始化函数
    BG_EffectProcessFunc process;        // 处理函数
    BG_EffectResetFunc reset;            // 重置函数
    
    void *user_data;                     // 效果私有数据
    uint16_t user_data_size;             // 私有数据大小
} BG_AudioEffect_Node_t;

/**
 * 音频处理器配置 (全局)
 */
typedef struct {
    float master_gain;           // 主增益 (0.0 ~ 2.0)
    uint8_t global_bypass;       // 全局bypass (1=所有效果直通, 0=按节点bypass)
} BG_AudioProcessor_Config_t;

/**
 * 音频处理器接口 (流水线管理)
 */
typedef struct {
    /**
     * 初始化音频处理器流水线
     */
    void (*Init)(void);
    
    /**
     * 注册音频效果到流水线
     * @param name 效果名称
     * @param init 初始化函数 (可为NULL)
     * @param process 处理函数 (必须)
     * @param reset 重置函数 (可为NULL)
     * @param user_data 效果私有数据指针
     * @param user_data_size 私有数据大小
     * @return 效果ID (0xFF表示失败)
     */
    uint8_t (*RegisterEffect)(const char *name,
                              BG_EffectInitFunc init,
                              BG_EffectProcessFunc process,
                              BG_EffectResetFunc reset,
                              void *user_data,
                              uint16_t user_data_size);
    
    /**
     * 注销音频效果
     * @param effect_id 效果ID
     * @return 0=成功, -1=失败
     */
    int (*UnregisterEffect)(uint8_t effect_id);
    
    /**
     * 设置效果bypass状态
     * @param effect_id 效果ID
     * @param bypass 1=bypass, 0=处理
     * @return 0=成功, -1=失败
     */
    int (*SetEffectBypass)(uint8_t effect_id, uint8_t bypass);
    
    /**
     * 获取效果bypass状态
     * @param effect_id 效果ID
     * @return bypass状态 (0xFF表示无效ID)
     */
    uint8_t (*GetEffectBypass)(uint8_t effect_id);
    
    /**
     * 配置处理器参数
     * @param config 配置结构体指针
     */
    void (*SetConfig)(const BG_AudioProcessor_Config_t *config);
    
    /**
     * 获取当前配置
     * @param config 输出配置结构体指针
     */
    void (*GetConfig)(BG_AudioProcessor_Config_t *config);
    
    /**
     * 流水线处理 (按注册顺序依次处理)
     * @param input 输入音频缓冲区
     * @param output 输出音频缓冲区
     * @param samples 样本数量
     * @return 处理后的样本数
     */
    uint16_t (*Process)(const short *input, short *output, uint16_t samples);
    
    /**
     * 重置所有效果状态
     */
    void (*Reset)(void);
    
    /**
     * 获取已注册效果数量
     * @return 效果数量
     */
    uint8_t (*GetEffectCount)(void);
    
    /**
     * 获取效果节点信息 (只读)
     * @param effect_id 效果ID
     * @return 效果节点指针 (NULL表示无效ID)
     */
    const BG_AudioEffect_Node_t* (*GetEffectInfo)(uint8_t effect_id);
    
} BG_AudioProcessor_t;

/* 导出接口实例 */
extern BG_AudioProcessor_t BG_AudioProcessor;

#endif /* _BG_AUDIO_PROCESSOR_H__ */
