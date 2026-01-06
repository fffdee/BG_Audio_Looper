# System Monitor Shell Commands

## 概述

系统监控模块提供了实时查看CPU占用率、内存使用情况和任务状态的命令。

## 命令列表

### 1. 内存使用情况
```
sysmon -m
或
sysmon --memory
```

**输出内容：**
- Current Free: 当前剩余堆内存
- Minimum Ever Free: 历史最低剩余内存（用于检测内存峰值）
- Total Heap Size: 总堆大小
- Used: 已使用内存及百分比
- Peak Used: 峰值使用内存及百分比

**示例输出：**
```
=== Memory Usage ===
Current Free:      45678 bytes
Minimum Ever Free: 42340 bytes
Total Heap Size:   65536 bytes
Used:              19858 bytes (30.3%)
Peak Used:         23196 bytes (35.4%)
```

### 2. CPU使用统计
```
sysmon -c
或
sysmon --cpu
```

**输出内容：**
- 每个任务的绝对运行时间
- 每个任务的CPU占用百分比

**示例输出：**
```
=== CPU Usage Statistics ===
Task            	Abs Time	Percent
-----------------------------------------------
AudioLoopTask   	1234567		45%
BTStackTask     	654321		24%
ShellTask       	234567		8%
IDLE            	987654		23%
```

**注意：** 需要在FreeRTOSConfig.h中启用以下配置：
```c
#define configGENERATE_RUN_TIME_STATS       1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1
```

并实现运行时统计时钟：
```c
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()  // 配置高精度定时器
#define portGET_RUN_TIME_COUNTER_VALUE()          // 获取当前计数值
```

### 3. 任务信息
```
sysmon -t
或
sysmon --tasks
```

**输出内容：**
- 任务名称
- 任务状态 (X=运行中, R=就绪, B=阻塞, S=挂起, D=删除)
- 优先级
- 剩余栈空间（单位：字，4字节）
- 任务编号

**示例输出：**
```
=== Task Information ===
Task            	State	Prio	Stack	Num
-----------------------------------------------
AudioLoopTask   	R	5	512	1
BTStackTask     	B	4	768	2
ShellTask       	X	3	1024	3
IDLE            	R	0	128	4

State: X=Running, R=Ready, B=Blocked, S=Suspended, D=Deleted
Stack: Free stack space in words (4 bytes each)
```

**注意：** 需要在FreeRTOSConfig.h中启用：
```c
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1
```

### 4. 系统信息
```
sysmon -s
或
sysmon --sysinfo
```

**输出内容：**
- RTOS类型和版本
- 系统时钟频率
- Tick频率
- 最大优先级
- 内存状态
- 功能开关状态

**示例输出：**
```
=== System Information ===
RTOS:              FreeRTOS
Version:           V10.0.0
Tick Rate:         1000 Hz
CPU Clock:         144 MHz
Max Priority:      32
Minimal Stack:     128 words

=== Memory Status ===
Heap Total:        65536 bytes
Heap Free:         45678 bytes (69.7%)

=== Features ===
Preemption:        Enabled
Idle Hook:         Disabled
Tick Hook:         Disabled
Runtime Stats:     Enabled
Trace Facility:    Enabled
```

## FreeRTOSConfig.h 配置要求

为了使用所有功能，请在 FreeRTOSConfig.h 中添加或确认以下配置：

```c
// 启用任务列表功能
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

// 启用CPU统计功能（需要高精度定时器）
#define configGENERATE_RUN_TIME_STATS           1

// 运行时统计时钟配置（根据实际硬件配置）
// 示例：使用一个高频定时器（比系统tick快10-100倍）
extern volatile uint32_t g_ulHighFrequencyTimerTicks;
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() do { \
    /* 初始化高频定时器 */ \
    g_ulHighFrequencyTimerTicks = 0; \
} while(0)
#define portGET_RUN_TIME_COUNTER_VALUE() g_ulHighFrequencyTimerTicks
```

## 使用场景

### 1. 调试内存泄漏
定期运行 `sysmon -m`，观察 "Minimum Ever Free" 是否持续下降：
```bash
$ sysmon -m
Current Free:      45000 bytes  # 第一次
...
$ sysmon -m
Minimum Ever Free: 42000 bytes  # 如果持续下降，可能有内存泄漏
```

### 2. 分析性能瓶颈
运行 `sysmon -c` 查看哪个任务占用CPU最多：
```bash
$ sysmon -c
AudioLoopTask   45%  # CPU占用过高，可能需要优化
BTStackTask     24%
```

### 3. 检查栈溢出风险
运行 `sysmon -t` 查看每个任务的剩余栈空间：
```bash
$ sysmon -t
Task            Stack
AudioLoopTask   50    # 剩余空间很小（50*4=200字节），可能需要增加栈大小
ShellTask       800   # 剩余空间充足
```

### 4. 综合系统检查
```bash
$ sysmon -s  # 查看整体系统状态
$ sysmon -m  # 检查内存
$ sysmon -t  # 检查任务
$ sysmon -c  # 检查CPU
```

## 注意事项

1. **性能影响**: CPU统计功能会轻微增加系统开销（约1-2%），但提供的诊断信息非常有价值

2. **内存消耗**: 统计命令会临时分配2KB内存用于格式化输出，执行完毕后立即释放

3. **实时性**: 统计数据反映的是命令执行时刻的系统状态，对于动态分析建议多次采样

4. **优先级**: Shell任务应该有适中的优先级，避免影响关键任务的实时性

## 集成方式

系统监控模块已集成到BG Shell框架中，只需在初始化时调用：

```c
// 在 bg_audio_Init() 中
ShellCmdSysmon_Register();
```

命令会自动注册到Shell系统，支持通过CDC-UART和BLE两种方式访问。

## 扩展功能建议

未来可以添加的功能：
- 定时自动采样并记录
- 内存分配跟踪
- 任务执行时间分布图
- 栈使用水位线监控
- 性能事件日志
