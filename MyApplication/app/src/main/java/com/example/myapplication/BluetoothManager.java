package com.example.myapplication;

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
}
