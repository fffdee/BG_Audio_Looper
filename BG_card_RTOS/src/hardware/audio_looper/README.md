# Audio Looper 模块使用说明

## 概述

这个Audio Looper模块将原来分散在main.c中的loop相关功能封装成了简单易用的函数，提供了清晰的接口来管理音频循环录制和播放功能。

## 文件结构

```
BG_card_RTOS/src/hardware/audio_looper/
├── audio_looper.h      # 头文件
├── audio_looper.c      # 实现文件
└── README.md          # 使用说明
```

## 主要功能

### 1. 状态管理
- `LOOP_STATE_IDLE`: 空闲状态
- `LOOP_STATE_RECORDING`: 录制状态  
- `LOOP_STATE_PLAYING`: 播放状态
- `LOOP_STATE_OVERDUB`: 叠录状态

### 2. 核心函数

#### 初始化和控制
```c
void loop_init(void);                    // 初始化Loop管理器
void loop_reset(void);                   // 重置Loop管理器
void loop_handle_button_press(void);     // 处理按键按下(智能状态切换)
```

#### 数据处理
```c
void loop_process_recording(int16_t* audio_data, uint8_t* buffer, uint16_t length);
void loop_process_playback(int16_t* output_data, uint8_t* buffer, uint16_t length);
```

#### 状态查询
```c
LoopState_t loop_get_state(void);        // 获取当前状态
bool loop_is_recording(void);            // 是否正在录制
bool loop_is_playing(void);              // 是否正在播放
uint32_t loop_get_current_address(void); // 获取当前地址
uint32_t loop_get_record_time(void);     // 获取录制时间
```

## 使用示例

### 在main.c中的修改

1. **包含头文件**
```c
#include "hardware/audio_looper/audio_looper.h"
```

2. **初始化**
```c
void EffectTask(){
    // ... 其他初始化代码 ...
    
    BG_flash_manager.Init();
    BG_flash_manager.EraseAll(DEV_NOR);
    
    // 初始化loop管理器
    loop_init();
    
    DBG("Loop manager is ready\n");
    
    // ... 其他代码 ...
}
```

3. **按键处理**
```c
if(!BG_encoder.enter()){
    DelayMs(100);
    if(!BG_encoder.enter()){
        // 使用loop函数处理按键
        loop_handle_button_press();
    }
}
```

4. **音频处理**
```c
// 在音频处理循环中
if(AudioADC_DataLenGet(ADC0_MODULE) >= 48) {
    // ... 获取音频数据 ...
    
    // 使用loop函数处理录制和播放
    loop_process_recording(PcmBuf3, Buffer, 48);
    loop_process_playback(PcmBuf3, Buffer, 48);
}
```

## 智能状态切换逻辑

`loop_handle_button_press()`函数实现了智能状态切换：

1. **第一次按下**: 从空闲状态进入录制状态
2. **录制时按下**: 结束录制，进入播放状态  
3. **播放时按下**: 进入叠录状态或循环播放
4. **叠录时按下**: 继续播放或停止

## 与原代码的对应关系

| 原代码变量/逻辑 | 新模块功能 |
|----------------|-----------|
| `play_flag` | `g_loop_manager.state` |
| `loop_play` | `g_loop_manager.loop_play` |
| `loop` | `g_loop_manager.loop` |
| 按键处理逻辑 | `loop_handle_button_press()` |
| 录制逻辑 | `loop_process_recording()` |
| 播放逻辑 | `loop_process_playback()` |

## 优势

1. **代码整理**: 将分散的loop逻辑集中管理
2. **函数封装**: 每个功能都有独立的函数
3. **状态清晰**: 明确的状态定义和管理
4. **易于调试**: 独立的功能模块便于问题定位
5. **便于扩展**: 可以轻松添加新功能

## 注意事项

1. 在使用任何功能前，必须先调用`loop_init()`初始化
2. 模块会同步全局变量，保持与原代码的兼容性
3. Flash读写操作需要确保BG_flash_manager已正确初始化
4. 数据处理函数需要配合定时器中断使用

这个简化的封装保持了原有功能的完整性，同时提供了更清晰的代码结构。
