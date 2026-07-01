# 第7章：蓝牙协议栈与 Looper

> **本章目标**：深入理解蓝牙音频栈和 Audio Looper 录制/播放机制
> **对应代码**：`02_device_drivers/bluetooth/` + `05_component/audio_looper/`

---

## 7.1 蓝牙协议栈架构

### 7.1.1 双模架构

BanBox 同时支持 **经典蓝牙 (Classic BT)** 和 **低功耗蓝牙 (BLE)**：

```
┌─────────────────────────────────────────────┐
│              蓝牙双模协议栈                     │
│                                              │
│  ┌─────────────┐    ┌──────────────────┐    │
│  │  Classic BT  │    │    BLE 5.x       │    │
│  │              │    │                  │    │
│  │  HFP (通话)  │    │  GATT Server     │    │
│  │  A2DP(音频)  │    │  ┌────────────┐  │    │
│  │  AVRCP(控制) │    │  │ 自定Service │  │    │
│  │  SPP (透传)  │    │  │ (参数同步)  │  │    │
│  │              │    │  └────────────┘  │    │
│  │  SBC 解码器  │    │  SPP Service    │    │
│  └─────────────┘    └──────────────────┘    │
│                                              │
│  MVSilicon 私有蓝牙协议栈 (Link Layer/HCI)      │
└─────────────────────────────────────────────┘
```

### 7.1.2 Classic BT Profile 详解

| Profile | 全称 | 用途 | 数据流向 |
|---------|------|------|----------|
| **A2DP** | Advanced Audio Distribution Profile | 高品质立体声音频流 | 手机 → BanBox (Source) |
| **HFP** | Hands-Free Profile | 免提通话 | 双向语音 |
| **AVRCP** | Audio/Video Remote Control Profile | 媒体控制 (播放/暂停/上下曲) | 双向控制指令 |
| **SPP** | Serial Port Profile | 串口透传 (Shell 命令等) | 双向数据 |

### 7.1.3 A2DP 音频数据流

```
手机 (A2DP Source)
   │  SBC 编码的蓝牙音频帧 (通过 A2DP 链路)
   ▼
a2dp_sbcBuf[8KB]   ← 蓝牙接收缓冲区
   │
   ▼
SBC Decoder (sbc_codec.c)
   │  SBC → PCM (44.1kHz/48kHz, 16-bit, Stereo)
   ▼
bt_decoded_buffer[256]  ← 解码后的 PCM
   │
   ▼
Effect Graph: BT_ReadAudioData()  ← 作为 Effect Graph 的一个 Source
   │
   ▼
DAC 输出 → 扬声器/耳机
```

### 7.1.4 HFP 通话数据流

```
手机 (HFP AG) ←── SCO/eSCO 音频链路 ──→ BanBox (HF)
                                          │
                   ┌──────────────────────┴──────────────────────┐
                   ▼                                              ▼
          ADC Mic 拾音                                       对方语音
          (AEC 回声消除)                                    SBC/HCI 解码
                   │                                              │
                   └────────────→ Mixer ←─────────────────────────┘
                                          │
                                          ▼
                                     DAC 输出
```

**AEC (Acoustic Echo Cancellation)** 参数存储在 `AECBuf.c` (572 bytes)，用于消除喇叭声音被 Mic 重新拾取的回声。

---

## 7.2 BLE 协议详解

### 7.2.1 BLE GATT 服务架构

```
BLE GATT Server (BanBox)
│
├── 自定义 Service (UUID: 厂商自定义)
│   ├── Playcontrol Characteristic (Notify/Write)
│   │   └── 效果参数实时调整
│   ├── Data Characteristic (Write/Indicate)
│   │   └── 参数同步、文件传输
│   └── Sync Characteristic (Write/Notify)
│       └── 批量同步握手
│
└── SPP Service (UUID: 标准)
    └── Shell 命令行通道
```

### 7.2.2 BLE 帧格式

```
┌───────┬───────┬──────┬──────┬──────┬───────────────┬───────┐
│ 0xAA  │ 0x55  │ CMD  │ SEQ  │ LEN  │   PAYLOAD     │ CRC16 │
│ 帧头0 │ 帧头1 │ 1字节│ 1字节│1字节 │  0~247 字节    │ 2字节 │
└───────┴───────┴──────┴──────┴──────┴───────────────┴───────┘
```

### 7.2.3 可靠传输协议

```
发送方                               接收方
  │                                    │
  ├─ BLE_CMD_XXX (SEQ=N) ────────────→│
  │                                    ├─ 校验 CRC
  │                                    ├─ OK → 处理数据
  │  ←────────── BLE_CMD_ACK (SEQ=N) ─┤
  │                                    │
  │  (若超时未收到 ACK)                  │
  ├─ BLE_CMD_XXX (SEQ=N) ──重传──→    │
  │  (最多重试 3 次)                    │
```

### 7.2.4 参数同步流程

```
App                        BanBox
 │                           │
 ├─ SYNC_REQ ──────────────→│
 │                           ├─ 收集所有参数状态
 │  ←── SYNC_START ─────────┤
 │                           │
 │  ←── DRC 参数 ────────────┤ (批量发送)
 │  ←── REVERB 参数 ─────────┤
 │  ←── EQ 参数 ─────────────┤
 │  ←── GAIN 参数 ───────────┤
 │  ←── LOOPER_SEG_STATE ────┤
 │  ←── VOLUME 参数 ─────────┤
 │                           │
 │  ←── SYNC_END ────────────┤
 │                           │
 ├─ ACK ───────────────────→│
```

---

## 7.3 Audio Looper (音频循环乐句工作站)

**文件**：`05_component/audio_looper/`

### 7.3.1 核心概念

Looper 是一个 **多轨音频循环录制/播放器**，允许你：
1. 录制一段音频（如吉他 riff）
2. 自动循环播放
3. 在循环上叠加新的录音（Overdub）
4. 撤销上一步（Undo）

### 7.3.2 数据模型

```
┌─── Looper ────────────────────────────────────────┐
│                                                    │
│  Segment 0 [████████████████]  录制/播放/空闲       │
│  Segment 1 [████████████████]  (最多 4 个段)        │
│  Segment 2 [                ]                      │
│  Segment 3 [████████████████]                      │
│                                                    │
│  存储: PSRAM (8MB) 或 NAND Flash (W25N02)         │
└────────────────────────────────────────────────────┘
```

### 7.3.3 段状态

每个 Segment 有 3 种状态：

| 状态 | 说明 | 图标 |
|------|------|------|
| `IDLE` | 未使用 | 空 |
| `RECORDING` | 正在录制 | ● 红点 |
| `PLAYING` | 循环播放 | ▶ 播放 |
| `OVERDUBBING` | 播放中叠加录音 | ◎ 叠加 |

### 7.3.4 Effect Graph 集成

```
录制路径 (Sink):
  Mixer 输出 → LooperRecord_SinkCallback → Flash/PSRAM 写入

播放路径 (Source):
  Flash/PSRAM → LooperPlay_SourceCallback → 混回 Effect Graph
```

### 7.3.5 录制缓冲区

```c
// 录制时写入（从 Effect Graph Sink）
static void LooperRecord_SinkCallback(EffectNode_t *node, uint32_t *buf, uint16_t len) {
    // 将混音后的数据写入指定 Segment 的存储区域
    loop_write_segment_data(active_segment, buf, len);
}

// 播放时读取（作为 Effect Graph Source）
static void LooperPlay_SourceCallback(EffectNode_t *node, uint32_t *buf, uint16_t len) {
    // 从当前活跃的 Segment 读取数据
    loop_read_segment_data(active_segment, buf, len);
}
```

### 7.3.6 WAV 导出功能

录制完成后，可以将 Segment 导出为 `.wav` 文件到 NAND Flash FAT32：

```c
// Shell 命令
wav export <seg>     # 导出 seg=0/1/2/3 为 WAV 文件
wav export mix       # 导出所有 segment 的混音
wav list             # 列出已导出的 WAV 文件
wav delete <file>    # 删除 WAV 文件
wav info             # 显示 FAT32 空间信息

wav_ble export <mask>  # 通过 BLE 发送 WAV 到手机 App
```

---

## 7.4 节拍器 (Metronome)

**文件**：`05_component/metronome/`

### 功能

内置数字节拍器，可作为 Effect Graph 的音频 Source 节点混入主输出：

```c
// 关键 API
void Metronome_SetBPM(uint16_t bpm);         // 设置速度
void Metronome_SetBeatsPerBar(uint8_t n);     // 每小节拍数
void Metronome_Start(void);                   // 启动
void Metronome_Stop(void);                    // 停止

// Shell 命令
metronome -b 120        # 设置 BPM=120
metronome -s 1          # 启动节拍器
metronome -s 0          # 停止节拍器
```

---

## 7.5 鼓机与合成器 (BanGTsynth)

**文件**：`05_component/BanGTsynth/`、`05_component/drum_machine/`

### BanGTsynth 合成器

支持 MIDI 输入的单音合成器，可加载不同的音色：
- **音源引擎**：波表合成 + 包络控制
- **MIDI 支持**：Note On/Off, Pitch Bend, CC
- **集成方式**：作为 Effect Graph 的 Source 节点

### 鼓机 (Drum Machine)

预制鼓节奏模式：

```bash
drum -p <pattern>   # 播放指定鼓模式
drum -s 0           # 停止鼓机
drum -l             # 列出所有模式
```

---

> **下一章**：[第8章：启动流程详解](08_启动流程详解.md)
