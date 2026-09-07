/**
 *****************************************************************************
 * @file     effect_graph.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    音频效果器图实现 - 基于图数据结构的音频处理链路
 *****************************************************************************
 */

#include "effect_graph.h"
#include "effect_graph_config.h"
#include <string.h>
#include <stdio.h>
#include "debug.h"
#include "rtos_api.h"  /* pvPortMalloc/vPortFree */

#if EFFECT_GRAPHICS_EN  /* 整个文件受 EFFECT_GRAPHICS_EN 控制，=0 时不编译，不分配任何 BSS */


/*******************************************************************************
 * 静态变量
 ******************************************************************************/
static EffectGraphRuntime_t g_EffectGraph;
static bool g_Initialized = false;

/* 静态节点缓冲区池
 * 21节点 × 256 samples × 4字节 = 21504字节 (21KB)
 * 位于BSS段，确保蓝牙和ADC有足够的缓冲空间 */
static uint32_t g_node_buf_pool[EFFECT_GRAPH_MAX_NODES][EFFECT_GRAPH_BUFFER_SIZE];

/*******************************************************************************
 * 内部辅助函数声明
 ******************************************************************************/
static bool IsSourceNode(EffectNodeType_t type);
static bool IsSinkNode(EffectNodeType_t type);
/* static bool IsProcessNode(EffectNodeType_t type); */ /* 未使�?*/
static void InitNodeDefaults(EffectNode_t *node, EffectNodeType_t type);
static GraphError_t TopologicalSort(void);

/*******************************************************************************
 * 内部辅助函数实现
 ******************************************************************************/

/* 判断是否为源节点 (包括 ADC0, ADC1, USB_IN, BT_IN, METRONOME, REMIND, LOOPER_PLAY) */
static bool IsSourceNode(EffectNodeType_t type)
{
    return (type >= EFFECT_NODE_TYPE_SOURCE_ADC0 && type <= EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY);
}

/* 判断是否为输出节点 (包括 DAC0, USB_OUT, LOOPER_RECORD) */
static bool IsSinkNode(EffectNodeType_t type)
{
    return (type >= EFFECT_NODE_TYPE_SINK_DAC0 && type <= EFFECT_NODE_TYPE_SINK_LOOPER_RECORD);
}

/* 判断是否为处理节�?(未使用，保留供将来扩�?
static bool IsProcessNode(EffectNodeType_t type)
{
    return (type >= EFFECT_NODE_TYPE_MIXER && type < EFFECT_NODE_TYPE_MAX);
}
*/

/* 初始化节点默认�?*/
static void InitNodeDefaults(EffectNode_t *node, EffectNodeType_t type)
{
    if (!node) return;
    
    node->type = type;
    node->state = NODE_STATE_IDLE;
    node->enabled = true;
    node->bypass = false;
    node->input_count = 0;
    node->output_count = 0;
    node->buffer_len = 0;
    node->processed = false;
    node->in_degree = 0;
    node->effect_ctx = NULL;
    node->avail_func = NULL;  /* 初始化可用数据量查询函数为空 */
    
    memset(node->inputs, 0, sizeof(node->inputs));
    memset(node->outputs, 0, sizeof(node->outputs));
    memset(&node->params, 0, sizeof(node->params));
    
    /* 根据类型设置默认参数 */
    switch (type) {
        case EFFECT_NODE_TYPE_MIXER:
            {
                int i;
                node->params.mixer.input_count = 0;
                for (i = 0; i < EFFECT_GRAPH_MAX_INPUTS; i++) {
                    node->params.mixer.input_gains[i] = 0; /* 0dB */
                }
            }
            break;
        case EFFECT_NODE_TYPE_EFFECT_GAIN:
            node->params.gain.gain_db = 0;
            break;
        case EFFECT_NODE_TYPE_EFFECT_REVERB:
            node->params.reverb.room_size = 50;
            node->params.reverb.damping = 50;
            node->params.reverb.wet_dry = 30;
            break;
        case EFFECT_NODE_TYPE_EFFECT_DRC:
            node->params.drc.threshold = -20;
            node->params.drc.ratio = 4;
            node->params.drc.attack = 10;
            node->params.drc.release = 100;
            break;
        case EFFECT_NODE_TYPE_EFFECT_EQ:
            node->params.eq.band_count = 5;
            memset(node->params.eq.band_gains, 0, sizeof(node->params.eq.band_gains));
            break;
        case EFFECT_NODE_TYPE_EFFECT_DELAY:
            node->params.delay.delay_ms = 250;
            node->params.delay.feedback = 30;
            node->params.delay.wet_dry = 30;
            break;
        default:
            break;
    }
}

/* 拓扑排序 - 确定处理顺序 */
static GraphError_t TopologicalSort(void)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    uint8_t in_degree[EFFECT_GRAPH_MAX_NODES];
    uint8_t queue[EFFECT_GRAPH_MAX_NODES];
    uint8_t front = 0, rear = 0;
    uint8_t i;
    uint8_t node_idx;
    EffectNode_t *node;
    uint8_t dst_idx;
    
    /* 计算所有节点的入度 */
    memset(in_degree, 0, sizeof(in_degree));
    for (i = 0; i < g->edge_count; i++) {
        if (g->edges[i].enabled && g->edges[i].dst_node) {
            in_degree[g->edges[i].dst_node->id]++;
        }
    }
    
    /* 将入度为0的节点加入队�?*/
    for (i = 0; i < g->node_count; i++) {
        g->nodes[i].in_degree = in_degree[i];
        if (in_degree[i] == 0) {
            queue[rear++] = i;
        }
    }
    
    /* BFS拓扑排序 */
    g->process_count = 0;
    while (front < rear) {
        node_idx = queue[front++];
        node = &g->nodes[node_idx];
        
        g->process_order[g->process_count++] = node;
        
        /* 减少相邻节点的入�?*/
        for (i = 0; i < g->edge_count; i++) {
            if (g->edges[i].enabled && 
                g->edges[i].src_node == node &&
                g->edges[i].dst_node) {
                dst_idx = g->edges[i].dst_node->id;
                in_degree[dst_idx]--;
                if (in_degree[dst_idx] == 0) {
                    queue[rear++] = dst_idx;
                }
            }
        }
    }
    
    /* 检查是否有�?*/
    if (g->process_count != g->node_count) {
        DBG("[EffectGraph] ERROR: Cycle detected in graph!\n");
        return GRAPH_ERR_CYCLE_DETECTED;
    }
    
    return GRAPH_OK;
}

/*******************************************************************************
 * 公共API实现
 ******************************************************************************/

// 初始化效果图系统
GraphError_t EffectGraph_Init(void)
{
    if (g_Initialized) {
        return GRAPH_OK;
    }
    
    memset(&g_EffectGraph, 0, sizeof(g_EffectGraph));
    g_EffectGraph.initialized = true;
    g_EffectGraph.sample_rate = 48000; /* 默认采样率 */
    
    /* 初始化驱动模式参数 */
    g_EffectGraph.drive_mode = DRIVE_MODE_ADC;  /* 默认 ADC 驱动模式 */
    g_EffectGraph.primary_source = NULL;
    g_EffectGraph.min_frame_size = 48;   /* 默认最小帧长 */
    g_EffectGraph.max_frame_size = 256;  /* 默认最大帧长 */
    
    DBG("[EffectGraph] Buffer pool: %u nodes × %u samples × 4 bytes = %u KB\n",
        EFFECT_GRAPH_MAX_NODES,
        EFFECT_GRAPH_BUFFER_SIZE,
        (unsigned)((EFFECT_GRAPH_MAX_NODES * EFFECT_GRAPH_BUFFER_SIZE * sizeof(uint32_t)) / 1024));
    
    g_Initialized = true;
    DBG("[EffectGraph] Initialized\n");
    
    return GRAPH_OK;
}

// 获取图实例
EffectGraphRuntime_t* EffectGraph_GetInstance(void)
{
    if (!g_Initialized) {
        return NULL;
    }
    return &g_EffectGraph;
}

// 从配置创建图
GraphError_t EffectGraph_CreateFromConfig(const GraphConfig_t *config)
{
    uint8_t i;
    EffectNode_t *node;
    const NodeConfig_t *nc;
    const EdgeConfig_t *ec;
    EffectNode_t *src;
    EffectNode_t *dst;
    GraphError_t err;
    
    if (!config) {
        return GRAPH_ERR_NULL_PTR;
    }
    if (!g_Initialized) {
        return GRAPH_ERR_NOT_INITIALIZED;
    }
    
    /* 重置�?*/
    EffectGraph_Reset();
    g_EffectGraph.sample_rate = config->sample_rate;
    
    /* 创建所有节�?*/
    for (i = 0; i < config->node_count; i++) {
        nc = &config->nodes[i];
        node = EffectGraph_AddNode(nc->type, nc->name, nc->enabled);
        if (!node) {
            DBG("[EffectGraph] Failed to add node: %s\n", nc->name);
            return GRAPH_ERR_NODE_FULL;
        }
        /* 设置节点参数 */
        memcpy(&node->params, &nc->params, sizeof(EffectParams_t));
    }
    
    /* 创建所有边(连接) */
    for (i = 0; i < config->edge_count; i++) {
        ec = &config->edges[i];
        src = EffectGraph_FindNodeById(ec->src_node_id);
        dst = EffectGraph_FindNodeById(ec->dst_node_id);
        
        if (!src || !dst) {
            DBG("[EffectGraph] Invalid edge: %d -> %d\n", ec->src_node_id, ec->dst_node_id);
            return GRAPH_ERR_INVALID_EDGE;
        }
        
        err = EffectGraph_Connect(src, dst, ec->src_port, ec->dst_port);
        if (err != GRAPH_OK) {
            return err;
        }
    }
    
    /* 构建处理顺序 */
    return EffectGraph_Build();
}

// 添加节点
EffectNode_t* EffectGraph_AddNode(EffectNodeType_t type, const char *name, bool enabled)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    EffectNode_t *node;
    
    if (!g_Initialized || !name) {
        return NULL;
    }
    if (g->node_count >= EFFECT_GRAPH_MAX_NODES) {
        DBG("[EffectGraph] Node pool full!\n");
        return NULL;
    }
    
    node = &g->nodes[g->node_count];
    node->id = g->node_count;
    
    /* 复制名称 */
    strncpy(node->name, name, EFFECT_GRAPH_NAME_LEN - 1);
    node->name[EFFECT_GRAPH_NAME_LEN - 1] = '\0';
    
    /* 初始化默认值*/
    InitNodeDefaults(node, type);
    node->enabled = enabled;
    
    /* 使用静态缓冲区池 */
    node->buffer = g_node_buf_pool[g->node_count];
    memset(node->buffer, 0, EFFECT_GRAPH_BUFFER_SIZE * sizeof(uint32_t));
    
    /* 分类存储 */
    if (IsSourceNode(type)) {
        if (g->source_count < 8) {   /* 最大 8 个源节点 (ADC0/1, USB, BT, Metro, LooperPlay + 预留) */
            g->source_nodes[g->source_count++] = node;
        }
    } else if (IsSinkNode(type)) {
        if (g->sink_count < 4) {     /* 最大 4 个 sink（DAC0, USB_OUT, Looper_Record + 预留） */
            g->sink_nodes[g->sink_count++] = node;
        }
    }
    
    g->node_count++;
    node->state = NODE_STATE_READY;
    
    DBG("[EffectGraph] Added node[%d]: %s (type=%d)\n", node->id, node->name, type);
    return node;
}

// 连接两个节点
GraphError_t EffectGraph_Connect(EffectNode_t *src_node, EffectNode_t *dst_node, uint8_t src_port, uint8_t dst_port)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    EffectEdge_t *edge;
    
    if (!src_node || !dst_node) {
        return GRAPH_ERR_NULL_PTR;
    }
    if (!g_Initialized) {
        return GRAPH_ERR_NOT_INITIALIZED;
    }
    if (g->edge_count >= EFFECT_GRAPH_MAX_EDGES) {
        DBG("[EffectGraph] Edge pool full!\n");
        return GRAPH_ERR_EDGE_FULL;
    }
    if (src_port >= EFFECT_GRAPH_MAX_OUTPUTS || dst_port >= EFFECT_GRAPH_MAX_INPUTS) {
        return GRAPH_ERR_INVALID_EDGE;
    }
    
    /* 创建�?*/
    edge = &g->edges[g->edge_count];
    edge->id = g->edge_count;
    edge->src_node = src_node;
    edge->dst_node = dst_node;
    edge->src_port = src_port;
    edge->dst_port = dst_port;
    edge->enabled = true;
    
    /* 更新节点连接信息 */
    if (src_node->output_count < EFFECT_GRAPH_MAX_OUTPUTS) {
        src_node->outputs[src_node->output_count++] = edge;
    }
    if (dst_node->input_count < EFFECT_GRAPH_MAX_INPUTS) {
        dst_node->inputs[dst_node->input_count++] = edge;
    }
    
    g->edge_count++;
    
    DBG("[EffectGraph] Connected: %s[%d] -> %s[%d]\n", 
        src_node->name, src_port, dst_node->name, dst_port);
    return GRAPH_OK;
}

// 断开两个节点的连接
GraphError_t EffectGraph_Disconnect(EffectNode_t *src_node, EffectNode_t *dst_node)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    uint8_t i, j;
    
    if (!src_node || !dst_node) {
        return GRAPH_ERR_NULL_PTR;
    }
    
    /* 查找并禁用对应的�?*/
    for (i = 0; i < g->edge_count; i++) {
        if (g->edges[i].src_node == src_node && g->edges[i].dst_node == dst_node) {
            g->edges[i].enabled = false;
            
            /* 从源节点的输出列表移�?*/
            for (j = 0; j < src_node->output_count; j++) {
                if (src_node->outputs[j] == &g->edges[i]) {
                    src_node->outputs[j] = NULL;
                    break;
                }
            }
            
            /* 从目标节点的输入列表移除 */
            for (j = 0; j < dst_node->input_count; j++) {
                if (dst_node->inputs[j] == &g->edges[i]) {
                    dst_node->inputs[j] = NULL;
                    break;
                }
            }
            
            DBG("[EffectGraph] Disconnected: %s -> %s\n", src_node->name, dst_node->name);
            return GRAPH_OK;
        }
    }
    
    return GRAPH_ERR_INVALID_EDGE;
}

// 构建处理顺序(拓扑排序)
GraphError_t EffectGraph_Build(void)
{
    GraphError_t err;
    uint8_t i;
    
    if (!g_Initialized) {
        return GRAPH_ERR_NOT_INITIALIZED;
    }
    
    err = TopologicalSort();
    if (err != GRAPH_OK) {
        return err;
    }
    
    DBG("[EffectGraph] Build complete, process order (%d nodes):\n", g_EffectGraph.process_count);
    for (i = 0; i < g_EffectGraph.process_count; i++) {
        DBG("  [%d] %s\n", i, g_EffectGraph.process_order[i]->name);
    }
    
    return GRAPH_OK;
}

// 处理一帧音频
uint16_t EffectGraph_Process(uint16_t frame_size)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    uint8_t i, j;
    uint16_t actual_len = 0;
    uint32_t *in_bufs[EFFECT_GRAPH_MAX_INPUTS];  /* 使用 uint32_t 与硬件接口统一 */
    uint8_t in_count;
    uint16_t max_len;
    uint16_t k;
    EffectNode_t *node;
    EffectNode_t *src;
    
    if (!g_Initialized || frame_size == 0) {
        return 0;
    }
    if (frame_size > EFFECT_GRAPH_BUFFER_SIZE) {
        frame_size = EFFECT_GRAPH_BUFFER_SIZE;
    }
    
    /* 重置所有节点的处理标志 */
    for (i = 0; i < g->node_count; i++) {
        g->nodes[i].processed = false;
        g->nodes[i].buffer_len = 0;
    }
    
    /* 按拓扑顺序处理每个节�?*/
    for (i = 0; i < g->process_count; i++) {
        node = g->process_order[i];
        
        if (!node->enabled) {
            node->processed = true;
            continue;
        }
        
        /* 根据节点类型处理 */
        if (IsSourceNode(node->type)) {
            /* 源节�? 产生数据 */
            if (node->func.source) {
                node->buffer_len = node->func.source(node, node->buffer, frame_size);
            }
        }
        else if (IsSinkNode(node->type)) {
            /* 输出节点: 消费数据 */
            /* 获取输入数据 */
            if (node->input_count > 0 && node->inputs[0] && node->inputs[0]->src_node) {
                src = node->inputs[0]->src_node;
                if (node->func.sink && src->buffer_len > 0) {
                    node->func.sink(node, src->buffer, src->buffer_len);
                }
                actual_len = src->buffer_len;
            }
        }
        else {
            /* 处理节点: 处理数据 */
            in_count = 0;
            max_len = 0;
            
            /* 收集所有输�?*/
            for (j = 0; j < node->input_count && j < EFFECT_GRAPH_MAX_INPUTS; j++) {
                if (node->inputs[j] && node->inputs[j]->enabled && node->inputs[j]->src_node) {
                    src = node->inputs[j]->src_node;
                    in_bufs[in_count++] = src->buffer;
                    if (src->buffer_len > max_len) {
                        max_len = src->buffer_len;
                    }
                }
            }
            
            /* 处理数据 */
            if (in_count > 0) {
                if (node->bypass && in_count == 1) {
                    /* 旁路模式: 直接复制输入 */
                    memcpy(node->buffer, in_bufs[0], max_len * sizeof(uint32_t));
                    node->buffer_len = max_len;
                }
                else if (node->func.process) {
                    node->func.process(node, in_bufs, in_count, node->buffer, max_len);
                    node->buffer_len = max_len;
                }
                else if (node->type == EFFECT_NODE_TYPE_MIXER) {
                    /* 默认混音器处�? 简单相�?*/
                    memset(node->buffer, 0, max_len * sizeof(uint32_t));
                    for (j = 0; j < in_count; j++) {
                        for (k = 0; k < max_len; k++) {
                            node->buffer[k] += in_bufs[j][k];
                        }
                    }
                    node->buffer_len = max_len;
                }
                else {
                    /* 默认: 复制第一个输�?*/
                    memcpy(node->buffer, in_bufs[0], max_len * sizeof(uint32_t));
                    node->buffer_len = max_len;
                }
            }
        }
        
        node->processed = true;
    }
    
    return actual_len;
}

// 设置节点启用状态
void EffectGraph_SetNodeEnabled(EffectNode_t *node, bool enabled)
{
    if (!node) {
        return;
    }
    
    if (node->enabled == enabled) {
        return;  /* 状态未变，跳过 */
    }
    
    /* 静态缓冲区池：buffer 始终有效，无需 malloc/free，只更新 enabled 标志 */
    node->enabled = enabled;
    DBG("[EffectGraph] Node %s %s\n", node->name, enabled ? "enabled" : "disabled");
}

// 设置节点旁路状态
void EffectGraph_SetNodeBypass(EffectNode_t *node, bool bypass)
{
    if (node) {
        node->bypass = bypass;
        DBG("[EffectGraph] Node %s bypass %s\n", node->name, bypass ? "ON" : "OFF");
    }
}

// 根据名称查找节点
EffectNode_t* EffectGraph_FindNodeByName(const char *name)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    uint8_t i;
    
    if (!name || !g_Initialized) {
        return NULL;
    }
    
    for (i = 0; i < g->node_count; i++) {
        if (strcmp(g->nodes[i].name, name) == 0) {
            return &g->nodes[i];
        }
    }
    
    return NULL;
}

// 根据ID查找节点
EffectNode_t* EffectGraph_FindNodeById(uint8_t id)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    
    if (!g_Initialized || id >= g->node_count) {
        return NULL;
    }
    
    return &g->nodes[id];
}

// 设置节点参数
GraphError_t EffectGraph_SetNodeParams(EffectNode_t *node, const EffectParams_t *params)
{
    if (!node || !params) {
        return GRAPH_ERR_NULL_PTR;
    }
    
    memcpy(&node->params, params, sizeof(EffectParams_t));
    DBG("[EffectGraph] Node %s params updated\n", node->name);
    
    return GRAPH_OK;
}

// 重置�?清除所有节点和�?
void EffectGraph_Reset(void)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    uint8_t i;
    
    if (!g_Initialized) {
        return;
    }
    
    /* 清除节点（静态缓冲区池，无需 free）*/
    for (i = 0; i < g->node_count; i++) {
        memset(&g->nodes[i], 0, sizeof(EffectNode_t));
    }
    /* 清零节点缓冲区池，以便下次 AddNode 时持有干净的缓冲区 */
    memset(g_node_buf_pool, 0, sizeof(g_node_buf_pool));
    
    /* 清除�?*/
    memset(g->edges, 0, sizeof(g->edges));
    
    /* 重置计数�?*/
    g->node_count = 0;
    g->edge_count = 0;
    g->process_count = 0;
    g->source_count = 0;
    g->sink_count = 0;
    
    memset(g->source_nodes, 0, sizeof(g->source_nodes));
    memset(g->sink_nodes, 0, sizeof(g->sink_nodes));
    memset(g->process_order, 0, sizeof(g->process_order));
    
    DBG("[EffectGraph] Reset complete\n");
}

// 打印图信�?调试�?
void EffectGraph_PrintInfo(void)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    uint8_t i, j;
    
    if (!g_Initialized) {
        DBG("[EffectGraph] Not initialized!\n");
        return;
    }
    
    DBG("\n========== Effect Graph Info ==========\n");
    DBG("Sample Rate: %d Hz\n", g->sample_rate);
    DBG("Nodes: %d, Edges: %d\n", g->node_count, g->edge_count);
    DBG("Sources: %d, Sinks: %d\n", g->source_count, g->sink_count);
    
    DBG("\n--- Nodes ---\n");
    for (i = 0; i < g->node_count; i++) {
        EffectNode_t *n = &g->nodes[i];
        DBG("[%d] %s (type=%d, enabled=%d, bypass=%d)\n",
            n->id, n->name, n->type, n->enabled, n->bypass);
        
        /* 打印输入连接 */
        if (n->input_count > 0) {
            DBG("    Inputs: ");
            for (j = 0; j < n->input_count; j++) {
                if (n->inputs[j] && n->inputs[j]->src_node) {
                    DBG("%s ", n->inputs[j]->src_node->name);
                }
            }
            DBG("\n");
        }
        
        /* 打印输出连接 */
        if (n->output_count > 0) {
            DBG("    Outputs: ");
            for (j = 0; j < n->output_count; j++) {
                if (n->outputs[j] && n->outputs[j]->dst_node) {
                    DBG("%s ", n->outputs[j]->dst_node->name);
                }
            }
            DBG("\n");
        }
    }
    
    DBG("\n--- Process Order ---\n");
    for (i = 0; i < g->process_count; i++) {
        DBG("[%d] %s\n", i, g->process_order[i]->name);
    }
    
    DBG("========================================\n\n");
}

// 创建默认音频�?ADC0+ADC1 -> Mixer -> Effects -> DAC0)
// 现在通过配置系统加载，方便修改参数而不需要改代码
GraphError_t EffectGraph_CreateDefault(uint16_t sample_rate)
{
    (void)sample_rate; /* 使用配置文件中的采样�?*/
    
    /* 直接使用配置系统加载默认预设 */
    return EffectGraphConfig_LoadPreset(GRAPH_PRESET_DEFAULT);
}

/*******************************************************************************
 * 自适应帧长处理 - 核心功能
 ******************************************************************************/

// 设置驱动模式
GraphError_t EffectGraph_SetDriveMode(GraphDriveMode_t mode, EffectNode_t *primary_source)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    uint8_t i;
    static GraphDriveMode_t last_mode = 0xFF;  /* 上次的模式（初始为非法值） */
    bool mode_changed = (last_mode != mode);
    
    if (!g_Initialized) {
        return GRAPH_ERR_NOT_INITIALIZED;
    }
    
    g->drive_mode = mode;
    
    /* 如果指定了主驱动源，使用�?*/
    if (primary_source) {
        g->primary_source = primary_source;
        if (mode_changed) {
            DBG("[EffectGraph] Drive mode set to %d, primary source: %s\n", 
                mode, primary_source->name);
            last_mode = mode;
        }
        return GRAPH_OK;
    }
    
    /* 否则根据模式自动选择主驱动源 */
    g->primary_source = NULL;
    
    /* 注意: ADC模式下不设置单一主驱动源�?     * 而是�?GetAvailableFrameSize 去查询所有ADC源的最小可用量 */
    if (mode != DRIVE_MODE_ADC) {
        for (i = 0; i < g->source_count; i++) {
            EffectNode_t *src = g->source_nodes[i];
            if (!src || !src->enabled) continue;
            
            switch (mode) {
                case DRIVE_MODE_BT:
                    if (src->type == EFFECT_NODE_TYPE_SOURCE_BT_IN) {
                        g->primary_source = src;
                    }
                    break;
                case DRIVE_MODE_USB:
                    if (src->type == EFFECT_NODE_TYPE_SOURCE_USB_IN) {
                        g->primary_source = src;
                    }
                    break;
                default:
                    break;
            }
            if (g->primary_source) break;
        }
    }
    
    /* 只在模式切换时打印一�?*/
    if (mode_changed) {
        if (g->primary_source) {
            DBG("[EffectGraph] Drive mode set to %d, auto selected: %s\n",
                mode, g->primary_source->name);
        } else {
            DBG("[EffectGraph] Drive mode set to %d, no primary source found (ADC mode)\n", mode);
        }
        last_mode = mode;
    }
    
    return GRAPH_OK;
}

// 查询所有源节点的可用数据量
uint16_t EffectGraph_GetAvailableFrameSize(void)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    uint16_t min_avail = 0xFFFF;
    uint16_t avail;
    uint8_t i;
    bool first = true;
    
    if (!g_Initialized) {
        return 0;
    }
    
    /* 如果有主驱动源，优先使用它的可用数据�?*/
    if (g->primary_source && g->primary_source->enabled && g->primary_source->avail_func) {
        avail = g->primary_source->avail_func(g->primary_source);
        
        /* 应用帧长限制 */
        if (g->max_frame_size > 0 && avail > g->max_frame_size) {
            avail = g->max_frame_size;
        }
        if (avail < g->min_frame_size) {
            return 0; /* 数据不足最小帧�?*/
        }
        return avail;
    }
    
    /* 没有主驱动源 (ADC 模式)，查询所�?ADC 类型的源节点 */
    for (i = 0; i < g->source_count; i++) {
        EffectNode_t *src = g->source_nodes[i];
        if (!src || !src->enabled || !src->avail_func) continue;
        
        /* ADC 驱动模式下只查询 ADC 类型的源节点 */
        if (g->drive_mode == DRIVE_MODE_ADC) {
            if (src->type != EFFECT_NODE_TYPE_SOURCE_ADC0 && 
                src->type != EFFECT_NODE_TYPE_SOURCE_ADC1) {
                continue;
            }
        }
        
        avail = src->avail_func(src);
        if (first || avail < min_avail) {
            min_avail = avail;
            first = false;
        }
    }
    
    if (first) {
        return 0; /* 没有有效的源节点 */
    }
    
    /* 应用帧长限制 */
    if (g->max_frame_size > 0 && min_avail > g->max_frame_size) {
        min_avail = g->max_frame_size;
    }
    if (min_avail < g->min_frame_size) {
        return 0;
    }
    
    return min_avail;
}

// 自适应帧长处理 - 根据主驱动源的可用数据量决定帧长
uint16_t EffectGraph_ProcessAdaptive(void)
{
    EffectGraphRuntime_t *g = &g_EffectGraph;
    uint16_t frame_size;
    uint16_t actual_len;
    uint8_t i, j;
    uint32_t *in_bufs[EFFECT_GRAPH_MAX_INPUTS];  /* 使用 uint32_t 与硬件接口统一 */
    uint8_t in_count;
    uint16_t max_len;
    uint16_t k;
    EffectNode_t *node;
    EffectNode_t *src;
    
    if (!g_Initialized) {
        return 0;
    }
    
    /* 第一�? 获取本帧应处理的帧长 */
    frame_size = EffectGraph_GetAvailableFrameSize();
    if (frame_size == 0) {
        return 0; /* 数据不足 */
    }
    
    if (frame_size > EFFECT_GRAPH_BUFFER_SIZE) {
        frame_size = EFFECT_GRAPH_BUFFER_SIZE;
    }
    
    actual_len = 0;
    
    /* 第二�? 重置所有节点的处理标志 */
    for (i = 0; i < g->node_count; i++) {
        g->nodes[i].processed = false;
        g->nodes[i].buffer_len = 0;
    }
    
    /* 第三�? 按拓扑顺序处理每个节�?*/
    for (i = 0; i < g->process_count; i++) {
        node = g->process_order[i];
        
        if (!node->enabled) {
            node->processed = true;
            continue;
        }
        
        /* 根据节点类型处理 */
        if (IsSourceNode(node->type)) {
            /* 源节�? 产生数据 */
            /* 关键: 源节点使�?frame_size 作为请求长度, 返回实际读取长度 */
            if (node->func.source) {
                node->buffer_len = node->func.source(node, node->buffer, frame_size);
            }
        }
        else if (IsSinkNode(node->type)) {
            /* 输出节点: 消费数据 */
            if (node->input_count > 0 && node->inputs[0] && node->inputs[0]->src_node) {
                src = node->inputs[0]->src_node;
                if (node->func.sink && src->buffer_len > 0) {
                    node->func.sink(node, src->buffer, src->buffer_len);
                }
                actual_len = src->buffer_len;
            }
        }
        else {
            /* 处理节点: 处理数据 */
            in_count = 0;
            max_len = 0;
            
            /* 收集所有输入，找最大有效长�?*/
            for (j = 0; j < node->input_count && j < EFFECT_GRAPH_MAX_INPUTS; j++) {
                if (node->inputs[j] && node->inputs[j]->enabled && node->inputs[j]->src_node) {
                    src = node->inputs[j]->src_node;
                    in_bufs[in_count++] = src->buffer;
                    if (src->buffer_len > max_len) {
                        max_len = src->buffer_len;
                    }
                }
            }
            
            /* 处理数据 */
            if (in_count > 0 && max_len > 0) {
                if (node->bypass && in_count == 1) {
                    /* 旁路模式: 直接复制输入 */
                    memcpy(node->buffer, in_bufs[0], max_len * sizeof(uint32_t));
                    node->buffer_len = max_len;
                }
                else if (node->func.process) {
                    node->func.process(node, in_bufs, in_count, node->buffer, max_len);
                    node->buffer_len = max_len;
                }
                else if (node->type == EFFECT_NODE_TYPE_MIXER) {
                    /* 默认混音器处�? 简单相�?*/
                    memset(node->buffer, 0, max_len * sizeof(uint32_t));
                    for (j = 0; j < in_count; j++) {
                        for (k = 0; k < max_len; k++) {
                            node->buffer[k] += in_bufs[j][k];
                        }
                    }
                    node->buffer_len = max_len;
                }
                else {
                    /* 默认: 复制第一个输�?*/
                    memcpy(node->buffer, in_bufs[0], max_len * sizeof(uint32_t));
                    node->buffer_len = max_len;
                }
            }
        }
        
        node->processed = true;
    }
    
    return actual_len;
}

#endif /* EFFECT_GRAPHICS_EN */
