/**
 *****************************************************************************
 * @file     shell_cmd_graph.c
 * @author   BG Card Team
 * @version  V1.2.0
 * @date     06-January-2026
 * @brief    音频效果图Shell命令模块实现 - 支持ID/名称索引、参数校验、快照功能
 *****************************************************************************
 */

#include "shell_cmd_graph.h"
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "sys_param.h"     /* For g_sys_param */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "audio_effect.h"  /* For AudioEffectReverbConfig */
#include "ctrlvars.h"      /* For gCtrlVars */

/*******************************************************************************
 * 参数范围定义 (用于校验)
 ******************************************************************************/
typedef struct {
    const char *name;       /* 参数名 */
    int32_t min_val;        /* 最小值 */
    int32_t max_val;        /* 最大值 */
    const char *unit;       /* 单位 */
} ParamRange_t;

/* 各效果器参数范围表 */
static const ParamRange_t g_ReverbParamRange[] = {
    { "room",  0, 100, "%" },
    { "damp",  0, 100, "%" },
    { "wet",   0, 100, "%" },
    { NULL, 0, 0, NULL }
};

static const ParamRange_t g_DrcParamRange[] = {
    { "threshold", -60, 0, "dB" },
    { "ratio",     1, 20, "" },
    { "attack",    1, 500, "ms" },
    { "release",   10, 2000, "ms" },
    { NULL, 0, 0, NULL }
};

static const ParamRange_t g_EqParamRange[] = {
        // 每段10个参数（band0~band9）
        { "band0", -12, 12, "dB" },
        { "band0_type", 0, 6, "type" },
        { "band0_f0", 20, 20000, "Hz" },
        { "band0_Q", 10, 1000, "0.01" },
        { "band0_enable", 0, 1, "bool" },
        { "band1", -12, 12, "dB" },
        { "band1_type", 0, 6, "type" },
        { "band1_f0", 20, 20000, "Hz" },
        { "band1_Q", 10, 1000, "0.01" },
        { "band1_enable", 0, 1, "bool" },
        { "band2", -12, 12, "dB" },
        { "band2_type", 0, 6, "type" },
        { "band2_f0", 20, 20000, "Hz" },
        { "band2_Q", 10, 1000, "0.01" },
        { "band2_enable", 0, 1, "bool" },
        { "band3", -12, 12, "dB" },
        { "band3_type", 0, 6, "type" },
        { "band3_f0", 20, 20000, "Hz" },
        { "band3_Q", 10, 1000, "0.01" },
        { "band3_enable", 0, 1, "bool" },
        { "band4", -12, 12, "dB" },
        { "band4_type", 0, 6, "type" },
        { "band4_f0", 20, 20000, "Hz" },
        { "band4_Q", 10, 1000, "0.01" },
        { "band4_enable", 0, 1, "bool" },
        { "band5", -12, 12, "dB" },
        { "band5_type", 0, 6, "type" },
        { "band5_f0", 20, 20000, "Hz" },
        { "band5_Q", 10, 1000, "0.01" },
        { "band5_enable", 0, 1, "bool" },
        { "band6", -12, 12, "dB" },
        { "band6_type", 0, 6, "type" },
        { "band6_f0", 20, 20000, "Hz" },
        { "band6_Q", 10, 1000, "0.01" },
        { "band6_enable", 0, 1, "bool" },
        { "band7", -12, 12, "dB" },
        { "band7_type", 0, 6, "type" },
        { "band7_f0", 20, 20000, "Hz" },
        { "band7_Q", 10, 1000, "0.01" },
        { "band7_enable", 0, 1, "bool" },
        { "band8", -12, 12, "dB" },
        { "band8_type", 0, 6, "type" },
        { "band8_f0", 20, 20000, "Hz" },
        { "band8_Q", 10, 1000, "0.01" },
        { "band8_enable", 0, 1, "bool" },
        { "band9", -12, 12, "dB" },
        { "band9_type", 0, 6, "type" },
        { "band9_f0", 20, 20000, "Hz" },
        { "band9_Q", 10, 1000, "0.01" },
        { "band9_enable", 0, 1, "bool" },
        // 全局参数
        { "pregain", -24, 24, "dB" },
        { "filter_count", 1, 10, "count" },
        { "channel", 0, 1, "ch" },
        { NULL, 0, 0, NULL }
};

static const ParamRange_t g_GainParamRange[] = {
    { "gain", -60, 20, "dB" },
    { NULL, 0, 0, NULL }
};

static const ParamRange_t g_DelayParamRange[] = {
    { "time",     10, 1000, "ms" },
    { "feedback", 0, 100, "%" },
    { "wet",      0, 100, "%" },
    { NULL, 0, 0, NULL }
};

static const ParamRange_t g_ExpanderParamRange[] = {
    { "threshold", -80, 0, "dB" },
    { "ratio",     1, 10, "" },
    { NULL, 0, 0, NULL }
};

static const ParamRange_t g_MixerParamRange[] = {
    { "in0_gain", -60, 20, "dB" },
    { "in1_gain", -60, 20, "dB" },
    { "in2_gain", -60, 20, "dB" },
    { "in3_gain", -60, 20, "dB" },
    { NULL, 0, 0, NULL }
};

/*******************************************************************************
 * 参数快照结构 (用于保存/恢复)
 ******************************************************************************/
#define SNAPSHOT_MAX_NODES   16
#define SNAPSHOT_MAX_SLOTS   4

typedef struct {
    uint8_t  node_id;
    bool     enabled;
    bool     bypass;
    uint8_t  param_data[32];  /* EffectParams_t raw data */
} NodeSnapshot_t;

typedef struct {
    bool          valid;
    char          name[16];
    uint8_t       node_count;
    NodeSnapshot_t nodes[SNAPSHOT_MAX_NODES];
} GraphSnapshot_t;

static GraphSnapshot_t g_Snapshots[SNAPSHOT_MAX_SLOTS];

/*******************************************************************************
 * 内部辅助函数
 ******************************************************************************/

/**
 * @brief 获取节点类型对应的参数范围表
 */
static const ParamRange_t* GetParamRangeTable(EffectNodeType_t type)
{
    switch (type) {
        case EFFECT_NODE_TYPE_EFFECT_REVERB:   return g_ReverbParamRange;
        case EFFECT_NODE_TYPE_EFFECT_DRC:      return g_DrcParamRange;
        case EFFECT_NODE_TYPE_EFFECT_EQ:       return g_EqParamRange;
        case EFFECT_NODE_TYPE_EFFECT_GAIN:     return g_GainParamRange;
        case EFFECT_NODE_TYPE_EFFECT_DELAY:    return g_DelayParamRange;
        case EFFECT_NODE_TYPE_EFFECT_EXPANDER: return g_ExpanderParamRange;
        case EFFECT_NODE_TYPE_MIXER:           return g_MixerParamRange;
        default:                        return NULL;
    }
}

/**
 * @brief 校验参数值是否在有效范围内
 * @return 0成功，-1参数名无效，-2值超范围
 */
static int ValidateParam(EffectNode_t *node, const char *param_name, int32_t value)
{
    const ParamRange_t *table;
    const ParamRange_t *p;
    
    if (!node || !param_name) return -1;
    
    table = GetParamRangeTable(node->type);
    if (!table) return 0; /* 无参数表，跳过校验 */
    
    for (p = table; p->name != NULL; p++) {
        if (strcmp(p->name, param_name) == 0) {
            if (value < p->min_val || value > p->max_val) {
                Shell_Printf("WARN: %s out of range [%ld~%ld]%s\n", 
                            param_name, (long)p->min_val, (long)p->max_val, p->unit);
                return -2;
            }
            return 0;
        }
    }
    
    return -1; /* 参数名无效 */
}

/**
 * @brief 打印节点可用参数列表
 */
static void PrintAvailableParams(EffectNode_t *node)
{
    const ParamRange_t *table;
    const ParamRange_t *p;
    
    if (!node) return;
    
    table = GetParamRangeTable(node->type);
    if (!table) {
        Shell_Printf("  (no parameters)\n");
        return;
    }
    
    Shell_Printf("Available parameters:\n");
    for (p = table; p->name != NULL; p++) {
        Shell_Printf("  %-12s  [%ld~%ld] %s\n", 
                    p->name, (long)p->min_val, (long)p->max_val, p->unit);
    }
}

/**
 * @brief 通过ID或名称查找节点
 * @param id_or_name 节点ID（数字）或名称（字符串）
 * @return 节点指针，未找到返回NULL
 */
static EffectNode_t* FindNode(const char *id_or_name)
{
    EffectNode_t *node = NULL;
    
    if (!id_or_name) return NULL;
    
    /* 先尝试作为数字ID解析 */
    if (id_or_name[0] >= '0' && id_or_name[0] <= '9') {
        uint8_t id = (uint8_t)atoi(id_or_name);
        node = EffectGraph_FindNodeById(id);
    }
    
    /* 如果ID查找失败，再尝试按名称查找 */
    if (!node) {
        node = EffectGraph_FindNodeByName(id_or_name);
    }
    
    return node;
}

/**
 * @brief 将EQ运行时节点参数同步到sys_param
 * @param node EQ运行时节点指针
 * 
 * EQ参数格式(88字节):
 *   [0-9]:    10个频段的gain值 (int8_t, 1字节/段)
 *   [10-49]:  10个频段的f0频率 (uint32_t, 4字节/段, 小端)
 *   [50-69]:  10个频段的Q值 (uint16_t, 2字节/段, 小端)
 *   [70-79]:  10个频段的type类型 (uint8_t, 1字节/段)
 *   [80-89]:  10个频段的enable标志 (uint8_t, 1字节/段)
 */
void SyncEQNodeToSysParam(EffectNode_t *node)
{
    extern SysParam_t g_sys_param;
    GraphNode_t *sys_node;
    uint8_t band;
    
    if (!node || node->type != EFFECT_NODE_TYPE_EFFECT_EQ) {
        return;
    }
    
    /* 找到sys_param中对应的节点 */
    if (node->id >= MAX_GRAPH_NODES) {
        return;
    }
    
    sys_node = &g_sys_param.audio_chain.node_pool[node->id];
    
    /* 清空params数组 */
    memset(sys_node->params, 0, sizeof(sys_node->params));
    
    /* 按正确格式序列化EQ参数到params数组 */
    for (band = 0; band < 10; band++) {
        /* gain (1 byte) at [0-9] */
        sys_node->params[band] = (uint8_t)node->params.eq.band_gains[band];
        
        /* f0 (4 bytes, little endian) at [10-49] */
        uint32_t f0 = node->params.eq.band_f0[band];
        sys_node->params[10 + band * 4 + 0] = (uint8_t)(f0 & 0xFF);
        sys_node->params[10 + band * 4 + 1] = (uint8_t)((f0 >> 8) & 0xFF);
        sys_node->params[10 + band * 4 + 2] = (uint8_t)((f0 >> 16) & 0xFF);
        sys_node->params[10 + band * 4 + 3] = (uint8_t)((f0 >> 24) & 0xFF);
        
        /* Q (2 bytes, little endian) at [50-69] */
        uint32_t q = node->params.eq.band_Q[band];
        sys_node->params[50 + band * 2 + 0] = (uint8_t)(q & 0xFF);
        sys_node->params[50 + band * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
        
        /* type (1 byte) at [70-79] */
        sys_node->params[70 + band] = node->params.eq.band_types[band];
        
        /* enable (1 byte) at [80-89] */
        sys_node->params[80 + band] = node->params.eq.band_enables[band];
    }
}

/**
 * @brief 打印节点的所有参数
 * @param node 节点指针
 */
static void PrintNodeParams(EffectNode_t *node)
{
    if (!node) return;
    
    Shell_Printf("\n=== Node[%d]: %s ===\n", node->id, node->name);
    Shell_Printf("Status: %s %s\n", 
                 node->enabled ? "Enabled" : "Disabled",
                 node->bypass ? "[Bypass]" : "");
    
    switch (node->type) {
        case EFFECT_NODE_TYPE_EFFECT_REVERB:
            Shell_Printf("Type: REVERB\n");
            Shell_Printf("  room   = %d (0-100)\n", node->params.reverb.room_size);
            Shell_Printf("  damp   = %d (0-100)\n", node->params.reverb.damping);
            Shell_Printf("  wet    = %d (0-100)\n", node->params.reverb.wet_dry);
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DRC:
            Shell_Printf("Type: DRC\n");
            Shell_Printf("  threshold = %d dB\n", node->params.drc.threshold);
            Shell_Printf("  ratio     = %d\n", node->params.drc.ratio);
            Shell_Printf("  attack    = %d ms\n", node->params.drc.attack);
            Shell_Printf("  release   = %d ms\n", node->params.drc.release);
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_EQ:
            Shell_Printf("Type: EQ (configured: %d bands, max: 10)\n", node->params.eq.band_count);
            Shell_Printf("  pregain = %d dB\n", node->params.eq.pregain);
            {
                uint8_t i;
                /* 显示所有 10 个 band 的完整参数 */
                for (i = 0; i < 10; i++) {
                    /* 如果 band 被启用，显示完整参数；否则只显示状态 */
                    if (node->params.eq.band_enables[i]) {
                        Shell_Printf("  band%d: %s | gain=%d dB, f0=%lu Hz, Q=%.2f, type=%d\n", 
                                    i,
                                    "ON ",
                                    node->params.eq.band_gains[i],
                                    (unsigned long)node->params.eq.band_f0[i],
                                    (float)node->params.eq.band_Q[i] / 100.0f,
                                    node->params.eq.band_types[i]);
                    } else {
                        Shell_Printf("  band%d: OFF\n", i);
                    }
                }
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_GAIN:
            Shell_Printf("Type: GAIN\n");
            Shell_Printf("  gain = %d dB\n", node->params.gain.gain_db);
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DELAY:
            Shell_Printf("Type: DELAY\n");
            Shell_Printf("  time     = %d ms\n", node->params.delay.delay_ms);
            Shell_Printf("  feedback = %d (0-100)\n", node->params.delay.feedback);
            Shell_Printf("  wet      = %d (0-100)\n", node->params.delay.wet_dry);
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_EXPANDER:
            Shell_Printf("Type: EXPANDER\n");
            Shell_Printf("  threshold = %d dB\n", node->params.expander.threshold);
            Shell_Printf("  ratio     = %d\n", node->params.expander.ratio);
            break;
            
        case EFFECT_NODE_TYPE_MIXER:
            Shell_Printf("Type: MIXER (%d inputs)\n", node->params.mixer.input_count);
            {
                uint8_t i;
                for (i = 0; i < node->params.mixer.input_count && i < EFFECT_GRAPH_MAX_INPUTS; i++) {
                    Shell_Printf("  in%d_gain = %d dB\n", i, node->params.mixer.input_gains[i]);
                }
            }
            break;
            
        case EFFECT_NODE_TYPE_SOURCE_ADC0:
        case EFFECT_NODE_TYPE_SOURCE_ADC1:
        case EFFECT_NODE_TYPE_SOURCE_USB_IN:
        case EFFECT_NODE_TYPE_SOURCE_BT_IN:
            Shell_Printf("Type: SOURCE (no params)\n");
            break;
            
        case EFFECT_NODE_TYPE_SOURCE_METRONOME:
            Shell_Printf("Type: METRONOME SOURCE (no params)\n");
            break;
            
        case EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY:
            Shell_Printf("Type: LOOPER PLAYBACK SOURCE (no params)\n");
            break;
            
        case EFFECT_NODE_TYPE_SINK_DAC0:
        case EFFECT_NODE_TYPE_SINK_USB_OUT:
            Shell_Printf("Type: SINK (no params)\n");
            break;
            
        case EFFECT_NODE_TYPE_SINK_LOOPER_RECORD:
            Shell_Printf("Type: LOOPER RECORD SINK (no params)\n");
            break;
            
        default:
            Shell_Printf("Type: Unknown (%d)\n", node->type);
            break;
    }
    Shell_Printf("========================\n\n");
}

/**
 * @brief 重建EQ的filter_params数组并应用滤波器配置
 * 
 * 关键！filter_params是压缩数组，只包含enable=1的频段
 * 参考 communication.c 的正确实现
 * 
 * @param target_eq 目标EQ单元
 * @param node 对应的效果节点（用于同步node参数）
 */
void RebuildAndApplyEQFilter(EQUnit *target_eq, EffectNode_t *node)
{
    int i;
    extern ControlVariablesContext gCtrlVars;
    
    if (!target_eq) return;
    
    /* 关键步骤1: 重置filter_count为0 */
    target_eq->filter_count = 0;
    
    /* 关键步骤2: 遍历所有频段，重建压缩的filter_params数组 */
    for (i = 0; i < 10; i++) {
        if (target_eq->eq_params[i].enable) {
            /* 参数有效性检查 - 防止f0=0或Q=0导致除零错误 */
            uint16_t f0 = target_eq->eq_params[i].f0;
            int16_t Q = target_eq->eq_params[i].Q;
            int16_t type = target_eq->eq_params[i].type;
            
            /* 检查f0有效性：必须大于0 */
            if (f0 == 0) {
                f0 = 1000;  /* 默认1000Hz */
                target_eq->eq_params[i].f0 = f0;
                Shell_Printf("[EQ] WARN: band%d f0=0 invalid, set to 1000Hz\n", i);
            }
            
            /* 检查Q有效性：Q6.10格式，必须大于0 */
            if (Q <= 0) {
                Q = 724;  /* 默认Q=0.707，Q6.10格式=724 */
                target_eq->eq_params[i].Q = Q;
                Shell_Printf("[EQ] WARN: band%d Q<=0 invalid, set to 724 (Q=0.707)\n", i);
            }
            
            /* 检查type有效性：类型必须在合理范围内 */
            if (type < 0 || type > 6) {
                type = 0;  /* 默认PEAKING */
                target_eq->eq_params[i].type = type;
                Shell_Printf("[EQ] WARN: band%d type=%d invalid, set to PEAKING(0)\n", i, type);
            }
            
            if (target_eq->filter_params) {
                target_eq->filter_params[target_eq->filter_count].Q    = Q;
                target_eq->filter_params[target_eq->filter_count].f0   = f0;
                target_eq->filter_params[target_eq->filter_count].gain = target_eq->eq_params[i].gain;
                target_eq->filter_params[target_eq->filter_count].type = type;
            }
            target_eq->filter_count++;
        }
    }
    
    /* 同步filter_count到节点参数 */
    if (node) {
        node->params.eq.band_count = target_eq->filter_count;
    }
    
    /* 关键步骤3: 调用AudioEffectEQFilterClearBufConfig（清除delay buffer + 重新配置） */
    if (target_eq->filter_count > 0) {
        target_eq->enable = 1;
        /* 确保channel正确 */
        if (target_eq->channel == 0) {
            target_eq->channel = 2;
        }
        /* 确保EQ已初始化 */
        if (target_eq->ct == NULL) {
            Shell_Printf("[EQ] Initializing EQ context (ch=%d)...\n", target_eq->channel);
            AudioEffectEQInit(target_eq, target_eq->channel, gCtrlVars.sample_rate);
        }
        if (target_eq->ct != NULL) {
            if (target_eq == &gCtrlVars.music_out_eq_unit) {
                #if CFG_AUDIO_EFFECT_MUSIC_OUT_EQ_EN
                AudioEffectEQFilterClearBufConfig(target_eq, gCtrlVars.sample_rate);
                #endif
            } else {
                #if CFG_AUDIO_EFFECT_MIC_OUT_EQ_EN
                AudioEffectEQFilterClearBufConfig(target_eq, gCtrlVars.sample_rate);
                #endif
            }
        } else {
            Shell_Printf("[EQ] WARN: ct is NULL after init attempt!\n");
        }
    } else {
        target_eq->enable = 0;
        Shell_Printf("[EQ] No enabled bands, EQ disabled\n");
    }
}

/**
 * @brief 设置节点参数 (通用函数，带校验)
 * @param node 节点指针
 * @param param_name 参数名
 * @param value 参数值
 * @param validate 是否校验范围
 * @return 0成功，-1参数无效，-2值超范围
 */
static int SetNodeParam(EffectNode_t *node, const char *param_name, int32_t value)
{
    int ret;
    
    if (!node || !param_name) return -1;
    
    /* 参数范围校验 */
    ret = ValidateParam(node, param_name, value);
    if (ret == -1) {
        Shell_Printf("ERROR: Unknown param '%s' for node type\n", param_name);
        PrintAvailableParams(node);
        return -1;
    }
    /* ret == -2 只是警告，继续设置 */
    
    switch (node->type) {
        case EFFECT_NODE_TYPE_EFFECT_REVERB:
            if (strcmp(param_name, "room") == 0) {
                node->params.reverb.room_size = (uint8_t)value;
                /* 同步到全局混响单元 */
                extern ControlVariablesContext gCtrlVars;
                gCtrlVars.reverb_unit.roomsize_scale = (int32_t)value;
                AudioEffectReverbConfig(&gCtrlVars.reverb_unit);
            } else if (strcmp(param_name, "damp") == 0) {
                node->params.reverb.damping = (uint8_t)value;
                /* 同步到全局混响单元 */
                extern ControlVariablesContext gCtrlVars;
                gCtrlVars.reverb_unit.damping_scale = (int32_t)value;
                AudioEffectReverbConfig(&gCtrlVars.reverb_unit);
            } else if (strcmp(param_name, "wet") == 0) {
                node->params.reverb.wet_dry = (uint8_t)value;
                /* 同步到全局混响单元 */
                extern ControlVariablesContext gCtrlVars;
                gCtrlVars.reverb_unit.wet_scale = (int32_t)value;
                AudioEffectReverbConfig(&gCtrlVars.reverb_unit);
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DRC:
            if (strcmp(param_name, "threshold") == 0) {
                node->params.drc.threshold = (int16_t)value;
                /* 同步到全局DRC单元 */
                extern ControlVariablesContext gCtrlVars;
                gCtrlVars.mic_drc_unit.threshold[0] = (int16_t)value;
                #if CFG_AUDIO_EFFECT_MIC_DRC_EN
                AudioEffectDRCConfig(&gCtrlVars.mic_drc_unit, 2, 48000);
                #endif
            } else if (strcmp(param_name, "ratio") == 0) {
                node->params.drc.ratio = (uint8_t)value;
                /* 同步到全局DRC单元 */
                extern ControlVariablesContext gCtrlVars;
                gCtrlVars.mic_drc_unit.ratio[0] = (uint8_t)value;
                #if CFG_AUDIO_EFFECT_MIC_DRC_EN
                AudioEffectDRCConfig(&gCtrlVars.mic_drc_unit, 2, 48000);
                #endif
            } else if (strcmp(param_name, "attack") == 0) {
                node->params.drc.attack = (uint8_t)value;
                /* 同步到全局DRC单元 */
                extern ControlVariablesContext gCtrlVars;
                gCtrlVars.mic_drc_unit.attack_tc[0] = (uint8_t)value;
                #if CFG_AUDIO_EFFECT_MIC_DRC_EN
                AudioEffectDRCConfig(&gCtrlVars.mic_drc_unit, 2, 48000);
                #endif
            } else if (strcmp(param_name, "release") == 0) {
                node->params.drc.release = (uint8_t)value;
                /* 同步到全局DRC单元 */
                extern ControlVariablesContext gCtrlVars;
                gCtrlVars.mic_drc_unit.release_tc[0] = (uint8_t)value;
                #if CFG_AUDIO_EFFECT_MIC_DRC_EN
                AudioEffectDRCConfig(&gCtrlVars.mic_drc_unit, 2, 48000);
                #endif
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_EQ:
            /* 支持 band<n>、band<n>_type、band<n>_f0、band<n>_Q、band<n>_enable、pregain、filter_count、channel */
            if (strncmp(param_name, "band", 4) == 0 && param_name[4] >= '0' && param_name[4] <= '9') {
                uint8_t band = param_name[4] - '0';
                const char *suffix = param_name + 5;
                extern ControlVariablesContext gCtrlVars;
                /* 根据节点ID选择对应的独立EQ单元 */
                EQUnit *target_eq;
                switch (node->id) {
                    case NODE_ID_EQ_GUITAR_L: target_eq = &gCtrlVars.eq_guitar_l_unit; break;
                    case NODE_ID_EQ_GUITAR_R: target_eq = &gCtrlVars.eq_guitar_r_unit; break;
                    case NODE_ID_EQ_MIC_L:    target_eq = &gCtrlVars.eq_mic_l_unit; break;
                    case NODE_ID_EQ_MIC_R:    target_eq = &gCtrlVars.eq_mic_r_unit; break;
                    case NODE_ID_USB_BT_EQ:   target_eq = &gCtrlVars.music_out_eq_unit; break;
                    default:
                        Shell_Printf("ERROR: Unknown EQ node ID %d\n", node->id);
                        return -1;
                }
                if (band >= 10) return -1;
                
                if (*suffix == '\0') {
                    /* 增益：接收的是×10的整数（例如40表示4.0dB），需要先除以10 */
                    int32_t gain_db = (int32_t)value / 10;  /* 将40转换为4 */
                    node->params.eq.band_gains[band] = (int8_t)gain_db;
                    target_eq->eq_params[band].gain = gain_db * 256;  /* SDK格式：dB×256 */
                    Shell_Printf("[EQ:%s] band%d gain = %d (%.1fdB)\n", node->name, band, 
                                (int8_t)gain_db, (float)value / 10.0f);
                } else if (strcmp(suffix, "_type") == 0) {
                    /* 类型：同时更新节点参数、eq_params */
                    node->params.eq.band_types[band] = (uint8_t)value;
                    target_eq->eq_params[band].type = value;
                    Shell_Printf("[EQ:%s] band%d type = %ld\n", node->name, band, value);
                } else if (strcmp(suffix, "_f0") == 0) {
                    /* 中心频率：同时更新节点参数、eq_params */
                    node->params.eq.band_f0[band] = (uint32_t)value;
                    target_eq->eq_params[band].f0 = value;
                    Shell_Printf("[EQ:%s] band%d f0 = %ld\n", node->name, band, value);
                } else if (strcmp(suffix, "_Q") == 0) {
                    /* Q值：接收的是×100的整数（例如100表示Q=1.00），直接存储 */
                    node->params.eq.band_Q[band] = (uint32_t)value;
                    target_eq->eq_params[band].Q = value;
                    Shell_Printf("[EQ:%s] band%d Q = %ld (%.2f)\n", node->name, band, value, (float)value / 100.0f);
                } else if (strcmp(suffix, "_enable") == 0) {
                    /* 使能：同时更新节点参数和全局EQ单元 */
                    node->params.eq.band_enables[band] = value ? 1 : 0;
                    target_eq->eq_params[band].enable = value ? 1 : 0;
                    Shell_Printf("[EQ:%s] band%d enable = %d\n", node->name, band, value ? 1 : 0);
                    
                    /* 如果启用该频段但参数无效，初始化为默认值 */
                    if (value && target_eq->eq_params[band].f0 == 0) {
                        /* 根据频段索引设置默认频率 */
                        uint16_t default_f0[10] = {100, 300, 1000, 3000, 8000, 100, 300, 1000, 3000, 8000};
                        target_eq->eq_params[band].f0 = default_f0[band];
                        node->params.eq.band_f0[band] = default_f0[band];
                        Shell_Printf("[EQ] Init band%d f0 = %d\n", band, default_f0[band]);
                    }
                    if (value && target_eq->eq_params[band].Q <= 0) {
                        target_eq->eq_params[band].Q = 724;  /* Q=0.707, Q6.10格式 */
                        node->params.eq.band_Q[band] = 724;
                        Shell_Printf("[EQ] Init band%d Q = 724\n", band);
                    }
                    if (value && target_eq->eq_params[band].type < 0) {
                        target_eq->eq_params[band].type = 0;  /* PEAKING */
                        node->params.eq.band_types[band] = 0;
                        Shell_Printf("[EQ] Init band%d type = PEAKING\n", band);
                    }
                } else {
                    return -1;
                }
                
                /* 使用统一的辅助函数重建filter_params并应用 */
                RebuildAndApplyEQFilter(target_eq, node);
                /* 同步EQ参数到sys_param（仅内存，不写Flash） */
                SyncEQNodeToSysParam(node);
                /* 注意：参数仅保存在内存中，重启后会丢失 */
                /* 使用 'param save chain' 命令手动保存到Flash */
            } else if (strcmp(param_name, "pregain") == 0) {
                extern ControlVariablesContext gCtrlVars;
                EQUnit *target_eq;
                switch (node->id) {
                    case NODE_ID_EQ_GUITAR_L: target_eq = &gCtrlVars.eq_guitar_l_unit; break;
                    case NODE_ID_EQ_GUITAR_R: target_eq = &gCtrlVars.eq_guitar_r_unit; break;
                    case NODE_ID_EQ_MIC_L:    target_eq = &gCtrlVars.eq_mic_l_unit; break;
                    case NODE_ID_EQ_MIC_R:    target_eq = &gCtrlVars.eq_mic_r_unit; break;
                    case NODE_ID_USB_BT_EQ:   target_eq = &gCtrlVars.music_out_eq_unit; break;
                    default:
                        Shell_Printf("ERROR: Unknown EQ node ID %d\n", node->id);
                        return -1;
                }
                /* 预增益：同时更新节点参数和全局EQ单元 */
                node->params.eq.pregain = (int16_t)value;
                target_eq->pregain = value;
                Shell_Printf("[EQ:%s] pregain = %ld\n", node->name, value);
                AudioEffectEQPregainConfig(target_eq);
                /* 同步到sys_param（仅内存） */
                SyncEQNodeToSysParam(node);
            } else if (strcmp(param_name, "filter_count") == 0) {
                extern ControlVariablesContext gCtrlVars;
                EQUnit *target_eq;
                switch (node->id) {
                    case NODE_ID_EQ_GUITAR_L: target_eq = &gCtrlVars.eq_guitar_l_unit; break;
                    case NODE_ID_EQ_GUITAR_R: target_eq = &gCtrlVars.eq_guitar_r_unit; break;
                    case NODE_ID_EQ_MIC_L:    target_eq = &gCtrlVars.eq_mic_l_unit; break;
                    case NODE_ID_EQ_MIC_R:    target_eq = &gCtrlVars.eq_mic_r_unit; break;
                    case NODE_ID_USB_BT_EQ:   target_eq = &gCtrlVars.music_out_eq_unit; break;
                    default:
                        Shell_Printf("ERROR: Unknown EQ node ID %d\n", node->id);
                        return -1;
                }
                /* 频段数量：只更新节点参数，实际filter_count由enable状态决定 */
                node->params.eq.band_count = (uint8_t)value;
                Shell_Printf("[EQ:%s] Requested filter_count = %ld\n", node->name, value);
                /* 使用统一的辅助函数重建（会自动计算实际filter_count） */
                RebuildAndApplyEQFilter(target_eq, node);
                /* 同步到sys_param（仅内存） */
                SyncEQNodeToSysParam(node);
            } else if (strcmp(param_name, "channel") == 0) {
                extern ControlVariablesContext gCtrlVars;
                EQUnit *target_eq;
                switch (node->id) {
                    case NODE_ID_EQ_GUITAR_L: target_eq = &gCtrlVars.eq_guitar_l_unit; break;
                    case NODE_ID_EQ_GUITAR_R: target_eq = &gCtrlVars.eq_guitar_r_unit; break;
                    case NODE_ID_EQ_MIC_L:    target_eq = &gCtrlVars.eq_mic_l_unit; break;
                    case NODE_ID_EQ_MIC_R:    target_eq = &gCtrlVars.eq_mic_r_unit; break;
                    case NODE_ID_USB_BT_EQ:   target_eq = &gCtrlVars.music_out_eq_unit; break;
                    default:
                        Shell_Printf("ERROR: Unknown EQ node ID %d\n", node->id);
                        return -1;
                }
                target_eq->channel = value;
                Shell_Printf("[EQ:%s] channel = %ld\n", node->name, value);
                /* 同步到sys_param（仅内存） */
                SyncEQNodeToSysParam(node);
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_GAIN:
            if (strcmp(param_name, "gain") == 0) {
                node->params.gain.gain_db = (int16_t)value;
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DELAY:
            if (strcmp(param_name, "time") == 0) {
                node->params.delay.delay_ms = (uint16_t)value;
            } else if (strcmp(param_name, "feedback") == 0) {
                node->params.delay.feedback = (uint8_t)value;
            } else if (strcmp(param_name, "wet") == 0) {
                node->params.delay.wet_dry = (uint8_t)value;
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_EXPANDER:
            if (strcmp(param_name, "threshold") == 0) {
                node->params.expander.threshold = (int16_t)value;
            } else if (strcmp(param_name, "ratio") == 0) {
                node->params.expander.ratio = (uint8_t)value;
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_MIXER:
            /* 支持 in0_gain, in1_gain, ... */
            if (strncmp(param_name, "in", 2) == 0 && param_name[2] >= '0' && param_name[2] <= '3') {
                uint8_t ch = param_name[2] - '0';
                if (ch < EFFECT_GRAPH_MAX_INPUTS && strstr(param_name, "_gain")) {
                    node->params.mixer.input_gains[ch] = (int16_t)value;
                } else {
                    return -1;
                }
            } else {
                return -1;
            }
            break;
            
        default:
            return -1;
    }
    
    Shell_Printf("[Node %d] %s = %ld\n", node->id, param_name, (long)value);
    
    /* 对于非EQ参数（Reverb, DRC, Delay, Expander, Mixer等），保存到Flash */
    /* EQ参数已经在各自的分支中单独调用了保存 */
    if (node->type != EFFECT_NODE_TYPE_EFFECT_EQ) {
        extern SysParam_Status_t SysParam_SaveModule(const char *module);
        if (SysParam_SaveModule("chain") == SYSPARAM_OK) {
            Shell_Printf("[PARAM] Parameters saved to Flash\n");
        } else {
            Shell_Printf("[PARAM] ERROR: Failed to save parameters to Flash\n");
        }
    }
    
    return 0;
}

/**
 * @brief 获取节点参数值 (通用函数)
 * @param node 节点指针
 * @param param_name 参数名
 * @param value 输出参数值
 * @return 0成功，-1失败
 */
static int GetNodeParam(EffectNode_t *node, const char *param_name, int32_t *value)
{
    if (!node || !param_name || !value) return -1;
    
    switch (node->type) {
        case EFFECT_NODE_TYPE_EFFECT_REVERB:
            if (strcmp(param_name, "room") == 0) {
                *value = node->params.reverb.room_size;
            } else if (strcmp(param_name, "damp") == 0) {
                *value = node->params.reverb.damping;
            } else if (strcmp(param_name, "wet") == 0) {
                *value = node->params.reverb.wet_dry;
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DRC:
            if (strcmp(param_name, "threshold") == 0) {
                *value = node->params.drc.threshold;
            } else if (strcmp(param_name, "ratio") == 0) {
                *value = node->params.drc.ratio;
            } else if (strcmp(param_name, "attack") == 0) {
                *value = node->params.drc.attack;
            } else if (strcmp(param_name, "release") == 0) {
                *value = node->params.drc.release;
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_EQ:
            if (strncmp(param_name, "band", 4) == 0 && param_name[4] >= '0' && param_name[4] <= '9') {
                uint8_t band = param_name[4] - '0';
                if (band < 10) {
                    *value = node->params.eq.band_gains[band];
                } else {
                    return -1;
                }
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_GAIN:
            if (strcmp(param_name, "gain") == 0) {
                *value = node->params.gain.gain_db;
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DELAY:
            if (strcmp(param_name, "time") == 0) {
                *value = node->params.delay.delay_ms;
            } else if (strcmp(param_name, "feedback") == 0) {
                *value = node->params.delay.feedback;
            } else if (strcmp(param_name, "wet") == 0) {
                *value = node->params.delay.wet_dry;
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_EXPANDER:
            if (strcmp(param_name, "threshold") == 0) {
                *value = node->params.expander.threshold;
            } else if (strcmp(param_name, "ratio") == 0) {
                *value = node->params.expander.ratio;
            } else {
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_MIXER:
            if (strncmp(param_name, "in", 2) == 0 && param_name[2] >= '0' && param_name[2] <= '3') {
                uint8_t ch = param_name[2] - '0';
                if (ch < EFFECT_GRAPH_MAX_INPUTS && strstr(param_name, "_gain")) {
                    *value = node->params.mixer.input_gains[ch];
                } else {
                    return -1;
                }
            } else {
                return -1;
            }
            break;
            
        default:
            return -1;
    }
    
    return 0;
}

/* 打印帮助信息 */
static void PrintHelp(void)
{
    Shell_Printf("\n===== Effect Graph Commands =====\n");
    Shell_Printf("graph list                  - List all nodes\n");
    Shell_Printf("graph info                  - Show graph details\n");
    Shell_Printf("graph preset [id]           - Switch/show presets\n");
    Shell_Printf("graph node <id|name> [on|off]  - Enable/disable node\n");
    Shell_Printf("graph bypass <id|name> [on|off]- Set node bypass\n");
    Shell_Printf("graph set <id|name> <key> <val> - Set node parameter\n");
    Shell_Printf("graph get <id|name> [key]   - Get node parameter(s)\n");
    Shell_Printf("graph params <id|name>      - Show available params\n");
    Shell_Printf("graph rebuild               - Rebuild graph\n");
    Shell_Printf("\n--- Batch Operations ---\n");
    Shell_Printf("graph allfx <on|off>        - Enable/disable all effects\n");
    Shell_Printf("graph allbypass <on|off>    - Bypass all effects\n");
    Shell_Printf("\n--- Snapshots ---\n");
    Shell_Printf("graph snapshot save <slot> [name] - Save state\n");
    Shell_Printf("graph snapshot load <slot>  - Load state\n");
    Shell_Printf("graph snapshot list         - List snapshots\n");
    Shell_Printf("\n--- APP Communication (JSON) ---\n");
    Shell_Printf("graph query all             - Query all nodes (JSON)\n");
    Shell_Printf("graph query node <id>       - Query single node (JSON)\n");
    Shell_Printf("graph query volume          - Query volume params (JSON)\n");
    Shell_Printf("graph query system          - Query system params (JSON)\n");
    Shell_Printf("graph query eq              - Query EQ params (JSON)\n");
    Shell_Printf("\n--- ID-based quick commands (fx) ---\n");
    Shell_Printf("fx <id> <key> [val]         - Quick get/set by ID\n");
    Shell_Printf("fx <id>                     - Show node info by ID\n");
    Shell_Printf("==================================\n\n");
}

/* 列出所有节点 */
static int CmdList(void)
{
    EffectGraphRuntime_t *graph = EffectGraph_GetInstance();
    uint8_t i;
    
    if (!graph) {
        Shell_Printf("ERROR: Graph not initialized\n");
        return -1;
    }
    
    Shell_Printf("\n===== Graph Nodes [%d/%d] =====\n", 
                 graph->node_count, EFFECT_GRAPH_MAX_NODES);
    Shell_Printf("ID  Name            Type        Status\n");
    Shell_Printf("--- --------------- ----------- --------\n");
    
    for (i = 0; i < graph->node_count; i++) {
        EffectNode_t *node = &graph->nodes[i];
        const char *type_str = "Unknown";
        
        switch (node->type) {
            case EFFECT_NODE_TYPE_SOURCE_ADC0: type_str = "ADC0"; break;
            case EFFECT_NODE_TYPE_SOURCE_ADC1: type_str = "ADC1"; break;
            case EFFECT_NODE_TYPE_SOURCE_USB_IN: type_str = "USB_IN"; break;
            case EFFECT_NODE_TYPE_SOURCE_BT_IN: type_str = "BT_IN"; break;
            case EFFECT_NODE_TYPE_SOURCE_METRONOME: type_str = "METRONOME"; break;
            case EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY: type_str = "LOOPER_PLAY"; break;
            case EFFECT_NODE_TYPE_SINK_DAC0: type_str = "DAC0"; break;
            case EFFECT_NODE_TYPE_SINK_USB_OUT: type_str = "USB_OUT"; break;
            case EFFECT_NODE_TYPE_SINK_LOOPER_RECORD: type_str = "LOOPER_REC"; break;
            case EFFECT_NODE_TYPE_MIXER: type_str = "MIXER"; break;
            case EFFECT_NODE_TYPE_EFFECT_REVERB: type_str = "REVERB"; break;
            case EFFECT_NODE_TYPE_EFFECT_DRC: type_str = "DRC"; break;
            case EFFECT_NODE_TYPE_EFFECT_EQ: type_str = "EQ"; break;
            case EFFECT_NODE_TYPE_EFFECT_EXPANDER: type_str = "EXPANDER"; break;
            case EFFECT_NODE_TYPE_EFFECT_HOWLING: type_str = "HOWLING"; break;
            case EFFECT_NODE_TYPE_EFFECT_NOISE_GATE: type_str = "NOISE_GATE"; break;
            case EFFECT_NODE_TYPE_EFFECT_GAIN: type_str = "GAIN"; break;
            case EFFECT_NODE_TYPE_EFFECT_DELAY: type_str = "DELAY"; break;
            case EFFECT_NODE_TYPE_EFFECT_CHORUS: type_str = "CHORUS"; break;
            case EFFECT_NODE_TYPE_LOOPER: type_str = "LOOPER"; break;
            default: break;
        }
        
        Shell_Printf("%2d  %-15s %-11s %s%s\n", 
                     node->id, 
                     node->name, 
                     type_str,
                     node->enabled ? "ON " : "OFF",
                     node->bypass ? " [BYP]" : "");
    }
    
    Shell_Printf("===============================\n");
    Shell_Printf("Use: graph get <id> to show node params\n\n");
    return 0;
}

/* 显示图详细信息 */
static int CmdInfo(void)
{
    EffectGraph_PrintInfo();
    return 0;
}

/* 切换预设 */
static int CmdPreset(int argc, char *argv[])
{
    if (argc < 3) {
        /* 显示所有预设 */
        EffectGraphConfig_PrintPresets();
        Shell_Printf("Current preset: %d\n", EffectGraphConfig_GetCurrentPreset());
        return 0;
    }
    
    int preset_id = atoi(argv[2]);
    if (preset_id < 0 || preset_id >= GRAPH_PRESET_MAX) {
        Shell_Printf("ERROR: Invalid preset ID [0-%d]\n", GRAPH_PRESET_MAX - 1);
        return -1;
    }
    
    GraphError_t err = EffectGraphConfig_LoadPreset((GraphPreset_t)preset_id);
    if (err != GRAPH_OK) {
        Shell_Printf("ERROR: Failed to load preset (%d)\n", err);
        return -1;
    }
    
    Shell_Printf("Preset %d loaded successfully\n", preset_id);
    return 0;
}

/* 启用/禁用节点 (支持ID或名称) */
static int CmdNode(int argc, char *argv[])
{
    if (argc < 3) {
        Shell_Printf("Usage: graph node <id|name> [on|off]\n");
        return -1;
    }
    
    EffectNode_t *node = FindNode(argv[2]);
    if (!node) {
        Shell_Printf("ERROR: Node '%s' not found\n", argv[2]);
        return -1;
    }
    
    if (argc >= 4) {
        bool enabled = (strcmp(argv[3], "on") == 0);
        EffectGraph_SetNodeEnabled(node, enabled);
        Shell_Printf("Node[%d] '%s' %s\n", node->id, node->name, enabled ? "enabled" : "disabled");
        
        /* 保存到Flash */
        extern SysParam_Status_t SysParam_SaveModule(const char *module);
        if (SysParam_SaveModule("chain") == SYSPARAM_OK) {
            Shell_Printf("[PARAM] Node state saved to Flash\n");
        } else {
            Shell_Printf("[PARAM] ERROR: Failed to save node state to Flash\n");
        }
    } else {
        Shell_Printf("Node[%d] '%s' is %s\n", node->id, node->name, node->enabled ? "enabled" : "disabled");
    }
    
    return 0;
}

/* 设置节点旁路 (支持ID或名称) */
static int CmdBypass(int argc, char *argv[])
{
    if (argc < 3) {
        Shell_Printf("Usage: graph bypass <id|name> [on|off]\n");
        return -1;
    }
    
    EffectNode_t *node = FindNode(argv[2]);
    if (!node) {
        Shell_Printf("ERROR: Node '%s' not found\n", argv[2]);
        return -1;
    }
    
    if (argc >= 4) {
        bool bypass = (strcmp(argv[3], "on") == 0);
        EffectGraph_SetNodeBypass(node, bypass);
        Shell_Printf("Node[%d] '%s' bypass %s\n", node->id, node->name, bypass ? "ON" : "OFF");
        
        /* 保存到Flash */
        extern SysParam_Status_t SysParam_SaveModule(const char *module);
        if (SysParam_SaveModule("chain") == SYSPARAM_OK) {
            Shell_Printf("[PARAM] Bypass state saved to Flash\n");
        } else {
            Shell_Printf("[PARAM] ERROR: Failed to save bypass state to Flash\n");
        }
    } else {
        Shell_Printf("Node[%d] '%s' bypass is %s\n", node->id, node->name, node->bypass ? "ON" : "OFF");
    }
    
    return 0;
}

/* 获取节点参数 (新命令) */
static int CmdGet(int argc, char *argv[])
{
    if (argc < 3) {
        Shell_Printf("Usage: graph get <id|name> [param]\n");
        return -1;
    }
    
    EffectNode_t *node = FindNode(argv[2]);
    if (!node) {
        Shell_Printf("ERROR: Node '%s' not found\n", argv[2]);
        return -1;
    }
    
    if (argc >= 4) {
        /* 获取指定参数 */
        int32_t value;
        if (GetNodeParam(node, argv[3], &value) == 0) {
            Shell_Printf("[Node %d] %s = %ld\n", node->id, argv[3], (long)value);
        } else {
            Shell_Printf("ERROR: Unknown param '%s' for node type\n", argv[3]);
            return -1;
        }
    } else {
        /* 打印所有参数 */
        PrintNodeParams(node);
    }
    
    return 0;
}

/* 设置节点参数 (新命令) */
static int CmdSet(int argc, char *argv[])
{
    if (argc < 5) {
        Shell_Printf("Usage: graph set <id|name> <param> <value>\n");
        return -1;
    }
    
    EffectNode_t *node = FindNode(argv[2]);
    if (!node) {
        Shell_Printf("ERROR: Node '%s' not found\n", argv[2]);
        return -1;
    }
    
    int32_t value = atoi(argv[4]);
    if (SetNodeParam(node, argv[3], value) != 0) {
        Shell_Printf("ERROR: Failed to set '%s' for node type\n", argv[3]);
        return -1;
    }
    
    return 0;
}

/* 读取/设置节点参数 */
static int CmdParam(int argc, char *argv[])
{
    if (argc < 4) {
        Shell_Printf("Usage: graph param <name> <param> [value]\n");
        return -1;
    }
    
    EffectNode_t *node = EffectGraph_FindNodeByName(argv[2]);
    if (!node) {
        Shell_Printf("ERROR: Node '%s' not found\n", argv[2]);
        return -1;
    }
    
    const char *param = argv[3];
    
    /* 根据节点类型和参数名读取/设置参数 */
    switch (node->type) {
        case EFFECT_NODE_TYPE_EFFECT_REVERB:
            if (strcmp(param, "room") == 0) {
                if (argc >= 5) {
                    node->params.reverb.room_size = atoi(argv[4]);
                }
                Shell_Printf("Reverb room_size: %d\n", node->params.reverb.room_size);
            } else if (strcmp(param, "damp") == 0) {
                if (argc >= 5) {
                    node->params.reverb.damping = atoi(argv[4]);
                }
                Shell_Printf("Reverb damping: %d\n", node->params.reverb.damping);
            } else if (strcmp(param, "wet") == 0) {
                if (argc >= 5) {
                    node->params.reverb.wet_dry = atoi(argv[4]);
                }
                Shell_Printf("Reverb wet_dry: %d\n", node->params.reverb.wet_dry);
            } else {
                Shell_Printf("ERROR: Unknown reverb param '%s'\n", param);
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DRC:
            if (strcmp(param, "threshold") == 0) {
                if (argc >= 5) {
                    node->params.drc.threshold = atoi(argv[4]);
                }
                Shell_Printf("DRC threshold: %d dB\n", node->params.drc.threshold);
            } else if (strcmp(param, "ratio") == 0) {
                if (argc >= 5) {
                    node->params.drc.ratio = atoi(argv[4]);
                }
                Shell_Printf("DRC ratio: %d\n", node->params.drc.ratio);
            } else if (strcmp(param, "attack") == 0) {
                if (argc >= 5) {
                    node->params.drc.attack = atoi(argv[4]);
                }
                Shell_Printf("DRC attack: %d ms\n", node->params.drc.attack);
            } else if (strcmp(param, "release") == 0) {
                if (argc >= 5) {
                    node->params.drc.release = atoi(argv[4]);
                }
                Shell_Printf("DRC release: %d ms\n", node->params.drc.release);
            } else {
                Shell_Printf("ERROR: Unknown DRC param '%s'\n", param);
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_GAIN:
            if (strcmp(param, "gain") == 0) {
                if (argc >= 5) {
                    node->params.gain.gain_db = atoi(argv[4]);
                }
                Shell_Printf("Gain: %d dB\n", node->params.gain.gain_db);
            } else {
                Shell_Printf("ERROR: Unknown gain param '%s'\n", param);
                return -1;
            }
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DELAY:
            if (strcmp(param, "time") == 0) {
                if (argc >= 5) {
                    node->params.delay.delay_ms = atoi(argv[4]);
                }
                Shell_Printf("Delay time: %d ms\n", node->params.delay.delay_ms);
            } else if (strcmp(param, "feedback") == 0) {
                if (argc >= 5) {
                    node->params.delay.feedback = atoi(argv[4]);
                }
                Shell_Printf("Delay feedback: %d\n", node->params.delay.feedback);
            } else {
                Shell_Printf("ERROR: Unknown delay param '%s'\n", param);
                return -1;
            }
            break;
            
        default:
            Shell_Printf("ERROR: Node type doesn't support parameter read/write\n");
            return -1;
    }
    
    return 0;
}

/* 重建图 */
static int CmdRebuild(void)
{
    GraphError_t err = EffectGraph_Build();
    if (err != GRAPH_OK) {
        Shell_Printf("ERROR: Failed to rebuild graph (%d)\n", err);
        return -1;
    }
    
    Shell_Printf("Graph rebuilt successfully\n");
    return 0;
}

/*******************************************************************************
 * 快照管理命令
 ******************************************************************************/

/**
 * @brief 保存当前图状态到快照槽
 */
static int CmdSnapshotSave(int argc, char *argv[])
{
    int slot;
    uint8_t i;
    EffectGraphRuntime_t *graph;
    GraphSnapshot_t *snap;
    
    if (argc < 4) {
        Shell_Printf("Usage: graph snapshot save <slot> [name]\n");
        Shell_Printf("  slot: 0-%d\n", SNAPSHOT_MAX_SLOTS - 1);
        return -1;
    }
    
    slot = atoi(argv[3]);
    if (slot < 0 || slot >= SNAPSHOT_MAX_SLOTS) {
        Shell_Printf("ERROR: Invalid slot [0-%d]\n", SNAPSHOT_MAX_SLOTS - 1);
        return -1;
    }
    
    graph = EffectGraph_GetInstance();
    if (!graph) {
        Shell_Printf("ERROR: Graph not initialized\n");
        return -1;
    }
    
    snap = &g_Snapshots[slot];
    memset(snap, 0, sizeof(GraphSnapshot_t));
    
    /* 保存名称 */
    if (argc >= 5) {
        strncpy(snap->name, argv[4], sizeof(snap->name) - 1);
    } else {
        snprintf(snap->name, sizeof(snap->name), "Snap%d", slot);
    }
    
    /* 保存节点状态 */
    snap->node_count = graph->node_count;
    for (i = 0; i < graph->node_count && i < SNAPSHOT_MAX_NODES; i++) {
        EffectNode_t *node = &graph->nodes[i];
        snap->nodes[i].node_id = node->id;
        snap->nodes[i].enabled = node->enabled;
        snap->nodes[i].bypass = node->bypass;
        memcpy(snap->nodes[i].param_data, &node->params, sizeof(EffectParams_t));
    }
    
    snap->valid = true;
    Shell_Printf("Snapshot saved to slot %d: '%s' (%d nodes)\n", 
                 slot, snap->name, snap->node_count);
    return 0;
}

/**
 * @brief 从快照槽恢复图状态
 */
static int CmdSnapshotLoad(int argc, char *argv[])
{
    int slot;
    uint8_t i;
    EffectGraphRuntime_t *graph;
    GraphSnapshot_t *snap;
    
    if (argc < 4) {
        Shell_Printf("Usage: graph snapshot load <slot>\n");
        return -1;
    }
    
    slot = atoi(argv[3]);
    if (slot < 0 || slot >= SNAPSHOT_MAX_SLOTS) {
        Shell_Printf("ERROR: Invalid slot [0-%d]\n", SNAPSHOT_MAX_SLOTS - 1);
        return -1;
    }
    
    snap = &g_Snapshots[slot];
    if (!snap->valid) {
        Shell_Printf("ERROR: Slot %d is empty\n", slot);
        return -1;
    }
    
    graph = EffectGraph_GetInstance();
    if (!graph) {
        Shell_Printf("ERROR: Graph not initialized\n");
        return -1;
    }
    
    /* 恢复节点状态 */
    for (i = 0; i < snap->node_count && i < graph->node_count; i++) {
        EffectNode_t *node = EffectGraph_FindNodeById(snap->nodes[i].node_id);
        if (node) {
            node->enabled = snap->nodes[i].enabled;
            node->bypass = snap->nodes[i].bypass;
            memcpy(&node->params, snap->nodes[i].param_data, sizeof(EffectParams_t));
        }
    }
    
    Shell_Printf("Snapshot '%s' loaded from slot %d\n", snap->name, slot);
    return 0;
}

/**
 * @brief 列出所有快照
 */
static int CmdSnapshotList(void)
{
    int i;
    
    Shell_Printf("\n===== Snapshots =====\n");
    Shell_Printf("Slot  Name            Nodes\n");
    Shell_Printf("----- --------------- -----\n");
    
    for (i = 0; i < SNAPSHOT_MAX_SLOTS; i++) {
        GraphSnapshot_t *snap = &g_Snapshots[i];
        if (snap->valid) {
            Shell_Printf("[%d]   %-15s %d\n", i, snap->name, snap->node_count);
        } else {
            Shell_Printf("[%d]   (empty)\n", i);
        }
    }
    
    Shell_Printf("=====================\n\n");
    return 0;
}

/**
 * @brief 快照命令入口
 */
static int CmdSnapshot(int argc, char *argv[])
{
    if (argc < 3) {
        Shell_Printf("Usage: graph snapshot <save|load|list> [args]\n");
        Shell_Printf("  graph snapshot save <slot> [name] - Save to slot\n");
        Shell_Printf("  graph snapshot load <slot>        - Load from slot\n");
        Shell_Printf("  graph snapshot list               - List all snapshots\n");
        return -1;
    }
    
    if (strcmp(argv[2], "save") == 0) {
        return CmdSnapshotSave(argc, argv);
    }
    else if (strcmp(argv[2], "load") == 0) {
        return CmdSnapshotLoad(argc, argv);
    }
    else if (strcmp(argv[2], "list") == 0) {
        return CmdSnapshotList();
    }
    else {
        Shell_Printf("ERROR: Unknown snapshot action '%s'\n", argv[2]);
        return -1;
    }
}

/*******************************************************************************
 * 批量操作命令
 ******************************************************************************/

/**
 * @brief 批量启用/禁用所有效果节点
 */
static int CmdAllFx(int argc, char *argv[])
{
    EffectGraphRuntime_t *graph;
    uint8_t i;
    bool enable;
    int count = 0;
    
    if (argc < 3) {
        Shell_Printf("Usage: graph allfx <on|off>\n");
        return -1;
    }
    
    enable = (strcmp(argv[2], "on") == 0);
    
    graph = EffectGraph_GetInstance();
    if (!graph) {
        Shell_Printf("ERROR: Graph not initialized\n");
        return -1;
    }
    
    for (i = 0; i < graph->node_count; i++) {
        EffectNode_t *node = &graph->nodes[i];
        /* 只处理效果器节点，跳过源/输出节点 */
        if (node->type >= EFFECT_NODE_TYPE_MIXER && node->type < EFFECT_NODE_TYPE_MAX) {
            EffectGraph_SetNodeEnabled(node, enable);
            count++;
        }
    }
    
    Shell_Printf("All effects %s (%d nodes)\n", enable ? "enabled" : "disabled", count);
    return 0;
}

/**
 * @brief 批量旁路所有效果节点
 */
static int CmdAllBypass(int argc, char *argv[])
{
    EffectGraphRuntime_t *graph;
    uint8_t i;
    bool bypass;
    int count = 0;
    
    if (argc < 3) {
        Shell_Printf("Usage: graph allbypass <on|off>\n");
        return -1;
    }
    
    bypass = (strcmp(argv[2], "on") == 0);
    
    graph = EffectGraph_GetInstance();
    if (!graph) {
        Shell_Printf("ERROR: Graph not initialized\n");
        return -1;
    }
    
    for (i = 0; i < graph->node_count; i++) {
        EffectNode_t *node = &graph->nodes[i];
        if (node->type >= EFFECT_NODE_TYPE_MIXER && node->type < EFFECT_NODE_TYPE_MAX) {
            EffectGraph_SetNodeBypass(node, bypass);
            count++;
        }
    }
    
    Shell_Printf("All effects bypass %s (%d nodes)\n", bypass ? "ON" : "OFF", count);
    return 0;
}

/*******************************************************************************
 * APP通信查询命令 - JSON格式输出
 ******************************************************************************/

/**
 * @brief 输出单个节点的JSON格式参数
 */
static void PrintNodeJSON(EffectNode_t *node, int is_last)
{
    int i;
    
    Shell_Printf("    {\"id\":%d,\"name\":\"%s\",\"type\":%d,\"enabled\":%d,\"bypass\":%d,\"params\":{",
                 node->id, node->name, node->type, node->enabled ? 1 : 0, node->bypass ? 1 : 0);
    
    switch (node->type) {
        case EFFECT_NODE_TYPE_EFFECT_REVERB:
            Shell_Printf("\"room\":%d,\"damp\":%d,\"wet\":%d",
                        node->params.reverb.room_size,
                        node->params.reverb.damping,
                        node->params.reverb.wet_dry);
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DRC:
            Shell_Printf("\"threshold\":%d,\"ratio\":%d,\"attack\":%d,\"release\":%d",
                        node->params.drc.threshold,
                        node->params.drc.ratio,
                        node->params.drc.attack,
                        node->params.drc.release);
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_EQ:
            Shell_Printf("\"band_count\":%d,\"pregain\":%d,\"bands\":[",
                        node->params.eq.band_count, node->params.eq.pregain);
            for (i = 0; i < node->params.eq.band_count && i < 10; i++) {
                Shell_Printf("{\"gain\":%d,\"type\":%d,\"f0\":%lu,\"Q\":%lu,\"en\":%d}%s",
                            node->params.eq.band_gains[i],
                            node->params.eq.band_types[i],
                            (unsigned long)node->params.eq.band_f0[i],
                            (unsigned long)node->params.eq.band_Q[i],
                            node->params.eq.band_enables[i],
                            (i < node->params.eq.band_count - 1) ? "," : "");
            }
            Shell_Printf("]");
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_GAIN:
            Shell_Printf("\"gain\":%d", node->params.gain.gain_db);
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DELAY:
            Shell_Printf("\"time\":%d,\"feedback\":%d,\"wet\":%d",
                        node->params.delay.delay_ms,
                        node->params.delay.feedback,
                        node->params.delay.wet_dry);
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_EXPANDER:
            Shell_Printf("\"threshold\":%d,\"ratio\":%d",
                        node->params.expander.threshold,
                        node->params.expander.ratio);
            break;
            
        case EFFECT_NODE_TYPE_MIXER:
            Shell_Printf("\"input_count\":%d,\"gains\":[",
                        node->params.mixer.input_count);
            for (i = 0; i < node->params.mixer.input_count && i < EFFECT_GRAPH_MAX_INPUTS; i++) {
                Shell_Printf("%d%s", node->params.mixer.input_gains[i],
                            (i < node->params.mixer.input_count - 1) ? "," : "");
            }
            Shell_Printf("]");
            break;
            
        default:
            /* 源/输出节点无参数 */
            break;
    }
    
    Shell_Printf("}}%s\n", is_last ? "" : ",");
}

/**
 * @brief APP通信查询命令 - 返回JSON格式的完整效果参数
 * 
 * 用法:
 *   graph query all      - 查询所有节点参数(JSON)
 *   graph query node <id>- 查询单个节点参数(JSON)
 *   graph query volume   - 查询音量参数(JSON)
 *   graph query system   - 查询系统参数(JSON)
 */
static int CmdQuery(int argc, char *argv[])
{
    EffectGraphRuntime_t *graph;
    extern ControlVariablesContext gCtrlVars;
    extern SysParam_t g_sys_param;
    uint8_t i;
    
    if (argc < 3) {
        Shell_Printf("{\"error\":\"Usage: graph query <all|node|volume|system>\"}\n");
        return -1;
    }
    
    const char *target = argv[2];
    
    if (strcmp(target, "all") == 0) {
        /* 查询所有节点 */
        graph = EffectGraph_GetInstance();
        if (!graph) {
            Shell_Printf("{\"error\":\"Graph not initialized\"}\n");
            return -1;
        }
        
        Shell_Printf("{\"status\":\"ok\",\"node_count\":%d,\"nodes\":[\n", graph->node_count);
        for (i = 0; i < graph->node_count; i++) {
            PrintNodeJSON(&graph->nodes[i], (i == graph->node_count - 1));
        }
        Shell_Printf("]}\n");
        return 0;
    }
    else if (strcmp(target, "node") == 0) {
        /* 查询单个节点 */
        if (argc < 4) {
            Shell_Printf("{\"error\":\"Missing node ID\"}\n");
            return -1;
        }
        EffectNode_t *node = FindNode(argv[3]);
        if (!node) {
            Shell_Printf("{\"error\":\"Node not found\"}\n");
            return -1;
        }
        Shell_Printf("{\"status\":\"ok\",\"node\":");
        PrintNodeJSON(node, 1);
        Shell_Printf("}\n");
        return 0;
    }
    else if (strcmp(target, "volume") == 0) {
        /* 查询音量参数 */
        Shell_Printf("{\"status\":\"ok\",\"volume\":{");
        Shell_Printf("\"mic1\":%d,\"mic2\":%d,\"guitar1\":%d,\"guitar2\":%d,\"output\":%d",
                    g_sys_param.volume.mic1_volume,
                    g_sys_param.volume.mic2_volume,
                    g_sys_param.volume.guitar1_volume,
                    g_sys_param.volume.guitar2_volume,
                    g_sys_param.volume.output_volume);
        Shell_Printf("}}\n");
        return 0;
    }
    else if (strcmp(target, "system") == 0) {
        /* 查询系统参数 */
        Shell_Printf("{\"status\":\"ok\",\"system\":{");
        Shell_Printf("\"version\":%d,\"boot_count\":%d,\"bt_enabled\":%d,\"bt_name\":\"%s\"",
                    g_sys_param.version,
                    g_sys_param.system.boot_count,
                    g_sys_param.bluetooth.enabled,
                    g_sys_param.bluetooth.device_name);
        Shell_Printf("}}\n");
        return 0;
    }
    else if (strcmp(target, "eq") == 0) {
        /* 查询EQ详细参数 - 用于APP同步 */
        Shell_Printf("{\"status\":\"ok\",\"eq\":{");
        
        /* ADC EQ (mic_out_eq_unit) */
        Shell_Printf("\"adc\":{\"enable\":%d,\"filter_count\":%d,\"bands\":[",
                    gCtrlVars.mic_out_eq_unit.enable,
                    gCtrlVars.mic_out_eq_unit.filter_count);
        for (i = 0; i < 10; i++) {
            Shell_Printf("{\"en\":%d,\"gain\":%ld,\"f0\":%u,\"Q\":%d,\"type\":%d}%s",
                        gCtrlVars.mic_out_eq_unit.eq_params[i].enable,
                        (long)gCtrlVars.mic_out_eq_unit.eq_params[i].gain,
                        gCtrlVars.mic_out_eq_unit.eq_params[i].f0,
                        gCtrlVars.mic_out_eq_unit.eq_params[i].Q,
                        gCtrlVars.mic_out_eq_unit.eq_params[i].type,
                        (i < 9) ? "," : "");
        }
        Shell_Printf("]},");
        
        /* Music EQ (music_out_eq_unit) */
        Shell_Printf("\"music\":{\"enable\":%d,\"filter_count\":%d,\"bands\":[",
                    gCtrlVars.music_out_eq_unit.enable,
                    gCtrlVars.music_out_eq_unit.filter_count);
        for (i = 0; i < 10; i++) {
            Shell_Printf("{\"en\":%d,\"gain\":%ld,\"f0\":%u,\"Q\":%d,\"type\":%d}%s",
                        gCtrlVars.music_out_eq_unit.eq_params[i].enable,
                        (long)gCtrlVars.music_out_eq_unit.eq_params[i].gain,
                        gCtrlVars.music_out_eq_unit.eq_params[i].f0,
                        gCtrlVars.music_out_eq_unit.eq_params[i].Q,
                        gCtrlVars.music_out_eq_unit.eq_params[i].type,
                        (i < 9) ? "," : "");
        }
        Shell_Printf("]}");
        
        Shell_Printf("}}\n");
        return 0;
    }
    else {
        Shell_Printf("{\"error\":\"Unknown query target: %s\"}\n", target);
        Shell_Printf("{\"hint\":\"Available: all, node, volume, system, eq\"}\n");
        return -1;
    }
}

/**
 * @brief 显示节点可用参数
 */
static int CmdParams(int argc, char *argv[])
{
    EffectNode_t *node;
    
    if (argc < 3) {
        Shell_Printf("Usage: graph params <id|name>\n");
        return -1;
    }
    
    node = FindNode(argv[2]);
    if (!node) {
        Shell_Printf("ERROR: Node '%s' not found\n", argv[2]);
        return -1;
    }
    
    Shell_Printf("\n=== Node[%d]: %s ===\n", node->id, node->name);
    PrintAvailableParams(node);
    Shell_Printf("========================\n\n");
    return 0;
}

/*******************************************************************************
 * 函数前向声明
 ******************************************************************************/
int ShellCmdFx_Execute(int argc, char *argv[]);
int ShellCmdEqTest_Execute(int argc, char *argv[]);

/*******************************************************************************
 * 公共API实现
 ******************************************************************************/

/**
 * @brief Graph命令默认处理 - 用于模块系统
 */
static int GraphModuleHandler(int argc, char *argv[])
{
    /* argc 不包含模块名本身，argv[0] 是第一个参数 */
    /* 重新构建完整的 argc/argv 供 ShellCmdGraph_Execute 使用 */
    char *fullArgv[SHELL_CMD_MAX_ARGS];
    int fullArgc = argc + 1;
    int i;
    
    fullArgv[0] = "graph";  /* 模块名 */
    for (i = 0; i < argc && i < SHELL_CMD_MAX_ARGS - 1; i++) {
        fullArgv[i + 1] = argv[i];
    }
    
    return ShellCmdGraph_Execute(fullArgc, fullArgv);
}

/**
 * @brief fx命令默认处理 - 用于模块系统
 */
static int FxModuleHandler(int argc, char *argv[])
{
    /* argc 不包含模块名本身，argv[0] 是第一个参数 */
    char *fullArgv[SHELL_CMD_MAX_ARGS];
    int fullArgc = argc + 1;
    int i;
    
    fullArgv[0] = "fx";  /* 模块名 */
    for (i = 0; i < argc && i < SHELL_CMD_MAX_ARGS - 1; i++) {
        fullArgv[i + 1] = argv[i];
    }
    
    return ShellCmdFx_Execute(fullArgc, fullArgv);
}

/**
 * @brief eq_test命令默认处理 - 用于模块系统
 */
static int EqTestModuleHandler(int argc, char *argv[])
{
    char *fullArgv[SHELL_CMD_MAX_ARGS];
    int fullArgc = argc + 1;
    int i;
    
    fullArgv[0] = "eq_test";
    for (i = 0; i < argc && i < SHELL_CMD_MAX_ARGS - 1; i++) {
        fullArgv[i + 1] = argv[i];
    }
    
    return ShellCmdEqTest_Execute(fullArgc, fullArgv);
}

/**
 * @brief Graph命令选项（使用默认选项模式）
 */
static const ShellOpt_t g_GraphOpts[] = {
    { "", NULL, "[subcmd] [args]", "Effect graph control", GraphModuleHandler },
    OPT_END()
};

/**
 * @brief fx快捷命令选项（使用默认选项模式）
 */
static const ShellOpt_t g_FxOpts[] = {
    { "", NULL, "<id> [param] [val]", "Quick effect parameter access", FxModuleHandler },
    OPT_END()
};

/**
 * @brief eq_test命令选项
 */
static const ShellOpt_t g_EqTestOpts[] = {
    { "", NULL, "<node_id> <preset>", "Apply EQ test presets (bass/treble/vocal/flat)", EqTestModuleHandler },
    OPT_END()
};

/**
 * @brief Graph命令模块定义
 */
static const ShellModule_t g_GraphModule = {
    "graph",
    "Audio Effect Graph Control",
    MOD_CAT_AUDIO,
    g_GraphOpts,
    1
};

/**
 * @brief fx快捷命令模块定义
 */
static const ShellModule_t g_FxModule = {
    "fx",
    "Quick Effect Parameter Access",
    MOD_CAT_AUDIO,
    g_FxOpts,
    1
};

/**
 * @brief eq_test命令模块定义
 */
static const ShellModule_t g_EqTestModule = {
    "eq_test",
    "EQ Test Presets (bass/treble/vocal/flat)",
    MOD_CAT_AUDIO,
    g_EqTestOpts,
    1
};

void ShellCmdGraph_Register(void)
{
    /* 注册到Shell系统 */
    Shell_RegisterModule(&g_GraphModule);
    Shell_RegisterModule(&g_FxModule);
    Shell_RegisterModule(&g_EqTestModule);
    DBG("[ShellCmdGraph] Registered (graph, fx, eq_test)\n");
}

int ShellCmdGraph_Execute(int argc, char *argv[])
{
    if (argc < 2) {
        PrintHelp();
        return 0;
    }
    
    const char *subcmd = argv[1];
    
    if (strcmp(subcmd, "help") == 0) {
        PrintHelp();
        return 0;
    }
    else if (strcmp(subcmd, "list") == 0) {
        return CmdList();
    }
    else if (strcmp(subcmd, "info") == 0) {
        return CmdInfo();
    }
    else if (strcmp(subcmd, "preset") == 0) {
        return CmdPreset(argc, argv);
    }
    else if (strcmp(subcmd, "node") == 0) {
        return CmdNode(argc, argv);
    }
    else if (strcmp(subcmd, "bypass") == 0) {
        return CmdBypass(argc, argv);
    }
    else if (strcmp(subcmd, "get") == 0) {
        return CmdGet(argc, argv);
    }
    else if (strcmp(subcmd, "set") == 0) {
        return CmdSet(argc, argv);
    }
    else if (strcmp(subcmd, "param") == 0) {
        /* 兼容旧命令 */
        return CmdParam(argc, argv);
    }
    else if (strcmp(subcmd, "params") == 0) {
        return CmdParams(argc, argv);
    }
    else if (strcmp(subcmd, "rebuild") == 0) {
        return CmdRebuild();
    }
    else if (strcmp(subcmd, "snapshot") == 0) {
        return CmdSnapshot(argc, argv);
    }
    else if (strcmp(subcmd, "allfx") == 0) {
        return CmdAllFx(argc, argv);
    }
    else if (strcmp(subcmd, "allbypass") == 0) {
        return CmdAllBypass(argc, argv);
    }
    else if (strcmp(subcmd, "query") == 0) {
        return CmdQuery(argc, argv);
    }
    else {
        Shell_Printf("ERROR: Unknown command '%s'\n", subcmd);
        PrintHelp();
        return -1;
    }
}

/**
 * @brief fx快捷命令入口 - 通过ID快速访问节点参数
 * 
 * 用法:
 *   fx <id>              - 显示节点信息
 *   fx <id> <param>      - 获取参数值
 *   fx <id> <param> <val>- 设置参数值
 */
int ShellCmdFx_Execute(int argc, char *argv[])
{
    EffectNode_t *node;
    
    if (argc < 2) {
        Shell_Printf("Usage: fx <id> [param] [value]\n");
        Shell_Printf("  fx 3           - Show node 3 info\n");
        Shell_Printf("  fx 3 threshold - Get threshold value\n");
        Shell_Printf("  fx 3 threshold -20 - Set threshold to -20\n");
        return 0;
    }
    
    /* 查找节点 */
    node = FindNode(argv[1]);
    if (!node) {
        Shell_Printf("ERROR: Node '%s' not found\n", argv[1]);
        return -1;
    }
    
    if (argc == 2) {
        /* fx <id> - 显示节点所有参数 */
        PrintNodeParams(node);
    }
    else if (argc == 3) {
        /* fx <id> <param> - 获取单个参数 */
        int32_t value;
        if (GetNodeParam(node, argv[2], &value) == 0) {
            Shell_Printf("[%d] %s = %ld\n", node->id, argv[2], (long)value);
        } else {
            Shell_Printf("ERROR: Unknown param '%s'\n", argv[2]);
            return -1;
        }
    }
    else {
        /* fx <id> <param> <value> - 设置参数 */
        int32_t value = atoi(argv[3]);
        if (SetNodeParam(node, argv[2], value) != 0) {
            Shell_Printf("ERROR: Failed to set '%s'\n", argv[2]);
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief EQ测试命令 - 一键应用测试配置
 * 
 * 用法:
 *   eq_test <node_id> bass     - 低音增强 (+12dB@60Hz, +8dB@150Hz, +4dB@300Hz)
 *   eq_test <node_id> treble   - 高音增强 (+8dB@5kHz, +10dB@10kHz)
 *   eq_test <node_id> vocal    - 人声增强 (+6dB@1kHz, +4dB@2kHz)
 *   eq_test <node_id> flat     - 平坦响应 (所有增益归零)
 * 
 * 示例:
 *   eq_test 10 bass    - 在USB/BT音乐路径上应用低音增强 (NODE_ID_USB_BT_EQ=10)
 *   eq_test 7 vocal     - 在ADC路径上应用人声增强 (NODE_ID_EQ=7)
 *   eq_test eq bass     - 也可以使用节点名称
 */
int ShellCmdEqTest_Execute(int argc, char *argv[])
{
    EffectNode_t *node;
    const char *preset;
    
    if (argc < 3) {
        Shell_Printf("Usage: eq_test <node_id|name> <preset>\n");
        Shell_Printf("Presets:\n");
        Shell_Printf("  bass   - Bass Boost (+12dB@60Hz, +8dB@150Hz, +4dB@300Hz)\n");
        Shell_Printf("  treble - Treble Boost (+8dB@5kHz, +10dB@10kHz)\n");
        Shell_Printf("  vocal  - Vocal Enhance (+6dB@1kHz, +4dB@2kHz)\n");
        Shell_Printf("  flat   - Flat Response (all gains to 0dB)\n");
        Shell_Printf("  debug  - Show EQ unit status and parameters\n");
        Shell_Printf("\nEQ Nodes:\n");
        Shell_Printf("  7 (eq)        - ADC mic/guitar EQ\n");
        Shell_Printf("  10 (usb_bt_eq) - USB/BT playback EQ\n");
        Shell_Printf("\nExample:\n");
        Shell_Printf("  eq_test 10 bass     - Bass boost on USB/BT\n");
        Shell_Printf("  eq_test usb_bt_eq treble - Treble boost on USB/BT\n");
        Shell_Printf("  eq_test 7 vocal     - Vocal enhance on ADC\n");
        Shell_Printf("  eq_test 10 debug    - Show USB/BT EQ status\n");
        return 0;
    }
    
    /* 查找节点 */
    node = FindNode(argv[1]);
    if (!node) {
        Shell_Printf("ERROR: Node '%s' not found\n", argv[1]);
        return -1;
    }
    
    /* 检查节点类型是否为EQ */
    if (node->type != EFFECT_NODE_TYPE_EFFECT_EQ) {
        Shell_Printf("ERROR: Node %d (%s) is not an EQ node\n", node->id, node->name);
        return -1;
    }
    
    preset = argv[2];
    Shell_Printf("Applying EQ preset '%s' to node %d (%s)...\n", preset, node->id, node->name);
    
    /* EQ启用和初始化由最后的 SetNodeParam(filter_count) 处理 */
    
    if (strcmp(preset, "bass") == 0) {
        /* Bass Boost: 60Hz/150Hz/300Hz */
        SetNodeParam(node, "band0_f0", 60);         // Band 0 freq
        SetNodeParam(node, "band0_type", 0);        // Band 0 type (Peaking)
        SetNodeParam(node, "band0", 12);            // Band 0 gain (+12dB)
        SetNodeParam(node, "band0_Q", 717);         // Band 0 Q (0.7)
        SetNodeParam(node, "band0_enable", 1);      // Band 0 enable
        
        SetNodeParam(node, "band1_f0", 150);        // Band 1 freq
        SetNodeParam(node, "band1_type", 0);        // Band 1 type
        SetNodeParam(node, "band1", 8);             // Band 1 gain (+8dB)
        SetNodeParam(node, "band1_Q", 717);         // Band 1 Q
        SetNodeParam(node, "band1_enable", 1);      // Band 1 enable
        
        SetNodeParam(node, "band2_f0", 300);        // Band 2 freq
        SetNodeParam(node, "band2_type", 0);        // Band 2 type
        SetNodeParam(node, "band2", 4);             // Band 2 gain (+4dB)
        SetNodeParam(node, "band2_Q", 100);         // Band 2 Q (1.0)
        SetNodeParam(node, "band2_enable", 1);      // Band 2 enable
        
        SetNodeParam(node, "filter_count", 3);      // filter_count = 3
        Shell_Printf("✓ Bass Boost applied: +12dB@60Hz, +8dB@150Hz, +4dB@300Hz\n");
    }
    else if (strcmp(preset, "treble") == 0) {
        /* Treble Boost: 5kHz/10kHz */
        SetNodeParam(node, "band0_f0", 5000);       // Band 0 freq
        SetNodeParam(node, "band0_type", 0);        // Band 0 type
        SetNodeParam(node, "band0", 8);             // Band 0 gain (+8dB)
        SetNodeParam(node, "band0_Q", 100);         // Band 0 Q (1.0)
        SetNodeParam(node, "band0_enable", 1);      // Band 0 enable
        
        SetNodeParam(node, "band1_f0", 10000);      // Band 1 freq
        SetNodeParam(node, "band1_type", 0);        // Band 1 type
        SetNodeParam(node, "band1", 10);            // Band 1 gain (+10dB)
        SetNodeParam(node, "band1_Q", 717);         // Band 1 Q (0.7)
        SetNodeParam(node, "band1_enable", 1);      // Band 1 enable
        
        SetNodeParam(node, "filter_count", 2);      // filter_count = 2
        Shell_Printf("✓ Treble Boost applied: +8dB@5kHz, +10dB@10kHz\n");
    }
    else if (strcmp(preset, "vocal") == 0) {
        /* Vocal Enhance: 1kHz/2kHz */
        SetNodeParam(node, "band0_f0", 1000);       // Band 0 freq
        SetNodeParam(node, "band0_type", 0);        // Band 0 type
        SetNodeParam(node, "band0", 6);             // Band 0 gain (+6dB)
        SetNodeParam(node, "band0_Q", 100);         // Band 0 Q (1.0)
        SetNodeParam(node, "band0_enable", 1);      // Band 0 enable
        
        SetNodeParam(node, "band1_f0", 2000);       // Band 1 freq
        SetNodeParam(node, "band1_type", 0);        // Band 1 type
        SetNodeParam(node, "band1", 4);             // Band 1 gain (+4dB)
        SetNodeParam(node, "band1_Q", 100);         // Band 1 Q (1.0)
        SetNodeParam(node, "band1_enable", 1);      // Band 1 enable
        
        SetNodeParam(node, "filter_count", 2);      // filter_count = 2
        Shell_Printf("✓ Vocal Enhance applied: +6dB@1kHz, +4dB@2kHz\n");
    }
    else if (strcmp(preset, "flat") == 0) {
        /* Flat Response: All gains to 0 */
        int i;
        for (i = 0; i < 10; i++) {
            char param_str[16];
            sprintf(param_str, "band%d", i);
            SetNodeParam(node, param_str, 0);       // All gains to 0
        }
        SetNodeParam(node, "filter_count", 0);      // filter_count = 0
        Shell_Printf("✓ Flat Response applied: All bands disabled\n");
    }
    else if (strcmp(preset, "debug") == 0) {
        /* Debug: 显示 EQ 单元状态 */
        extern ControlVariablesContext gCtrlVars;
        EQUnit *target_eq = (node->id == NODE_ID_USB_BT_EQ || strcmp(node->name, "usb_bt_eq") == 0) ? 
                            &gCtrlVars.music_out_eq_unit : &gCtrlVars.mic_out_eq_unit;
        int i;
        
        Shell_Printf("\n=== EQ Unit Debug Info ===\n");
        Shell_Printf("Target: %s\n", (target_eq == &gCtrlVars.music_out_eq_unit) ? 
                     "music_out_eq_unit" : "mic_out_eq_unit");
        Shell_Printf("  enable: %d\n", target_eq->enable);
        Shell_Printf("  filter_count: %d\n", target_eq->filter_count);
        Shell_Printf("  pregain: %ld (%.2f dB)\n", (long)target_eq->pregain, 
                     (float)target_eq->pregain / 256.0f);
        Shell_Printf("  channel: %d\n", target_eq->channel);
        Shell_Printf("  ct (EQContext): %s\n", target_eq->ct ? "allocated" : "NULL!");
        Shell_Printf("  filter_params: %s\n", target_eq->filter_params ? "valid" : "NULL!");
        
        Shell_Printf("\n--- Band Parameters (eq_params) ---\n");
        for (i = 0; i < 4 && i < target_eq->filter_count; i++) {
            Shell_Printf("  band%d: en=%d type=%ld f0=%lu Q=%ld gain=%ld(%.1fdB)\n",
                        i, target_eq->eq_params[i].enable,
                        (long)target_eq->eq_params[i].type,
                        (unsigned long)target_eq->eq_params[i].f0,
                        (long)target_eq->eq_params[i].Q,
                        (long)target_eq->eq_params[i].gain,
                        (float)target_eq->eq_params[i].gain / 256.0f);
        }
        
        if (target_eq->filter_params) {
            Shell_Printf("\n--- SDK Parameters (filter_params) ---\n");
            for (i = 0; i < 4 && i < target_eq->filter_count; i++) {
                Shell_Printf("  band%d: type=%d f0=%u Q=%d gain=%d(%.1fdB)\n",
                            i, target_eq->filter_params[i].type,
                            target_eq->filter_params[i].f0,
                            target_eq->filter_params[i].Q,
                            target_eq->filter_params[i].gain,
                            (float)target_eq->filter_params[i].gain / 256.0f);
            }
        }
        
        Shell_Printf("\n--- Node Parameters (node->params.eq) ---\n");
        Shell_Printf("  band_count: %d, pregain: %d\n", 
                    node->params.eq.band_count, node->params.eq.pregain);
        for (i = 0; i < 4 && i < node->params.eq.band_count; i++) {
            Shell_Printf("  band%d: gain=%d type=%d f0=%lu Q=%lu en=%d\n",
                        i, node->params.eq.band_gains[i],
                        node->params.eq.band_types[i],
                        (unsigned long)node->params.eq.band_f0[i],
                        (unsigned long)node->params.eq.band_Q[i],
                        node->params.eq.band_enables[i]);
        }
        
        Shell_Printf("=== End Debug ===\n");
        return 0;
    }
    else {
        Shell_Printf("ERROR: Unknown preset '%s'\n", preset);
        Shell_Printf("Available presets: bass, treble, vocal, flat, debug\n");
        return -1;
    }
    
    Shell_Printf("TIP: Use 'chain -S' to save configuration to flash\n");
    return 0;
}
