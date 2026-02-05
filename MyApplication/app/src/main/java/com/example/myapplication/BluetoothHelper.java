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
                    // 主动请求MTU为250
                    boolean mtuReq = gatt.requestMtu(250);
                    Log.d("BLE", "requestMtu(250) called, result=" + mtuReq);
                    handler.post(() -> {
                        if (listener != null) listener.onConnected(connectedDevice, gatt);
                    });
                    gatt.discoverServices();
                } else if (newState == android.bluetooth.BluetoothProfile.STATE_DISCONNECTED) {
                    isConnected = false;
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
                for (BluetoothGattService service : gatt.getServices()) {
                    Log.d("BLE", "[Discovery] Service: " + service.getUuid());
                    for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                        Log.d("BLE", "[Discovery]   Char: " + characteristic.getUuid() + " props=" + characteristic.getProperties());
                    }
                }
                
                // 延迟100ms再使能CCCD，确保服务发现完全完成
                handler.postDelayed(() -> enableNotify(gatt), 100);
            }
            
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
            
            @Override
            public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
                super.onDescriptorWrite(gatt, descriptor, status);
                Log.d("BLE", "[STEP 10] onDescriptorWrite: descriptor=" + descriptor.getUuid() + ", status=" + status + (status == BluetoothGatt.GATT_SUCCESS ? " (SUCCESS)" : " (FAILED)"));
                
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    isCccdEnabled = true;
                    Log.i("BLE", "[SUCCESS] ✓ CCCD enabled successfully! Device is ready to receive notifications.");
                    Log.i("BLE", "[STATUS] isConnected=" + isConnected + ", isCccdEnabled=" + isCccdEnabled);
                } else {
                    isCccdEnabled = false;
                    Log.e("BLE", "[ERROR] ✗ Failed to enable CCCD! status=" + status);
                    // 重试一次
                    Log.w("BLE", "[RETRY] Retrying CCCD write in 500ms...");
                    handler.postDelayed(() -> enableNotify(gatt), 500);
                }
            }
            
            @Override
            public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
                super.onCharacteristicWrite(gatt, characteristic, status);
                Log.d("BLE", "onCharacteristicWrite: char=" + characteristic.getUuid() + ", status=" + status + (status == BluetoothGatt.GATT_SUCCESS ? " (SUCCESS)" : " (FAILED)"));
            }
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
        // 打印所有服务和特征uuid
        for (BluetoothGattService service : bluetoothGatt.getServices()) {
            Log.d("BLE", "Service: " + service.getUuid());
            for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                Log.d("BLE", "  Characteristic: " + characteristic.getUuid() + " props=" + characteristic.getProperties());
            }
        }
        // 推荐完整 uuid 匹配
        String targetUuid = null;
        for (BluetoothGattService service : bluetoothGatt.getServices()) {
            for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                String uuidStr = characteristic.getUuid().toString().toLowerCase();
                if (uuidStr.endsWith(handleOrUuid.toLowerCase()) || uuidStr.contains(handleOrUuid.toLowerCase())) {
                    targetUuid = uuidStr;
                    Log.d("BLE", "Found candidate characteristic uuid: " + uuidStr);
                    break;
                }
            }
        }
        if (targetUuid == null) {
            Log.e("BLE", "No matching characteristic found for: " + handleOrUuid);
            if (callback != null) callback.accept(false);
            return;
        }
        BluetoothGattCharacteristic targetChar = null;
        BluetoothGattService targetService = null;
        for (BluetoothGattService service : bluetoothGatt.getServices()) {
            for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                if (characteristic.getUuid().toString().toLowerCase().equals(targetUuid)) {
                    targetService = service;
                    targetChar = characteristic;
                    break;
                }
            }
        }
        if (targetChar == null) {
            Log.e("BLE", "targetChar is null after uuid match");
            if (callback != null) callback.accept(false);
            return;
        }
        // 检查写入权限
        int props = targetChar.getProperties();
        if ((props & BluetoothGattCharacteristic.PROPERTY_WRITE) == 0 &&
            (props & BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) == 0) {
            Log.e("BLE", "Characteristic does not support write: " + targetChar.getUuid());
            if (callback != null) callback.accept(false);
            return;
        }
        
        // 直接写入数据到ab01特性（不需要配置notify，ab02才是notify特性）
        targetChar.setValue(data);
        boolean result = bluetoothGatt.writeCharacteristic(targetChar);
        Log.d("BLE", "writeCharacteristic result: " + result);
        if (callback != null) callback.accept(result);
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
