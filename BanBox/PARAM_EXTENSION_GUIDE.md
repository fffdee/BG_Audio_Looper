# 参数保存系统扩展 - 使用说明

## 修复问题

### 1. UI弹窗CDC断开问题
**问题**: 使用 `ui -p` 显示弹窗时CDC串口会断开  
**原因**: Shell_Printf 在弹窗显示期间输出导致通信冲突  
**解决**: 修改 `ui_show_popup`，在显示弹窗前输出确认信息，避免冲突

## 新增功能

### 音频参数独立控制

#### 吉他音量
```bash
audio -g          # 查看吉他音量
audio -g 80       # 设置吉他音量为80
audio -S          # 保存音频参数
```

#### 麦克风音量
```bash
audio -i          # 查看麦克风音量
audio -i 75       # 设置麦克风音量为75
audio -S          # 保存音频参数
```

#### 输出音量
```bash
audio -o          # 查看输出音量
audio -o 85       # 设置输出音量为85
audio -S          # 保存音频参数
```

### Looper参数保存

#### BPM（节拍）
```bash
looper -M         # 查看节拍器状态
looper -M bpm 120 # 设置BPM为120（自动保存到参数）
looper -S         # 保存looper参数
```

#### 拍号
```bash
looper -M beats 4 # 设置4/4拍（自动保存到参数）
looper -S         # 保存looper参数
```

#### 循环模式
```bash
looper -m         # 查看当前模式
looper -m song    # 设置为song模式（自动保存到参数）
looper -m free    # 设置为free模式（自动保存到参数）
looper -S         # 保存looper参数
```

### 蓝牙参数保存

#### 蓝牙名称
```bash
bt -n             # 查看蓝牙名称
bt -n "MyGuitar"  # 设置蓝牙名称（重启后生效）
bt -S             # 保存蓝牙参数
```

#### A2DP音量
```bash
bt -v             # 查看A2DP音量
bt -v 80          # 设置A2DP音量为80
bt -S             # 保存蓝牙参数
```

### LCD参数保存

#### 对比度/亮度
```bash
lcd -b            # 查看对比度
lcd -b 50         # 设置对比度为50
lcd -S            # 保存LCD参数
```

#### 背景颜色
```bash
lcd -c            # 查看背景颜色
lcd -c 0x001F     # 设置背景为蓝色（RGB565格式）
lcd -c 0xF800     # 设置背景为红色
lcd -c 0x07E0     # 设置背景为绿色
lcd -S            # 保存LCD参数
```

## 参数管理命令

### 查看参数
```bash
param -p          # 查看所有参数
param -p audio    # 查看音频参数
param -p looper   # 查看looper参数
param -p bt       # 查看蓝牙参数
param -p lcd      # 查看LCD参数
```

### 保存参数
```bash
param -s          # 保存所有参数到Flash
param -s audio    # 仅保存音频参数
param -s looper   # 仅保存looper参数
```

### 重置参数
```bash
param -d          # 重置为默认值
param -s          # 保存到Flash
```

### 系统信息
```bash
param -i          # 显示参数系统信息
param -t          # 测试Flash读写
```

## 使用流程示例

### 典型配置流程
```bash
# 1. 设置音量
audio -g 80       # 吉他音量80
audio -i 75       # 麦克风音量75
audio -o 85       # 输出音量85

# 2. 设置looper
looper -M bpm 120 # 节拍120
looper -m song    # song模式

# 3. 设置蓝牙
bt -n "MyGuitar"  # 设置名称
bt -v 80          # A2DP音量

# 4. 设置LCD
lcd -b 60         # 对比度60
lcd -c 0x0000     # 黑色背景

# 5. 保存所有设置
param -s          # 一次保存所有参数
```

### 分模块保存
```bash
# 修改音频参数
audio -g 85
audio -i 80
audio -S          # 仅保存音频参数

# 修改looper参数
looper -M bpm 130
looper -S         # 仅保存looper参数
```

## 注意事项

1. **立即保存**: 由于设备是直接断电关机，修改参数后必须调用 `-S` 保存
2. **参数生效**: 
   - 音量、BPM等参数立即生效
   - 蓝牙名称需要重启后生效
3. **自动加载**: 系统启动时自动从Flash加载所有保存的参数
4. **参数修改标记**: 
   - 设置参数时自动标记为已修改
   - 使用 `-S` 保存后清除修改标记
5. **写入次数**: Flash擦写寿命约10万次，建议批量修改后统一保存

## 参数列表

| 模块 | 参数 | 范围 | 说明 |
|------|------|------|------|
| Audio | guitar_volume | 0-100 | 吉他输入音量 |
| Audio | mic_volume | 0-100 | 麦克风输入音量 |
| Audio | output_volume | 0-100 | 输出音量 |
| Looper | tempo | 40-240 | BPM节拍 |
| Looper | time_signature | 1-16 | 每小节拍数 |
| Looper | overdub_mode | 0/1 | 循环模式 |
| BT | device_name | 15字符 | 蓝牙设备名称 |
| BT | a2dp_volume | 0-100 | A2DP音量 |
| LCD | contrast | 0-100 | 对比度 |
| LCD | bg_color | RGB565 | 背景颜色 |

## 常用颜色值（RGB565）

| 颜色 | 值 |
|------|------|
| 黑色 | 0x0000 |
| 白色 | 0xFFFF |
| 红色 | 0xF800 |
| 绿色 | 0x07E0 |
| 蓝色 | 0x001F |
| 黄色 | 0xFFE0 |
| 青色 | 0x07FF |
| 紫色 | 0xF81F |
