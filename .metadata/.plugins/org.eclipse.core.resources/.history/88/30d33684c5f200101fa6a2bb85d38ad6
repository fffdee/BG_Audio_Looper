# 效果图Shell系统编译和部署指南

## 📋 文件清单

### 已修改文件
```
BanBox/src/banux/
├── 01_vfs/
│   ├── vfs.h                      (无修改，已有VFS基础)
│   └── vfs.c
├── 03_driver_framework/
│   └── drv_init.c                 (✓ 已修改：添加VFS初始化)
├── 04_shell_commands/
│   ├── bg_shell.h                 (✓ 已修改：添加MOD_CAT_AUDIO，删除重复宏)
│   ├── bg_shell_commands.c        (✓ 已修改：注册graph/fx/effect命令)
│   └── shell_cmd_effect.c         (✓ 已修改：添加EffectModuleHandler)
└── 05_component/effect_graph/
    ├── effect_graph.h/c           (无修改)
    ├── effect_graph_config.h/c    (无修改)
    ├── shell_cmd_graph.h          (✓ 已修改：添加bg_shell.h引用)
    ├── shell_cmd_graph.c          (✓ 已修改：完整Shell命令实现)
    ├── effect_graph_vfs.h         (✓ 新增：VFS接口)
    ├── effect_graph_vfs.c         (✓ 新增：VFS实现)
    ├── shell_cmd_audio_vfs.h      (✓ 新增：audio命令接口)
    ├── shell_cmd_audio_vfs.c      (✓ 新增：audio命令实现)
    ├── AUDIO_VFS_GUIDE.md         (✓ 新增：VFS使用指南)
    ├── SHELL_TEST_SCRIPT.md       (✓ 更新：测试脚本)
    └── SHELL_INTEGRATION_SUMMARY.md (✓ 更新：集成总结)
```

## 🔧 编译配置

### 1. Makefile 检查

确保以下文件被包含在编译中：

**在 `Debug/src/banux/05_component/effect_graph/subdir.mk` 中添加：**
```makefile
C_SRCS += \
../src/banux/05_component/effect_graph/effect_graph.c \
../src/banux/05_component/effect_graph/effect_graph_config.c \
../src/banux/05_component/effect_graph/effect_graph_vfs.c \
../src/banux/05_component/effect_graph/shell_cmd_graph.c \
../src/banux/05_component/effect_graph/shell_cmd_audio_vfs.c

OBJS += \
./src/banux/05_component/effect_graph/effect_graph.o \
./src/banux/05_component/effect_graph/effect_graph_config.o \
./src/banux/05_component/effect_graph/effect_graph_vfs.o \
./src/banux/05_component/effect_graph/shell_cmd_graph.o \
./src/banux/05_component/effect_graph/shell_cmd_audio_vfs.o
```

**在 `Debug/src/banux/04_shell_commands/subdir.mk` 中检查：**
```makefile
C_SRCS += \
../src/banux/04_shell_commands/shell_cmd_effect.c \
# ... 其他文件
```

### 2. 包含路径配置

确保编译器能找到头文件：
```makefile
-I"../src/banux/01_vfs"
-I"../src/banux/04_shell_commands"
-I"../src/banux/05_component/effect_graph"
```

## 🔨 编译步骤

### 方法1：使用IDE（推荐）
1. 打开项目文件（.project 或 .cproject）
2. 右键项目 -> Clean Project
3. 右键项目 -> Build Project
4. 检查编译输出，确保没有错误

### 方法2：使用命令行
```bash
# Windows PowerShell
cd C:\Users\BanGO\Desktop\BanGO_prj\BG_Audio_Looper\BanBox
cd Debug

# 清理
make clean

# 编译
make -j4

# 检查输出
ls -l output/*.elf
```

### 方法3：使用Cygwin
```bash
# Cygwin
cd /cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox
cd Debug

make clean
make -j4
```

## 🐛 常见编译错误及解决

### 错误1：unknown type name 'GraphPreset_t'
**原因**: 缺少头文件包含
**解决**: 已在 `effect_graph_vfs.h` 中添加 `#include "effect_graph_config.h"`

### 错误2：REGISTER_MODULE redefined
**原因**: 宏定义重复
**解决**: 已从 `bg_shell.h` 第258行删除重复定义

### 错误3：undefined reference to `EffectGraphVfs_*`
**原因**: VFS文件未编译或链接
**解决**: 检查Makefile中是否包含 `effect_graph_vfs.c`

### 错误4：undefined reference to `ShellCmdGraph_Register`
**原因**: Shell命令未注册
**解决**: 检查 `bg_shell_commands.c` 中是否调用注册函数

### 错误5：implicit declaration of function 'atoi'
**原因**: 缺少 `<stdlib.h>` 头文件
**解决**: 在相应文件中添加 `#include <stdlib.h>`

## ⚡ 快速编译命令

创建快速编译脚本 `build.sh`:
```bash
#!/bin/bash
cd Debug
echo "=== Cleaning ==="
make clean
echo "=== Building ==="
make -j4
if [ $? -eq 0 ]; then
    echo "=== Build SUCCESS ==="
    ls -lh output/*.elf
else
    echo "=== Build FAILED ==="
    exit 1
fi
```

使用：
```bash
chmod +x build.sh
./build.sh
```

## 📦 烧录和部署

### 1. 固件烧录
```bash
# 使用J-Link
JLinkExe -device DEVICE_NAME -if SWD -speed 4000
loadfile output/BanBox.elf
r
g
q

# 或使用IDE烧录功能
# Debug -> Debug As -> ... 或 Flash 工具
```

### 2. 串口连接测试
```bash
# Windows (使用Tera Term或PuTTY)
# COM口设置: 115200 8N1

# Linux/Mac
screen /dev/ttyUSB0 115200
# 或
minicom -D /dev/ttyUSB0 -b 115200
```

### 3. 基本功能测试
```bash
# 连接后按回车
$ help -a
# 应该看到 graph, fx, effect, audio 命令

$ audio list
# 应该看到 graph0

$ ls /audio
# 应该看到 graph0/

$ cd /audio/graph0/nodes
$ ls
# 应该看到所有节点目录
```

## 📊 编译输出检查

### 正常编译输出示例
```
Building file: ../src/banux/05_component/effect_graph/effect_graph_vfs.c
arm-none-eabi-gcc ... -c -o effect_graph_vfs.o effect_graph_vfs.c
Finished building: effect_graph_vfs.c

Building file: ../src/banux/05_component/effect_graph/shell_cmd_audio_vfs.c
arm-none-eabi-gcc ... -c -o shell_cmd_audio_vfs.o shell_cmd_audio_vfs.c
Finished building: shell_cmd_audio_vfs.c

Linking: BanBox.elf
arm-none-eabi-gcc ... -o "output/BanBox.elf" ...
Finished building: BanBox.elf

Creating binary: BanBox.bin
arm-none-eabi-objcopy -O binary output/BanBox.elf output/BanBox.bin
Finished building: BanBox.bin

Size:
   text    data     bss     dec     hex filename
 245678   12345   23456  281479   44b67 output/BanBox.elf
```

### 代码大小估算
- **effect_graph_vfs.c**: ~10KB
- **shell_cmd_audio_vfs.c**: ~3KB
- **shell_cmd_graph.c**: ~8KB (已有，已优化)
- **总增量**: ~21KB Flash, ~2KB RAM

## 🔍 运行时检查

### 系统启动日志
启动时应看到：
```
[DrvInit] Initializing Audio Graph VFS...
[GraphVfs] /audio created successfully
[GraphVfs] Default graph mounted as /audio/graph0
[DrvInit] Audio Graph VFS mounted OK
[ShellCmdAudioVfs] Registered
```

### 内存使用检查
```bash
$ sys mem
# 检查堆使用情况，确保有足够内存

$ sys tasks
# 检查任务运行状态
```

## 🚀 部署检查清单

- [ ] 代码编译通过，无错误无警告
- [ ] 固件大小在Flash容量内
- [ ] 烧录成功
- [ ] 串口连接正常
- [ ] help命令显示graph/fx/effect/audio
- [ ] audio list显示graph0
- [ ] ls /audio 正常工作
- [ ] cd/cat等VFS命令正常工作
- [ ] 参数读取正常
- [ ] 参数设置生效（音频输出有变化）

## 📝 版本信息

- **系统版本**: BG_Audio_Looper V1.2.0
- **效果图Shell**: V1.0.0
- **效果图VFS**: V1.0.0
- **更新日期**: 2026年1月6日

## 📞 问题排查

如果遇到问题：

1. **编译失败**: 检查上述常见错误
2. **链接失败**: 检查Makefile配置
3. **烧录失败**: 检查连接和设备
4. **命令不可用**: 检查初始化日志
5. **VFS目录不存在**: 检查效果图是否初始化

**调试建议**:
- 使用 `DBG()` 宏增加调试输出
- 检查 `drv_init.c` 中的初始化顺序
- 使用Shell的 `dbg` 命令调整日志级别
- 查看系统启动日志

## 📖 相关文档

- `AUDIO_VFS_GUIDE.md` - VFS详细使用指南
- `SHELL_TEST_SCRIPT.md` - 测试脚本
- `SHELL_INTEGRATION_SUMMARY.md` - 集成总结
- `GRAPH_PARAMS_GUIDE.md` - 参数说明
