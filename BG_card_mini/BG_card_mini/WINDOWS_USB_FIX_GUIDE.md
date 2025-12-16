# Windows USB设备驱动问题解决方案

## 问题描述
设备管理器显示"BG Card audio"但提示"设备驱动程序未安装(代码28)"

## 已修复的问题
✅ USB配置描述符总长度错误 (0x012B → 0x0119，即299→281字节)
✅ CDC串口错误打印问题 (添加IsConnected状态检测)

## Windows端解决步骤

### 1. 清除Windows USB设备缓存

**方法A: 使用设备管理器**
```
1. 在设备管理器中找到"BG Card audio"
2. 右键 → 卸载设备
3. 勾选"删除此设备的驱动程序软件"
4. 拔出USB设备
5. 重新插入
```

**方法B: 使用USBDeview工具**
```
1. 下载 USBDeview (NirSoft官方工具)
2. 运行并找到所有 VID_1234 设备
3. 右键删除选中的设备
4. 拔出并重新插入USB
```

**方法C: 清理注册表缓存**
```powershell
# 以管理员身份运行PowerShell

# 1. 停止即插即用服务
Stop-Service -Name PlugPlay -Force

# 2. 删除USB设备缓存
Remove-Item "HKLM:\SYSTEM\CurrentControlSet\Enum\USB\VID_1234*" -Recurse -Force -ErrorAction SilentlyContinue

# 3. 启动即插即用服务
Start-Service -Name PlugPlay

# 4. 拔出USB，等待5秒，重新插入
```

### 2. 强制Windows重新枚举设备

```powershell
# 扫描硬件改动
$devcon = "C:\Windows\System32\pnputil.exe"
& $devcon /scan-devices

# 或使用设备管理器: 操作 → 扫描检测硬件改动
```

### 3. 检查USB描述符 (验证修复)

使用 **USBTreeView** 或 **USB Device Tree Viewer** 查看：
- Configuration Descriptor Length 应该是 281 (0x0119)
- 应该看到5个接口
- CDC接口应该正确显示

## 固件端已修复

### 修改文件
1. **otg_device_descriptor.h** - 修正配置描述符长度
2. **otg_device_cdc.h** - 添加IsConnected状态标志  
3. **otg_device_cdc.c** - 优化CDC接收逻辑

### 重新编译步骤
```bash
# 清理旧的编译
make clean

# 重新编译
make

# 烧录固件
# (使用你的烧录工具)
```

## 预期结果

### 设备管理器应该显示:
```
音频输入和输出
  └─ USB 音频设备 (BG Card audio)
  
端口 (COM 和 LPT)
  └─ USB Serial Device (COMx)
```

### USB描述符信息:
```
Device Descriptor:
  bDeviceClass: 0xEF (Miscellaneous)
  bDeviceSubClass: 0x02 (Common Class)
  bDeviceProtocol: 0x01 (IAD)

Configuration Descriptor:
  wTotalLength: 281 (0x0119)
  bNumInterfaces: 5
  
Interface 0: Audio Control
Interface 1: Audio Streaming (Speaker)
Interface 2: Audio Streaming (Microphone)
Interface 3: CDC Control
Interface 4: CDC Data
```

## 故障排除

### 如果CDC串口仍未出现:

1. **检查Windows日志**
```powershell
Get-WinEvent -LogName "Microsoft-Windows-USB-USBHUB3/Operational" -MaxEvents 50 | Format-List
```

2. **确认CDC类驱动已加载**
```powershell
Get-WindowsDriver -Online | Where-Object {$_.ClassName -like "*Serial*"}
```

3. **手动安装usbser.sys**
- 设备管理器中找到未知设备
- 右键 → 更新驱动程序
- 浏览计算机以查找驱动程序
- 让我从计算机上的可用驱动程序列表中选取
- 选择"端口(COM和LPT)"
- 厂商选择"Microsoft"
- 型号选择"USB Serial Device"

### 如果音频设备未识别:

检查音频描述符中的采样率配置:
- Speaker: 48000 Hz (0x44AC00, 0xBB8000)
- Microphone: 48000 Hz

## 技术细节

### 配置描述符长度计算
```
9   配置描述符
8   IAD (Audio)
88  Audio Control接口
9   Audio Stream接口0 (Alt 0)
46  Audio Stream接口1 (Alt 1) 
9   Audio Stream接口2 (Alt 0)
46  Audio Stream接口2 (Alt 1)
8   IAD (CDC)
35  CDC Control接口
23  CDC Data接口
---
281 bytes (0x0119)
```

### CDC端点配置
- Command EP: 0x86 (Interrupt IN)
- Data OUT EP: 0x08 (Bulk OUT)
- Data IN EP: 0x87 (Bulk IN)

### Audio端点配置
- Speaker: 0x05 (Isochronous OUT)
- Microphone: 0x84 (Isochronous IN)

## 验证成功的标志

✅ 设备管理器无黄色感叹号
✅ 出现新的COM端口
✅ 音频设备正常识别
✅ 启动日志无"error 001"
✅ 串口软件能连接并收发数据
