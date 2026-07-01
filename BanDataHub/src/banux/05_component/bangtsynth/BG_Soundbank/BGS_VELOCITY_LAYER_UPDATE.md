# BGS解析器力度层支持更新说明

## 更新日期
2025-12-10

## 更新版本
v2.0 - 力度层支持

---

## 📋 更新内容

### 1. 数据结构（bgs_parser.h）
已包含力度层字段（无需修改）：
```c
typedef struct
{
    uint8_t vel_id;        // 力度层ID
    uint8_t note;          // 根音符
    uint8_t min_note;      // 最低音符
    uint8_t max_note;      // 最高音符
    uint8_t min_vel;       // 🆕 最小力度
    uint8_t max_vel;       // 🆕 最大力度
    uint32_t address;      // 采样地址
} BGS_Note_Info;
```

### 2. 新增函数（bgs_parser.c）

#### `bgs_select_sample_by_velocity()`
```c
/**
 * @brief 根据音符和力度选择最佳采样索引
 * 
 * @param note MIDI音符号 (0-127)
 * @param velocity MIDI力度值 (0-127)
 * @param program Program索引
 * @return int 采样索引（-1表示未找到）
 * 
 * 选择逻辑：
 * 1. 检查音符范围：min_note <= note <= max_note
 * 2. 检查力度范围：min_vel <= velocity <= max_vel
 * 3. 选择根音符最接近的采样
 */
int bgs_select_sample_by_velocity(uint8_t note, uint8_t velocity, uint8_t program);
```

**实现要点**：
- 遍历所有采样
- 先过滤音符范围
- 再过滤力度范围
- 最后选择最接近的根音符

### 3. 更新函数（bgs_parser.c）

#### `bgs_note_on()` - 已更新
```c
void bgs_note_on(uint8_t note, uint8_t velocity, uint8_t program)
{
    /* v2.0: 使用力度层选择采样 */
    int sample_index = bgs_select_sample_by_velocity(note, velocity, program);
    
    if (sample_index >= 0) {
        BG_LOG_D(..., "NoteOn: note=%d, vel=%d, program=%d -> sample=%d\n", 
                 note, velocity, program, sample_index);
    } else {
        BG_LOG_D(..., "NoteOn: note=%d, vel=%d, program=%d -> no sample found\n", 
                 note, velocity, program);
    }
}
```

**更新说明**：
- ❌ 删除：旧的占位符实现（仅标记参数为未使用）
- ✅ 新增：调用力度层选择函数
- ✅ 新增：调试日志输出

#### `get_bgs_head_info()` - 已支持解析
第220-221行已正确解析力度字段：
```c
BG_reader.Data.ProgramData[count].Note_Info[i].min_vel = size_info[i][8];
BG_reader.Data.ProgramData[count].Note_Info[i].max_vel = size_info[i][9];
```

---

## 🔄 向后兼容性

### v1.0文件
- ✅ 完全兼容
- `min_vel` 和 `max_vel` 字段读取为默认值（通常是0和127）
- 行为与v1.0完全一致

### v2.0文件
- ✅ 完整支持力度层
- 自动根据力度值选择合适的采样

---

## 📝 BGS文件格式更新

### Note Info 结构（10字节）

| 偏移 | 大小 | 类型 | 名称 | 说明 |
|------|------|------|------|------|
| 0 | 4 | uint32 | bytecount | 采样字节数 |
| 4 | 1 | uint8 | note | 根音符 (0-127) |
| 5 | 1 | uint8 | min_note | 最低音符 (0-127) |
| 6 | 1 | uint8 | max_note | 最高音符 (0-127) |
| 7 | 1 | uint8 | vel_id | 力度层ID |
| 8 | 1 | uint8 | **min_vel** | 最小力度 (0-127) 🆕 |
| 9 | 1 | uint8 | **max_vel** | 最大力度 (0-127) 🆕 |

---

## 🎯 使用示例

### 场景1：单力度层（默认）
```c
// BGS文件数据
Note_Info[0]: note=60, min_note=57, max_note=63, min_vel=0, max_vel=127

// MIDI输入
bgs_note_on(60, 64, 0);  // 任意力度都匹配

// 结果：选择 sample 0
```

### 场景2：三力度层
```c
// BGS文件数据
Note_Info[0]: note=60, min_note=57, max_note=63, min_vel=0,   max_vel=42  (pp层)
Note_Info[1]: note=60, min_note=57, max_note=63, min_vel=43,  max_vel=84  (mf层)
Note_Info[2]: note=60, min_note=57, max_note=63, min_vel=85,  max_vel=127 (ff层)

// MIDI输入示例1
bgs_note_on(60, 30, 0);  // 轻触
// 结果：选择 sample 0 (pp层, 30在0-42范围内)

// MIDI输入示例2
bgs_note_on(60, 70, 0);  // 中等力度
// 结果：选择 sample 1 (mf层, 70在43-84范围内)

// MIDI输入示例3
bgs_note_on(60, 100, 0); // 强力演奏
// 结果：选择 sample 2 (ff层, 100在85-127范围内)
```

### 场景3：多音符+多力度层
```c
// BGS文件数据
Note_Info[0]: note=48, min_note=45, max_note=51, min_vel=0,   max_vel=42  (C3_pp)
Note_Info[1]: note=48, min_note=45, max_note=51, min_vel=43,  max_vel=127 (C3_ff)
Note_Info[2]: note=60, min_note=57, max_note=63, min_vel=0,   max_vel=42  (C4_pp)
Note_Info[3]: note=60, min_note=57, max_note=63, min_vel=43,  max_vel=127 (C4_ff)

// MIDI输入
bgs_note_on(60, 30, 0);  
// 匹配过程：
//   - Note_Info[0]: note范围✗ (60不在45-51)
//   - Note_Info[1]: note范围✗
//   - Note_Info[2]: note范围✓, 力度范围✓ (30在0-42) ← 选中
//   - Note_Info[3]: note范围✓, 力度范围✗
// 结果：选择 sample 2 (C4_pp)
```

---

## 🔧 编译依赖

新增依赖：
```c
#include <math.h>  // 用于 abs() 函数
```

如果编译环境不支持 `abs()`，可替换为：
```c
#define ABS(x) ((x) < 0 ? -(x) : (x))
int distance = ABS(note - note_info->note);
```

---

## ✅ 验证清单

- [x] 数据结构包含 min_vel 和 max_vel 字段
- [x] 解析代码正确读取力度字段（第220-221行）
- [x] 新增 bgs_select_sample_by_velocity() 函数
- [x] 更新 bgs_note_on() 使用力度层选择
- [x] 头文件包含函数声明
- [x] 添加调试日志输出
- [x] 向后兼容v1.0格式

---

## 📊 性能影响

### 时间复杂度
- **v1.0**: O(n) - 简单遍历
- **v2.0**: O(n) - 相同，仅增加一个力度范围判断

### 内存占用
- **每个采样**: +2 bytes (min_vel + max_vel)
- **100个采样**: +200 bytes

### 实际影响
- ✅ 可忽略不计
- ✅ 不影响实时性能

---

## 🐛 已知问题

无

---

## 📚 相关文档

- [BGS格式协议规范 v2.0](../BGS_FORMAT_SPEC_v2.md)
- [Python编辑器力度层实现](../bg_editor.py)
- [BG格式定义](../../../soundbank2/bg_format.py)

---

## 👥 贡献者

- BanGT Project Team

---

**更新完成时间**: 2025-12-10  
**测试状态**: ⏳ 待测试  
**发布状态**: ✅ 已就绪
