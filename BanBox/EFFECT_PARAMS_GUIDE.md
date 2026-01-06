# 效果器参数调节Shell命令使用指南

## 概述

`shell_cmd_effect.c/h` 模块提供了通过Shell命令行调节音频效果器参数的功能。用户可以通过CDC-UART或BLE串口实时查询和调节各种效果器的参数，便于音频调试和优化。

## 支持的效果器

| ID | 名称 | 描述 | 支持参数 |
|:--:|------|------|--------|
| 0 | reverb | 混响效果 | room, damp, wet |
| 1 | drc | 动态范围压缩 (麦克风) | threshold, ratio, attack, release |
| 2 | eq | 均衡器 (麦克风输出) | band0-9, gain |
| 3 | expander | 扩展器 | threshold, ratio |
| 4 | echo | 回声效果 | delay, feedback, wet |
| 5 | howling | 啸叫抑制 | - |
| 6 | 3d | 3D音效 | - |
| 7 | vbass | 虚拟低音 | - |
| 8 | plate_reverb | 板式混响 | - |
| 9 | music_drc | 动态范围压缩 (音乐) | threshold, ratio, attack, release |
| 10 | music_eq | 均衡器 (音乐输出) | band0-9, gain |

## 命令语法

### 1. 列出所有效果器

```bash
effect list
```

**输出示例：**
```
===== Audio Effects [11] =====
[ 0] reverb           - 混响效果 [ON]
[ 1] drc              - 动态范围压缩 (麦克风) [ON]
[ 2] eq               - 均衡器 (麦克风输出) [ON]
[ 3] expander         - 扩展器 [ON]
[ 4] echo             - 回声效果 [OFF]
[ 5] howling          - 啸叫抑制 [ON]
...
```

### 2. 显示效果器详细信息

```bash
effect info <id>
```

**示例：**
```bash
effect info 1
```

**输出：**
```
===== Effect 1: drc =====
Status:     Enabled
Available params:
  threshold - Threshold (dB)
  ratio     - Compression ratio
  attack    - Attack time (ms)
  release   - Release time (ms)
==========================
```

### 3. 获取效果器参数

```bash
effect get <id> <param>
```

**示例：**
```bash
effect get 1 threshold
effect get 1 ratio
```

**输出：**
```
[Effect 1] Getting parameter 'threshold'...
DRC threshold: -20 dB
```

### 4. 设置效果器参数

```bash
effect set <id> <param> <value>
```

**示例：**
```bash
effect set 1 threshold -25
effect set 1 ratio 6
effect set 1 attack 10
effect set 1 release 100
```

**输出：**
```
[Effect 1] Setting parameter 'threshold' to -25...
DRC threshold set to -25 dB
```

### 5. 启用/禁用效果器

```bash
effect enable <id> [on|off]
```

**示例：**
```bash
effect enable 1 on      # 启用DRC
effect enable 4 off     # 禁用Echo
effect enable 0         # 查询Reverb状态
```

**输出：**
```
Effect 'drc' enabled
```

### 6. 显示帮助

```bash
effect help
```

## 使用示例

### 场景1：解决音频失真问题

调节DRC和Expander参数以解决音频失真：

```bash
# 1. 启用并查看当前DRC设置
effect info 1

# 2. 调节DRC参数
effect set 1 threshold -20     # 降低阈值
effect set 1 ratio 4           # 调节压缩比
effect set 1 attack 5          # 快速启动
effect set 1 release 100       # 缓慢释放

# 3. 启用Expander以提高清晰度
effect enable 3 on
effect set 3 threshold -60
effect set 3 ratio 2

# 4. 查看结果
effect get 1 threshold
effect get 1 ratio
```

### 场景2：调节混响效果

```bash
# 1. 启用混响
effect enable 0 on

# 2. 查看混响参数
effect info 0

# 3. 调节混响参数
effect set 0 room 50       # 房间大小
effect set 0 damp 70       # 阻尼
effect set 0 wet 30        # 干湿比
```

### 场景3：启用均衡器

```bash
# 1. 查看EQ频段
effect info 2

# 2. 调节特定频段
effect set 2 band0 3       # 低频增益 3dB
effect set 2 band5 -2      # 中频衰减 -2dB
effect set 2 band9 5       # 高频增益 5dB

# 3. 查询结果
effect get 2 band0
```

### 场景4：音频调试工作流

```bash
# 1. 列出所有效果器
effect list

# 2. 检查每个效果器的状态
effect info 0
effect info 1
effect info 2
effect info 3

# 3. 根据需要调节参数
effect set 1 threshold -25
effect set 3 threshold -60

# 4. 保存配置 (如果实现了保存功能)
# effect save

# 5. 重置为默认值 (如果遇到问题)
# effect reset 1
```

## API函数参考

### 核心函数

#### `void ShellCmdEffect_Register(void)`
注册效果器Shell命令模块，在系统初始化时调用。

```c
// 在 bg_audio_io_manager.c 中调用：
ShellCmdEffect_Register();
```

#### `bool Effect_GetEnabled(EffectId_t id)`
获取效果器的启用状态。

```c
bool enabled = Effect_GetEnabled(EFFECT_ID_DRC);
if (enabled) {
    Shell_Printf("DRC is enabled\n");
}
```

#### `int Effect_SetEnabled(EffectId_t id, bool enabled)`
设置效果器的启用状态。

```c
Effect_SetEnabled(EFFECT_ID_DRC, true);   // 启用DRC
Effect_SetEnabled(EFFECT_ID_ECHO, false); // 禁用Echo
```

#### `const char* Effect_GetName(EffectId_t id)`
获取效果器的名称。

```c
const char *name = Effect_GetName(EFFECT_ID_DRC);
Shell_Printf("Effect name: %s\n", name);  // 输出: drc
```

### 参数操作函数

#### `int Effect_GetDRCParam(EffectId_t id, const char *param_name, int32_t *value)`
获取DRC参数值。

```c
int32_t threshold;
Effect_GetDRCParam(EFFECT_ID_DRC, "threshold", &threshold);
Shell_Printf("DRC threshold: %ld dB\n", (long)threshold);
```

#### `int Effect_SetDRCParam(EffectId_t id, const char *param_name, int32_t value)`
设置DRC参数值。

```c
Effect_SetDRCParam(EFFECT_ID_DRC, "threshold", -25);
Effect_SetDRCParam(EFFECT_ID_DRC, "ratio", 6);
```

#### `int Effect_GetEQBandGain(EffectId_t id, uint8_t band_index, int8_t *gain)`
获取EQ某个频段的增益。

```c
int8_t gain;
Effect_GetEQBandGain(EFFECT_ID_EQ, 0, &gain);
Shell_Printf("Band 0 gain: %d dB\n", gain);
```

#### `int Effect_SetEQBandGain(EffectId_t id, uint8_t band_index, int8_t gain)`
设置EQ某个频段的增益。

```c
Effect_SetEQBandGain(EFFECT_ID_EQ, 0, 3);   // 低频增益3dB
Effect_SetEQBandGain(EFFECT_ID_EQ, 9, -2);  // 高频衰减2dB
```

#### `int Effect_PrintAllParams(EffectId_t id)`
打印指定效果器的所有参数。

```c
Effect_PrintAllParams(EFFECT_ID_DRC);
```

#### `int Effect_Reset(EffectId_t id)`
重置效果器为默认参数。

```c
Effect_Reset(EFFECT_ID_DRC);
```

## 集成步骤

### 1. 在头文件中包含

编辑 `bg_audio_io_manager.c`，在头文件区添加：

```c
#include "shell_cmd_effect.h"
```

### 2. 注册命令

在 `BG_audio_Init()` 函数中添加：

```c
void BG_audio_Init(uint16_t SampleRate)
{
    // ... 其他初始化代码 ...
    
    // 注册效果器Shell命令
    ShellCmdEffect_Register();
    
    // ... 其他初始化代码 ...
}
```

### 3. 编译项目

```bash
cd BanBox/Debug
make clean
make -j4
```

## 参数范围参考

| 效果器 | 参数 | 范围 | 单位 | 默认值 |
|--------|------|------|------|--------|
| DRC | threshold | -60 ~ 0 | dB | -20 |
| DRC | ratio | 1 ~ 20 | - | 4 |
| DRC | attack | 0 ~ 1000 | ms | 5 |
| DRC | release | 0 ~ 1000 | ms | 100 |
| Reverb | room | 0 ~ 100 | % | 50 |
| Reverb | damp | 0 ~ 100 | % | 50 |
| Reverb | wet | 0 ~ 100 | % | 30 |
| Expander | threshold | -100 ~ 0 | dB | -60 |
| Expander | ratio | 1 ~ 10 | - | 2 |
| EQ | band0-9 | -12 ~ +12 | dB | 0 |

## 故障排除

### 问题1：命令无法识别

**错误信息：** `Unknown module: effect`

**解决方案：**
- 确保已调用 `ShellCmdEffect_Register()`
- 确保 `SHELL_MODULE_MAX` 足够大（至少20）
- 重新编译项目

### 问题2：参数设置无效果

**原因：** 可能效果器处于禁用状态

**解决方案：**
```bash
effect enable <id> on
effect set <id> <param> <value>
```

### 问题3：参数读取返回0

**原因：** 某些参数不支持读取操作

**解决方案：**
使用 `effect info <id>` 查看支持的参数，仅读取可支持的参数。

## 扩展说明

### 添加新的效果器

1. 在 `shell_cmd_effect.h` 中添加新的ID：
```c
typedef enum {
    // ... 现有ID ...
    EFFECT_ID_NEW_EFFECT,
    EFFECT_ID_MAX
} EffectId_t;
```

2. 在 `shell_cmd_effect.c` 中的 `g_EffectInfoTable` 中添加条目：
```c
{ EFFECT_ID_NEW_EFFECT, "new_effect", "新效果器描述" },
```

3. 在对应的函数中添加case分支处理该效果器。

## 参考文档

- [bg_audio_io_manager.c](./bg_audio_io_manager.c) - 音频管理器
- [ctrlvars.h](../audio/ctrlvars.h) - 控制变量定义
- [audio_effect.h](../audio/audio_effect.h) - 音频效果器定义
