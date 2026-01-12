# 效果图VFS系统指南

## 概述

效果图VFS（Virtual File System）模块将音频效果图的参数映射到Shell文件系统中，使得可以像操作文件一样访问和修改效果器参数。

## 目录结构

```
/
├── bin/                    # 系统命令
├── driver/                 # 硬件驱动
└── audio/                  # 音频效果图 ★新增★
    ├── graph0/             # 默认效果图
    │   ├── info            # 图信息（只读）
    │   ├── preset          # 当前预设ID（读写）
    │   ├── node_count      # 节点数量（只读）
    │   └── nodes/          # 节点目录
    │       ├── 0_adc0/     # 节点：<id>_<name>
    │       │   ├── enabled # 启用状态（0/1）
    │       │   ├── bypass  # 旁路状态（0/1）
    │       │   └── type    # 节点类型（只读）
    │       ├── 3_drc/      # DRC效果器节点
    │       │   ├── enabled
    │       │   ├── bypass
    │       │   ├── type
    │       │   ├── threshold  # DRC参数
    │       │   ├── ratio
    │       │   ├── attack
    │       │   └── release
    │       ├── 5_reverb/   # 混响效果器节点
    │       │   ├── enabled
    │       │   ├── bypass
    │       │   ├── type
    │       │   ├── room    # 混响参数
    │       │   ├── damp
    │       │   └── wet
    │       └── ...
    └── graph1/             # 可动态创建的第二个图
```

## 命令使用

### audio 命令 - 管理效果图

```bash
# 列出所有已挂载的效果图
$ audio list

# 创建新效果图
$ audio create <name> [preset]
$ audio create myGraph 0      # 使用预设0创建

# 删除效果图
$ audio delete <name>

# 重载效果图（刷新VFS结构）
$ audio reload <name>

# 显示效果图信息
$ audio info <name>
```

### VFS命令 - 浏览和操作参数

```bash
# 进入音频目录
$ cd /audio
$ ls
graph0/

# 进入效果图
$ cd graph0
$ ls
info    preset    node_count    nodes/

# 查看图信息
$ cat info
name=graph0 nodes=8 state=2

# 查看/修改预设
$ cat preset
0
$ echo 1 > preset    # 切换到预设1

# 进入节点目录
$ cd nodes
$ ls
0_adc0/    1_usb_in/    3_drc/    5_reverb/    ...

# 进入具体节点
$ cd 3_drc
$ ls
enabled    bypass    type    threshold    ratio    attack    release

# 读取参数
$ cat threshold
-20
$ cat enabled
1

# 修改参数
$ echo -25 > threshold
$ echo 0 > enabled      # 禁用节点
```

### 快捷路径访问

```bash
# 直接访问任意参数
$ cat /audio/graph0/nodes/3_drc/threshold
$ echo -30 > /audio/graph0/nodes/5_reverb/room

# 使用pwd查看当前位置
$ pwd
/audio/graph0/nodes/3_drc
```

## 参数说明

### 通用参数（所有节点都有）

| 参数 | 类型 | 说明 |
|------|------|------|
| enabled | 读写 | 节点启用状态，0=禁用，1=启用 |
| bypass | 读写 | 节点旁路状态，0=正常处理，1=信号直通 |
| type | 只读 | 节点类型名称 |

### DRC效果器参数

| 参数 | 范围 | 说明 |
|------|------|------|
| threshold | -60~0 | 阈值 (dB) |
| ratio | 1~20 | 压缩比 |
| attack | 1~500 | 起音时间 (ms) |
| release | 10~2000 | 释放时间 (ms) |

### Reverb效果器参数

| 参数 | 范围 | 说明 |
|------|------|------|
| room | 0~100 | 房间大小 (%) |
| damp | 0~100 | 阻尼 (%) |
| wet | 0~100 | 干湿比 (%) |

### Delay效果器参数

| 参数 | 范围 | 说明 |
|------|------|------|
| time | 10~1000 | 延迟时间 (ms) |
| feedback | 0~100 | 反馈量 (%) |
| wet | 0~100 | 干湿比 (%) |

### Gain效果器参数

| 参数 | 范围 | 说明 |
|------|------|------|
| gain | -60~+20 | 增益 (dB) |

### Expander效果器参数

| 参数 | 范围 | 说明 |
|------|------|------|
| threshold | -80~0 | 阈值 (dB) |
| ratio | 1~10 | 扩展比 |

### EQ效果器参数

| 参数 | 范围 | 说明 |
|------|------|------|
| band0~band9 | -12~+12 | 各频段增益 (dB) |

## 多图支持

系统支持同时挂载最多4个效果图实例：

```bash
# 创建多个效果图
$ audio create graph0 0   # 主效果图
$ audio create graph1 1   # 备用效果图
$ audio create monitor 2  # 监听效果图

# 独立控制各图
$ echo -20 > /audio/graph0/nodes/3_drc/threshold
$ echo -15 > /audio/graph1/nodes/3_drc/threshold

# 列出所有图
$ audio list
===== Effect Graphs =====
  /audio/graph0       nodes=8
  /audio/graph1       nodes=8
  /audio/monitor      nodes=5
=========================
Total: 3 graph(s)
```

## 典型使用场景

### 场景1：实时调试DRC效果

```bash
$ cd /audio/graph0/nodes/3_drc
$ cat threshold
-20
$ echo -25 > threshold   # 调低阈值
$ echo -30 > threshold   # 继续调整
$ cat ratio
4
$ echo 6 > ratio         # 增加压缩比
```

### 场景2：切换预设并微调

```bash
$ echo 1 > /audio/graph0/preset   # 切换到预设1
$ cat /audio/graph0/nodes/5_reverb/room
50
$ echo 70 > /audio/graph0/nodes/5_reverb/room  # 增大混响
```

### 场景3：批量禁用效果器

```bash
# 禁用DRC
$ echo 0 > /audio/graph0/nodes/3_drc/enabled

# 旁路混响
$ echo 1 > /audio/graph0/nodes/5_reverb/bypass
```

### 场景4：使用脚本批量配置

```bash
# 保存当前配置到文件（伪代码，需要shell支持）
# cat /audio/graph0/nodes/3_drc/* > drc_config.txt

# 批量设置
$ echo -25 > /audio/graph0/nodes/3_drc/threshold
$ echo 6 > /audio/graph0/nodes/3_drc/ratio
$ echo 10 > /audio/graph0/nodes/3_drc/attack
$ echo 100 > /audio/graph0/nodes/3_drc/release
```

## 注意事项

1. **参数生效**: 参数修改后立即生效，无需手动刷新
2. **范围校验**: 参数设置会进行范围校验，超出范围会显示警告
3. **持久化**: 参数修改不会自动保存到Flash，需要使用其他命令保存
4. **图实例**: 当前简化实现共享同一个效果图实例，多图需要扩展
5. **echo命令**: 需要Shell系统支持echo重定向功能

## 文件列表

| 文件 | 说明 |
|------|------|
| effect_graph_vfs.h | 效果图VFS头文件 |
| effect_graph_vfs.c | 效果图VFS实现 |
| shell_cmd_audio_vfs.h | audio命令头文件 |
| shell_cmd_audio_vfs.c | audio命令实现 |

## 初始化流程

系统启动时自动执行：
1. `EffectGraphVfs_Init()` - 创建 `/audio` 目录
2. `EffectGraphVfs_MountDefault()` - 挂载默认效果图为 `graph0`
3. `ShellCmdAudioVfs_Register()` - 注册 `audio` Shell命令

```c
// 在 drv_init.c 中
ret = EffectGraphVfs_MountDefault();
ShellCmdAudioVfs_Register();
```

## 扩展开发

### 添加新的效果器参数

1. 在 `effect_graph_vfs.c` 中添加参数读写回调函数
2. 在 `CreateNodeParams()` 函数中注册新参数

```c
// 示例：添加新参数
static int MyParamGet(char *buf, uint16_t maxLen, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    return snprintf(buf, maxLen, "%d", node->params.xxx.value);
}

static int MyParamSet(const char *value, void *userData)
{
    EffectNode_t *node = (EffectNode_t *)userData;
    node->params.xxx.value = atoi(value);
    return 0;
}

// 在CreateNodeParams中注册
case NODE_TYPE_EFFECT_XXX:
    Vfs_CreateParam(nodeDir, "value", "Description", 
                    MyParamGet, MyParamSet, node);
    break;
```
