# 效果图节点参数调节功能 - 使用指南

## 📌 更新说明 (V1.2.0)

基于现有效果图系统，扩展了Shell命令，增加了以下新功能：

- **参数范围校验**: 设置参数时自动校验是否在有效范围内
- **参数帮助**: `graph params <id>` 命令显示节点支持的所有参数及范围
- **快照管理**: 保存/恢复效果图状态，支持4个快照槽位
- **批量操作**: 一键启用/禁用/旁路所有效果

## 🎯 核心功能

### 1. 节点ID索引
- 每个效果节点在效果图中有唯一的ID (0, 1, 2, ...)
- 可以通过ID或名称访问节点
- `graph list` 命令显示所有节点及其ID

### 2. 快捷命令 `fx`
新增 `fx` 命令用于快速参数调节：
```bash
fx <id>              # 显示节点所有参数
fx <id> <param>      # 获取指定参数
fx <id> <param> <val># 设置参数值
```

### 3. 增强的 `graph` 命令
- `graph get <id|name> [param]` - 获取参数
- `graph set <id|name> <param> <val>` - 设置参数 (带校验)
- `graph params <id|name>` - 显示可用参数及范围
- 所有命令支持ID和名称两种方式

### 4. 快照管理 (新功能)
```bash
graph snapshot save <slot> [name]  # 保存当前状态
graph snapshot load <slot>         # 加载状态
graph snapshot list                # 列出所有快照
```

### 5. 批量操作 (新功能)
```bash
graph allfx <on|off>       # 批量启用/禁用所有效果
graph allbypass <on|off>   # 批量旁路所有效果
```

---

## 📋 命令参考

### graph list - 列出所有节点
```bash
$ graph list

===== Graph Nodes [8/16] =====
ID  Name            Type        Status
--- --------------- ----------- --------
 0  adc0            ADC0        ON 
 1  adc1            ADC1        ON 
 2  mixer           MIXER       ON 
 3  drc             DRC         ON 
 4  eq              EQ          ON 
 5  reverb          REVERB      ON  [BYP]
 6  gain            GAIN        ON 
 7  dac0            DAC0        ON 
===============================
Use: graph get <id> to show node params
```

### graph params - 显示可用参数 (新)
```bash
$ graph params 3

=== Node[3]: drc ===
Available parameters:
  threshold     [-60~0] dB
  ratio         [1~20] 
  attack        [1~500] ms
  release       [10~2000] ms
========================
```

### graph get - 获取参数
```bash
# 获取节点所有参数
$ graph get 3

=== Node[3]: drc ===
Status: Enabled
Type: DRC
  threshold = -20 dB
  ratio     = 4
  attack    = 10 ms
  release   = 100 ms
========================

# 获取单个参数
$ graph get 3 threshold
[Node 3] threshold = -20

# 也可用名称
$ graph get drc threshold
[Node 3] threshold = -20
```

### graph set - 设置参数 (带校验)
```bash
# 通过ID设置
$ graph set 3 threshold -25
[Node 3] threshold = -25

# 设置超范围值时会警告
$ graph set 3 threshold -100
WARN: threshold out of range [-60~0]dB
[Node 3] threshold = -100

# 设置无效参数时会提示可用参数
$ graph set 3 invalid 50
ERROR: Unknown param 'invalid' for node type
Available parameters:
  threshold     [-60~0] dB
  ratio         [1~20] 
  attack        [1~500] ms
  release       [10~2000] ms
```

### graph snapshot - 快照管理 (新)
```bash
# 保存当前状态
$ graph snapshot save 0 clean
Snapshot saved to slot 0: 'clean' (8 nodes)

# 调节参数...
$ fx 3 threshold -30
$ fx 5 wet 80

# 恢复之前的状态
$ graph snapshot load 0
Snapshot 'clean' loaded from slot 0

# 列出所有快照
$ graph snapshot list

===== Snapshots =====
Slot  Name            Nodes
----- --------------- -----
[0]   clean           8
[1]   (empty)
[2]   (empty)
[3]   (empty)
=====================
```

### graph allfx / allbypass - 批量操作 (新)
```bash
# 关闭所有效果 (静音调试)
$ graph allfx off
All effects disabled (5 nodes)

# 重新启用
$ graph allfx on
All effects enabled (5 nodes)

# 旁路所有效果 (干声直通)
$ graph allbypass on
All effects bypass ON (5 nodes)

$ graph allbypass off
All effects bypass OFF (5 nodes)
```

### fx - 快捷命令
```bash
# 查看节点参数
$ fx 3

=== Node[3]: drc ===
Status: Enabled
Type: DRC
  threshold = -20 dB
  ratio     = 4
  attack    = 10 ms
  release   = 100 ms
========================

# 获取参数
$ fx 3 threshold
[3] threshold = -20

# 设置参数
$ fx 3 threshold -25
[Node 3] threshold = -25

# 连续调节
$ fx 3 attack 5
$ fx 3 release 200
```

### graph node - 启用/禁用节点
```bash
# 通过ID
$ graph node 5 off
Node[5] 'reverb' disabled

# 通过名称
$ graph node reverb on
Node[5] 'reverb' enabled
```

### graph bypass - 旁路节点
```bash
$ graph bypass 5 on
Node[5] 'reverb' bypass ON

$ graph bypass 5 off
Node[5] 'reverb' bypass OFF
```

---

## 📊 支持的节点类型和参数

### DRC (动态范围压缩)
| 参数 | 范围 | 单位 | 说明 |
|------|------|------|------|
| threshold | -60~0 | dB | 压缩阈值 |
| ratio | 1~20 | - | 压缩比 |
| attack | 1~500 | ms | 启动时间 |
| release | 10~2000 | ms | 释放时间 |

```bash
fx 3 threshold -20
fx 3 ratio 4
fx 3 attack 10
fx 3 release 100
```

### EQ (均衡器)
| 参数 | 范围 | 单位 | 说明 |
|------|------|------|------|
| band0~band9 | -12~+12 | dB | 各频段增益 |

```bash
fx 4 band0 3     # 低频 +3dB
fx 4 band5 -2    # 中频 -2dB
fx 4 band9 5     # 高频 +5dB
```

### Reverb (混响)
| 参数 | 范围 | 单位 | 说明 |
|------|------|------|------|
| room | 0~100 | % | 房间大小 |
| damp | 0~100 | % | 阻尼 |
| wet | 0~100 | % | 干湿比 |

```bash
fx 5 room 50
fx 5 damp 30
fx 5 wet 40
```

### Delay (延迟)
| 参数 | 范围 | 单位 | 说明 |
|------|------|------|------|
| time | 10~1000 | ms | 延迟时间 |
| feedback | 0~100 | % | 反馈量 |
| wet | 0~100 | % | 干湿比 |

```bash
fx 6 time 250
fx 6 feedback 50
fx 6 wet 30
```

### Gain (增益)
| 参数 | 范围 | 单位 | 说明 |
|------|------|------|------|
| gain | -60~+20 | dB | 增益值 |

```bash
fx 7 gain -6
```

### Expander (扩展器)
| 参数 | 范围 | 单位 | 说明 |
|------|------|------|------|
| threshold | -80~0 | dB | 阈值 |
| ratio | 1~10 | - | 扩展比 |

```bash
fx 8 threshold -60
fx 8 ratio 2
```

### Mixer (混音器)
| 参数 | 范围 | 单位 | 说明 |
|------|------|------|------|
| in0_gain~in3_gain | -60~+20 | dB | 各输入增益 |

```bash
fx 2 in0_gain 0
fx 2 in1_gain -6
```

---

## 🔧 典型使用场景

### 场景1: 调节DRC压缩效果
```bash
# 1. 查看当前效果图
graph list

# 2. 找到DRC节点ID (假设是3)
# 3. 查看当前DRC参数
fx 3

# 4. 调节参数
fx 3 threshold -25    # 降低阈值，更容易压缩
fx 3 ratio 6          # 增加压缩比
fx 3 attack 5         # 加快启动
fx 3 release 150      # 调整释放

# 5. 验证设置
fx 3
```

### 场景2: 调节EQ均衡
```bash
# 增强低频
fx 4 band0 6
fx 4 band1 3

# 清晰人声
fx 4 band4 2
fx 4 band5 3

# 降低刺耳高频
fx 4 band9 -3
```

### 场景3: 快速A/B对比
```bash
# 启用效果
graph node 5 on

# 旁路效果 (直通)
graph bypass 5 on

# 关闭旁路 (恢复效果)
graph bypass 5 off

# 禁用效果
graph node 5 off
```

### 场景4: 切换预设
```bash
# 查看可用预设
graph preset

# 切换到预设1
graph preset 1
```

### 场景5: 使用快照进行A/B对比 (新)
```bash
# 保存当前状态作为参考
graph snapshot save 0 reference

# 调节参数
fx 3 threshold -30
fx 5 wet 80

# 保存调节后的状态
graph snapshot save 1 bright

# A/B 对比
graph snapshot load 0    # 加载参考状态
graph snapshot load 1    # 加载调节后状态
```

### 场景6: 静音调试 (新)
```bash
# 关闭所有效果，只保留直通
graph allfx off

# 逐个启用效果，定位问题
graph node drc on
graph node eq on
graph node reverb on

# 或者使用旁路方式
graph allfx on
graph allbypass on       # 全部直通
graph bypass 3 off       # 只启用DRC
```

---

## 📁 代码文件

| 文件 | 位置 | 说明 |
|------|------|------|
| `shell_cmd_graph.h` | `05_component/effect_graph/` | 命令接口声明 |
| `shell_cmd_graph.c` | `05_component/effect_graph/` | 命令实现 |
| `effect_graph.h` | `05_component/effect_graph/` | 效果图核心定义 |
| `effect_graph.c` | `05_component/effect_graph/` | 效果图核心实现 |

---

## 🔗 命令注册

在Shell系统初始化时注册命令：

```c
#include "shell_cmd_graph.h"

void Shell_Init(void)
{
    // ... 其他初始化
    
    // 注册 graph 命令
    ShellCmdGraph_Register();
    
    // 在命令表中添加:
    // { "graph", ShellCmdGraph_Execute },
    // { "fx", ShellCmdFx_Execute },
}
```

---

## ✅ 更新总结 (V1.2.0)

### 新增功能
1. **参数范围校验** - 设置参数时自动校验并提示有效范围
2. **参数帮助命令** - `graph params <id>` 显示可用参数及范围
3. **快照管理** - 4个快照槽位，支持保存/恢复状态
4. **批量操作** - `allfx`/`allbypass` 一键操作所有效果

### 已有功能
5. **ID索引支持** - 所有命令支持通过数字ID访问节点
6. **fx快捷命令** - 简化参数调节流程
7. **统一参数接口** - `SetNodeParam`/`GetNodeParam` 通用函数
8. **增强的list输出** - 表格式显示，清晰展示ID
9. **完善的参数打印** - `graph get <id>` 显示节点所有参数
10. **兼容旧命令** - `graph param` 保留兼容

### 典型工作流
```bash
# 1. 查看节点列表
graph list

# 2. 查看节点可用参数
graph params 3

# 3. 调节参数
fx 3 threshold -25

# 4. 保存满意的状态
graph snapshot save 0 "perfect"

# 5. 继续调试，随时恢复
graph snapshot load 0
```
