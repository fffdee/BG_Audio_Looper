/**
 *****************************************************************************
 * @file     effect_graph_config.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    音频效果器图配置定义 - 默认图参数和配置结构
 * 
 * 说明:
 *   修改此文件可以改变默认音频处理图的结构
 *   无需修改程序代码，只需修改配置参数即可重构音频链路
 *****************************************************************************
 */

#ifndef __EFFECT_GRAPH_CONFIG_H__
#define __EFFECT_GRAPH_CONFIG_H__

#include "effect_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * 默认采样率
 ******************************************************************************/
#define DEFAULT_SAMPLE_RATE     48000

/*******************************************************************************
 * 节点ID定义 - 方便配置边时引用
 ******************************************************************************/
typedef enum {
    /* 输入源节点 ID: 0-3 */
    NODE_ID_ADC0_GUITAR = 0,
    NODE_ID_ADC1_MIC,
    NODE_ID_USB_IN,
    NODE_ID_BT_IN,
    
    /* 混音器节点 ID: 4 */
    NODE_ID_MAIN_MIXER,
    
    /* 效果器节点 ID: 5-8 */
    NODE_ID_EXPANDER,
    NODE_ID_DRC,
    NODE_ID_EQ,
    NODE_ID_REVERB,
    
    /* 输出节点 ID: 9-10 */
    NODE_ID_DAC0_OUT,
    NODE_ID_USB_OUT,
    
    /* 节点总数 */
    DEFAULT_NODE_COUNT
} DefaultNodeId_t;

/*******************************************************************************
 * 默认节点配置表
 * 格式: { node_id, type, name, enabled, params }
 ******************************************************************************/
#define DEFAULT_NODES_CONFIG { \
    /* ===== 输入源节点 ===== */ \
    { NODE_ID_ADC0_GUITAR, NODE_TYPE_SOURCE_ADC0,   "ADC0_Guitar", true,  {0} }, \
    { NODE_ID_ADC1_MIC,    NODE_TYPE_SOURCE_ADC1,   "ADC1_Mic",    true,  {0} }, \
    { NODE_ID_USB_IN,      NODE_TYPE_SOURCE_USB_IN, "USB_In",      true,  {0} }, \
    { NODE_ID_BT_IN,       NODE_TYPE_SOURCE_BT_IN,  "BT_In",       true,  {0} }, \
    \
    /* ===== 混音器节点 ===== */ \
    { NODE_ID_MAIN_MIXER,  NODE_TYPE_MIXER,         "MainMixer",   true,  {0} }, \
    \
    /* ===== 效果器节点 ===== */ \
    { NODE_ID_EXPANDER,    NODE_TYPE_EFFECT_EXPANDER, "Expander",  true,  {0} }, \
    { NODE_ID_DRC,         NODE_TYPE_EFFECT_DRC,      "DRC",       true,  {0} }, \
    { NODE_ID_EQ,          NODE_TYPE_EFFECT_EQ,       "EQ",        true,  {0} }, \
    { NODE_ID_REVERB,      NODE_TYPE_EFFECT_REVERB,   "Reverb",    true,  {0} }, \
    \
    /* ===== 输出节点 ===== */ \
    { NODE_ID_DAC0_OUT,    NODE_TYPE_SINK_DAC0,    "DAC0_Out",    true,  {0} }, \
    { NODE_ID_USB_OUT,     NODE_TYPE_SINK_USB_OUT, "USB_Out",     true,  {0} }, \
}

/*******************************************************************************
 * 默认边(连接)配置表
 * 格式: { src_node_id, dst_node_id, src_port, dst_port }
 * 
 * 音频流图:
 *   ADC0 (Guitar) ──┐
 *   ADC1 (Mic)    ──┼──> Mixer -> Expander -> DRC -> EQ -> Reverb ──┬──> DAC0
 *   USB_In        ──┤                                               │
 *   BT_In         ──┘                                               └──> USB_Out
 ******************************************************************************/
#define DEFAULT_EDGES_CONFIG { \
    /* 输入到混音器 */ \
    { NODE_ID_ADC0_GUITAR, NODE_ID_MAIN_MIXER, 0, 0 }, \
    { NODE_ID_ADC1_MIC,    NODE_ID_MAIN_MIXER, 0, 1 }, \
    { NODE_ID_USB_IN,      NODE_ID_MAIN_MIXER, 0, 2 }, \
    { NODE_ID_BT_IN,       NODE_ID_MAIN_MIXER, 0, 3 }, \
    \
    /* 效果链 */ \
    { NODE_ID_MAIN_MIXER, NODE_ID_EXPANDER, 0, 0 }, \
    { NODE_ID_EXPANDER,   NODE_ID_DRC,      0, 0 }, \
    { NODE_ID_DRC,        NODE_ID_EQ,       0, 0 }, \
    { NODE_ID_EQ,         NODE_ID_REVERB,   0, 0 }, \
    \
    /* 输出 */ \
    { NODE_ID_REVERB, NODE_ID_DAC0_OUT, 0, 0 }, \
    { NODE_ID_REVERB, NODE_ID_USB_OUT,  0, 0 }, \
}

#define DEFAULT_EDGE_COUNT  10

/*******************************************************************************
 * 效果器默认参数配置
 ******************************************************************************/

/* 混响默认参数 */
#define DEFAULT_REVERB_ROOM_SIZE    50      /* 房间大小 0-100 */
#define DEFAULT_REVERB_DAMPING      50      /* 阻尼 0-100 */
#define DEFAULT_REVERB_WET_DRY      30      /* 干湿比 0-100 */

/* DRC默认参数 */
#define DEFAULT_DRC_THRESHOLD       (-20)   /* 阈值 dB */
#define DEFAULT_DRC_RATIO           4       /* 压缩比 */
#define DEFAULT_DRC_ATTACK          10      /* 启动时间 ms */
#define DEFAULT_DRC_RELEASE         100     /* 释放时间 ms */

/* EQ默认参数 */
#define DEFAULT_EQ_BAND_COUNT       5       /* 频段数 */
#define DEFAULT_EQ_BAND_GAINS       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

/* 扩展器默认参数 */
#define DEFAULT_EXPANDER_THRESHOLD  (-40)   /* 阈值 dB */
#define DEFAULT_EXPANDER_RATIO      2       /* 扩展比 */

/* 增益默认参数 */
#define DEFAULT_GAIN_DB             0       /* 增益 dB */

/* 延迟默认参数 */
#define DEFAULT_DELAY_MS            250     /* 延迟时间 ms */
#define DEFAULT_DELAY_FEEDBACK      30      /* 反馈量 0-100 */
#define DEFAULT_DELAY_WET_DRY       30      /* 干湿比 0-100 */

/*******************************************************************************
 * 预设配置 - 可以定义多套配置方便切换
 ******************************************************************************/

/* 配置ID枚举 */
typedef enum {
    GRAPH_PRESET_DEFAULT = 0,       /* 默认完整配置 */
    GRAPH_PRESET_SIMPLE,            /* 简单配置(无效果) */
    GRAPH_PRESET_GUITAR_ONLY,       /* 仅吉他 */
    GRAPH_PRESET_MIC_ONLY,          /* 仅麦克风 */
    GRAPH_PRESET_BLUETOOTH,         /* 蓝牙音箱模式 */
    GRAPH_PRESET_USB_AUDIO,         /* USB声卡模式 */
    GRAPH_PRESET_MAX
} GraphPreset_t;

/*******************************************************************************
 * 简单配置 - 无效果直通
 * ADC0+ADC1 -> Mixer -> DAC0
 ******************************************************************************/
#define SIMPLE_NODE_COUNT   5

#define SIMPLE_NODES_CONFIG { \
    { 0, NODE_TYPE_SOURCE_ADC0,   "ADC0",    true, {0} }, \
    { 1, NODE_TYPE_SOURCE_ADC1,   "ADC1",    true, {0} }, \
    { 2, NODE_TYPE_MIXER,         "Mixer",   true, {0} }, \
    { 3, NODE_TYPE_SINK_DAC0,     "DAC0",    true, {0} }, \
    { 4, NODE_TYPE_SINK_USB_OUT,  "USB_Out", true, {0} }, \
}

#define SIMPLE_EDGES_CONFIG { \
    { 0, 2, 0, 0 }, \
    { 1, 2, 0, 1 }, \
    { 2, 3, 0, 0 }, \
    { 2, 4, 0, 0 }, \
}

#define SIMPLE_EDGE_COUNT   4

/*******************************************************************************
 * 蓝牙音箱配置
 * BT_In -> EQ -> DAC0
 ******************************************************************************/
#define BT_SPEAKER_NODE_COUNT   3

#define BT_SPEAKER_NODES_CONFIG { \
    { 0, NODE_TYPE_SOURCE_BT_IN,  "BT_In",   true, {0} }, \
    { 1, NODE_TYPE_EFFECT_EQ,     "EQ",      true, {0} }, \
    { 2, NODE_TYPE_SINK_DAC0,     "DAC0",    true, {0} }, \
}

#define BT_SPEAKER_EDGES_CONFIG { \
    { 0, 1, 0, 0 }, \
    { 1, 2, 0, 0 }, \
}

#define BT_SPEAKER_EDGE_COUNT   2

/*******************************************************************************
 * API函数
 ******************************************************************************/

/**
 * @brief 获取预设配置
 * @param preset 预设ID
 * @param config 输出配置结构指针
 * @return 错误码
 */
GraphError_t EffectGraphConfig_GetPreset(GraphPreset_t preset, GraphConfig_t *config);

/**
 * @brief 从预设创建图
 * @param preset 预设ID
 * @return 错误码
 */
GraphError_t EffectGraphConfig_LoadPreset(GraphPreset_t preset);

/**
 * @brief 获取当前预设ID
 * @return 当前预设ID
 */
GraphPreset_t EffectGraphConfig_GetCurrentPreset(void);

/**
 * @brief 打印所有可用预设
 */
void EffectGraphConfig_PrintPresets(void);

#ifdef __cplusplus
}
#endif

#endif /* __EFFECT_GRAPH_CONFIG_H__ */
