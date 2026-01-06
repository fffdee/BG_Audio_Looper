# 系统监控命令 - 快速参考

## 命令概览

| 命令 | 简写 | 功能 |
|------|------|------|
| `sysmon --memory` | `sysmon -m` | 查看内存使用情况 |
| `sysmon --cpu` | `sysmon -c` | 查看CPU占用率 |
| `sysmon --tasks` | `sysmon -t` | 查看任务信息 |
| `sysmon --sysinfo` | `sysmon -s` | 查看系统信息 |

## 快速开始

### 1. 连接串口
- **CDC-UART方式**：通过USB连接设备，打开串口工具（波特率115200）
- **BLE方式**：通过蓝牙连接设备，使用串口助手APP

### 2. 查看帮助
```bash
$ help
$ help -m sysmon
```

### 3. 基础检查
```bash
# 检查内存
$ sysmon -m

# 检查任务
$ sysmon -t

# 检查CPU
$ sysmon -c

# 系统信息
$ sysmon -s
```

## 实际使用示例

### 场景1：调试音频失真问题
```bash
# 1. 先查看CPU占用
$ sysmon -c
=== CPU Usage Statistics ===
Task            	Abs Time	Percent
-----------------------------------------------
AudioLoopTask   	1234567		78%    # CPU占用过高！
BTStackTask     	234567		12%
ShellTask       	123456		5%
IDLE            	123456		5%

# 分析：AudioLoopTask占用78%，可能需要优化音频处理算法
```

### 场景2：内存泄漏检测
```bash
# 每隔一段时间执行一次
$ sysmon -m
Minimum Ever Free: 42340 bytes

# 5分钟后
$ sysmon -m
Minimum Ever Free: 38200 bytes  # 持续下降，可能有内存泄漏

# 10分钟后
$ sysmon -m
Minimum Ever Free: 35100 bytes  # 确认有内存泄漏问题
```

### 场景3：栈溢出检查
```bash
$ sysmon -t
=== Task Information ===
Task            	State	Prio	Stack	Num
-----------------------------------------------
AudioLoopTask   	R	5	45	1     # 剩余45*4=180字节，危险！
BTStackTask     	B	4	512	2
ShellTask       	X	3	800	3

# 建议：增加AudioLoopTask的栈大小
```

## 组合使用技巧

### 全面系统检查脚本
```bash
$ sysmon -s  # 系统总览
$ sysmon -m  # 内存状态
$ sysmon -t  # 任务状态
$ sysmon -c  # CPU分析
```

### 性能监控（需要定期执行）
```bash
# 在音频播放前
$ sysmon -c
$ sysmon -m

# 在音频播放中
$ sysmon -c
$ sysmon -m

# 在音频播放后
$ sysmon -c
$ sysmon -m

# 对比三次结果，分析性能变化
```

## 常见问题解决

### Q: 命令无响应
**A:** 检查串口连接，确保波特率正确（115200）

### Q: CPU统计显示错误
**A:** 确认FreeRTOSConfig.h中已启用：
```c
#define configGENERATE_RUN_TIME_STATS 1
```

### Q: 任务信息显示不全
**A:** 确认FreeRTOSConfig.h中已启用：
```c
#define configUSE_TRACE_FACILITY 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1
```

### Q: 内存统计不准确
**A:** FreeRTOS的内存统计仅包含堆内存，栈内存和全局变量不包括在内

## 调试技巧

### 1. CPU占用过高
- 使用 `sysmon -c` 找出CPU占用最高的任务
- 检查该任务是否有死循环或耗时操作
- 考虑降低任务优先级或拆分任务

### 2. 内存不足
- 使用 `sysmon -m` 查看剩余内存
- 检查 "Peak Used" 了解历史最大使用量
- 考虑增加总堆大小或优化内存使用

### 3. 任务阻塞
- 使用 `sysmon -t` 查看任务状态
- 如果关键任务处于 'B' (阻塞) 状态，检查信号量或队列
- 检查是否有优先级反转问题

### 4. 栈溢出风险
- 使用 `sysmon -t` 查看每个任务的剩余栈空间
- 如果剩余空间 < 100字（400字节），建议增加栈大小
- 考虑启用栈溢出检测：`configCHECK_FOR_STACK_OVERFLOW = 2`

## 性能基准参考

### 正常系统状态
- **CPU IDLE任务**: 应该 > 10%（系统有空闲时间）
- **内存使用**: 应该 < 80%（留有余量）
- **任务栈剩余**: 应该 > 200字（800字节）

### 警告阈值
- **CPU单任务**: > 80%（可能有性能瓶颈）
- **内存使用**: > 90%（接近耗尽）
- **栈剩余**: < 100字（400字节，有溢出风险）

## 集成到自动化测试

可以通过脚本定期采集数据：

```python
# Python示例：通过串口自动采集系统状态
import serial
import time

ser = serial.Serial('COM3', 115200)

while True:
    # 发送命令
    ser.write(b'sysmon -m\r\n')
    time.sleep(0.5)
    
    # 读取结果
    output = ser.read(ser.in_waiting).decode()
    
    # 解析并记录
    parse_and_log(output)
    
    time.sleep(60)  # 每分钟采集一次
```

## 更多帮助

详细文档请参考：
- `SYSMON_COMMANDS_GUIDE.md` - 完整功能说明
- 在Shell中输入 `help -m sysmon` 查看命令帮助
