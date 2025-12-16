# 灵活录音分配机制 (Flexible Recording Allocation)

## 概述

灵活录音分配机制允许按钮和录音之间的动态绑定，而不是固定的一对一绑定关系。这样可以实现更灵活的录音管理。

## 特性

- **动态分配**：开机时所有录音都未分配ID，按钮按下时动态分配
- **灵活绑定**：任意按钮可以绑定到任意录音
- **顺序存储**：录音在Flash中按创建顺序存储，不依赖按钮编号
- **独立控制**：每个录音都有独立的状态控制（录制/播放/停止）

## 工作原理

### 1. 初始状态
- 所有按钮：未绑定录音（is_assigned = 0）
- 录音计数：0
- 下一个录音ID：1

### 2. 首次按下按钮
假设按下第3个按钮（索引2）：
- 分配录音ID = 1
- 绑定：按钮2 -> 录音ID 1
- Flash存储：索引0位置（第一个录音）
- 状态：开始录制

### 3. 按下其他按钮
假设按下第1个按钮（索引0）：
- 分配录音ID = 2
- 绑定：按钮0 -> 录音ID 2
- Flash存储：索引1位置（第二个录音）
- 状态：开始录制

### 4. 绑定映射关系
```
按钮索引 -> 录音ID -> Flash索引
按钮2   -> 录音1   -> Flash0
按钮0   -> 录音2   -> Flash1
按钮1   -> 录音3   -> Flash2
按钮3   -> 录音4   -> Flash3
```

## API接口

### 核心函数
```c
// 处理灵活按钮按下（核心函数）
uint8_t AudioLooper.FlexibleButtonPress(uint8_t button_index);

// 手动分配录音到按钮
uint8_t AudioLooper.AssignRecordingToButton(uint8_t button_index);

// 查询按钮绑定的录音ID
uint8_t AudioLooper.GetButtonRecordingId(uint8_t button_index);

// 清除按钮绑定
void AudioLooper.ClearButtonBinding(uint8_t button_index);

// 清除所有绑定
void AudioLooper.ClearAllBindings(void);
```

### 辅助函数
```c
// 获取录音在Flash中的实际索引
uint8_t loop_get_recording_flash_index(uint8_t recording_id);

// 获取录音在Flash中的地址
uint32_t loop_get_recording_flash_address(uint8_t recording_id);

// 获取录音状态
SegmentState_t loop_get_recording_state(uint8_t recording_id);

// 获取完整绑定信息
uint8_t loop_get_flexible_binding_info(uint8_t button_index, 
                                        uint8_t* recording_id_out, 
                                        uint8_t* flash_index_out, 
                                        uint32_t* flash_address_out);
```

## 使用示例

### 基本用法
```c
// 初始化
AudioLooper.Init();

// 按下按钮3（索引2）- 首次按下，开始录制
uint8_t recording_id = AudioLooper.FlexibleButtonPress(2);
// 返回录音ID = 1，在Flash索引0处存储

// 再次按下按钮3 - 停止录制，开始播放
AudioLooper.FlexibleButtonPress(2);

// 按下按钮1（索引0）- 首次按下，开始录制第二段
recording_id = AudioLooper.FlexibleButtonPress(0);
// 返回录音ID = 2，在Flash索引1处存储
```

### 状态查询
```c
// 查询按钮绑定状态
for(uint8_t i = 0; i < 4; i++) {
    uint8_t recording_id = AudioLooper.GetButtonRecordingId(i);
    if(recording_id > 0) {
        printf("Button %d -> Recording %d\n", i, recording_id);
    } else {
        printf("Button %d -> Not assigned\n", i);
    }
}

// 获取详细信息
uint8_t recording_id, flash_index;
uint32_t flash_address;
if(loop_get_flexible_binding_info(2, &recording_id, &flash_index, &flash_address)) {
    printf("Button 2: Recording %d, Flash Index %d, Address 0x%08X\n", 
           recording_id, flash_index, flash_address);
}
```

### 清除绑定
```c
// 清除单个按钮绑定
AudioLooper.ClearButtonBinding(2);

// 清除所有绑定（复位）
AudioLooper.ClearAllBindings();
```

## 状态转换

每个录音有以下状态：
- `SEGMENT_INACTIVE`: 未激活
- `SEGMENT_RECORDING`: 正在录制
- `SEGMENT_PLAYING`: 正在播放
- `SEGMENT_STOPPED`: 已停止（可重新播放）

状态转换规则：
```
未绑定按钮 --按下--> 分配录音ID + 开始录制
录制中     --按下--> 停止录制 + 开始播放
播放中     --按下--> 停止播放
已停止     --按下--> 开始播放
```

## 数据结构

### 按钮绑定结构
```c
typedef struct {
    uint8_t recording_id;          // 分配的录音ID (0表示未分配)
    uint8_t is_assigned;           // 是否已分配录音
} ButtonBinding_t;
```

### Loop管理器扩展
```c
typedef struct {
    // ...现有字段...
    
    // 灵活录音分配机制
    ButtonBinding_t button_bindings[MAX_SEGMENTS];  // 按钮绑定数组
    uint8_t next_recording_id;      // 下一个可分配的录音ID
    uint8_t recording_count;        // 已创建的录音数量
} LoopManager_t;
```

## 优势

1. **灵活性**：按钮和录音解耦，任意按钮可控制任意录音
2. **直观性**：按下哪个按钮就激活哪个录音，符合用户直觉
3. **存储效率**：录音按创建顺序存储，充分利用Flash空间
4. **扩展性**：易于扩展支持更多按钮或录音
5. **兼容性**：保持与现有代码的兼容性

## 调试支持

提供了演示函数用于测试和调试：
```c
// 运行演示
loop_flexible_allocation_demo();
```

该函数会模拟按钮按下序列并输出详细的分配信息，便于理解和调试。
