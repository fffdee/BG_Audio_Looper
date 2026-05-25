package com.example.myapplication;

import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * 接收 MCU 通过 BLE 发送的 WAV 导出数据，组装并保存为 WAV 文件。
 *
 * BLE 导出协议 (CMD = 0x40):
 *   子命令 0x02: 导出开始 → WAV 头 + 总包数 + 总数据字节
 *   子命令 0x03: 数据包   → 包序号 + PCM 数据 (≤192B)
 *   子命令 0x04: 导出完成 → 结果码
 */
public class WavBleReceiver {
    private static final String TAG = "WavBleReceiver";

    // 子命令定义（与 MCU 端 looper_wav_ble_export.h 一致）
    private static final int SUBCMD_EXPORT_START = 0x02;
    private static final int SUBCMD_DATA_PACKET  = 0x03;
    private static final int SUBCMD_EXPORT_END   = 0x04;

    // 结果码
    private static final int RESULT_OK             = 0;
    private static final int RESULT_ERROR          = 1;
    private static final int RESULT_CANCELLED      = 2;
    private static final int RESULT_BUSY           = 3;
    private static final int RESULT_NO_DATA        = 4;
    private static final int RESULT_LEN_MISMATCH   = 5;

    public enum State { IDLE, RECEIVING, COMPLETED, ERROR }

    public interface Listener {
        void onProgress(int receivedPackets, int totalPackets);
        /** @param savedPath 已保存路径，如 "音乐/BanBox/BanBox_Loop_xxx.wav" */
        void onComplete(Uri savedUri, String savedPath);
        void onError(String message);
    }

    private State state = State.IDLE;
    private Listener listener;
    private Context context;
    /** 可选自定义文件名（不含扩展名），null 时使用时间戳 */
    private String customFileName = null;

    // 导出会话数据
    private byte[] wavHeader;
    private int totalPackets;
    private long totalDataBytes;
    private int receivedPackets;
    private ByteArrayOutputStream pcmBuffer;

    public WavBleReceiver(Context context) {
        this.context = context.getApplicationContext();
    }

    public void setListener(Listener listener) {
        this.listener = listener;
    }

    /** 设置导出文件名（不含扩展名）；null 或空字符串恢复为时间戳命名 */
    public void setCustomFileName(String name) {
        this.customFileName = (name != null && !name.trim().isEmpty()) ? name.trim() : null;
    }

    public State getState() {
        return state;
    }

    public boolean isBusy() {
        return state == State.RECEIVING;
    }

    public int getReceivedPackets() {
        return receivedPackets;
    }

    public int getTotalPackets() {
        return totalPackets;
    }

    /**
     * 处理从 BLE 收到的 WAV 导出帧 (CMD=0x40 的 payload)
     * @param payload BLE 帧 payload 字节数组
     */
    public void handlePayload(byte[] payload) {
        if (payload == null || payload.length < 1) return;
        int subcmd = payload[0] & 0xFF;

        switch (subcmd) {
            case SUBCMD_EXPORT_START:
                handleExportStart(payload);
                break;
            case SUBCMD_DATA_PACKET:
                handleDataPacket(payload);
                break;
            case SUBCMD_EXPORT_END:
                handleExportEnd(payload);
                break;
            default:
                Log.w(TAG, "Unknown WAV export subcmd: 0x" + Integer.toHexString(subcmd));
                break;
        }
    }

    /**
     * 从 hex 字符串形式的 BLE 帧数据中解析 WAV 导出 payload。
     * 帧格式 hex: AA55 40 SEQ [payload...] CRC16
     * @param hexFrame 完整帧的 hex 字符串（大写，无空格）
     */
    public void handleHexFrame(String hexFrame) {
        if (hexFrame == null || hexFrame.length() < 12) return;
        // HDR(4B=8hex) + CRC(2B=4hex) = 12 hex chars minimum
        int payloadHexLen = hexFrame.length() - 8 - 4; // minus header(4B=8hex) and CRC(2B=4hex)
        if (payloadHexLen <= 0) return;

        byte[] payload = new byte[payloadHexLen / 2];
        for (int i = 0; i < payload.length; i++) {
            int hexOffset = 8 + i * 2; // skip 4-byte header (8 hex chars)
            payload[i] = (byte) Integer.parseInt(hexFrame.substring(hexOffset, hexOffset + 2), 16);
        }
        handlePayload(payload);
    }

    private void handleExportStart(byte[] payload) {
        // payload[0] = 0x02
        // payload[1..4] = total_packets (uint32 LE)
        // payload[5..8] = total_data_bytes (uint32 LE)
        // payload[9..52] = WAV header (44 bytes)
        if (payload.length < 53) {
            Log.e(TAG, "EXPORT_START payload too short: " + payload.length);
            notifyError("导出启动包数据不完整");
            return;
        }

        totalPackets = readU32LE(payload, 1);
        totalDataBytes = readU32LE(payload, 5) & 0xFFFFFFFFL;
        wavHeader = new byte[44];
        System.arraycopy(payload, 9, wavHeader, 0, 44);
        receivedPackets = 0;
        pcmBuffer = new ByteArrayOutputStream((int) Math.min(totalDataBytes, 16 * 1024 * 1024));
        state = State.RECEIVING;

        Log.i(TAG, String.format("Export started: %d packets, %d bytes", totalPackets, totalDataBytes));
    }

    private void handleDataPacket(byte[] payload) {
        if (state != State.RECEIVING) {
            Log.w(TAG, "DATA_PACKET received but not in RECEIVING state");
            return;
        }
        // payload[0] = 0x03
        // payload[1..4] = packet_index (uint32 LE)
        // payload[5..N] = PCM data
        if (payload.length < 6) return;

        int packetIndex = readU32LE(payload, 1);
        int pcmLen = payload.length - 5;
        pcmBuffer.write(payload, 5, pcmLen);
        receivedPackets++;

        if (listener != null) {
            listener.onProgress(receivedPackets, totalPackets);
        }
    }

    private void handleExportEnd(byte[] payload) {
        // payload[0] = 0x04
        // payload[1] = result code
        int result = (payload.length >= 2) ? (payload[1] & 0xFF) : RESULT_ERROR;

        if (result == RESULT_OK && state == State.RECEIVING) {
            state = State.COMPLETED;
            saveWavFile();
        } else {
            state = State.ERROR;
            String msg;
            switch (result) {
                case RESULT_CANCELLED:    msg = "导出已取消"; break;
                case RESULT_BUSY:         msg = "设备忙"; break;
                case RESULT_NO_DATA:      msg = "无录音数据"; break;
                case RESULT_LEN_MISMATCH: msg = "段长度不一致，无法混音"; break;
                default:                  msg = "导出失败 (错误码: " + result + ")"; break;
            }
            notifyError(msg);
            cleanup();
        }
    }

    /**
     * 组装 WAV 头 + PCM 数据并保存到 Music 目录。
     */
    private void saveWavFile() {
        if (wavHeader == null || pcmBuffer == null) {
            notifyError("内部错误：无 WAV 数据");
            cleanup();
            return;
        }

        String baseName;
        if (customFileName != null) {
            // 过滤文件名中的非法字符，只保留字母/数字/下划线/横杠/空格
            baseName = customFileName.replaceAll("[^a-zA-Z0-9_\\-\\s]", "_");
            customFileName = null; // 用完即清，防止下次复用
        } else {
            baseName = "BanBox_Loop_" +
                    new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(new Date());
        }
        String fileName = baseName + ".wav";
        // 显示给用户看的相对路径
        String displayPath = Environment.DIRECTORY_MUSIC + "/BanBox/" + fileName;

        try {
            Uri savedUri = saveToMediaStore(fileName);
            state = State.IDLE;
            if (listener != null) {
                listener.onComplete(savedUri, displayPath);
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to save WAV file", e);
            state = State.ERROR;
            notifyError("保存文件失败: " + e.getMessage());
        } finally {
            cleanup();
        }
    }

    private Uri saveToMediaStore(String fileName) throws IOException {
        ContentValues values = new ContentValues();
        values.put(MediaStore.Audio.Media.DISPLAY_NAME, fileName);
        values.put(MediaStore.Audio.Media.MIME_TYPE, "audio/wav");

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            values.put(MediaStore.Audio.Media.RELATIVE_PATH, Environment.DIRECTORY_MUSIC + "/BanBox");
            values.put(MediaStore.Audio.Media.IS_PENDING, 1);
        }

        Uri collection = MediaStore.Audio.Media.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
        Uri uri = context.getContentResolver().insert(collection, values);
        if (uri == null) {
            throw new IOException("MediaStore insert returned null");
        }

        try (OutputStream os = context.getContentResolver().openOutputStream(uri)) {
            if (os == null) throw new IOException("Cannot open output stream");
            os.write(wavHeader);
            pcmBuffer.writeTo(os);
            os.flush();
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            values.clear();
            values.put(MediaStore.Audio.Media.IS_PENDING, 0);
            context.getContentResolver().update(uri, values, null, null);
        }

        Log.i(TAG, "WAV saved: " + fileName + " → " + uri);
        return uri;
    }

    public void reset() {
        state = State.IDLE;
        customFileName = null;
        cleanup();
    }

    private void cleanup() {
        wavHeader = null;
        pcmBuffer = null;
        receivedPackets = 0;
        totalPackets = 0;
        totalDataBytes = 0;
    }

    private void notifyError(String msg) {
        Log.e(TAG, msg);
        if (listener != null) {
            listener.onError(msg);
        }
    }

    private static int readU32LE(byte[] data, int offset) {
        return (data[offset] & 0xFF)
             | ((data[offset + 1] & 0xFF) << 8)
             | ((data[offset + 2] & 0xFF) << 16)
             | ((data[offset + 3] & 0xFF) << 24);
    }
}
