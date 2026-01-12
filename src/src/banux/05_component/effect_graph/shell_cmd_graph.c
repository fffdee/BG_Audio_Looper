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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"

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
    { "band0", -12, 12, "dB" },
    { "band1", -12, 12, "dB" },
    { "band2", -12, 12, "dB" },
    { "band3", -12, 12, "dB" },
    { "band4", -12, 12, "dB" },
    { "band5", -12, 12, "dB" },
    { "band6", -12, 12, "dB" },
    { "band7", -12, 12, "dB" },
    { "band8", -12, 12, "dB" },
    { "band9", -12, 12, "dB" },
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
        case NODE_TYPE_EFFECT_REVERB:   return g_ReverbParamRange;
        case NODE_TYPE_EFFECT_DRC:      return g_DrcParamRange;
        case NODE_TYPE_EFFECT_EQ:       return g_EqParamRange;
        case NODE_TYPE_EFFECT_GAIN:     return g_GainParamRange;
        case NODE_TYPE_EFFECT_DELAY:    return g_DelayParamRange;
        case NODE_TYPE_EFFECT_EXPANDER: return g_ExpanderParamRange;
        case NODE_TYPE_MIXER:           return g_MixerParamRange;
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
        case NODE_TYPE_EFFECT_REVERB:
            Shell_Printf("Type: REVERB\n");
            Shell_Printf("  room   = %d (0-100)\n", node->params.reverb.room_size);
            Shell_Printf("  damp   = %d (0-100)\n", node->params.reverb.damping);
            Shell_Printf("  wet    = %d (0-100)\n", node->params.reverb.wet_dry);
            break;
            
        case NODE_TYPE_EFFECT_DRC:
            Shell_Printf("Type: DRC\n");
            Shell_Printf("  threshold = %d dB\n", node->params.drc.threshold);
            Shell_Printf("  ratio     = %d\n", node->params.drc.ratio);
            Shell_Printf("  attack    = %d ms\n", node->params.drc.attack);
            Shell_Printf("  release   = %d ms\n", node->params.drc.release);
            break;
            
        case NODE_TYPE_EFFECT_EQ:
            Shell_Printf("Type: EQ (%d bands)\n", node->params.eq.band_count);
            {
                uint8_t i;
                for (i = 0; i < node->params.eq.band_count && i < 10; i++) {
                    Shell_Printf("  band%d = %d dB\n", i, node->params.eq.band_gains[i]);
                }
            }
            break;
            
        case NODE_TYPE_EFFECT_GAIN:
            Shell_Printf("Type: GAIN\n");
            Shell_Printf("  gain = %d dB\n", node->params.gain.gain_db);
            break;
            
        case NODE_TYPE_EFFECT_DELAY:
            Shell_Printf("Type: DELAY\n");
            Shell_Printf("  time     = %d ms\n", node->params.delay.delay_ms);
            Shell_Printf("  feedback = %d (0-100)\n", node->params.delay.feedback);
            Shell_Printf("  wet      = %d (0-100)\n", node->params.delay.wet_dry);
            break;
            
        case NODE_TYPE_EFFECT_EXPANDER:
            Shell_Printf("Type: EXPANDER\n");
            Shell_Printf("  threshold = %d dB\n", node->params.expander.threshold);
            Shell_Printf("  ratio     = %d\n", node->params.expander.ratio);
            break;
            
        case NODE_TYPE_MIXER:
            Shell_Printf("Type: MIXER (%d inputs)\n", node->params.mixer.input_count);
            {
                uint8_t i;
                for (i = 0; i < node->params.mixer.input_count && i < EFFECT_GRAPH_MAX_INPUTS; i++) {
                    Shell_Printf("  in%d_gain = %d dB\n", i, node->params.mixer.input_gains[i]);
                }
            }
            break;
            
        case NODE_TYPE_SOURCE_ADC0:
        case NODE_TYPE_SOURCE_ADC1:
        case NODE_TYPE_SOURCE_USB_IN:
        case NODE_TYPE_SOURCE_BT_IN:
            Shell_Printf("Type: SOURCE (no params)\n");
            break;
            
        case NODE_TYPE_SINK_DAC0:
        case NODE_TYPE_SINK_USB_OUT:
            Shell_Printf("Type: SINK (no params)\n");
            break;
            
        default:
            Shell_Printf("Type: Unknown (%d)\n", node->type);
            break;
    }
    Shell_Printf("========================\n\n");
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
        case NODE_TYPE_EFFECT_REVERB:
            if (strcmp(param_name, "room") == 0) {
                node->params.reverb.room_size = (uint8_t)value;
            } else if (strcmp(param_name, "damp") == 0) {
                node->params.reverb.damping = (uint8_t)value;
            } else if (strcmp(param_name, "wet") == 0) {
                node->params.reverb.wet_dry = (uint8_t)value;
            } else {
                return -1;
            }
            break;
            
        case NODE_TYPE_EFFECT_DRC:
            if (strcmp(param_name, "threshold") == 0) {
                node->params.drc.threshold = (int16_t)value;
            } else if (strcmp(param_name, "ratio") == 0) {
                node->params.drc.ratio = (uint8_t)value;
            } else if (strcmp(param_name, "attack") == 0) {
                node->params.drc.attack = (uint8_t)value;
            } else if (strcmp(param_name, "release") == 0) {
                node->params.drc.release = (uint8_t)value;
            } else {
                return -1;
            }
            break;
            
        case NODE_TYPE_EFFECT_EQ:
            /* 支持 band0, band1, ..., band9 */
            if (strncmp(param_name, "band", 4) == 0 && param_name[4] >= '0' && param_name[4] <= '9') {
                uint8_t band = param_name[4] - '0';
                if (band < 10) {
                    node->params.eq.band_gains[band] = (int8_t)value;
                } else {
                    return -1;
                }
            } else {
                return -1;
            }
            break;
            
        case NODE_TYPE_EFFECT_GAIN:
            if (strcmp(param_name, "gain") == 0) {
                node->params.gain.gain_db = (int16_t)value;
            } else {
                return -1;
            }
            break;
            
        case NODE_TYPE_EFFECT_DELAY:
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
            
        case NODE_TYPE_EFFECT_EXPANDER:
            if (strcmp(param_name, "threshold") == 0) {
                node->params.expander.threshold = (int16_t)value;
            } else if (strcmp(param_name, "ratio") == 0) {
                node->params.expander.ratio = (uint8_t)value;
            } else {
                return -1;
            }
            break;
            
        case NODE_TYPE_MIXER:
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
        case NODE_TYPE_EFFECT_REVERB:
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
            
        case NODE_TYPE_EFFECT_DRC:
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
            
        case NODE_TYPE_EFFECT_EQ:
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
            
        case NODE_TYPE_EFFECT_GAIN:
            if (strcmp(param_name, "gain") == 0) {
                *value = node->params.gain.gain_db;
            } else {
                return -1;
            }
            break;
            
        case NODE_TYPE_EFFECT_DELAY:
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
            
        case NODE_TYPE_EFFECT_EXPANDER:
            if (strcmp(param_name, "threshold") == 0) {
                *value = node->params.expander.threshold;
            } else if (strcmp(param_name, "ratio") == 0) {
                *value = node->params.expander.ratio;
            } else {
                return -1;
            }
            break;
            
        case NODE_TYPE_MIXER:
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
    Shell_Printf("\n--- ID-based quick commands (fx) ---\n");
    Shell_Printf("fx <id> <key> [val]         - Quick get/set by ID\n");
    Shell_Printf("fx <id>                     - Show node info by ID\n");
    Shell_Printf("==================================\n\n");
}

/* 列出所有节点 */
static int CmdList(void)
{
    EffectGraph_t *graph = EffectGraph_GetInstance();
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
            case NODE_TYPE_SOURCE_ADC0: type_str = "ADC0"; break;
            case NODE_TYPE_SOURCE_ADC1: type_str = "ADC1"; break;
            case NODE_TYPE_SOURCE_USB_IN: type_str = "USB_IN"; break;
            case NODE_TYPE_SOURCE_BT_IN: type_str = "BT_IN"; break;
            case NODE_TYPE_SINK_DAC0: type_str = "DAC0"; break;
            case NODE_TYPE_SINK_USB_OUT: type_str = "USB_OUT"; break;
            case NODE_TYPE_MIXER: type_str = "MIXER"; break;
            case NODE_TYPE_EFFECT_REVERB: type_str = "REVERB"; break;
            case NODE_TYPE_EFFECT_DRC: type_str = "DRC"; break;
            case NODE_TYPE_EFFECT_EQ: type_str = "EQ"; break;
            case NODE_TYPE_EFFECT_EXPANDER: type_str = "EXPANDER"; break;
            case NODE_TYPE_EFFECT_HOWLING: type_str = "HOWLING"; break;
            case NODE_TYPE_EFFECT_NOISE_GATE: type_str = "NOISE_GATE"; break;
            case NODE_TYPE_EFFECT_GAIN: type_str = "GAIN"; break;
            case NODE_TYPE_EFFECT_DELAY: type_str = "DELAY"; break;
            case NODE_TYPE_EFFECT_CHORUS: type_str = "CHORUS"; break;
            case NODE_TYPE_LOOPER: type_str = "LOOPER"; break;
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
        case NODE_TYPE_EFFECT_REVERB:
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
            
        case NODE_TYPE_EFFECT_DRC:
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
            
        case NODE_TYPE_EFFECT_GAIN:
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
            
        case NODE_TYPE_EFFECT_DELAY:
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
    EffectGraph_t *graph;
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
    EffectGraph_t *graph;
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
    EffectGraph_t *graph;
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
        if (node->type >= NODE_TYPE_MIXER && node->type < NODE_TYPE_MAX) {
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
    EffectGraph_t *graph;
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
        if (node->type >= NODE_TYPE_MIXER && node->type < NODE_TYPE_MAX) {
            EffectGraph_SetNodeBypass(node, bypass);
            count++;
        }
    }
    
    Shell_Printf("All effects bypass %s (%d nodes)\n", bypass ? "ON" : "OFF", count);
    return 0;
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

void ShellCmdGraph_Register(void)
{
    /* 注册到Shell系统 */
    Shell_RegisterModule(&g_GraphModule);
    Shell_RegisterModule(&g_FxModule);
    DBG("[ShellCmdGraph] Registered\n");
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
