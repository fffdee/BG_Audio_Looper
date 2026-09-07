# BanEffector 架构文档

> 本文档描述 **BanEffector** 从一代框架迁移到 **banux2 二代框架** 后的系统架构。
> 内容基于对源码树的实测核对（目录结构、音频图预设、启动流程、配置开关、蓝牙裁剪、内存占用）。

---

## 1. 项目概述

- **定位**：BanEffector 是一款**音频效果器**固件，从 BanBox 工程复制而来，沿用 BanBox 硬件，运行在 nds32（Andes D1088）+ FreeRTOS 平台上。
- **框架**：采用 **banux2 二代框架**（`Banux_Init` / `Banux_Process` 门面 + 组件/事件/驱动框架 + `bg_shell` V2.0.0 命令行）。
- **音频**：一代音频效果图引擎（`effect_graph`）+ `BG_AudioIO_Manager` 移植到二代框架，运行时拓扑简化为**直进直出（passthrough）**——效果器框架完整保留，后续加效果只需改拓扑与回调。
- **裁剪**：砍掉文件系统、蓝牙音频、Looper、节拍器、提示音、合成器；保留 USB 声卡、BLE 控制通道、存储与系统服务。
- **验证状态**：命令行全链接通过（EXIT 0，生成有效 ELF），0 硬错误、0 重复符号，关键符号（`main`/`Banux_Init`/`Banux_Process`/`MainTask`）均已定义。
- **版本控制**：`BanEffector/` 目前在 git 中为**未跟踪**状态（`?? BanEffector/`）。

---

## 2. 硬件与板级配置

板级在 `src/product_def.h` 顶部选择（三选一），当前激活 **`BANBOX_1_0_V2`**：

```c
// #define BANBOX_1_0       /* 旧板: 2x NOR Flash */
 #define BANBOX_1_0_V2    /* 存储版本二: 引脚不变, NOR#0→PSRAM, NOR#1→NAND */
//#define BANBOX_II       /* 新板 */
```

`BANBOX_1_0_V2` 的硬件驱动开关（`product_def.h` L36–46）：

| 宏 | 值 | 说明 |
|----|----|------|
| `HW_DRV_FLASH_NAND_EN` | 1 | W25N02 NAND Flash |
| `HW_DRV_PSRAM_EN`      | 1 | ESP-PSRAM64H PSRAM |
| `HW_DRV_FLASH_NOR_EN`  | 0 | 无 NOR（被 NAND+PSRAM 取代） |
| `HW_DRV_SDCARD_EN`     | 0 | 无 SD 卡 |
| `HW_DRV_LCD_EN`        | 0 | 无 LCD |
| `HW_DRV_BATTERY_EN`    | 1 | 电池管理 |
| `HW_DRV_USB_CDC_EN`    | 1 | USB CDC 串口 |
| `HW_DRV_BT_EN`         | 1 | 蓝牙/BLE |

---

## 3. 目录结构（banux2 五层）

源码根：`src/banux/`。五层骨架（`00`–`04`）来自 banux2 二代框架；原 `05_component/` 的一代系统组件（ble_app/firmware_upgrade/sys_led/sys_param/sys_state）已并入 `03_application_components/`，`05_component/` 目录已移除。

```
src/
├── product_def.h            板级选择 + 功能开关（覆盖 banux_config.h 默认值）
├── product_features.h       产品特性开关
├── build_config.h           构建配置
└── banux/
    ├── 00_core/             框架核心：Banux 门面、组件、调度器、调试、banux_config.h
    ├── 01_driver/           驱动层
    │   ├── bluetooth/       蓝牙栈（BLE-only，含 libBtStack.a）
    │   ├── hal/             adc / gpio / sdio / spi 硬件抽象
    │   ├── library/         battery/psram/sdcard/usb_cdc/w25n02/w25qxx + legacy_flash
    │   ├── power_mgr/       电源管理
    │   ├── usb/             USB 设备驱动
    │   └── driver_init.c    BanuxDriver_RegisterAll（注册 DrvDevice）
    ├── 02_system_components/
    │   ├── command_line/    bg_shell V2.0.0 + shell_io_cdc + shell_io_ble
    │   ├── command_parser/  命令解析（COMMAND_PARSER_EN 控制）
    │   ├── driver_framework/ 驱动框架（core + vfs）
    │   ├── event/           事件发布/订阅
    │   └── file_io/         路径式 IO（无 FS 依赖部分）
    ├── 03_application_components/
    │   ├── audio/
    │   │   ├── effect_graph/       效果图引擎 + 配置 + shell_cmd_graph + chain_graph_apply
    │   │   ├── BG_AudioIO_Manager/ 音频 IO 管理（bg_audio_init 等）
    │   │   └── app_audio/          effect_parameter / music_parameter
    │   ├── ble_app/                BLE 应用（控制通道）
    │   ├── firmware_upgrade/       OTA 升级引擎（CDC + BLE）
    │   ├── sys_led/                LED 状态
    │   ├── sys_param/              系统参数（NAND 持久化）
    │   └── sys_state/              系统状态机 / 电源时序
    └── 04_application/
        ├── main.c                  入口 + MainTask + BanuxConfig + power_on
        ├── app_sys_handler.c/.h    系统事件处理
        ├── app_version.h           版本号
        └── shell_cmd_*.c/.h        effect/flash/psram/param/mode/battery_calib/sysmon
```

---

## 4. 启动与运行流程

### 4.1 入口链路

```
main()
  ├─ #if HAS_BOOTLOADER  →  仅重建被 __c_init 清空的驱动软件状态
  │     WDG_Disable / DbgUartInit / Remap_InitTcm / SpiFlashInit / DMA_ChannelAllocTableSet
  │     （芯片/时钟/UART/SPI/DMA/TCM 已由 bootloader 配好，不重复初始化以免 PLL 失锁挂死）
  └─ #else               →  完整 Chip_Init(1) + 时钟树配置（PLL 288M / APLL 240M / USB 时钟）
        ↓
  SysState_Init()  →  创建 MainTask  →  vTaskStartScheduler()
```

### 4.2 MainTask（RTOS 主任务，`main.c` L457）

```
MainTask()
  1. SysState_SetIoConfig()        配置上电 IO 电平（LED GPIO）
  2. Banux_Init(&banux_cfg)        初始化二代框架：组件/事件/驱动框架/DrvDevice/Shell
  3. power_on()                    应用级上电时序（BUTTON_POWER_ENABLE 时先等按键）
  4. FwUpgrade_ConfirmBootSuccess()  清 boot_fail_cnt，告知 bootloader 本分区启动成功
     FwUpgrade_Init()                启动 OTA 升级引擎（CDC + BLE）
  5. while(1) Banux_Process()      主循环：每次一遍框架迭代（含 platformProcess）
```

### 4.3 Banux 门面配置（`BanuxConfig_t`，`main.c` L477–485）

| 字段 | 值 | 说明 |
|------|----|------|
| `logWriter`       | `App_LogWriter`          | 框架 DBG 日志路由到 UART printf |
| `shellIo`         | `ShellIO_CDC_Get()`      | Shell 走 USB-CDC，运行时经 ShellIOManager 自动切到 BLE |
| `filesystemInit`  | `NULL`                   | **无文件系统**（VFS/FatFs/InternalFlashFs 全关） |
| `driverInit`      | `BanuxDriver_RegisterAll`| 注册 USB-CDC / NAND / PSRAM / Battery 等 DrvDevice |
| `platformInit`    | `NULL`                   | 见下方说明——改由 MainTask 显式调 `power_on()` |
| `platformProcess` | `App_PlatformProcess`    | `Banux_Process()` 每轮回调的非阻塞应用迭代 |

> **为何 `platformInit = NULL`**：`Banux_Init()` 会在 `Shell_Init()` 与 `driverInit()` **之前**调用 `platformInit`；而 `Audio_Init()` 内部依赖 Shell（`ShellIOManager_Init`），`SysParam`/`BLE` 依赖驱动框架。因此完整的 `power_on()` 时序被显式放到 `Banux_Init()` **返回之后**的 MainTask 中执行，以保留一代“框架初始化 → RTOS 上下文中 power_on”的顺序。

---

## 5. 音频处理图（核心）

### 5.1 运行时拓扑：直进直出（passthrough）

默认加载 **6 节点 5 边**的极简拓扑，无任何效果节点：

```
   ADC0 (guitar) ─┐
   ADC1 (mic)    ─┼──> Mixer ──┬──> DAC0 (speaker)
   USB_IN        ─┘            └──> USB_OUT
```

- 输入：吉他（ADC0）、麦克风（ADC1）、USB 声卡输入（USB_IN）
- 输出：扬声器（DAC0）、USB 声卡输出（USB_OUT）
- 定义位置：`effect_graph_config.h` 的 `SIMPLE_NODES_CONFIG` / `SIMPLE_EDGES_CONFIG`（L262–285）

### 5.2 预设系统（`GraphPreset_t`）

`effect_graph_config.h` 定义了多套预设，运行时通过 `EffectGraphConfig_LoadPreset()` 选择：

| 预设 | 拓扑 | 状态 |
|------|------|------|
| `GRAPH_PRESET_DEFAULT` | **= 直进直出**（显式指向 `g_SimpleNodes`/`g_SimpleEdges`，6 节点 5 边） | ✅ 运行时默认 |
| `GRAPH_PRESET_SIMPLE`  | 直进直出（同上） | ✅ |
| `GRAPH_PRESET_BLUETOOTH` | BT_In → EQ → DAC0（3 节点） | 保留 |
| `GRAPH_PRESET_SECONDARY_SPEAKER` | 副音箱多路混音（8 节点，无效果/无 Looper） | 保留（`shell_cmd_mode` 副音箱模式） |
| `GRAPH_PRESET_GUITAR_ONLY` / `MIC_ONLY` / `USB_AUDIO` | 未实现（TODO），回退到完整图 | 未启用 |

**关键设计**：`effect_graph_config.c` 中 `case GRAPH_PRESET_DEFAULT` **显式**把 `config->nodes/edges` 指向 `g_SimpleNodes`/`g_SimpleEdges`（而非完整图 `g_DefaultNodes`），并打印 `"DEFAULT preset (passthrough)"`。因此所有调用 `LoadPreset(GRAPH_PRESET_DEFAULT)` 的地方（`bg_audio_init.c` 启动、`effect_graph.c` 初始化、`shell_cmd_mode.c` 主音箱）都得到直进直出拓扑。

> **完整图为休眠态**：`DEFAULT_NODES_CONFIG`（23 边，含 EQ/Reverb/DRC/Expander/BT_In/Metronome/Remind/Looper）仍编译进 rodata，但**运行时不激活**，仅被未实现的 TODO 预设引用。保留它可让未来“加效果”直接复用现成节点定义。

### 5.3 如何扩展效果

后续要加效果（如 EQ/Reverb），**无需改动引擎或回调框架**，只需：
1. 在 `effect_graph_config.h` 的 `SIMPLE_*` 拓扑中插入效果节点并改写边；
2. 在 `bg_graph_setup.c` 中补上对应节点的处理回调。
效果上下文（如 reverb 的大内存）只在拓扑实际含该节点时才会被分配。

---

## 6. 蓝牙（BLE-only）

蓝牙栈整体保留（`01_driver/bluetooth/` + `libBtStack.a`），通过 `bt_config.h` 配置为 **BLE-only**：

```c
#define BLE_SUPPORT          ENABLE      // BLE 控制通道保留
#define BT_A2DP_SUPPORT      DISABLE     // 关闭 A2DP 音频
#if (BT_A2DP_SUPPORT == ENABLE)
    ... // AVRCP=ENABLE, SPP=ENABLE
#else
#define BT_AVRCP_SUPPORT     DISABLE     // ← A2DP 关闭时级联生效
#define BT_HFP_SUPPORT       DISABLE
#define BT_SPP_SUPPORT       DISABLE
#endif
//#define BT_RECONNECTION_FUNC           // 已注释关闭（避免经典蓝牙回连）
```

- **保留**：BLE（`ble_app`）、`bt_stack_service`、`bt_platform_interface`、`shell_io_ble`、HID/MFi/PBAP 源码模块。
- **砍掉**：`bt_a2dp_app` / `bt_hfp_app` / `bt_avrcp_app` / `audio_decoder_api` / `bt_vfs_driver` 等经典蓝牙音频应用源文件。
- **效果**：A2DP=DISABLE 经 `#else` 级联使 AVRCP/HFP/SPP 全部 DISABLE，实现纯 BLE 控制通道，无蓝牙音频链路。

---

## 7. 无文件系统

二代框架**不启用任何文件系统**：

| 开关 | 值 | 位置 |
|------|----|------|
| `VFS_EN`                   | 0 | `product_def.h` L123 |
| `BANUX_FATFS_EN`           | 0 | `product_def.h` L352 |
| `BANUX_INTERNAL_FLASH_FS_EN` | 0 | `product_def.h` L353 |
| `filesystemInit`           | NULL | `main.c` BanuxConfig |

- 已砍：VFS / FatFs / internal_flash_fs / fat32 / effect_graph_vfs / shell_fs。
- `file_io` / `banux_io` 仅在不引入 FS 依赖的前提下保留。
- 参数持久化走 `sys_param`（直接读写 NAND），不经文件系统。

---

## 8. 配置开关覆盖机制

`00_core/banux_config.h` 集中管理框架默认开关，**全部用 `#ifndef` 防护**，并在文件顶部**先 include `product_def.h`**：

```c
/* banux_config.h */
#include "product_def.h"          // 板级开关先定义（L18）

#ifndef VFS_EN
#define VFS_EN  1                 // 框架默认值（被 product_def.h 覆盖）
#endif
#ifndef EFFECT_GRAPHICS_EN
#define EFFECT_GRAPHICS_EN  0     // 框架默认值（被 product_def.h 覆盖）
#endif
```

由于 `product_def.h` 先被包含并已 `#define VFS_EN 0` / `EFFECT_GRAPHICS_EN 1`，`banux_config.h` 的 `#ifndef` 默认值被跳过。**最终生效值以板级 `product_def.h` 为准**：

| 开关 | banux_config.h 默认 | product_def.h 覆盖 | 实际生效 |
|------|:---:|:---:|:---:|
| `VFS_EN`            | 1 | 0 | **0** |
| `EFFECT_GRAPHICS_EN`| 0 | 1 | **1** |

> 两个头文件互相 include 是安全的（都有 include-guard）。banux_config.h 中残留的 3D 打印机宏（`BANUX_GCODE_EN`/`GCODE_*`/`STEPPER_*`）均防护为 0，属休眠状态，可忽略。

---

## 9. Shell 命令系统

- **传输**：`command_line`（`bg_shell` V2.0.0）承载于 USB-CDC，运行时经 `ShellIOManager` 自动切换到 BLE（`shell_io_ble`）。
- **保留的命令**（`04_application/shell_cmd_*`）：

| 命令 | 功能 |
|------|------|
| `shell_cmd_effect`   | 音效参数调节 |
| `shell_cmd_graph`    | 音频图预设查看/切换（位于 effect_graph/） |
| `shell_cmd_flash`    | NAND/NOR Flash 读写测试 |
| `shell_cmd_psram`    | PSRAM 测试 |
| `shell_cmd_param`    | 系统参数读写 |
| `shell_cmd_mode`     | 主/副音箱模式切换（加载对应图预设） |
| `shell_cmd_battery_calib` | 电池校准 |
| `shell_cmd_sysmon`   | 系统监控 |

- **已砍命令**：`shell_cmd_{fat,wav,wav_ble,drum,metronome,soundbank,lp}`、`shell_fs`、`app_event_example`。

---

## 10. 内存占用与构建

### 10.1 内存占用（Phase 6 命令行全链接实测）

| 段 | 字节 | 说明 |
|----|------|------|
| `text` | 739,412 | 代码 594,916 + rodata 144,496 |
| `data` | 5,360   | 已初始化全局 |
| `bss`  | 129,508 | 静态 RAM ≈ 135 KB |
| 镜像   | ≈ 2.3 MB | `_fulllink.adx`（有效 ELF） |

链接输入：30 个 SDK 目标 + 105 个 banux 目标 + 8 个库
（`-lDriver -lAudioDecoderLibrary -lsra -lAudioEffectLibrary -lresampler -ldsp -lm -lBtStack`）。

### 10.2 构建方式

- **IDE**：AndeSight 打开工程 → **Clean + Build**（`Debug/makefile` 与各 `subdir.mk` 会按新目录树重新生成）。
- **工程名**：`.project` 中为 `BanEffector`。
- **include 路径**：`.cproject` 已从 `/BanBox/…` 全量重写为 `/BanEffector/…` 新五层布局；库搜索路径中蓝牙已指向 `01_driver/bluetooth`。
- **SDK 引用**：`Debug/makefile` 对 `middleware/{audio,rtos,mv_utils}`、`driver/driver_api`、`startup` 的引用保持不变。

---

## 11. 相比 BanBox 的裁剪清单

| 类别 | 砍掉 | 保留 |
|------|------|------|
| 文件系统 | VFS/FatFs/internal_flash_fs/fat32/effect_graph_vfs/shell_fs | — |
| 蓝牙音频 | A2DP/HFP/AVRCP/audio_decoder/bt_vfs | BLE 控制、HID/MFi/PBAP 源、libBtStack.a |
| 音频功能 | Looper/Metronome/Remind 提示音/synth/soundbank/drum/audio_spectrum/pitch_shift | effect_graph 引擎、Mixer/Passthrough、USB 声卡 |
| 硬件 | LCD/SD 卡/NOR（本板级） | NAND/PSRAM/Battery/USB-CDC/ADC/DAC |
| 框架 | 一代 `01_vfs`/`03_driver_framework`/`04_shell_commands/bg_shell*`（被二代取代） | banux2 五层框架、事件系统、驱动框架 |

---

## 12. 已知遗留与注意事项

1. **休眠的完整音频图**：`DEFAULT_NODES_CONFIG`（含 BT/Metronome/Remind/Looper 节点）仍在 `effect_graph_config.h` 中定义并编译进 rodata，但运行时不激活（`GRAPH_PRESET_DEFAULT` 已改指 passthrough）。如需彻底瘦身可后续删除，但会牵动 `COMPILE_TIME_ASSERT` 与 `DEFAULT_NODE_COUNT`，非必要不动。
2. **陈旧注释**：`03_application_components/sys_param/sys_param.c`（约 L59–61）注释仍称 `GRAPH_PRESET_DEFAULT` 加载“含 ADC_Mixer 4 路输入和 Looper 连接”的完整图——这是 BanBox 遗留文案，与实际代码（passthrough）不符，仅注释误导，无功能影响。
3. **隐式声明遗留债**：约 11 个 BanBox 沿袭源文件存在 C89 隐式声明警告（如 `ble_app_sync.c` 未 include `string.h`），与 BanBox 现状一致，属迁移范围外的既有债务，未改动。
4. **未实现预设**：`GRAPH_PRESET_GUITAR_ONLY`/`MIC_ONLY`/`USB_AUDIO` 为 TODO，当前会回退到完整图；如需启用应先在 `effect_graph_config.c` 补齐其节点/边配置。

---

*文档随源码实测生成，如后续修改拓扑或开关，请同步更新本文。*
