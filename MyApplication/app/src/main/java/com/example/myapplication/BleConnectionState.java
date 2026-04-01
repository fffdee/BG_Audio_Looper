package com.example.myapplication;

import android.bluetooth.BluetoothGatt;

/**
 * BLE 连接状态数据类，用于 LiveData 广播
 */
public class BleConnectionState {
    public enum State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED
    }

    private final State state;
    private final String deviceName;
    private final BluetoothGatt gatt;

    public BleConnectionState(State state, String deviceName, BluetoothGatt gatt) {
        this.state = state;
        this.deviceName = deviceName;
        this.gatt = gatt;
    }

    public static BleConnectionState disconnected() {
        return new BleConnectionState(State.DISCONNECTED, null, null);
    }

    public static BleConnectionState connected(String deviceName, BluetoothGatt gatt) {
        return new BleConnectionState(State.CONNECTED, deviceName, gatt);
    }

    public State getState() { return state; }
    public String getDeviceName() { return deviceName; }
    public BluetoothGatt getGatt() { return gatt; }
    public boolean isConnected() { return state == State.CONNECTED; }
}
