# 第5章：Shell 命令系统 (04_shell_commands)

> **本章目标**：掌握 Shell 命令行框架，了解所有可用命令，学会调试
> **对应代码**：`BanBox/src/banux/04_shell_commands/`

---

## 5.1 Shell 框架概述

BanBox 内置了一个完整的 **命令行 Shell**，可通过 USB CDC 或 BLE 串口访问。它是开发和调试的核心入口。

### 访问方式

```
┌───────────────────────────────────────────────┐
│                  Shell 解析器                    │
│         (bg_shell.c — 命令解析与路由)             │
├─────────────────────┬─────────────────────────┤
│     USB CDC (虚拟串口)│   BLE SPP (蓝牙串口)      │
│   ShellIOManager     │   ShellIO_BLE           │
│   自动选择通道        │                          │
└─────────────────────┴─────────────────────────┘
```

### 连接方法

```bash
# USB 连接 (PC)
# 设备管理器中找到 "BanBox CDC" COM 口
# 使用任意串口工具连接，波特率 115200, 8N1

# BLE 连接 (手机 App)
# App 连接 BLE 后，通过 SPP 特征值发送命令
```

---

## 5.2 Shell 框架核心

### 命令注册机制

每个命令模块通过三个步骤注册：

```c
// 步骤 1：定义选项映射表
static const ShellOpt_t mycmd_opts[] = {
    OPT("s", "short",  NULL,     "简短描述",    short_handler),
    OPT("l", "long",   "<arg>",  "带参数描述",   long_handler),
    OPT_END()
};

// 步骤 2：声明模块
DEFINE_MODULE(mycmd, "My Command Description", MOD_CAT_DEBUG, mycmd_opts);

// 步骤 3：提供注册函数
void ShellCmdMyCmd_Register(void) {
    REGISTER_MODULE(mycmd);
}
```

### 宏说明

| 宏 | 说明 |
|-----|------|
| `OPT(s, l, a, h, fn)` | 定义选项：短选项、长选项、参数描述、帮助、处理函数 |
| `OPT_END()` | 选项表结束标记 |
| `DEFINE_MODULE(n, d, c, o)` | 定义模块：名称、描述、分类、选项表 |
| `REGISTER_MODULE(n)` | 注册模块到 Shell |

### 处理函数签名

```c
int handler(int argc, char *argv[]);
// argc: 参数个数 (不含命令名)
// argv: 参数数组
// 返回: 0=成功, -1=失败
```

### 输出函数

```c
Shell_Print("hello\r\n");              // 输出字符串
Shell_Printf("value=%d\r\n", val);     // 格式化输出
```

---

## 5.3 命令模块分类

### 5.3.1 系统命令 (`sys` / `dbg`)

| 命令 | 用法 | 说明 |
|------|------|------|
| `sys -i` | `sys --info` | 显示系统信息（版本、Flash 大小等） |
| `sys -p` | `sys --ps` | 显示 FreeRTOS 任务列表（类 Linux ps） |
| `sys -f` | `sys --free` | 显示堆剩余空间 |
| `sys -r` | `sys --reboot` | 系统软件复位 |
| `sys -t` | `sys --time` | 显示系统运行时间 |
| `dbg -h` | `dbg --heap` | 堆内存统计详情 |

### 5.3.2 音频控制命令 (`audio`)

| 命令 | 说明 |
|------|------|
| `audio -v <vol>` | 设置主音量 0-100 |
| `audio -m <ch> <0/1>` | 静音/取消静音通道 |
| `audio -s <rate>` | 设置采样率 (44100/48000) |
| `audio -i` | 显示当前音频配置 |

### 5.3.3 GPIO / LED / LCD 命令

| 命令 | 用法 | 说明 |
|------|------|------|
| `gpio -r <port> <pin>` | `gpio --read A 5` | 读取引脚电平 |
| `gpio -w <port> <pin> <0/1>` | `gpio --write B 3 1` | 设置引脚输出 |
| `lcd -t <text>` | `lcd --text "Hello"` | LCD 显示文字 |
| `lcd -c <color>` | `lcd --color 0xFFFF` | LCD 填充颜色 |
| `led -s <0/1>` | `led --set 1` | LED 亮/灭 |

### 5.3.4 Looper 命令 (`looper`)

| 命令 | 用法 | 说明 |
|------|------|------|
| `looper -r <seg>` | 录制指定段 (0-3) |
| `looper -p <seg>` | 播放指定段 |
| `looper -t <seg>` | 停止指定段 |
| `looper -c <seg>` | 清除指定段 |
| `looper -o <seg>` | 叠加录制到指定段 |
| `looper -u` | 撤销上一步操作 |
| `looper -s` | 显示所有段状态 |

### 5.3.5 Flash / 文件系统命令

| 命令 | 用法 | 说明 |
|------|------|------|
| `flash -i` | Flash 信息 |
| `flash -e <addr> <len>` | 擦除 Flash 扇区 |
| `flash -r <addr> <len>` | 读取并打印 Flash 数据 |
| `ls <path>` | 列出 FAT32 目录文件 |
| `cat <file>` | 查看文件内容 |
| `tree <path>` | 递归显示目录树 |

### 5.3.6 蓝牙命令 (`bt` / `ble`)

| 命令 | 用法 | 说明 |
|------|------|------|
| `bt -s` | 查看蓝牙状态 |
| `bt -c` | 连接设备 |
| `bt -d` | 断开连接 |
| `bt -n <name>` | 设置蓝牙名称 |
| `ble -s` | 查看 BLE 状态 |
| `ble -a <0/1>` | 开启/关闭 BLE 广播 |
| `ble_send <hex>` | 发送 BLE 数据（十六进制） |

### 5.3.7 效果器命令 (`effect` / `graph` / `chain`)

| 命令 | 用法 | 说明 |
|------|------|------|
| `effect -l` | 列出所有效果器 |
| `effect -g <name>` | 获取效果器参数 |
| `effect -s <name> <val>` | 设置效果器参数 |
| `graph -i` | 查看 Effect Graph 拓扑 |
| `graph -n <name>` | 显示指定节点信息 |
| `chain -l` | 列出预设效果链 |
| `chain -a <name>` | 激活指定效果链 |

### 5.3.8 电池命令

| 命令 | 用法 | 说明 |
|------|------|------|
| `battery` | 查看电池电压/电量 |
| `battery_calib` | 进入电池校准模式 |
| `lp` | 查看低功耗状态 |

### 5.3.9 节拍器 / 鼓机 / 提示音命令

| 命令 | 用法 | 说明 |
|------|------|------|
| `metronome -b <bpm>` | 设置节拍器 BPM |
| `metronome -s <0/1>` | 启动/停止节拍器 |
| `drum -p <pattern>` | 播放鼓模式 |
| `remind -l` | 列出所有提示音 |
| `remind -p <id>` | 播放指定提示音 |
| `remind -n` | 播放开机提示音 |
| `pwr_music -p` | 播放开机音乐（测试） |
| `pwr_music -i` | 显示开机音乐信息 |

### 5.3.10 参数 / UI / 升级命令

| 命令 | 用法 | 说明 |
|------|------|------|
| `param -s` | 保存当前参数到 Flash |
| `param -l` | 从 Flash 加载参数 |
| `param -r` | 恢复出厂参数 |
| `ui -t` | 测试 UI 界面 |
| `upg` | 进入 USB CDC 固件升级模式 |

---

## 5.4 Shell IO 管理器

**文件**：`shell_io_manager.h/.c`

自动管理 USB CDC 和 BLE 两个 Shell 通信通道：

```c
// 初始化（USB CDC 优先，BLE 备用）
void ShellIOManager_Init(void);

// 每轮主循环调用，处理输入输出
void ShellIOManager_Process(void);
```

**工作逻辑**：
1. USB 连接时 → 使用 USB CDC 通道
2. USB 断开时 → 自动切换到 BLE 通道
3. 两个通道的数据最终送到同一个 Shell 解析器

---

## 5.5 Shell 命令开发示例

以下演示如何添加一个简单的 `hello` 命令：

```c
// hello_cmd.c
#include "bg_shell.h"

// 处理函数
static int hello_world(int argc, char *argv[]) {
    (void)argc;
    Shell_Print("Hello, World!\r\n");
    return 0;
}

static int hello_name(int argc, char *argv[]) {
    if (argc < 1) {
        Shell_Print("Usage: hello -n <name>\r\n");
        return -1;
    }
    Shell_Printf("Hello, %s!\r\n", argv[0]);
    return 0;
}

// 选项表
static const ShellOpt_t hello_opts[] = {
    OPT("w", "world", NULL,    "Print 'Hello, World!'",       hello_world),
    OPT("n", "name",  "<name>", "Print 'Hello, <name>!'",     hello_name),
    OPT_END()
};

// 模块定义
DEFINE_MODULE(hello, "Hello command example", MOD_CAT_DEBUG, hello_opts);

// 注册函数
void ShellCmdHello_Register(void) {
    REGISTER_MODULE(hello);
}
```

然后在 `Shell_RegisterAllModules()` 中添加：

```c
extern void ShellCmdHello_Register(void);
ShellCmdHello_Register();
```

---

> **下一章**：[第6章：音频效果器与 IO 管理器](06_音频效果器与IO管理器.md)
