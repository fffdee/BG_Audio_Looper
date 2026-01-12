# 自动挂载调试指南

## 问题现象

```bash
$ ls
driver    bin
audio

$ ls audio 
(空目录)

$ audio mount
Invalid option: mount
Use 'audio' to see options
```

## 调试步骤

### 1. 检查调试日志

重新编译并运行系统，查看启动日志中是否有以下信息：

#### VFS初始化阶段（drv_init.c）
```
[GraphVfs] Init: Start
[GraphVfs] Init: Getting VFS root...
[GraphVfs] Init: Creating /audio directory...
[GraphVfs] /audio created successfully at 0xXXXXXXXX
```

#### 音频系统初始化阶段（bg_audio_io_manager.c）
```
[Audio] Initializing Effect Graph...
[Audio] Effect Graph initialized successfully
[GraphVfs] TryAutoMount: Start
[GraphVfs] TryAutoMount: Current graph count = 0
[GraphVfs] TryAutoMount: Getting graph instance...
[GraphVfs] TryAutoMount: Graph instance OK, node_count=8
[GraphVfs] TryAutoMount: Mounting as graph0...
[GraphVfs] Mount: Start mounting 'graph0'
[GraphVfs] Mount: Checking if 'graph0' already exists...
[GraphVfs] Mount: Finding free handle slot...
[GraphVfs] Mount: Creating directory /audio/graph0
[GraphVfs] Graph 'graph0' mounted at /audio/graph0 (8 nodes)
[GraphVfs] TryAutoMount: SUCCESS - graph0 mounted with 8 nodes
```

### 2. 常见问题诊断

#### 问题A：/audio 目录未创建

**日志特征**：
```
[GraphVfs] ERROR: VFS not initialized!
```

**原因**：VFS系统未初始化

**解决**：检查 `drv_init.c` 中是否调用了：
```c
Vfs_Init();
ShellFs_Init();
EffectGraphVfs_MountDefault();
```

#### 问题B：效果图未初始化

**日志特征**：
```
[GraphVfs] TryAutoMount: ERROR - Graph instance is NULL!
```

**原因**：`EffectGraph_Init()` 未被调用或失败

**解决**：检查 `bg_audio_io_manager.c` 中的音频初始化流程

#### 问题C：挂载失败

**日志特征**：
```
[GraphVfs] Mount: ERROR - Failed to create /audio/graph0
```

**原因**：VFS内存不足或节点创建失败

**解决**：
1. 检查VFS配置的最大节点数
2. 检查内存使用情况

#### 问题D：audio命令无法识别

**日志特征**：
```
Invalid option: mount
```

**原因**：Shell命令注册问题

**检查清单**：
1. `drv_init.c` 中是否调用了 `ShellCmdAudioVfs_Register()`
2. `bg_shell_commands.c` 中是否包含了 `shell_cmd_audio_vfs.h`
3. `Shell_RegisterAllModules()` 中是否调用了注册函数

### 3. 手动测试步骤

#### 测试1：VFS初始化
```bash
$ ls /
audio/  driver/  bin/
```
✅ 如果能看到 `audio/`，说明VFS初始化成功

#### 测试2：效果图状态
```bash
$ audio list
===== Effect Graphs =====
  (no graphs mounted)
  Use 'audio mount' to mount default graph
=========================
```
✅ 如果能执行 `audio list`，说明audio命令已注册

#### 测试3：手动挂载
```bash
$ audio mount
Default graph mounted at /audio/graph0
Use 'cd /audio/graph0' to access it
```
✅ 如果成功，说明效果图已初始化，挂载功能正常

#### 测试4：验证挂载
```bash
$ ls /audio
graph0/

$ cd /audio/graph0
$ ls
info    preset    node_count    nodes/
```
✅ 如果能看到这些文件，说明挂载成功

### 4. 代码检查清单

#### drv_init.c
```c
void Drv_Init(void)
{
    // ...
    
    /* VFS初始化 */
    Vfs_Init();                          // ✓ 必须
    ShellFs_Init();                      // ✓ 必须
    
    /* 挂载效果图VFS */
    EffectGraphVfs_MountDefault();       // ✓ 必须
    
    /* 注册audio命令 */
    ShellCmdAudioVfs_Register();         // ✓ 必须
    
    // ...
}
```

#### bg_audio_io_manager.c
```c
void BG_audio_Init(uint16_t SampleRate)
{
    // ...
    
    // 1. 初始化 Effect Graph 核心模块
    if (EffectGraph_Init() != 0) {       // ✓ 必须
        DBG("[Audio] ERROR: Effect Graph Init failed!\n");
        return;
    }
    
    // 2. 加载默认预设
    if (EffectGraphConfig_LoadPreset(GRAPH_PRESET_DEFAULT) != 0) {  // ✓ 必须
        DBG("[Audio] ERROR: Effect Graph Load Preset failed!\n");
        return;
    }
    
    // 3. 自动挂载效果图到VFS
    EffectGraphVfs_TryAutoMount();       // ✓ 必须（新增）
    
    // ...
}
```

#### bg_shell_commands.c
```c
#include "shell_cmd_audio_vfs.h"        // ✓ 必须

void Shell_RegisterAllModules(void)
{
    // ...
    ShellCmdAudioVfs_Register();         // ✓ 必须（新增）
    // ...
}
```

### 5. 编译检查

确保以下文件被编译：

```makefile
src/banux/05_component/effect_graph/effect_graph_vfs.c
src/banux/05_component/effect_graph/shell_cmd_audio_vfs.c
```

检查 `Debug/src/banux/05_component/effect_graph/` 目录下是否有：
- `effect_graph_vfs.o`
- `shell_cmd_audio_vfs.o`

### 6. 强制调试输出

在 `main.c` 或主循环中添加一次性测试：

```c
static bool test_done = false;

void MainLoop(void)
{
    if (!test_done) {
        test_done = true;
        
        // 测试VFS
        VfsNode_t *root = Vfs_GetRoot();
        DBG("[TEST] VFS root = %p\n", root);
        
        VfsNode_t *audio = Vfs_FindNode("/audio");
        DBG("[TEST] /audio = %p\n", audio);
        
        // 测试效果图
        EffectGraph_t *graph = EffectGraph_GetInstance();
        DBG("[TEST] Graph = %p\n", graph);
        if (graph) {
            DBG("[TEST] Graph nodes = %d\n", graph->node_count);
        }
        
        // 测试挂载
        DBG("[TEST] Calling TryAutoMount...\n");
        GraphVfsError_t err = EffectGraphVfs_TryAutoMount();
        DBG("[TEST] TryAutoMount result = %d\n", err);
        
        // 测试列表
        int count = EffectGraphVfs_ListGraphs(NULL);
        DBG("[TEST] Graph count = %d\n", count);
    }
    
    // 正常循环代码
    // ...
}
```

### 7. 快速诊断命令

系统启动后，依次执行：

```bash
# 1. 检查VFS结构
$ ls /
$ ls /audio

# 2. 检查audio命令
$ audio
$ audio list

# 3. 尝试手动挂载
$ audio mount

# 4. 验证挂载结果
$ ls /audio
$ cd /audio/graph0
$ ls
```

### 8. 预期的完整日志流程

```
=== 系统启动 ===
[VFS] Init: Root created
[ShellFs] Init: Shell FS registered
[GraphVfs] MountDefault: Start
[GraphVfs] Init: Start
[GraphVfs] Init: Getting VFS root...
[GraphVfs] Init: Creating /audio directory...
[GraphVfs] /audio created successfully at 0x20001234
[GraphVfs] MountDefault: Graph not ready yet, will retry later

=== 音频系统初始化 ===
[Audio] Initializing Effect Graph...
[EffectGraph] Init: Start
[EffectGraph] Init: Success
[Audio] Loading default preset...
[EffectGraphConfig] LoadPreset: GRAPH_PRESET_DEFAULT
[EffectGraphConfig] LoadPreset: Success (8 nodes)
[GraphVfs] TryAutoMount: Start
[GraphVfs] TryAutoMount: Current graph count = 0
[GraphVfs] TryAutoMount: Getting graph instance...
[GraphVfs] TryAutoMount: Graph instance OK, node_count=8
[GraphVfs] TryAutoMount: Mounting as graph0...
[GraphVfs] Mount: Start mounting 'graph0'
[GraphVfs] Mount: Checking if 'graph0' already exists...
[GraphVfs] Mount: Finding free handle slot...
[GraphVfs] Mount: Creating directory /audio/graph0
[GraphVfs] Graph 'graph0' mounted at /audio/graph0 (8 nodes)
[GraphVfs] TryAutoMount: SUCCESS - graph0 mounted with 8 nodes
[Audio] Effect Graph initialized successfully

=== 用户命令 ===
$ ls /audio
graph0/

$ cd /audio/graph0
$ ls
info    preset    node_count    nodes/
```

## 总结

如果按照上述调试步骤，应该能够定位问题所在：

1. ✅ VFS初始化
2. ✅ 效果图初始化
3. ✅ 自动挂载调用
4. ✅ Shell命令注册

任何一步失败都会在日志中有明确提示。
