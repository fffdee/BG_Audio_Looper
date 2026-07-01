# BanGTsynth SD卡+NAND+PSRAM 合成器使用指南

## 概述

新的合成器架构实现了三级存储层次：
- **SD卡**: 存储原始 SF2 音源文件
- **NAND Flash**: 缓存处理后的音色数据
- **PSRAM**: 高速音符缓冲区，支持并发播放

## 启用新架构

### 1. 宏定义启用

在 `product_def.h` 中取消注释：

```c
/* BanGTsynth SD卡+NAND+PSRAM 新合成器方案 (BanBox_II 特有) */
#define SYNTH_SD_NAND_PSRAM_EN
```

### 2. 硬件要求

确保 BanBox II 硬件配置：
- SD卡插槽 (GPIO_A15/A16/A17)
- NAND Flash W25N02 (GPIO_A29)
- PSRAM ESP-PSRAM64H (GPIO_B6)

### 3. 编译和烧录

重新编译项目并烧录到 BanBox II 设备。

## 使用方法

### 初始化

```c
#include "synth_sdnandpsram.h"

// 在系统启动时调用
if (SYNTH_SDNANDPSRAM_Init() != SUCCESS) {
    // 初始化失败处理
}
```

### 音符播放

```c
// 播放音符 (自动管理缓冲区)
SYNTH_SDNANDPSRAM_NoteOn(60, 100, 0);  // 中音C, 力度100, 程序0

// 停止音符
SYNTH_SDNANDPSRAM_NoteOff(60, 0);
```

### 重新加载音源

```c
// 从 SD 卡重新加载 SF2 文件
if (SYNTH_SDNANDPSRAM_ReloadFromSD() != SUCCESS) {
    // 重新加载失败
}
```

## 架构特性

### 自动存储管理

- **开机加载**: SD卡 → NAND Flash 自动拷贝
- **按需加载**: 音符数据从 NAND → PSRAM 异步加载
- **LRU缓存**: PSRAM 缓冲区智能替换最久未用数据
- **并发支持**: 多音符同时播放，无阻塞

### 性能优化

- **DMA传输**: 大块数据高速传输
- **异步加载**: 音符触发不等待数据加载完成
- **预测预加载**: 自动预加载相邻音符
- **校验和验证**: 数据完整性保证

### 错误处理

- **降级模式**: SD卡未插入时跳过加载
- **恢复机制**: NAND/PSRAM 访问失败时的自动恢复
- **日志记录**: 详细的调试和错误信息

## 监控和调试

### 状态查询

```c
SYNTH_Status_t status;
SYNTH_SDNANDPSRAM_GetStatus(&status);

if (status.soundbank_ready) {
    // 音源已就绪
}
```

### 拷贝进度

```c
uint32_t done, total;
SYNTH_SDNANDPSRAM_GetCopyProgress(&done, &total);
float progress = (float)done / total * 100.0f;
```

### 性能测试

```c
// 运行完整性能测试套件
SYNTH_RunPerformanceTests();

// 获取性能指标
SYNTH_PerfMetrics_t metrics;
SYNTH_GetPerformanceMetrics(&metrics);

// 打印性能报告
SYNTH_PrintPerformanceReport();
```

### 启动测试

```c
// 执行启动序列测试
SYNTH_StartupSequence();

// 检查启动状态
if (SYNTH_IsStartupComplete()) {
    // 启动完成
}
```

## 文件结构

```
02_core/
├── fat32/                    # FAT32 读取器
├── nand_store/              # NAND 存储管理器
├── psram_buffer/            # PSRAM 缓冲区管理器
└── synth_integration/       # 集成模块
    ├── synth_sdnandpsram.h/c      # 核心集成接口
    ├── synth_startup.c            # 启动管理
    ├── synth_integration_test.c   # 功能测试
    └── synth_performance_test.c   # 性能测试
```

## 配置参数

### PSRAM 配置

```c
#define PSRAM_TOTAL_SIZE            (8 * 1024 * 1024)     // 8MB
#define PSRAM_BUFFER_POOL_SIZE      (6 * 1024 * 1024)     // 6MB 缓冲池
#define PSRAM_NOTE_BUFFER_SIZE      (64 * 1024)           // 64KB/音符
#define PSRAM_MAX_NOTE_BUFFERS      96                    // 最大96个缓冲区
```

### NAND 配置

```c
#define NAND_INDEX_START           (32 * 1024 * 1024)     // 32MB 索引区
#define NAND_DATA_START            (64 * 1024 * 1024)     // 64MB 数据区
#define NAND_MAX_PROGRAMS          128                    // 最大128个程序
```

### 性能参数

```c
#define PERF_TEST_ITERATIONS       100     // 测试迭代次数
#define PERF_TEST_SAMPLE_RATE      44100   // 采样率
#define PSRAM_GC_THRESHOLD         8       // GC 阈值
```

## 故障排除

### 常见问题

1. **SD卡未识别**
   - 检查 SD卡格式 (FAT32)
   - 确认卡中包含 .sf2 文件
   - 查看日志中的 FAT32 初始化错误

2. **NAND 存储失败**
   - 检查 NAND Flash 硬件连接
   - 确认 FlashDevices_GetNandFlash() 返回有效指针
   - 查看 NAND 坏块情况

3. **PSRAM 缓冲区不足**
   - 减少同时播放的音符数量
   - 检查 PSRAM 硬件连接
   - 查看缓冲区统计信息

4. **音符播放延迟**
   - 检查 NAND 到 PSRAM 的加载时间
   - 确认异步加载任务正常运行
   - 查看 LRU 缓存命中率

### 调试命令

```c
// 查看存储状态
BG_LOG_I(BG_LOG_TAG_SYNTH, "Storage ready: %d", g_synth_status.storage_ready);

// 查看缓冲区统计
uint32_t total, free, ready, playing;
PSRAM_GetStats(&total, &free, &ready, &playing);

// 查看 NAND 使用情况
uint32_t nand_total, nand_used, nand_programs;
NAND_GetStats(&nand_total, &nand_used, &nand_programs);
```

## 扩展开发

### 添加新的音源格式

1. 在 `soundbank_manager.h` 中添加格式枚举
2. 实现对应的解析器 (如 `sf3_parser.c`)
3. 在 `synth_sdnandpsram.c` 中集成新的加载逻辑

### 自定义存储布局

1. 修改 `nand_store.h` 中的布局常量
2. 更新 `NAND_StoreInit()` 中的初始化逻辑
3. 调整 `synth_sdnandpsram.c` 中的数据定位函数

### 性能优化

1. 调整 PSRAM 缓冲区大小
2. 修改 LRU 缓存策略
3. 优化 DMA 传输块大小
4. 实现更智能的预测预加载算法

---

## 总结

新的 SD卡+NAND+PSRAM 合成器架构提供了高效、可靠的音源管理方案。通过合理的存储层次设计，实现了大容量音源存储、高速音符播放和优秀的并发性能。

要开始使用，只需启用宏定义并重新编译即可。系统会自动处理复杂的存储管理和数据加载，让开发者专注于音乐合成逻辑。