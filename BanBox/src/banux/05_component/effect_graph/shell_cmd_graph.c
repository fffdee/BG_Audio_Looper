/**
 *****************************************************************************
 * @file     shell_cmd_graph.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    音频效果图Shell命令模块实现
 *****************************************************************************
 */

#include "shell_cmd_graph.h"
#include "effect_graph.h"
#include "effect_graph_config.h"
#include <string.h>
#include <stdio.h>
#include "debug.h"

/*******************************************************************************
 * 内部辅助函数
 ******************************************************************************/

/* 打印帮助信息 */
static void PrintHelp(void)
{
    Shell_Printf("\n===== Effect Graph Commands =====\n");
    Shell_Printf("graph list                  - List all nodes\n");
    Shell_Printf("graph info                  - Show graph details\n");
    Shell_Printf("graph preset [id]           - Switch/show presets\n");
    Shell_Printf("graph node <name> [on|off]  - Enable/disable node\n");
    Shell_Printf("graph bypass <name> [on|off]- Set node bypass\n");
    Shell_Printf("graph param <name> <key> [val] - Read/set parameters\n");
    Shell_Printf("graph rebuild               - Rebuild graph\n");
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
        
        Shell_Printf("[%d] %s (%s) - %s %s\n", 
                     node->id, 
                     node->name, 
                     type_str,
                     node->enabled ? "ON" : "OFF",
                     node->bypass ? "[BYPASS]" : "");
    }
    
    Shell_Printf("===============================\n\n");
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

/* 启用/禁用节点 */
static int CmdNode(int argc, char *argv[])
{
    if (argc < 3) {
        Shell_Printf("Usage: graph node <name> [on|off]\n");
        return -1;
    }
    
    EffectNode_t *node = EffectGraph_FindNodeByName(argv[2]);
    if (!node) {
        Shell_Printf("ERROR: Node '%s' not found\n", argv[2]);
        return -1;
    }
    
    if (argc >= 4) {
        bool enabled = (strcmp(argv[3], "on") == 0);
        EffectGraph_SetNodeEnabled(node, enabled);
        Shell_Printf("Node '%s' %s\n", node->name, enabled ? "enabled" : "disabled");
    } else {
        Shell_Printf("Node '%s' is %s\n", node->name, node->enabled ? "enabled" : "disabled");
    }
    
    return 0;
}

/* 设置节点旁路 */
static int CmdBypass(int argc, char *argv[])
{
    if (argc < 3) {
        Shell_Printf("Usage: graph bypass <name> [on|off]\n");
        return -1;
    }
    
    EffectNode_t *node = EffectGraph_FindNodeByName(argv[2]);
    if (!node) {
        Shell_Printf("ERROR: Node '%s' not found\n", argv[2]);
        return -1;
    }
    
    if (argc >= 4) {
        bool bypass = (strcmp(argv[3], "on") == 0);
        EffectGraph_SetNodeBypass(node, bypass);
        Shell_Printf("Node '%s' bypass %s\n", node->name, bypass ? "ON" : "OFF");
    } else {
        Shell_Printf("Node '%s' bypass is %s\n", node->name, node->bypass ? "ON" : "OFF");
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
 * 公共API实现
 ******************************************************************************/

void ShellCmdGraph_Register(void)
{
    /* 在这里可以注册到Shell系统 */
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
    else if (strcmp(subcmd, "param") == 0) {
        return CmdParam(argc, argv);
    }
    else if (strcmp(subcmd, "rebuild") == 0) {
        return CmdRebuild();
    }
    else {
        Shell_Printf("ERROR: Unknown command '%s'\n", subcmd);
        PrintHelp();
        return -1;
    }
}
