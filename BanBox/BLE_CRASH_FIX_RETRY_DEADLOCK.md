# BLE崩溃修复 - 重试死锁问题

## 🔴 问题诊断

### 崩溃日志分析
```
app_att_write for handle 06  ← App发送命令到AB01
[SHELL_BLE] OnDataReceived called, len=4
[SHELL_BLE] Calling Shell_Process()...
[BLE_TX] BLE_Send called, total_len=22
[BLE_TX] Waiting for CCCD to be enabled (attempt 1/10)...  ← 开始死循环
[BLE_TX] Waiting for CCCD to be enabled (attempt 2/10)...
...
[BLE_TX] ERROR: CCCD not enabled after waiting
Error exception happened  ← 崩溃
```

### 根本原因
1. **重试死锁**：`BLE_Send()` 中的 `while` 循环阻塞任务1秒（10次×100ms）
2. **多任务竞争**：测试任务和Shell响应任务同时调用 `BLE_Send()`
3. **CCCD状态混乱**：App重连后CCCD被清空，但 `att_server_can_send()` 返回0
4. **任务栈溢出**：多次重试消耗栈空间，导致崩溃

## ✅ 修复方案

### 1. 移除重试逻辑
**位置**：`shell_io_ble.c` - `BLE_Send()`

**修改前**（危险）：
```c
/* 等待CCCD使能，最多重试10次 */
retry_count = 0;
while (att_server_can_send() == 0 && retry_count < 10) {
    DBG("[BLE_TX] Waiting for CCCD to be enabled (attempt %d/10)...\n", retry_count + 1);
    vTaskDelay(pdMS_TO_TICKS(100));  /* 阻塞100ms */
    retry_count++;
}
```

**修改后**（安全）：
```c
/* 立即检查BLE协议栈是否就绪，不阻塞等待 */
if (att_server_can_send() == 0) {
    DBG("[BLE_TX] WARN: CCCD not enabled, skipping send\n");
    return 0;  /* 直接返回，不阻塞任务 */
}
```

### 2. Shell处理前检查CCCD
**位置**：`shell_io_ble.c` - `ShellIO_BLE_OnDataReceived()`

**新增逻辑**：
```c
/* 检查CCCD是否使能，避免Shell响应时死锁 */
if (att_server_can_send() == 0) {
    DBG("[SHELL_BLE] WARN: CCCD not enabled, dropping %d bytes\n", len);
    return;  /* CCCD未使能时不处理命令，避免响应时阻塞 */
}
```

**作用**：
- App发送命令时，先检查CCCD状态
- 如果CCCD未使能，直接丢弃命令，不调用Shell
- 避免Shell响应时调用 `BLE_Send()` 导致死锁

### 3. 确保测试宏关闭
**位置**：`ble_app_callback.c`

**新增编译警告**：
```c
#define AUTO_START_NOTIFY_TEST 0

#if (AUTO_START_NOTIFY_TEST != 0)
#warning "AUTO_START_NOTIFY_TEST is enabled! This should be disabled in production."
#endif
```

## 📊 修复效果对比

| 场景 | 修复前 | 修复后 |
|------|--------|--------|
| CCCD未使能时发送 | 阻塞1秒 → 崩溃 | 立即返回，记录警告 |
| App发送命令 | 可能死锁 | 安全丢弃或成功处理 |
| 多任务并发 | 栈溢出 | 无阻塞，安全退出 |
| 测试任务干扰 | 高风险 | 完全禁用 |

## 🔍 新的工作流程

### 正常流程
```
1. Android App连接
   ↓
2. BluetoothHelper.onServicesDiscovered()
   ↓
3. 自动写入CCCD (0x0001)
   ↓
4. att_server_can_send() 返回 1
   ↓
5. App发送命令到AB01
   ↓
6. ShellIO_BLE_OnDataReceived() 检查CCCD ✓
   ↓
7. Shell_Process() 处理命令
   ↓
8. BLE_Send() 检查CCCD ✓
   ↓
9. GattServerNotify() 发送响应
   ↓
10. App收到notify ✓
```

### 异常流程（CCCD未使能）
```
1. App发送命令到AB01
   ↓
2. ShellIO_BLE_OnDataReceived() 检查CCCD ✗
   ↓
3. 记录警告日志
   ↓
4. 立即返回，不处理命令
   ↓
5. 不会崩溃 ✓
```

## 🧪 测试验证

### 1. 编译固件
```bash
cd BanBox/Release
make clean && make
```

### 2. 观察日志
**成功场景**：
```
[BLE] BLE_STACK_CONNECTED
app_att_write for handle 09
[BLE_CCCD] AB02 CCCD write, size=2, val=0x0001  ← CCCD已使能
app_att_write for handle 06
[SHELL_BLE] OnDataReceived called, len=4
[SHELL_BLE] Calling Shell_Process()...
[BLE_TX] BLE_Send called, total_len=22
[BLE_TX] result=0 (SUCCESS)  ← 发送成功
```

**CCCD未使能场景**：
```
app_att_write for handle 06
[SHELL_BLE] WARN: CCCD not enabled, dropping 4 bytes  ← 安全丢弃
← 不会崩溃
```

### 3. Android App测试
```
1. 连接设备 → 等待5秒（确保CCCD写入完成）
2. 打开终端
3. 发送命令 "help"
4. 观察：
   - 如果收到响应 → CCCD正常 ✓
   - 如果无响应 → 检查Logcat是否有CCCD写入日志
```

## ⚠️ 注意事项

### 1. CCCD写入时序
- Android App在 `onServicesDiscovered()` 时自动写入CCCD
- 但写入操作是**异步**的，需要等待 `onDescriptorWrite()` 回调
- 建议：**连接后等待2-3秒再发送命令**

### 2. 重连处理
- 每次重连后CCCD会被清空
- App必须重新写入CCCD
- 固件端会收到 `handle 09` 的 `app_att_write()` 回调

### 3. 错误处理
- 如果App长时间收不到响应，检查：
  1. Logcat中是否有 `✓ CCCD enabled successfully`
  2. 固件日志中是否有 `[BLE_TX] WARN: CCCD not enabled`
  3. 是否在连接后立即发送命令（未等待CCCD写入完成）

## 📈 后续优化建议

### 1. 添加CCCD状态缓存
```c
static uint8_t g_BLE_CCCD_Enabled = 0;

/* 在 app_att_write() 中更新状态 */
case ATT_CHARACTERISTIC_AB02_01_CLIENT_CONFIGURATION_HANDLE:
    if (buffer_size == 2 && buffer[0] == 0x01 && buffer[1] == 0x00) {
        g_BLE_CCCD_Enabled = 1;
    } else {
        g_BLE_CCCD_Enabled = 0;
    }
    break;
```

### 2. 添加响应队列
```c
/* 当CCCD未使能时，缓存Shell响应到队列 */
/* CCCD使能后，从队列发送所有缓存的响应 */
```

### 3. 优化日志输出
```c
/* 减少高频日志（如EQ node），避免日志刷屏影响调试 */
```

---
**修复时间**：2026-02-03  
**状态**：✅ 已修复，待测试验证  
**关键改动**：移除阻塞重试 + 添加CCCD前置检查
