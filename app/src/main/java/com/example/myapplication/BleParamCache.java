package com.example.myapplication;

import android.util.Log;
<<<<<<< Updated upstream
import java.util.HashMap;
import java.util.Map;
=======
<<<<<<< HEAD
import java.util.HashMap;
import java.util.Map;
=======
import org.json.JSONObject;
import org.json.JSONArray;
import org.json.JSONException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes

public class BleParamCache {
    private static final String TAG = "BleParamCache";
    private static BleParamCache instance;

    public interface OnParamUpdateListener {
        void onParamUpdated(byte cmd, byte[] payload);
        void onSyncComplete();
    }

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
    /** Listener for product feature list updates */
    public interface OnFeatureListListener {
        void onFeatureListReceived(ProductFeatures features);
    }

>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    private final Map<Byte, byte[]> cache = new HashMap<>();
    private OnParamUpdateListener listener;
    private boolean syncComplete = false;
    private int productId = 0;

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
    /* 产品功能列表 */
    private ProductFeatures productFeatures = null;
    private OnFeatureListListener featureListListener;

>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    private BleParamCache() {}

    public static synchronized BleParamCache getInstance() {
        if (instance == null) {
            instance = new BleParamCache();
        }
        return instance;
    }

    public void setListener(OnParamUpdateListener listener) {
        this.listener = listener;
    }

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
    public void setFeatureListListener(OnFeatureListListener listener) {
        this.featureListListener = listener;
    }

>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    public void put(byte cmd, byte[] payload) {
        byte[] copy = new byte[payload.length];
        System.arraycopy(payload, 0, copy, 0, payload.length);
        cache.put(cmd, copy);
        Log.d(TAG, "Cached param: " + BleProtocol.cmdName(cmd) + " len=" + payload.length);
        if (listener != null) {
            listener.onParamUpdated(cmd, payload);
        }
    }

    public byte[] get(byte cmd) {
        return cache.get(cmd);
    }

    public boolean has(byte cmd) {
        return cache.containsKey(cmd);
    }

    public void setSyncComplete(boolean complete) {
        this.syncComplete = complete;
        if (complete && listener != null) {
            listener.onSyncComplete();
        }
    }

    public boolean isSyncComplete() {
        return syncComplete;
    }

    public void clear() {
        cache.clear();
        syncComplete = false;
        productId = 0;
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
        productFeatures = null;
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    }

    public void setProductId(int id) {
        this.productId = id;
        Log.d(TAG, "Product ID set: 0x" + String.format("%04X", id));
    }

    public int getProductId() {
        return productId;
    }

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
    /* ================================================================
     *  产品功能列表
     * ================================================================ */

    /**
     * 从 BLE payload 解析并存储产品功能列表
     * @param payload  BLE_CMD_SYSTEM 帧 payload, payload[0]=子命令, payload[1..N]=JSON 字符串
     */
    public void setFeatureListFromPayload(byte[] payload) {
        if (payload == null || payload.length < 2) return;
        String json = new String(payload, 1, payload.length - 1);
        Log.d(TAG, "Feature list JSON: " + json);
        try {
            JSONObject obj = new JSONObject(json);
            ProductFeatures pf = new ProductFeatures();
            pf.ver = obj.optInt("ver", 0);
            pf.pid = obj.optInt("pid", 0);
            pf.fwVersion = obj.optString("fw", "");
            pf.hwVersion = obj.optString("hw", "");
            JSONArray arr = obj.optJSONArray("features");
            if (arr != null) {
                pf.features = new HashSet<>();
                for (int i = 0; i < arr.length(); i++) {
                    pf.features.add(arr.getString(i));
                }
            }
            productFeatures = pf;
            Log.d(TAG, "Parsed " + (pf.features != null ? pf.features.size() : 0) + " features");
            if (featureListListener != null) {
                featureListListener.onFeatureListReceived(pf);
            }
        } catch (JSONException e) {
            Log.e(TAG, "Failed to parse feature list JSON", e);
        }
    }

    public ProductFeatures getProductFeatures() {
        return productFeatures;
    }

    /** 检查是否支持指定功能 */
    public boolean hasFeature(String name) {
        return productFeatures != null && productFeatures.features != null
                && productFeatures.features.contains(name);
    }

>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    public int[] getDrcParams() {
        byte[] data = cache.get(BleProtocol.CMD_DRC);
        if (data == null || data.length < 8) return null;
        int threshold = (data[0] & 0xFF) | ((data[1] & 0xFF) << 8);
        int ratio     = (data[2] & 0xFF) | ((data[3] & 0xFF) << 8);
        int attack    = (data[4] & 0xFF) | ((data[5] & 0xFF) << 8);
        int release   = (data[6] & 0xFF) | ((data[7] & 0xFF) << 8);
        return new int[]{threshold, ratio, attack, release};
    }

    public int[] getReverbParams() {
        byte[] data = cache.get(BleProtocol.CMD_REVERB);
        if (data == null || data.length < 3) return null;
        int roomSize = data[0] & 0xFF;
        int damping  = data[1] & 0xFF;
        int wetDry   = data[2] & 0xFF;
        return new int[]{roomSize, damping, wetDry};
    }

    public int[] getVolumeParams() {
        byte[] data = cache.get(BleProtocol.CMD_VOLUME);
        if (data == null || data.length < 5) return null;
        return new int[]{data[0] & 0xFF, data[1] & 0xFF, data[2] & 0xFF, data[3] & 0xFF, data[4] & 0xFF};
    }

    public int[] getMetronomeParams() {
        byte[] data = cache.get(BleProtocol.CMD_METRONOME);
        if (data == null || data.length < 5) return null;
        return new int[]{data[0] & 0xFF, data[1] & 0xFF, data[2] & 0xFF, data[3] & 0xFF, data[4] & 0xFF};
    }

    public int[] getLooperParams() {
        byte[] data = cache.get(BleProtocol.CMD_LOOPER);
        if (data == null || data.length < 8) return null;
        int loopCount = data[0] & 0xFF;
        int overdub   = data[1] & 0xFF;
        int quantize  = data[2] & 0xFF;
        int clickVol  = data[3] & 0xFF;
        int tempo     = data[4] & 0xFF;
        int timeSig   = data[5] & 0xFF;
        int fadeTime  = (data[6] & 0xFF) | ((data[7] & 0xFF) << 8);
        /* 扩展：各段录制源 (12字节 payload) */
        int src0 = (data.length >= 12) ? (data[8]  & 0xFF) : 4;
        int src1 = (data.length >= 12) ? (data[9]  & 0xFF) : 4;
        int src2 = (data.length >= 12) ? (data[10] & 0xFF) : 4;
        int src3 = (data.length >= 12) ? (data[11] & 0xFF) : 4;
        /* 扩展：导出设置 (15字节 payload, gain_pct 为 uint16_t LE) */
        int exportMonoMix = (data.length >= 13) ? (data[12] & 0xFF) : 0;
        int exportGainPct = (data.length >= 15)
                ? ((data[13] & 0xFF) | ((data[14] & 0xFF) << 8)) : 100;
        return new int[]{loopCount, overdub, quantize, clickVol, tempo, timeSig, fadeTime,
                         src0, src1, src2, src3, exportMonoMix, exportGainPct};
    }

    public byte[] getEqRaw() {
        return cache.get(BleProtocol.CMD_EQ);
    }

    /**
     * 解析 CMD_LOOPER_SEG_STATE (0x21) 载荷
     * payload: [s0,s1,s2,s3, s0_len_lo,s0_len_hi, s1_len_lo,s1_len_hi,
     *           s2_len_lo,s2_len_hi, s3_len_lo,s3_len_hi]  (共 12 字节)
     * @return int[8]: [state0..3, lenPages0..3]，或 null（数据不足）
     */
    public int[] getLooperSegStates() {
        byte[] data = cache.get(BleProtocol.CMD_LOOPER_SEG_STATE);
        if (data == null || data.length < 12) return null;
        int[] result = new int[8];
        for (int i = 0; i < 4; i++) result[i] = data[i] & 0xFF;
        for (int i = 0; i < 4; i++) {
            result[4 + i] = (data[4 + i * 2] & 0xFF) | ((data[4 + i * 2 + 1] & 0xFF) << 8);
        }
        return result;
    }
}
