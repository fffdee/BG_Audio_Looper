# Effect Graph 集成指南

本文档说明如何将 Effect Graph 模块集成到 `bg_audio_io_manager.c` 主工程中。

## 集成步骤

### 1. 添加头文件

在 `bg_audio_io_manager.c` 顶部添加：

```c
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "shell_cmd_graph.h"
```

### 2. 在 BG_audio_Init() 中初始化 Effect Graph

在 `BG_audio_Init()` 函数的末尾，Shell IO 初始化之后添加：

```c
void BG_audio_Init(uint16_t SampleRate)
{
    // ... 现有初始化代码 ...
    
    // 初始化Shell IO管理器（自动管理CDC和BLE接口）
    ShellIOManager_Init();
    
    // ========== Effect Graph 初始化 ==========
    DBG("[Audio] Initializing Effect Graph...\n");
    
    // 1. 初始化 Effect Graph 核心模块
    if (EffectGraph_Init() != 0) {
        DBG("[Audio] ERROR: Effect Graph Init failed!\n");
        return;
    }
    
    // 2. 加载默认预设（可根据需求选择其他预设）
    if (EffectGraph_LoadPreset(PRESET_DEFAULT) != 0) {
        DBG("[Audio] ERROR: Effect Graph Load Preset failed!\n");
        return;
    }
    
    // 3. 注册 Shell 命令（支持 CDC/BLE 远程控制）
    ShellCmd_RegisterGraphCommands();
    
    DBG("[Audio] Effect Graph initialized successfully\n");
    // ==========================================
    
    // ... 其他初始化代码 ...
}
```

### 3. 在 Audio_loop() 中处理 Effect Graph

在 `Audio_loop()` 函数中，音频处理循环中添加 Effect Graph 处理调用。

根据你的音频处理架构，可能需要在以下位置调用：

```c
void Audio_loop(void)
{
    // ... 现有代码 ...
    
    // 在音频数据处理部分调用 Effect Graph
    // 注意：这里需要根据实际的音频数据流调整
    
    if (GetA2dpState() == BT_A2DP_STATE_STREAMING) {
        if (mv_msize(&SBC_MemHandle) > SBC_DECODER_FIFO_MIN)
            AudioLoopWithBT(bt_audio_buffer);
    } else {
        AudioLoopMinimal(bt_audio_buffer);
    }
    
    // 可选：在这里触发 Effect Graph 处理
    // 注意：实际音频处理应该在音频回调中进行
    // EffectGraph_Process();  // 如果需要周期性处理
    
    // ... 其他代码 ...
}
```

### 4. 挂接实际音频设备回调

需要将实际的音频设备 Source/Sink 回调挂接到 Effect Graph 节点。这通常在初始化完成后进行：

#### 示例：挂接 ADC 输入回调

```c
// 在 BG_audio_Init() 末尾或适当位置
void SetupAudioCallbacks(void)
{
    EffectGraphNode* guitar_node = EffectGraph_FindNode("guitar_in");
    if (guitar_node) {
        // 挂接实际的 ADC0 数据读取回调
        guitar_node->source_callback = ADC0_ReadGuitarData;  // 你需要实现这个函数
        guitar_node->user_data = NULL;  // 可选的用户数据
    }
    
    EffectGraphNode* mic_node = EffectGraph_FindNode("mic_in");
    if (mic_node) {
        // 挂接实际的 ADC1 麦克风数据读取回调
        mic_node->source_callback = ADC1_ReadMicData;  // 你需要实现这个函数
        mic_node->user_data = NULL;
    }
    
    EffectGraphNode* dac_node = EffectGraph_FindNode("dac_out");
    if (dac_node) {
        // 挂接实际的 DAC 数据写入回调
        dac_node->sink_callback = DAC0_WriteSpeakerData;  // 你需要实现这个函数
        dac_node->user_data = NULL;
    }
    
    // 挂接其他节点...
}
```

#### 回调函数示例实现

```c
// Guitar ADC Source 回调示例
static int32_t ADC0_ReadGuitarData(void* user_data, int16_t* buffer, uint32_t frame_count)
{
    // 从 ADC0 读取吉他输入数据
    // 这里需要根据你的 AudioADC API 实现
    uint32_t samples_read = AudioADC_Read(ADC0_MODULE, (uint32_t*)buffer, frame_count * 2);
    return (int32_t)(samples_read / 2);  // 返回实际读取的帧数
}

// Mic ADC Source 回调示例
static int32_t ADC1_ReadMicData(void* user_data, int16_t* buffer, uint32_t frame_count)
{
    // 从 ADC1 读取麦克风数据
    uint32_t samples_read = AudioADC_Read(ADC1_MODULE, (uint32_t*)buffer, frame_count * 2);
    return (int32_t)(samples_read / 2);
}

// DAC Sink 回调示例
static int32_t DAC0_WriteSpeakerData(void* user_data, const int16_t* buffer, uint32_t frame_count)
{
    // 写入数据到 DAC0 输出
    uint32_t samples_written = AudioDAC_Write(DAC0, (uint32_t*)buffer, frame_count * 2);
    return (int32_t)(samples_written / 2);
}
```

### 5. 测试 Shell 命令

初始化完成后，可以通过 CDC 或 BLE 串口测试以下命令：

```bash
# 列出所有预设
graph preset list

# 切换到蓝牙预设
graph preset load bluetooth

# 查看图拓扑结构
graph list

# 查看节点详情
graph node guitar_in

# Bypass 某个节点
graph bypass compressor 1

# 读取参数
graph param compressor threshold

# 修改参数
graph param compressor threshold -20.0

# 重建图（重新拓扑排序）
graph rebuild
```

### 6. 可选：持久化参数到 Flash

如果需要将参数保存到 Flash，可以添加以下功能：

```c
// 保存当前预设到 Flash
int SaveCurrentPresetToFlash(void)
{
    // 使用你的 Flash API 保存参数
    // 例如：BG_flash_manager.Write(...)
    return 0;
}

// 从 Flash 加载预设
int LoadPresetFromFlash(void)
{
    // 从 Flash 读取并加载预设
    return 0;
}
```

## 集成清单

- [ ] 添加头文件引用
- [ ] 在 BG_audio_Init() 中初始化 Effect Graph
- [ ] 注册 Shell 命令
- [ ] 挂接 ADC Source 回调
- [ ] 挂接 DAC Sink 回调
- [ ] 挂接 USB Audio 回调
- [ ] 挂接 BT Audio 回调
- [ ] 测试 CDC Shell 命令
- [ ] 测试 BLE Shell 命令
- [ ] 实现参数持久化（可选）
- [ ] 添加更多预设配置（可选）

## 注意事项

1. **线程安全**：如果 Effect Graph 在不同线程中访问，需要添加互斥锁保护
2. **性能优化**：音频处理在实时任务中，确保回调函数执行时间足够短
3. **内存管理**：确保音频缓冲区大小合适，避免溢出
4. **采样率匹配**：确保所有节点的采样率一致
5. **错误处理**：生产环境中需要更完善的错误处理和恢复机制

## 下一步

- 实现更多预设（Guitar Only, Mic Only, USB Audio）
- 添加更多效果器节点（Chorus, Flanger, Phaser 等）
- 实现参数动画（渐变）
- 添加参数限制和验证
- 实现 A/B 预设对比功能
