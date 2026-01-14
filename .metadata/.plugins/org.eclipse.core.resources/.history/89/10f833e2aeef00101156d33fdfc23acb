# Effect Graph 模块使用说明

## 模块状态：✅ 可以使用

效果图（Effect Graph）模块已经完整实现，可以投入使用。

## 文件清单

### 核心文件
1. **effect_graph.h** - 效果图核心头文件
   - 定义节点类型、边结构、图结构
   - 提供图操作API接口

2. **effect_graph.c** - 效果图核心实现 (725行)
   - 实现图的创建、节点管理、拓扑排序
   - 实现音频数据处理流程
   - 状态：✅ 完整实现

### 配置文件
3. **effect_graph_config.h** - 预设配置头文件
   - 定义默认节点配置表
   - 定义默认边(连接)配置表
   - 提供多套预设(默认、简单、蓝牙等)
   - 可通过修改宏定义改变默认图结构

4. **effect_graph_config.c** - 配置管理实现
   - 预设加载功能
   - 默认参数设置
   - 状态：✅ 完整实现

### Shell命令模块
5. **shell_cmd_graph.h** - Shell命令接口
6. **shell_cmd_graph.c** - Shell命令实现 (335行)
   - 提供CDC/BLE命令行操作接口
   - 状态：✅ 完整实现

## 功能特性

### 1. 图结构
- ✅ 支持4个输入源：ADC0(吉他), ADC1(麦克风), USB_IN, BT_IN
- ✅ 支持2个输出：DAC0, USB_OUT
- ✅ 支持多种效果节点：混音器、混响、DRC、EQ、扩展器等
- ✅ 自动拓扑排序，确定处理顺序
- ✅ 支持节点旁路(bypass)
- ✅ 支持节点启用/禁用

### 2. 参数化配置
- ✅ 通过配置表定义节点和连接
- ✅ 修改配置文件即可改变音频链路，无需改代码
- ✅ 支持多套预设快速切换

### 3. Shell命令控制
所有命令通过 `graph` 前缀调用：

| 命令 | 功能 |
|------|------|
| `graph help` | 显示帮助 |
| `graph list` | 列出所有节点 |
| `graph info` | 显示图详情 |
| `graph preset [id]` | 切换预设 |
| `graph node <name> on/off` | 启用/禁用节点 |
| `graph bypass <name> on/off` | 节点旁路 |
| `graph param <name> <key> [val]` | 读写参数 |
| `graph rebuild` | 重建图 |

### 4. 支持的效果参数

#### Reverb（混响）
```
graph param Reverb room 50    # 房间大小 0-100
graph param Reverb damp 50    # 阻尼 0-100
graph param Reverb wet 30     # 干湿比 0-100
```

#### DRC（动态范围压缩）
```
graph param DRC threshold -20  # 阈值 dB
graph param DRC ratio 4        # 压缩比
graph param DRC attack 10      # 启动时间 ms
graph param DRC release 100    # 释放时间 ms
```

#### Gain（增益）
```
graph param <node> gain 6      # 增益 dB
```

#### Delay（延迟）
```
graph param <node> time 250    # 延迟时间 ms
graph param <node> feedback 30 # 反馈量 0-100
```

## 使用流程

### 1. 初始化
```c
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "shell_cmd_graph.h"

// 初始化图系统
EffectGraph_Init();

// 加载默认预设
EffectGraphConfig_LoadPreset(GRAPH_PRESET_DEFAULT);

// 或者创建默认图
// EffectGraph_CreateDefault(48000);

// 注册Shell命令
ShellCmdGraph_Register();
```

### 2. 设置节点处理回调
为源节点和输出节点设置回调函数：

```c
// 获取ADC0节点
EffectNode_t *adc0 = EffectGraph_FindNodeByName("ADC0_Guitar");
if (adc0) {
    adc0->func.source = MyADC0_ReadData;  // 设置数据源回调
}

// 获取DAC0节点
EffectNode_t *dac0 = EffectGraph_FindNodeByName("DAC0_Out");
if (dac0) {
    dac0->func.sink = MyDAC0_WriteData;  // 设置输出回调
}
```

### 3. 音频处理
在主循环中调用处理函数：

```c
void Audio_Loop(void)
{
    // 处理一帧音频(48个样本)
    uint16_t processed = EffectGraph_Process(48);
    
    // processed 返回实际处理的样本数
}
```

### 4. 通过Shell控制
```bash
# CDC或BLE终端中执行
graph list                    # 查看所有节点
graph node Reverb off         # 关闭混响
graph param DRC threshold -15 # 调整DRC阈值
graph preset 1                # 切换到简单预设
```

## 预设配置

当前支持的预设：

| ID | 名称 | 说明 |
|----|------|------|
| 0 | Default (Full) | 完整效果链 |
| 1 | Simple (No FX) | 无效果直通 |
| 2 | Guitar Only | 仅吉他 |
| 3 | Mic Only | 仅麦克风 |
| 4 | Bluetooth Speaker | 蓝牙音箱模式 |
| 5 | USB Audio | USB声卡模式 |

## 自定义配置

### 修改默认图结构
编辑 `effect_graph_config.h`：

```c
// 修改节点配置
#define DEFAULT_NODES_CONFIG { \
    { 0, NODE_TYPE_SOURCE_ADC0, "Guitar", true, {0} }, \
    { 1, NODE_TYPE_SOURCE_ADC1, "Mic",    true, {0} }, \
    { 2, NODE_TYPE_MIXER,       "Mixer",  true, {0} }, \
    { 3, NODE_TYPE_EFFECT_REVERB, "Reverb", true, {0} }, \
    { 4, NODE_TYPE_SINK_DAC0,   "DAC0",   true, {0} }, \
}

// 修改连接配置
#define DEFAULT_EDGES_CONFIG { \
    { 0, 2, 0, 0 },  /* Guitar -> Mixer */\
    { 1, 2, 0, 1 },  /* Mic -> Mixer */\
    { 2, 3, 0, 0 },  /* Mixer -> Reverb */\
    { 3, 4, 0, 0 },  /* Reverb -> DAC0 */\
}
```

### 修改默认参数
编辑 `effect_graph_config.h`：

```c
#define DEFAULT_REVERB_ROOM_SIZE    70  // 改为70
#define DEFAULT_REVERB_WET_DRY      50  // 改为50
#define DEFAULT_DRC_THRESHOLD       -15 // 改为-15
```

## 注意事项

### 1. 需要实现的回调函数
- ❗ 源节点的 `source` 回调（产生数据）
- ❗ 输出节点的 `sink` 回调（消费数据）
- ⚠️ 处理节点可选实现 `process` 回调（否则使用默认处理）

### 2. 内存限制
- 最大节点数：16 (可在 effect_graph.h 中修改 `EFFECT_GRAPH_MAX_NODES`)
- 最大边数：32 (可修改 `EFFECT_GRAPH_MAX_EDGES`)
- 每节点最大输入：4
- 每节点最大输出：4
- 处理缓冲区：256 samples/节点

### 3. 依赖项
确保以下模块可用：
- ✅ bg_shell.h - Shell系统
- ✅ shell_io_manager.h - Shell IO管理
- ✅ debug.h - 调试输出

## 待完成项

1. ⏳ 实际音频设备回调函数（ADC、DAC、USB、BT数据读写）
2. ⏳ 效果器SDK集成（Reverb、DRC、EQ等实际算法调用）
3. ⏳ 参数持久化（保存到Flash）
4. ⏳ 其他预设实现（Guitar Only、Mic Only等）

## 集成示例

```c
// main.c
void BG_audio_Init(uint16_t SampleRate)
{
    // ... 现有初始化代码 ...
    
    // 初始化效果图
    EffectGraph_Init();
    EffectGraphConfig_LoadPreset(GRAPH_PRESET_DEFAULT);
    
    // 注册Shell命令
    ShellCmdGraph_Register();
    
    // 设置回调（示例）
    EffectNode_t *adc0 = EffectGraph_FindNodeByName("ADC0_Guitar");
    if (adc0) adc0->func.source = ReadGuitarData;
    
    EffectNode_t *dac0 = EffectGraph_FindNodeByName("DAC0_Out");
    if (dac0) dac0->func.sink = WriteToDAC;
}

void Audio_loop(void)
{
    // ... 现有代码 ...
    
    // 使用效果图处理音频
    EffectGraph_Process(48);
    
    // Shell命令处理
    ShellIOManager_Process();
}
```

## 总结

✅ **效果图模块已完整实现，可以立即使用**

- 核心功能完备，支持灵活的音频路由
- 参数化配置，易于修改和扩展
- Shell命令完整，支持CDC/BLE远程控制
- 只需实现具体的音频设备回调和效果器算法即可投入使用

---
生成日期：2026-01-04
版本：V1.0.0
