package com.example.myapplication;

import java.util.Set;

/**
 * 产品功能列表数据类
 *
 * 从下位机 BLE 协议读取的 const 字符串解析而来，描述硬件支持的功能。
 * 下位机定义在 product_features.h 中，编译后存储在 flash .rodata 段，不可更改。
 *
 * JSON 格式:
 * {"ver":1,"pid":1,"fw":"1.0.1","hw":"v1.0","features":["eq","reverb",...]}
 */
public class ProductFeatures {
    /** 功能列表格式版本 */
    public int ver;
    /** 产品 ID */
    public int pid;
    /** 固件版本 */
    public String fwVersion;
    /** 硬件版本 */
    public String hwVersion;
    /** 支持的功能名称集合 */
    public Set<String> features;

    /** 检查是否支持指定功能 */
    public boolean hasFeature(String name) {
        return features != null && features.contains(name);
    }

    /** 功能名称常量（与下位机 product_features.h 中的定义对应） */
    public static final String FEAT_EQ            = "eq";
    public static final String FEAT_REVERB        = "reverb";
    public static final String FEAT_DELAY         = "delay";
    public static final String FEAT_DRC           = "drc";
    public static final String FEAT_GAIN          = "gain";
    public static final String FEAT_LOOPER        = "looper";
    public static final String FEAT_METRONOME     = "metronome";
    public static final String FEAT_DRUM          = "drum";
    public static final String FEAT_WAV_EXPORT    = "wav_export";
    public static final String FEAT_BATTERY_CALIB = "battery_calib";
    public static final String FEAT_USB_AUDIO     = "usb_audio";
    public static final String FEAT_BLE           = "ble";
    public static final String FEAT_FW_UPGRADE    = "fw_upgrade";

    @Override
    public String toString() {
        return "ProductFeatures{ver=" + ver + ", pid=0x" + String.format("%04X", pid)
                + ", fw='" + fwVersion + "', hw='" + hwVersion + "'"
                + ", features=" + (features != null ? features.toString() : "null") + "}";
    }
}
