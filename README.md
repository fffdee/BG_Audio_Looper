# BG_Audio_Looper 工程文档

> **项目性质**: 临时工程，用于音频循环录制（Looper）及相关功能开发
> **芯片平台**: Mountain View Silicon BP10xx 系列（NDS32 架构）
> **操作系统**: FreeRTOS

---

## 目录

- [BG\_Audio\_Looper 工程文档](#bg_audio_looper-工程文档)
  - [目录](#目录)
  - [1. 工程概述](#1-工程概述)
    - [技术栈](#技术栈)
  - [2. 工程结构](#2-工程结构)
  - [3. BanBox 主工程](#3-banbox-主工程)
    - [3.1 核心功能模块](#31-核心功能模块)
    - [3.2 硬件驱动层](#32-硬件驱动层)
    - [3.3 音频组件层](#33-音频组件层)
      - [Audio Looper - 音频循环录制](#audio-looper---音频循环录制)
      - [Effect Graph - 音频效果链路](#effect-graph---音频效果链路)
      - [Audio Spectrum - 音频频谱分析](#audio-spectrum---音频频谱分析)
    - [3.4 UI 系统 (BanGUI)](#34-ui-系统-bangui)
    - [3.5 MIDI 合成器 (BanGTsynth)](#35-midi-合成器-bangtsynth)
    - [3.6 其他组件](#36-其他组件)
  - [4. BT\_Audio\_APP 工程](#4-bt_audio_app-工程)
    - [应用模式](#应用模式)
    - [音频处理](#音频处理)
    - [外设驱动](#外设驱动)
    - [AI 语音](#ai-语音)
  - [5. MVsB1\_Base\_SDK](#5-mvsb1_base_sdk)
    - [驱动库](#驱动库)
    - [文档](#文档)
    - [SDK 版本历史](#sdk-版本历史)
  - [6. 硬件配置](#6-硬件配置)
    - [板级支持](#板级支持)
      - [BANBOX\_1\_0（旧版）](#banbox_1_0旧版)
      - [BANBOX\_1\_0\_V2（旧版 V2）](#banbox_1_0_v2旧版-v2)
      - [BANBOX\_II（新版）](#banbox_ii新版)
    - [DMA 通道分配](#dma-通道分配)
    - [引脚配置（BANBOX\_1\_0\_V2）](#引脚配置banbox_1_0_v2)
  - [7. 构建说明](#7-构建说明)
    - [开发环境](#开发环境)
    - [构建步骤](#构建步骤)
    - [链接脚本](#链接脚本)
    - [宏定义配置指南](#宏定义配置指南)
  - [8. 版本历史](#8-版本历史)
    - [SDK 版本](#sdk-版本)
  - [附录](#附录)
    - [架构文档](#架构文档)
    - [关键配置宏](#关键配置宏)

---

## 1. 工程概述

本工程是基于 **Mountain View Silicon BP10xx** 系列芯片的音频处理平台，主要功能包括：

- **Audio Looper**: 多段音频循环录制与播放，支持叠录（Overdub）、节拍器、段音量控制、裁剪功能
- **BanGUI UI 系统**: 基于 LCD 的图形用户界面，包含开机动画、主界面、菜单、Looper 界面
- **BanGTsynth MIDI 合成器**: 模块化音频合成器框架，支持 SF2/BGS 音源
- **Effect Graph**: 可配置的音频效果链路（混响、DRC、EQ、延迟等）
- **蓝牙音频**: A2DP/HFP/BLE 蓝牙协议栈
- **USB Audio**: USB CDC/UAC 音频设备
- **多存储介质**: NOR Flash / NAND Flash / PSRAM / SD Card

### 技术栈

| 项目 | 说明 |
|------|------|
| 芯片 | BP10xx 系列（BP1064L2 等） |
| 架构 | NDS32（Andes Technology） |
| IDE | AndeSight |
| OS | FreeRTOS |
| 系统时钟 | 288MHz / 320MHz（Karaoke 模式） |
| 音频采样率 | 44.1kHz / 48kHz |
| 编译器 | NDS32 GCC（C89 兼容） |

---

## 2. 工程结构

```
BG_Audio_Looper/
├── BanBox/                      # 主工程：BanBox 音频卡固件
│   ├── src/                     # 源代码
│   │   ├── main.c               # 主入口（FreeRTOS 启动）
│   │   ├── product_def.h        # 产品定义与硬件配置
│   │   └── banux/               # 模块化架构目录
│   │       ├── 01_vfs/          # 虚拟文件系统
│   │       ├── 02_device_drivers/  # 硬件设备驱动
│   │       ├── 03_driver_framework/ # 驱动框架
│   │       ├── 04_shell_commands/   # Shell 命令层
│   │       └── 05_component/    # 音频组件层
│   ├── drum_set/                # 鼓组音源 (tip.sf2)
│   ├── tip_sound/               # 开关机提示音
│   └── nds32-ae210p.ld          # 链接脚本
│
├── BT_Audio_APP/                # 蓝牙音频应用工程（SDK 示例）
│   ├── bt_audio_app_src/        # 源代码
│   │   ├── main.c               # 入口
│   │   ├── apps/                # 应用模式（蓝牙、LineIn、Radio 等）
│   │   ├── audio/               # 音频处理（AEC、EQ、音量）
│   │   ├── device/              # 外设驱动（按键、ADC、IR、升级）
│   │   ├── display/             # 显示驱动（SEG、LED）
│   │   ├── ble/                 # BLE 应用
│   │   ├── ai/                  # AI 语音（小米、Speex）
│   │   └── libopus/             # Opus 编解码库
│   └── remind_file/             # 提示音 MP3 文件
│
├── MVsB1_Base_SDK/              # SDK 基础库
│   ├── driver/                  # 芯片底层驱动
│   └── documents/               # 芯片手册与开发指南
│
└── bootloader/                  # Bootloader 工程（可选）
```

---

## 3. BanBox 主工程

### 3.1 核心功能模块

**入口文件**: [main.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/main.c)

主流程：
1. 芯片初始化 → 时钟配置 → 看门狗
2. SPI 硬件初始化 → 驱动框架全量初始化
3. FreeRTOS 任务创建 → MainTask 启动
4. MainTask 中：
   - 系统参数加载（Flash）
   - BanGTsynth 合成器初始化
   - 音频管理器初始化（44.1kHz）
   - NAND FAT32 文件系统初始化
   - 事件系统初始化
   - BanGUI UI 系统启动
   - 主循环：音频处理 + UI 更新 + CDC 文件管理 + BLE 同步

**产品定义**: [product_def.h](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/product_def.h)

支持多板级配置：
- `BANBOX_1_0`: 旧版，2×NOR Flash
- `BANBOX_1_0_V2`: 旧版 V2，NAND + PSRAM（复用旧引脚）
- `BANBOX_II`: 新版，NOR + NAND + PSRAM + SD Card

### 3.2 硬件驱动层

**目录**: `BanBox/src/banux/02_device_drivers/`

| 驱动 | 文件 | 说明 |
|------|------|------|
| **LCD** | [st7735.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/lcd/st7735.c), [bg_lcd.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/lcd/bg_lcd.c) | ST7735 LCD 驱动 + 适配层 + 帧缓冲 |
| **Flash** | [flash_nor_w25qxx.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/flash/flash_nor_w25qxx.c), [flash_nand_w25n02.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/flash/flash_nand_w25n02.c), [psram_esp64h.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/flash/psram_esp64h.c) | NOR/NAND Flash + PSRAM 驱动 + 管理器 |
| **USB** | [otg_device_audio.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/USB/src/otg_device_audio.c), [usb_audio_api.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/USB/src/usb_audio_api.c) | USB OTG Audio/CDC/Storage |
| **Bluetooth** | [bt_stack_service.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/bluetooth/src/bt_stack_service.c), [bt_manager.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/bluetooth/src/bt_manager.c) | 蓝牙协议栈管理（A2DP/HFP/AVRCP/BLE） |
| **Power** | [battery_drv.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/power_mgr/battery_drv.c), [pwr_btn.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/power_mgr/pwr_btn.c) | 电池管理 + 电源按钮 |
| **SD Card** | [sd_card_driver.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/sdcard/sd_card_driver.c) | SDIO SD Card 驱动 |
| **Remind Sound** | [remind_sound.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/remind_sound/remind_sound.c) | 开关机提示音（内嵌音频数据） |

**驱动框架**: `03_driver_framework/`
- 虚拟文件系统（VFS）- UNIX 风格目录结构 `/driver/`, `/bin/`
- 设备注册管理
- 自动驱动注册（SPI LCD、Flash、Battery、USB CDC）

### 3.3 音频组件层

**目录**: `BanBox/src/banux/05_component/`

#### Audio Looper - 音频循环录制

**核心文件**: [audio_looper.h](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/audio_looper/audio_looper.h), [audio_looper.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/audio_looper/audio_looper.c)

功能特性：
- **多段录制**: 最多 4 段独立音频段
- **叠录（Overdub）**: 支持在已有段上叠加录制
- **节拍器**: BPM 60-200，可配置拍号、音量、音色
- **段控制**: 独立音量（0-100）、裁剪（Trim Start/End Page）
- **多 Flash 支持**: 段可绑定不同 Flash 芯片，实现边播边录
- **IO 缓冲**: 解耦音频回调与 Flash IO，避免帧超时
- **存储抽象层**: 自动适配 NOR/NAND/PSRAM
- **WAV 导出**: 支持将录制内容导出为 WAV 文件

存储后端：
- [looper_storage_nor.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/audio_looper/looper_storage_nor.c) - NOR Flash 后端
- [looper_storage_nand.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/audio_looper/looper_storage_nand.c) - NAND Flash 后端
- [looper_storage_psram.c](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/audio_looper/looper_storage_psram.c) - PSRAM 后端（支持叠录）

#### Effect Graph - 音频效果链路

**文档**: [EFFECT_GRAPH_README.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/effect_graph/EFFECT_GRAPH_README.md)

功能特性：
- **图结构音频路由**: 支持 4 输入源（ADC0/ADC1/USB_IN/BT_IN）→ 2 输出（DAC0/USB_OUT）
- **效果节点**: 混音器、混响（Reverb）、DRC、EQ、扩展器、延迟
- **拓扑排序**: 自动计算处理顺序
- **参数化配置**: 通过配置文件定义节点和连接，无需改代码
- **Shell 命令控制**: `graph list`, `graph param`, `graph preset` 等
- **预设模式**: Default/Simple/Guitar Only/Mic Only/Bluetooth Speaker/USB Audio

#### Audio Spectrum - 音频频谱分析

**文件**: [audio_spectrum.h](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/audio_spectrum/audio_spectrum.h)

### 3.4 UI 系统 (BanGUI)

**入口**: [bangui.h](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/bangui.h)

架构分层：
```
ui/
├── core/              # 核心层
│   ├── bg_ui.c/h          # UI 主对象（状态机、视图管理）
│   ├── ui_page.c/h        # 页面管理器
│   └── bg_page_compat.c/h # 旧 API 兼容层
├── components/        # 组件层
│   ├── comp_statusbar.c/h # 状态栏（蓝牙、电池、ADC 检测）
│   ├── comp_popup.c/h     # 弹窗组件
│   ├── comp_menu.c/h      # 菜单组件
│   └── ui_system.c/h      # 旧 UI 系统兼容层
├── views/             # 视图层
│   ├── view_boot.c/h      # 开机动画（Logo + 进度条）
│   ├── view_home.c/h      # 主界面（图标网格）
│   ├── view_menu.c/h      # 菜单界面
│   ├── view_looper.c/h    # Looper 界面
│   └── app_pages.c/h      # 应用页面定义
└── resources/         # 资源层
    ├── ui_icons.h         # 图标定义
    ├── ui_fonts.h         # 字体定义
    └── picture.h          # 图片资源
```

便捷宏：
```c
BANGUI_QUICK_INIT();           // 快速初始化
BANGUI_START(UI_STATE_BOOT);   // 从开机画面启动
BG_UI.Update(20);              // 20ms 周期更新
```

### 3.5 MIDI 合成器 (BanGTsynth)

**文档**: [README.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/bangtsynth/README.md)

设计理念：
1. **模块化功能** - 每个功能独立封装
2. **可配置裁剪** - 编译时功能裁剪，适配不同资源
3. **抽象化接口** - HAL 层设计，跨平台移植

架构：
```
应用层 (bangtsynth_node)
    ↓
音源管理器 (soundbank_manager)
    ↓
组件层 (MIDI 控制器 / 采样器 / 包络 / 音频处理器)
    ↓
HAL 接口层 (存储 / 下载 / OSAL / 日志)
    ↓
平台实现 (BP10 / Linux / Embedded)
```

核心模块：
- **01_hal/**: 硬件抽象层（BP10 移植、存储驱动、下载协议）
- **02_core/**: 核心组件
  - `midi/` - MIDI 控制器 + USB MIDI 标准请求
  - `sampler/` - 采样器
  - `soundbank/` - SF2/BGS 音源解析器 + 音源管理器
  - `envelope/` - 包络发生器
  - `fat32/` - FAT32 音源读取 + PSRAM 堆管理
  - `psram_buffer/` - PSRAM 缓冲区
  - `nand_store/` - NAND 存储
  - `synth_integration/` - 合成器集成（SD+NAND+PSRAM 预热）
- **03_app/**: 应用层
  - `synth_node/` - 合成器节点（音频处理流水线集成）
  - `drum_machine/` - 鼓机

音源格式：
- **SF2**: SoundFont 2 标准格式解析
- **BGS**: BanGO 自定义音源格式（支持速度层分层）

### 3.6 其他组件

| 组件 | 说明 |
|------|------|
| **sys_param** | 系统参数存储（Flash 持久化） |
| **fat32** | NAND FAT32 文件系统 |
| **BanGUI base_func** | GUI 基础功能（字体渲染、LCD 适配、Shell-LCD 桥接） |
| **Shell Commands** | UART/CDC/BLE 命令行系统（`bg_shell.c`, `shell_fs.c`, `shell_io_ble.c`） |

---

## 4. BT_Audio_APP 工程

**目录**: `BT_Audio_APP/`

这是 Mountain View SDK 提供的蓝牙音频参考应用，包含完整的产品级功能。

### 应用模式

| 模式 | 文件 | 说明 |
|------|------|------|
| 蓝牙播放 | [bt_play_mode.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/apps/bt_play_mode.c) | A2DP 音乐播放 |
| 蓝牙免提 | [bt_hf_mode.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/apps/bt_hf_mode.c) | HFP 通话 |
| LineIn | [linein_mode.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/apps/linein_mode.c) | 模拟音频输入 |
| 收音机 | [radio_mode.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/apps/radio_mode.c) | FM 收音（QN8035/RDA5807） |
| HDMI 输入 | [hdmi_in_mode.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/apps/hdmi_in_mode.c) | HDMI 音频 |
| SPDIF | [spdif_mode.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/apps/spdif_mode.c) | 数字音频输入 |
| I2S 输入 | [i2sin_mode.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/apps/i2sin_mode.c) | I2S 从模式 |
| 待机 | [rest_mode.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/apps/rest_mode.c) | 低功耗待机 |

### 音频处理

| 模块 | 文件 | 说明 |
|------|------|------|
| AEC | [audio_aec.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/audio/audio_aec.c) | 回声消除 |
| 音量控制 | [audio_vol.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/audio/audio_vol.c) | 音量管理 |
| EQ 参数 | [eq_params.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/audio/eq_params.c) | 均衡器参数 |
| Beep | [beep.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/audio/beep.c) | 提示音 |
| 重采样 | [ai_resample.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/ai_resample.c) | 采样率转换 |

### 外设驱动

| 驱动 | 说明 |
|------|------|
| [key.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/device/key.c) | 按键管理（ADC/IR/IO/Code Key） |
| [adc_key.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/device/adc_key.c) | ADC 按键 |
| [ir_nec_key.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/device/ir_nec_key.c) | IR NEC 遥控 |
| [upgrade.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/device/upgrade.c) | 固件升级 |
| [deepsleep.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/device/deepsleep.c) | 深度睡眠 |
| [rtc_ctrl.c](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_Looper/BT_Audio_APP/bt_audio_app_src/device/rtc_ctrl.c) | RTC 控制 |

### AI 语音

- **小米 AI**: `xiaomi_ai/` - 小米语音服务集成
- **Speex**: 音频编解码库
- **Opus**: `libopus/` - Opus 编解码

---

## 5. MVsB1_Base_SDK

**目录**: `MVsB1_Base_SDK/`

芯片原厂 SDK，提供底层驱动和开发文档。

### 驱动库

`driver/driver/inc/` 包含所有芯片外设驱动头文件：
- ADC / Audio ADC / DAC / I2S
- GPIO / SPI / I2C / SDIO
- Timer / PWM / PPWM
- UART / IR
- RTC / Backup / EFUSE
- USB OTG (Device/Host)
- Power Management (PWC/LDO)
- LCD SEG / FFT / DMA / Random

### 文档

| 文档 | 说明 |
|------|------|
| BP10系列芯片使用手册 | 芯片 datasheet |
| BP10系列SDK使用手册 | SDK 使用指南 |
| AndeSight使用指南 | IDE 使用指南 |
| 蓝牙应用开发FAQ | 蓝牙开发常见问题 |

### SDK 版本历史

详见 [MVsB1_Base_SDK_history.txt](file:///e:/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/MVsB1_Base_SDK_history.txt)

当前基于 **MVsB1_BT_Audio_SDK_v0.1.12** 系列。

---

## 6. 硬件配置

### 板级支持

#### BANBOX_1_0（旧版）
- 2× W25Qxx NOR Flash
- ST7735 LCD
- 电池管理（ADC 检测）
- USB CDC
- 蓝牙/BLE
- LineIn（双路）+ 麦克风（双路）
- 音量旋钮 ADC

#### BANBOX_1_0_V2（旧版 V2）
- W25N02 NAND Flash（复用旧 NOR#1 引脚 A22）
- ESP-PSRAM64H PSRAM（复用旧 NOR#0 引脚 A21）
- 其余同 BANBOX_1_0

#### BANBOX_II（新版）
- 1× W25Qxx NOR Flash（A28）
- W25N02 NAND Flash（A29）
- ESP-PSRAM64H PSRAM（B6）
- SD Card SDIO（A15/A16/A17）
- ST7735 LCD
- 电池管理
- USB CDC
- 蓝牙/BLE
- LineIn（双路，无麦克风）
- 按钮开机（长按 1 秒）

### DMA 通道分配

| 外设 | 通道 |
|------|------|
| Audio ADC0 RX | 0 |
| Audio ADC1 RX | 1 |
| Audio DAC0 TX | 2 |
| Audio DAC1 TX | 3 |
| SDIO RX/TX | 4 |
| SPIM RX/TX | 0/1（半双工复用） |
| UART1 RX/TX | 7/6 |

### 引脚配置（BANBOX_1_0_V2）

| 功能 | 引脚 |
|------|------|
| PSRAM CS | GPIO_A21 |
| NAND CS | GPIO_A22 |
| 音量 ADC | GPIO_A28 (ADC_CHANNEL_GPIOA28) |
| 电池 ADC | GPIO_A31 (ADC_CHANNEL_GPIOA31) |
| UART TX | GPIOA10 |
| UART RX | GPIOA9 |
| 电源按钮 | GPIO_A23 |
| GPIO 输出 #20 | GPIO_A20 |
| GPIO 输出 #24 | GPIO_A24 |

---

## 7. 构建说明

### 开发环境

- **IDE**: AndeSight（Andes Technology）
- **工具链**: NDS32 GCC
- **调试器**: JTAG/SWD
- **烧录工具**: PC Tools / MVAssistant

### 构建步骤

1. 在 AndeSight 中导入工程（`.cproject` / `.project`）
2. 选择目标板级配置（修改 `product_def.h` 中的宏定义）
3. 构建配置：`Demo_FreeRTOS Debug`
4. 编译：`Build Project`
5. 烧录：使用 PC Tools 或 Flash Boot 升级

### 链接脚本

`nds32-ae210p.ld` - NDS32 AE210P 平台链接脚本

### 宏定义配置指南

详见 [头文件宏定义编写指南.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/头文件宏定义编写指南.md)

---

## 8. 版本历史

### SDK 版本

| 日期 | 版本 | 主要更新 |
|------|------|----------|
| 2022-02-14 | v0.1.12_p03 | FlashBoot v2.2.3、音频通路优化、HDMI 修复、MediaPlayer 修复 |
| 2021-10-25 | v0.1.12_p02 | 混音优化、播放效果修复 |
| 2021-05-12 | v0.1.12_p01 | Flash 协议 V1.1、AEC 优化、USB Phone 模式 |
| 2021-02-08 | v0.1.12 | FlashBoot v2.2.0、AEC v5.7.2w、HDMI/CEC、MediaPlayer 增强 |
| 2020-07-06 | v0.1.11 | 蓝牙稳定性优化、音效 v1.33.5、系统资源优化 |
| 2020-05-26 | v0.1.10 | 蓝牙通话增强、FlashBoot v2.1.4、录音优化 |

详见：
- [BT_Audio_APP_history.txt](file:///e:/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/BT_Audio_APP_history.txt)
- [MVsB1_Base_SDK_history.txt](file:///e:/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/MVsB1_Base_SDK_history.txt)

---

## 附录

### 架构文档

| 文档 | 路径 |
|------|------|
| VFS 架构 | [VFS_ARCHITECTURE.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/VFS_ARCHITECTURE.md) |
| 架构重组 | [ARCHITECTURE_REORGANIZATION.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/ARCHITECTURE_REORGANIZATION.md) |
| BanUX 重组报告 | [BANUX_REORGANIZATION_REPORT.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/BANUX_REORGANIZATION_REPORT.md) |
| BanGUI UI 重构 | [UI_REFACTOR_DOC.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/UI_REFACTOR_DOC.md) |
| Effect Graph 集成 | [EFFECT_GRAPH_INTEGRATION_GUIDE.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/effect_graph/EFFECT_GRAPH_INTEGRATION_GUIDE.md) |
| BanGTsynth 架构 | [ARCHITECTURE.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/bangtsynth/ARCHITECTURE.md) |
| Flash 管理器 | [BG_FLASHMGR_README.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/flash/BG_FLASHMGR_README.md) |
| USB CDC | [CDC_README.md](file:///e:/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/USB/CDC_README.md) |

### 关键配置宏

| 宏 | 说明 | 默认值 |
|----|------|--------|
| `BANBOX_1_0_V2` | 板级选择 | 启用 |
| `LOOPER_STORAGE_TYPE` | Looper 存储类型 | 0（自动检测） |
| `BANGTSYNTH_EN` | MIDI 合成器 | 0（禁用） |
| `FAT32_EN` | FAT32 文件系统 | 0（禁用） |
| `CDC_FILE_MANAGER_EN` | CDC 文件管理器 | 0（禁用） |
| `BUTTON_POWER_ENABLE` | 按钮开机 | 0（上电开机） |
| `HW_DRV_LCD_EN` | LCD 驱动 | 0（V2 板禁用） |
| `HW_DRV_PSRAM_EN` | PSRAM 驱动 | 1（V2 板启用） |
| `HW_DRV_FLASH_NAND_EN` | NAND Flash 驱动 | 1（V2 板启用） |

---

*本文档由工程代码自动生成，最后更新：2026-04-29*
