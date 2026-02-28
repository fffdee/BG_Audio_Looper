# BGS 解析器力度层支持更新 (v2.0)

## 更新日期
2025-12-10

## 更新概述

BGS解析器已更新以支持v2.0格式的力度层功能。现在可以根据MIDI力度值选择不同的采样，实现更真实的音色表现。

---

## 文件改动

### 1. `include/bgs_parser.h`

#### 新增接口

```c
/* v2.0: 力度层选择接口 */
int bgs_select_sample_by_velocity(uint8_t note, uint8_t velocity, uint8_t program);
```

**功能**: 根据音符和力度值选择最佳采样索引

**参数**:
- `note`: MIDI音符号 (0-127)
- `velocity`: MIDI力度值 (0-127)
- `program`: Program索引

**返回值**:
- 成功: 采样索引 (>=0)
- 失败: -1

---

### 2. `src/bgs_parser.c`

#### 2.1 新增头文件

```c
#include <math.h>  /* 用于abs()函数 */
```

#### 2.2 实现力度层选择函数

```c
int bgs_select_sample_by_velocity(uint8_t note, uint8_t velocity, uint8_t program)
{
    if (program >= BG_reader.Data.program_count) {
        return -1;
    }
    
    int best_sample = -1;
    int best_distance = 999;
    
    for (uint16_t i = 0; i < BG_reader.Data.ProgramData[program].file_count; i++) {
        BGS_Note_Info *info = &BG_reader.Data.ProgramData[program].Note_Info[i];
        
        /* 1. 检查音符范围 */
        if (note < info->min_note || note > info->max_note) {
            continue;
        }
        
        /* 2. 检查力度范围 (v2.0新增) */
        if (velocity < info->min_vel || velocity > info->max_vel) {
            continue;
        }
        
        /* 3. 选择最接近的根音符 */
        int distance = abs(note - info->note);
        if (distance < best_distance) {
            best_distance = distance;
            best_sample = i;
        }
    }
    
    return best_sample;
}
```

**算法说明**:
1. **音符范围匹配**: 检查 `min_note ≤ note ≤ max_note`
2. **力度范围匹配**: 检查 `min_vel ≤ velocity ≤ max_vel` (**v2.0新增**)
3. **根音符优先**: 选择根音符最接近的采样

#### 2.3 更新 NoteOn 处理

```c
void bgs_note_on(uint8_t note, uint8_t velocity, uint8_t program)
{
    /* v2.0: 使用力度层选择采样 */
    int sample_index = bgs_select_sample_by_velocity(note, velocity, program);
    
    if (sample_index >= 0) {
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "NoteOn: note=%d, vel=%d, program=%d -> sample=%d\n", 
                 note, velocity, program, sample_index);
    } else {
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "NoteOn: note=%d, vel=%d, program=%d -> no sample found\n", 
                 note, velocity, program);
    }
}
```

---

## 数据结构说明

### BGS_Note_Info 结构体

已有字段（v1.0）:
```c
typedef struct {
    uint8_t vel_id;      /* 力度层ID（兼容旧格式） */
    uint8_t note;        /* 根音符 */
    uint8_t min_note;    /* 最低音符 */
    uint8_t max_note;    /* 最高音符 */
    uint8_t min_vel;     /* 最小力度 (v2.0) */
    uint8_t max_vel;     /* 最大力度 (v2.0) */
    uint32_t address;    /* PCM数据地址 */
} BGS_Note_Info;
```

**字段说明**:
- `min_vel`, `max_vel`: v2.0新增，定义力度层范围
- 文件解析时已正确读取这些字段（第220-221行）

---

## 使用示例

### 示例1: 钢琴三力度层

```c
/* 假设BGS文件包含以下采样: */
// C4_pp:  note=60, min_note=57, max_note=63, min_vel=0,   max_vel=42
// C4_mf:  note=60, min_note=57, max_note=63, min_vel=43,  max_vel=84
// C4_ff:  note=60, min_note=57, max_note=63, min_vel=85,  max_vel=127

/* 演奏轻柔的C4 */
bgs_note_on(60, 30, 0);  /* velocity=30 → 选择 C4_pp */

/* 演奏中等的C4 */
bgs_note_on(60, 70, 0);  /* velocity=70 → 选择 C4_mf */

/* 演奏强劲的C4 */
bgs_note_on(60, 110, 0); /* velocity=110 → 选择 C4_ff */
```

### 示例2: 完整应用流程

```c
/* 初始化 */
bgs_init();

/* 播放音符 */
uint8_t note = 60;      /* C4 */
uint8_t velocity = 70;  /* 中等力度 */
uint8_t program = 0;    /* Program 0 */

/* 选择采样（可选，调试用） */
int sample_idx = bgs_select_sample_by_velocity(note, velocity, program);
if (sample_idx >= 0) {
    printf("Selected sample: %d\n", sample_idx);
}

/* 触发音符 */
bgs_note_on(note, velocity, program);

/* 读取音频数据 */
short audio_buffer[512];
while (bgs_read_callback(audio_buffer, note, 512, program)) {
    /* 播放 audio_buffer */
}

/* 停止音符 */
bgs_note_off(note, program);

/* 清理 */
bgs_deinit();
```

---

## 兼容性

### 向后兼容

**v1.0文件 → v2.0解析器**:
- ✅ 完全兼容
- 如果文件中 `min_vel=0, max_vel=127`，行为与v1.0完全一致
- 自动忽略力度值，选择第一个匹配音符范围的采样

**v2.0文件 → v1.0解析器**:
- ⚠️ 部分兼容
- 旧解析器会读取但忽略 `min_vel` 和 `max_vel` 字段
- 可能选择错误的采样（不考虑力度）

### 建议

- 新项目使用v2.0格式和解析器
- 支持力度分层的乐器建议至少2-3个力度层
- 鼓组可继续使用单层模式（全部设置为0-127）

---

## 调试日志

启用调试日志可以查看采样选择过程：

```c
/* 编译时定义 */
#define BG_LOG_LEVEL BG_LOG_LEVEL_DEBUG

/* 运行时日志输出示例 */
NoteOn: note=60, vel=30, program=0 -> sample=0
NoteOn: note=60, vel=70, program=0 -> sample=1
NoteOn: note=60, vel=110, program=0 -> sample=2
```

---

## 性能考虑

### 时间复杂度
- **O(N)**: N为Program中的采样数量
- 对于大多数乐器（<100个采样），性能影响可忽略
- 对于超大音色库（>500个采样），可考虑优化：
  - 使用二分查找（需按音符和力度排序）
  - 使用查找表（空间换时间）

### 内存占用
- 无新增内存占用
- 仅复用现有数据结构

---

## 测试建议

### 单元测试

```c
/* 测试1: 基本力度层选择 */
void test_velocity_layer_selection() {
    bgs_init();
    
    /* 测试低力度 */
    int idx = bgs_select_sample_by_velocity(60, 20, 0);
    assert(idx == 0); /* 应选择pp层 */
    
    /* 测试中等力度 */
    idx = bgs_select_sample_by_velocity(60, 60, 0);
    assert(idx == 1); /* 应选择mf层 */
    
    /* 测试高力度 */
    idx = bgs_select_sample_by_velocity(60, 100, 0);
    assert(idx == 2); /* 应选择ff层 */
    
    bgs_deinit();
}

/* 测试2: 边界情况 */
void test_edge_cases() {
    bgs_init();
    
    /* 测试力度边界 */
    assert(bgs_select_sample_by_velocity(60, 0, 0) == 0);
    assert(bgs_select_sample_by_velocity(60, 42, 0) == 0);
    assert(bgs_select_sample_by_velocity(60, 43, 0) == 1);
    assert(bgs_select_sample_by_velocity(60, 127, 0) == 2);
    
    /* 测试无匹配采样 */
    assert(bgs_select_sample_by_velocity(10, 50, 0) == -1);
    
    bgs_deinit();
}
```

### 集成测试

1. **使用v2.0编辑器创建测试文件**
   ```bash
   python bg_editor.py
   # 创建包含多个力度层的Program
   # 导出为 test_velocity.bgs
   ```

2. **在C代码中加载测试**
   ```c
   bgs_init();  /* 加载 test_velocity.bgs */
   
   /* 手动测试不同力度 */
   for (int vel = 0; vel <= 127; vel += 10) {
       int idx = bgs_select_sample_by_velocity(60, vel, 0);
       printf("Velocity %d → Sample %d\n", vel, idx);
   }
   
   bgs_deinit();
   ```

---

## 已知问题和限制

### 当前限制

1. **采样选择是静态的**
   - 在NoteOn时选择采样后不能动态切换
   - 未来可考虑支持力度交叉淡化（crossfade）

2. **没有力度曲线映射**
   - 直接使用MIDI力度值
   - 可在上层添加力度曲线（如指数曲线）

3. **单采样播放**
   - 每次只能选择一个采样
   - 不支持多采样混合

### 未来改进

1. **力度交叉淡化**
   ```c
   /* 在力度层边界处混合两个采样 */
   if (velocity near boundary) {
       mix_two_samples(sample1, sample2, ratio);
   }
   ```

2. **力度曲线**
   ```c
   /* 应用非线性力度响应 */
   float mapped_vel = apply_velocity_curve(velocity, curve_type);
   ```

3. **采样缓存**
   ```c
   /* 缓存最近使用的采样索引 */
   static int last_sample_cache[128];
   ```

---

## 相关文档

- [BGS格式协议规范 v2.0](../../soundbank2/BGS_FORMAT_SPEC_v2.md)
- [BG音色编辑器使用指南](../../soundbank2/EDITOR_GUIDE.md)
- [力度层智能分割功能说明](../../soundbank2/力度层智能分割功能说明.md)

---

## 维护者

BanGT Project Team

## 最后更新

2025-12-10
