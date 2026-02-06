package com.example.myapplication;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothGattDescriptor;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.util.UUID;

public class BluetoothHelper {
    public interface BleNotifyListener {
        void onNotify(String data);
    }
    private BleNotifyListener bleNotifyListener;
        public void setBleNotifyListener(BleNotifyListener listener) {
            this.bleNotifyListener = listener;
        }
    private BluetoothGatt bluetoothGatt;
    private boolean isConnected = false;
    private boolean isCccdEnabled = false;  // 跟踪CCCD是否成功使能
    private String connectedDevice = null;
    private BluetoothGattCallback gattCallback;
    private OnConnectionChangedListener listener;
    private Handler handler = new Handler(Looper.getMainLooper());
    
    // CCCD重试控制
    private int cccdRetryCount = 0;
    private static final int MAX_CCCD_RETRIES = 3;  // 最多重试3次
    
    // 写入回调管理
    private java.util.function.Consumer<Boolean> pendingWriteCallback = null;

    public interface OnConnectionChangedListener {
        void onConnected(String deviceName, BluetoothGatt gatt);
        void onDisconnected();
    }

    public void setOnConnectionChangedListener(OnConnectionChangedListener listener) {
        this.listener = listener;
    }

    public boolean isConnected() {
        return isConnected;
    }

    public String getConnectedDevice() {
        return connectedDevice;
    }

    public BluetoothGatt getBluetoothGatt() {
        return bluetoothGatt;
    }

    public void connect(Context context, BluetoothDevice device) {
        disconnect();
        gattCallback = new BluetoothGattCallback() {
            @Override
            public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
                if (newState == android.bluetooth.BluetoothProfile.STATE_CONNECTED) {
                    isConnected = true;
                    connectedDevice = device.getName();
                    bluetoothGatt = gatt;
                    cccdRetryCount = 0;  // 重置重试计数
                    // 主动请求MTU为250
                    boolean mtuReq = gatt.requestMtu(250);
                    Log.d("BLE", "requestMtu(250) called, result=" + mtuReq);
                    handler.post(() -> {
                        if (listener != null) listener.onConnected(connectedDevice, gatt);
                    });
                    gatt.discoverServices();
                } else if (newState == android.bluetooth.BluetoothProfile.STATE_DISCONNECTED) {
                    isConnected = false;
                    isCccdEnabled = false;  // 重置CCCD状态
                    cccdRetryCount = 0;  // 重置重试计数
                    connectedDevice = null;
                    bluetoothGatt = null;
                    handler.post(() -> {
                        if (listener != null) listener.onDisconnected();
                    });
                }
            }
            @Override
            public void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {
                super.onMtuChanged(gatt, mtu, status);
                Log.d("BLE", "onMtuChanged: mtu=" + mtu + ", status=" + status);
            }
            @Override
            public void onServicesDiscovered(BluetoothGatt gatt, int status) {
                Log.d("BLE", "[STEP 2] onServicesDiscovered called, status=" + status);
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e("BLE", "[ERROR] Service discovery failed!");
                    return;
                }
                
                // 打印所有服务和特性
                Log.d("BLE", "[STEP 3] Listing all services and characteristics:");
                int serviceCount = 0;
                int charCount = 0;
                for (BluetoothGattService service : gatt.getServices()) {
                    serviceCount++;
                    Log.d("BLE", "[Discovery] Service: " + service.getUuid());
                    for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                        charCount++;
                        Log.d("BLE", "[Discovery]   Char: " + characteristic.getUuid() + " props=" + characteristic.getProperties());
                    }
                }
                Log.i("BLE", "[Discovery] 发现 " + serviceCount + " 个服务, " + charCount + " 个特征值");
                
                // [已禁用] 不再启用通知功能，工作在只写模式
                // handler.postDelayed(() -> enableNotify(gatt), 100);
                Log.i("BLE", "[MODE] Working in WRITE-ONLY mode (notifications disabled)");
                isCccdEnabled = false;  // 明确标记为未启用通知
            }
            
            // [已禁用] enableNotify功能 - 不再依赖下位机回复
            // 应用工作在只写模式，不接收通知
            /*
            private void enableNotify(BluetoothGatt gatt) {
                Log.d("BLE", "[STEP 4] Starting to enable AB02 notify...");
                
                // 自动使能 AB02 notify (Shell响应通道)
                // AB01 (0x0006): Write (写入命令)
                // AB02 (0x0008): Notify (接收Shell输出) ← 主通道
                // AB03 (0x000c): Notify (预留，暂不使用)
                String ab02Uuid = "0000ab02-0000-1000-8000-00805f9b34fb";
                String cccdUuid = "00002902-0000-1000-8000-00805f9b34fb";
                
                boolean found = false;
                
                for (BluetoothGattService service : gatt.getServices()) {
                    for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                        String charUuid = characteristic.getUuid().toString();
                        
                        if (charUuid.equalsIgnoreCase(ab02Uuid)) {
                            found = true;
                            Log.d("BLE", "[STEP 5] Found AB02 characteristic: " + charUuid);
                            
                            // 检查特性是否支持 notify
                            int props = characteristic.getProperties();
                            if ((props & BluetoothGattCharacteristic.PROPERTY_NOTIFY) == 0) {
                                Log.e("BLE", "[ERROR] AB02 does not support NOTIFY (props=" + props + ")");
                                return;
                            }
                            Log.d("BLE", "[STEP 6] AB02 supports NOTIFY (props=" + props + ")");
                            
                            // 启用通知
                            boolean notifySet = gatt.setCharacteristicNotification(characteristic, true);
                            Log.d("BLE", "[STEP 7] setCharacteristicNotification(AB02): " + notifySet);
                            if (!notifySet) {
                                Log.e("BLE", "[ERROR] Failed to set characteristic notification");
                                return;
                            }
                            
                            // 写入CCCD
                            BluetoothGattDescriptor cccd = characteristic.getDescriptor(UUID.fromString(cccdUuid));
                            if (cccd == null) {
                                Log.e("BLE", "[ERROR] No CCCD descriptor found for AB02");
                                return;
                            }
                            
                            Log.d("BLE", "[STEP 8] Found CCCD descriptor, writing ENABLE_NOTIFICATION_VALUE (0x0001)...");
                            cccd.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                            boolean descResult = gatt.writeDescriptor(cccd);
                            Log.d("BLE", "[STEP 9] writeDescriptor(CCCD-AB02): " + descResult);
                            
                            if (!descResult) {
                                Log.e("BLE", "[ERROR] Failed to write CCCD descriptor");
                            }
                            break;
                        }
                    }
                    if (found) break;
                }
                
                if (!found) {
                    Log.e("BLE", "[ERROR] AB02 characteristic not found!");
                }
            }
            */
            
            // [已禁用] onDescriptorWrite - 不再处理CCCD写入回调
            /*
            @Override
            public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
                super.onDescriptorWrite(gatt, descriptor, status);
                Log.d("BLE", "[STEP 10] onDescriptorWrite: descriptor=" + descriptor.getUuid() + ", status=" + status + (status == BluetoothGatt.GATT_SUCCESS ? " (SUCCESS)" : " (FAILED)"));
                
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    isCccdEnabled = true;
                    cccdRetryCount = 0;  // 成功后重置计数
                    Log.i("BLE", "[SUCCESS] ✓ CCCD enabled successfully! Device is ready to receive notifications.");
                    Log.i("BLE", "[STATUS] isConnected=" + isConnected + ", isCccdEnabled=" + isCccdEnabled);
                } else {
                    isCccdEnabled = false;
                    cccdRetryCount++;
                    Log.e("BLE", "[ERROR] ✗ Failed to enable CCCD! status=" + status + " (retry " + cccdRetryCount + "/" + MAX_CCCD_RETRIES + ")");
                    
                    if (cccdRetryCount < MAX_CCCD_RETRIES) {
                        // 继续重试
                        Log.w("BLE", "[RETRY] Retrying CCCD write in 500ms...");
                        handler.postDelayed(() -> enableNotify(gatt), 500);
                    } else {
                        // 达到最大重试次数，放弃并警告用户
                        Log.e("BLE", "[FATAL] ❌ CCCD enable failed after " + MAX_CCCD_RETRIES + " retries!");
                        Log.e("BLE", "[FATAL] Device will work in WRITE-ONLY mode (no response notifications)");
                        Log.e("BLE", "[SUGGESTION] Try: 1) Reconnect device  2) Restart app  3) Check device firmware");
                        
                        // 即使CCCD失败，也标记为已连接，但提醒用户功能受限
                        isCccdEnabled = false;  // 明确标记通知未启用
                    }
                }
            }
            */
            
            @Override
            public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
                super.onCharacteristicWrite(gatt, characteristic, status);
                boolean success = (status == BluetoothGatt.GATT_SUCCESS);
                Log.d("BLE", "onCharacteristicWrite: char=" + characteristic.getUuid() + ", status=" + status + (success ? " (SUCCESS)" : " (FAILED)"));
                
                // 触发回调
                if (pendingWriteCallback != null) {
                    final java.util.function.Consumer<Boolean> callback = pendingWriteCallback;
                    pendingWriteCallback = null;  // 清空回调
                    handler.post(() -> callback.accept(success));
                }
            }
            // [保留但未使用] onCharacteristicChanged - 通知功能已禁用
            // 由于工作在只写模式，此回调不会被触发
            @Override
            public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
                if (bleNotifyListener != null) {
                    String data = new String(characteristic.getValue());
                    Log.d("BLE", "[Notify] " + data);
                    handler.post(() -> bleNotifyListener.onNotify(data));
                } else {
                    String data = new String(characteristic.getValue());
                    Log.d("BLE", "[Notify] " + data);
                }
            }
        };
        bluetoothGatt = device.connectGatt(context, false, gattCallback);
    }
    // 写入AB02特征（0x0008），uuid: "0008"
    public void writeCharacteristic(String handleOrUuid, byte[] data, java.util.function.Consumer<Boolean> callback) {
        if (bluetoothGatt == null) {
            Log.e("BLE", "bluetoothGatt is null, cannot write");
            if (callback != null) callback.accept(false);
            return;
        }
        Log.d("BLE", "[writeCharacteristic] handleOrUuid=" + handleOrUuid);
        
        // 检查服务列表是否为空
        if (bluetoothGatt.getServices().isEmpty()) {
            Log.e("BLE", "❌ 服务列表为空，尝试重新发现服务...");
            
            // 触发服务发现
            boolean discoverResult = bluetoothGatt.discoverServices();
            Log.d("BLE", "discoverServices() result: " + discoverResult);
            
            if (!discoverResult) {
                Log.e("BLE", "❌ 服务发现失败");
                if (callback != null) callback.accept(false);
                return;
            }
            
            // 等待服务发现完成（会在 onServicesDiscovered 回调中重试）
            Log.w("BLE", "⏳ 等待服务发现完成，请稍后重试写入操作");
            if (callback != null) callback.accept(false);
            return;
        }
        
        // 打印所有服务和特征uuid（用于调试）
        Log.d("BLE", "=== 开始查找特征值 ===");
        for (BluetoothGattService service : bluetoothGatt.getServices()) {
            Log.d("BLE", "Service: " + service.getUuid());
            for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                Log.d("BLE", "  Characteristic: " + characteristic.getUuid() + " props=" + characteristic.getProperties());
            }
        }
        
        // 改进的特征值匹配逻辑
        BluetoothGattCharacteristic targetChar = null;
        String matchedUuid = null;
        
        outerLoop:
        for (BluetoothGattService service : bluetoothGatt.getServices()) {
            for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                String uuidStr = characteristic.getUuid().toString().toLowerCase();
                String searchStr = handleOrUuid.toLowerCase();
                
                // 尝试多种匹配方式
                if (uuidStr.equals(searchStr) ||                     // 完整匹配
                    uuidStr.endsWith(searchStr) ||                   // 后缀匹配
                    uuidStr.contains("-" + searchStr + "-") ||       // 中间匹配
                    uuidStr.endsWith("-" + searchStr)) {             // 末尾短UUID匹配
                    
                    targetChar = characteristic;
                    matchedUuid = uuidStr;
                    Log.d("BLE", "✓ 找到匹配特征值: " + matchedUuid);
                    break outerLoop;
                }
            }
        }
        
        if (targetChar == null) {
            Log.e("BLE", "❌ 未找到匹配的特征值: " + handleOrUuid);
            if (callback != null) callback.accept(false);
            return;
        }
        
        // 检查写入权限
        int props = targetChar.getProperties();
        Log.d("BLE", "特征值属性: " + props);
        if ((props & BluetoothGattCharacteristic.PROPERTY_WRITE) == 0 &&
            (props & BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) == 0) {
            Log.e("BLE", "❌ 特征值不支持写入: " + targetChar.getUuid());
            if (callback != null) callback.accept(false);
            return;
        }
        
        // 写入数据
        Log.d("BLE", "准备写入数据，长度: " + data.length + " 字节");
        targetChar.setValue(data);
        
        // 检查是否有未完成的写入操作
        if (pendingWriteCallback != null) {
            Log.w("BLE", "⚠️ 上一次写入还未完成，拒绝新的写入请求");
            if (callback != null) callback.accept(false);
            return;
        }
        
        // 保存回调，等待 onCharacteristicWrite 触发
        pendingWriteCallback = callback;
        
        boolean result = bluetoothGatt.writeCharacteristic(targetChar);
        Log.d("BLE", result ? "✓ writeCharacteristic 调用成功" : "❌ writeCharacteristic 调用失败");
        
        if (!result) {
            // 调用失败，清空回调并立即返回false
            pendingWriteCallback = null;
            if (callback != null) callback.accept(false);
        }
        // 如果调用成功，callback会在 onCharacteristicWrite 中触发
    }

    public void disconnect() {
        if (bluetoothGatt != null) {
            bluetoothGatt.disconnect();
            bluetoothGatt.close();
            bluetoothGatt = null;
        }
        isConnected = false;
        connectedDevice = null;
        if (listener != null) listener.onDisconnected();
    }
}
