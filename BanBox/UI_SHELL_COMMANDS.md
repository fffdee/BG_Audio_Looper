# UI Shell 命令使用指南

## 概述

UI命令模块提供了通过命令行控制UI系统的能力，可以方便地进行调试和测试。

## 命令格式

```
ui [选项] [参数]
```

## 可用命令

### 1. 状态控制

#### 设置UI状态
```bash
ui -s <state>
ui --state <state>
```

可用状态：
- `boot` - 开机界面
- `idle` - 主界面（Home）
- `menu` - 菜单界面
- `looper` - 循环器界面
- `settings` - 设置界面

示例：
```bash
ui -s idle      # 切换到主界面
ui -s boot      # 返回开机界面
ui -s menu      # 进入菜单
```

#### 获取当前状态
```bash
ui -g
ui --get
```

显示信息：
- 当前状态
- 上一个状态
- 系统就绪状态

示例输出：
```
Current state: IDLE
Previous state: BOOT
Ready: Yes
```

### 2. 弹窗控制

#### 显示弹窗
```bash
ui -p <message>                    # 简单消息
ui -p <title> <message>            # 带标题
ui -p <title> <message> <duration> # 指定持续时间(ms)
ui --popup <message>
```

示例：
```bash
ui -p "Hello World"                    # 默认标题"Info"，2秒
ui -p "Warning" "Battery Low"          # 自定义标题，2秒
ui -p "Error" "Connection Failed" 5000 # 显示5秒
```

#### 关闭弹窗
```bash
ui -c
ui --close
```

### 3. UI刷新

#### 强制刷新UI
```bash
ui -r
ui --refresh
```

强制重绘当前界面的所有视图。

### 4. 按钮状态查询

#### 查看按钮状态
```bash
ui -k
ui --keys
```

显示所有按钮的当前状态：
```
Button States:
  UP: Released
  DOWN: Released
  ENTER: PRESSED
  BACK: Released
```

### 5. 状态栏控制

#### 设置电池电量
```bash
ui -b <0-100>
ui --battery <0-100>
```

示例：
```bash
ui -b 85    # 设置电量为85%
ui -b 20    # 设置电量为20%
```

#### 设置蓝牙状态
```bash
ui -t <0-4>
ui --bt <0-4>
```

状态值：
- `0` - 未连接
- `1` - 广播中
- `2` - 连接中
- `3` - 已连接
- `4` - 播放中

示例：
```bash
ui -t 0     # 蓝牙断开
ui -t 3     # 蓝牙已连接
ui -t 4     # 蓝牙播放中
```

#### 设置音量
```bash
ui -v <0-100>
ui --volume <0-100>
```

示例：
```bash
ui -v 50    # 设置音量为50
ui -v 80    # 设置音量为80
```

#### 更新状态栏
```bash
ui -u
ui --update
```

强制刷新状态栏显示。

### 6. 调试模式

#### 启用/禁用调试模式
```bash
ui -d <on|off>
ui --debug <on|off>
```

示例：
```bash
ui -d on    # 启用UI调试输出
ui -d off   # 禁用UI调试输出
```

## 使用场景

### 场景1: 测试界面切换
```bash
ui -s boot    # 显示开机动画
# 等待动画完成
ui -s idle    # 返回主界面
ui -s menu    # 进入菜单
ui -g         # 查看当前状态
```

### 场景2: 测试弹窗功能
```bash
ui -p "Test" "This is a test message" 3000
# 等待3秒或手动关闭
ui -c
```

### 场景3: 模拟电池和蓝牙状态
```bash
ui -b 100     # 满电
ui -t 4       # 蓝牙播放
ui -v 75      # 音量75
ui -u         # 更新显示
```

### 场景4: 调试界面问题
```bash
ui -d on      # 启用调试
ui -s idle    # 切换到主界面
ui -r         # 强制刷新
ui -k         # 查看按钮状态
ui -g         # 查看UI状态
```

## 组合使用示例

### 完整测试流程
```bash
# 1. 启用调试
ui -d on

# 2. 查看当前状态
ui -g

# 3. 设置状态栏
ui -b 85
ui -t 3
ui -v 60

# 4. 切换界面
ui -s idle

# 5. 显示测试弹窗
ui -p "Test" "UI System OK" 2000

# 6. 检查按钮
ui -k

# 7. 刷新界面
ui -r
```

## 注意事项

1. **状态切换**: 切换到Boot状态会播放完整的开机动画
2. **弹窗持续时间**: 默认2000ms，可通过第三个参数自定义
3. **状态栏更新**: 修改状态栏数据后，系统会自动更新显示
4. **调试模式**: 启用后会在串口输出详细的UI事件日志
5. **按钮状态**: 仅用于查询，不能通过命令模拟按键

## 错误处理

命令执行失败时会返回错误信息：

```bash
ui -s invalid
# 输出: Unknown state: invalid

ui -b 150
# 输出: Battery level must be 0-100

ui -t 10
# 输出: Bluetooth status must be 0-4
```

## 帮助信息

查看ui命令的帮助：
```bash
help ui
```

或直接输入命令查看用法：
```bash
ui -s
ui -b
ui -t
```
