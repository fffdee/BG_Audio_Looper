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
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class BluetoothHelper {
    public interface BleNotifyListener {
        void onNotify(String data);
    }
    private BleNotifyListener bleNotifyListener;
        public void setBleNotifyListener(BleNotifyListener listener) {
            this.bleNotifyListener = listener;
        }

    public interface BleRawDataListener {
        void onData(byte[] data);
    }
    private volatile BleRawDataListener rawDataListener;
    public void setRawDataListener(BleRawDataListener listener) {
        this.rawDataListener = listener;
    }
    private BluetoothGatt bluetoothGatt;
    private boolean isConnected = false;
    private boolean isCccdEnabled = false;
    private String connectedDevice = null;
    private BluetoothGattCallback gattCallback;
    private final List<OnConnectionChangedListener> connectionListeners = new ArrayList<>();
    private Handler handler = new Handler(Looper.getMainLooper());

    /** 同步超时：如果 8 秒内没收到 SYNC_END，强制关闭弹窗 */
    private static final long SYNC_TIMEOUT_MS = 8000;
    
    private final MutableLiveData<Boolean> syncingLiveData = new MutableLiveData<>(false);
    
    private final Runnable syncTimeoutRunnable = () -> {
        Log.w("BLE", "[Sync] Timeout: SYNC_END not received in " + SYNC_TIMEOUT_MS + "ms, dismissing dialog");
        syncingLiveData.postValue(false);
    };

    private void postSyncTimeout() {
        handler.removeCallbacks(syncTimeoutRunnable);
        handler.postDelayed(syncTimeoutRunnable, SYNC_TIMEOUT_MS);
    }

    private void cancelSyncTimeout() {
        handler.removeCallbacks(syncTimeoutRunnable);
    }

    private final MutableLiveData<BleConnectionState> connectionStateLiveData =
            new MutableLiveData<>(BleConnectionState.disconnected());

    private final MutableLiveData<Integer> batteryLevelLiveData = new MutableLiveData<>(-1);

    public LiveData<Integer> getBatteryLevelLiveData() {
        return batteryLevelLiveData;
    }

    private final MutableLiveData<String> calibStateLiveData = new MutableLiveData<>("Idle");

    public LiveData<String> getCalibStateLiveData() {
        return calibStateLiveData;
    }

    /** 系统状态 LiveData：0=IDLE, 1=NORMAL, 2=TRANSFER；-1=尚未收到数据 */
    private final MutableLiveData<Integer> sysStateLiveData = new MutableLiveData<>(-1);

    public LiveData<Integer> getSysStateLiveData() {
        return sysStateLiveData;
    }

    /** 自动低功耗启用状态 LiveData：1=已启用，0=已禁用，-1=尚未收到状态 */
    private final MutableLiveData<Integer> lpStateLiveData = new MutableLiveData<>(-1);

    public LiveData<Integer> getLpStateLiveData() {
        return lpStateLiveData;
    }

    /** 自动低功耗空闲超时 LiveData：分钟数（1-60），-1=尚未收到 */
    private final MutableLiveData<Integer> lpTimeoutLiveData = new MutableLiveData<>(-1);

    public LiveData<Integer> getLpTimeoutLiveData() {
        return lpTimeoutLiveData;
    }

    /** 产品功能列表 LiveData：连接同步后从下位机读取，描述硬件支持的功能 */
    private final MutableLiveData<ProductFeatures> featureListLiveData = new MutableLiveData<>(null);

    public LiveData<ProductFeatures> getFeatureListLiveData() {
        return featureListLiveData;
    }

    // batteryCurveLiveData: each int[] = {soc_pct, mv}
    private final MutableLiveData<int[][]> batteryCurveLiveData = new MutableLiveData<>(null);

    public LiveData<int[][]> getBatteryCurveLiveData() {
        return batteryCurveLiveData;
    }

    public LiveData<Boolean> getLiveSyncing() {
        return syncingLiveData;
    }

    private int cccdRetryCount = 0;
    private static final int MAX_CCCD_RETRIES = 3;

    private java.util.function.Consumer<Boolean> pendingWriteCallback = null;
    private java.util.function.Consumer<Boolean> pendingProtoWriteCallback = null;
    /** 写锁被设置的时刻（elapsedRealtime），超过 WRITE_LOCK_TIMEOUT_MS 自动释放 */
    private long pendingProtoWriteSetAt = 0;
    private static final long WRITE_LOCK_TIMEOUT_MS = 2000;
    /** onServicesDiscovered 时缓存的 AB01 写特征，避免每次遍历 getServices() */
    private BluetoothGattCharacteristic cachedAb01Char = null;

    private StringBuilder dataBuffer = new StringBuilder();

    // ---- WAV 导出帧分片累积缓冲区 ----
    // BLE MTU 通常 200字节，而 WAV 导出帧最大 199字节，一般不分片。
    // 但某些设备 MTU 更小时会分多个 notify 到达，需要累积。
    // 不能用 parseAllProtocolFrames 处理，密度大的 PCM 数据会导致偶然 CRC 碰撞導致误判。
    private final java.io.ByteArrayOutputStream wavExportBuffer = new java.io.ByteArrayOutputStream(256);
    /** 当前 WAV 帧期望总字节数（HDR+payload_len+CRC），0=尚未知 */

    private void enableNotify(BluetoothGatt gatt) {
        Log.d("BLE", "[STEP 4] Starting to enable AB02 notify...");
        String ab02Uuid = "0000ab02-0000-1000-8000-00805f9b34fb";
        String cccdUuid = "00002902-0000-1000-8000-00805f9b34fb";
        boolean found = false;
        for (BluetoothGattService service : gatt.getServices()) {
            for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                String charUuid = characteristic.getUuid().toString();
                if (charUuid.equalsIgnoreCase(ab02Uuid)) {
                    found = true;
                    int props = characteristic.getProperties();
                    if ((props & BluetoothGattCharacteristic.PROPERTY_NOTIFY) == 0) {
                        Log.e("BLE", "[ERROR] AB02 does not support NOTIFY (props=" + props + ")");
                        return;
                    }
                    boolean notifySet = gatt.setCharacteristicNotification(characteristic, true);
                    if (!notifySet) {
                        Log.e("BLE", "[ERROR] Failed to set characteristic notification");
                        return;
                    }
                    BluetoothGattDescriptor cccd = characteristic.getDescriptor(UUID.fromString(cccdUuid));
                    if (cccd == null) {
                        Log.e("BLE", "[ERROR] No CCCD descriptor found for AB02");
                        return;
                    }
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

    public interface OnConnectionChangedListener {
        void onConnected(String deviceName, BluetoothGatt gatt);
        void onDisconnected();
    }

    @Deprecated
    public void setOnConnectionChangedListener(OnConnectionChangedListener listener) {
        synchronized (connectionListeners) {
            connectionListeners.removeIf(l -> l instanceof TaggedListener && ((TaggedListener) l).tag.equals("legacy"));
            if (listener != null) {
                connectionListeners.add(new TaggedListener("legacy", listener));
            }
        }
    }

    public void addConnectionListener(OnConnectionChangedListener listener) {
        if (listener == null) return;
        synchronized (connectionListeners) {
            if (!connectionListeners.contains(listener)) {
                connectionListeners.add(listener);
            }
        }
    }

    public void removeConnectionListener(OnConnectionChangedListener listener) {
        if (listener == null) return;
        synchronized (connectionListeners) {
            connectionListeners.remove(listener);
        }
    }

    public LiveData<BleConnectionState> getConnectionStateLiveData() {
        return connectionStateLiveData;
    }

    private static class TaggedListener implements OnConnectionChangedListener {
        final String tag;
        final OnConnectionChangedListener delegate;
        TaggedListener(String tag, OnConnectionChangedListener delegate) {
            this.tag = tag;
            this.delegate = delegate;
        }
        @Override public void onConnected(String deviceName, BluetoothGatt gatt) { delegate.onConnected(deviceName, gatt); }
        @Override public void onDisconnected() { delegate.onDisconnected(); }
    }

    private void notifyConnected(String deviceName, BluetoothGatt gatt) {
        List<OnConnectionChangedListener> snapshot;
        synchronized (connectionListeners) {
            snapshot = new ArrayList<>(connectionListeners);
        }
        for (OnConnectionChangedListener l : snapshot) {
            l.onConnected(deviceName, gatt);
        }
    }

    private void notifyDisconnected() {
        List<OnConnectionChangedListener> snapshot;
        synchronized (connectionListeners) {
            snapshot = new ArrayList<>(connectionListeners);
        }
        for (OnConnectionChangedListener l : snapshot) {
            l.onDisconnected();
        }
    }

    public boolean isConnected() {
        return isConnected;
    }

    /** BLE 通道完全就绪：已连接 + 服务已发现 + AB01 可用 */
    public boolean isServiceReady() {
        return isConnected && cachedAb01Char != null;
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
                    cccdRetryCount = 0;
                    // 清除上一次连接遗留的写锁和队列，防止永久阻塞
                    pendingWriteCallback = null;
                    pendingProtoWriteCallback = null;
                    protoWriteBusy = false;
                    protoWriteQueue.clear();
                    cachedAb01Char = null;  // 等 onServicesDiscovered 后重新缓存
                    boolean mtuReq = gatt.requestMtu(250);
                    Log.d("BLE", "requestMtu(250) called, result=" + mtuReq);
                    // 连接建立后立即显示同步弹窗，设置超时安全阀
                    syncingLiveData.postValue(true);
                    postSyncTimeout();
                    connectionStateLiveData.postValue(
                            BleConnectionState.connected(connectedDevice, gatt));
                    handler.post(() -> notifyConnected(connectedDevice, gatt));
                    gatt.discoverServices();
                } else if (newState == android.bluetooth.BluetoothProfile.STATE_DISCONNECTED) {
                    isConnected = false;
                    isCccdEnabled = false;
                    cccdRetryCount = 0;
                    connectedDevice = null;
                    bluetoothGatt = null;
                    pendingWriteCallback = null;
                    pendingProtoWriteCallback = null;
                    protoWriteBusy = false;
                    protoWriteQueue.clear();
                    cachedAb01Char = null;
                    cancelSyncTimeout();
                    syncingLiveData.postValue(false);
                    BleParamCache.getInstance().clear();
                    wavExportBuffer.reset();
                    connectionStateLiveData.postValue(BleConnectionState.disconnected());
                    handler.post(() -> notifyDisconnected());
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
                int serviceCount = 0;
                int charCount = 0;
                for (BluetoothGattService service : gatt.getServices()) {
                    serviceCount++;
                    for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                        charCount++;
                    }
                }
                Log.i("BLE", "[Discovery] Found " + serviceCount + " services, " + charCount + " characteristics");
                // 缓存 AB01 写特征，供 drainProtoWriteQueue() 直接使用
                final String ab01 = "0000ab01-0000-1000-8000-00805f9b34fb";
                cachedAb01Char = null;
                for (BluetoothGattService svc : gatt.getServices()) {
                    BluetoothGattCharacteristic c = svc.getCharacteristic(
                            java.util.UUID.fromString(ab01));
                    if (c != null) { cachedAb01Char = c; break; }
                }
                if (cachedAb01Char != null) {
                    Log.d("BLE", "[Discovery] AB01 write characteristic cached: " + cachedAb01Char.getUuid());
                } else {
                    Log.e("BLE", "[Discovery] AB01 write characteristic NOT found in service list!");
                }
                // 服务就绪后，立即尝试发送队列中挂起的帧
                handler.post(() -> {
                    serviceNotReadyRetries = 0;
                    if (!protoWriteQueue.isEmpty() && !protoWriteBusy) {
                        Log.d("BLE", "[Discovery] Flushing " + protoWriteQueue.size() + " pending proto writes");
                        drainProtoWriteQueue();
                    }
                });
                handler.postDelayed(() -> enableNotify(gatt), 100);
            }

            @Override
            public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
                super.onDescriptorWrite(gatt, descriptor, status);
                Log.d("BLE", "[STEP 10] onDescriptorWrite: status=" + status + (status == BluetoothGatt.GATT_SUCCESS ? " (SUCCESS)" : " (FAILED)"));

                if (status == BluetoothGatt.GATT_SUCCESS) {
                    isCccdEnabled = true;
                    cccdRetryCount = 0;
                    Log.i("BLE", "[SUCCESS] CCCD enabled! Device will auto-sync via BleProto_RequestSync.");
                    // 设备 CCCD 写入就会自动触发同步，这里不再重复发 SYNC_REQ
                    // 弹窗已在 STATE_CONNECTED 时显示，超时保护已启动
                } else {
                    isCccdEnabled = false;
                    cccdRetryCount++;
                    if (cccdRetryCount < MAX_CCCD_RETRIES) {
                        handler.postDelayed(() -> enableNotify(gatt), 500);
                    } else {
                        Log.e("BLE", "[FATAL] CCCD enable failed after " + MAX_CCCD_RETRIES + " retries!");
                    }
                }
            }

            @Override
            public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
                super.onCharacteristicWrite(gatt, characteristic, status);
                boolean success = (status == BluetoothGatt.GATT_SUCCESS);
                Log.d("BLE", "onCharacteristicWrite: status=" + status + (success ? " (SUCCESS)" : " (FAILED)"));

                if (pendingWriteCallback != null) {
                    final java.util.function.Consumer<Boolean> callback = pendingWriteCallback;
                    pendingWriteCallback = null;
                    handler.post(() -> callback.accept(success));
                } else if (pendingProtoWriteCallback != null) {
                    final java.util.function.Consumer<Boolean> callback = pendingProtoWriteCallback;
                    pendingProtoWriteCallback = null;
                    protoWriteBusy = false;
                    handler.post(() -> {
                        callback.accept(success);
                        // 当前帧完成后继续处理队列中的下一帧
                        drainProtoWriteQueue();
                    });
                }
            }

            @Override
            public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
                byte[] rawData = characteristic.getValue();

                BleRawDataListener raw = rawDataListener;
                if (raw != null) {
                    raw.onData(rawData);
                    return;
                }

                // ---- WAV 导出 0x40 帧分片累积处理 ----
                // 判断当前片段属于 0x40 帧：缓冲区已有积平数据，或本片头是 AA5540
                boolean isWavChunk = (wavExportBuffer.size() > 0);
                if (!isWavChunk && rawData.length >= 3
                        && (rawData[0] & 0xFF) == 0xAA
                        && (rawData[1] & 0xFF) == 0x55
                        && (rawData[2] & 0xFF) == 0x40) {
                    isWavChunk = true;
                }

                if (isWavChunk) {
                    wavExportBuffer.write(rawData, 0, rawData.length);
                    byte[] buf = wavExportBuffer.toByteArray();

                    // 帧格式: AA 55 cmd seq [payload] crc_hi crc_lo，无 len 字段。
                    // 实测 MTU≥200B，每帧一次 notify 到达，直接验证整个 buf 是否是合法帧。
                    // 避免从最小长度逐步探测 CRC——否则小长度上的偶发 CRC 碰撞会误判为合法帧。
                    BleProtocol.Frame completeFrame = BleProtocol.decode(buf, 0, buf.length);
                    if (completeFrame != null) {
                        // 完整帧已确认，转 hex 分发给 bleNotifyListener
                        StringBuilder sb = new StringBuilder(buf.length * 2);
                        for (byte b : buf) sb.append(String.format("%02X", b & 0xFF));
                        String hexFrame = sb.toString();
                        Log.d("BLE", "[WAV] Complete frame (" + buf.length + "B): "
                                + hexFrame.substring(0, Math.min(20, hexFrame.length())) + "...");
                        wavExportBuffer.reset();
                        if (bleNotifyListener != null) {
                            final String hf = hexFrame;
                            handler.post(() -> bleNotifyListener.onNotify(hf));
                        }
                    } else if (buf.length > BleProtocol.HDR_SIZE + BleProtocol.MAX_PAYLOAD + BleProtocol.CRC_SIZE) {
                        // 超出最大帧长仍无法解析，缓冲区损坏，丢弃重置
                        Log.w("BLE", "[WAV] Buffer overflow (" + buf.length + "B), resetting");
                        wavExportBuffer.reset();
                    }
                    // else: 帧尚不完整，等待下一次 notify 继续累积
                    return;
                }

                // 多帧扫描解析：单次回调可能包含连续多个帧，逐个找 AA55 头并尝试匹配 CRC
                boolean anyProtocolFrame = parseAllProtocolFrames(rawData);
                if (anyProtocolFrame) return;

                String chunk = new String(rawData);
                Log.d("BLE", "[Notify] " + chunk);

                if (rawData.length >= 4) {
                    int firstByte = rawData[0] & 0xFF;
                    int secondByte = rawData[1] & 0xFF;
                    int typeByte = rawData[2] & 0xFF;

                    if (firstByte == 0xAA && secondByte == 0x55 &&
                        (typeByte == 0x01 || typeByte == 0x10 || typeByte == 0x11 ||
                         typeByte == 0x12 || typeByte == 0x20 || typeByte == 0x21 ||
                         typeByte == 0x22 || typeByte == 0x23 || typeByte == 0x40)) {
                        StringBuilder hexString = new StringBuilder();
                        for (byte b : rawData) {
                            hexString.append(String.format("%02X", b));
                        }
                        String hexData = hexString.toString();
                        Log.d("BLE", "[Notify] Legacy binary data, converted to hex: " + hexData);

                        if (bleNotifyListener != null) {
                            handler.post(() -> bleNotifyListener.onNotify(hexData));
                        }
                        return;
                    }
                }

                dataBuffer.append(chunk);
                String accumulatedData = dataBuffer.toString();

                int startIndex = accumulatedData.indexOf('{');
                if (startIndex >= 0) {
                    String potentialJson = accumulatedData.substring(startIndex);

                    try {
                        int braceCount = 0;
                        int endIndex = -1;
                        for (int i = 0; i < potentialJson.length(); i++) {
                            char c = potentialJson.charAt(i);
                            if (c == '{') braceCount++;
                            else if (c == '}') {
                                braceCount--;
                                if (braceCount == 0) {
                                    endIndex = i;
                                    break;
                                }
                            }
                        }

                        if (endIndex > 0) {
                            String completeJson = potentialJson.substring(0, endIndex + 1);
                            Log.d("BLE", "[Notify] Found complete JSON: " + completeJson);

                            new org.json.JSONObject(completeJson);

                            if (bleNotifyListener != null) {
                                handler.post(() -> bleNotifyListener.onNotify(completeJson));
                            }

                            dataBuffer.setLength(0);
                        }
                    } catch (org.json.JSONException e) {
                        Log.d("BLE", "[Notify] Invalid JSON format, continuing to accumulate");
                    }
                }
            }
        };
        bluetoothGatt = device.connectGatt(context, false, gattCallback);
    }

    public void writeCharacteristic(String handleOrUuid, byte[] data, java.util.function.Consumer<Boolean> callback) {
        if (bluetoothGatt == null) {
            Log.e("BLE", "bluetoothGatt is null, cannot write");
            if (callback != null) callback.accept(false);
            return;
        }

        dataBuffer.setLength(0);

        Log.d("BLE", "[writeCharacteristic] handleOrUuid=" + handleOrUuid);

        if (bluetoothGatt.getServices().isEmpty()) {
            Log.e("BLE", "Service list empty, rediscovering...");
            boolean discoverResult = bluetoothGatt.discoverServices();
            if (!discoverResult) {
                if (callback != null) callback.accept(false);
                return;
            }
            if (callback != null) callback.accept(false);
            return;
        }

        BluetoothGattCharacteristic targetChar = null;
        String matchedUuid = null;

        outerLoop:
        for (BluetoothGattService service : bluetoothGatt.getServices()) {
            for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                String uuidStr = characteristic.getUuid().toString().toLowerCase();
                String searchStr = handleOrUuid.toLowerCase();

                if (uuidStr.equals(searchStr) ||
                    uuidStr.endsWith(searchStr) ||
                    uuidStr.contains("-" + searchStr + "-") ||
                    uuidStr.endsWith("-" + searchStr)) {

                    targetChar = characteristic;
                    matchedUuid = uuidStr;
                    break outerLoop;
                }
            }
        }

        if (targetChar == null) {
            Log.e("BLE", "Characteristic not found: " + handleOrUuid);
            if (callback != null) callback.accept(false);
            return;
        }

        int props = targetChar.getProperties();
        if ((props & BluetoothGattCharacteristic.PROPERTY_WRITE) == 0 &&
            (props & BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) == 0) {
            Log.e("BLE", "Characteristic not writable: " + targetChar.getUuid());
            if (callback != null) callback.accept(false);
            return;
        }

        targetChar.setValue(data);

        if (pendingWriteCallback != null || pendingProtoWriteCallback != null) {
            Log.w("BLE", "Previous write still pending, rejecting new write");
            if (callback != null) callback.accept(false);
            return;
        }

        pendingWriteCallback = callback;

        boolean result = bluetoothGatt.writeCharacteristic(targetChar);
        Log.d("BLE", result ? "writeCharacteristic call succeeded" : "writeCharacteristic call failed");

        if (!result) {
            pendingWriteCallback = null;
            if (callback != null) callback.accept(false);
        }
    }

    public void disconnect() {
        if (bluetoothGatt != null) {
            bluetoothGatt.disconnect();
            bluetoothGatt.close();
            bluetoothGatt = null;
        }
        isConnected = false;
        connectedDevice = null;
        dataBuffer.setLength(0);
        BleParamCache.getInstance().clear();
        connectionStateLiveData.postValue(BleConnectionState.disconnected());
        handler.post(() -> notifyDisconnected());
    }

    private byte seqCounter = 1;
    private byte nextSeq() {
        byte s = seqCounter++;
        if (seqCounter == (byte)0xFF) seqCounter = 1;
        return s;
    }

    private void handleProtocolFrame(BleProtocol.Frame frame) {
        switch (frame.cmd) {
            case BleProtocol.CMD_ACK:
                if (frame.payload != null && frame.payload.length >= 1) {
                    Log.d("BLE", "[Proto] ACK received: seq=" + (frame.seq & 0xFF) +
                            " cmd=" + BleProtocol.cmdName(frame.payload[0]));
                    synchronized (ackLock) {
                        pendingAckSeq = frame.seq;
                        pendingAckReceived = true;
                        ackSuccess = true;
                        ackLock.notifyAll();
                    }
                }
                break;

            case BleProtocol.CMD_NACK:
                Log.w("BLE", "[Proto] NACK received: seq=" + (frame.seq & 0xFF));
                synchronized (ackLock) {
                    pendingAckSeq = frame.seq;
                    pendingAckReceived = true;
                    ackSuccess = false;
                    ackLock.notifyAll();
                }
                break;

            case BleProtocol.CMD_SYNC_START:
                Log.d("BLE", "[Proto] SYNC_START: total_groups=" + (frame.payload != null && frame.payload.length > 0 ? (frame.payload[0] & 0xFF) : 0));
                BleParamCache.getInstance().clear();
                // 不回 ACK：MCU 使用 fire-and-forget，回写 AB01 会触发 ATT_WRITE_RSP 竞争，
                // 导致 BP10 BLE 栈 TX 路径阻塞，后续所有 sendAckForFrame/writeCharacteristic 永久挂起
                break;

            case BleProtocol.CMD_SYNC_END:
                Log.d("BLE", "[Proto] SYNC_END received");
                BleParamCache.getInstance().setSyncComplete(true);
                cancelSyncTimeout();
                syncingLiveData.postValue(false);
                // 不回 ACK：同上
                break;

            case BleProtocol.CMD_SYSTEM:
                if (frame.payload != null && frame.payload.length >= 2) {
                    int subType = frame.payload[0] & 0xFF;
                    if (subType == BleProtocol.SYSTEM_SUB_BATTERY) {
                        int soc = frame.payload[1] & 0xFF;
                        Log.d("BLE", "[Proto] Battery SOC received: " + soc + "%");
                        batteryLevelLiveData.postValue(soc);
                    } else if (subType == (BleProtocol.SYSTEM_SUB_STATE & 0xFF)) {
                        int state = frame.payload[1] & 0xFF;
                        Log.d("BLE", "[Proto] SysState received: " + state);
                        sysStateLiveData.postValue(state);
                    } else if (subType == (BleProtocol.SYSTEM_SUB_LP_STATE & 0xFF)) {
                        int lpEnabled = frame.payload[1] & 0xFF;
                        Log.d("BLE", "[Proto] LP state received: " + lpEnabled);
                        lpStateLiveData.postValue(lpEnabled);
                    } else if (subType == (BleProtocol.SYSTEM_SUB_LP_TIMEOUT & 0xFF)) {
                        int timeoutMin = frame.payload[1] & 0xFF;
                        Log.d("BLE", "[Proto] LP timeout received: " + timeoutMin + " min");
                        lpTimeoutLiveData.postValue(timeoutMin);
                    } else if (subType == (BleProtocol.SYSTEM_SUB_PRODUCT_ID & 0xFF)) {
                        if (frame.payload.length >= 3) {
                            int pid = (frame.payload[1] & 0xFF) | ((frame.payload[2] & 0xFF) << 8);
                            Log.d("BLE", "[Proto] Product ID received: 0x" + String.format("%04X", pid));
                            BleParamCache.getInstance().setProductId(pid);
                        }
                    } else if (subType == (BleProtocol.SYSTEM_SUB_FEATURE_LIST & 0xFF)) {
                        Log.d("BLE", "[Proto] Feature list received, len=" + (frame.payload.length - 1));
                        BleParamCache.getInstance().setFeatureListFromPayload(frame.payload);
                        ProductFeatures pf = BleParamCache.getInstance().getProductFeatures();
                        if (pf != null) {
                            featureListLiveData.postValue(pf);
                        }
                    }
                }
                break;

            case BleProtocol.CMD_BATTERY_CALIB:
                if (frame.payload != null && frame.payload.length >= 1) {
                    int sub = frame.payload[0] & 0xFF;
                    if (sub == (BleProtocol.CALIB_CMD_STATUS_RSP & 0xFF)) {
                        // Extended format:
                        // payload[0]: sub=0x83
                        // payload[1]: state (0=Idle, 1=Calibrating)
                        // payload[2]: num_points
                        // payload[3]: calib_step
                        // payload[4-5]: calib_mv (little-endian)
                        // payload[6..]: [time_s_lo, time_s_hi, mv_lo, mv_hi] * num_points (4 bytes/point)
                        int state      = frame.payload.length > 1 ? (frame.payload[1] & 0xFF) : 0;
                        int numPoints  = frame.payload.length > 2 ? (frame.payload[2] & 0xFF) : 0;
                        int calibStep  = frame.payload.length > 3 ? (frame.payload[3] & 0xFF) : 0;
                        int calibMv    = frame.payload.length > 5
                                         ? ((frame.payload[4] & 0xFF) | ((frame.payload[5] & 0xFF) << 8))
                                         : 0;
                        String info = state == 1
                                ? "矫正中: 步骤 " + calibStep + " (" + calibMv + " mV)"
                                : "空闲";
                        Log.d("BLE", "[Calib] " + info + " points=" + numPoints);
                        calibStateLiveData.postValue(info);
                        // Parse curve points: [time_s, mv]
                        int dataStart = 6;
                        if (numPoints > 0 && frame.payload.length >= dataStart + numPoints * 4) {
                            int[][] points = new int[numPoints][2];
                            for (int i = 0; i < numPoints; i++) {
                                int base = dataStart + i * 4;
                                int timeS = (frame.payload[base] & 0xFF)
                                          | ((frame.payload[base + 1] & 0xFF) << 8);
                                int pmv   = (frame.payload[base + 2] & 0xFF)
                                          | ((frame.payload[base + 3] & 0xFF) << 8);
                                points[i][0] = timeS;  // 累积时间（秒）
                                points[i][1] = pmv;    // 电压（mV）
                            }
                            Log.d("BLE", "[Calib] Parsed " + numPoints + " curve points");
                            batteryCurveLiveData.postValue(points);
                        } else if (numPoints == 0) {
                            batteryCurveLiveData.postValue(new int[0][]);
                        }
                    }
                }
                break;

            default:
                // 数据帧：不回 ACK。设备同步使用 fire-and-forget，不等待 App ACK。
                // 如果 App 回写 AB01，会占用 BLE 连接事件 TX 槽，导致设备后续通知发送失败（CCCD not ready）。
                if (BleProtocol.isDataCmd(frame.cmd)) {
                    if (frame.payload != null && frame.payload.length > 0) {
                        BleParamCache.getInstance().put(frame.cmd, frame.payload);
                    }
                }
                break;
        }
    }

    private void sendAckForFrame(BleProtocol.Frame frame) {
        byte[] ackData = BleProtocol.buildAck(frame.seq, frame.cmd);
        writeProtoCharacteristic(ackData, success -> {
            if (success) {
                Log.d("BLE", "[Proto] ACK sent for seq=" + (frame.seq & 0xFF) +
                        " cmd=" + BleProtocol.cmdName(frame.cmd));
            } else {
                Log.w("BLE", "[Proto] ACK send FAILED for seq=" + (frame.seq & 0xFF));
            }
        });
    }

    private final Object ackLock = new Object();
    private volatile byte pendingAckSeq = 0;
    private volatile boolean pendingAckReceived = false;
    private volatile boolean ackSuccess = true;

    /** 队列条目：待发送帧 + 完成回调 */
    private static class ProtoWriteEntry {
        final byte[] data;
        final java.util.function.Consumer<Boolean> callback;
        ProtoWriteEntry(byte[] d, java.util.function.Consumer<Boolean> cb) { data = d; callback = cb; }
    }
    private final java.util.Queue<ProtoWriteEntry> protoWriteQueue = new java.util.LinkedList<>();
    private boolean protoWriteBusy = false;
    /** 服务未就绪时的重试计数，防止无限循环 */
    private int serviceNotReadyRetries = 0;
    private static final int MAX_SERVICE_NOT_READY_RETRIES = 6; // 6×500ms = 最长等3s

    /**
     * 向 AB01 写特征发送一帧数据。
     * 内部使用队列保证串行执行——如果当前有写操作进行中，新请求入队等待，
     * 不会立即失败，从根本上解决并发写锁卡死问题。
     */
    private void writeProtoCharacteristic(byte[] data, java.util.function.Consumer<Boolean> callback) {
        handler.post(() -> {
            if (bluetoothGatt == null) {
                Log.w("BLE", "[writeProtoChar] skip: bluetoothGatt is null");
                if (callback != null) callback.accept(false);
                return;
            }
            protoWriteQueue.add(new ProtoWriteEntry(data, callback));
            if (!protoWriteBusy) {
                drainProtoWriteQueue();
            }
        });
    }

    /** 从队列中取下一帧并发送；必须在 handler 线程调用。 */
    private void drainProtoWriteQueue() {
        // 检查超时的写锁：若 onCharacteristicWrite 超过 2s 未回调，强制释放
        if (protoWriteBusy) {
            long elapsed = android.os.SystemClock.elapsedRealtime() - pendingProtoWriteSetAt;
            if (elapsed < WRITE_LOCK_TIMEOUT_MS) {
                return; // 正常等待中
            }
            Log.w("BLE", "[writeProtoChar] write lock timed out after " + elapsed + "ms, force releasing");
            protoWriteBusy = false;
            pendingProtoWriteCallback = null;
        }

        ProtoWriteEntry entry = protoWriteQueue.poll();
        if (entry == null) return;

        if (bluetoothGatt == null) {
            Log.w("BLE", "[writeProtoChar] gatt null when draining queue, dropping frame");
            if (entry.callback != null) entry.callback.accept(false);
            drainProtoWriteQueue(); // 处理剩余队列
            return;
        }

        // 搜索 AB01 写特征：优先用 onServicesDiscovered 时缓存的引用
        BluetoothGattCharacteristic targetChar = cachedAb01Char;
        if (targetChar == null) {
            // 降级：实时遍历 getServices()，同时打诊断日志
            String ab01Uuid = "0000ab01-0000-1000-8000-00805f9b34fb";
            java.util.List<BluetoothGattService> services = bluetoothGatt.getServices();
            Log.w("BLE", "[writeProtoChar] cachedAb01Char is null, fallback search in "
                    + services.size() + " services");
            for (BluetoothGattService service : services) {
                for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                    if (characteristic.getUuid().toString().equalsIgnoreCase(ab01Uuid)) {
                        targetChar = characteristic;
                        cachedAb01Char = targetChar;  // 顺便补缓存
                        break;
                    }
                }
                if (targetChar != null) break;
            }
        }
        if (targetChar == null) {
            // 服务尚未就绪（services 为空或 AB01 不存在）
            // 若仍处于连接状态，不直接失败——放回队列并延迟重试
            if (isConnected && serviceNotReadyRetries < MAX_SERVICE_NOT_READY_RETRIES) {
                serviceNotReadyRetries++;
                protoWriteQueue.add(entry); // 放回队尾
                Log.w("BLE", "[writeProtoChar] service not ready, retry " + serviceNotReadyRetries
                        + "/" + MAX_SERVICE_NOT_READY_RETRIES + ", will retry in 500ms");
                // 尝试重新发现服务
                if (bluetoothGatt.getServices().isEmpty()) {
                    bluetoothGatt.discoverServices();
                }
                handler.postDelayed(this::drainProtoWriteQueue, 500);
                return;
            }
            Log.e("BLE", "[writeProtoChar] AB01 not found after " + serviceNotReadyRetries
                    + " retries (connected=" + isConnected + "), failing queued frames");
            serviceNotReadyRetries = 0;
            if (entry.callback != null) entry.callback.accept(false);
            // 清空剩余队列（全部失败）
            ProtoWriteEntry remaining;
            while ((remaining = protoWriteQueue.poll()) != null) {
                if (remaining.callback != null) remaining.callback.accept(false);
            }
            return;
        }
        serviceNotReadyRetries = 0; // 写成功意味着服务已就绪，重置计数

        targetChar.setValue(entry.data);
        protoWriteBusy = true;
        pendingProtoWriteCallback = entry.callback;
        pendingProtoWriteSetAt = android.os.SystemClock.elapsedRealtime();
        boolean result = bluetoothGatt.writeCharacteristic(targetChar);
        Log.d("BLE", "[writeProtoChar] writeCharacteristic result=" + result
                + " queueSize=" + protoWriteQueue.size());
        if (!result) {
            Log.e("BLE", "[writeProtoChar] writeCharacteristic() returned false, draining next");
            protoWriteBusy = false;
            pendingProtoWriteCallback = null;
            if (entry.callback != null) entry.callback.accept(false);
            drainProtoWriteQueue();
        }
    }

    public void sendReliable(byte cmd, byte[] payload, java.util.function.Consumer<Boolean> callback) {
        if (!BleProtocol.isDataCmd(cmd)) {
            if (callback != null) callback.accept(false);
            return;
        }

        byte seq = nextSeq();
        BleProtocol.Frame frame = new BleProtocol.Frame(cmd, seq, payload, payload != null ? payload.length : 0);
        byte[] frameData = BleProtocol.encode(frame);

        new Thread(() -> {
            boolean success = false;
            for (int retry = 0; retry < BleProtocol.MAX_RETRIES; retry++) {
                synchronized (ackLock) {
                    pendingAckSeq = seq;
                    pendingAckReceived = false;
                    ackSuccess = true;
                }

                final boolean[] writeResult = {false};
                final Object writeLock = new Object();

                writeProtoCharacteristic(frameData, result -> {
                    writeResult[0] = result;
                    synchronized (writeLock) {
                        writeLock.notifyAll();
                    }
                });

                synchronized (writeLock) {
                    try {
                        writeLock.wait(500);
                    } catch (InterruptedException ignored) {}
                }

                if (!writeResult[0]) {
                    Log.w("BLE", "[Proto] Write failed, retry " + (retry + 1));
                    try { Thread.sleep(100); } catch (InterruptedException ignored) {}
                    continue;
                }

                synchronized (ackLock) {
                    try {
                        ackLock.wait(BleProtocol.ACK_TIMEOUT_MS);
                    } catch (InterruptedException ignored) {}
                }

                if (pendingAckReceived && ackSuccess && pendingAckSeq == seq) {
                    Log.d("BLE", "[Proto] Reliable send ACK confirmed: cmd=" + BleProtocol.cmdName(cmd));
                    success = true;
                    break;
                }

                Log.w("BLE", "[Proto] ACK timeout, retry " + (retry + 1) + "/" + BleProtocol.MAX_RETRIES);
                try { Thread.sleep(100); } catch (InterruptedException ignored) {}
            }

            if (callback != null) {
                boolean finalSuccess = success;
                handler.post(() -> callback.accept(finalSuccess));
            }
        }).start();
    }

    public void requestSync() {
        byte seq = nextSeq();
        byte[] syncReq = BleProtocol.buildSyncReq(seq);
        Log.d("BLE", "[Proto] Sending SYNC_REQ");
        writeProtoCharacteristic(syncReq, success -> {
            Log.d("BLE", "[Proto] SYNC_REQ sent: " + (success ? "OK" : "FAILED"));
        });
    }

    /**
     * 向 MCU 请求电池曲线数据（从 Flash 读取）。
     * MCU 收到后会回复 CMD_BATTERY_CALIB / CALIB_CMD_STATUS_RSP，包含完整曲线点。
     * 使用 sendReliable（后台线程）保证重试与 ACK 确认，避免写锁卡死时静默丢包。
     */
    public void requestBatteryCurve() {
        Log.d("BLE", "[Calib] requestBatteryCurve queued");
        new Thread(() -> {
            sendReliable(BleProtocol.CMD_BATTERY_CALIB,
                    new byte[]{BleProtocol.CALIB_CMD_STATUS},
                    success -> Log.d("BLE", "[Calib] requestBatteryCurve result=" + success));
        }, "calib-req").start();
    }

    /**
     * 请求产品功能列表（按需查询）。
     * MCU 收到后回复 BLE_CMD_SYSTEM + SYSTEM_SUB_FEATURE_LIST + JSON 字符串。
     * 连接同步时也会自动推送，此方法用于手动刷新。
     */
    public void requestFeatureList() {
        Log.d("BLE", "[Proto] requestFeatureList queued");
        new Thread(() -> {
            sendReliable(BleProtocol.CMD_SYSTEM,
                    new byte[]{BleProtocol.SYSTEM_SUB_FEATURE_LIST},
                    success -> Log.d("BLE", "[Proto] requestFeatureList result=" + success));
        }, "feat-req").start();
    }

    /**
     * Fire-and-forget send (mirrors MCU BleProto_SendOnce).
     * Used to send commands like CALIB_CMD_START to the device.
     */
    public void sendOnce(byte cmd, byte[] payload) {
        byte seq = nextSeq();
        BleProtocol.Frame frame = new BleProtocol.Frame(cmd, seq, payload,
                payload != null ? payload.length : 0);
        byte[] frameData = BleProtocol.encode(frame);
        // 使用非 null callback，确保写锁在 onCharacteristicWrite 后正确释放，
        // 防止并发 sendOnce 导致多帧同时在途（Android BLE 规定同一时刻只能有一个写操作）
        writeProtoCharacteristic(frameData, success -> {
            if (!success) {
                Log.w("BLE", "[sendOnce] write failed: cmd=" + BleProtocol.cmdName(cmd));
            }
        });
    }

    /**
     * 多帧扫描解析器。
     * Android BLE 有时把多帧合并到同一次 onCharacteristicChanged 回调。
     * 此方法逐字扫描 AA 55 头，对每个起始位置尝试小→大依次外扩帧长进行 CRC 校验。
     * CRC 匹配则确认该帧并移动到下一帧起始。
     * @return true 表示至少找到一帧并处理了
     */
    private boolean parseAllProtocolFrames(byte[] data) {
        final int MIN_FRAME = BleProtocol.HDR_SIZE + BleProtocol.CRC_SIZE; // 6 bytes
        boolean found = false;
        int offset = 0;
        while (offset <= data.length - MIN_FRAME) {
            // 找 AA55 头
            if ((data[offset] & 0xFF) != 0xAA || (data[offset + 1] & 0xFF) != 0x55) {
                offset++;
                continue;
            }
            // 跳过 CMD=0x40 (WAV_EXPORT)，这些帧由 wavExportBuffer 路径专门处理
            if (offset + 2 < data.length && (data[offset + 2] & 0xFF) == 0x40) {
                offset++;
                continue;
            }
            // 尝试所有可能的帧长直到缓冲区末尾
            boolean frameFound = false;
            for (int tryLen = MIN_FRAME; tryLen <= data.length - offset; tryLen++) {
                BleProtocol.Frame frame = BleProtocol.decode(data, offset, tryLen);
                if (frame != null) {
                    Log.d("BLE", "[Proto] RX: cmd=" + BleProtocol.cmdName(frame.cmd) +
                            " seq=" + (frame.seq & 0xFF) + " len=" + (frame.len & 0xFF));
                    handleProtocolFrame(frame);
                    offset += tryLen;
                    found = true;
                    frameFound = true;
                    break;
                }
            }
            if (!frameFound) {
                offset++; // 未匹配，跳过这个字节
            }
        }
        return found;
    }
}
