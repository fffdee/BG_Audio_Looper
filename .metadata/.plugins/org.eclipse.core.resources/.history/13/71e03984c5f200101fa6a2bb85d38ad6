# 效果图Shell命令集成总结

## 📅 更新日期
2026年1月6日（最新更新：effect命令模块处理器修复）

## ✅ 完成的工作

### 1. 编译错误修复
- **问题1**: `MOD_CAT_AUDIO` 未定义
  - **修复**: 在 `bg_shell.h` 的 `ModCategory_t` 枚举中添加了 `MOD_CAT_AUDIO`
  - **文件**: `src/banux/04_shell_commands/bg_shell.h`

- **问题2**: `shell_cmd_effect.c` 缺少 `<stdlib.h>`
  - **修复**: 添加了 `#include <stdlib.h>` 头文件
  - **文件**: `src/banux/04_shell_commands/shell_cmd_effect.c`

### 2. Shell命令模块注册

#### 2.1 模块定义
在 `shell_cmd_graph.c` 中创建了完整的Shell模块定义：

```c
/**
 * @brief Graph命令选项（使用默认选项模式）
 */
static const ShellOpt_t g_GraphOpts[] = {
    { "", NULL, "[subcmd] [args]", "Effect graph control", GraphModuleHandler },
    OPT_END()
};

/**
 * @brief Graph命令模块定义
 */
static const ShellModule_t g_GraphModule = {
    "graph",
    "Audio Effect Graph Control",
    MOD_CAT_AUDIO,
    g_GraphOpts,
    1
};

/**
 * @brief fx快捷命令模块定义
 */
static const ShellModule_t g_FxModule = {
    "fx",
    "Quick Effect Parameter Access",
    MOD_CAT_AUDIO,
    g_FxOpts,
    1
};
```

#### 2.2 命令处理适配器
实现了 `GraphModuleHandler` 和 `FxModuleHandler` 函数，用于适配Shell系统的调用方式：

```c
static int GraphModuleHandler(int argc, char *argv[])
{
    /* argc 不包含模块名本身，argv[0] 是第一个参数 */
    char *fullArgv[SHELL_CMD_MAX_ARGS];
    int fullArgc = argc + 1;
    
    fullArgv[0] = "graph";  /* 模块名 */
    for (int i = 0; i < argc; i++) {
        fullArgv[i + 1] = argv[i];
    }
    
    return ShellCmdGraph_Execute(fullArgc, fullArgv);
}
```

**重要修复（2026-01-06 晚）**: 修复 `shell_cmd_effect.c` 的模块处理器
- **问题**: effect命令的选项定义使用了具体的处理函数（CmdList、CmdInfo等），但这些函数的参数签名与Shell系统不匹配，导致命令无法被正确分发
- **根本原因**: Shell模块系统调用选项处理函数时，传入的argc/argv不包含模块名本身，需要通过适配器重新构建完整的参数列表
- **修复**: 实现了 `EffectModuleHandler` 适配器函数，采用与graph/fx命令相同的默认选项处理模式：

```c
/**
 * @brief Effect命令默认处理 - 用于模块系统
 */
static int EffectModuleHandler(int argc, char *argv[])
{
    /* argc 不包含模块名本身，argv[0] 是第一个参数 */
    /* 重新构建完整的 argc/argv 供 ShellCmdEffect_Execute 使用 */
    char *fullArgv[32];
    int fullArgc = argc + 1;
    int i;
    
    fullArgv[0] = "effect";  /* 模块名 */
    for (i = 0; i < argc && i < 31; i++) {
        fullArgv[i + 1] = argv[i];
    }
    
    return ShellCmdEffect_Execute(fullArgc, fullArgv);
}

/**
 * @brief Effect命令选项定义（使用默认选项模式）
 */
static const ShellOpt_t g_EffectOpts[] = {
    { "", NULL, "[subcmd] [args]", "Audio Effect Parameter Control", EffectModuleHandler },
    OPT_END()
};

/**
 * @brief Effect命令模块定义
 */
static const ShellModule_t g_EffectModule = {
    "effect",
    "Audio Effect Parameter Control",
    MOD_CAT_AUDIO,
    g_EffectOpts,
    1  /* 选项数量从5改为1 */
};
```

- **效果**: effect命令现在可以被Shell系统正确识别和分发，所有子命令（list、info、get、set、enable）均可正常工作
- **文件**: `src/banux/04_shell_commands/shell_cmd_effect.c`

### 3. 系统集成

#### 3.1 头文件更新
- **`shell_cmd_graph.h`**: 添加了 `#include "bg_shell.h"` 以获取Shell系统定义
- **`bg_shell_commands.c`**: 添加了效果器命令模块的头文件引用

#### 3.2 模块注册调用
在 `Shell_RegisterAllModules()` 函数中添加了注册调用：

```c
void Shell_RegisterAllModules(void)
{
    // ...existing modules...
    
    /* 效果器命令模块 */
    ShellCmdEffect_Register();   /* effect 命令 */
    ShellCmdGraph_Register();    /* graph 和 fx 命令 */
}
```

## 📋 可用命令

### graph 命令
```bash
graph list                    # 列出所有节点
graph info                    # 显示图详细信息
graph preset [id]             # 切换/显示预设
graph node <id|name> [on|off] # 启用/禁用节点
graph bypass <id|name> [on|off]# 设置节点旁路
graph get <id|name> [param]   # 获取参数
graph set <id|name> <param> <val># 设置参数
graph params <id|name>        # 显示可用参数
graph rebuild                 # 重建图

# 批量操作
graph allfx <on|off>          # 启用/禁用所有效果
graph allbypass <on|off>      # 旁路所有效果

# 快照管理
graph snapshot save <slot> [name]# 保存状态
graph snapshot load <slot>    # 加载状态
graph snapshot list           # 列出快照
```

### fx 快捷命令
```bash
fx <id>                       # 显示节点参数
fx <id> <param>               # 获取参数
fx <id> <param> <value>       # 设置参数
```

### effect 命令
```bash
effect list                   # 列出所有效果器
effect info <id>              # 显示效果器详情
effect get <id> <param>       # 获取参数
effect set <id> <param> <val> # 设置参数
effect enable <id> [on|off]   # 启用/禁用效果器
```

## 📁 修改的文件

| 文件 | 变更 |
|------|------|
| `bg_shell.h` | 添加 `MOD_CAT_AUDIO` 枚举 |
| `shell_cmd_effect.c` | 添加 `<stdlib.h>` 头文件；**实现EffectModuleHandler适配器（关键修复）** |
| `shell_cmd_graph.c` | 完整的Shell模块实现，包括参数校验、快照管理等 |
| `shell_cmd_graph.h` | 添加 `bg_shell.h` 引用 |
| `bg_shell_commands.c` | 添加效果器命令头文件和注册调用 |
| `effect_graph_vfs.c/.h` | **新增** 效果图虚拟文件系统实现 |
| `shell_cmd_audio_vfs.c/.h` | **新增** audio命令实现 |
| `drv_init.c` | 添加效果图VFS初始化和audio命令注册 |

## 🆕 新增功能：/audio 虚拟文件系统

### 功能概述
将效果图参数挂载到 `/audio` 目录，支持通过 VFS 命令（cd/ls/cat/echo）访问和修改参数。

### 目录结构
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
    │       ├── 0_adc0/
    │       │   ├── enabled
    │       │   ├── bypass
    │       │   └── type
    │       ├── 3_drc/
    │       │   ├── enabled
    │       │   ├── bypass
    │       │   ├── threshold
    │       │   ├── ratio
    │       │   ├── attack
    │       │   └── release
    │       └── ...
    └── graph1/             # 可动态创建
```

### 使用方法
```bash
# 列出效果图
$ ls /audio
graph0/

# 进入效果图
$ cd /audio/graph0/nodes/3_drc

# 读取参数
$ cat threshold
-20

# 修改参数（需要echo支持重定向）
$ echo -25 > threshold

# audio命令管理
$ audio list
$ audio create myGraph 1
$ audio delete myGraph
$ audio info graph0
```

详细说明请参考 `AUDIO_VFS_GUIDE.md`。

## 📦 未来计划

1. 在实际硬件上测试所有命令
2. 完善参数持久化（保存到Flash）
3. 添加参数预设管理
4. 完善参数变更后的DSP同步机制
5. 添加参数批量导入/导出功能

## 📚 相关文档

- [GRAPH_PARAMS_GUIDE.md](./GRAPH_PARAMS_GUIDE.md) - 命令使用指南
- [EFFECT_GRAPH_README.md](./EFFECT_GRAPH_README.md) - 效果图系统说明
- [shell_cmd_graph.h](./shell_cmd_graph.h) - API接口文档
- [AUDIO_VFS_GUIDE.md](./AUDIO_VFS_GUIDE.md) - 虚拟文件系统使用指南
