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

#include "product_def.h"  /* 需要 EFFECT_GRAPHICS_EN 宏定义 */
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
    NODE_ID_REMIND,          /* 提示音源节点 */
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
    /* ===== 节拍器、提示音和Looper节点 ===== */ \
    { NODE_ID_METRONOME,     EFFECT_NODE_TYPE_SOURCE_METRONOME,    "metronome",     true,  {{0}} }, \
    { NODE_ID_REMIND,        EFFECT_NODE_TYPE_SOURCE_REMIND,       "remind",        true,  {{0}} }, \
    { NODE_ID_LOOPER_PLAY,   EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY,  "looper_play",   true,  {{0}} }, \
    { NODE_ID_LOOPER_RECORD, EFFECT_NODE_TYPE_SINK_LOOPER_RECORD,  "looper_record", true,  {{0}} }, \
}

/* BanGTsynth合成器已移除 */

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
    /* USB/BT + 节拍器 + 提示音 输入到 USB_BT 混音器 */ \
    { NODE_ID_USB_IN,    NODE_ID_USB_BT_MIXER, 0, 0 }, \
    { NODE_ID_BT_IN,     NODE_ID_USB_BT_MIXER, 0, 1 }, \
    { NODE_ID_METRONOME, NODE_ID_USB_BT_MIXER, 0, 2 }, \
    { NODE_ID_REMIND,    NODE_ID_USB_BT_MIXER, 0, 3 }, \
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
    /* Expander输出 → Looper录制
     * 录制 Expander 处理后、未混入 Looper_Play 之前的纯吉他+麦克风信号。
     * 这与 Pre_Reverb_Mixer 收到的 ADC 链信号完全一致，
     * 避免录制时引入已有 Looper 层（防止叠录反馈循环）。
     * 原来连 ADC_Mixer 跳过了 Expander 增益，现已修正。 */ \
    { NODE_ID_EXPANDER, NODE_ID_LOOPER_RECORD, 0, 0 }, \
}

#define DEFAULT_EDGE_COUNT  23

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

/* Delay 节点最大可配置延迟 (SDK PcmDelay 的环形缓冲大小上限) */
#define FX_DELAY_MAX_MS             500

/* 合唱默认参数 */
#define DEFAULT_CHORUS_DELAY_LENGTH 13      /* 基准延迟长度 ms (1-25) */
#define DEFAULT_CHORUS_MOD_DEPTH    3       /* 调制深度 ms (< delay_length) */
#define DEFAULT_CHORUS_MOD_RATE     10      /* 调制速率 (0.1Hz 单位, 10 = 1.0Hz) */
#define DEFAULT_CHORUS_FEEDBACK     30      /* 反馈量 0-50 */
#define DEFAULT_CHORUS_DRY          90      /* 干声 0-100 */
#define DEFAULT_CHORUS_WET          60      /* 湿声 0-100 */

/* 失真默认参数 (可切换类型: SOFT=软削波/蓝调, HARD=硬限幅/摇滚, FUZZ=法兹) */
#define DEFAULT_DISTORTION_TYPE    1       /* 默认 HARD(摇滚金属) */
#define DEFAULT_DISTORTION_DRIVE   55      /* 失真强度 0-100 */
#define DEFAULT_DISTORTION_ASYM    50      /* 非对称量 0-100 (50=对称) */
#define DEFAULT_DISTORTION_LEVEL   90      /* 输出电平 0-100 */
#define DEFAULT_DISTORTION_TONE    80      /* 亮度/低通 0-100 (100=直通) */
#define DEFAULT_DISTORTION_FEEDBACK 0      /* 反馈量 0-100 (0=关闭, 高=高增益金属) */

/* 无限延音(Infinite Sustain)默认参数
 * 默认关闭(enable=0)，开启后才会抓取并循环当前音符，避免默认改变声音 */
#define DEFAULT_SUSTAIN_ENABLE       0      /* 延音引擎总开关 0/1 */
#define DEFAULT_SUSTAIN_MODE         0      /* 0=瞬态 1=慢速(Slow) */
#define DEFAULT_SUSTAIN_SENSITIVITY 70     /* 起音检测灵敏度 0-100 */
#define DEFAULT_SUSTAIN_THRESHOLD    8      /* 触发门限 0-100 */
#define DEFAULT_SUSTAIN_ATTACK_WAIT  4      /* 攻击平息等待(帧) 0-20 */
#define DEFAULT_SUSTAIN_RETRIGGER    0      /* 新音符重抓 0/1 (默认锁存) */
#define DEFAULT_SUSTAIN_LAYER        0      /* 分层叠加 0/1 */
#define DEFAULT_SUSTAIN_TREMOLO      0      /* 颤音深度 0-100 (0=关) */
#define DEFAULT_SUSTAIN_TREMOLO_RATE 10     /* 颤音速率 0.1Hz (10=1Hz) */
#define DEFAULT_SUSTAIN_FILTER       0      /* 自然衰减低通 0-100 (0=直通) */
#define DEFAULT_SUSTAIN_LEVEL        90     /* 输出电平 0-100 */
#define DEFAULT_SUSTAIN_MIX          100    /* 干湿比(湿声) 0-100 */

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
 * 直进直出配置 (备选预设, shell: graph preset 1) - 无效果器
 *   ADC0(guitar) + ADC1(mic) + USB_IN -> Mixer -> DAC0(speaker) + USB_OUT
 *   6 节点 5 边。这套预设里没有任何效果节点，USB 回放经 Mixer 与琴声相加不会
 *   被染色，所以保持原样，不需要像 FX_CHAIN 那样为它单开一个末端混音节点。
 *   它也不是开机默认拓扑 —— GRAPH_PRESET_DEFAULT 走的是下面的 FX_CHAIN。
 ******************************************************************************/
#define SIMPLE_NODE_COUNT   6

#define SIMPLE_NODES_CONFIG { \
    { 0, EFFECT_NODE_TYPE_SOURCE_ADC0,   "ADC0",    true, {{0}} }, \
    { 1, EFFECT_NODE_TYPE_SOURCE_ADC1,   "ADC1",    true, {{0}} }, \
    { 2, EFFECT_NODE_TYPE_SOURCE_USB_IN, "USB_In",  true, {{0}} }, \
    { 3, EFFECT_NODE_TYPE_MIXER,         "Mixer",   true, {{0}} }, \
    { 4, EFFECT_NODE_TYPE_SINK_DAC0,     "DAC0",    true, {{0}} }, \
    { 5, EFFECT_NODE_TYPE_SINK_USB_OUT,  "USB_Out", true, {{0}} }, \
}

#define SIMPLE_EDGES_CONFIG { \
    { 0, 3, 0, 0 }, \
    { 1, 3, 0, 1 }, \
    { 2, 3, 0, 2 }, \
    { 3, 4, 0, 0 }, \
    { 3, 5, 0, 0 }, \
}

#define SIMPLE_EDGE_COUNT   5

/*******************************************************************************
 * 综合效果器配置 (BanEffector 实际使用的默认拓扑)
 *   在直进直出链路的基础上串入失真、Delay(PCM Delay)、Chorus(合唱)与无限延音:
 *
 *   11 节点 10 边。不用框线图画拓扑，因为中日韩字符宽度会让跨行竖线对不齐:
 *     效果链  : ADC0(guitar) + ADC1(mic) -> Mixer -> Distortion(默认关)
 *               -> Delay -> Chorus -> Sustain -> OutMix
 *     USB 旁路: USB_In(主机回放) ---------------------------> OutMix
 *     输出    : OutMix -> DAC0(speaker)，OutMix -> USB_Out(内录)
 *   Chorus 算法为单声道，处理回调内部按 L/R 各跑一路实例。
 *
 *   【USB 回放不进效果链】USB_In 原先接在输入 Mixer 上，主机放的音乐会和琴声
 *   一起被 Distortion/Delay/Chorus/Sustain 加工：失真把音乐削成方波，Delay 与
 *   Chorus 的 fb=30 反馈环还会把它拖成一片糊响。现改为把 USB_In 直接接到末端
 *   OutMix，只在整条 ADC 效果链跑完之后与琴声相加：
 *     - 效果链只处理 ADC0(琴)/ADC1(麦)，USB 音乐保持原样、不被染色
 *     - OutMix 的输出同时喂 DAC0 与 USB_Out，所以 USB 上行(内录)拿到的是
 *       "琴声+效果+主机回放"的完整混音，PC 侧能录到本机正在播放的内容
 *   OutMix 复用 EFFECT_NODE_TYPE_MIXER，注册到 Mixer_Process()。该回调内部
 *   (void)node、没有任何静态状态，因此能与输入 Mixer 同时存在两份实例；又因
 *   bg_graph_setup.c 只在 input_count>=4 时才改挂 ADC_Mixer_Process，两个
 *   mixer 都是 2 输入，都会拿到通用 Mixer_Process。
 *   引擎的 sink 节点只消费 inputs[0](见 EffectGraph_Process)，所以要使两路信号
 *   同时进 DAC0/USB_Out 必须经一个混音节点，不能把两条边直接挂到 sink 上。
 *   节点处理顺序由 EffectGraph_Build() 的 Kahn 拓扑排序自动得出，与配置数组的
 *   书写顺序无关；但 EffectGraph_AddNode() 用 g->node_count 充当 node->id，故
 *   FxChainNodeId_t 的枚举值必须与 FX_CHAIN_NODES_CONFIG 的行序严格一致。
 *   注意：若用 shell 关掉 out_mix(graph node out_mix off)，引擎的禁用节点直通
 *   只复制 inputs[0](效果链)，USB 回放会随之丢失 —— 不要关这个节点。
 *
 *   【失真默认关闭】历史上曾把默认失真定性为"参数过于极端、听感是刺耳
 *   杂音"，那是误判：真正原因是 Distortion_Process() 里负半周削波阈值算成了
 *   正数(th_neg = 3072 - (asym-50)*40，asym=50 时得 +3072)，使主循环 else-if 链
 *   短路，HARD/FUZZ 的输出对任意输入都恒为 +32767 满幅直流，琴声完全被
 *   抹掉，只剩下游 Delay(fb=30)/Chorus(fb=30) 反馈环里的宽带噪声。该符号
 *   错误已在 bg_graph_effects.c 修正，同时把输出级归一化基准从固定 Q10 满幅
 *   (>>10) 改为按削波阈值 out_norm 归一化，把软膝从 "1024+(xg-1024)*3/4"（它在
 *   xg=th_pos 处只到 2560、与硬限幅值 3072 之间有 512 的跳变）换成 C1 连续的
 *   抛物线软膝，并把 SOFT 从三次方曲线换成单调有界的有理软削波。
 *
 *   【第二轮修正：预放大偏低约 20dB】上述改动消除了噪声，但实机复测仍是"开了
 *   失真跟没开一样"。根因是一条与 th_pos/level 都无关的标定关系：在归一化域里
 *       软膝起点对应的输入幅度 = 32768 / 预放大倍数
 *       硬削波点对应的输入幅度 = 32*(2*th_pos-1024) / 预放大倍数
 *   即削波门槛只由预放大倍数决定。吉他经 ADC0 的 PGA(CFG_LINE1_*_GAIN=18 ->
 *   AudioADC_PGAGainSet 索引 31-18=13，近似 unity)后峰值通常只有 2%~12% 满幅，
 *   而当时 HARD 的 3x..24x 把软膝起点放在 6.8% 满幅、硬削波点放在 34% 满幅 ——
 *   实测 drive=55 时 2%~12%FS 输入的 THD 只有 2.7%~3.5%，输出就是一条 +12.8dB
 *   的直线放大，听感与不开失真无异。真实失真踏板的前置增益在 40dB 上下
 *   (DS-1≈40dB、RAT≈45dB、Metal Zone≈60dB)，故预放大范围整体上移：
 *       HARD 4x..224x(12..47dB)、FUZZ 8x..388x(18..52dB)、SOFT 2x..32x(6..30dB)
 *   修正后默认参数(drive=55/asym=50/level=90)的实测传输特性：
 *       HARD  预放大 125x(+41.9dB)，软膝起点 0.80%FS、硬削波点 4.00%FS，
 *             THD 5%FS->20.1% / 8%->30.2% / 12%->36.0% / 20%->40.7%，削波输出 90%FS
 *       FUZZ  预放大 217x(+46.7dB)，软膝起点 0.46%FS、硬削波点 3.23%FS，
 *             THD 5%FS->25.5% / 12%->38.3% / 20%->42.2%，削波输出 90%FS
 *       SOFT  预放大 18.5x(+25.3dB)，THD 5%FS->10.5% / 20%->22.6% / 满幅 85.3%FS，
 *             有理软削波渐近无硬限幅，仍是蓝调过载音色
 *       drive 单调平滑：HARD 在 5%FS 输入下 drive=0->THD 0.0%(峰值 1966，≈unity
 *       清音增强)、20->3.4%、40->11.9%、55->20.1%、100->32.0%，无跳变。
 *
 *   【配套两项】① 40dB 量级增益会把 ADC 底噪同比抬起来，故 Distortion_Process
 *   内置了与增益联动的输入包络噪声门(DIST_GATE_TH/ATK/REL，门限按小信号线性
 *   增益 lg10 在 2x..8x 之间线性淡入，drive 很低时 gth=0 完全透明)；实测静音段
 *   底噪 SOFT/HARD/FUZZ 分别为 -82.0/-63.6/-60.7 dBFS(输入 ±30LSB 白噪)。
 *   ② 预放大乘法原写作 ((x>>5)*g)>>10，它把 x 的低 5 位直接丢掉，而 >>5 是
 *   floor：[-32,-1] 全部映射到 -1 而 [0,31] 映射到 0，正负不对称。在 125x 增益下
 *   这 1 个 LSB 台阶被放大成 ≈3.7%FS 的输出跳变，静音段被整流成负直流，再经
 *   tone 一阶低通积分成 -35dBFS 的低频轰鸣(噪声门也压不住)。已换成 DIST_PREGAIN
 *   宏做 x = 32*(x>>5) + (x&31) 的精确拆分，数学上严格等于 (x*g)>>15，修复后
 *   底噪下降 28~39dB。全参数空间扫描(720 组合静态传输 + 572832 样本带状态实跑)
 *   int32 溢出 0、输出越界 0、正半周非单调 0。
 *
 *   音色仍嫌不够脏时优先动 drive(55->80)，其次换 type(HARD->FUZZ)；tone 实测对
 *   4.4kHz 以上 fizz 的影响很小(fizz/music 比值约 0.006)，不必靠压 tone 去噪。
 *   仍保持开机默认关闭，理由回到与 Sustain 的 DEFAULT_SUSTAIN_ENABLE=0 一致的
 *   产品约定("避免默认改变声音")——直进直出应是干净的干声。需要时打开:
 *       graph node distortion on
 *       graph set distortion type <0=SOFT|1=HARD|2=FUZZ>
 ******************************************************************************/
typedef enum {
    FX_NODE_ADC0 = 0,      /* 乐器/线路输入 */
    FX_NODE_ADC1,          /* 麦克风输入 */
    FX_NODE_USB_IN,        /* USB 音频输入(主机回放) - 不过效果链 */
    FX_NODE_MIXER,         /* 输入混音(仅 ADC0/ADC1) */
    FX_NODE_DISTORTION,    /* 失真 */
    FX_NODE_DELAY,         /* 延迟 */
    FX_NODE_CHORUS,        /* 合唱 */
    FX_NODE_SUSTAIN,       /* 无限延音 */
    FX_NODE_OUT_MIX,       /* 末端混音(效果链输出 + USB 回放) */
    FX_NODE_DAC0,          /* 扬声器输出 */
    FX_NODE_USB_OUT,       /* USB 录音输出(内录) */

    FX_CHAIN_NODE_COUNT
} FxChainNodeId_t;

#define FX_CHAIN_NODES_CONFIG { \
    { FX_NODE_ADC0,    EFFECT_NODE_TYPE_SOURCE_ADC0,   "adc0",    true, {{0}} }, \
    { FX_NODE_ADC1,    EFFECT_NODE_TYPE_SOURCE_ADC1,   "adc1",    true, {{0}} }, \
    { FX_NODE_USB_IN,  EFFECT_NODE_TYPE_SOURCE_USB_IN, "usb_in",  true, {{0}} }, \
    { FX_NODE_MIXER,   EFFECT_NODE_TYPE_MIXER,         "mixer",   true, {{0}} }, \
    { FX_NODE_DISTORTION, EFFECT_NODE_TYPE_EFFECT_DISTORTION, "distortion", false, {{0}} }, \
    { FX_NODE_DELAY,   EFFECT_NODE_TYPE_EFFECT_DELAY,  "delay",   true, {{0}} }, \
    { FX_NODE_CHORUS,  EFFECT_NODE_TYPE_EFFECT_CHORUS, "chorus",  true, {{0}} }, \
    { FX_NODE_SUSTAIN, EFFECT_NODE_TYPE_EFFECT_SUSTAIN, "sustain", true, {{0}} }, \
    { FX_NODE_OUT_MIX, EFFECT_NODE_TYPE_MIXER,         "out_mix", true, {{0}} }, \
    { FX_NODE_DAC0,    EFFECT_NODE_TYPE_SINK_DAC0,     "dac0",    true, {{0}} }, \
    { FX_NODE_USB_OUT, EFFECT_NODE_TYPE_SINK_USB_OUT,  "usb_out", true, {{0}} }, \
}

#define FX_CHAIN_EDGES_CONFIG { \
    { FX_NODE_ADC0,   FX_NODE_MIXER, 0, 0 }, \
    { FX_NODE_ADC1,   FX_NODE_MIXER, 0, 1 }, \
    { FX_NODE_MIXER,  FX_NODE_DISTORTION, 0, 0 }, \
    { FX_NODE_DISTORTION, FX_NODE_DELAY, 0, 0 }, \
    { FX_NODE_DELAY,  FX_NODE_CHORUS, 0, 0 }, \
    { FX_NODE_CHORUS, FX_NODE_SUSTAIN, 0, 0 }, \
    /* USB 回放绕过整条效果链，只在末端与效果链输出混音 */ \
    { FX_NODE_SUSTAIN, FX_NODE_OUT_MIX, 0, 0 }, \
    { FX_NODE_USB_IN,  FX_NODE_OUT_MIX, 0, 1 }, \
    { FX_NODE_OUT_MIX, FX_NODE_DAC0,    0, 0 }, \
    { FX_NODE_OUT_MIX, FX_NODE_USB_OUT, 0, 0 }, \
}

#define FX_CHAIN_EDGE_COUNT   10

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

#if EFFECT_GRAPHICS_EN

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

#else /* !EFFECT_GRAPHICS_EN */

/* Stub functions when effect graph is disabled */
static inline GraphError_t EffectGraphConfig_GetPreset(GraphPreset_t preset, GraphConfig_t *config) 
    { (void)preset; (void)config; return GRAPH_OK; }
static inline GraphError_t EffectGraphConfig_LoadPreset(GraphPreset_t preset) 
    { (void)preset; return GRAPH_OK; }
static inline GraphPreset_t EffectGraphConfig_GetCurrentPreset(void) 
    { return GRAPH_PRESET_DEFAULT; }
static inline void EffectGraphConfig_PrintPresets(void) { }

#endif /* EFFECT_GRAPHICS_EN */

#ifdef __cplusplus
}
#endif

#endif /* __EFFECT_GRAPH_CONFIG_H__ */
