/**
 *****************************************************************************
 * @file     effect_graph_config.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    音频效果器图配置实现 - 预设管理和参数加载
 *****************************************************************************
 */

#include "effect_graph_config.h"
#include "effect_graph.h"
#include <string.h>
#include "debug.h"

/*******************************************************************************
 * 静态变量
 ******************************************************************************/
static GraphPreset_t g_CurrentPreset = GRAPH_PRESET_DEFAULT;

/*******************************************************************************
 * 默认配置数据(静态存储)
 ******************************************************************************/

/* 默认节点配置 */
static const NodeConfig_t g_DefaultNodes[] = DEFAULT_NODES_CONFIG;

/* 默认边配置 */
static const EdgeConfig_t g_DefaultEdges[] = DEFAULT_EDGES_CONFIG;

/* 编译时断言：确保数组大小与宏定义一致 */
#define COMPILE_TIME_ASSERT(cond) typedef char assertion_failed_##__LINE__[(cond) ? 1 : -1]
COMPILE_TIME_ASSERT(sizeof(g_DefaultNodes)/sizeof(g_DefaultNodes[0]) == DEFAULT_NODE_COUNT);
COMPILE_TIME_ASSERT(sizeof(g_DefaultEdges)/sizeof(g_DefaultEdges[0]) == DEFAULT_EDGE_COUNT);

/* 运行时检查用的大小常量 */
static const uint8_t g_DefaultNodesArraySize = sizeof(g_DefaultNodes)/sizeof(g_DefaultNodes[0]);
static const uint8_t g_DefaultEdgesArraySize = sizeof(g_DefaultEdges)/sizeof(g_DefaultEdges[0]);

/* 简单配置节点 */
static const NodeConfig_t g_SimpleNodes[] = SIMPLE_NODES_CONFIG;

/* 简单配置边 */
static const EdgeConfig_t g_SimpleEdges[] = SIMPLE_EDGES_CONFIG;

/* 蓝牙音箱配置节点 */
static const NodeConfig_t g_BtSpeakerNodes[] = BT_SPEAKER_NODES_CONFIG;

/* 蓝牙音箱配置边 */
static const EdgeConfig_t g_BtSpeakerEdges[] = BT_SPEAKER_EDGES_CONFIG;

/* 副音箱配置节点 */
static const NodeConfig_t g_SecondaryNodes[] = SECONDARY_SPEAKER_NODES_CONFIG;

/* 副音箱配置边 */
static const EdgeConfig_t g_SecondaryEdges[] = SECONDARY_SPEAKER_EDGES_CONFIG;

/* Secondary Speaker运行时检查用的大小常量 */
static const uint8_t g_SecondaryNodesArraySize = sizeof(g_SecondaryNodes)/sizeof(g_SecondaryNodes[0]);
static const uint8_t g_SecondaryEdgesArraySize = sizeof(g_SecondaryEdges)/sizeof(g_SecondaryEdges[0]);

/* 编译时断言：确保Secondary Speaker数组大小与宏定义一致 */
COMPILE_TIME_ASSERT(sizeof(g_SecondaryNodes)/sizeof(g_SecondaryNodes[0]) == SECONDARY_SPEAKER_NODE_COUNT);
COMPILE_TIME_ASSERT(sizeof(g_SecondaryEdges)/sizeof(g_SecondaryEdges[0]) == SECONDARY_SPEAKER_EDGE_COUNT);

/*******************************************************************************
 * 预设名称表
 ******************************************************************************/
static const char* g_PresetNames[] = {
    "Default (Full)",
    "Simple (No FX)",
    "Guitar Only",
    "Mic Only",
    "Bluetooth Speaker",
    "USB Audio",
    "Secondary Speaker"
};

/*******************************************************************************
 * 内部函数
 ******************************************************************************/

/**
 * @brief 设置效果器节点的默认参数
 */
static void SetDefaultEffectParams(EffectNode_t *node)
{
    if (!node) return;
    
    switch (node->type) {
        case EFFECT_NODE_TYPE_EFFECT_REVERB:
            node->params.reverb.room_size = DEFAULT_REVERB_ROOM_SIZE;
            node->params.reverb.damping = DEFAULT_REVERB_DAMPING;
            node->params.reverb.wet_dry = DEFAULT_REVERB_WET_DRY;
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DRC:
            node->params.drc.threshold = DEFAULT_DRC_THRESHOLD;
            node->params.drc.ratio = DEFAULT_DRC_RATIO;
            node->params.drc.attack = DEFAULT_DRC_ATTACK;
            node->params.drc.release = DEFAULT_DRC_RELEASE;
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_EQ:
            node->params.eq.band_count = DEFAULT_EQ_BAND_COUNT;
            memset(node->params.eq.band_gains, 0, sizeof(node->params.eq.band_gains));
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_EXPANDER:
            node->params.expander.threshold = DEFAULT_EXPANDER_THRESHOLD;
            node->params.expander.ratio = DEFAULT_EXPANDER_RATIO;
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_GAIN:
            node->params.gain.gain_db = DEFAULT_GAIN_DB;
            break;
            
        case EFFECT_NODE_TYPE_EFFECT_DELAY:
            node->params.delay.delay_ms = DEFAULT_DELAY_MS;
            node->params.delay.feedback = DEFAULT_DELAY_FEEDBACK;
            node->params.delay.wet_dry = DEFAULT_DELAY_WET_DRY;
            break;
            
        case EFFECT_NODE_TYPE_MIXER:
            {
                int i;
                node->params.mixer.input_count = EFFECT_GRAPH_MAX_INPUTS;
                for (i = 0; i < EFFECT_GRAPH_MAX_INPUTS; i++) {
                    node->params.mixer.input_gains[i] = 0; /* 0dB */
                }
            }
            break;
            
        default:
            break;
    }
}

/*******************************************************************************
 * 公共API实现
 ******************************************************************************/

/**
 * @brief 获取预设配置
 */
GraphError_t EffectGraphConfig_GetPreset(GraphPreset_t preset, GraphConfig_t *config)
{
    if (!config) {
        return GRAPH_ERR_NULL_PTR;
    }
    
    memset(config, 0, sizeof(GraphConfig_t));
    config->sample_rate = DEFAULT_SAMPLE_RATE;
    
    switch (preset) {
        case GRAPH_PRESET_DEFAULT:
            config->nodes = (NodeConfig_t*)g_DefaultNodes;
            config->node_count = g_DefaultNodesArraySize;  /* 使用实际数组大小 */
            config->edges = (EdgeConfig_t*)g_DefaultEdges;
            config->edge_count = g_DefaultEdgesArraySize;  /* 使用实际数组大小，避免宏值不匹配 */
            DBG("[GraphConfig] DEFAULT preset: node_count=%d, edge_count=%d, array_sizes: nodes=%d, edges=%d\n",
                DEFAULT_NODE_COUNT, DEFAULT_EDGE_COUNT, g_DefaultNodesArraySize, g_DefaultEdgesArraySize);
            break;
            
        case GRAPH_PRESET_SIMPLE:
            config->nodes = (NodeConfig_t*)g_SimpleNodes;
            config->node_count = SIMPLE_NODE_COUNT;
            config->edges = (EdgeConfig_t*)g_SimpleEdges;
            config->edge_count = SIMPLE_EDGE_COUNT;
            break;
            
        case GRAPH_PRESET_BLUETOOTH:
            config->nodes = (NodeConfig_t*)g_BtSpeakerNodes;
            config->node_count = BT_SPEAKER_NODE_COUNT;
            config->edges = (EdgeConfig_t*)g_BtSpeakerEdges;
            config->edge_count = BT_SPEAKER_EDGE_COUNT;
            break;
            
        case GRAPH_PRESET_SECONDARY_SPEAKER:
            config->nodes = (NodeConfig_t*)g_SecondaryNodes;
            config->node_count = g_SecondaryNodesArraySize;  /* 使用实际数组大小 */
            config->edges = (EdgeConfig_t*)g_SecondaryEdges;
            config->edge_count = g_SecondaryEdgesArraySize;  /* 使用实际数组大小 */
            DBG("[GraphConfig] SECONDARY_SPEAKER preset: node_count=%d, edge_count=%d\n",
                config->node_count, config->edge_count);
            break;
            
        case GRAPH_PRESET_GUITAR_ONLY:
        case GRAPH_PRESET_MIC_ONLY:
        case GRAPH_PRESET_USB_AUDIO:
            /* TODO: 实现这些预设 */
            DBG("[GraphConfig] Preset %d not implemented, using default\n", preset);
            config->nodes = (NodeConfig_t*)g_DefaultNodes;
            config->node_count = g_DefaultNodesArraySize;  /* 使用实际数组大小 */
            config->edges = (EdgeConfig_t*)g_DefaultEdges;
            config->edge_count = g_DefaultEdgesArraySize;  /* 使用实际数组大小 */
            break;
            
        default:
            DBG("[GraphConfig] Invalid preset: %d\n", preset);
            return GRAPH_ERR_INVALID_NODE;
    }
    
    return GRAPH_OK;
}

/**
 * @brief 从预设创建图
 */
GraphError_t EffectGraphConfig_LoadPreset(GraphPreset_t preset)
{
    GraphConfig_t config;
    GraphError_t err;
    EffectGraphRuntime_t *graph;
    uint8_t i;
    
    DBG("[GraphConfig] Loading preset: %s\n", 
        (preset < GRAPH_PRESET_MAX) ? g_PresetNames[preset] : "Unknown");
    
    /* 确保图系统已初始化 */
    err = EffectGraph_Init();
    if (err != GRAPH_OK) {
        return err;
    }
    
    /* 获取预设配置 */
    err = EffectGraphConfig_GetPreset(preset, &config);
    if (err != GRAPH_OK) {
        return err;
    }
    
    /* 运行时验证: 打印实际配置值 */
    DBG("[GraphConfig] Config: nodes=%d (expect %d), edges=%d (expect %d)\n",
        config.node_count, DEFAULT_NODE_COUNT, 
        config.edge_count, DEFAULT_EDGE_COUNT);
    
    /* 重置图 */
    EffectGraph_Reset();
    
    graph = EffectGraph_GetInstance();
    if (!graph) {
        return GRAPH_ERR_NOT_INITIALIZED;
    }
    graph->sample_rate = config.sample_rate;
    
    /* 创建所有节点 */
    for (i = 0; i < config.node_count; i++) {
        const NodeConfig_t *nc = &config.nodes[i];
        EffectNode_t *node = EffectGraph_AddNode(nc->type, nc->name, nc->enabled);
        if (!node) {
            DBG("[GraphConfig] Failed to add node: %s\n", nc->name);
            return GRAPH_ERR_NODE_FULL;
        }
        
        /* 设置效果器默认参数 */
        SetDefaultEffectParams(node);
    }
    
    /* 创建所有边(连接) */
    /* 计算实际的边数组大小 */
    size_t actual_edges_array_size = 0;
    if (config.edges == g_DefaultEdges) {
        actual_edges_array_size = g_DefaultEdgesArraySize;
    } else if (config.edges == g_SimpleEdges) {
        actual_edges_array_size = sizeof(g_SimpleEdges)/sizeof(g_SimpleEdges[0]);
    } else if (config.edges == g_BtSpeakerEdges) {
        actual_edges_array_size = sizeof(g_BtSpeakerEdges)/sizeof(g_BtSpeakerEdges[0]);
    } else if (config.edges == g_SecondaryEdges) {
        actual_edges_array_size = sizeof(g_SecondaryEdges)/sizeof(g_SecondaryEdges[0]);
    } else {
        /* 默认使用默认数组大小 */
        actual_edges_array_size = g_DefaultEdgesArraySize;
    }

    DBG("[GraphConfig] Creating %d edges (array size=%d)...\n", config.edge_count, (int)actual_edges_array_size);

    /* 安全检查：防止数组越界 */
    if (config.edge_count > actual_edges_array_size) {
        DBG("[GraphConfig] ERROR: edge_count (%d) > array_size (%d)! Limiting to array size.\n",
            config.edge_count, (int)actual_edges_array_size);
        config.edge_count = actual_edges_array_size;
    }
    
    for (i = 0; i < config.edge_count; i++) {
        const EdgeConfig_t *ec = &config.edges[i];
        EffectNode_t *src = EffectGraph_FindNodeById(ec->src_node_id);
        EffectNode_t *dst = EffectGraph_FindNodeById(ec->dst_node_id);
        
        /* 在连接前打印边信息，帮助调试 */
        if (i >= 15 || !src || !dst) {
            DBG("[GraphConfig] Edge[%d]: src_id=%d, dst_id=%d, src=%p, dst=%p\n",
                i, ec->src_node_id, ec->dst_node_id, (void*)src, (void*)dst);
        }
        
        if (!src || !dst) {
            DBG("[GraphConfig] Invalid edge: %d -> %d\n", ec->src_node_id, ec->dst_node_id);
            return GRAPH_ERR_INVALID_EDGE;
        }
        
        err = EffectGraph_Connect(src, dst, ec->src_port, ec->dst_port);
        if (err != GRAPH_OK) {
            return err;
        }
    }
    
    /* 构建处理顺序 */
    err = EffectGraph_Build();
    if (err != GRAPH_OK) {
        return err;
    }
    
    g_CurrentPreset = preset;
    
    DBG("[GraphConfig] Preset '%s' loaded successfully\n", g_PresetNames[preset]);
    EffectGraph_PrintInfo();
    
    return GRAPH_OK;
}

/**
 * @brief 获取当前预设ID
 */
GraphPreset_t EffectGraphConfig_GetCurrentPreset(void)
{
    return g_CurrentPreset;
}

/**
 * @brief 打印所有可用预设
 */
void EffectGraphConfig_PrintPresets(void)
{
    int i;
    DBG("\n===== Available Graph Presets =====\n");
    for (i = 0; i < GRAPH_PRESET_MAX; i++) {
        DBG("  [%d] %s%s\n", i, g_PresetNames[i], 
            (i == g_CurrentPreset) ? " (current)" : "");
    }
    DBG("===================================\n\n");
}
