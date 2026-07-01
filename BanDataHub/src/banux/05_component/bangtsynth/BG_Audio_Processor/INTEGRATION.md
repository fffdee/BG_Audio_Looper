# 音频处理器快速集成指南

## 1. 编译配置

在 Linux 环境中重新配置 CMake:

```bash
cd build
cmake .. -DENABLE_AUDIO_PROCESSOR=ON
make clean
make
```

## 2. 集成到 MIDI Controller

### 2.1 在 `midi_controller.c` 头部添加

```c
#include "bg_audio_processor.h"
```

### 2.2 在 `MIDI_Init()` 中初始化处理器

```c
void MIDI_Init()
{
    // ... 原有初始化代码 ...
    
    /* 初始化音频处理器 */
    BG_AudioProcessor.Init();
    
    /* 配置包络模式 (可选,默认为bypass) */
    BG_AudioProcessor_Config_t config = {
        .bypass_enabled = 0,         // 启用处理
        .envelope_enabled = 1,       // 启用包络
        .master_gain = 0.8f
    };
    BG_AudioProcessor.SetConfig(&config);
    
    /* 设置包络参数 */
    BG_Envelope_Params_t envelope = {
        .attack_ms = 10,
        .decay_ms = 200,
        .sustain_level = 0.6f,
        .release_ms = 400
    };
    BG_AudioProcessor.SetEnvelope(&envelope);
}
```

### 2.3 修改 `MIDI_ProcessAudio()` 函数

将原来的力度应用替换为音频处理器调用:

```c
void MIDI_ProcessAudio(void)
{
    short data[MAX_POLYPHONY][49] = {0};
    short temp_data[48];
    short processed_data[48];  // 新增: 处理后的数据
    short play[48] = {0}; 
    uint8_t polyphony_count = 0;
    uint8_t play_flag = 0;
    
    /* 遍历所有 MIDI 通道 */
    for(uint8_t ch = 0; ch < 16; ch++)
    {
        if(BG_MIDI_data.BG_channel_info[ch].NoteOn_count > 0) {
            
            /* 遍历所有音符 */
            for(uint8_t note = 0; note < 128; note++) {
                uint8_t velocity = BG_MIDI_data.BG_channel_info[ch].Note_Map[note];
                
                if(velocity > 0) {
                    
                    /* 读取音频样本 */
                    if (BG_reader.Callback(temp_data, note, 48, 
                        BG_MIDI_data.BG_channel_info[ch].program_index))
                    {
                        /* === 使用音频处理器处理 === */
                        uint16_t processed_samples = BG_AudioProcessor.Process(
                            temp_data,           // 输入
                            processed_data,      // 输出
                            48,                  // 样本数
                            ch,                  // 通道
                            note,                // 音符
                            velocity,            // 力度
                            1                    // NoteOn
                        );
                        
                        if (processed_samples > 0) {
                            /* 将处理后的数据加入混音 */
                            for(uint8_t i = 0; i < processed_samples; i++) {
                                data[polyphony_count][i] = processed_data[i];
                            }
                            data[polyphony_count][48] = 1;
                            polyphony_count++;
                            play_flag = 1;
                        } else {
                            /* 包络结束,停止该音符 */
                            BG_MIDI_data.BG_channel_info[ch].Note_Map[note] = 0;
                            if(BG_MIDI_data.BG_channel_info[ch].NoteOn_count > 0)
                                BG_MIDI_data.BG_channel_info[ch].NoteOn_count--;
                        }
                    }
                    else {
                        /* 样本播放完成,停止该音符 */
                        BG_MIDI_data.BG_channel_info[ch].Note_Map[note] = 0;
                        if(BG_MIDI_data.BG_channel_info[ch].NoteOn_count > 0)
                            BG_MIDI_data.BG_channel_info[ch].NoteOn_count--;
                    }
                }
            }
        }
    }
    
    /* 混音所有活动的复音 */
    for(uint8_t channel = 0; channel < polyphony_count; channel++) {
        if(data[channel][48] > 0) {
            for(uint8_t i = 0; i < 48; i++)
                play[i] += data[channel][i];
        }
    }
    
    /* 输出音频数据 */
    if(play_flag)
        audioPlay.Callbaclk(play);
}
```

## 3. 处理 NoteOff 事件

在 `MIDI_Message_Handle()` 中处理 NoteOff 时,需要通知音频处理器:

```c
void MIDI_Message_Handle(uint8_t *data, uint8_t len)
{
    uint8_t channel = data[0] & 0x0F;
    uint8_t status = data[0] & 0xF0;
    
    switch(status) {
        case 0x80:  // NoteOff
        case 0x90:  // NoteOn (velocity=0 也是 NoteOff)
            if (data[2] == 0) {
                // NoteOff: 在下次 ProcessAudio 时会触发 Release
                BG_MIDI_data.BG_channel_info[channel].Note_Map[data[1]] = 0;
            }
            break;
        // ... 其他处理 ...
    }
}
```

## 4. 测试方案

### 4.1 Bypass模式测试
```c
// 在 main.c 初始化后测试
BG_AudioProcessor_Config_t config = {
    .bypass_enabled = 1,  // 启用bypass
    .master_gain = 1.0f
};
BG_AudioProcessor.SetConfig(&config);
// 此时应该和未集成前效果一样
```

### 4.2 包络模式测试
```c
// 切换到包络模式
BG_AudioProcessor_Config_t config = {
    .bypass_enabled = 0,
    .envelope_enabled = 1,
    .master_gain = 0.8f
};
BG_AudioProcessor.SetConfig(&config);

// 应该能听到明显的起音和释音效果
```

## 5. 常见问题

### Q1: 编译错误 "bg_audio_processor.h: No such file"
**A:** 确保在 CMakeLists.txt 中启用了 `ENABLE_AUDIO_PROCESSOR=ON`

### Q2: 声音有爆音或削波
**A:** 降低 `master_gain` 参数,例如改为 `0.5f`

### Q3: 包络没有效果
**A:** 检查:
1. `bypass_enabled` 是否为 0
2. `envelope_enabled` 是否为 1
3. NoteOff 事件是否正确处理

### Q4: 音符释放太慢/太快
**A:** 调整 `release_ms` 参数:
```c
BG_Envelope_Params_t envelope = {
    .release_ms = 100  // 调整这个值 (单位: 毫秒)
};
BG_AudioProcessor.SetEnvelope(&envelope);
```

## 6. 下一步扩展

完成基本集成后,可以:

1. **添加滤波器**: 在 `bg_audio_processor.c` 中实现滤波器逻辑
2. **添加效果器**: 实现混响、延迟等效果
3. **MIDI映射**: 将 MIDI CC 映射到包络参数
4. **预设管理**: 为不同乐器创建包络预设

## 7. 性能优化建议

- 默认使用 bypass 模式,按需启用包络
- 包络计算使用整数运算替代浮点 (针对嵌入式平台)
- 限制同时处理的复音数 (当前最大32)
