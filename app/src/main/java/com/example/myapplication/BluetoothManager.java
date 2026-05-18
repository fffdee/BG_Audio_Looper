package com.example.myapplication;

import androidx.lifecycle.LiveData;

/**
 * 全局蓝牙管理器单例
 * 用于在整个应用中共享BluetoothHelper实例，确保连接状态一致
 */
public class BluetoothManager {
    private static BluetoothManager instance;
    private BluetoothHelper bluetoothHelper;

    private BluetoothManager() {
        bluetoothHelper = new BluetoothHelper();
    }

    public static synchronized BluetoothManager getInstance() {
        if (instance == null) {
            instance = new BluetoothManager();
        }
        return instance;
    }

    public BluetoothHelper getBluetoothHelper() {
        return bluetoothHelper;
    }

    /**
     * 获取全局蓝牙连接状态 LiveData
     * 所有界面可 observe 此 LiveData 以实时同步蓝牙状态
     */
    public LiveData<BleConnectionState> getConnectionState() {
        return bluetoothHelper.getConnectionStateLiveData();
    }

    /**
     * 获取参数同步状态 LiveData
     * true = 正在同步中，false = 空闲 / 同步完成
     */
    public LiveData<Boolean> getSyncingState() {
        return bluetoothHelper.getLiveSyncing();
    }

    /**
     * 获取设备电量 LiveData（0~100，-1 表示尚未收到数据）
     */
    public LiveData<Integer> getBatteryLevel() {
        return bluetoothHelper.getBatteryLevelLiveData();
    }

    /**
     * 获取系统状态 LiveData（0=IDLE 1=NORMAL 2=TRANSFER，-1=尚未收到）
     */
    public LiveData<Integer> getSysState() {
        return bluetoothHelper.getSysStateLiveData();
    }

    /**
     * 获取电量曲线矫正状态 LiveData（"Idle" 或 "Running: step N (XXXX mV)"）
     */
    public LiveData<String> getCalibState() {
        return bluetoothHelper.getCalibStateLiveData();
    }

    /**
     * 向设备发送电量矫正命令（CALIB_CMD_START / STOP / STATUS / CLEAR）
     * 使用后台线程 + sendReliable，确保 ACK 确认与自动重试。
     */
    public void sendCalibCmd(byte subCmd) {
        new Thread(() -> {
            bluetoothHelper.sendReliable(BleProtocol.CMD_BATTERY_CALIB,
                    new byte[]{ subCmd },
                    success -> android.util.Log.d("BLE",
                            "[Calib] sendCalibCmd 0x" + Integer.toHexString(subCmd & 0xFF)
                                    + " result=" + success));
        }, "calib-cmd").start();
    }
}
