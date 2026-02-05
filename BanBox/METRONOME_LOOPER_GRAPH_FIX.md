# Metronome和Looper节点在Graph中不显示问题 - 修复总结

## 问题描述

用户发现：
- `metro` 命令可以正常工作并显示状态
- 但 `graph list` 和 `chain -w` 命令中**没有显示metronome和looper节点**
- 只显示了14个节点，缺少 metronome/looper_play/looper_record 三个节点

## 根因分析

### 1. 节点类型定义已存在
在 `effect_graph.h` 中已经定义了节点类型：
```c
EFFECT_NODE_TYPE_SOURCE_METRONOME,     /* 节拍器源节点 */
EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY,   /* Looper播放源节点 */
EFFECT_NODE_TYPE_SINK_LOOPER_RECORD,   /* Looper录制输出节点 */
```

### 2. 节点配置已定义
在 `effect_graph_config.h` 中，`DEFAULT_NODES_CONFIG` 宏已包含了17个节点（包括metronome和looper）。

### 3. **问题1：边数量配置错误**
- `DEFAULT_EDGES_CONFIG` 宏中添加了3条新边（metronome、looper相关连接）
- 但 `DEFAULT_EDGE_COUNT` 仍然是 **16**，应该是 **19**
- 导致最后3条边没有被处理，metronome和looper节点成为孤立节点

### 4. **问题2：graph list命令缺少节点类型显示**
在 `shell_cmd_graph.c` 的 `CmdList()` 函数中，switch语句缺少metronome和looper的case分支，导致这些节点显示为"Unknown"类型。

### 5. 回调函数已实现
在 `bg_audio_io_manager.c` 中：
- `Metronome_SourceCallback` - 节拍器源数据生成
- `LooperPlay_SourceCallback` - Looper播放源数据读取
- `LooperRecord_SinkCallback` - Looper录制数据写入
- 这些回调已经在 `BG_AudioManager_SetupEffectGraphCallbacks()` 中注册

## 修复内容

### 修改1：修复边数量宏定义
**文件：** `BanBox/src/banux/05_component/effect_graph/effect_graph_config.h`

```c
// 修改前：
#define DEFAULT_EDGE_COUNT  16

// 修改后：
#define DEFAULT_EDGE_COUNT  19
```

### 修改2：添加节点类型显示
**文件：** `BanBox/src/banux/05_component/effect_graph/shell_cmd_graph.c`

在 `CmdList()` 函数的switch语句中添加：
```c
case EFFECT_NODE_TYPE_SOURCE_METRONOME: type_str = "METRONOME"; break;
case EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY: type_str = "LOOPER_PLAY"; break;
case EFFECT_NODE_TYPE_SINK_LOOPER_RECORD: type_str = "LOOPER_REC"; break;
```

在 `PrintNodeParams()` 函数中添加：
```c
case EFFECT_NODE_TYPE_SOURCE_METRONOME:
    Shell_Printf("Type: METRONOME SOURCE (no params)\n");
    break;
    
case EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY:
    Shell_Printf("Type: LOOPER PLAYBACK SOURCE (no params)\n");
    break;
    
case EFFECT_NODE_TYPE_SINK_LOOPER_RECORD:
    Shell_Printf("Type: LOOPER RECORD SINK (no params)\n");
    break;
```

## 节点拓扑结构

修复后，完整的effect graph拓扑为：

```
【ADC吉他/麦克风路径】
N0  Guitar ──┐
N1  Mic    ──┴──> N4 ADC_Mixer ──> N5 Expander ──> N6 DRC ──> N7 EQ ──> N8 Reverb ──┐
                                                                                      │
【Looper循环录音】                                                                    │
N4 ADC_Mixer ──────────────────────> N16 Looper_Record                              │
                                                                                      │
【Looper播放回放】                                                                    │
N15 Looper_Play ─────────────────────> N7 EQ (port 1)                              │
                                                                                      │
【USB/蓝牙/节拍器路径】                                                              │
N2  USB_In ──┐                                                                       │
N3  BT_In  ──┤──> N9 USB_BT_Mixer ──> N10 USB_BT_EQ ──────────────────────────────┤
N14 Metronome ─┘ (port 2)                                                           │
                                                                                      │
【最终混音输出】                                                                      │
N8  Reverb ──┐                                                                       │
N10 USB_BT_EQ ─┴──> N11 Final_Mixer ──┬──> N12 DAC0_Out                            │
                                        └──> N13 USB_Out                              │
```

### 关键连接说明

1. **Metronome → USB_BT_Mixer (port 2)**
   - 节拍器音频混入USB/BT通道，不影响吉他/麦克风录音

2. **Looper_Play → EQ (port 1)**
   - Looper回放插入DRC之后、EQ之前
   - 与吉他/麦克风信号在EQ处混合

3. **ADC_Mixer → Looper_Record**
   - 录制吉他+麦克风的原始混音
   - 在效果器链之前采样，保证录音质量

## 验证步骤

### 1. 编译验证
```bash
cd BanBox
make clean
make
```

### 2. 运行时验证
```
$ graph list

===== Graph Nodes [17/64] =====
ID  Name            Type        Status
--- --------------- ----------- --------
 0        guitar_in        ADC0 ON 
 1           mic_in        ADC1 ON 
 2           usb_in      USB_IN ON 
 3            bt_in       BT_IN ON 
 4        adc_mixer       MIXER ON 
 5        expander    EXPANDER ON 
 6              drc         DRC ON 
 7               eq          EQ ON 
 8           reverb      REVERB ON 
 9    usb_bt_mixer       MIXER ON 
10       usb_bt_eq          EQ ON 
11     final_mixer       MIXER ON 
12          dac_out        DAC0 ON 
13          usb_out     USB_OUT ON 
14        metronome   METRONOME ON      <-- 新增
15      looper_play LOOPER_PLAY ON      <-- 新增
16    looper_record  LOOPER_REC ON      <-- 新增
===============================
```

### 3. 节点详细信息
```
$ graph get 14
=== Node[14]: metronome ===
Status: Enabled
Type: METRONOME SOURCE (no params)
========================

$ graph get 15
=== Node[15]: looper_play ===
Status: Enabled
Type: LOOPER PLAYBACK SOURCE (no params)
========================

$ graph get 16
=== Node[16]: looper_record ===
Status: Enabled
Type: LOOPER RECORD SINK (no params)
========================
```

### 4. 控制命令测试
```
# 启用/禁用metronome节点（控制节拍器是否输出到graph）
$ graph node metronome off
$ graph node metronome on

# 启用/禁用looper播放
$ graph node looper_play off
$ graph node looper_play on

# 启用/禁用looper录制
$ graph node looper_record off
$ graph node looper_record on
```

## 注意事项

### Metronome控制层次
- **`metro on/off`**: 控制metronome模块是否生成节拍音频
- **`graph node metronome on/off`**: 控制metronome节点是否输出到graph
- 两者需要都启用才能听到节拍声

### Looper控制层次
- **`looper rec/play/stop`**: 控制looper模块的录制/播放状态
- **`graph node looper_play on/off`**: 控制looper播放节点是否输出到graph
- **`graph node looper_record on/off`**: 控制looper录制节点是否接收输入

### Chain命令兼容性
`chain -w` 命令使用的是sys_param中的旧架构（NODE_TYPE_SOURCE/EFFECT/MIXER/OUTPUT），与新的effect_graph架构是两套独立系统。如果需要在chain命令中看到looper节点，需要单独更新sys_param架构。

## 总结

修复非常简单，只需要：
1. 将 `DEFAULT_EDGE_COUNT` 从16改为19
2. 在shell命令显示代码中添加metronome和looper的case分支

这个问题是典型的**配置不一致**错误：
- 节点定义✅（17个节点）
- 边定义✅（19条边）
- 边数量配置❌（写的是16）
- 显示代码❌（缺少case分支）

修复后，metronome和looper将正常出现在effect graph中，可以通过graph命令控制其启用/禁用状态，实现更灵活的音频路由。
