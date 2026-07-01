# BanGTsynth 分层架构

## 目录结构

```
bangtsynth/
├── bg_config.h                 # 中央配置 (功能开关、平台选择、参数)
│
├── common/                     # 共享类型定义
│   └── err_handle.h            # 错误码枚举 (BG_ERR)
│
├── 01_hal/                     # ★ 硬件抽象层 (移植只改这里)
│   ├── bg_hal.h                # HAL 统一接口定义
│   ├── bg_storage.h/c          # 存储抽象接口 + 路由
│   ├── bg_log.h/c              # 日志抽象接口
│   ├── bg_download_port.h      # 下载端口抽象接口
│   ├── hardware_interfance.h/c # 旧版兼容接口 (BGS格式依赖)
│   ├── bg_soundbank_dl_protocol.h
│   ├── bg_soundbank_download.h
│   └── port/                   # ★ 平台实现 (一个目录 = 一个平台)
│       ├── bp10/               # BP10 NDS32 实现
│       │   ├── bg_storage_bp10.c
│       │   └── bg_download_port_bp10.c
│       ├── linux/              # Linux 开发/调试实现
│       │   ├── bg_storage_linux.c
│       │   ├── bg_download_port_linux.c
│       │   ├── bg_hal_linux.c
│       │   └── bg_hal_filesystem_linux.c
│       └── embedded/           # 嵌入式通用实现
│           └── bg_storage_embedded.c
│
├── 02_core/                    # 核心引擎 (平台无关)
│   ├── soundbank/              # 音源管理 + 格式解析
│   │   ├── soundbank_manager.h/c  # 统一音源接口
│   │   ├── sf2_parser.h/c        # SoundFont 2 解析器
│   │   └── bgs_parser.h/c        # BGS 自有格式解析器
│   ├── midi/                   # MIDI 协议处理
│   │   ├── midi_controller.h/c    # MIDI 消息分发
│   │   ├── midi_info.h            # MIDI 数据类型
│   │   └── standard_request_processing.h/c  # NoteOn/Off/CC 处理
│   └── envelope/               # 包络生成器
│       └── bg_envelope.h/c        # ADSR 包络
│
├── 03_app/                     # 应用层模块
│   ├── synth_node/             # 合成器节点 (Effect Graph 桥接)
│   │   └── bangtsynth_node.h/c    # 顶层合成器接口
│   └── drum_machine/           # 鼓机音序器
│       └── drum_machine.h/c       # 16步×8轨鼓机
│
└── data/                       # 嵌入二进制资源
    ├── soundbank_data/         # 合成器音源数据
    │   ├── sf2_source.c/h
    │   └── tip_data.c/h
    └── drum_data/              # 鼓机音源数据
        └── sf2_source.c/h
```

## 分层依赖规则

```
  03_app (应用层)
    ↓ 依赖
  02_core (核心引擎)
    ↓ 依赖
  01_hal (硬件抽象层)
    ↓ 依赖
  common (共享类型)
```

**严禁反向依赖**: 底层不得引用上层头文件。

## 移植指南

移植到新 MCU 只需：

1. 在 `01_hal/port/` 下创建新平台目录 (如 `stm32/`)
2. 实现 `bg_storage_xxx.c` (Flash/文件系统读写)
3. 实现 `bg_download_port_xxx.c` (数据接收)
4. 可选: 实现 `bg_hal_xxx.c` (音频输出)
5. 在 `bg_config.h` 中添加平台宏并设置 `BG_TARGET_PLATFORM`

`02_core/` 和 `03_app/` 代码**完全不需要修改**。

## BP10 IDE 配置更新

重新组织后需要在 Andes IDE 项目设置中更新 Include Paths:

**旧路径 → 新路径:**
| 旧路径 | 新路径 |
|--------|--------|
| `bangtsynth/` | `bangtsynth/` |
| `bangtsynth/BG_err_handle` | `bangtsynth/common` |
| `bangtsynth/BG_HAL` | `bangtsynth/01_hal` |
| `bangtsynth/BG_Midi_Controller` | `bangtsynth/02_core/midi` |
| `bangtsynth/BG_Soundbank` | `bangtsynth/02_core/soundbank` |
| `bangtsynth/BG_Envelope_Generator` | `bangtsynth/02_core/envelope` |
| `bangtsynth/BG_Audio_Processor` | *(已删除)* |
| `bangtsynth/BG_Audio_Processor/effects` | *(已删除)* |
| `bangtsynth/drum_machine` | `bangtsynth/03_app/drum_machine` |
| `bangtsynth/soundbank_data` | `bangtsynth/data/soundbank_data` |
| — | `bangtsynth/03_app/synth_node` *(新增)* |
| — | `bangtsynth/data/drum_data` *(新增)* |

还需要添加平台端口路径:
- `bangtsynth/01_hal/port/bp10`
