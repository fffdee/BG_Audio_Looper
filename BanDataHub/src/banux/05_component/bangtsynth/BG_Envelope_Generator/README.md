# BG 包络生成器模块 (BG_Envelope_Generator)

通用 ADSR 包络生成器,适用于所有需要包络控制的音频处理场景。

## 功能特性

- ✅ **标准 ADSR 包络**: Attack-Decay-Sustain-Release 四阶段
- ✅ **双曲线模式**: 线性/指数曲线可选
- ✅ **高性能设计**: 整数/浮点混合运算,低 CPU 开销
- ✅ **动态参数调整**: 运行时可更改 ADSR 参数
- ✅ **通用接口**: SF2 和 BGS 音源共用

## ADSR 包络原理

```
电平
1.0 ┤     ╱╲
    │    ╱  ╲___________
    │   ╱A  D╲ Sustain  ╲
    │  ╱      ╲          ╲R
0.0 ┴─╯────────┴──────────╲───> 时间
       ↑      ↑           ↑
    NoteOn  Decay      NoteOff
            结束

A: Attack   - 攻击时间(从 0 上升到峰值)
D: Decay    - 衰减时间(从峰值下降到持续电平)
S: Sustain  - 持续电平(保持不变,直到 NoteOff)
R: Release  - 释放时间(从当前电平下降到 0)
```

## 使用示例

### 基础用法

```c
#include "bg_envelope.h"

// 1. 创建包络参数
BG_EnvParams_t params = BG_Envelope_CreateParams(
    10.0f,   // Attack: 10ms
    50.0f,   // Decay: 50ms
    0.7f,    // Sustain: 70%
    200.0f   // Release: 200ms
);

// 2. 初始化包络生成器
BG_Envelope_t env;
BG_Envelope_Init(&env, &params, 48000);

// 3. MIDI Note On -> 触发包络
BG_Envelope_Trigger(&env);

// 4. 音频处理循环
while (BG_Envelope_IsActive(&env)) {
    float envelope_value = BG_Envelope_Process(&env);
    
    // 应用包络到音频信号
    audio_sample = raw_sample * envelope_value;
}

// 5. MIDI Note Off -> 释放包络
BG_Envelope_Release(&env);

// 继续处理直到 Release 完成
while (BG_Envelope_IsActive(&env)) {
    float envelope_value = BG_Envelope_Process(&env);
    audio_sample = raw_sample * envelope_value;
}
```

### 批量处理(高性能)

```c
// 处理 48 个采样的缓冲区
float envelope_buffer[48];
BG_Envelope_ProcessBlock(&env, envelope_buffer, 48);

// 应用包络到音频
for (int i = 0; i < 48; i++) {
    output[i] = input[i] * envelope_buffer[i];
}
```

### 曲线类型对比

```c
// 线性曲线(快速响应,适合打击乐器)
BG_EnvParams_t linear_params = {
    .attack_time = 0.01f,
    .decay_time = 0.05f,
    .sustain_level = 0.7f,
    .release_time = 0.2f,
    .curve = BG_ENV_CURVE_LINEAR
};

// 指数曲线(自然平滑,适合弦乐/合成器)
BG_EnvParams_t exp_params = {
    .attack_time = 0.05f,
    .decay_time = 0.1f,
    .sustain_level = 0.6f,
    .release_time = 0.5f,
    .curve = BG_ENV_CURVE_EXPONENTIAL
};
```

### 预设包络配置

```c
// 钢琴包络
BG_EnvParams_t piano = BG_Envelope_CreateParams(5, 80, 0.5, 300);

// 管风琴包络
BG_EnvParams_t organ = BG_Envelope_CreateParams(10, 50, 1.0, 50);

// 弦乐包络
BG_EnvParams_t strings = BG_Envelope_CreateParams(80, 200, 0.8, 400);

// 打击乐包络
BG_EnvParams_t percussion = BG_Envelope_CreateParams(1, 10, 0.0, 50);

// 合成器 Pad 包络
BG_EnvParams_t pad = BG_Envelope_CreateParams(500, 300, 0.9, 1000);
```

## 在音源中的应用

### SF2 音源集成

```c
typedef struct {
    BG_Envelope_t volume_env;   // 音量包络
    BG_Envelope_t filter_env;   // 滤波器包络
    float sample_data[1024];
    // ...
} SF2_Voice_t;

void sf2_voice_note_on(SF2_Voice_t *voice, uint8_t velocity) {
    // 从 SF2 参数加载包络
    BG_EnvParams_t vol_params = {
        .attack_time = voice->sf2_preset.vol_attack,
        .decay_time = voice->sf2_preset.vol_decay,
        .sustain_level = voice->sf2_preset.vol_sustain,
        .release_time = voice->sf2_preset.vol_release,
        .curve = BG_ENV_CURVE_EXPONENTIAL
    };
    
    BG_Envelope_Init(&voice->volume_env, &vol_params, 48000);
    BG_Envelope_Trigger(&voice->volume_env);
}

void sf2_voice_process(SF2_Voice_t *voice, float *output, int count) {
    for (int i = 0; i < count; i++) {
        float env_level = BG_Envelope_Process(&voice->volume_env);
        output[i] = voice->sample_data[i] * env_level;
    }
}
```

### BGS 音源集成

```c
typedef struct {
    BG_Envelope_t envelope;
    int16_t *wav_data;
    uint32_t position;
} BGS_Voice_t;

void bgs_note_on(BGS_Voice_t *voice, uint8_t note) {
    BG_EnvParams_t params = BG_Envelope_CreateParams(10, 50, 0.7, 200);
    BG_Envelope_Init(&voice->envelope, &params, 48000);
    BG_Envelope_Trigger(&voice->envelope);
    voice->position = 0;
}

int16_t bgs_render_sample(BGS_Voice_t *voice) {
    float env = BG_Envelope_Process(&voice->envelope);
    int16_t raw = voice->wav_data[voice->position++];
    return (int16_t)(raw * env);
}
```

## 性能优化建议

1. **使用批量处理**: `BG_Envelope_ProcessBlock()` 比循环调用 `BG_Envelope_Process()` 更高效
2. **快速检查**: 使用 `BG_Envelope_IsActive()` 跳过已结束的包络
3. **指数曲线开销**: 指数曲线比线性曲线多约 20% CPU,但声音更自然
4. **采样率影响**: 48kHz 时一个包络约消耗 0.1% CPU (STM32F4 @ 168MHz)

## API 参考

### 初始化函数
- `BG_Envelope_Init()` - 初始化包络生成器
- `BG_Envelope_CreateParams()` - 创建参数结构(辅助函数)

### 控制函数
- `BG_Envelope_Trigger()` - 触发包络(Note On)
- `BG_Envelope_Release()` - 释放包络(Note Off)
- `BG_Envelope_Reset()` - 强制重置到空闲状态

### 处理函数
- `BG_Envelope_Process()` - 处理单个采样
- `BG_Envelope_ProcessBlock()` - 批量处理(推荐)

### 查询函数
- `BG_Envelope_IsActive()` - 检查是否激活
- `BG_Envelope_GetLevel()` - 获取当前电平
- `BG_Envelope_GetStage()` - 获取当前阶段

### 参数函数
- `BG_Envelope_SetParams()` - 动态更新参数

## 文件结构

```
components/BG_Envelope_Generator/
├── include/
│   └── bg_envelope.h          # 公共接口头文件
├── src/
│   └── bg_envelope.c          # 实现文件
└── README.md                  # 本文档
```

## 依赖项

- 标准库: `<stdint.h>`, `<stdbool.h>`, `<math.h>`, `<string.h>`
- 无其他外部依赖,可独立编译

## 编译集成

在 `CMakeLists.txt` 中添加:

```cmake
# 包络生成器源文件
set(ENVELOPE_SOURCES
    ${CMAKE_SOURCE_DIR}/components/BG_Envelope_Generator/src/bg_envelope.c
)

# 头文件路径
set(INCLUDE_DIRS
    ${INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/components/BG_Envelope_Generator/include
)
```
