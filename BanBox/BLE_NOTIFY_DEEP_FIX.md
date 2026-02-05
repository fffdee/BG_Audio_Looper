# BLE Notify 深度修复 - 完整流程

## 🎯 目标
确保Android App连接后立即正确订阅notify，下位机及时响应

## 📱 Android App端优化

### 1. 分步骤日志跟踪
```
[STEP 1] onConnectionStateChange → connected
[STEP 2] onServicesDiscovered → status=0 (GATT_SUCCESS)
[STEP 3] Listing all services
[STEP 4] Starting to enable AB02 notify
[STEP 5] Found AB02 characteristic
[STEP 6] AB02 supports NOTIFY
[STEP 7] setCharacteristicNotification(AB02): true
[STEP 8] Found CCCD descriptor
[STEP 9] writeDescriptor(CCCD-AB02): true
[STEP 10] onDescriptorWrite → status=0 (SUCCESS)
[SUCCESS] ✓ CCCD enabled successfully!
```

### 2. 关键优化点

**延迟机制**
```java
// 服务发现完成后延迟100ms再写入CCCD，确保协议栈准备就绪
handler.postDelayed(() -> enableNotify(gatt), 100);
```

**重试机制**
```java
if (status != BluetoothGatt.GATT_SUCCESS) {
    Log.w("BLE", "[RETRY] Retrying CCCD write in 500ms...");
    handler.postDelayed(() -> enableNotify(gatt), 500);
}
```

**状态标志**
```java
private boolean isCccdEnabled = false;  // 跟踪CCCD是否成功使能
```

### 3. 完整连接流程
```
1. device.connectGatt() 
   ↓
2. onConnectionStateChange(CONNECTED)
   ↓
3. requestMtu(250)
   ↓
4. discoverServices()
   ↓
5. onServicesDiscovered(GATT_SUCCESS)
   ↓
6. 延迟100ms
   ↓
7. setCharacteristicNotification(AB02, true)
   ↓
8. writeDescriptor(CCCD, 0x0001)
   ↓
9. onDescriptorWrite(GATT_SUCCESS)
   ↓
10. isCccdEnabled = true ✓
```

## 🔧 下位机端优化

### 1. CCCD写入处理
```c
case ATT_CHARACTERISTIC_AB02_01_CLIENT_CONFIGURATION_HANDLE:
    BT_DBG("[BLE_CCCD] ========== CCCD WRITE RECEIVED ==========\n");
    
    if (buffer[0] == 0x01 && buffer[1] == 0x00) {
        // 检查写入前后的att_server_can_send()状态
        int can_send_before = att_server_can_send();
        
        // 延迟50ms让ATT栈更新
        vTaskDelay(pdMS_TO_TICKS(50));
        
        int can_send_after = att_server_can_send();
        
        BT_DBG("[BLE_CCCD] ✓ CCCD ENABLED!\n");
        BT_DBG("[BLE_CCCD]   - att_server_can_send (before): %d\n", can_send_before);
        BT_DBG("[BLE_CCCD]   - att_server_can_send (after 50ms): %d\n", can_send_after);
    }
```

### 2. Shell命令处理
```c
void ShellIO_BLE_OnDataReceived(uint8_t *data, uint16_t len)
{
    DBG("[SHELL_BLE] OnDataReceived called, len=%d\n", len);
    
    // 实时检查CCCD状态
    if (att_server_can_send() == 0) {
        DBG("[SHELL_BLE] WARN: CCCD not ready, dropping command\n");
        return;
    }
    DBG("[SHELL_BLE] CCCD check passed (att_server_can_send=1)\n");
    
    // 处理命令
    Shell_Process();
}
```

### 3. Notify发送
```c
uint16_t BLE_Send(uint8_t *data, uint16_t len)
{
    DBG("[BLE_TX] BLE_Send called, total_len=%d\n", len);
    
    // 实时检查
    if (att_server_can_send() == 0) {
        DBG("[BLE_TX] WARN: CCCD not ready, skipping send\n");
        return 0;
    }
    
    // 发送数据
    result = GattServerNotify(handle, data, chunk_size);
    if (result == 0) {
        DBG("[BLE_TX] result=0 (SUCCESS)\n");
    } else {
        DBG("[BLE_TX] result=0x%02X (ERROR)\n", result);
    }
}
```

## 🧪 测试步骤

### 1. 编译固件
```bash
cd BanBox/Release
make clean && make
```

### 2. Android App连接
```
1. 打开App
2. 点击"蓝牙连接"
3. 选择设备
4. 观察Logcat日志：
   - 查看 [STEP 1-10] 完整流程
   - 确认 [SUCCESS] ✓ CCCD enabled successfully!
```

### 3. 下位机日志验证
```
预期输出：
BLE_STACK_CONNECTED
[BLE_CCCD] ========== CCCD WRITE RECEIVED ==========
[BLE_CCCD] AB02 CCCD write, size=2, val=0x0001
[BLE_CCCD] ✓ CCCD ENABLED!
[BLE_CCCD]   - att_server_can_send (before): 0 或 1
[BLE_CCCD]   - att_server_can_send (after 50ms): 1
[BLE_CCCD] Device is ready to send notify!
[BLE_CCCD] ==========================================
```

### 4. 发送测试命令
```
App端：
1. 打开"终端"
2. 输入命令："help"
3. 点击"发送"

下位机日志：
app_att_write for handle 06
[SHELL_BLE] OnDataReceived called, len=5
[SHELL_BLE] CCCD check passed (att_server_can_send=1)  ← 关键！
[SHELL_BLE] Calling Shell_Process()...
[BLE_TX] BLE_Send called, total_len=X
[BLE_TX] result=0 (SUCCESS)  ← 成功！

App端：
[Notify] Available commands:
...
```

## 🔍 故障排查

### 问题1：App端 status=2 (FAILED)
**原因**：CCCD写入失败
**解决**：
- 检查是否正确找到AB02特征
- 确认CCCD descriptor存在
- 查看重试日志

### 问题2：下位机 att_server_can_send=0
**原因**：ATT协议栈未准备好
**解决**：
- 检查CCCD是否真的写入（查看日志）
- 延迟50ms后再次检查
- 确认App端onDescriptorWrite成功

### 问题3：GattServerNotify返回0x57
**原因**：CCCD未使能或ACL缓冲区满
**解决**：
- 实时调用 `att_server_can_send()` 检查
- 不要依赖缓存标志
- 添加发送间隔，避免缓冲区溢出

## 📊 时序图

```
Time  Android App                      下位机
--------------------------------------------------
0ms   connectGatt()                    
      ↓                                
50ms  onConnectionStateChange(CONNECTED)  BLE_STACK_CONNECTED
      ↓                                
100ms requestMtu(250)                  
      ↓                                
150ms onMtuChanged(250)                
      ↓                                
200ms discoverServices()               
      ↓                                
400ms onServicesDiscovered(SUCCESS)    
      ↓                                
500ms [延迟100ms]                      
      ↓                                
600ms setCharacteristicNotification(AB02, true)
      writeDescriptor(CCCD, 0x0001)    
      ↓                                ↓
650ms                                  app_att_write(handle=09)
                                       [BLE_CCCD] CCCD WRITE RECEIVED
                                       [延迟50ms检查]
                                       att_server_can_send() → 1 ✓
700ms onDescriptorWrite(SUCCESS)       
      isCccdEnabled = true ✓           
      ↓                                
800ms 用户发送命令 "help"              
      writeCharacteristic(AB01, "help\r\n")
      ↓                                ↓
850ms                                  app_att_write(handle=06)
                                       ShellIO_BLE_OnDataReceived()
                                       att_server_can_send() → 1 ✓
                                       Shell_Process()
                                       BLE_Send() → GattServerNotify()
                                       ↓
900ms onCharacteristicChanged(AB02)    
      [Notify] Available commands: ... ✓
```

## ✅ 成功标志

1. **App端**：
   - `[SUCCESS] ✓ CCCD enabled successfully!`
   - `isCccdEnabled = true`

2. **下位机端**：
   - `[BLE_CCCD] ✓ CCCD ENABLED!`
   - `att_server_can_send (after 50ms): 1`

3. **通信测试**：
   - 发送命令后收到响应
   - `[BLE_TX] result=0 (SUCCESS)`
   - App端显示Shell输出

---
**更新时间**：2026-02-03  
**状态**：✅ 深度优化完成  
**关键改进**：延迟机制 + 重试逻辑 + 实时状态检查
