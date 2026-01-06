# 效果器参数调节模块集成清单

## 文件清单

### 新增文件

- ✅ `BanBox/src/banux/04_shell_commands/shell_cmd_effect.h` - 头文件声明
- ✅ `BanBox/src/banux/04_shell_commands/shell_cmd_effect.c` - 实现文件
- ✅ `BanBox/EFFECT_PARAMS_GUIDE.md` - 详细使用指南
- ✅ `BanBox/EFFECT_QUICK_REFERENCE.md` - 快速参考卡

### 需要修改的文件

- [ ] `BanBox/src/banux/06_app/BG_AudioIO_Manager/bg_audio_io_manager.c` - 注册命令

### 编译配置

- [ ] `BanBox/Debug/src/banux/04_shell_commands/subdir.mk` - 添加源文件

## 集成步骤

### 第1步：添加源文件到编译系统

编辑 `BanBox/Debug/src/banux/04_shell_commands/subdir.mk`：

**查找：**
```makefile
SUBDIRS += \
../src/banux/04_shell_commands \

# 或

SRC_FILES += \
../src/banux/04_shell_commands/shell_cmd_*.c
```

**确认以下文件被包含：**
```makefile
SRC_FILES += \
../src/banux/04_shell_commands/bg_shell.c \
../src/banux/04_shell_commands/bg_shell_commands.c \
../src/banux/04_shell_commands/shell_cmd_sysmon.c \
../src/banux/04_shell_commands/shell_cmd_effect.c \
```

如果使用通配符模式，则无需修改。

### 第2步：在头文件中包含

编辑 `BanBox/src/banux/06_app/BG_AudioIO_Manager/bg_audio_io_manager.c`：

**找到现有的include区域（约第1-50行）：**
```c
#include "shell_cmd_sysmon.h"
```

**在其后添加：**
```c
#include "shell_cmd_effect.h"
```

### 第3步：注册命令

在同一文件中，找到 `BG_audio_Init()` 函数（约第270-330行）：

**找到现有的注册语句：**
```c
ShellCmdSysmon_Register();
ShellCmdGraph_Register();
```

**在其后添加：**
```c
ShellCmdEffect_Register();
```

### 第4步：验证Shell模块容量

编辑 `BanBox/src/banux/04_shell_commands/bg_shell.h`：

**确认SHELL_MODULE_MAX足够大：**
```c
#define SHELL_MODULE_MAX        20      // Max module count
```

当前已注册的模块数：
- sys, audio, gpio, lcd, led, dbg, looper, flash, battery, bt (10个)
- ls, pwd, cd, cat, tree, drivers (6个)
- sysmon (1个)
- graph (1个)
- **effect (1个) <- 新增**

**总计：19个模块，SHELL_MODULE_MAX=20应该足够**

如果添加了更多模块，增加此值：
```c
#define SHELL_MODULE_MAX        24      // Max module count (增加4)
```

## 验证步骤

### 编译检查

```bash
cd BanBox/Debug
make clean
make -j4 2>&1 | tee build.log

# 检查是否有错误
grep -i "error" build.log
```

**预期结果：** 编译成功，无error信息

### 符号检查

```bash
# 检查是否包含所需符号
grep "ShellCmdEffect_Register" output/objdump.txt
grep "Effect_SetEnabled" output/objdump.txt
```

**预期结果：** 两个符号都应该出现

### 链接检查

```bash
# 检查是否链接正确
nm output/*.o | grep ShellCmdEffect
```

**预期结果：** 输出应包含 `ShellCmdEffect_Register` 和相关函数

## 运行测试

### 1. 基础命令测试

连接串口（CDC或BLE），输入以下命令：

```bash
# 1. 列出所有效果器
effect list

# 2. 查看帮助
effect help

# 3. 查看DRC详情
effect info 1
```

**预期输出：**
```
===== Audio Effects [11] =====
[ 0] reverb           - 混响效果 [ON]
[ 1] drc              - 动态范围压缩 (麦克风) [ON]
...
```

### 2. 参数调节测试

```bash
# 1. 获取当前参数
effect get 1 threshold

# 2. 设置参数
effect set 1 threshold -25

# 3. 验证设置
effect get 1 threshold
```

**预期输出：**
```
[Effect 1] Getting parameter 'threshold'...
DRC threshold: -20 dB
[Effect 1] Setting parameter 'threshold' to -25...
DRC threshold set to -25 dB
[Effect 1] Getting parameter 'threshold'...
DRC threshold: -25 dB
```

### 3. 启用/禁用测试

```bash
# 1. 查询状态
effect enable 4

# 2. 启用
effect enable 4 on

# 3. 禁用
effect enable 4 off

# 4. 列表验证
effect list
```

**预期输出：**
```
Effect 'echo' is currently [OFF]
Effect 'echo' enabled
Effect 'echo' disabled
```

## 故障排除

### 错误1: "Unknown module: effect"

**原因：** 命令未被正确注册

**检查清单：**
- [ ] shell_cmd_effect.c 编译是否成功？
- [ ] ShellCmdEffect_Register() 是否被调用？
- [ ] SHELL_MODULE_MAX 是否足够大？
- [ ] 是否重新编译了项目？

**解决方案：**
```bash
# 清理并重新编译
cd BanBox/Debug
make clean
make -j4

# 检查符号
grep ShellCmdEffect output/objdump.txt
```

### 错误2: "Effect X does not support parameter setting"

**原因：** 该效果器不支持该参数

**解决方案：**
```bash
# 使用 effect info 查看支持的参数
effect info <id>
```

### 错误3: 参数修改无效果

**原因：** 效果器可能处于禁用状态

**解决方案：**
```bash
# 启用效果器
effect enable <id> on

# 然后修改参数
effect set <id> <param> <value>
```

### 错误4: 编译错误 "undefined reference to 'Effect_XXX'"

**原因：** shell_cmd_effect.c 未被链接

**解决方案：**
1. 检查 subdir.mk 是否包含 shell_cmd_effect.c
2. 删除 Debug/src/banux/04_shell_commands/*.o
3. 重新编译

## 性能考虑

### CPU占用

启用所有效果器的CPU占用估计：
- DRC: ~2-3%
- EQ: ~3-4%
- Reverb: ~5-8%
- **总计：~10-15%**

监控CPU占用：
```bash
sysmon -c
```

### 内存占用

新模块的内存使用：
- 代码段: ~8KB (shell_cmd_effect.c)
- 数据段: ~0.5KB (效果器信息表)
- **总计：<10KB**

## 后续优化

### 可选功能

1. **参数保存到Flash**
   - 实现 `Effect_SaveConfig()` 和 `Effect_LoadConfig()`
   - 需要Flash抽象层支持

2. **预设管理**
   - 添加预设存储和加载
   - `effect preset save <name>`
   - `effect preset load <name>`

3. **实时监控**
   - 添加 `effect monitor <id>` 命令
   - 持续显示实时参数值

4. **音频输出分析**
   - 集成频谱分析
   - 显示实时音频特性

## 文档更新

- [x] EFFECT_PARAMS_GUIDE.md - 详细使用指南
- [x] EFFECT_QUICK_REFERENCE.md - 快速参考
- [ ] README.md - 更新项目概述
- [ ] CHANGELOG.md - 记录版本更新

## 版本信息

**模块版本:** V1.0.0  
**创建日期:** 2026-01-06  
**作者:** BG Card Team  
**状态:** ✅ 就绪

## 检查清单（集成完成）

- [x] 头文件 shell_cmd_effect.h 创建
- [x] 实现文件 shell_cmd_effect.c 创建
- [x] 函数实现完成
- [x] 文档编写完成
- [ ] bg_audio_io_manager.c 已修改（include + register）
- [ ] 项目编译成功
- [ ] 基础命令测试通过
- [ ] 参数调节测试通过
- [ ] 性能监控测试通过
- [ ] 文档最终审查

## 下一步

1. **修改 bg_audio_io_manager.c**
   - 添加 `#include "shell_cmd_effect.h"`
   - 在 BG_audio_Init() 中调用 ShellCmdEffect_Register()

2. **编译验证**
   - 执行 `make clean && make -j4`
   - 检查编译日志

3. **功能测试**
   - 连接串口测试各命令
   - 验证参数调节效果

4. **性能评估**
   - 使用 `sysmon -c` 监控CPU占用
   - 记录基准数据

---

**更新日期:** 2026-01-06  
**最后修改:** 完成初版集成指南
