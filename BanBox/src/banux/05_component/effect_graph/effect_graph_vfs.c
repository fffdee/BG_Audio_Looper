/**
 *****************************************************************************
 * @file     effect_graph_vfs.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     06-January-2026
 * @brief    效果图虚拟文件系统 - 将效果图参数映射到/audio目录
 *****************************************************************************
 * @attention
 *
 * 目录结构：
 *   /audio
 *       ├── graph0/              (默认效果图)
 *       │   ├── info             (只读：图信息)
 *       │   ├── preset           (读写：当前预设ID)
 *       │   ├── node_count       (只读：节点数量)
 *       │   └── nodes/
 *       │       ├── 0_adc0/
 *       │       │   ├── enabled  (读写：0/1)
 *       │       │   ├── bypass   (读写：0/1)
 *       │       │   └── type     (只读)
 *       │       ├── 3_drc/
 *       │       │   ├── enabled
 *       │       │   ├── bypass
 *       │       │   ├── type
 *       │       │   ├── threshold
 *       │       │   ├── ratio
 *       │       │   ├── attack
 *       │       │   └── release
 *       │       └── ...
 *       └── graph1/              (可动态创建)
 *
 *****************************************************************************
 */

#include "effect_graph_vfs.h"
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "vfs.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*******************************************************************************
 * 配置定义
 ******************************************************************************/
#define GRAPH_VFS_MAX_GRAPHS     4    /* 最大支持4个效果图实例 */
#define GRAPH_VFS_MAX_PARAMS    16    /* 每个节点最多16个参数 */

/*******************************************************************************
 * 静态变量
 ******************************************************************************/
static VfsNode_t *g_AudioDir = NULL;
static bool g_GraphVfsInitialized = FALSE;
static GraphVfsHandle_t g_GraphHandles[GRAPH_VFS_MAX_GRAPHS];

/*******************************************************************************
 * 参数读写回调 - 节点通用属性
 ******************************************************************************/

/**
 * @brief 读取节点enabled属性
 */
static int NodeEnabledGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->enabled ? 1 : 0);
}

/**
 * @brief 设置节点enabled属性
 */
static int NodeEnabledSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    int val = atoi(value);
    EffectGraph_SetNodeEnabled(node, val != 0);
    return 0;
}

/**
 * @brief 读取节点bypass属性
 */
static int NodeBypassGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->bypass ? 1 : 0);
}

/**
 * @brief 设置节点bypass属性
 */
static int NodeBypassSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    int val = atoi(value);
    EffectGraph_SetNodeBypass(node, val != 0);
    return 0;
}

/**
 * @brief 读取节点类型
 */
static int NodeTypeGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    const char *type_str = "unknown";
    
    if (!node || !buf) return -1;
    
    switch (node->type) {
        case NODE_TYPE_SOURCE_ADC0: type_str = "adc0"; break;
        case NODE_TYPE_SOURCE_ADC1: type_str = "adc1"; break;
        case NODE_TYPE_SOURCE_USB_IN: type_str = "usb_in"; break;
        case NODE_TYPE_SOURCE_BT_IN: type_str = "bt_in"; break;
        case NODE_TYPE_SINK_DAC0: type_str = "dac0"; break;
        case NODE_TYPE_SINK_USB_OUT: type_str = "usb_out"; break;
        case NODE_TYPE_MIXER: type_str = "mixer"; break;
        case NODE_TYPE_EFFECT_REVERB: type_str = "reverb"; break;
        case NODE_TYPE_EFFECT_DRC: type_str = "drc"; break;
        case NODE_TYPE_EFFECT_EQ: type_str = "eq"; break;
        case NODE_TYPE_EFFECT_EXPANDER: type_str = "expander"; break;
        case NODE_TYPE_EFFECT_HOWLING: type_str = "howling"; break;
        case NODE_TYPE_EFFECT_NOISE_GATE: type_str = "noise_gate"; break;
        case NODE_TYPE_EFFECT_GAIN: type_str = "gain"; break;
        case NODE_TYPE_EFFECT_DELAY: type_str = "delay"; break;
        case NODE_TYPE_EFFECT_CHORUS: type_str = "chorus"; break;
        case NODE_TYPE_LOOPER: type_str = "looper"; break;
        default: break;
    }
    
    return snprintf(buf, maxLen, "%s", type_str);
}

/*******************************************************************************
 * 参数读写回调 - 效果器特定参数
 ******************************************************************************/

/* DRC参数 */
static int DrcThresholdGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.drc.threshold);
}

static int DrcThresholdSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.drc.threshold = (int16_t)atoi(value);
    return 0;
}

static int DrcRatioGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.drc.ratio);
}

static int DrcRatioSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.drc.ratio = (uint8_t)atoi(value);
    return 0;
}

static int DrcAttackGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.drc.attack);
}

static int DrcAttackSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.drc.attack = (uint8_t)atoi(value);
    return 0;
}

static int DrcReleaseGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.drc.release);
}

static int DrcReleaseSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.drc.release = (uint8_t)atoi(value);
    return 0;
}

/* Reverb参数 */
static int ReverbRoomGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.reverb.room_size);
}

static int ReverbRoomSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.reverb.room_size = (uint8_t)atoi(value);
    return 0;
}

static int ReverbDampGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.reverb.damping);
}

static int ReverbDampSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.reverb.damping = (uint8_t)atoi(value);
    return 0;
}

static int ReverbWetGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.reverb.wet_dry);
}

static int ReverbWetSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.reverb.wet_dry = (uint8_t)atoi(value);
    return 0;
}

/* Gain参数 */
static int GainGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.gain.gain_db);
}

static int GainSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.gain.gain_db = (int16_t)atoi(value);
    return 0;
}

/* Delay参数 */
static int DelayTimeGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.delay.delay_ms);
}

static int DelayTimeSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.delay.delay_ms = (uint16_t)atoi(value);
    return 0;
}

static int DelayFeedbackGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.delay.feedback);
}

static int DelayFeedbackSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.delay.feedback = (uint8_t)atoi(value);
    return 0;
}

static int DelayWetGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.delay.wet_dry);
}

static int DelayWetSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.delay.wet_dry = (uint8_t)atoi(value);
    return 0;
}

/* Expander参数 */
static int ExpanderThresholdGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.expander.threshold);
}

static int ExpanderThresholdSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.expander.threshold = (int16_t)atoi(value);
    return 0;
}

static int ExpanderRatioGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", node->params.expander.ratio);
}

static int ExpanderRatioSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    if (!node || !value) return -1;
    node->params.expander.ratio = (uint8_t)atoi(value);
    return 0;
}

/* EQ参数 (band0-band9) */
typedef struct {
    EffectNode_t *node;
    uint8_t band;
} EqBandCtx_t;

static EqBandCtx_t g_EqBandCtx[10];  /* 简化处理，实际需要动态分配 */

static int EqBandGet(char *buf, uint16_t maxLen, void *userData)
{
    EqBandCtx_t *ctx = (EqBandCtx_t *)userData;
    if (!ctx || !ctx->node || !buf) return -1;
    return snprintf(buf, maxLen, "%d", ctx->node->params.eq.band_gains[ctx->band]);
}

static int EqBandSet(const char *value, void *userData)
{
    EqBandCtx_t *ctx = (EqBandCtx_t *)userData;
    if (!ctx || !ctx->node || !value) return -1;
    ctx->node->params.eq.band_gains[ctx->band] = (int8_t)atoi(value);
    return 0;
}

/*******************************************************************************
 * 图级参数读写回调
 ******************************************************************************/

static int GraphInfoGet(char *buf, uint16_t maxLen, void *userData)
{
    GraphVfsHandle_t *handle = (GraphVfsHandle_t *)userData;
    if (!handle || !handle->graph || !buf) return -1;
    
    return snprintf(buf, maxLen, "name=%s nodes=%d sr=%d",
                    handle->name,
                    handle->graph->node_count,
                    handle->graph->sample_rate);
}

static int GraphPresetGet(char *buf, uint16_t maxLen, void *userData)
{
    (void)userData;
    return snprintf(buf, maxLen, "%d", EffectGraphConfig_GetCurrentPreset());
}

static int GraphPresetSet(const char *value, void *userData)
{
    (void)userData;
    int preset = atoi(value);
    if (preset < 0 || preset >= GRAPH_PRESET_MAX) return -1;
    EffectGraphConfig_LoadPreset((GraphPreset_t)preset);
    return 0;
}

static int GraphNodeCountGet(char *buf, uint16_t maxLen, void *userData)
{
    GraphVfsHandle_t *handle = (GraphVfsHandle_t *)userData;
    if (!handle || !handle->graph || !buf) return -1;
    return snprintf(buf, maxLen, "%d", handle->graph->node_count);
}

/*******************************************************************************
 * 内部辅助函数
 ******************************************************************************/

/**
 * @brief 为节点创建参数子目录
 */
static void CreateNodeParams(VfsNode_t *nodeDir, EffectNode_t *node)
{
    /* 通用属性 */
    Vfs_CreateParam(nodeDir, "enabled", "Node enabled (0/1)", 
                    NodeEnabledGet, NodeEnabledSet, node);
    Vfs_CreateParam(nodeDir, "bypass", "Node bypass (0/1)", 
                    NodeBypassGet, NodeBypassSet, node);
    Vfs_CreateParam(nodeDir, "type", "Node type (readonly)", 
                    NodeTypeGet, NULL, node);
    
    /* 根据节点类型创建特定参数 */
    switch (node->type) {
        case NODE_TYPE_EFFECT_DRC:
            Vfs_CreateParam(nodeDir, "threshold", "Threshold dB (-60~0)", 
                            DrcThresholdGet, DrcThresholdSet, node);
            Vfs_CreateParam(nodeDir, "ratio", "Ratio (1~20)", 
                            DrcRatioGet, DrcRatioSet, node);
            Vfs_CreateParam(nodeDir, "attack", "Attack ms (1~500)", 
                            DrcAttackGet, DrcAttackSet, node);
            Vfs_CreateParam(nodeDir, "release", "Release ms (10~2000)", 
                            DrcReleaseGet, DrcReleaseSet, node);
            break;
            
        case NODE_TYPE_EFFECT_REVERB:
            Vfs_CreateParam(nodeDir, "room", "Room size (0~100)", 
                            ReverbRoomGet, ReverbRoomSet, node);
            Vfs_CreateParam(nodeDir, "damp", "Damping (0~100)", 
                            ReverbDampGet, ReverbDampSet, node);
            Vfs_CreateParam(nodeDir, "wet", "Wet/Dry (0~100)", 
                            ReverbWetGet, ReverbWetSet, node);
            break;
            
        case NODE_TYPE_EFFECT_GAIN:
            Vfs_CreateParam(nodeDir, "gain", "Gain dB (-60~+20)", 
                            GainGet, GainSet, node);
            break;
            
        case NODE_TYPE_EFFECT_DELAY:
            Vfs_CreateParam(nodeDir, "time", "Delay ms (10~1000)", 
                            DelayTimeGet, DelayTimeSet, node);
            Vfs_CreateParam(nodeDir, "feedback", "Feedback (0~100)", 
                            DelayFeedbackGet, DelayFeedbackSet, node);
            Vfs_CreateParam(nodeDir, "wet", "Wet/Dry (0~100)", 
                            DelayWetGet, DelayWetSet, node);
            break;
            
        case NODE_TYPE_EFFECT_EXPANDER:
            Vfs_CreateParam(nodeDir, "threshold", "Threshold dB (-80~0)", 
                            ExpanderThresholdGet, ExpanderThresholdSet, node);
            Vfs_CreateParam(nodeDir, "ratio", "Ratio (1~10)", 
                            ExpanderRatioGet, ExpanderRatioSet, node);
            break;
            
        case NODE_TYPE_EFFECT_EQ:
            /* EQ bands - 简化实现 */
            {
                int i;
                char bandName[8];
                for (i = 0; i < 10 && i < node->params.eq.band_count; i++) {
                    snprintf(bandName, sizeof(bandName), "band%d", i);
                    /* 注意：需要为每个band创建独立的上下文，这里简化处理 */
                    g_EqBandCtx[i].node = node;
                    g_EqBandCtx[i].band = i;
                    Vfs_CreateParam(nodeDir, bandName, "EQ band dB (-12~+12)", 
                                    EqBandGet, EqBandSet, &g_EqBandCtx[i]);
                }
            }
            break;
            
        default:
            /* 源/输出节点无额外参数 */
            break;
    }
}

/**
 * @brief 查找空闲的图句柄槽位
 */
static GraphVfsHandle_t* FindFreeHandle(void)
{
    int i;
    for (i = 0; i < GRAPH_VFS_MAX_GRAPHS; i++) {
        if (!g_GraphHandles[i].mounted) {
            return &g_GraphHandles[i];
        }
    }
    return NULL;
}

/**
 * @brief 通过名称查找图句柄
 */
static GraphVfsHandle_t* FindHandleByName(const char *name)
{
    int i;
    for (i = 0; i < GRAPH_VFS_MAX_GRAPHS; i++) {
        if (g_GraphHandles[i].mounted && strcmp(g_GraphHandles[i].name, name) == 0) {
            return &g_GraphHandles[i];
        }
    }
    return NULL;
}

/*******************************************************************************
 * 公共API实现
 ******************************************************************************/

GraphVfsError_t EffectGraphVfs_Init(void)
{
    VfsNode_t *root;
    
    DBG("[GraphVfs] Init: Start\n");
    
    if (g_GraphVfsInitialized) {
        DBG("[GraphVfs] Init: Already initialized\n");
        return GRAPH_VFS_OK;
    }
    
    DBG("[GraphVfs] Init: Getting VFS root...\n");
    root = Vfs_GetRoot();
    if (!root) {
        DBG("[GraphVfs] ERROR: VFS not initialized!\n");
        return GRAPH_VFS_ERR_NOT_INITIALIZED;
    }
    
    DBG("[GraphVfs] Init: Creating /audio directory...\n");
    
    /* 创建 /audio 目录 */
    g_AudioDir = Vfs_CreateDir(root, "audio");
    if (!g_AudioDir) {
        DBG("[GraphVfs] ERROR: Failed to create /audio\n");
        return GRAPH_VFS_ERR_NO_MEMORY;
    }
    
    /* 初始化句柄数组 */
    memset(g_GraphHandles, 0, sizeof(g_GraphHandles));
    
    DBG("[GraphVfs] /audio created successfully at %p\n", g_AudioDir);
    g_GraphVfsInitialized = TRUE;
    
    return GRAPH_VFS_OK;
}

GraphVfsHandle_t* EffectGraphVfs_Mount(const char *graph_name, EffectGraph_t *graph)
{
    GraphVfsHandle_t *handle;
    VfsNode_t *graphDir, *nodesDir;
    uint8_t i;
    char nodeDirName[24];
    
    DBG("[GraphVfs] Mount: Start mounting '%s'\n", graph_name ? graph_name : "NULL");
    
    if (!g_GraphVfsInitialized || !graph_name || !graph) {
        DBG("[GraphVfs] Mount: ERROR - Invalid params (init=%d, name=%p, graph=%p)\n",
            g_GraphVfsInitialized, graph_name, graph);
        return NULL;
    }
    
    DBG("[GraphVfs] Mount: Checking if '%s' already exists...\n", graph_name);
    
    /* 检查是否已存在 */
    if (FindHandleByName(graph_name)) {
        DBG("[GraphVfs] ERROR: Graph '%s' already mounted\n", graph_name);
        return NULL;
    }
    
    /* 获取空闲槽位 */
    DBG("[GraphVfs] Mount: Finding free handle slot...\n");
    handle = FindFreeHandle();
    if (!handle) {
        DBG("[GraphVfs] ERROR: No free handle slots\n");
        return NULL;
    }
    
    DBG("[GraphVfs] Mount: Creating directory /audio/%s\n", graph_name);
    
    /* 创建图目录 /audio/graph_name */
    graphDir = Vfs_CreateDir(g_AudioDir, graph_name);
    if (!graphDir) {
        DBG("[GraphVfs] ERROR: Failed to create /audio/%s\n", graph_name);
        return NULL;
    }
    
    /* 初始化句柄 */
    strncpy(handle->name, graph_name, sizeof(handle->name) - 1);
    handle->graph = graph;
    handle->root_dir = graphDir;
    
    /* 创建图级参数 */
    handle->info_node = Vfs_CreateParam(graphDir, "info", "Graph info (readonly)",
                                        GraphInfoGet, NULL, handle);
    handle->preset_node = Vfs_CreateParam(graphDir, "preset", "Current preset ID",
                                          GraphPresetGet, GraphPresetSet, handle);
    Vfs_CreateParam(graphDir, "node_count", "Node count (readonly)",
                    GraphNodeCountGet, NULL, handle);
    
    /* 创建 nodes/ 目录 */
    nodesDir = Vfs_CreateDir(graphDir, "nodes");
    if (!nodesDir) {
        DBG("[GraphVfs] ERROR: Failed to create nodes directory\n");
        Vfs_RemoveNode(graphDir);
        return NULL;
    }
    handle->nodes_dir = nodesDir;
    
    /* 为每个节点创建子目录 */
    for (i = 0; i < graph->node_count; i++) {
        EffectNode_t *node = &graph->nodes[i];
        VfsNode_t *nodeDir;
        
        /* 目录名格式: <id>_<name>, 如 "3_drc" */
        snprintf(nodeDirName, sizeof(nodeDirName), "%d_%s", node->id, node->name);
        
        nodeDir = Vfs_CreateDir(nodesDir, nodeDirName);
        if (nodeDir) {
            CreateNodeParams(nodeDir, node);
        }
    }
    
    handle->mounted = true;
    DBG("[GraphVfs] Graph '%s' mounted at /audio/%s (%d nodes)\n", 
        graph_name, graph_name, graph->node_count);
    
    return handle;
}

GraphVfsError_t EffectGraphVfs_Unmount(GraphVfsHandle_t *handle)
{
    if (!handle || !handle->mounted) {
        return GRAPH_VFS_ERR_NOT_FOUND;
    }
    
    /* 删除VFS节点树 */
    if (handle->root_dir) {
        Vfs_RemoveNode(handle->root_dir);
    }
    
    /* 清除句柄 */
    memset(handle, 0, sizeof(GraphVfsHandle_t));
    
    DBG("[GraphVfs] Graph unmounted\n");
    return GRAPH_VFS_OK;
}

GraphVfsError_t EffectGraphVfs_Refresh(GraphVfsHandle_t *handle)
{
    if (!handle || !handle->mounted || !handle->graph) {
        return GRAPH_VFS_ERR_NOT_FOUND;
    }
    
    /* 简单实现：卸载后重新挂载 */
    char name[16];
    EffectGraph_t *graph = handle->graph;
    
    strncpy(name, handle->name, sizeof(name) - 1);
    EffectGraphVfs_Unmount(handle);
    EffectGraphVfs_Mount(name, graph);
    
    return GRAPH_VFS_OK;
}

VfsNode_t* EffectGraphVfs_GetAudioDir(void)
{
    return g_AudioDir;
}

EffectNode_t* EffectGraphVfs_FindNodeByPath(const char *path)
{
    VfsNode_t *vfsNode;
    
    if (!path) return NULL;
    
    vfsNode = Vfs_FindNode(path);
    if (!vfsNode || vfsNode->type != VFS_NODE_DIR) {
        return NULL;
    }
    
    /* 从VFS节点获取关联的EffectNode */
    /* 注意：这里假设节点目录的userData存储了EffectNode指针 */
    return (EffectNode_t *)vfsNode->userData;
}

GraphVfsHandle_t* EffectGraphVfs_CreateGraph(const char *graph_name, GraphPreset_t preset_id)
{
    /* 创建新的效果图实例 */
    /* 注意：这需要效果图系统支持多实例，当前简化实现 */
    EffectGraph_t *graph;
    
    if (!graph_name) return NULL;
    
    /* 获取默认图实例 */
    graph = EffectGraph_GetInstance();
    if (!graph) {
        DBG("[GraphVfs] ERROR: Failed to get graph instance\n");
        return NULL;
    }
    
    /* 加载预设 */
    if (preset_id < GRAPH_PRESET_MAX) {
        EffectGraphConfig_LoadPreset(preset_id);
    }
    
    /* 挂载到VFS */
    return EffectGraphVfs_Mount(graph_name, graph);
}

GraphVfsError_t EffectGraphVfs_DeleteGraph(const char *graph_name)
{
    GraphVfsHandle_t *handle = FindHandleByName(graph_name);
    if (!handle) {
        return GRAPH_VFS_ERR_NOT_FOUND;
    }
    return EffectGraphVfs_Unmount(handle);
}

int EffectGraphVfs_ListGraphs(void (*callback)(const char *name, GraphVfsHandle_t *handle))
{
    int count = 0;
    int i;
    
    for (i = 0; i < GRAPH_VFS_MAX_GRAPHS; i++) {
        if (g_GraphHandles[i].mounted) {
            if (callback) {
                callback(g_GraphHandles[i].name, &g_GraphHandles[i]);
            }
            count++;
        }
    }
    
    return count;
}

/*******************************************************************************
 * 初始化 - 自动挂载默认效果图
 ******************************************************************************/

/**
 * @brief 挂载默认效果图（系统启动时调用）
 * @note 如果效果图未初始化，会静默返回OK，稍后可调用此函数重试
 */
GraphVfsError_t EffectGraphVfs_MountDefault(void)
{
    EffectGraph_t *graph;
    GraphVfsError_t err;
    
    /* 初始化VFS */
    err = EffectGraphVfs_Init();
    if (err != GRAPH_VFS_OK) {
        return err;
    }
    
    /* 检查是否已挂载 */
    if (EffectGraphVfs_ListGraphs(NULL) > 0) {
        DBG("[GraphVfs] Default graph already mounted\n");
        return GRAPH_VFS_OK;
    }
    
    /* 获取默认效果图实例 */
    graph = EffectGraph_GetInstance();
    if (!graph) {
        /* 效果图未初始化，静默返回，稍后会自动重试 */
        DBG("[GraphVfs] Graph not ready yet, will retry later\n");
        return GRAPH_VFS_OK;  /* 不报错，允许延迟挂载 */
    }
    
    /* 挂载为 graph0 */
    if (!EffectGraphVfs_Mount("graph0", graph)) {
        DBG("[GraphVfs] ERROR: Failed to mount default graph\n");
        return GRAPH_VFS_ERR_NO_MEMORY;
    }
    
    DBG("[GraphVfs] Default graph mounted as /audio/graph0\n");
    return GRAPH_VFS_OK;
}

/**
 * @brief 尝试自动挂载（音频系统初始化后调用）
 * @note 此函数应在音频效果图初始化后被调用
 */
GraphVfsError_t EffectGraphVfs_TryAutoMount(void)
{
    EffectGraph_t *graph;
    int count;
    
    DBG("[GraphVfs] TryAutoMount: Start\n");
    
    /* 检查VFS是否已初始化 */
    if (!g_GraphVfsInitialized) {
        DBG("[GraphVfs] TryAutoMount: VFS not initialized, initializing...\n");
        GraphVfsError_t err = EffectGraphVfs_Init();
        if (err != GRAPH_VFS_OK) {
            DBG("[GraphVfs] TryAutoMount: VFS init failed!\n");
            return err;
        }
    }
    
    /* 检查是否已挂载 */
    count = EffectGraphVfs_ListGraphs(NULL);
    DBG("[GraphVfs] TryAutoMount: Current graph count = %d\n", count);
    if (count > 0) {
        DBG("[GraphVfs] TryAutoMount: Already mounted, skip\n");
        return GRAPH_VFS_OK;  /* 已挂载，无需重复 */
    }
    
    /* 尝试挂载 */
    DBG("[GraphVfs] TryAutoMount: Getting graph instance...\n");
    graph = EffectGraph_GetInstance();
    if (!graph) {
        DBG("[GraphVfs] TryAutoMount: ERROR - Graph instance is NULL!\n");
        return GRAPH_VFS_ERR_NOT_FOUND;
    }
    
    DBG("[GraphVfs] TryAutoMount: Graph instance OK, node_count=%d\n", graph->node_count);
    DBG("[GraphVfs] TryAutoMount: Mounting as graph0...\n");
    
    if (!EffectGraphVfs_Mount("graph0", graph)) {
        DBG("[GraphVfs] TryAutoMount: ERROR - Mount failed!\n");
        return GRAPH_VFS_ERR_NO_MEMORY;
    }
    
    DBG("[GraphVfs] TryAutoMount: SUCCESS - graph0 mounted with %d nodes\n", graph->node_count);
    return GRAPH_VFS_OK;
}
