# BLE通信完整指南

## 📡 通信架构

### 下位机（固件）
```
AB01 (0x0006) ← 接收命令（Write）
AB02 (0x0008) → 发送响应（Notify）- Shell输出
AB03 (0x000c) → 保留通道（Notify）
```

### 上位机（Android App）
```
发送命令 → AB01 (writeCharacteristic)
接收响应 ← AB02 + AB03 (enableNotification + CCCD)
```

## 🔧 关键修复点

### 1. 栈溢出修复（已解决）
**问题**：BleNotifyTestTask任务栈仅256字，导致第4次循环时崩溃
**修复**：
- 栈大小：256 → 1024（增加4倍）
- msg变量：改为static减少栈占用
- 延迟时间：1秒 → 2秒（给BLE底层更多处理时间）

```c
// shell_io_ble.c
xTaskCreate(BleNotifyTestTask, "BleNotifyTest", 1024, NULL, tskIDLE_PRIORITY + 1, &g_BleNotifyTestTaskHandle);
```

### 2. CCCD自动订阅（已实现）
**位置**：`BluetoothHelper.java` - `onServicesDiscovered()`
**逻辑**：
- 自动扫描AB02和AB03特征
- 检查PROPERTY_NOTIFY属性
- 写入CCCD（0x2902）使能通知
- 支持同时订阅多个notify通道

```java
// 自动写入 ENABLE_NOTIFICATION_VALUE (0x0001)
cccd.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
gatt.writeDescriptor(cccd);
```

### 3. 命令发送通道（已明确）
**发送到AB01**：`0000ab01-0000-1000-8000-00805f9b34fb`
```java
// BanBoxSettingsActivity.java
private void sendBleShellCommand(String cmd, java.util.function.Consumer<Boolean> callback) {
    String cmdWithCRLF = cmd + "\r\n";
    bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", cmdWithCRLF.getBytes(), callback);
}
```

### 4. 数据流向图
```
Android App                     固件（下位机）
    |                              |
    | write AB01 (0x0006)          |
    |----------------------------->|
    |                              | app_att_write()
    |                              | ↓
    |                              | ShellIO_BLE_OnDataReceived()
    |                              | ↓
    |                              | Shell_Process()
    |                              | ↓
    |                              | BLE_Send() → GattServerNotify(0x0008)
    |                              |
    | onCharacteristicChanged()   <|
    |<-----------------------------|
    | (AB02 notify)                |
```

## 📝 测试宏控制

### AUTO_START_NOTIFY_TEST
**文件**：`ble_app_callback.c`
```c
#define AUTO_START_NOTIFY_TEST 0  // 0=关闭测试，1=开启测试
```

**开启测试**：连接时自动每2秒发送一条测试消息
**关闭测试**：仅响应App发送的命令

## 🧪 测试步骤

### 1. 固件端验证
```bash
# 编译固件
cd BanBox/Release
make clean && make

# 烧录并观察日志
# 应该看到：
[BLE] BLE_STACK_CONNECTED
[BLE] Ready to send notify (auto-start enabled)  # 如果测试宏=1
```

### 2. Android App验证
```
1. 打开App → WelcomeActivity
2. 点击"蓝牙连接"
3. 扫描并连接设备
4. 观察Logcat：
   - [Discovery] Service: ...
   - [Discovery] Char: 0000ab01... (Write)
   - [Discovery] Char: 0000ab02... (Notify)
   - Found AB02 characteristic, enabling notify...
   - writeDescriptor(CCCD-AB02): true
   - onDescriptorWrite: ... status=0 (SUCCESS)
   - ✓ CCCD enabled successfully

5. 进入"设置" → 点击"终端"
6. 输入命令如"help"并发送
7. 接收区应该显示Shell响应
```

### 3. 终端命令测试
```
help         - 显示帮助
sysmon       - 系统监控命令
fx           - 效果命令
vol          - 音量查询
bletest      - BLE连接测试（手动发送notify）
```

## 🐛 调试技巧

### 固件端日志
```c
[BLE_TX] BLE_Send called, total_len=21
[BLE_TX_DATA] help\r\n
[BLE_TX] GattServerNotify: handle=0x0008, offset=0, chunk=21
[BLE_TX] result=0 (SUCCESS)
[BLE_TX] Completed: sent=21/21
```

### Android端日志
```
D/BLE: [Notify] Shell response: Available commands:\r\n
D/BLE: [writeCharacteristic] handleOrUuid=0000ab01-0000-1000-8000-00805f9b34fb
D/BLE: writeCharacteristic result: true
```

## ⚠️ 常见问题

### Q1: App收不到notify
**检查**：
1. Logcat中是否有 "✓ CCCD enabled successfully"
2. 固件是否调用了 `BLE_Send()`
3. `att_server_can_send()` 是否返回1

### Q2: 固件收不到命令
**检查**：
1. App是否写入AB01（不是AB02！）
2. `app_att_write()` 是否被调用
3. `ShellIO_BLE_OnDataReceived()` 是否收到数据

### Q3: 发送第4-5条消息时崩溃
**原因**：任务栈溢出
**解决**：已修改栈大小为1024

## 📊 内存占用

| 组件 | 栈大小 | 优先级 |
|------|--------|--------|
| BleNotifyTestTask | 1024字（4KB） | tskIDLE_PRIORITY + 1 |
| Shell主任务 | 默认 | 默认 |

## 🎯 后续优化建议

1. **MTU协商**：已请求250字节，可进一步优化大数据传输
2. **分包传输**：当前max_len=23，可根据MTU动态调整
3. **流控机制**：添加发送队列和确认机制
4. **重连机制**：断线自动重连
5. **加密通信**：敏感命令加密传输

## 📄 相关文件

### 固件
- `ble_app_func.c` - GATT服务和ATT读写处理
- `ble_app_callback.c` - BLE协议栈回调
- `shell_io_ble.c` - Shell与BLE的桥接层

### Android
- `BluetoothHelper.java` - BLE通信核心类
- `BanBoxSettingsActivity.java` - 主界面和终端弹窗
- `WelcomeActivity.java` - 连接入口

---
**最后更新**：2026-02-03
**状态**：✅ 通信正常，测试宏已关闭
