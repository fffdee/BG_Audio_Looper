# USB CDC 串口功能集成说明

## 概述

已为BG Card Mini USB驱动添加CDC (Communication Device Class) 串口功能，实现虚拟串口通信。支持音频设备与CDC串口同时工作的复合设备模式。

## 新增文件

### 头文件
- `inc/otg_device_cdc.h` - CDC串口接口定义

### 源文件
- `src/otg_device_cdc.c` - CDC串口核心实现
- `CDC_USAGE_EXAMPLE.c` - 使用示例代码

### 修改的文件
- `inc/otg_device_standard_request.h` - 添加CDC端点和接口定义
- `src/otg_device_descriptor.h` - 添加CDC配置描述符
- `src/otg_device_standard_request.c` - 添加CDC请求处理

## 支持的USB模式

### 新增模式
1. **CDC_ONLY** (模式12) - 仅CDC串口
2. **AUDIO_CDC** (模式9) - 音频播放 + CDC串口
3. **MIC_CDC** (模式10) - 麦克风 + CDC串口  
4. **AUDIO_MIC_CDC** (模式11) - 音频播放 + 麦克风 + CDC串口

## USB端点分配

- **DEVICE_CDC_CMD_EP** (0x86) - CDC命令端点 (Interrupt IN)
- **DEVICE_CDC_DATA_IN_EP** (0x87) - CDC数据输入 (Bulk IN)
- **DEVICE_CDC_DATA_OUT_EP** (0x08) - CDC数据输出 (Bulk OUT)

## 快速开始

### 1. 选择USB模式

在初始化代码中选择合适的USB模式：

```c
#include "otg_device_standard_request.h"
#include "otg_device_cdc.h"

// 音频+CDC串口复合设备
OTG_DeviceModeSel(AUDIO_CDC, USB_VID, USBPID(AUDIO_CDC));
```

### 2. 发送数据

```c
uint8_t data[] = "Hello USB CDC!\r\n";
uint16_t sent = OTG_DeviceCDC_Send(data, sizeof(data)-1);
```

### 3. 接收数据

```c
if(OTG_DeviceCDC_GetRxCount() > 0)
{
    uint8_t buffer[64];
    uint16_t len = OTG_DeviceCDC_Receive(buffer, sizeof(buffer));
    // 处理接收到的数据...
}
```

### 4. 主循环集成

```c
void main_loop(void)
{
    while(1)
    {
        // CDC任务 - 必须周期性调用
        OTG_DeviceCDC_Task();
        
        // 应用逻辑
        if(OTG_DeviceCDC_GetRxCount() > 0)
        {
            // 处理串口数据
        }
        
        // 其他任务...
    }
}
```

## API参考

### 初始化函数
```c
bool OTG_DeviceCDC_Init(void);          // 初始化CDC
bool OTG_DeviceCDC_DeInit(void);        // 反初始化CDC
```

### 数据传输函数
```c
uint16_t OTG_DeviceCDC_Send(uint8_t *buf, uint16_t len);       // 发送数据
uint16_t OTG_DeviceCDC_Receive(uint8_t *buf, uint16_t len);    // 接收数据
```

### 状态查询函数
```c
uint16_t OTG_DeviceCDC_GetRxCount(void);        // 获取接收缓冲区字节数
uint16_t OTG_DeviceCDC_GetTxFreeSpace(void);    // 获取发送缓冲区可用空间
```

### 控制函数
```c
bool OTG_DeviceCDC_FlushTx(void);    // 立即发送缓冲区数据
bool OTG_DeviceCDC_FlushRx(void);    // 清空接收缓冲区
void OTG_DeviceCDC_Task(void);       // CDC任务，需周期性调用
```

## 串口参数

### 默认配置
- **波特率**: 115200 bps
- **数据位**: 8
- **停止位**: 1
- **校验**: 无

### 缓冲区大小
- **接收缓冲区**: 512 字节
- **发送缓冲区**: 512 字节

## 系统要求

### Windows
- 自动识别为USB串口设备
- 设备管理器中显示为 "USB Serial Device (COMx)"
- 无需额外驱动（Windows 10及以上）

### Linux
- 自动识别为 `/dev/ttyACM0` (或ACM1, ACM2...)
- 无需额外驱动

### macOS
- 自动识别为 `/dev/cu.usbmodem*`
- 无需额外驱动

## 测试工具

### Windows
- PuTTY
- TeraTerm
- Realterm
- Arduino Serial Monitor

### Linux/Mac
```bash
# 查看设备
ls /dev/ttyACM*

# 使用minicom
minicom -D /dev/ttyACM0 -b 115200

# 使用screen
screen /dev/ttyACM0 115200

# 使用cat/echo
echo "test" > /dev/ttyACM0
cat /dev/ttyACM0
```

## 应用场景示例

### 1. 调试输出
```c
void debug_printf(const char *format, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if(len > 0)
    {
        OTG_DeviceCDC_Send((uint8_t*)buffer, len);
    }
}
```

### 2. 命令行接口
```c
// 接收命令，执行操作
if(OTG_DeviceCDC_GetRxCount() > 0)
{
    uint8_t cmd[32];
    uint16_t len = OTG_DeviceCDC_Receive(cmd, sizeof(cmd));
    
    if(strncmp(cmd, "PLAY", 4) == 0)
    {
        // 开始播放音频
    }
    else if(strncmp(cmd, "STOP", 4) == 0)
    {
        // 停止播放
    }
}
```

### 3. 固件更新
```c
// 通过串口接收固件数据
uint8_t firmware_buffer[1024];
uint16_t received = OTG_DeviceCDC_Receive(firmware_buffer, sizeof(firmware_buffer));
// 写入Flash...
```

### 4. 参数配置
```c
// 接收配置参数
typedef struct {
    uint32_t volume;
    uint32_t equalizer[10];
} Config_t;

Config_t config;
OTG_DeviceCDC_Receive((uint8_t*)&config, sizeof(config));
```

## 注意事项

1. **周期性调用Task函数**  
   必须在主循环中定期调用 `OTG_DeviceCDC_Task()`，否则无法接收数据

2. **缓冲区管理**  
   接收缓冲区满时会丢弃新数据，及时读取数据

3. **发送数据**  
   数据先缓存到TX buffer，如需立即发送调用 `OTG_DeviceCDC_FlushTx()`

4. **中断安全**  
   CDC函数不是中断安全的，避免在中断中直接调用

5. **模式选择**  
   确保在USB初始化时选择了包含CDC的模式

## 编译配置

确保在编译时包含以下文件：
```makefile
SRC += src/hardware/BG_AudioIO_Manager/USB/src/otg_device_cdc.c
INC += -Isrc/hardware/BG_AudioIO_Manager/USB/inc
```

## 常见问题

### Q: 电脑无法识别CDC设备？
A: 
1. 检查是否选择了正确的USB模式（如AUDIO_CDC）
2. 确认USB描述符配置正确
3. Windows 7可能需要安装CDC驱动

### Q: 无法接收数据？
A: 
1. 确认 `OTG_DeviceCDC_Task()` 在主循环中被周期性调用
2. 检查接收缓冲区是否已满
3. 验证波特率配置是否匹配

### Q: 发送数据失败？
A: 
1. 检查TX缓冲区是否已满 (`OTG_DeviceCDC_GetTxFreeSpace()`)
2. 调用 `OTG_DeviceCDC_FlushTx()` 强制发送
3. 确认USB连接正常

### Q: 音频和CDC会冲突吗？
A: 不会，CDC和音频使用不同的端点和接口，可以同时工作

## 技术支持

如有问题，请查看示例代码 `CDC_USAGE_EXAMPLE.c`

---
**版本**: v1.0.0  
**日期**: 2025-12-15  
**作者**: BG Card Team
