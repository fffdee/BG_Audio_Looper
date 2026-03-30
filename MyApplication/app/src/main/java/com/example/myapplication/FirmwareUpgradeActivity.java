package com.example.myapplication;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.example.myapplication.BluetoothHelper;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.Deque;

/**
 * FirmwareUpgradeActivity — BLE 固件升级
 *
 * 流程：
 * 1. SYNC(0x01)       — 握手，获取协议版本
 * 2. QUERY_INFO(0x07) — 查询设备分区信息
 * 3. ERASE(0x06)      — 擦除B分区
 * 4. START(0x02)      — 发起升级，指定固件大小
 * 5. DATA(0x03) ×N    — 分块发送固件数据
 * 6. FINISH(0x04)     — 完成传输，标记B分区为待升级
 * 7. JUMP(0x05)       — 请求重启到B分区
 *
 * 使用 BluetoothHelper 的原始字节回调处理升级协议响应。
 */
public class FirmwareUpgradeActivity extends AppCompatActivity {
    private static final String TAG = "FirmwareUpgrade";

    // UI 控件
    private TextView tvBleStatus, tvProtocolVer, tvActivePart, tvFwSize;
    private TextView tvFileName, tvFileSize;
    private TextView tvProgressText, tvProgressPct;
    private ProgressBar progressBar;
    private TextView tvLog;
    private ScrollView scrollLog;
    private Button btnSelectFile, btnQueryInfo, btnStartUpgrade, btnRollback;

    // 数据
    private BluetoothHelper bluetoothHelper;
    private Handler mainHandler = new Handler(Looper.getMainLooper());
    private Uri selectedFileUri;
    private byte[] firmwareData;
    private int firmwareSize;

    // 升级状态机
    private enum UpgradeState {
        IDLE, SYNCING, QUERYING, ERASING, STARTING, UPLOADING, FINISHING, JUMPING, REBOOTING
    }

    private UpgradeState upgradeState = UpgradeState.IDLE;
    private int uploadedBytes = 0;
    private int totalBytes = 0;
    private short nextSeq = 0;

    // 协议常量
    private static final byte SOF = (byte) 0xAA;
    private static final byte CMD_SYNC = 0x01;
    private static final byte CMD_START = 0x02;
    private static final byte CMD_DATA = 0x03;
    private static final byte CMD_FINISH = 0x04;
    private static final byte CMD_JUMP = 0x05;
    private static final byte CMD_ERASE = 0x06;
    private static final byte CMD_QUERY_INFO = 0x07;
    private static final byte CMD_SET_PART = 0x08;
    private static final byte CMD_REBOOT = 0x09;

    private static final byte RSP_ACK = (byte) 0xA1;
    private static final byte RSP_NACK = (byte) 0xA2;

    private static final int CHUNK_SIZE = 256;
    private static final int TIMEOUT_MS = 5000;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_firmware_upgrade);

        // 查找UI控件
        initViews();

        // 获取 BluetoothHelper 实例
        bluetoothHelper = BluetoothHelperSingleton.getInstance();
        updateBleStatus();

        // 设置原始字节回调，用于接收升级协议响应
        bluetoothHelper.setRawDataListener(this::onRawDataReceived);

        // 返回按钮
        ImageButton btnBack = findViewById(R.id.btn_back);
        if (btnBack != null) {
            btnBack.setOnClickListener(v -> finish());
        }

        // 选择文件
        btnSelectFile.setOnClickListener(v -> selectFirmwareFile());

        // 查询设备信息
        btnQueryInfo.setOnClickListener(v -> doQueryInfo());

        // 开始升级
        btnStartUpgrade.setOnClickListener(v -> startUpgradeProcess());

        // 回退至A区
        btnRollback.setOnClickListener(v -> doRollback());
    }

    private void initViews() {
        tvBleStatus = findViewById(R.id.tv_ble_status);
        tvProtocolVer = findViewById(R.id.tv_protocol_ver);
        tvActivePart = findViewById(R.id.tv_active_part);
        tvFwSize = findViewById(R.id.tv_fw_size);
        tvFileName = findViewById(R.id.tv_file_name);
        tvFileSize = findViewById(R.id.tv_file_size);
        tvProgressText = findViewById(R.id.tv_progress_text);
        tvProgressPct = findViewById(R.id.tv_progress_pct);
        progressBar = findViewById(R.id.progress_bar);
        tvLog = findViewById(R.id.tv_log);
        scrollLog = findViewById(R.id.scroll_log);
        btnSelectFile = findViewById(R.id.btn_select_file);
        btnQueryInfo = findViewById(R.id.btn_query_info);
        btnStartUpgrade = findViewById(R.id.btn_start_upgrade);
        btnRollback = findViewById(R.id.btn_rollback);
    }

    private void updateBleStatus() {
        if (bluetoothHelper.isConnected()) {
            tvBleStatus.setText("已连接");
            tvBleStatus.setTextColor(0xFF66CC99);
            btnSelectFile.setEnabled(true);
            btnQueryInfo.setEnabled(true);
        } else {
            tvBleStatus.setText("未连接");
            tvBleStatus.setTextColor(0xFFFF6666);
            btnSelectFile.setEnabled(false);
            btnQueryInfo.setEnabled(false);
            btnStartUpgrade.setEnabled(false);
            btnRollback.setEnabled(false);
        }
    }

    // ======================== 文件选择 ========================
    private void selectFirmwareFile() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, 1001);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 1001 && resultCode == RESULT_OK && data != null) {
            selectedFileUri = data.getData();
            if (selectedFileUri != null) {
                loadFirmwareFile(selectedFileUri);
            }
        }
    }

    private void loadFirmwareFile(Uri uri) {
        try (InputStream is = getContentResolver().openInputStream(uri)) {
            firmwareData = new byte[16 * 1024 * 1024];  // Max 16MB
            int bytesRead = is.read(firmwareData);
            firmwareSize = bytesRead;

            String fileName = getFileName(uri);
            tvFileName.setText(fileName != null ? fileName : "固件文件");
            tvFileSize.setText(String.format("大小: %.1f KB", firmwareSize / 1024.0));

            appendLog("[FILE] 已加载固件: " + firmwareSize + " 字节");
            btnStartUpgrade.setEnabled(true);

        } catch (IOException e) {
            Log.e(TAG, "Failed to load firmware file", e);
            Toast.makeText(this, "读取文件失败: " + e.getMessage(), Toast.LENGTH_SHORT).show();
        }
    }

    private String getFileName(Uri uri) {
        try (android.database.Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int columnIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                return cursor.getString(columnIndex);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to get file name", e);
        }
        return null;
    }

    // ======================== 协议方法 ========================
    // CRC16-CCITT
    private int crc16(byte[] data, int offset, int len) {
        int crc = 0xFFFF;
        for (int i = offset; i < offset + len; i++) {
            crc ^= (data[i] & 0xFF) << 8;
            for (int j = 0; j < 8; j++) {
                if ((crc & 0x8000) != 0) {
                    crc = ((crc << 1) ^ 0x1021) & 0xFFFF;
                } else {
                    crc = (crc << 1) & 0xFFFF;
                }
            }
        }
        return crc;
    }

    // 构建数据包
    private byte[] buildPacket(byte cmd, byte[] payload) {
        int payloadLen = (payload != null) ? payload.length : 0;
        int totalLen = 1 + 1 + 2 + 2 + payloadLen + 2;
        byte[] pkt = new byte[totalLen];
        int idx = 0;

        pkt[idx++] = SOF;
        pkt[idx++] = cmd;
        pkt[idx++] = (byte) ((nextSeq >> 8) & 0xFF);
        pkt[idx++] = (byte) (nextSeq & 0xFF);
        pkt[idx++] = (byte) ((payloadLen >> 8) & 0xFF);
        pkt[idx++] = (byte) (payloadLen & 0xFF);

        if (payload != null) {
            System.arraycopy(payload, 0, pkt, idx, payloadLen);
            idx += payloadLen;
        }

        // CRC over cmd+seq+len+payload
        int crc = crc16(pkt, 1, 5 + payloadLen);
        pkt[idx++] = (byte) ((crc >> 8) & 0xFF);
        pkt[idx++] = (byte) (crc & 0xFF);

        nextSeq = (short) ((nextSeq + 1) & 0xFFFF);
        return pkt;
    }

    // ======================== 升级步骤 ========================
    private void doQueryInfo() {
        if (!bluetoothHelper.isConnected()) {
            Toast.makeText(this, "设备未连接", Toast.LENGTH_SHORT).show();
            return;
        }
        upgradeState = UpgradeState.QUERYING;
        appendLog("[QUERY] 发送查询设备信息...");
        byte[] pkt = buildPacket(CMD_QUERY_INFO, null);
        bluetoothHelper.writeCharacteristic("ab01", pkt, success -> {
            if (!success) appendLog("[ERR] 发送QUERY_INFO失败");
        });
    }

    private void startUpgradeProcess() {
        if (!bluetoothHelper.isConnected()) {
            Toast.makeText(this, "设备未连接", Toast.LENGTH_SHORT).show();
            return;
        }
        if (firmwareData == null || firmwareSize == 0) {
            Toast.makeText(this, "未选择固件文件", Toast.LENGTH_SHORT).show();
            return;
        }

        upgradeState = UpgradeState.SYNCING;
        uploadedBytes = 0;
        totalBytes = firmwareSize;
        nextSeq = 0;

        appendLog("[START] 开始升级流程");
        appendLog("[SYNC] 发送同步...");

        byte[] pkt = buildPacket(CMD_SYNC, null);
        bluetoothHelper.writeCharacteristic("ab01", pkt, success -> {
            if (!success) appendLog("[ERR] 发送SYNC失败");
        });
    }

    private void doRollback() {
        if (!bluetoothHelper.isConnected()) {
            Toast.makeText(this, "设备未连接", Toast.LENGTH_SHORT).show();
            return;
        }
        appendLog("[ROLLBACK] 将活跃分区切换为A...");
        byte[] pkt = buildPacket(CMD_SET_PART, new byte[]{0});
        bluetoothHelper.writeCharacteristic("ab01", pkt, success -> {
            if (success) {
                appendLog("[ROLLBACK] 命令已发送，重启生效");
            } else {
                appendLog("[ERR] 发送SET_PART失败");
            }
        });
    }

    // ======================== 响应处理 ========================
    private void onRawDataReceived(byte[] data) {
        if (data == null || data.length < 8) return;

        mainHandler.post(() -> {
            try {
                byte sof = data[0];
                if (sof != SOF) {
                    appendLog("[WARN] 无效SOF字节: 0x" + String.format("%02X", sof & 0xFF));
                    return;
                }

                byte cmd = data[1];
                int seq = ((data[2] & 0xFF) << 8) | (data[3] & 0xFF);
                int len = ((data[4] & 0xFF) << 8) | (data[5] & 0xFF);

                if (cmd == RSP_ACK) {
                    handleAck(seq, data, len);
                } else if (cmd == RSP_NACK) {
                    byte err = (len > 0) ? data[6] : 0;
                    handleNack(seq, err);
                }
            } catch (Exception e) {
                Log.e(TAG, "Error processing response", e);
            }
        });
    }

    private void handleAck(int seq, byte[] data, int dataLen) {
        switch (upgradeState) {
        case SYNCING:
            appendLog("[SYNC] ACK收到，协议版本: v" + (dataLen > 0 ? (data[6] & 0xFF) : "?"));
            upgradeState = UpgradeState.ERASING;
            appendLog("[ERASE] 正在擦除B分区...");
            byte[] erasePkt = buildPacket(CMD_ERASE, null);
            bluetoothHelper.writeCharacteristic("ab01", erasePkt, success -> {
                if (!success) appendLog("[ERR] 发送ERASE失败");
            });
            break;

        case ERASING:
            appendLog("[ERASE] 完成 ✓");
            upgradeState = UpgradeState.STARTING;
            appendLog("[START] 发起升级, 大小=" + totalBytes + "字节");
            byte[] sizeBuf = new byte[4];
            sizeBuf[0] = (byte) ((totalBytes >> 24) & 0xFF);
            sizeBuf[1] = (byte) ((totalBytes >> 16) & 0xFF);
            sizeBuf[2] = (byte) ((totalBytes >> 8) & 0xFF);
            sizeBuf[3] = (byte) (totalBytes & 0xFF);
            byte[] startPkt = buildPacket(CMD_START, sizeBuf);
            bluetoothHelper.writeCharacteristic("ab01", startPkt, success -> {
                if (!success) appendLog("[ERR] 发送START失败");
            });
            break;

        case STARTING:
            appendLog("[START] ACK收到, 开始发送数据");
            upgradeState = UpgradeState.UPLOADING;
            sendNextChunk();
            break;

        case UPLOADING:
            updateProgress();
            if (uploadedBytes < totalBytes) {
                sendNextChunk();
            } else {
                upgradeState = UpgradeState.FINISHING;
                appendLog("[FINISH] 发送完成");
                byte[] szBuf = new byte[4];
                szBuf[0] = (byte) ((totalBytes >> 24) & 0xFF);
                szBuf[1] = (byte) ((totalBytes >> 16) & 0xFF);
                szBuf[2] = (byte) ((totalBytes >> 8) & 0xFF);
                szBuf[3] = (byte) (totalBytes & 0xFF);
                byte[] finishPkt = buildPacket(CMD_FINISH, szBuf);
                bluetoothHelper.writeCharacteristic("ab01", finishPkt, success -> {
                    if (!success) appendLog("[ERR] 发送FINISH失败");
                });
            }
            break;

        case FINISHING:
            appendLog("[FINISH] 固件已保存至B分区");
            upgradeState = UpgradeState.JUMPING;
            appendLog("[JUMP] 请求设备重启...");
            byte[] jumpPkt = buildPacket(CMD_JUMP, null);
            bluetoothHelper.writeCharacteristic("ab01", jumpPkt, success -> {
                if (!success) appendLog("[ERR] 发送JUMP失败");
            });
            break;

        case JUMPING:
            appendLog("[SUCCESS] ✓✓✓ 升级完成! 设备已重启到B分区");
            upgradeState = UpgradeState.IDLE;
            break;

        case QUERYING:
            if (dataLen >= 12) {
                byte protVer = data[6];
                byte bootMode = data[7];
                byte activePart = data[8];
                byte pendingUpg = data[9];
                byte failCnt = data[10];
                int fwSizeB = ((data[11] & 0xFF) << 24) | ((data[12] & 0xFF) << 16) |
                              ((data[13] & 0xFF) << 8) | (data[14] & 0xFF);
                tvProtocolVer.setText("v" + protVer);
                tvActivePart.setText(activePart == 0 ? "A(工厂)" : "B(用户)");
                tvFwSize.setText(String.format("%,d 字节", fwSizeB));
                appendLog(String.format("[QUERY] 协议v%d, 模式=%d, 活跃分区=%s, 待升级=%d, 失败计数=%d, B区大小=%,d",
                        protVer, bootMode, activePart == 0 ? "A" : "B", pendingUpg, failCnt, fwSizeB));
            }
            break;

        default:
            break;
        }
    }

    private void handleNack(int seq, byte err) {
        String errName = getErrorName(err);
        appendLog("[NACK] 错误: " + errName + " (0x" + String.format("%02X", err & 0xFF) + ")");
        upgradeState = UpgradeState.IDLE;
    }

    private String getErrorName(byte code) {
        switch (code) {
        case 0x01: return "CRC错误";
        case 0x02: return "Flash错误";
        case 0x03: return "大小溢出";
        case 0x04: return "状态错误";
        case 0x05: return "参数错误";
        default: return "未知错误";
        }
    }

    private void sendNextChunk() {
        if (uploadedBytes >= totalBytes) return;

        int chunkSize = Math.min(CHUNK_SIZE, totalBytes - uploadedBytes);
        byte[] payload = new byte[4 + chunkSize];

        // 4字节大端序偏移 + 数据
        payload[0] = (byte) ((uploadedBytes >> 24) & 0xFF);
        payload[1] = (byte) ((uploadedBytes >> 16) & 0xFF);
        payload[2] = (byte) ((uploadedBytes >> 8) & 0xFF);
        payload[3] = (byte) (uploadedBytes & 0xFF);
        System.arraycopy(firmwareData, uploadedBytes, payload, 4, chunkSize);

        uploadedBytes += chunkSize;

        byte[] pkt = buildPacket(CMD_DATA, payload);
        bluetoothHelper.writeCharacteristic("ab01", pkt, success -> {
            if (!success) {
                appendLog("[ERR] 发送DATA包失败 @ offset=0x" + String.format("%X", uploadedBytes));
            }
        });
    }

    private void updateProgress() {
        if (totalBytes > 0) {
            int pct = (int) (100.0 * uploadedBytes / totalBytes);
            progressBar.setProgress(pct);
            tvProgressPct.setText(pct + "%");
            tvProgressText.setText(String.format("已上传: %,d / %,d 字节", uploadedBytes, totalBytes));
        }
    }

    // ======================== 日志 ========================
    private void appendLog(String msg) {
        mainHandler.post(() -> {
            String currentText = tvLog.getText().toString();
            String newText = (currentText.isEmpty() ? "" : currentText + "\n") + msg;
            tvLog.setText(newText);
            // 自动滚动到底部
            scrollLog.post(() -> scrollLog.fullScroll(View.FOCUS_DOWN));
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (bluetoothHelper != null) {
            bluetoothHelper.setRawDataListener(null);
        }
    }
}

/**
 * BluetoothHelper 单例管理器
 */
class BluetoothHelperSingleton {
    private static BluetoothHelper instance;

    public static BluetoothHelper getInstance() {
        if (instance == null) {
            // 应该由主应用程序初始化实例
            // 这里作为备选方案
            Log.w("BluetoothHelper", "getInstance called but instance is null");
        }
        return instance;
    }

    public static void setInstance(BluetoothHelper helper) {
        instance = helper;
    }
}
