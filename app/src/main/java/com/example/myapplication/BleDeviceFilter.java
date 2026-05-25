package com.example.myapplication;

import android.util.Log;

/**
 * BLE设备过滤工具类
 * 用于过滤BG品牌设备（MAC地址前两个字节为0x42和0x47）
 */
public class BleDeviceFilter {
    private static final String TAG = "BleDeviceFilter";
    
    // BG设备的MAC地址前缀：42:47
    private static final int BG_PREFIX_BYTE0 = 0x42;  // 'B'
    private static final int BG_PREFIX_BYTE1 = 0x47;  // 'G'
    
    /**
     * 检查设备MAC地址是否是BG设备
     * 
     * @param address MAC地址字符串，格式: XX:XX:XX:XX:XX:XX
     * @return true表示是BG设备，false表示不是
     */
    public static boolean isBGDevice(String address) {
        if (address == null || address.length() < 5) {
            return false;
        }
        
        String[] addressParts = address.split(":");
        if (addressParts.length < 2) {
            return false;
        }
        
        try {
            int byte0 = Integer.parseInt(addressParts[0], 16);
            int byte1 = Integer.parseInt(addressParts[1], 16);
            
            // 只接受地址为 42:47:XX:XX:XX:XX 的设备
            return (byte0 == BG_PREFIX_BYTE0 && byte1 == BG_PREFIX_BYTE1);
        } catch (NumberFormatException e) {
            Log.e(TAG, "Invalid MAC address format: " + address, e);
            return false;
        }
    }
    
    /**
     * 获取设备显示信息（已过滤）
     * 
     * @param deviceName 设备名称，可以为null
     * @param address MAC地址
     * @return 如果是BG设备则返回格式化的字符串，否则返回null
     */
    public static String getDeviceInfo(String deviceName, String address) {
        if (!isBGDevice(address)) {
            return null;
        }
        
        String name = (deviceName != null && !deviceName.isEmpty()) ? deviceName : "未知设备";
        return name + " (" + address + ")";
    }
    
    /**
     * 获取BG设备前缀说明
     * 
     * @return 前缀说明字符串
     */
    public static String getFilterDescription() {
        return String.format("只显示MAC地址前缀为%02X:%02X的BG设备", 
                           BG_PREFIX_BYTE0, BG_PREFIX_BYTE1);
    }
}
