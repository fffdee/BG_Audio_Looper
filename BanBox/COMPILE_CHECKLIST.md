# 系统监控功能 - 编译集成检查清单

## 📋 文件清单

请确保以下文件已添加到工程：

### 1. 源文件
- [ ] `BanBox/src/banux/04_shell_commands/shell_cmd_sysmon.c`
- [ ] `BanBox/src/banux/04_shell_commands/shell_cmd_sysmon.h`

### 2. 修改的文件
- [ ] `BanBox/src/banux/06_app/BG_AudioIO_Manager/bg_audio_io_manager.c`
- [ ] `MVsB1_Base_SDK/middleware/rtos/freertos/inc/FreeRTOSConfig.h`

### 3. 文档文件（可选，不影响编译）
- [ ] `BanBox/SYSMON_COMMANDS_GUIDE.md`
- [ ] `BanBox/SYSMON_QUICK_REFERENCE.md`
- [ ] `BanBox/SYSMON_INTEGRATION_SUMMARY.md`
- [ ] `BanBox/系统监控功能说明.md`

## 🔧 工程配置检查

### 1. 添加源文件到工程

#### Eclipse/AndesIDE环境
1. 右键点击工程 → Properties
2. C/C++ General → Paths and Symbols → Source Location
3. 确认包含路径：`src/banux/04_shell_commands`

#### Makefile环境
在 `sources.mk` 或对应的makefile中添加：
```makefile
# 添加系统监控模块
C_SRCS += \
    src/banux/04_shell_commands/shell_cmd_sysmon.c
```

### 2. 包含路径配置

确保以下路径在工程的包含路径中：
```
BanBox/src/banux/04_shell_commands/
MVsB1_Base_SDK/middleware/rtos/freertos/inc/
```

### 3. 链接FreeRTOS库

确保链接了FreeRTOS相关库：
- `libfreertos.a` (如果使用预编译库)
- 或确保FreeRTOS源文件已包含在工程中

## ✅ 代码修改验证

### 1. bg_audio_io_manager.c
确认以下修改已应用：

```c
// 头文件包含区域（约第42-45行）
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "shell_cmd_graph.h"

// System Monitor 模块
#include "shell_cmd_sysmon.h"  // <-- 新增这行

#include "bt_manager.h"
```

```c
// 初始化函数中（约第318-323行）
// 4. 注册 Shell 命令（支持 CDC/BLE 远程控制）
ShellCmdGraph_Register();

// 5. 注册系统监控命令（CPU/内存/任务统计）
ShellCmdSysmon_Register();  // <-- 新增这行

DBG("[Audio] Effect Graph initialized successfully\n");
```

### 2. FreeRTOSConfig.h
确认以下修改已应用（约第104-107行）：

```c
#define configMAX_TASK_NAME_LEN			( 16 )
#define configUSE_TRACE_FACILITY		1
#define configUSE_STATS_FORMATTING_FUNCTIONS	1  // <-- 新增这行
#define configUSE_16_BIT_TICKS			0
```

## 🔨 编译步骤

### 方式1: IDE编译
1. Clean Project（清理工程）
2. Build Project（编译工程）
3. 检查编译输出，确认无错误

### 方式2: Makefile编译
```bash
# 清理
make clean

# 编译
make all

# 检查输出
# 应该看到 shell_cmd_sysmon.o 被编译
```

## 🚨 常见编译错误及解决

### 错误1: "shell_cmd_sysmon.h: No such file or directory"
**原因**: 头文件路径未添加到工程
**解决**: 添加 `src/banux/04_shell_commands/` 到包含路径

### 错误2: "undefined reference to 'vTaskGetRunTimeStats'"
**原因**: FreeRTOS配置不正确或库未链接
**解决**: 
- 检查 `configGENERATE_RUN_TIME_STATS` 是否为1
- 检查 `configUSE_STATS_FORMATTING_FUNCTIONS` 是否为1
- 确认FreeRTOS库已链接

### 错误3: "undefined reference to 'xPortGetMinimumEverFreeHeapSize'"
**原因**: FreeRTOS版本较旧，不支持此API
**解决**: 
- 注释掉 `shell_cmd_sysmon.c` 中 `xPortGetMinimumEverFreeHeapSize()` 相关代码
- 或升级FreeRTOS版本到V8.0.0以上

### 错误4: "ShellCmdSysmon_Register' undeclared"
**原因**: 头文件未包含
**解决**: 在 `bg_audio_io_manager.c` 中添加：
```c
#include "shell_cmd_sysmon.h"
```

## 📊 编译成功标志

编译成功后，应该看到：
```
Compiling: shell_cmd_sysmon.c
Linking: BanBox.elf
Building target: BanBox.elf
Finished building: BanBox.elf
```

代码大小应该增加约2-3KB：
```
   text	   data	    bss	    dec	    hex	filename
 345678	   1234	  56789	 403701	  62a55	BanBox.elf (增加约2-3KB)
```

## 🧪 运行时验证

### 1. 烧录程序
```bash
# 使用你的烧录工具
# 例如：
openocd -f interface/jlink.cfg -f target/nds32.cfg -c "program BanBox.elf verify reset exit"
```

### 2. 连接串口
- 波特率: 115200
- 数据位: 8
- 停止位: 1
- 校验: 无

### 3. 测试命令
在串口终端输入：
```bash
# 查看帮助
help

# 查看sysmon模块
help -m sysmon

# 测试内存统计
sysmon -m

# 测试任务信息
sysmon -t

# 测试系统信息
sysmon -s

# 测试CPU统计
sysmon -c
```

### 4. 预期输出
如果看到以下输出，说明功能正常：
```
$ sysmon -m

=== Memory Usage ===
Current Free:      45678 bytes
Minimum Ever Free: 42340 bytes
Total Heap Size:   37888 bytes
Used:              19858 bytes (52.4%)
Peak Used:         23196 bytes (61.2%)
```

## 🔍 调试技巧

### 如果命令不响应
1. 检查Shell初始化: 在 `BG_audio_Init()` 中添加调试输出
```c
DBG("[Debug] Registering sysmon commands...\n");
ShellCmdSysmon_Register();
DBG("[Debug] Sysmon commands registered\n");
```

2. 检查命令注册: 在串口终端输入 `help`，应该能看到 `sysmon` 模块

3. 检查IO接口: 确认Shell IO Manager正常工作

### 如果CPU统计为0
1. 检查 `configGENERATE_RUN_TIME_STATS` 配置
2. 检查 `portGET_RUN_TIME_COUNTER_VALUE()` 是否正常递增
3. 让系统运行一段时间后再查询（需要积累统计数据）

### 如果任务列表为空
1. 检查 `configUSE_TRACE_FACILITY` 配置
2. 确认有任务在运行（至少有IDLE任务）
3. 检查任务名称长度是否超过 `configMAX_TASK_NAME_LEN`

## ✅ 最终检查清单

编译并烧录前，请确认：

- [x] 所有新文件已添加到工程
- [x] 包含路径已配置
- [x] FreeRTOSConfig.h 已修改
- [x] bg_audio_io_manager.c 已修改
- [x] 工程能成功编译
- [x] 代码大小增加约2-3KB（在可接受范围内）
- [x] 连接串口，波特率115200
- [x] 烧录并运行
- [x] 测试 `sysmon -m` 命令
- [x] 测试 `sysmon -t` 命令
- [x] 测试 `sysmon -s` 命令
- [x] 测试 `sysmon -c` 命令

## 📞 问题反馈

如果遇到问题，请提供：
1. 编译错误信息（完整的）
2. 运行时输出（串口日志）
3. FreeRTOSConfig.h 相关配置
4. 使用的开发环境和工具链版本

## 📚 参考文档

- `系统监控功能说明.md` - 快速入门
- `SYSMON_COMMANDS_GUIDE.md` - 完整功能说明
- `SYSMON_QUICK_REFERENCE.md` - 快速参考
- FreeRTOS官方文档

---

**编译检查完成后，即可开始使用系统监控功能！** ✨
