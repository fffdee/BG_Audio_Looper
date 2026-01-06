# 效果图自动挂载指南

## 概述

系统现在支持在音频系统初始化后自动挂载默认效果图到 `/audio/graph0`，无需手动操作。

## 工作流程

### 1. 系统启动阶段

```
系统启动
  ↓
VFS初始化 (drv_init.c)
  ↓
创建 /audio 目录 (EffectGraphVfs_MountDefault)
  ↓
检查效果图是否就绪
  ├─ 未就绪 → 静默返回，等待后续挂载
  └─ 已就绪 → 立即挂载为 graph0
```

### 2. 音频系统初始化阶段

```
音频系统启动 (bg_audio_io_manager.c)
  ↓
EffectGraph_Init() - 初始化效果图核心
  ↓
EffectGraphConfig_LoadPreset() - 加载默认预设
  ↓
EffectGraphVfs_TryAutoMount() - 自动挂载到VFS ★
  ↓
Shell命令注册
```

### 3. 挂载完成

现在用户可以通过命令行访问效果图：

```bash
$ cd /audio
$ ls
graph0/

$ cd graph0
$ ls
info    preset    node_count    nodes/

$ cd nodes
$ ls
0_adc0/    1_adc1/    2_mixer/    3_drc/    ...
```

## 代码修改位置

### 1. effect_graph_vfs.c

```c
/**
 * @brief 挂载默认效果图（系统启动时调用）
 * @note 如果效果图未初始化，会静默返回OK，稍后可重试
 */
GraphVfsError_t EffectGraphVfs_MountDefault(void)
{
    // ...
    
    /* 获取默认效果图实例 */
    graph = EffectGraph_GetInstance();
    if (!graph) {
        /* 效果图未初始化，静默返回，稍后会自动重试 */
        DBG("[GraphVfs] Graph not ready yet, will retry later\n");
        return GRAPH_VFS_OK;  /* 不报错，允许延迟挂载 */
    }
    
    // ...
}

/**
 * @brief 尝试自动挂载（音频系统初始化后调用）
 */
GraphVfsError_t EffectGraphVfs_TryAutoMount(void)
{
    /* 检查是否已挂载 */
    if (EffectGraphVfs_ListGraphs(NULL) > 0) {
        return GRAPH_VFS_OK;
    }
    
    /* 尝试挂载 */
    graph = EffectGraph_GetInstance();
    if (!graph) {
        return GRAPH_VFS_ERR_NOT_FOUND;
    }
    
    if (!EffectGraphVfs_Mount("graph0", graph)) {
        return GRAPH_VFS_ERR_NO_MEMORY;
    }
    
    return GRAPH_VFS_OK;
}
```

### 2. bg_audio_io_manager.c

```c
// 1. 初始化 Effect Graph 核心模块
if (EffectGraph_Init() != 0) {
    DBG("[Audio] ERROR: Effect Graph Init failed!\n");
    return;
}

// 2. 加载默认预设
if (EffectGraphConfig_LoadPreset(GRAPH_PRESET_DEFAULT) != 0) {
    DBG("[Audio] ERROR: Effect Graph Load Preset failed!\n");
    return;
}

// 3. 自动挂载效果图到VFS ★ 新增
EffectGraphVfs_TryAutoMount();

// 4. 挂接音频设备回调
SetupEffectGraphCallbacks();
```

### 3. drv_init.c

```c
void Drv_Init(void)
{
    // ...
    
    /* VFS初始化 */
    Vfs_Init();
    ShellFs_Init();
    
    /* 挂载效果图VFS (创建/audio目录，延迟挂载graph0) */
    EffectGraphVfs_MountDefault();  // 静默处理未就绪情况
    
    /* 注册audio命令 */
    ShellCmdAudioVfs_Register();
    
    // ...
}
```

## 用户体验

### 情况1：正常启动

系统启动 → 音频初始化 → 自动挂载 → 用户可立即使用

```bash
$ cd /audio/graph0
$ ls
info    preset    node_count    nodes/
```

### 情况2：手动挂载

如果自动挂载失败或需要重新挂载：

```bash
$ audio list
===== Effect Graphs =====
  (no graphs mounted)
  Use 'audio mount' to mount default graph
=========================

$ audio mount
Default graph mounted at /audio/graph0
Use 'cd /audio/graph0' to access it

$ cd /audio/graph0
$ ls
info    preset    node_count    nodes/
```

## 调试日志

成功挂载时的日志输出：

```
[GraphVfs] /audio created successfully
[GraphVfs] Graph not ready yet, will retry later
[Audio] Initializing Effect Graph...
[Audio] Effect Graph initialized successfully
[GraphVfs] Auto-mounted graph0
[GraphVfs] Graph 'graph0' mounted at /audio/graph0 (8 nodes)
```

## API参考

### EffectGraphVfs_MountDefault()

- **调用时机**：VFS初始化后立即调用（drv_init.c）
- **作用**：创建 `/audio` 目录，尝试挂载默认图
- **返回值**：始终返回 `GRAPH_VFS_OK`（即使图未就绪）

### EffectGraphVfs_TryAutoMount()

- **调用时机**：音频系统初始化后调用（bg_audio_io_manager.c）
- **作用**：确保默认图已挂载
- **返回值**：
  - `GRAPH_VFS_OK` - 已挂载或挂载成功
  - `GRAPH_VFS_ERR_NOT_FOUND` - 效果图未初始化
  - `GRAPH_VFS_ERR_NO_MEMORY` - 挂载失败

### ShellCmdAudioVfs_CheckAutoMount()

- **调用时机**：可在任何时候调用（可选）
- **作用**：检查并自动挂载
- **返回值**：void

## 测试用例

### 测试1：启动后检查

```bash
# 系统启动后
$ cd /audio
$ ls
graph0/

$ cd graph0/nodes
$ ls
0_adc0/  1_adc1/  2_mixer/  3_drc/  4_eq/  5_reverb/  6_dac0/  7_usb_out/

# ✅ 自动挂载成功
```

### 测试2：参数访问

```bash
$ cd /audio/graph0/nodes/3_drc
$ cat threshold
-20

$ cat enabled
1

$ cat type
drc

# ✅ 参数可正常访问
```

### 测试3：列出效果图

```bash
$ audio list
===== Effect Graphs =====
  /audio/graph0       nodes=8
=========================
Total: 1 graph(s)

# ✅ 列表显示正常
```

### 测试4：图信息

```bash
$ audio info graph0
=== Graph: graph0 ===
name=graph0 nodes=8 sr=44100
Preset: 0
Node count: 8
==================

# ✅ 信息显示正常
```

## 故障排除

### 问题1：/audio目录为空

**症状**：
```bash
$ cd /audio
$ ls
(empty)
```

**原因**：效果图尚未初始化

**解决**：
```bash
$ audio mount
Default graph mounted at /audio/graph0
```

### 问题2：audio命令不存在

**症状**：
```bash
$ audio list
ERROR: Unknown command 'audio'
```

**原因**：Shell命令未注册

**解决**：检查 `drv_init.c` 中是否调用了 `ShellCmdAudioVfs_Register()`

### 问题3：挂载失败

**症状**：
```bash
$ audio mount
ERROR: Failed to mount graph
```

**原因**：内存不足或VFS问题

**解决**：
1. 检查VFS初始化是否成功
2. 检查内存使用情况
3. 查看调试日志

## 总结

自动挂载功能确保了用户在系统启动后可以立即通过命令行访问效果图参数，无需手动干预。系统采用两阶段挂载策略：

1. **VFS初始化阶段**：创建 `/audio` 目录，尝试挂载（静默失败）
2. **音频初始化阶段**：自动挂载效果图到 `graph0`

这种设计既保证了启动顺序的灵活性，又提供了良好的用户体验。
