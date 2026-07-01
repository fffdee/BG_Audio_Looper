# BanGTsynth - 模块化音频合成器框架

一个高度模块化、可配置、易移植的音频合成器软件框架。

## ✨ 设计理念

1. **模块化功能** - 每个功能独立封装，易于复用和维护
2. **可配置功能** - 支持编译时功能裁剪，适配不同平台资源
3. **抽象化接口** - 硬件抽象层设计，移植时只需实现标准接口

## 🚀 特性

- ✅ 支持多平台 (Linux / STM32 / ESP32 等)
- ✅ MIDI 控制器
- ✅ 多轨混音器
- ✅ 包络发生器 (可选)
- ✅ 音序器 (可选)
- ✅ 自定义音色格式 (.bg)
- ✅ 可配置的功能裁剪
- ✅ 清晰的移植文档

## 📦 依赖 (Linux)

```bash
sudo apt-get install libasound2-dev  # ALSA 音频库
sudo apt-get install libev-dev       # 事件循环库
```

## 🔧 快速开始

### 方法1: 自动化测试脚本 (推荐)

```bash
chmod +x quick_test.sh download_soundbank.sh
./quick_test.sh  # 自动下载音源、编译、准备运行
cd build && ./demo
```

### 方法2: 手动步骤

#### 1. 下载音源到 soundbank.bin

```bash
# Linux
chmod +x download_soundbank.sh
./download_soundbank.sh soundbank/piano/piano.sf2 0

# Windows PowerShell
.\download_soundbank.ps1 soundbank\piano\piano.sf2 0
```

#### 2. 编译项目

```bash
mkdir build && cd build
cmake ..
make
```

#### 3. 运行

```bash
./demo  # 自动从 soundbank.bin 加载音源
```

### 使用下载接口 (模拟MCU串口下载)

```bash
cd example/download_example
mkdir build && cd build
cmake ..
make
./download_example ../../soundbank/piano.sf2 0
```

## 📚 文档

- [架构说明](doc/ARCHITECTURE.md) - 了解框架设计
- [移植指南](doc/PORTING_GUIDE.md) - 移植到新平台
- [配置选项](include/bg_config.h) - 功能裁剪配置

## 🛠️ 功能裁剪

编辑 `include/bg_config.h` 或使用 CMake 选项：

```bash
# 禁用音序器和包络发生器
cmake -DENABLE_SEQUENCER=OFF -DENABLE_ENVELOPE_GEN=OFF ..
make
```

## 🌍 平台移植

移植到新平台只需 3 步：

1. 创建 `components/BG_HAL/bg_hal_[platform].c`
2. 实现 5 个标准接口（音频、存储、定时器、内存、输入）
3. 修改 `CMakeLists.txt` 添加平台配置

详见：[移植指南](doc/PORTING_GUIDE.md)

## 📖 架构概览

```
应用层 (main.c)
    ↓
音源管理器 (soundbank_manager)
    ↓  ↑ Download
    ↓  ↑ (下载接口)
组件层 (MIDI/Mixer/Envelope/Audio Processor)
    ↓
HAL接口层 (bg_storage, bg_download_port)
    ↓
平台实现 (Linux: 文件I/O, MCU: Flash/串口)
    ↓
硬件层 (ALSA / I2S / Flash / UART)
```

### 音源存储架构

```
Linux开发环境:
  音源文件 (SF2/BGS)
     ↓ download_soundbank.sh
  soundbank.bin (32MB)
     ↓ soundbank_manager.Init(offset)
  内存中的音源数据

MCU部署环境:
  PC端音源文件
     ↓ 串口传输
  MCU Flash (32MB)
     ↓ soundbank_manager.Init(offset)
  内存中的音源数据
```

## 📝 示例代码

### 基本使用

```c
#include "soundbank_manager.h"

int main(void) {
    // 初始化音源 (从soundbank.bin偏移0处加载)
    soundbank_manager.Init(0);
    
    // 显示音源信息
    printf("音源: %s\n", soundbank_manager.GetInfo());
    
    // 触发音符
    soundbank_manager.NoteOn(60, 100, 0);  // 中央C
    
    // 主循环
    while (1) {
        short buffer[1024];
        soundbank_manager.ReadSamples(buffer, 60, 1024, 0);
        // ... 播放音频
    }
}
```

### 下载音源 (模拟MCU串口下载)

```c
#include "soundbank_manager.h"

void progress_callback(size_t written, size_t total, void *data) {
    printf("进度: %zu/%zu bytes\n", written, total);
}

int main(void) {
    // 下载音源到soundbank.bin
    // Linux: 从文件读取
    // MCU: 从串口接收 (只需替换 bg_download_port_linux.c)
    BG_ERR ret = soundbank_manager.Download(
        "piano.sf2",           // 数据源 (Linux: 文件路径, MCU: 串口号)
        0,                     // 写入偏移
        0,                     // 文件大小 (0=自动检测)
        progress_callback,     // 进度回调
        NULL                   // 用户数据
    );
    
    if (ret == SUCCESS) {
        // 加载并使用
        soundbank_manager.Init(0);
    }
}
```

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 License

MIT License

## 👤 作者

**BanGO**

---

⭐ 如果这个项目对你有帮助，请给个 Star！
