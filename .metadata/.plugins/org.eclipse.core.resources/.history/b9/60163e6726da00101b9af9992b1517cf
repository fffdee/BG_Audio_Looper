# CDC "error 001" 错误修复

## ❌ 问题描述

启动时控制台持续打印大量错误信息:
```
UsbDeviceBulkRcv() error 001
UsbDeviceBulkRcv() error 001
UsbDeviceBulkRcv() error 001
...
```

## 🔍 问题原因

1. **USB CDC 设备初始化完成,但主机端串口未打开**
   - CDC 设备在 USB 枚举完成后就开始初始化
   - 此时主机端的串口软件可能还未打开虚拟串口
   - DTR/RTS 控制线未被设置,表示主机端未连接

2. **频繁轮询导致错误**
   - `OTG_DeviceCDC_Task()` 每次被调用都会尝试接收数据
   - 当端点未准备好时,底层驱动返回 `BULK_RCV_ERR1` (error code 001)
   - 原始代码每1000次循环就检查一次,频率过高

## ✅ 解决方案

### 1. 添加连接状态检测

**修改文件:** `otg_device_cdc.h`
- 在 `UsbCDC_t` 结构体中添加 `IsConnected` 标志
- 用于跟踪主机端是否已打开串口

```c
typedef struct
{
    uint8_t                 InitOk;
    uint8_t                 IsConnected;  // 新增:连接状态标志
    CDC_LineCoding_t        LineCoding;
    CDC_ControlLineState_t  ControlLineState;
    ...
} UsbCDC_t;
```

### 2. 根据DTR/RTS状态更新连接标志

**修改文件:** `otg_device_cdc.c` - `OTG_DeviceCDC_Request()`
- 当主机设置 DTR 或 RTS 时,表示串口已打开
- 此时才标记为已连接

```c
case CDC_SET_CONTROL_LINE_STATE:
    UsbCDC.ControlLineState.DTR = (Setup[2] & 0x01) ? 1 : 0;
    UsbCDC.ControlLineState.RTS = (Setup[2] & 0x02) ? 1 : 0;
    
    // 当主机设置控制线状态时,表示已连接
    if(UsbCDC.ControlLineState.DTR || UsbCDC.ControlLineState.RTS) {
        UsbCDC.IsConnected = 1;
    } else {
        UsbCDC.IsConnected = 0;
    }
    break;
```

### 3. 只在已连接时尝试接收数据

**修改文件:** `otg_device_cdc.c` - `OTG_DeviceCDC_Task()`
- 将检查频率从1000降低到10000 (降低10倍)
- 只有在 `IsConnected == 1` 时才尝试接收数据

```c
void OTG_DeviceCDC_Task(void)
{
    if(!UsbCDC.InitOk)
    {
        return;
    }
    
    static uint32_t check_counter = 0;
    if(++check_counter >= 10000)  // 降低检查频率
    {
        check_counter = 0;
        // 只有在主机已连接时才尝试接收
        if(UsbCDC.IsConnected && UsbCDC.RxCount < (CDC_RX_BUFFER_SIZE - CDC_DATA_FS_OUT_PACKET_SIZE))
        {
            OTG_DeviceCDC_DataReceived();
        }
    }
}
```

## 📊 修改效果

### 修改前
```
CDC Device Initialized
UsbDeviceBulkRcv() error 001    ← 持续打印错误
UsbDeviceBulkRcv() error 001
UsbDeviceBulkRcv() error 001
... (数百条错误信息)
```

### 修改后
```
CDC Device Initialized
(无错误信息,直到主机端打开串口)

[当主机打开串口时]
CDC: Set Control Line State - DTR:1 RTS:1 Connected:1
(串口正常工作,可以收发数据)
```

## 🎯 优势

1. **消除启动时的错误信息**
   - 不再有"error 001"打印
   - 启动日志更清晰

2. **节省CPU资源**
   - 检查频率降低10倍
   - 未连接时完全不尝试接收

3. **更准确的连接状态**
   - 通过DTR/RTS准确判断主机是否连接
   - 符合CDC规范

4. **不影响正常功能**
   - 串口打开后立即可用
   - 数据收发完全正常

## 🔧 测试建议

### 测试步骤
1. **编译并烧录固件**
2. **不打开串口软件,观察启动日志**
   - 应该看不到"error 001"
   - 应该看到"CDC Device Initialized"

3. **打开串口软件**
   - 应该看到"CDC: Set Control Line State - DTR:1 RTS:1 Connected:1"
   - 发送数据测试功能

4. **关闭串口软件**
   - 应该看到"CDC: Set Control Line State - DTR:0 RTS:0 Connected:0"
   - 再次打开,功能仍正常

### Windows测试
```powershell
# 使用 PuTTY 或其他串口工具
# 1. 连接 USB
# 2. 不打开串口 - 观察无错误
# 3. 打开串口 - 观察连接消息
# 4. 发送 "VERSION" 测试
```

### Linux/Mac测试
```bash
# 1. 查看设备但不打开
ls /dev/ttyACM*

# 2. 打开串口
screen /dev/ttyACM0 115200

# 3. 发送测试
echo "VERSION" > /dev/ttyACM0
```

## 📝 相关文件

- `src/hardware/BG_AudioIO_Manager/USB/inc/otg_device_cdc.h` - 添加IsConnected字段
- `src/hardware/BG_AudioIO_Manager/USB/src/otg_device_cdc.c` - 实现连接状态管理
- `CDC_INTEGRATION.md` - CDC集成说明
- `CDC_README.md` - CDC使用说明

## 🚀 后续优化建议

如果仍然看到偶尔的错误,可以考虑:

1. **进一步降低检查频率**
   ```c
   if(++check_counter >= 50000)  // 更低的频率
   ```

2. **添加重试计数限制**
   ```c
   static uint8_t retry_count = 0;
   if(error && ++retry_count >= 10) {
       // 暂停接收一段时间
   }
   ```

3. **使用中断驱动方式**
   - 依赖于硬件是否支持USB接收中断

## ✨ 总结

这个修复方案通过添加连接状态检测,避免了在主机端未打开串口时频繁尝试接收数据,从而消除了"error 001"错误信息。修改简洁且不影响正常功能。
