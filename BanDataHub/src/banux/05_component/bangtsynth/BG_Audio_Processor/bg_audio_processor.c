#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "bg_audio_processor.h"
#include "bg_log.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* 流水线效果节点数组 */
static BG_AudioEffect_Node_t g_effects[BG_AUDIO_MAX_EFFECTS];

/* 全局配置 */
static BG_AudioProcessor_Config_t g_config = {
    .master_gain = 1.0f,
    .global_bypass = 0
};

/* 临时处理缓冲区 (用于流水线传递) */
static short g_temp_buffer[BG_BUFFER_SIZE];

/* 内部函数声明 */
static void processor_init(void);
static uint8_t processor_register_effect(const char *name,
                                          BG_EffectInitFunc init,
                                          BG_EffectProcessFunc process,
                                          BG_EffectResetFunc reset,
                                          void *user_data,
                                          uint16_t user_data_size);
static int processor_unregister_effect(uint8_t effect_id);
static int processor_set_effect_bypass(uint8_t effect_id, uint8_t bypass);
static uint8_t processor_get_effect_bypass(uint8_t effect_id);
static void processor_set_config(const BG_AudioProcessor_Config_t *config);
static void processor_get_config(BG_AudioProcessor_Config_t *config);
static uint16_t processor_process(const short *input, short *output, uint16_t samples);
static void processor_reset(void);
static uint8_t processor_get_effect_count(void);
static const BG_AudioEffect_Node_t* processor_get_effect_info(uint8_t effect_id);

/* 接口实例 */
BG_AudioProcessor_t BG_AudioProcessor = {
    .Init = processor_init,
    .RegisterEffect = processor_register_effect,
    .UnregisterEffect = processor_unregister_effect,
    .SetEffectBypass = processor_set_effect_bypass,
    .GetEffectBypass = processor_get_effect_bypass,
    .SetConfig = processor_set_config,
    .GetConfig = processor_get_config,
    .Process = processor_process,
    .Reset = processor_reset,
    .GetEffectCount = processor_get_effect_count,
    .GetEffectInfo = processor_get_effect_info
};

/**
 * 初始化音频处理器流水线
 */
static void processor_init(void)
{
    /* 清空所有效果节点 */
    memset(g_effects, 0, sizeof(g_effects));
    
    BG_LOG_I(BG_LOG_TAG_AUDIO_PROC, "Pipeline initialized - Max effects: %d\n", BG_AUDIO_MAX_EFFECTS);
}

/**
 * 注册音频效果到流水线
 */
static uint8_t processor_register_effect(const char *name,
                                          BG_EffectInitFunc init,
                                          BG_EffectProcessFunc process,
                                          BG_EffectResetFunc reset,
                                          void *user_data,
                                          uint16_t user_data_size)
{
    uint8_t i;
    
    /* 参数检查 */
    if (!name || !process) {
        BG_LOG_E(BG_LOG_TAG_AUDIO_PROC, "Register failed: invalid parameters\n");
        return 0xFF;
    }
    
    /* 查找空闲槽位 */
    for (i = 0; i < BG_AUDIO_MAX_EFFECTS; i++) {
        if (!g_effects[i].enabled) {
            /* 填充效果节点 */
            g_effects[i].name = name;
            g_effects[i].bypass = 0;  // 默认不bypass
            g_effects[i].enabled = 1;
            g_effects[i].init = init;
            g_effects[i].process = process;
            g_effects[i].reset = reset;
            g_effects[i].user_data = user_data;
            g_effects[i].user_data_size = user_data_size;
            
            /* 调用初始化函数 */
            if (init && user_data) {
                init(user_data);
            }
            
            BG_LOG_I(BG_LOG_TAG_AUDIO_PROC, "Effect registered: ID=%d Name=%s\n", i, name);
            return i;
        }
    }
    
    BG_LOG_E(BG_LOG_TAG_AUDIO_PROC, "Register failed: pipeline full\n");
    return 0xFF;
}

/**
 * 注销音频效果
 */
static int processor_unregister_effect(uint8_t effect_id)
{
    if (effect_id >= BG_AUDIO_MAX_EFFECTS || !g_effects[effect_id].enabled) {
        return -1;
    }
    
    BG_LOG_I(BG_LOG_TAG_AUDIO_PROC, "Effect unregistered: ID=%d Name=%s\n", 
           effect_id, g_effects[effect_id].name);
    
    /* 清空节点 */
    memset(&g_effects[effect_id], 0, sizeof(BG_AudioEffect_Node_t));
    
    return 0;
}

/**
 * 设置效果bypass状态
 */
static int processor_set_effect_bypass(uint8_t effect_id, uint8_t bypass)
{
    if (effect_id >= BG_AUDIO_MAX_EFFECTS || !g_effects[effect_id].enabled) {
        return -1;
    }
    
    g_effects[effect_id].bypass = bypass ? 1 : 0;
    
    BG_LOG_D(BG_LOG_TAG_AUDIO_PROC, "Effect bypass set: ID=%d Name=%s Bypass=%s\n",
           effect_id, g_effects[effect_id].name, bypass ? "ON" : "OFF");
    
    return 0;
}

/**
 * 获取效果bypass状态
 */
static uint8_t processor_get_effect_bypass(uint8_t effect_id)
{
    if (effect_id >= BG_AUDIO_MAX_EFFECTS || !g_effects[effect_id].enabled) {
        return 0xFF;
    }
    
    return g_effects[effect_id].bypass;
}

/**
 * 配置处理器参数
 */
static void processor_set_config(const BG_AudioProcessor_Config_t *config)
{
    if (config) {
        memcpy(&g_config, config, sizeof(BG_AudioProcessor_Config_t));
        BG_LOG_D(BG_LOG_TAG_AUDIO_PROC, "Config updated - Gain:%.2f GlobalBypass:%s\n",
               g_config.master_gain, g_config.global_bypass ? "ON" : "OFF");
    }
}

/**
 * 获取当前配置
 */
static void processor_get_config(BG_AudioProcessor_Config_t *config)
{
    if (config) {
        memcpy(config, &g_config, sizeof(BG_AudioProcessor_Config_t));
    }
}

/**
 * 流水线处理 (按注册顺序依次处理)
 */
static uint16_t processor_process(const short *input, short *output, uint16_t samples)
{
    uint16_t i;
    const short *current_input;
    short *current_output;
    uint8_t active_effects;
    
    if (!input || !output || samples == 0) {
        return 0;
    }
    
    /* 全局bypass模式: 仅应用增益 */
    if (g_config.global_bypass) {
        for (i = 0; i < samples; i++) {
            float processed = (float)input[i] * g_config.master_gain;
            if (processed > 32767.0f) processed = 32767.0f;
            if (processed < -32768.0f) processed = -32768.0f;
            output[i] = (short)processed;
        }
        return samples;
    }
    
    /* 流水线处理 */
    current_input = input;
    current_output = output;
    active_effects = 0;
    
    for (i = 0; i < BG_AUDIO_MAX_EFFECTS; i++) {
        if (!g_effects[i].enabled) {
            continue;
        }
        
        /* Bypass检查: 跳过该效果,直接复制数据 */
        if (g_effects[i].bypass) {
            /* 如果是第一个效果或奇数个效果,需要复制到输出 */
            if (active_effects % 2 == 0) {
                if (current_input != current_output) {
                    memcpy(current_output, current_input, samples * sizeof(short));
                }
            }
        } else {
            /* 处理效果 */
            uint16_t processed = g_effects[i].process(current_input, current_output, 
                                                       samples, g_effects[i].user_data);
            if (processed != samples) {
                BG_LOG_E(BG_LOG_TAG_AUDIO_PROC, "Warning: Effect %d processed %d/%d samples\n",
                       i, processed, samples);
            }
        }
        
        /* 切换缓冲区: 交替使用output和temp_buffer */
        active_effects++;
        if (active_effects % 2 == 1) {
            current_input = output;
            current_output = g_temp_buffer;
        } else {
            current_input = g_temp_buffer;
            current_output = output;
        }
    }
    
    /* 如果最终数据在temp_buffer,复制回output */
    if (active_effects > 0 && active_effects % 2 == 1 && current_input == g_temp_buffer) {
        memcpy(output, g_temp_buffer, samples * sizeof(short));
    }
    
    /* 如果没有任何效果,直接复制输入 */
    if (active_effects == 0) {
        if (input != output) {
            memcpy(output, input, samples * sizeof(short));
        }
    }
    
    /* 应用主增益 (最后一步) */
    if (g_config.master_gain != 1.0f) {
        for (i = 0; i < samples; i++) {
            float processed = (float)output[i] * g_config.master_gain;
            if (processed > 32767.0f) processed = 32767.0f;
            if (processed < -32768.0f) processed = -32768.0f;
            output[i] = (short)processed;
        }
    }
    
    return samples;
}

/**
 * 重置所有效果状态
 */
static void processor_reset(void)
{
    uint8_t i;
    
    for (i = 0; i < BG_AUDIO_MAX_EFFECTS; i++) {
        if (g_effects[i].enabled && g_effects[i].reset && g_effects[i].user_data) {
            g_effects[i].reset(g_effects[i].user_data);
        }
    }
    BG_LOG_I(BG_LOG_TAG_AUDIO_PROC, "All effects reset\n");
}

/**
 * 获取已注册效果数量
 */
static uint8_t processor_get_effect_count(void)
{
    uint8_t count = 0;
    uint8_t i;
    
    for (i = 0; i < BG_AUDIO_MAX_EFFECTS; i++) {
        if (g_effects[i].enabled) {
            count++;
        }
    }
    return count;
}

/**
 * 获取效果节点信息 (只读)
 */
static const BG_AudioEffect_Node_t* processor_get_effect_info(uint8_t effect_id)
{
    if (effect_id >= BG_AUDIO_MAX_EFFECTS || !g_effects[effect_id].enabled) {
        return NULL;
    }
    
    return &g_effects[effect_id];
}

#endif /* BANGTSYNTH_EN */
