# 音频循环器模块化API文档

## 概述

这个模块化API提供了对4段独立录音的精细控制，每个段可以：
- 独立录制和停止
- 独立播放和停止  
- 音量控制（0-100%）
- 静音控制
- 状态查询

支持两种播放模式：
1. **混音播放**：同时播放多个段并混音输出
2. **单段播放**：只播放指定的一个段

## API接口分类

### 1. 基础初始化和控制接口

#### `void loop_init(void)`
- **功能**：初始化音频循环器系统
- **说明**：必须在使用其他API前调用

#### `void loop_reset(void)`  
- **功能**：重置所有段状态
- **说明**：清除所有录音数据，恢复初始状态

#### `void loop_handle_button_press(void)`
- **功能**：原有的单按钮控制接口
- **说明**：保留兼容性，实现4段循环录制

### 2. 模块化段控制接口

#### `uint8_t loop_start_recording_segment(uint8_t segment_id)`
- **功能**：开始录制指定段
- **参数**：
  - `segment_id`: 段ID (0-3)
- **返回值**：1=成功, 0=失败
- **使用场景**：按钮按下开始录音

#### `uint8_t loop_stop_recording_segment(uint8_t segment_id)`
- **功能**：停止录制指定段
- **参数**：
  - `segment_id`: 段ID (0-3)  
- **返回值**：1=成功, 0=失败
- **使用场景**：按钮再次按下停止录音

#### `uint8_t loop_play_segment(uint8_t segment_id)`
- **功能**：播放指定段（单段播放模式）
- **参数**：
  - `segment_id`: 段ID (0-3)
- **返回值**：1=成功, 0=失败（段为空）
- **使用场景**：独奏播放某个段

#### `uint8_t loop_stop_segment(uint8_t segment_id)`
- **功能**：停止播放指定段
- **参数**：
  - `segment_id`: 段ID (0-3)
- **返回值**：1=成功, 0=失败
- **使用场景**：停止特定段的播放

#### `uint8_t loop_clear_segment(uint8_t segment_id)`
- **功能**：删除指定段的录音数据
- **参数**：
  - `segment_id`: 段ID (0-3)
- **返回值**：1=成功, 0=失败
- **使用场景**：清除不需要的录音

#### `uint8_t loop_play_all_segments(void)`
- **功能**：混音播放所有活跃段
- **返回值**：1=成功, 0=失败（无活跃段）
- **使用场景**：播放完整的多段录音作品

#### `void loop_stop_all_playback(void)`
- **功能**：停止所有播放
- **使用场景**：暂停播放

### 3. 音量和静音控制

#### `uint8_t loop_set_segment_volume(uint8_t segment_id, uint8_t volume)`
- **功能**：设置段的音量
- **参数**：
  - `segment_id`: 段ID (0-3)
  - `volume`: 音量 (0-100)
- **返回值**：1=成功, 0=失败
- **使用场景**：旋钮或滑杆控制音量

#### `uint8_t loop_mute_segment(uint8_t segment_id, uint8_t mute)`
- **功能**：静音/取消静音指定段
- **参数**：
  - `segment_id`: 段ID (0-3)
  - `mute`: 1=静音, 0=取消静音
- **返回值**：1=成功, 0=失败
- **使用场景**：静音按钮控制

### 4. 段状态查询接口

#### `uint8_t loop_is_segment_recording(uint8_t segment_id)`
- **功能**：查询段是否正在录制
- **返回值**：1=正在录制, 0=未录制
- **使用场景**：LED指示灯控制

#### `uint8_t loop_is_segment_playing(uint8_t segment_id)`
- **功能**：查询段是否正在播放
- **返回值**：1=正在播放, 0=未播放
- **使用场景**：播放状态显示

#### `uint8_t loop_segment_has_data(uint8_t segment_id)`
- **功能**：查询段是否有录音数据
- **返回值**：1=有数据, 0=无数据
- **使用场景**：判断按钮功能（录制 vs 播放）

#### `float loop_get_segment_duration(uint8_t segment_id)`
- **功能**：获取段的录音长度
- **返回值**：录音长度（秒），0=无录音
- **使用场景**：显示录音时长

#### `uint8_t loop_get_segment_volume(uint8_t segment_id)`
- **功能**：获取段的音量设置
- **返回值**：音量 (0-100)，255=段ID无效
- **使用场景**：音量显示同步

#### `uint8_t loop_is_segment_muted(uint8_t segment_id)`
- **功能**：查询段是否被静音
- **返回值**：1=静音, 0=未静音
- **使用场景**：静音状态指示

### 5. 系统状态查询接口

#### `uint8_t loop_get_active_segments_count(void)`
- **功能**：获取有录音数据的段数量
- **返回值**：活跃段数量 (0-4)
- **使用场景**：系统状态显示

#### `uint8_t loop_get_current_recording_segment(void)`
- **功能**：获取当前正在录制的段ID
- **返回值**：段ID (0-3)，255=无段在录制
- **使用场景**：录制状态指示

## 使用模式示例

### 模式1：4个独立按钮控制

```c
// 每个按钮控制一个段
void handle_button_press(uint8_t button_id) {
    if (loop_is_segment_recording(button_id)) {
        // 正在录制 -> 停止录制
        loop_stop_recording_segment(button_id);
    } else if (loop_segment_has_data(button_id)) {
        // 有数据 -> 播放这个段
        loop_play_segment(button_id);
    } else {
        // 无数据 -> 开始录制
        loop_start_recording_segment(button_id);
    }
}
```

### 模式2：录制+播放分离

```c
// 录制按钮组
void handle_record_button(uint8_t segment_id) {
    if (loop_is_segment_recording(segment_id)) {
        loop_stop_recording_segment(segment_id);
    } else {
        loop_start_recording_segment(segment_id);
    }
}

// 播放按钮组  
void handle_play_button(uint8_t segment_id) {
    if (loop_is_segment_playing(segment_id)) {
        loop_stop_segment(segment_id);
    } else {
        loop_play_segment(segment_id);
    }
}

// 混音播放按钮
void handle_mix_button(void) {
    if (loop_is_playing()) {
        loop_stop_all_playback();
    } else {
        loop_play_all_segments();
    }
}
```

### 模式3：音量和静音控制

```c
// 音量控制（旋钮/滑杆）
void update_volume_controls(void) {
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t volume = read_volume_control(i);  // 0-100
        loop_set_segment_volume(i, volume);
    }
}

// 静音按钮
void handle_mute_button(uint8_t segment_id) {
    uint8_t muted = loop_is_segment_muted(segment_id);
    loop_mute_segment(segment_id, !muted);
}
```

## 播放模式说明

### 混音播放模式（playback_mode = 0）
- 同时播放所有活跃且未静音的段
- 自动混音多个段的音频信号
- 音量控制独立作用于每个段

### 单段播放模式（playback_mode = 1）  
- 只播放指定的一个段
- 其他段被忽略
- 适合独奏和预览

## 音频处理流程

1. **录制**：`loop_process_recording_uint32()` 处理输入音频
2. **播放**：`loop_process_playback_uint32()` 处理输出音频
3. **混音**：根据播放模式和音量设置进行信号混合
4. **定时更新**：`loop_timer_update()` 维护系统状态

## 注意事项

1. **初始化顺序**：必须先调用 `loop_init()`
2. **段ID范围**：所有段ID必须在0-3范围内
3. **音量范围**：音量值0-100，超出会被限制
4. **并发控制**：同时只能录制一个段
5. **Flash容量**：4个段平均分配Flash存储空间
6. **实时性**：录制和播放函数需要在音频中断中调用

## 错误处理

- 无效的段ID会返回失败
- 对空段的播放操作会返回失败  
- 重复的录制操作会返回失败
- 查询函数对无效段ID返回默认值
