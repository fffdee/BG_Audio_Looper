# 系统监控功能 - 内存优化说明

## 问题描述

在实际运行中发现，系统堆内存只有37KB，运行时剩余内存只有680字节，导致`sysmon`命令无法分配2KB临时缓冲区而失败。

```
$ sysmon -c
Error: Failed to allocate memory for CPU stats
Error: -1
```

## 已实施的优化方案

### 1. 使用静态缓冲区（已实施）✅

**优点**:
- 不占用堆内存
- 无分配失败风险
- 执行速度更快

**缺点**:
- 占用1KB全局RAM（BSS段）
- 不可重入（但Shell命令本身就是串行执行）

**修改**:
```c
// 原来：动态分配2KB
char *buf = (char *)pvPortMalloc(2048);

// 现在：使用静态缓冲区1KB
static char g_sysmon_buffer[1024];
memset(g_sysmon_buffer, 0, sizeof(g_sysmon_buffer));
vTaskGetRunTimeStats(g_sysmon_buffer);
```

### 2. 增加堆内存大小（已实施）✅

从37KB增加到48KB，增加约11KB空间：

```c
// FreeRTOSConfig.h
#define configTOTAL_HEAP_SIZE  ( ( size_t ) ( 48 * 1024 ) )  /* 原来37KB */
```

**影响**:
- RAM使用增加11KB
- 为系统提供更多动态内存余量
- 降低内存耗尽风险

## 内存使用分析

### 优化前
- 总堆大小: 37,888 字节
- 剩余: 680 字节
- 使用率: 98.2% ⚠️ **极度危险**

### 优化后（预期）
- 总堆大小: 49,152 字节
- 增加: 11,264 字节
- 预期剩余: ~12KB
- 使用率: ~75% ✅ **健康**

### sysmon命令内存占用
- **优化前**: 需要动态分配2KB，失败
- **优化后**: 使用1KB静态缓冲区，成功

## 性能对比

| 项目 | 动态分配 | 静态缓冲区 |
|------|---------|-----------|
| 内存类型 | 堆(Heap) | 全局(BSS) |
| 分配开销 | ~100+ CPU周期 | 0 |
| 失败风险 | 内存不足时失败 | 无 |
| 可重入性 | 支持 | 不支持 |
| 适用场景 | 多线程环境 | 串行Shell命令 |

## 使用建议

### 1. 监控内存使用
定期运行 `sysmon -m` 检查内存：

```bash
$ sysmon -m

=== Memory Usage ===
Current Free:      12340 bytes   # 应该 > 10KB
Minimum Ever Free: 11200 bytes   # 不应持续下降
Peak Used:         36952 bytes   # 应该 < 40KB
```

### 2. 内存告警阈值
- **健康**: 剩余 > 10KB (绿色)
- **警告**: 剩余 5-10KB (黄色)
- **危险**: 剩余 < 5KB (红色)
- **紧急**: 剩余 < 1KB (需要立即优化)

### 3. 内存优化技巧

#### 减少动态内存分配
```c
// 不好：频繁分配
void process() {
    char *buf = malloc(1024);
    // ...
    free(buf);
}

// 好：使用静态缓冲区
static char g_buf[1024];
void process() {
    // 直接使用 g_buf
}
```

#### 使用内存池
```c
// 预分配固定大小的内存块
typedef struct {
    uint8_t data[256];
    bool in_use;
} MemBlock_t;

static MemBlock_t g_mem_pool[8];
```

#### 及时释放不用的内存
```c
// 任务初始化完成后释放初始化数据
void task_init() {
    init_data = malloc(1024);
    // ... 初始化
    free(init_data);  // 立即释放
    init_data = NULL;
}
```

## 常见内存问题排查

### 1. 内存泄漏
**现象**: `Minimum Ever Free` 持续下降

```bash
# 间隔5分钟执行
$ sysmon -m
Minimum Ever Free: 12000 bytes

$ sysmon -m  # 5分钟后
Minimum Ever Free: 10500 bytes  # 下降了1500字节

$ sysmon -m  # 再过5分钟
Minimum Ever Free: 9000 bytes   # 继续下降 ⚠️ 有泄漏！
```

**排查方法**:
1. 检查是否有 `malloc` 没有对应的 `free`
2. 检查是否有指针丢失导致无法释放
3. 使用调试器追踪内存分配

### 2. 栈溢出导致堆损坏
**现象**: 系统崩溃或随机错误

```bash
$ sysmon -t
Task         Stack
AudioTask    45     # 只剩180字节，危险！
BTTask       512    # 安全
```

**解决方法**:
1. 增加任务栈大小
2. 减少局部变量大小
3. 启用栈溢出检测：`configCHECK_FOR_STACK_OVERFLOW = 2`

### 3. 碎片化
**现象**: 总剩余内存足够，但分配失败

**解决方法**:
1. 使用固定大小的内存池
2. 避免频繁分配/释放不同大小的内存
3. 考虑使用 heap_4.c (支持内存合并)

## 紧急情况处理

如果内存不足导致系统不稳定：

### 短期方案
1. 禁用非关键功能
2. 减小缓冲区大小
3. 降低任务栈大小

### 长期方案
1. 增加RAM（如果硬件支持）
2. 代码优化减少内存使用
3. 使用外部存储（如Flash）存储大数据

## 编译后检查

重新编译后，检查RAM使用：

```
Memory region         Used Size  Region Size  %age Used
           DTCM:          0 GB       128 KB      0.00%
           SRAM:      45678 B        64 KB     69.70%  # 应该 < 80%
```

如果RAM使用超过80%，考虑：
1. 减小 `configTOTAL_HEAP_SIZE`
2. 减小任务栈大小
3. 使用更大RAM的芯片

## 测试验证

编译烧录后测试：

```bash
# 1. 检查内存增加
$ sysmon -m
Heap Total:        49152 bytes   # 应该是48KB
Heap Free:         12000+ bytes  # 应该 > 10KB

# 2. 测试CPU统计
$ sysmon -c
=== CPU Usage Statistics ===
Task            	Abs Time	Percent
-----------------------------------------------
AudioLoopTask   	1234567		45%    # 应该正常显示

# 3. 测试任务信息
$ sysmon -t
=== Task Information ===
Task            	State	Prio	Stack	Num
-----------------------------------------------
AudioLoopTask   	R	5	512	1      # 应该正常显示

# 4. 系统信息
$ sysmon -s
Heap Free:         12000 bytes (24.4%)  # 使用率应该 < 80%
```

## 总结

通过以下两项优化：
1. ✅ 使用1KB静态缓冲区替代2KB动态分配
2. ✅ 增加堆内存从37KB到48KB

**预期效果**:
- sysmon命令可正常工作
- 系统内存余量充足（~12KB）
- 降低内存耗尽风险
- 提高系统稳定性

**注意事项**:
- 需要重新编译整个工程
- 需要确认芯片RAM足够（建议总RAM > 64KB）
- 定期监控内存使用情况

---

**更新日期**: 2026年1月6日  
**状态**: 已优化，待测试验证
