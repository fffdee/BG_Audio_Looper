# UI Shell 命令快速测试脚本

## 通过串口或BLE发送以下命令来测试UI系统

### 基础测试
```
# 查看帮助
help ui

# 获取当前状态
ui -g

# 刷新界面
ui -r
```

### 界面切换测试
```
# 切换到开机界面
ui -s boot

# 等待3秒后切换到主界面
ui -s idle

# 进入菜单
ui -s menu

# 返回主界面
ui -s idle

# 进入Looper
ui -s looper

# 返回主界面
ui -s idle
```

### 弹窗测试
```
# 简单弹窗
ui -p "Hello World"

# 带标题的弹窗
ui -p "Info" "System Ready"

# 长时间显示
ui -p "Warning" "Low Battery" 5000

# 关闭弹窗
ui -c
```

### 状态栏测试
```
# 设置电量为50%
ui -b 50

# 设置蓝牙为已连接
ui -t 3

# 设置音量为75
ui -v 75

# 更新状态栏
ui -u

# 模拟电量下降
ui -b 30
ui -b 20
ui -b 10

# 模拟蓝牙状态变化
ui -t 0
ui -t 1
ui -t 2
ui -t 3
ui -t 4
```

### 按钮状态查询
```
# 查看所有按钮状态
ui -k
```

### 调试模式测试
```
# 启用调试
ui -d on

# 执行一些操作
ui -s idle
ui -p "Debug" "Testing"
ui -r

# 关闭调试
ui -d off
```

### 组合测试场景

#### 场景1: 启动流程
```
ui -d on
ui -s boot
# 等待开机动画完成（约3秒）
ui -g
```

#### 场景2: 电池低电量警告
```
ui -b 15
ui -p "Warning" "Battery Low: 15%" 3000
ui -t 0
```

#### 场景3: 蓝牙连接流程
```
ui -t 1
ui -p "Info" "Bluetooth Advertising"
# 延迟
ui -t 2
ui -p "Info" "Connecting..."
# 延迟
ui -t 3
ui -p "Success" "Bluetooth Connected" 2000
ui -t 4
```

#### 场景4: 菜单导航
```
ui -s idle
ui -k
ui -s menu
ui -g
ui -s idle
```

#### 场景5: 完整系统状态展示
```
ui -b 85
ui -t 3
ui -v 60
ui -u
ui -s idle
ui -p "System" "All Systems Normal" 2000
```

### 压力测试
```
# 快速切换状态
ui -s idle
ui -s menu
ui -s idle
ui -s looper
ui -s idle

# 连续弹窗
ui -p "Test1"
ui -c
ui -p "Test2"
ui -c
ui -p "Test3"
ui -c

# 状态栏快速更新
ui -b 100
ui -b 90
ui -b 80
ui -b 70
ui -b 60
```

### 边界条件测试
```
# 电量边界
ui -b 0
ui -b 100

# 蓝牙状态边界
ui -t 0
ui -t 4

# 音量边界
ui -v 0
ui -v 100

# 无效输入测试
ui -s invalid
ui -b 150
ui -t 10
ui -v -10
```

### 自动化测试序列

将以下命令按顺序发送（每个命令间隔1秒）：
```
ui -d on
ui -g
ui -s boot
ui -b 100
ui -t 0
ui -v 50
ui -s idle
ui -t 1
ui -p "Ready" "System Initialized"
ui -t 3
ui -v 75
ui -k
ui -s menu
ui -s idle
ui -p "Test" "Complete" 3000
ui -d off
```

## 预期结果

1. **状态切换**: 界面应该平滑切换，无闪烁
2. **弹窗显示**: 弹窗应该居中显示，按时自动关闭
3. **状态栏更新**: 图标和数值应该立即更新
4. **按钮查询**: 应该显示所有按钮的实时状态
5. **调试输出**: 启用调试后应该看到详细日志

## 问题排查

如果命令不工作：
1. 检查Shell连接（CDC/BLE/UART）
2. 确认UI系统已初始化
3. 查看调试输出（`ui -d on`）
4. 检查当前状态（`ui -g`）
5. 尝试刷新界面（`ui -r`）
