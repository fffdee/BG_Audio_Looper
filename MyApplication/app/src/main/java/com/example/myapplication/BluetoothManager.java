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
}
