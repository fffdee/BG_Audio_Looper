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
 * 
 * 新架构说明 (2026-02-04):
 *   ADC0/ADC1 各为单声道输入，每个声道独立配置EQ
 *   - ADC0_L/R: 乐器左右声道 → EQ_GUITAR_L/R
 *   - ADC1_L/R: 麦克风左右声道 → EQ_MIC_L/R
 *   4个EQ处理后混音，再进入效果链（Expander→DRC→Reverb）
 ******************************************************************************/
typedef enum {
    /* 输入源节点 ID: 0-3 */
    NODE_ID_ADC0_GUITAR = 0,    /* 乐器输入（双声道，拆分为L/R处理） */
    NODE_ID_ADC1_MIC,            /* 麦克风输入（双声道，拆分为L/R处理） */
    NODE_ID_USB_IN,
    NODE_ID_BT_IN,
    
    /* 4个独立EQ节点 ID: 4-7 (每个声道独立EQ) */
    NODE_ID_EQ_GUITAR_L,         /* 乐器左声道EQ */
    NODE_ID_EQ_GUITAR_R,         /* 乐器右声道EQ */
    NODE_ID_EQ_MIC_L,            /* 麦克风左声道EQ */
    NODE_ID_EQ_MIC_R,            /* 麦克风右声道EQ */
    
    /* ADC EQ后混音器节点 ID: 8 */
    NODE_ID_ADC_MIXER,
    
    /* ADC 效果器链节点 ID: 9-11 (去掉了原来的EQ，由上游4个EQ替代) */
    NODE_ID_EXPANDER,
    NODE_ID_DRC,
    NODE_ID_PRE_REVERB_MIXER,   /* 混响前混音器（ADC链 + Looper播放） */
    NODE_ID_REVERB,
    
    /* USB/BT 混音器节点 ID: 13 */
    NODE_ID_USB_BT_MIXER,
    
    /* USB/BT EQ节点 ID: 14 */
    NODE_ID_USB_BT_EQ,
    
    /* 最终混音器节点 ID: 15 */
    NODE_ID_FINAL_MIXER,
    
    /* 输出节点 ID: 16-17 */
    NODE_ID_DAC0_OUT,
    NODE_ID_USB_OUT,
    
    /* 新增节点 ID: 18-20 */
    NODE_ID_METRONOME,       /* 节拍器源节点 */
    NODE_ID_LOOPER_PLAY,     /* Looper播放源节点 */
    NODE_ID_LOOPER_RECORD,   /* Looper录制输出节点 */
    
    /* 节点总数 */
    DEFAULT_NODE_COUNT
} DefaultNodeId_t;

/*******************************************************************************
 * 默认节点配置表
 * 格式: { node_id, type, name, enabled, params }
 * 
 * 新架构 (2026-02-04):
 *   ADC0/ADC1 双声道各拆分为L/R，每个声道有独立EQ
 *   共4个EQ：EQ_GUITAR_L, EQ_GUITAR_R, EQ_MIC_L, EQ_MIC_R
 ******************************************************************************/
#define DEFAULT_NODES_CONFIG { \
    /* ===== 输入源节点 ===== */ \
    { NODE_ID_ADC0_GUITAR, EFFECT_NODE_TYPE_SOURCE_ADC0,   "guitar_in", true,  {{0}} }, \
    { NODE_ID_ADC1_MIC,    EFFECT_NODE_TYPE_SOURCE_ADC1,   "mic_in",    true,  {{0}} }, \
    { NODE_ID_USB_IN,      EFFECT_NODE_TYPE_SOURCE_USB_IN, "usb_in",    true,  {{0}} }, \
    { NODE_ID_BT_IN,       EFFECT_NODE_TYPE_SOURCE_BT_IN,  "bt_in",     true,  {{0}} }, \
    \
    /* ===== 4个独立EQ节点 (每个声道独立处理) ===== */ \
    { NODE_ID_EQ_GUITAR_L, EFFECT_NODE_TYPE_EFFECT_EQ,     "eq_guitar_l", true, {{0}} }, \
    { NODE_ID_EQ_GUITAR_R, EFFECT_NODE_TYPE_EFFECT_EQ,     "eq_guitar_r", true, {{0}} }, \
    { NODE_ID_EQ_MIC_L,    EFFECT_NODE_TYPE_EFFECT_EQ,     "eq_mic_l",    true, {{0}} }, \
    { NODE_ID_EQ_MIC_R,    EFFECT_NODE_TYPE_EFFECT_EQ,     "eq_mic_r",    true, {{0}} }, \
    \
    /* ===== ADC EQ后混音器节点 ===== */ \
    { NODE_ID_ADC_MIXER,   EFFECT_NODE_TYPE_MIXER,         "adc_mixer", true,  {{0}} }, \
    \
    /* ===== ADC 效果器链节点 (无独立EQ，由上游4个EQ替代) ===== */ \
    { NODE_ID_EXPANDER,    EFFECT_NODE_TYPE_EFFECT_EXPANDER, "expander",  true,  {{0}} }, \
    { NODE_ID_DRC,         EFFECT_NODE_TYPE_EFFECT_DRC,      "drc",       true,  {{0}} }, \
    { NODE_ID_PRE_REVERB_MIXER, EFFECT_NODE_TYPE_MIXER,      "pre_reverb_mixer", true, {{0}} }, \
    { NODE_ID_REVERB,      EFFECT_NODE_TYPE_EFFECT_REVERB,   "reverb",    true,  {{0}} }, \
    \
    /* ===== USB/BT 混音器 ===== */ \
    { NODE_ID_USB_BT_MIXER, EFFECT_NODE_TYPE_MIXER,         "usb_bt_mixer", true, {{0}} }, \
    \
    /* ===== USB/BT EQ ===== */ \
    { NODE_ID_USB_BT_EQ,   EFFECT_NODE_TYPE_EFFECT_EQ,      "usb_bt_eq", true,  {{0}} }, \
    \
    /* ===== 最终混音器 ===== */ \
    { NODE_ID_FINAL_MIXER, EFFECT_NODE_TYPE_MIXER,          "final_mixer", true, {{0}} }, \
    \
    /* ===== 输出节点 ===== */ \
    { NODE_ID_DAC0_OUT,    EFFECT_NODE_TYPE_SINK_DAC0,      "dac_out",   true,  {{0}} }, \
    { NODE_ID_USB_OUT,     EFFECT_NODE_TYPE_SINK_USB_OUT,   "usb_out",   true,  {{0}} }, \
    \
    /* ===== 节拍器和Looper节点 ===== */ \
    { NODE_ID_METRONOME,     EFFECT_NODE_TYPE_SOURCE_METRONOME,    "metronome",     true,  {{0}} }, \
    { NODE_ID_LOOPER_PLAY,   EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY,  "looper_play",   true,  {{0}} }, \
    { NODE_ID_LOOPER_RECORD, EFFECT_NODE_TYPE_SINK_LOOPER_RECORD,  "looper_record", true,  {{0}} }, \
}

/*******************************************************************************
 * 默认边(连接)配置表
 * 格式: { src_node_id, dst_node_id, src_port, dst_port }
 * 
 * 新架构音频流图 (2026-02-04):
 *   【ADC 独立EQ处理后混音】
 *   ADC0 (Guitar) ─┬─[L声道]─> EQ_GUITAR_L ──┐
 *                  └─[R声道]─> EQ_GUITAR_R ──┤
 *                                             ├──> ADC_Mixer ──> Expander ──> DRC ──┐
 *   ADC1 (Mic)    ─┬─[L声道]─> EQ_MIC_L    ──┤                                      │
 *                  └─[R声道]─> EQ_MIC_R    ──┘                                      │
 *                                                                                    │
 *                  Looper_Play ──────────────────────────────────────────────────────┤
 *                                                                                    │
 *                                        Pre_Reverb_Mixer -> Reverb ──┐              │
 *                                                                      │              │
 *   【USB/BT + 节拍器路径】                                           │              │
 *   USB_In    ──┐                                                     │              │
 *   BT_In     ──┼──> USB_BT_Mixer -> USB_BT_EQ ───────────────────────┤              │
 *   Metronome ──┘                                                     │              │
 *                                                                      │              │
 *   【最终混音输出】                                                   │              │
 *   Reverb ──────┐                                                    │              │
 *   USB_BT_EQ ───┴──> Final_Mixer ──┬──> DAC0_Out                     │              │
 *                                    └──> USB_Out                      │              │
 *
 ******************************************************************************/
#define DEFAULT_EDGES_CONFIG { \
    /* ADC0 (Guitar) 左右声道分别进入独立EQ */ \
    { NODE_ID_ADC0_GUITAR, NODE_ID_EQ_GUITAR_L, 0, 0 }, /* ADC0 L -> EQ_GUITAR_L */ \
    { NODE_ID_ADC0_GUITAR, NODE_ID_EQ_GUITAR_R, 1, 0 }, /* ADC0 R -> EQ_GUITAR_R */ \
    \
    /* ADC1 (Mic) 左右声道分别进入独立EQ */ \
    { NODE_ID_ADC1_MIC, NODE_ID_EQ_MIC_L, 0, 0 },       /* ADC1 L -> EQ_MIC_L */ \
    { NODE_ID_ADC1_MIC, NODE_ID_EQ_MIC_R, 1, 0 },       /* ADC1 R -> EQ_MIC_R */ \
    \
    /* 4个EQ输出到ADC混音器 */ \
    { NODE_ID_EQ_GUITAR_L, NODE_ID_ADC_MIXER, 0, 0 },   /* EQ_GUITAR_L -> ADC_Mixer:0 */ \
    { NODE_ID_EQ_GUITAR_R, NODE_ID_ADC_MIXER, 0, 1 },   /* EQ_GUITAR_R -> ADC_Mixer:1 */ \
    { NODE_ID_EQ_MIC_L,    NODE_ID_ADC_MIXER, 0, 2 },   /* EQ_MIC_L -> ADC_Mixer:2 */ \
    { NODE_ID_EQ_MIC_R,    NODE_ID_ADC_MIXER, 0, 3 },   /* EQ_MIC_R -> ADC_Mixer:3 */ \
    \
    /* ADC 效果链 (混音后进入Expander->DRC->Pre_Reverb_Mixer) */ \
    { NODE_ID_ADC_MIXER, NODE_ID_EXPANDER, 0, 0 }, \
    { NODE_ID_EXPANDER,  NODE_ID_PRE_REVERB_MIXER,      0, 0 }, \
    \
    { NODE_ID_LOOPER_PLAY, NODE_ID_PRE_REVERB_MIXER, 0, 1 }, \
    \
    /* Pre_Reverb_Mixer → Reverb */ \
    { NODE_ID_PRE_REVERB_MIXER, NODE_ID_REVERB, 0, 0 }, \
    /* USB/BT + 节拍器 输入到 USB_BT 混音器 */ \
    { NODE_ID_USB_IN,    NODE_ID_USB_BT_MIXER, 0, 0 }, \
    { NODE_ID_BT_IN,     NODE_ID_USB_BT_MIXER, 0, 1 }, \
    { NODE_ID_METRONOME, NODE_ID_USB_BT_MIXER, 0, 2 }, \
    \
    /* USB/BT 混音器 → EQ处理 */ \
    { NODE_ID_USB_BT_MIXER, NODE_ID_USB_BT_EQ, 0, 0 }, \
    \
    /* 最终混音器 (Reverb + USB_BT_EQ) */ \
    { NODE_ID_REVERB,    NODE_ID_FINAL_MIXER, 0, 0 }, \
    { NODE_ID_USB_BT_EQ, NODE_ID_FINAL_MIXER, 0, 1 }, \
    \
    /* 输出 */ \
    { NODE_ID_FINAL_MIXER, NODE_ID_DRC, 0, 0 }, \
    { NODE_ID_DRC, NODE_ID_DAC0_OUT, 0, 0 }, \
    { NODE_ID_DRC, NODE_ID_USB_OUT,  0, 0 }, \
    \
    /* ADC混音器 → Looper录制 */ \
    { NODE_ID_ADC_MIXER, NODE_ID_LOOPER_RECORD, 0, 0 }, \
}

#define DEFAULT_EDGE_COUNT  22

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
#define DEFAULT_DRC_RELEASE         200     /* 释放时间 ms */

/* EQ默认参数 */
#define DEFAULT_EQ_BAND_COUNT       5       /* 频段数 */
#define DEFAULT_EQ_BAND_GAINS       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

/* 扩展器默认参数 */
#define DEFAULT_EXPANDER_THRESHOLD  (-20)   /* 阈值 dB */
#define DEFAULT_EXPANDER_RATIO      1      /* 扩展比 */

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
    GRAPH_PRESET_SECONDARY_SPEAKER, /* 副音箱模式 - 仅混音，无效果和looper */
    GRAPH_PRESET_MAX
} GraphPreset_t;

/*******************************************************************************
 * 简单配置 - 无效果直通
 * ADC0+ADC1 -> Mixer -> DAC0
 ******************************************************************************/
#define SIMPLE_NODE_COUNT   5

#define SIMPLE_NODES_CONFIG { \
    { 0, EFFECT_NODE_TYPE_SOURCE_ADC0,   "ADC0",    true, {{0}} }, \
    { 1, EFFECT_NODE_TYPE_SOURCE_ADC1,   "ADC1",    true, {{0}} }, \
    { 2, EFFECT_NODE_TYPE_MIXER,         "Mixer",   true, {{0}} }, \
    { 3, EFFECT_NODE_TYPE_SINK_DAC0,     "DAC0",    true, {{0}} }, \
    { 4, EFFECT_NODE_TYPE_SINK_USB_OUT,  "USB_Out", true, {{0}} }, \
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
    { 0, EFFECT_NODE_TYPE_SOURCE_BT_IN,  "BT_In",   true, {{0}} }, \
    { 1, EFFECT_NODE_TYPE_EFFECT_EQ,     "EQ",      true, {{0}} }, \
    { 2, EFFECT_NODE_TYPE_SINK_DAC0,     "DAC0",    true, {{0}} }, \
}

#define BT_SPEAKER_EDGES_CONFIG { \
    { 0, 1, 0, 0 }, \
    { 1, 2, 0, 0 }, \
}

#define BT_SPEAKER_EDGE_COUNT   2

/*******************************************************************************
 * 副音箱配置 - 仅混音所有输入源到输出，无效果和Looper
 * 使用三个混音器实现完整的信号路径，避免端口冲突
 * [ADC0, ADC1] -> ADC_Mixer -> Final_Mixer -> DAC0
 * [USB_IN, BT_IN] -> USB_BT_Mixer -> Final_Mixer -> DAC0
 * 特点：
 *   - 不使用任何效果器
 *   - 不使用Looper功能
 *   - 不使用Metronome节拍器
 *   - 仅简单混音多路输入到输出
 *   - 适用于副音箱场景
 ******************************************************************************/
#define SECONDARY_SPEAKER_NODE_COUNT   8

#define SECONDARY_SPEAKER_NODES_CONFIG { \
    { 0, EFFECT_NODE_TYPE_SOURCE_ADC0,   "guitar_in",   true, {{0}} }, \
    { 1, EFFECT_NODE_TYPE_SOURCE_ADC1,   "mic_in",   true, {{0}} }, \
    { 2, EFFECT_NODE_TYPE_SOURCE_USB_IN, "usb_in",    true, {{0}} }, \
    { 3, EFFECT_NODE_TYPE_SOURCE_BT_IN,  "bt_in",     true, {{0}} }, \
    { 4, EFFECT_NODE_TYPE_MIXER,         "adc_mixer", true, {{0}} }, \
    { 5, EFFECT_NODE_TYPE_MIXER,         "usb_bt_mixer", true, {{0}} }, \
    { 6, EFFECT_NODE_TYPE_MIXER,         "final_mixer", true, {{0}} }, \
    { 7, EFFECT_NODE_TYPE_SINK_DAC0,     "dac_out",   true, {{0}} }, \
}

#define SECONDARY_SPEAKER_EDGES_CONFIG { \
    /* ADC输入到ADC混音器 - ADC是单声道 */ \
    { 0, 4, 0, 0 }, /* ADC0 (单声道) -> ADC_Mixer:0 */ \
    { 1, 4, 0, 1 }, /* ADC1 (单声道) -> ADC_Mixer:1 */ \
    \
    /* USB/BT输入到USB_BT混音器 - USB/BT是立体声 */ \
    { 2, 5, 0, 0 }, /* USB_IN (立体声) -> USB_BT_Mixer:0 */ \
    { 3, 5, 0, 1 }, /* BT_IN (立体声) -> USB_BT_Mixer:1 */ \
    \
    /* 两个混音器输出到最终混音器 */ \
    { 4, 6, 0, 0 }, /* ADC_Mixer -> Final_Mixer:0 */ \
    { 5, 6, 0, 1 }, /* USB_BT_Mixer -> Final_Mixer:1 */ \
    \
    /* 最终混音器输出到DAC0 */ \
    { 6, 7, 0, 0 }, /* Final_Mixer -> DAC0 */ \
}

#define SECONDARY_SPEAKER_EDGE_COUNT   7

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
