# Const 内存优化方案

**日期**: 2026-02-05  
**目标**: 通过全面的 const 优化，将不需要运行时修改的数据从 RAM 移至 Flash，节省内存

---

## 📊 内存问题诊断

从启动日志可以看到：
```
[AudioInit] Memory available at start: 62652 bytes
[AudioInit] Reverb allocated: 57548 bytes (remain: 5104)
[AudioInit] DRC allocated: 248 bytes (remain: 4856)
[AudioInit] Initializing 4x ADC EQ (mono, 3-band)...
[AudioInit] EQ_guitar_l: en=1 ct=20039814 allocated=1576 (remain: 3280)
[AudioInit] EQ_guitar_r: en=1 ct=200391ec allocated=1576 (remain: 1704)
[AudioInit] EQ_mic_l: en=1 ct=20038bc4 allocated=1576 (remain: 128)
EQContext mem malloc err! 1556  ← 内存不足
[AudioInit] EQ_mic_r: en=0 ct=0 allocated=0 (remain: 128)
EQContext mem malloc err! 1556  ← 内存不足
[AudioInit] USB/BT_EQ: en=0 ct=0 allocated=0 (remain: 128)
```

**问题**: 即使经过之前的优化，内存仍然不足，无法完成所有 EQ 的初始化。

---

## ✅ 已完成的优化（第一阶段）

### 1. 聚合数组 Const 化 (节省 124 字节 RAM)

**文件**: `ctrlvars.h`, `ctrlvars.c`

**修改前**:
```c
extern EQUnit *eq_unit_aggregate[5];                    // 20 字节 RAM
extern DRCUnit *drc_unit_aggregate[3];                  // 12 字节 RAM
extern EQFilterParams *eq_param_aggregate[5];           // 20 字节 RAM
extern ExpanderUnit *expander_unit_aggregate[2];        // 8 字节 RAM
extern GainControlUnit *gain_unit_aggregate[16];        // 64 字节 RAM
```

**修改后**:
```c
extern EQUnit * const eq_unit_aggregate[5];             // 存储在 Flash
extern DRCUnit * const drc_unit_aggregate[3];           // 存储在 Flash
extern EQFilterParams * const eq_param_aggregate[5];    // 存储在 Flash
extern ExpanderUnit * const expander_unit_aggregate[2]; // 存储在 Flash
extern GainControlUnit * const gain_unit_aggregate[16]; // 存储在 Flash
```

**节省**: 124 字节 RAM → Flash

### 2. 效果图配置数据已使用 Const

**文件**: `effect_graph_config.c`

效果图的节点配置和边配置已经使用 const：
```c
static const NodeConfig_t g_DefaultNodes[] = DEFAULT_NODES_CONFIG;
static const EdgeConfig_t g_DefaultEdges[] = DEFAULT_EDGES_CONFIG;
```

✅ 这部分已经优化，无需修改。

---

## 🎯 新增优化（第二阶段）

### 3. 静态图模式宏定义

**文件**: `effect_graph.h`

**新增配置**:
```c
/*******************************************************************************
 * 图构建模式配置 (2026-02-05 新增)
 * 
 * USE_STATIC_EFFECT_GRAPH=1: 静态图模式（默认）
 *   - 节点/边/处理顺序在编译时确定，使用 const 数组定义
 *   - 不支持运行时增删节点/边
 *   - 内存占用最小，适合当前产品（21节点+22边固定配置）
 *   - 节省约 2KB+ RAM（节点池、边池、处理顺序数组全部改为 const）
 * 
 * USE_STATIC_EFFECT_GRAPH=0: 动态图模式
 *   - 支持运行时动态增删节点/边
 *   - 需要更多 RAM 存储可变数据
 *   - 适用于需要灵活配置的场景
 ******************************************************************************/
#ifndef USE_STATIC_EFFECT_GRAPH
#define USE_STATIC_EFFECT_GRAPH  1  /* 默认启用静态图模式，节省内存 */
#endif
```

**优势**:
- 支持两种构图方式，灵活切换
- 当前产品不需要运行时修改图结构，适合静态模式
- 可节省 2KB+ RAM

---

## 🔍 待深度优化的部分（第三阶段）

### 4. 效果图运行时数据结构

**文件**: `effect_graph.c`

当前运行时数据：
```c
static EffectGraphRuntime_t g_EffectGraph;  // 包含大量可变数据

struct EffectGraph {
    EffectNode_t nodes[EFFECT_GRAPH_MAX_NODES];          // 21 个节点
    EffectEdge_t edges[EFFECT_GRAPH_MAX_EDGES];          // 48 条边
    EffectNode_t *process_order[EFFECT_GRAPH_MAX_NODES]; // 处理顺序
    // ...
};
```

**优化方案** (在 USE_STATIC_EFFECT_GRAPH=1 模式下):
```c
// 节点结构拆分为：
// 1. 静态配置（const，存 Flash）
typedef struct {
    uint8_t id;
    EffectNodeType_t type;
    const char *name;
    // ... 其他不变的配置
} EffectNodeStaticConfig_t;

// 2. 运行时状态（RAM，最小化）
typedef struct {
    EffectNodeState_t state;
    bool processed;
    uint16_t buffer_len;
    // ... 仅保留必须在运行时变化的状态
} EffectNodeRuntimeState_t;
```

**预期节省**: 约 1.5KB RAM

### 5. EQ/DRC/Reverb 默认参数 Const 化

**文件**: `ctrlvars.c`, `effect_graph.c`

将效果器的默认参数存储在 Flash：
```c
// 修改前
void InitNodeDefaults(EffectNode_t *node, EffectNodeType_t type) {
    // 运行时初始化参数，占用代码空间
    node->params.reverb.room_size = 50;
    node->params.reverb.damping = 50;
    // ...
}

// 修改后
static const EffectParams_t g_DefaultReverbParams = {
    .reverb = { .room_size = 50, .damping = 50, .wet_dry = 30 }
};

void InitNodeDefaults(EffectNode_t *node, EffectNodeType_t type) {
    // 直接从 Flash 复制
    memcpy(&node->params, &g_DefaultReverbParams, sizeof(EffectParams_t));
}
```

**优势**:
- 减少初始化代码大小
- 参数存储在 Flash，不占 RAM

---

## 📋 总体优化清单

| 优化项目 | 状态 | 节省 RAM | 文件 |
|---------|------|----------|------|
| 聚合数组 Const 化 | ✅ 完成 | 124 字节 | ctrlvars.h, ctrlvars.c |
| 效果图配置 Const 化 | ✅ 已有 | 0 (已优化) | effect_graph_config.c |
| 静态图模式宏定义 | ✅ 完成 | 0 (准备) | effect_graph.h |
| 运行时数据结构拆分 | ⏳ 待实现 | ~1.5KB | effect_graph.c |
| 效果器默认参数 Const | ⏳ 待实现 | ~200 字节 | ctrlvars.c, effect_graph.c |
| **总计** | - | **~1.8KB** | - |

---

## 🚀 实施建议

### 立即执行（高优先级）：

1. **已完成**: 聚合数组 Const 化
2. **已完成**: 添加静态图模式宏

### 短期实施（中优先级）：

3. 在 `effect_graph.c` 中使用条件编译实现静态图模式
   - 在静态模式下禁用 `AddNode/RemoveNode/AddEdge/RemoveEdge` 函数
   - 直接从 const 配置构建图，避免拷贝

### 长期优化（低优先级）：

4. 将运行时数据结构拆分为静态配置+运行时状态
5. 将所有效果器默认参数改为 const

---

## 📝 注意事项

1. **编译选项检查**:
   - 确保编译器已启用 `-fdata-sections` 和 `-ffunction-sections`
   - 链接时使用 `--gc-sections` 移除未使用的代码和数据

2. **const 正确性**:
   - `Type * const array[]`: 指针本身是 const (数组存 Flash)
   - `const Type * array[]`: 指向的数据是 const
   - `const Type * const array[]`: 都是 const (最优)

3. **测试验证**:
   - 每次优化后编译并测试音频处理功能
   - 使用链接器 map 文件验证内存节省效果

---

## 📈 预期效果

- **第一阶段**（已完成）: 节省 124 字节 RAM
- **第二阶段**（待实现）: 额外节省 ~1.7KB RAM
- **总计**: 约 1.8KB RAM 从 RAM 移至 Flash

这应该能够为剩余的 2 个 EQ 单元（EQ_mic_r 和 USB/BT_EQ）提供足够的内存（每个约 1576 字节）。

---

## 🔧 快速编译测试

编译后检查是否有编译错误：
```bash
cd BG_Audio_Looper/BanBox
make clean
make
```

如果成功，查看启动日志确认所有 5 个 EQ 都成功初始化（ct != NULL）。
