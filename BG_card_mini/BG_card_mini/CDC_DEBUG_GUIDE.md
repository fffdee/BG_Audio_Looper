# CDC串口调试指南 - 解决"设备不存在"问题

## 🔍 当前问题分析

**现象**：
- ✅ COM5已创建
- ❌ "设备不存在"错误
- ❌ 串口无法打开

**根本原因**：
1. CDC数据接收函数使用了错误的端点（ControlReceive应该是BulkReceive）
2. 复合设备描述符在某些Windows版本上兼容性问题
3. Windows可能缓存了错误的驱动信息

---

## ✅ 已完成的修复

### 1. **CDC数据接收函数修复** ✅
```c
// 错误的实现（已修复）
OTG_DeviceControlReceive(tmpBuf, ...) ❌

// 正确的实现
OTG_DeviceBulkReceive(DEVICE_CDC_DATA_OUT_EP, tmpBuf, ...) ✅
```

### 2. **临时使用CDC_ONLY模式** ✅
为了排除复合设备干扰，临时改为纯CDC模式：
```c
// bg_audio_io_manager.c
OTG_DeviceModeSel(CDC_ONLY, 0x1234, 0x1234);
```

### 3. **设备描述符优化** ✅
改为标准CDC设备类：
```c
bDeviceClass: 0x02  // Communications Device Class
bcdUSB: 0x0110      // USB 1.1（兼容性更好）
```

---

## 🚀 测试步骤

### 步骤1：清除Windows USB缓存
```powershell
# 以管理员权限运行PowerShell

# 1. 停止USB服务
Stop-Service -Name "USB"

# 2. 删除USB设备注册表缓存（谨慎操作！）
# 这会让Windows重新识别所有USB设备
# 建议只删除问题设备的条目

# 3. 重启USB服务
Start-Service -Name "USB"

# 或者简单方法：重启电脑
```

### 步骤2：物理操作
1. **完全断电** - 拔掉USB，关闭设备电源
2. **等待30秒** - 让Windows释放设备缓存
3. **重新连接** - 先开设备，再插USB

### 步骤3：手动安装驱动（如果自动失败）
1. 设备管理器 → 右键 "BG Card audio" 或未知设备
2. 更新驱动程序 → 浏览我的电脑以查找驱动
3. 让我从计算机上的可用驱动程序列表中选择
4. 选择 "通用串行总线控制器" 或 "端口(COM和LPT)"
5. 选择 "USB串行设备" 或 "USB CDC串行设备"

### 步骤4：检查设备管理器
正确的状态应该是：
```
端口(COM和LPT)
  └─ USB串行设备 (COMx)  ← 无感叹号
```

### 步骤5：测试串口
```powershell
# 使用PowerShell测试
$port = new-Object System.IO.Ports.SerialPort COM5,115200,None,8,One
$port.Open()
$port.WriteLine("VERSION")
$response = $port.ReadLine()
Write-Host $response
$port.Close()
```

---

## 🛠️ 进阶调试

### 检查USB枚举日志
```powershell
# 查看USB设备事件日志
Get-WinEvent -LogName "Microsoft-Windows-USB-USBHUB3/Operational" -MaxEvents 20 |
  Where-Object {$_.Message -like "*1234*"} |
  Format-List
```

### 使用USBDeview工具
1. 下载 USBDeview: https://www.nirsoft.net/utils/usb_devices_view.html
2. 找到 VID=0x1234, PID=0x1723 的设备
3. 右键 → "Uninstall Selected Devices"
4. 重新插拔USB

### 使用USB分析工具
推荐工具：
- **USBPcap** - 捕获USB流量
- **Wireshark** - 分析USB协议
- **USB Device Tree Viewer** - 查看设备树

---

## 🔧 如果还是不行

### 方案A：修改VID/PID
Windows可能记住了旧的设备信息：
```c
// otg_device_standard_request.h
#define USB_VID    0x0483  // STM32的VID（测试用）
#define USB_PID    0x5740  // CDC设备PID
```

### 方案B：添加INF驱动文件
创建 `bgcard_cdc.inf`：
```ini
[Version]
Signature="$Windows NT$"
Class=Ports
ClassGuid={4D36E978-E325-11CE-BFC1-08002BE10318}
Provider=%PROVIDER%
DriverVer=12/15/2025,1.0.0.0

[Manufacturer]
%MFGNAME%=DeviceList, NTx86, NTamd64

[DeviceList.NTx86]
%DESCRIPTION%=DriverInstall, USB\VID_1234&PID_1723

[DeviceList.NTamd64]
%DESCRIPTION%=DriverInstall, USB\VID_1234&PID_1723

[DriverInstall]
include=mdmcpq.inf
CopyFiles=FakeModemCopyFileSection
AddReg=DriverInstall.AddReg

[DriverInstall.AddReg]
HKR,,DevLoader,,*ntkern
HKR,,NTMPDriver,,usbser.sys
HKR,,EnumPropPages32,,"MsPorts.dll,SerialPortPropPageProvider"

[DriverInstall.Services]
AddService=usbser, 0x00000002, DriverService

[DriverService]
DisplayName=%SERVICE%
ServiceType=1
StartType=3
ErrorControl=1
ServiceBinary=%12%\usbser.sys

[Strings]
PROVIDER="BG Card"
MFGNAME="BanGO"
DESCRIPTION="BG Card CDC Serial Port"
SERVICE="USB Serial Driver"
```

右键INF文件 → 安装

### 方案C：禁用驱动签名检查（测试用）
```powershell
# 以管理员权限运行
bcdedit /set testsigning on
# 重启电脑
```

---

## 📊 验证清单

- [ ] 编译无错误
- [ ] 烧录成功
- [ ] USB插入有声音
- [ ] 设备管理器无感叹号
- [ ] COMx端口出现
- [ ] 串口工具能打开
- [ ] 能发送数据
- [ ] 能接收数据
- [ ] 回显正常

---

## 💡 成功后恢复音频功能

CDC调试成功后，恢复音频+CDC复合模式：

```c
// bg_audio_io_manager.c - InitUSBDevice()
// 改回复合设备模式
OTG_DeviceModeSel(AUDIO_MIC_CDC, 0x1234, 0x1234);
```

然后需要：
1. 恢复设备描述符为EF类
2. 确保IAD描述符正确
3. 重新测试

---

## 🆘 仍然失败？

请提供以下信息：
1. Windows版本（Win7/10/11）
2. 设备管理器完整截图
3. USB设备属性 → 详细信息 → 硬件ID
4. 串口日志输出
5. USBDeview中的设备信息

**可能的终极方案**：
- 使用Linux测试（`dmesg | grep tty`）
- 使用不同的USB端口
- 更换测试电脑
- 检查硬件USB信号质量

---

**版本**: v1.1  
**日期**: 2025-12-15  
**状态**: 🔧 调试中
