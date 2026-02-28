# BG Audio Processor - 音频处理流水线

## 概述

BG Audio Processor 采用**流水线架构**设计,允许动态注册和管理多个音频效果。每个效果都是流水线上的一个工位,支持独立的 bypass 控制。

## 核心概念

### 1. 流水线架构
```
输入音频 -> 效果1 -> 效果2 -> 效果3 -> ... -> 输出音频
```

- 效果按**注册顺序**依次处理
- 每个效果可独立 bypass(跳过处理,数据直通)
- 最多支持 8 个效果节点(`BG_AUDIO_MAX_EFFECTS`)

### 2. 效果节点
每个效果需要实现三个函数:
- **Init**: 初始化效果状态(可选)
- **Process**: 处理音频数据(必需)
- **Reset**: 重置效果状态(可选)

### 3. Bypass 机制
- **节点 bypass**: 单个效果跳过,数据直通
- **全局 bypass**: 所有效果跳过,仅应用主增益

## API 接口

### 初始化流水线
```c
BG_AudioProcessor.Init();
```

### 注册效果
```c
uint8_t effect_id = BG_AudioProcessor.RegisterEffect(
    "EffectName",           // 效果名称
    effect_init,            // 初始化函数 (可为 NULL)
    effect_process,         // 处理函数 (必需)
    effect_reset,           // 重置函数 (可为 NULL)
    &effect_data,           // 私有数据指针
    sizeof(effect_data)     // 数据大小
);

// 返回值: 效果ID (0xFF 表示失败)
```

### 处理函数签名
```c
uint16_t effect_process(const short *input, short *output, 
                        uint16_t samples, void *user_data)
{
    // 1. 处理音频数据
    for (uint16_t i = 0; i < samples; i++) {
        output[i] = process_sample(input[i], user_data);
    }
    
    // 2. 返回处理的样本数
    return samples;
}
```

### Bypass 控制
```c
// 设置效果 bypass
BG_AudioProcessor.SetEffectBypass(effect_id, 1);  // 1=bypass, 0=处理

// 获取 bypass 状态
uint8_t bypass = BG_AudioProcessor.GetEffectBypass(effect_id);
```

### 流水线处理
```c
short input[480];   // 输入缓冲区
short output[480];  // 输出缓冲区

uint16_t processed = BG_AudioProcessor.Process(input, output, 480);
```

### 查询信息
```c
// 获取已注册效果数量
uint8_t count = BG_AudioProcessor.GetEffectCount();

// 获取效果详细信息
const BG_AudioEffect_Node_t *info = BG_AudioProcessor.GetEffectInfo(effect_id);
printf("Effect: %s, Bypass: %s\n", info->name, info->bypass ? "ON" : "OFF");
```

## 使用示例

### 示例1: 使用内置 DRC 效果

```c
#include "bg_audio_processor.h"
#include "effects/bg_effect_drc.h"

void setup_drc_pipeline(void)
{
    // 1. 初始化
    BG_AudioProcessor.Init();
    
    // 2. 创建 DRC 实例
    BG_DRC_Effect_t *drc = bg_effect_drc_create(
        0.7f,   // threshold: 70%
        4.0f,   // ratio: 4:1
        1.0f,   // attack: 1ms
        50.0f   // release: 50ms
    );
    
    // 3. 注册到流水线
    uint8_t drc_id = BG_AudioProcessor.RegisterEffect(
        "DRC",
        bg_effect_drc_init,
        bg_effect_drc_process,
        bg_effect_drc_reset,
        drc,
        sizeof(BG_DRC_Effect_t)
    );
    
    // 4. 处理音频
    short input[480], output[480];
    BG_AudioProcessor.Process(input, output, 480);
}
```

### 示例2: 自定义简单增益效果

```c
typedef struct {
    float gain;
} MyGain_t;

static MyGain_t g_gain = { .gain = 0.5f };

uint16_t my_gain_process(const short *input, short *output, 
                         uint16_t samples, void *user_data)
{
    MyGain_t *gain = (MyGain_t *)user_data;
    
    for (uint16_t i = 0; i < samples; i++) {
        float processed = (float)input[i] * gain->gain;
        // 限幅
        if (processed > 32767.0f) processed = 32767.0f;
        if (processed < -32768.0f) processed = -32768.0f;
        output[i] = (short)processed;
    }
    
    return samples;
}

void register_custom_gain(void)
{
    uint8_t gain_id = BG_AudioProcessor.RegisterEffect(
        "MyGain",
        NULL,                   // 无需初始化
        my_gain_process,
        NULL,                   // 无需重置
        &g_gain,
        sizeof(MyGain_t)
    );
}
```

### 示例3: 多效果流水线

```c
void multi_effect_pipeline(void)
{
    BG_AudioProcessor.Init();
    
    // 创建并注册 EQ (使用流行音乐预设)
    BG_EQ_Effect_t *eq = bg_effect_eq_create();
    bg_effect_eq_preset_pop(eq);
    uint8_t eq_id = BG_AudioProcessor.RegisterEffect(
        "EQ", bg_effect_eq_init, bg_effect_eq_process, 
        bg_effect_eq_reset, eq, sizeof(BG_EQ_Effect_t));
    
    // 注册 DRC
    BG_DRC_Effect_t *drc = bg_effect_drc_create(0.7f, 4.0f, 1.0f, 50.0f);
    uint8_t drc_id = BG_AudioProcessor.RegisterEffect(
        "DRC", bg_effect_drc_init, bg_effect_drc_process, 
        bg_effect_drc_reset, drc, sizeof(BG_DRC_Effect_t));
    
    // 流水线顺序: EQ -> DRC
    
    // 处理音频
    short input[48], output[48];
    BG_AudioProcessor.Process(input, output, 48);
    
    // Bypass EQ,保留 DRC
    BG_AudioProcessor.SetEffectBypass(eq_id, 1);
    
    // 切换 EQ 预设
    BG_AudioProcessor.SetEffectBypass(eq_id, 0);
    bg_effect_eq_preset_rock(eq);
}
```

## 内置效果

### DRC (动态范围压缩)
- **文件**: `effects/bg_effect_drc.h` / `bg_effect_drc.c`
- **用途**: 防止复音叠加导致失真
- **参数**:
  - `threshold`: 压缩阈值 (0.0 ~ 1.0)
  - `ratio`: 压缩比 (1.0 ~ 20.0)
  - `attack_ms`: 起音时间 (ms)
  - `release_ms`: 释音时间 (ms)

### EQ (参数化均衡器)
- **文件**: `effects/bg_effect_eq.h` / `bg_effect_eq.c`
- **用途**: 多段参数化均衡调节音色
- **支持**: 最多 5 段独立 EQ
- **滤波器类型**:
  - Peak: 峰值滤波器 (中频调节)
  - Low Shelf: 低频搁架
  - High Shelf: 高频搁架
  - Low Pass: 低通滤波器
  - High Pass: 高通滤波器
- **参数** (每段):
  - `freq`: 中心频率 (Hz)
  - `gain`: 增益 (dB, -12 ~ +12)
  - `q`: 品质因数 (0.1 ~ 10.0)
- **预设**: Pop, Rock, Classical, Jazz

## 流水线特性

### 数据流转
- 使用**双缓冲**机制: `output` 和 `temp_buffer` 交替使用
- 避免不必要的内存拷贝
- 支持 in-place 处理

### Bypass 行为
- **效果 bypass**: 数据直通,不调用处理函数
- **全局 bypass**: 所有效果跳过,仅应用 `master_gain`
- Bypass 状态可动态切换

### 性能优化
- 效果节点按需分配
- 未启用的节点不参与处理
- 流水线长度可动态调整 (注册/注销)

## 配置选项

### 全局配置
```c
BG_AudioProcessor_Config_t config = {
    .master_gain = 1.0f,        // 主增益 (0.0 ~ 2.0)
    .global_bypass = 0          // 全局 bypass (0=关闭, 1=开启)
};

BG_AudioProcessor.SetConfig(&config);
```

## 编译选项

在 `bg_audio_processor.h` 中:
```c
#define BG_AUDIO_MAX_EFFECTS 8  // 最大效果数量,可根据需求调整
```

## 注意事项

1. **注册顺序决定处理顺序**: 先注册的效果先处理
2. **效果数据生命周期**: `user_data` 指针必须在效果生命周期内有效
3. **线程安全**: 当前实现不保证线程安全,需在单线程环境使用
4. **缓冲区大小**: 临时缓冲区大小为 `BG_BUFFER_SIZE`,确保足够
5. **处理函数返回值**: 必须返回实际处理的样本数

## 扩展开发

### 创建新效果的步骤

1. **定义效果数据结构**
```c
typedef struct {
    // 配置参数
    float param1;
    int param2;
    
    // 运行状态
    float state1;
    int state2;
} MyEffect_t;
```

2. **实现处理函数**
```c
uint16_t my_effect_process(const short *input, short *output, 
                           uint16_t samples, void *user_data)
{
    MyEffect_t *effect = (MyEffect_t *)user_data;
    
    // 处理逻辑
    for (uint16_t i = 0; i < samples; i++) {
        output[i] = process_logic(input[i], effect);
    }
    
    return samples;
}
```

3. **可选: 实现初始化和重置**
```c
void my_effect_init(void *user_data) { /* 初始化状态 */ }
void my_effect_reset(void *user_data) { /* 重置状态 */ }
```

4. **注册到流水线**
```c
static MyEffect_t g_my_effect = { .param1 = 1.0f, .param2 = 10 };

uint8_t effect_id = BG_AudioProcessor.RegisterEffect(
    "MyEffect", my_effect_init, my_effect_process, 
    my_effect_reset, &g_my_effect, sizeof(MyEffect_t));
```

## 示例代码

完整应用示例见: `components/BG_Midi_Controller/src/midi_controller.c` 中的 `MIDI_Init()` 函数

## 架构优势

✅ **可扩展**: 轻松添加新效果,无需修改核心代码  
✅ **灵活**: 动态注册/注销,运行时调整流水线  
✅ **高效**: 最小化内存拷贝,双缓冲优化  
✅ **易用**: 清晰的 API,简单的效果开发接口  
✅ **可控**: 独立 bypass 控制,便于 A/B 测试
