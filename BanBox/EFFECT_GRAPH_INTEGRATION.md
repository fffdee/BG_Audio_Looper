# Effect Graph 模块集成说明

## 概述

Effect Graph 模块已成功集成到 BanBox 音频工程中。该模块提供了一个基于图数据结构的音频处理框架，可以灵活配置音频路由和效果链。

## 音频流图

```
【ADC 效果链路径】
ADC0 (Guitar) ──┐
ADC1 (Mic)    ──┴──> ADC_Mixer -> Expander -> DRC -> EQ -> Reverb ──┐
                                                                     │
【USB/BT 直通路径】                                                   │
USB_In ──┐                                                          │
BT_In  ──┴──> USB_BT_Mixer -> USB_BT_EQ ──────────────────────────┤
                                                                     │
【最终混音输出】                                                      │
Reverb ──┐                                                          │
USB_BT_EQ ──┴──> Final_Mixer ──┬──> DAC0_Out
                                └──> USB_Out
```

## 主要特性

1. **ADC0/ADC1 完整效果链**
   - 吉他和麦克风输入经过完整效果处理链
   - 效果顺序: Expander -> DRC -> EQ -> Reverb
   - 支持独立启用/禁用每个效果

2. **USB/BT 快速路径**
   - USB和蓝牙音频只经过 EQ 处理
   - 低延迟设计，适合实时播放

3. **灵活配置**
   - 通过 `effect_graph_config.h` 修改节点和边配置
   - 支持多种预设切换 (Default, Simple, Bluetooth Speaker 等)
   - Shell 命令支持运行时调整

## 文件结构

```
BanBox/src/banux/05_component/effect_graph/
├── effect_graph.h           - 核心数据结构和API声明
├── effect_graph.c           - 核心实现（拓扑排序、节点处理）
├── effect_graph_config.h    - 配置定义（节点、边、预设）
├── effect_graph_config.c    - 配置加载和预设管理
├── shell_cmd_graph.h        - Shell命令接口声明
└── shell_cmd_graph.c        - Shell命令实现

BanBox/src/banux/06_app/BG_AudioIO_Manager/
└── bg_audio_io_manager.c    - 音频设备回调和主循环集成
```

## 节点配置 (共14个节点)

| ID | 名称 | 类型 | 说明 |
|----|------|------|------|
| 0 | guitar_in | SOURCE_ADC0 | 吉他输入 (ADC0) |
| 1 | mic_in | SOURCE_ADC1 | 麦克风输入 (ADC1) |
| 2 | usb_in | SOURCE_USB_IN | USB音频输入 |
| 3 | bt_in | SOURCE_BT_IN | 蓝牙音频输入 |
| 4 | adc_mixer | MIXER | ADC混音器 |
| 5 | expander | EFFECT_EXPANDER | 扩展器 |
| 6 | drc | EFFECT_DRC | 动态范围压缩 |
| 7 | eq | EFFECT_EQ | 均衡器 |
| 8 | reverb | EFFECT_REVERB | 混响 |
| 9 | usb_bt_mixer | MIXER | USB/BT混音器 |
| 10 | usb_bt_eq | EFFECT_EQ | USB/BT专用EQ |
| 11 | final_mixer | MIXER | 最终混音器 |
| 12 | dac_out | SINK_DAC0 | DAC输出 |
| 13 | usb_out | SINK_USB_OUT | USB音频输出 |

## 边配置 (共13条边)

连接关系见上方音频流图。

## 使用方法

### 1. 启用/禁用 Effect Graph 模式

在 `bg_audio_io_manager.c` 中设置:
```c
#define USE_EFFECT_GRAPH_MODE  1  // 1=启用, 0=使用传统模式
```

### 2. Shell 命令

通过 CDC/BLE 远程控制:
```
graph info      - 显示图信息
graph node <name> enable/disable  - 启用/禁用节点
graph node <name> bypass on/off   - 设置旁路
graph preset <id> - 切换预设
```

### 3. 切换预设

```c
EffectGraphConfig_LoadPreset(GRAPH_PRESET_DEFAULT);     // 完整配置
EffectGraphConfig_LoadPreset(GRAPH_PRESET_SIMPLE);      // 无效果直通
EffectGraphConfig_LoadPreset(GRAPH_PRESET_BLUETOOTH);   // 蓝牙音箱模式
```

## 自适应帧长机制 (2026-01-05 新增)

### 设计思路

参考老方案的"以数据可用量驱动帧长"处理方式，Effect Graph 现在支持自适应帧长：

1. **主驱动源决定帧长**: 图结构内指定一个"主驱动源"节点，它的可用数据量决定本帧帧长
2. **驱动模式切换**: 支持 ADC/BT/USB 三种驱动模式，可根据场景自动切换
3. **帧长限制**: 可配置最小/最大帧长，防止极端情况

### 新增 API

```c
// 设置驱动模式
GraphError_t EffectGraph_SetDriveMode(GraphDriveMode_t mode, EffectNode_t *primary_source);

// 自适应帧长处理 (核心函数)
uint16_t EffectGraph_ProcessAdaptive(void);

// 查询可用帧长
uint16_t EffectGraph_GetAvailableFrameSize(void);
```

### 驱动模式枚举

```c
typedef enum {
    DRIVE_MODE_ADC = 0,   // ADC驱动: 帧长由ADC数据量决定
    DRIVE_MODE_BT,        // BT驱动: 帧长由蓝牙解码数据量决定
    DRIVE_MODE_USB,       // USB驱动: 帧长由USB数据量决定
    DRIVE_MODE_FIXED      // 固定帧长模式
} GraphDriveMode_t;
```

### 源节点回调

每个源节点需要实现两个回调：
1. `source` - 读取数据
2. `avail_func` - 查询可用数据量

```c
// 示例：ADC0 回调
node->func.source = ADC0_ReadGuitarData;
node->avail_func = ADC0_GetAvailableData;
```

### 处理流程

`EffectGraph_ProcessAdaptive()` 的工作流程：

1. 查询主驱动源的可用数据量 → 决定本帧 `frame_size`
2. 如果数据不足 `min_frame_size`，返回 0
3. 按拓扑顺序处理所有节点
4. 各源节点根据 `frame_size` 读取数据
5. 混音/效果器节点处理数据
6. Sink 节点输出数据

### 蓝牙/ADC 模式自动切换

```c
static void AudioLoopWithGraph(void)
{
    // 检查蓝牙是否活跃
    bt_active = (GetA2dpState() == BT_A2DP_STATE_STREAMING) && 
                (mv_msize(&SBC_MemHandle) > SBC_DECODER_FIFO_MIN);
    
    // 自动切换驱动模式
    if (bt_active && last_drive_mode != DRIVE_MODE_BT) {
        EffectGraph_SetDriveMode(DRIVE_MODE_BT, NULL);
        last_drive_mode = DRIVE_MODE_BT;
    } else if (!bt_active && last_drive_mode != DRIVE_MODE_ADC) {
        EffectGraph_SetDriveMode(DRIVE_MODE_ADC, NULL);
        last_drive_mode = DRIVE_MODE_ADC;
    }
    
    // 调用自适应帧长处理
    processed_samples = EffectGraph_ProcessAdaptive();
}
```

## 代码规范

- 所有 for 循环使用 C89 风格（变量提前声明）
- 回调函数签名与 `EffectNode_t` 结构体一致
- 无 C99 特性使用

## 注意事项

1. 修改配置后需重新编译
2. 节点ID必须连续从0开始
3. 边数 `DEFAULT_EDGE_COUNT` 需与实际边数一致
4. 效果器参数通过 `gCtrlVars` 全局变量控制
5. **新增**: 源节点必须同时实现 `source` 和 `avail_func` 回调才能支持自适应帧长
