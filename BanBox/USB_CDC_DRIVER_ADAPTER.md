# USB CDC驱动框架适配说明

## 概述

USB CDC (Communication Device Class) 驱动已成功适配到 BanGUI 驱动框架，提供虚拟串口功能。

## 文件位置

### 驱动框架适配层
- **头文件**: `03_driver_framework/drivers/drv_usb_cdc.h`
- **实现文件**: `03_driver_framework/drivers/drv_usb_cdc.c`

### 底层实现
- **源码目录**: `02_device_drivers/USB/`
- **核心文件**: 
  - `USB/inc/otg_device_cdc.h`
  - `USB/src/otg_device_cdc.c`

## 文件系统结构

USB CDC驱动注册后将创建以下文件系统节点：

```
/driver/usb/cdc/
├── name          (只读) - 设备名称 "USB_CDC"
├── status        (只读) - 连接状态 "connected" / "disconnected"
├── baudrate      (读写) - 波特率 (300-115200)
├── databits      (读写) - 数据位 (5/6/7/8)
├── stopbits      (读写) - 停止位 (1/1.5/2)
├── parity        (读写) - 校验位 (none/odd/even/mark/space)
├── rx_count      (只读) - 接收缓冲区数据量
├── tx_count      (只读) - 发送缓冲区数据量
└── flush         (只写) - 清空缓冲区命令 (rx/tx/all)
```

## Shell命令示例

### 查看驱动信息
```bash
drivers                          # 列出所有已注册驱动
ls /driver/usb                   # 查看USB目录
ls /driver/usb/cdc               # 查看CDC设备参数
```

### 读取参数
```bash
cat /driver/usb/cdc/status       # 查看连接状态
cat /driver/usb/cdc/baudrate     # 读取当前波特率
cat /driver/usb/cdc/rx_count     # 查看接收缓冲区数据量
cat /driver/usb/cdc/tx_count     # 查看发送缓冲区数据量
```

### 设置参数
```bash
echo 9600 > /driver/usb/cdc/baudrate     # 设置波特率为9600
echo 8 > /driver/usb/cdc/databits        # 设置数据位为8
echo 1 > /driver/usb/cdc/stopbits        # 设置停止位为1
echo none > /driver/usb/cdc/parity       # 设置无校验
```

### 清空缓冲区
```bash
echo rx > /driver/usb/cdc/flush          # 清空接收缓冲区
echo tx > /driver/usb/cdc/flush          # 清空发送缓冲区
echo all > /driver/usb/cdc/flush         # 清空所有缓冲区
```

## 驱动操作函数

### 初始化/反初始化
- `usb_cdc_drv_init()` - 初始化USB CDC设备
- `usb_cdc_drv_deinit()` - 反初始化

### 打开/关闭
- `usb_cdc_drv_open()` - 打开设备
- `usb_cdc_drv_close()` - 关闭设备

### 读写操作
- `usb_cdc_drv_read()` - 从USB CDC读取数据
- `usb_cdc_drv_write()` - 向USB CDC写入数据

### IOCTL控制命令

| 命令码 | 功能 | 参数 |
|--------|------|------|
| 0x01 | 检查连接状态 | NULL (返回值: 0=未连接, 1=已连接) |
| 0x02 | 刷新接收缓冲区 | NULL |
| 0x03 | 刷新发送缓冲区 | NULL |
| 0x04 | 获取可用数据量 | uint32_t* (返回接收缓冲区数据量) |

## 技术规格

### 缓冲区
- **接收缓冲区**: 512 字节
- **发送缓冲区**: 512 字节

### 线路编码 (Line Coding)
- **波特率**: 300 - 115200 bps (默认 115200)
- **数据位**: 5, 6, 7, 8 (默认 8)
- **停止位**: 1, 1.5, 2 (默认 1)
- **校验位**: None, Odd, Even, Mark, Space (默认 None)

### 连接检测
- 通过 `SET_LINE_CODING` 控制请求检测主机连接
- `UsbCDC.IsConnected` 标志指示连接状态

### 数据传输
- **接收**: 中断驱动，回调函数 `OnDeviceCDC_BulkOutReceived()`
- **发送**: `OTG_DeviceCDC_SendChar()` / `OTG_DeviceCDC_SendBuffer()`
- **读取**: `OTG_DeviceCDC_GetChar()` / `OTG_DeviceCDC_GetBuffer()`

## 代码集成

### 驱动注册
USB CDC驱动在 `drv_init.c` 中自动注册：

```c
int DrvFramework_RegisterAll(void)
{
    // ... 其他驱动 ...
    
    /* 注册USB CDC驱动 */
    ret = UsbCdc_DrvRegister();
    if (ret == 0) {
        total++;
    } else {
        failed++;
    }
    
    return (failed > 0) ? -1 : 0;
}
```

### 使用示例

#### C代码中使用
```c
#include "drv_usb_cdc.h"

// 检查USB连接状态
int is_connected = usb_cdc_drv_ioctl(NULL, 0x01, NULL);
if (is_connected) {
    // USB已连接
}

// 读取数据
uint8_t buffer[128];
int bytes_read = usb_cdc_drv_read(NULL, buffer, sizeof(buffer));

// 写入数据
const char *msg = "Hello USB!\r\n";
usb_cdc_drv_write(NULL, (uint8_t*)msg, strlen(msg));

// 获取可用数据量
uint32_t available;
usb_cdc_drv_ioctl(NULL, 0x04, &available);
```

## 总线类型

USB CDC驱动使用 `DRV_BUS_USB` 总线类型：

```c
typedef enum {
    DRV_BUS_SPI = 0,        /* SPI总线 */
    DRV_BUS_I2C,            /* I2C总线 */
    DRV_BUS_I2S,            /* I2S总线 */
    DRV_BUS_SDIO,           /* SDIO总线 */
    DRV_BUS_GPIO,           /* GPIO直接控制 */
    DRV_BUS_UART,           /* UART总线 */
    DRV_BUS_POWER,          /* 电源管理总线 */
    DRV_BUS_USB,            /* USB总线 */
    DRV_BUS_MAX
} DrvBusType_t;
```

## 更新的框架文件

### 核心框架
1. **drv_device.h** - 添加 `DRV_BUS_USB` 枚举值
2. **drv_device.c** - 添加 "usb" 到总线名称表，更新 `GetBusDir()` 函数
3. **drv_fs.h** - 添加 `DrvFs_GetUsbDir()` 函数声明
4. **drv_fs.c** - 创建 `/driver/usb/` 目录，添加 `g_UsbDir` 变量和 `DrvFs_GetUsbDir()` 函数

### 驱动初始化
5. **drv_init.h** - 无需更改
6. **drv_init.c** - 包含 `drv_usb_cdc.h`，调用 `UsbCdc_DrvRegister()`

## 已注册驱动列表

目前框架已注册以下驱动：

1. **ST7735 LCD** - `/driver/spi/st7735/` (SPI总线)
2. **W25Qxx Flash** - `/driver/spi/w25qxx/` (SPI总线)
3. **Battery Manager** - `/driver/power/battery/` (Power总线)
4. **USB CDC** - `/driver/usb/cdc/` (USB总线)

## 注意事项

1. **连接状态**: USB CDC需要主机连接才能收发数据，未连接时读写操作返回0
2. **缓冲区**: 512字节循环缓冲区，数据量超限时会丢失旧数据
3. **线路编码**: 参数设置会被主机SET_LINE_CODING覆盖
4. **Shell集成**: 已有 `shell_io_cdc.c/h` 提供Shell over USB CDC功能

## 编译集成

确保以下文件包含在Makefile中：

```makefile
# 驱动框架核心
SOURCES += banux/03_driver_framework/core/drv_fs.c
SOURCES += banux/03_driver_framework/core/drv_device.c

# 驱动适配层
SOURCES += banux/03_driver_framework/drivers/drv_st7735.c
SOURCES += banux/03_driver_framework/drivers/drv_w25qxx.c
SOURCES += banux/03_driver_framework/drivers/drv_battery.c
SOURCES += banux/03_driver_framework/drivers/drv_usb_cdc.c

# 驱动初始化
SOURCES += banux/03_driver_framework/drv_init.c

# 底层实现
SOURCES += banux/02_device_drivers/USB/src/otg_device_cdc.c
```

## 参考文档

- [USB CDC协议规范](02_device_drivers/USB/CDC_README.md)
- [CDC集成指南](02_device_drivers/USB/CDC_INTEGRATION.md)
- [驱动框架文档](03_driver_framework/DRIVER_FRAMEWORK_GUIDE.md)

## 版本历史

- **V1.0.0** (2026-01-02) - 初始版本，完成USB CDC驱动框架适配
