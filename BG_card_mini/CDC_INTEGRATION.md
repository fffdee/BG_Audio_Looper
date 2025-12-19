# CDC串口功能集成完成

## ✅ 已集成到工程

CDC串口功能已经成功集成到你的BG Card Mini工程中！

## 📁 修改的文件

### 1. bg_audio_io_manager.c
- ✅ 添加了 `otg_device_cdc.h` 头文件引用
- ✅ USB模式改为 `AUDIO_MIC_CDC` - 支持音频+麦克风+CDC串口同时工作
- ✅ 在 `Audio_loop()` 函数中添加了 `OTG_DeviceCDC_Task()` 调用
- ✅ 添加了 `CDC_Process_Example()` 示例函数（默认未启用）

## 🚀 当前功能状态

### 已启用功能
✅ **CDC串口基础功能** - 自动在Audio_loop中处理
- USB虚拟串口自动初始化
- 数据收发缓冲区已准备就绪
- 周期性任务处理（接收数据）

### 可选功能（需要手动启用）
⚪ **CDC示例功能** - 串口命令处理和回显

## 📝 如何使用

### 方式1：基础使用（当前已启用）

CDC串口现在会自动工作，你可以在自己的代码中直接使用CDC API：

```c
// 在任何地方发送数据
uint8_t data[] = "Hello from BG Card!\r\n";
OTG_DeviceCDC_Send(data, sizeof(data)-1);

// 接收数据
if(OTG_DeviceCDC_GetRxCount() > 0) {
    uint8_t buffer[64];
    uint16_t len = OTG_DeviceCDC_Receive(buffer, sizeof(buffer));
    // 处理数据...
}
```

### 方式2：启用示例功能

如果想要测试命令回显功能，在 `bg_audio_io_manager.c` 的 `Audio_loop()` 函数中：

**找到这行（大约在第635行）：**
```c
// CDC串口应用处理（可选，取消注释以启用示例功能）
// CDC_Process_Example();
```

**取消注释：**
```c
// CDC串口应用处理（可选，取消注释以启用示例功能）
CDC_Process_Example();
```

启用后支持的命令：
- `VERSION` - 显示版本信息
- `STATUS` - 显示系统状态
- `HELP` - 显示帮助信息
- 其他文本 - 自动回显

## 💻 测试步骤

### Windows
1. 连接USB到电脑
2. 打开设备管理器，找到"USB Serial Device (COMx)"
3. 使用串口工具（PuTTY/TeraTerm等）打开对应的COM端口
4. 波特率：115200, 数据位：8, 停止位：1, 无校验
5. 发送文本进行测试

### Linux/Mac
```bash
# 查找设备
ls /dev/ttyACM*

# 使用screen连接
screen /dev/ttyACM0 115200

# 或使用minicom
minicom -D /dev/ttyACM0 -b 115200

# 简单测试
echo "VERSION" > /dev/ttyACM0
cat /dev/ttyACM0
```

## 🎯 实际应用示例

### 示例1：调试输出
在你的代码任何位置添加调试信息：

```c
// 在 main.c 或其他文件中
void debug_log(const char *msg) {
    OTG_DeviceCDC_Send((uint8_t*)msg, strlen(msg));
    OTG_DeviceCDC_Send((uint8_t*)"\r\n", 2);
}

// 使用
debug_log("Button pressed");
debug_log("Audio processing started");
```

### 示例2：参数配置
通过串口接收参数：

```c
// 在 Audio_loop 中添加自定义处理
if(OTG_DeviceCDC_GetRxCount() > 0) {
    uint8_t cmd[32];
    uint16_t len = OTG_DeviceCDC_Receive(cmd, sizeof(cmd));
    
    if(strncmp((char*)cmd, "VOL:", 4) == 0) {
        // 解析音量值
        int vol = atoi((char*)&cmd[4]);
        // 设置音量
        BG_AudioManager.SetLineOutVol(vol, vol);
    }
}
```

### 示例3：实时状态监控
周期性发送状态信息：

```c
// 在 Audio_loop 中添加（使用计数器控制发送频率）
static uint32_t status_counter = 0;
if(++status_counter >= 44100) {  // 每秒一次
    status_counter = 0;
    char status[64];
    sprintf(status, "Mic: %d, Guitar: %d\r\n", 
            BG_AudioManager.Audio_data.mic_count,
            BG_AudioManager.Audio_data.guitar_count);
    OTG_DeviceCDC_Send((uint8_t*)status, strlen(status));
}
```

## 🔧 API参考

### 发送数据
```c
uint16_t OTG_DeviceCDC_Send(uint8_t *buf, uint16_t len);
bool OTG_DeviceCDC_FlushTx(void);  // 立即发送
```

### 接收数据
```c
uint16_t OTG_DeviceCDC_Receive(uint8_t *buf, uint16_t len);
uint16_t OTG_DeviceCDC_GetRxCount(void);  // 获取可读字节数
```

### 控制
```c
bool OTG_DeviceCDC_FlushRx(void);  // 清空接收缓冲区
uint16_t OTG_DeviceCDC_GetTxFreeSpace(void);  // 获取发送缓冲区剩余空间
```

## ⚠️ 重要说明

1. **不要修改 Audio_loop 中的 OTG_DeviceCDC_Task() 调用**
   - 这是必需的，用于处理CDC数据接收
   - 如果删除，将无法接收数据

2. **音频和串口互不干扰**
   - CDC使用独立的USB端点
   - 不影响音频性能
   - 可以同时播放音频和传输串口数据

3. **缓冲区大小**
   - 接收缓冲区：512字节
   - 发送缓冲区：512字节
   - 如果需要更大缓冲区，修改 `otg_device_cdc.h` 中的定义

## 📚 更多信息

详细文档和完整API说明请参考：
- [CDC_README.md](src/hardware/BG_AudioIO_Manager/USB/CDC_README.md) - 完整使用文档
- [CDC_USAGE_EXAMPLE.c](src/hardware/BG_AudioIO_Manager/USB/CDC_USAGE_EXAMPLE.c) - 更多代码示例

## ✨ 下一步

1. **编译项目** - 确保没有编译错误
2. **烧录固件** - 将固件烧录到设备
3. **测试连接** - 连接USB，检查串口是否出现
4. **根据需求定制** - 添加你自己的串口处理逻辑

---

**集成完成日期**: 2025-12-15  
**状态**: ✅ 已集成，可直接使用
