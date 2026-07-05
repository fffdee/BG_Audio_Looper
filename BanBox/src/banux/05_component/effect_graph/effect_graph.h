/**
 *****************************************************************************
 * @file     effect_graph.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    音频效果器图 - 基于图数据结构的音频处理链路
 *
 * 功能说明:
 *   - 输入源节点: ADC0, ADC1, USB_IN, BT_IN
 *   - 输出源节点: DAC0, USB_OUT
 *   - 处理节点: 混音器(Mixer), 各种效果器(Reverb, DRC, EQ等)
 *   - 支持灵活的音频路由和效果链配置
 *
 * 使用方法:
 *   1. EffectGraph_Init() 初始化图系统
 *   2. EffectGraph_CreateFromConfig() 从配置创建图
 *   3. EffectGraph_Process() 处理音频数据
 *****************************************************************************
 */

#ifndef __EFFECT_GRAPH_H__
#define __EFFECT_GRAPH_H__

#include <stdint.h>
#include <stdbool.h>
#include "product_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * 图构建模式配置 (2026-02-05 新增)
 * 
 * USE_STATIC_EFFECT_GRAPH=1: 静态图模式（默认）
 *   - 节点/边/处理顺序在编译时确定，使用 const 数组定义
 *   - 不支持运行时增删节点/边
 *   - 内存占用最小，适合当前产品（21节点+22边固定配置）
 *   - 节省约 2KB+ RAM（节点池、边池、处理顺序数组全部改为 const）
 * 
 * USE_STATIC_EFFECT_GRAPH=0: 动态图模式
 *   - 支持运行时动态增删节点/边
 *   - 需要更多 RAM 存储可变数据
 *   - 适用于需要灵活配置的场景
 ******************************************************************************/
#ifndef USE_STATIC_EFFECT_GRAPH
#define USE_STATIC_EFFECT_GRAPH  1  /* 默认启用静态图模式，节省内存 */
#endif

/*******************************************************************************
 * 配置定义
 * 
 * 2026-02-05 内存优化更新:
 *   - EFFECT_GRAPH_MAX_NODES: 21 (精确匹配当前使用节点数)
 *   - EFFECT_GRAPH_MAX_EDGES: 24 (精确匹配当前使用22条边 + 2条预留)
 *   - EFFECT_GRAPH_BUFFER_SIZE: 200 → 节省大量RAM (22*200*4=17600 bytes)
 *   - 如需更大帧长可改为 512 (SBC最大帧595，但实际处理帧256)
 ******************************************************************************/
#define EFFECT_GRAPH_MAX_NODES      22   /* 精确节点数（含 REMIND 提示音源节点）*/
#define EFFECT_GRAPH_MAX_EDGES      24   /* 精确边数（当前23条+1预留）节省 24*8=192 bytes */
#define EFFECT_GRAPH_MAX_INPUTS     4    /* 最大输入端口数 */
#define EFFECT_GRAPH_MAX_OUTPUTS    4    /* 最大输出端口数 */
#define EFFECT_GRAPH_NAME_LEN       16   /* 节点名称长度 */
#define EFFECT_GRAPH_BUFFER_SIZE    200 /* 节点缓冲区大小：21×256×4=21504字节 (21KB)
                                          * 现在使用PSRAM分配，不占用内部RAM
                                          * 支持蓝牙、ADC等最大帧长需求 */

/*******************************************************************************
 * 节点类型定义
 ******************************************************************************/
typedef enum {
    /* 源节点 - 产生数据 */
    EFFECT_NODE_TYPE_SOURCE_ADC0 = 0,      /* ADC0输入源 (吉他) */
    EFFECT_NODE_TYPE_SOURCE_ADC1,          /* ADC1输入源 (麦克风) */
    EFFECT_NODE_TYPE_SOURCE_USB_IN,        /* USB音频输入 */
    EFFECT_NODE_TYPE_SOURCE_BT_IN,         /* 蓝牙音频输入 */
    EFFECT_NODE_TYPE_SOURCE_METRONOME,     /* 节拍器源节点 */
    EFFECT_NODE_TYPE_SOURCE_REMIND,        /* 提示音源节点（WAV/MP3） */
    EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY,   /* Looper播放源节点 */
    
    /* 输出节点 - 消费数据 */
    EFFECT_NODE_TYPE_SINK_DAC0,            /* DAC0输出 */
    EFFECT_NODE_TYPE_SINK_USB_OUT,         /* USB音频输出 */
    EFFECT_NODE_TYPE_SINK_LOOPER_RECORD,   /* Looper录制输出节点 */
    
    /* 处理节点 - 处理数据 */
    EFFECT_NODE_TYPE_MIXER,                /* 混音器 */
    EFFECT_NODE_TYPE_EFFECT_REVERB,        /* 混响效果 */
    EFFECT_NODE_TYPE_EFFECT_DRC,           /* 动态范围压缩 */
    EFFECT_NODE_TYPE_EFFECT_EQ,            /* 均衡器 */
    EFFECT_NODE_TYPE_EFFECT_EXPANDER,      /* 扩展器 */
    EFFECT_NODE_TYPE_EFFECT_HOWLING,       /* 啸叫抑制 */
    EFFECT_NODE_TYPE_EFFECT_NOISE_GATE,    /* 噪声门 */
    EFFECT_NODE_TYPE_EFFECT_GAIN,          /* 增益控制 */
    EFFECT_NODE_TYPE_EFFECT_DELAY,         /* 延迟效果 */
    EFFECT_NODE_TYPE_EFFECT_CHORUS,        /* 合唱效果 */
    EFFECT_NODE_TYPE_LOOPER,               /* 循环录音器(旧版兼容) */
    
    EFFECT_NODE_TYPE_MAX
} EffectNodeType_t;

/*******************************************************************************
 * 节点状态
 ******************************************************************************/
typedef enum {
    NODE_STATE_IDLE = 0,            /* 空闲 */
    NODE_STATE_READY,               /* 就绪 */
    NODE_STATE_PROCESSING,          /* 处理中 */
    NODE_STATE_ERROR                /* 错误 */
} EffectNodeState_t;

/*******************************************************************************
 * 错误码
 ******************************************************************************/
typedef enum {
    GRAPH_OK = 0,
    GRAPH_ERR_NULL_PTR,
    GRAPH_ERR_NO_MEMORY,
    GRAPH_ERR_NODE_FULL,
    GRAPH_ERR_EDGE_FULL,
    GRAPH_ERR_INVALID_NODE,
    GRAPH_ERR_INVALID_EDGE,
    GRAPH_ERR_CYCLE_DETECTED,
    GRAPH_ERR_NOT_INITIALIZED
} GraphError_t;

/*******************************************************************************
 * 前向声明
 ******************************************************************************/
typedef struct EffectNode EffectNode_t;
typedef struct EffectEdge EffectEdge_t;
typedef struct EffectGraph EffectGraphRuntime_t;

/*******************************************************************************
 * 节点处理回调函数类型
 ******************************************************************************/
/* 源节点: 产生数据 - 使用 uint32_t 与硬件接口统一 */
typedef uint16_t (*NodeSourceFunc_t)(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);

/* 源节点: 查询可用数据量 (用于自适应帧长) */
typedef uint16_t (*NodeSourceAvailFunc_t)(EffectNode_t *node);

/* 处理节点: 处理数据 (支持多输入) - 使用 uint32_t 与硬件接口统一 */
typedef void (*NodeProcessFunc_t)(EffectNode_t *node, 
                                   uint32_t **in_bufs, uint8_t in_count,
                                   uint32_t *out_buf, uint16_t len);

/* 输出节点: 消费数据 - 使用 uint32_t 与硬件接口统一 */
typedef void (*NodeSinkFunc_t)(EffectNode_t *node, uint32_t *in_buf, uint16_t len);

/*******************************************************************************
 * 效果器参数联合体
 ******************************************************************************/
typedef union {
    /* 增益参数 */
    struct {
        int16_t gain_db;            /* 增益值 dB (-60 ~ +20) */
    } gain;
    
    /* 混音器参数 */
    struct {
        uint8_t input_count;        /* 输入通道数 */
        int16_t input_gains[EFFECT_GRAPH_MAX_INPUTS]; /* 各输入增益 */
    } mixer;
    
    /* 混响参数 */
    struct {
        uint8_t room_size;          /* 房间大小 0-100 */
        uint8_t damping;            /* 阻尼 0-100 */
        uint8_t wet_dry;            /* 干湿比 0-100 */
    } reverb;
    
    /* DRC参数 */
    struct {
        int16_t threshold;          /* 阈值 dB */
        uint8_t ratio;              /* 压缩比 */
        uint8_t attack;             /* 启动时间 ms */
        uint8_t release;            /* 释放时间 ms */
    } drc;
    
    /* EQ参数 */
    struct {
        uint8_t band_count;         /* 频段数 */
        int8_t band_gains[10];      /* 各频段增益 dB */
        uint8_t band_types[10];     /* 各频段类型 (0:lowpass, 1:highpass, 2:bandpass, 3:notch, 4:peaking, 5:lowshelf, 6:highshelf) */
        uint32_t band_f0[10];       /* 各频段中心频率 Hz */
        uint32_t band_Q[10];        /* 各频段Q值 */
        uint8_t band_enables[10];   /* 各频段使能状态 */
        int16_t pregain;            /* 预增益 dB */
    } eq;
    
    /* 扩展器参数 */
    struct {
        int16_t threshold;          /* 阈值 dB */
        uint8_t ratio;              /* 扩展比 */
    } expander;
    
    /* 延迟参数 */
    struct {
        uint16_t delay_ms;          /* 延迟时间 ms */
        uint8_t feedback;           /* 反馈量 0-100 */
        uint8_t wet_dry;            /* 干湿比 0-100 */
    } delay;
    
    /* 通用参数 */
    uint8_t raw[88];
} EffectParams_t;

/*******************************************************************************
 * 边结构 (连接两个节点)
 ******************************************************************************/
struct EffectEdge {
    uint8_t             id;                 /* 边ID */
    EffectNode_t       *src_node;           /* 源节点 */
    EffectNode_t       *dst_node;           /* 目标节点 */
    uint8_t             src_port;           /* 源端口 */
    uint8_t             dst_port;           /* 目标端口 */
    bool                enabled;            /* 是否启用 */
};

/*******************************************************************************
 * 节点结构
 ******************************************************************************/
struct EffectNode {
    uint8_t             id;                 /* 节点ID */
    char                name[EFFECT_GRAPH_NAME_LEN]; /* 节点名称 */
    EffectNodeType_t    type;               /* 节点类型 */
    EffectNodeState_t   state;              /* 节点状态 */
    bool                enabled;            /* 是否启用 */
    bool                bypass;             /* 旁路模式 */
    
    /* 处理函数 */
    union {
        NodeSourceFunc_t  source;           /* 源节点处理函数 */
        NodeProcessFunc_t process;          /* 处理节点函数 */
        NodeSinkFunc_t    sink;             /* 输出节点函数 */
    } func;
    
    /* 源节点可用数据量查询函数(用于自适应帧长) */
    NodeSourceAvailFunc_t avail_func;
    
    /* 效果参数 */
    EffectParams_t      params;
    void               *effect_ctx;         /* 效果器上下文(SDK结构体) */
    
    /* 连接信息 */
    uint8_t             input_count;        /* 输入端口数 */
    uint8_t             output_count;       /* 输出端口数 */
    EffectEdge_t       *inputs[EFFECT_GRAPH_MAX_INPUTS];   /* 输入边 */
    EffectEdge_t       *outputs[EFFECT_GRAPH_MAX_OUTPUTS]; /* 输出边 */
    
    /* 处理缓冲区 - 使用 uint32_t 与硬件接口统一 */
    uint32_t           *buffer;             /* 输出缓冲区 */
    uint16_t            buffer_len;         /* 缓冲区有效长度 */
    bool                processed;          /* 当前帧是否已处理 */
    
    /* 拓扑排序用 */
    uint8_t             in_degree;          /* 入度(拓扑排序用) */
};

/*******************************************************************************
 * 图配置结构 (用于创建图)
 ******************************************************************************/
typedef struct {
    uint8_t             node_id;
    EffectNodeType_t    type;
    const char         *name;
    bool                enabled;
    EffectParams_t      params;
} NodeConfig_t;

typedef struct {
    uint8_t             src_node_id;
    uint8_t             dst_node_id;
    uint8_t             src_port;
    uint8_t             dst_port;
} EdgeConfig_t;

typedef struct {
    NodeConfig_t       *nodes;
    uint8_t             node_count;
    EdgeConfig_t       *edges;
    uint8_t             edge_count;
    uint16_t            sample_rate;
} GraphConfig_t;

/*******************************************************************************
 * 驱动模式枚举 (决定帧长由哪个源决定)
 ******************************************************************************/
typedef enum {
    DRIVE_MODE_ADC = 0,             /* ADC驱动模式: 帧长由ADC可用数据量决定 */
    DRIVE_MODE_BT,                  /* 蓝牙驱动模式: 帧长由蓝牙解码数据量决定 */
    DRIVE_MODE_USB,                 /* USB驱动模式: 帧长由USB可用数据量决定 */
    DRIVE_MODE_FIXED                /* 固定帧长模式 */
} GraphDriveMode_t;

/*******************************************************************************
 * 效果图结构
 ******************************************************************************/
struct EffectGraph {
    bool                initialized;
    uint16_t            sample_rate;
    
    /* 驱动模式配置 */
    GraphDriveMode_t    drive_mode;         /* 当前驱动模式 */
    EffectNode_t       *primary_source;     /* 主驱动源节点 */
    uint16_t            min_frame_size;     /* 最小帧长 */
    uint16_t            max_frame_size;     /* 最大帧长 */
    
    /* 节点存储 */
    EffectNode_t        nodes[EFFECT_GRAPH_MAX_NODES];
    uint8_t             node_count;
    
    /* 边存储 */
    EffectEdge_t        edges[EFFECT_GRAPH_MAX_EDGES];
    uint8_t             edge_count;
    
    /* 拓扑排序后的处理顺序 */
    EffectNode_t       *process_order[EFFECT_GRAPH_MAX_NODES];
    uint8_t             process_count;
    
    /* 源节点和输出节点快速访问
     * 源节点：ADC0, ADC1, USB_IN, BT_IN, Metronome, Looper_Play (共 6 个, 预留 2)
     * Sink节点：DAC0_Out, USB_Out, Looper_Record (共 3 个, 预留 1) */
    EffectNode_t       *source_nodes[8];   /* 扩展到 8 (from 4)，容纳所有源节点 */
    uint8_t             source_count;
    EffectNode_t       *sink_nodes[4];     /* 扩展到 4 (from 2)，容纳 DAC/USB/Looper_Record */
    uint8_t             sink_count;
    
    /* 节点缓冲区改为堆分配（方案1优化）：节省约 21KB BSS
     * 每个节点的 buffer 在启用时按需分配，禁用时释放 */
};

/*******************************************************************************
 * 公共API
 ******************************************************************************/

#if EFFECT_GRAPHICS_EN

/**
 * @brief 初始化效果图系统
 * @return 错误码
 */
GraphError_t EffectGraph_Init(void);

/**
 * @brief 获取图实例
 * @return 图指针
 */
EffectGraphRuntime_t* EffectGraph_GetInstance(void);

/**
 * @brief 从配置创建图
 * @param config 图配置
 * @return 错误码
 */
GraphError_t EffectGraph_CreateFromConfig(const GraphConfig_t *config);

/**
 * @brief 添加节点
 * @param type 节点类型
 * @param name 节点名称
 * @param enabled 是否启用
 * @return 节点指针，失败返回NULL
 */
EffectNode_t* EffectGraph_AddNode(EffectNodeType_t type, const char *name, bool enabled);

/**
 * @brief 连接两个节点
 * @param src_node 源节点
 * @param dst_node 目标节点
 * @param src_port 源端口
 * @param dst_port 目标端口
 * @return 错误码
 */
GraphError_t EffectGraph_Connect(EffectNode_t *src_node, EffectNode_t *dst_node,
                                  uint8_t src_port, uint8_t dst_port);

/**
 * @brief 断开两个节点的连接
 * @param src_node 源节点
 * @param dst_node 目标节点
 * @return 错误码
 */
GraphError_t EffectGraph_Disconnect(EffectNode_t *src_node, EffectNode_t *dst_node);

/**
 * @brief 构建处理顺序(拓扑排序)
 * @return 错误码
 */
GraphError_t EffectGraph_Build(void);

/**
 * @brief 处理一帧音频
 * @param frame_size 帧大小(samples)
 * @return 实际处理的样本数
 */
uint16_t EffectGraph_Process(uint16_t frame_size);

/**
 * @brief 设置节点启用状态
 * @param node 节点指针
 * @param enabled 是否启用
 */
void EffectGraph_SetNodeEnabled(EffectNode_t *node, bool enabled);

/**
 * @brief 设置节点旁路状态
 * @param node 节点指针
 * @param bypass 是否旁路
 */
void EffectGraph_SetNodeBypass(EffectNode_t *node, bool bypass);

/**
 * @brief 根据名称查找节点
 * @param name 节点名称
 * @return 节点指针，未找到返回NULL
 */
EffectNode_t* EffectGraph_FindNodeByName(const char *name);

/**
 * @brief 根据ID查找节点
 * @param id 节点ID
 * @return 节点指针，未找到返回NULL
 */
EffectNode_t* EffectGraph_FindNodeById(uint8_t id);

/**
 * @brief 设置节点参数
 * @param node 节点指针
 * @param params 参数
 * @return 错误码
 */
GraphError_t EffectGraph_SetNodeParams(EffectNode_t *node, const EffectParams_t *params);

/**
 * @brief 重置图(清除所有节点和边)
 */
void EffectGraph_Reset(void);

/**
 * @brief 打印图信息(调试用)
 */
void EffectGraph_PrintInfo(void);

/**
 * @brief 创建默认音频图(ADC0+ADC1 -> Mixer -> Effects -> DAC0)
 * @param sample_rate 采样率
 * @return 错误码
 */
GraphError_t EffectGraph_CreateDefault(uint16_t sample_rate);

/**
 * @brief 设置驱动模式
 * @param mode 驱动模式
 * @param primary_source 主驱动源节点(可为NULL使用自动选择)
 * @return 错误码
 */
GraphError_t EffectGraph_SetDriveMode(GraphDriveMode_t mode, EffectNode_t *primary_source);

/**
 * @brief 自适应帧长处理 - 根据主驱动源的可用数据量决定帧长
 * @return 实际处理的样本数
 */
uint16_t EffectGraph_ProcessAdaptive(void);

/**
 * @brief 查询所有源节点的可用数据量，返回最小值
 * @return 所有启用的源节点中可用数据量的最小值
 */
uint16_t EffectGraph_GetAvailableFrameSize(void);

#else /* !EFFECT_GRAPHICS_EN */

/* EFFECT_GRAPHICS_EN=0：所有 API 替换为空操作，调用方无需修改
 * 
 * Stub 宏规则:
 *   - 函数宏必须包含完整参数列表，使其能正确替换 extern 函数声明
 *   - 例：EffectGraph_FindNodeById(id) 宏中 (id) 必须与函数声明 (uint8_t id) 对应
 *   - 无参函数必须含空列表 ()，例：EffectGraph_GetInstance()
 * 这样可在代码中做 extern 声明后直接使用，参数会被宏吃掉
 */
#define EffectGraph_Init()                          (GRAPH_OK)
#define EffectGraph_GetInstance()                   ((EffectGraphRuntime_t*)0)
#define EffectGraph_CreateFromConfig(cfg)           (GRAPH_OK)
#define EffectGraph_AddNode(t,n,e)                  ((EffectNode_t*)0)
#define EffectGraph_Connect(s,d,sp,dp)              (GRAPH_OK)
#define EffectGraph_Disconnect(s,d)                 (GRAPH_OK)
#define EffectGraph_Build()                         (GRAPH_OK)
#define EffectGraph_Process(fs)                     (0)
#define EffectGraph_ProcessAdaptive()               (0)
#define EffectGraph_GetAvailableFrameSize()         (0)
#define EffectGraph_SetNodeEnabled(n,e)             ((void)0)
#define EffectGraph_SetNodeBypass(n,b)              ((void)0)
#define EffectGraph_FindNodeByName(name)            ((EffectNode_t*)0)
#define EffectGraph_FindNodeById(id)                ((EffectNode_t*)0)
#define EffectGraph_SetNodeParams(n,p)              (GRAPH_OK)
#define EffectGraph_Reset()                         ((void)0)
#define EffectGraph_PrintInfo()                     ((void)0)
#define EffectGraph_CreateDefault(sr)               (GRAPH_OK)
#define EffectGraph_SetDriveMode(m,s)               (GRAPH_OK)

#endif /* EFFECT_GRAPHICS_EN */

#ifdef __cplusplus
}
#endif

#endif /* __EFFECT_GRAPH_H__ */
