package com.example.myapplication;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import com.google.gson.Gson;
import com.google.gson.annotations.SerializedName;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.ByteBuffer;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.CRC32;

public class FirmwareUpgradeActivity extends Activity {

    private static final String TAG = "FW_UPG";

    private static final int SOF_H = 0x42;
    private static final int SOF_L = 0x47;
    private static final int CMD_SYNC       = 0x01;
    private static final int CMD_START      = 0x02;
    private static final int CMD_DATA       = 0x03;
    private static final int CMD_FINISH     = 0x04;
    private static final int CMD_REBOOT     = 0x05;
    private static final int CMD_QUERY_INFO = 0x06;
    private static final int CMD_ABORT      = 0x07;
    private static final int RSP_ACK        = 0xA1;
    private static final int RSP_NACK       = 0xA2;

    private static final int CHUNK_SIZE = 256;
    private static final int MAX_RETRY  = 3;
    private static final long DATA_DELAY_MS = 10;
    private static final int BLE_MTU = 200;

    /* boot_mode values (must match dual_partition.h) */
    private static final int BOOT_MODE_SINGLE  = 0;
    private static final int BOOT_MODE_DUAL_AB = 1;

    private static final String BEMFA_API =
        "https://apis.bemfa.com/vb/api/v1/firmwareVersion";
    private static final String BEMFA_OPENID = "4d9ec352e0376f2110a0c601a2857225";
    private static final String BEMFA_TOPIC   = "AM8kJZZgX002";
    private static final int    BEMFA_DEVTYPE = 3;

    private TextView tvBleStatus, tvProtocolVer, tvAppSize;
    private TextView tvBleStatusSimple;
    private Button btnQueryInfo, btnCheckUpdate, btnSelectFile, btnSelectFileSimple;
    private Button btnStartUpgrade, btnReboot;
    private ImageButton btnOtaSettings;
    private TextView tvFileName, tvFileSize, tvCloudInfo;
    private ProgressBar progressBar;
    private TextView tvProgressText, tvProgressPct;
    private TextView tvLog;
    private ScrollView scrollLog;

    // Cards / sections for mode switching
    private View cardDeviceInfo;
    private View cardSimpleStatus;
    private View cardLog;
    private View rowFwSourceDual;

    // OTA 设置侧边栏
    private View drawerOtaSettings;

    // Current device mode (default: single-partition for simplicity)
    private int currentBootMode = BOOT_MODE_SINGLE;

    private BluetoothHelper bleHelper;
    private Handler uiHandler = new Handler(Looper.getMainLooper());
    private ExecutorService executor = Executors.newSingleThreadExecutor();

    private byte[] firmware;
    private int seq = 0;
    private volatile boolean upgrading = false;
    private BemfaFirmwareInfo cloudFwInfo;

    private final Object ackLock = new Object();
    private volatile byte[] ackData;
    private volatile boolean ackReceived;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_firmware_upgrade);

        tvBleStatus    = findViewById(R.id.tv_ble_status);
        tvBleStatusSimple = findViewById(R.id.tv_ble_status_simple);
        tvProtocolVer  = findViewById(R.id.tv_protocol_ver);
        tvAppSize      = findViewById(R.id.tv_app_size);
        btnQueryInfo   = findViewById(R.id.btn_query_info);
        btnCheckUpdate = findViewById(R.id.btn_check_update);
        btnSelectFile  = findViewById(R.id.btn_select_file);
        btnSelectFileSimple = findViewById(R.id.btn_select_file_simple);
        tvFileName     = findViewById(R.id.tv_file_name);
        tvFileSize     = findViewById(R.id.tv_file_size);
        tvCloudInfo    = findViewById(R.id.tv_cloud_info);
        progressBar    = findViewById(R.id.progress_bar);
        tvProgressText = findViewById(R.id.tv_progress_text);
        tvProgressPct  = findViewById(R.id.tv_progress_pct);
        tvLog          = findViewById(R.id.tv_log);
        scrollLog      = findViewById(R.id.scroll_log);
        btnStartUpgrade = findViewById(R.id.btn_start_upgrade);
        btnReboot      = findViewById(R.id.btn_reboot);
        btnOtaSettings = findViewById(R.id.btn_ota_settings);

        // Section cards
        cardDeviceInfo   = findViewById(R.id.card_device_info);
        cardSimpleStatus = findViewById(R.id.card_simple_status);
        cardLog          = findViewById(R.id.card_log);
        rowFwSourceDual  = findViewById(R.id.row_fw_source_dual);

        bleHelper = BluetoothManager.getInstance().getBluetoothHelper();

        updateBleStatus();

        btnQueryInfo.setOnClickListener(v -> executor.execute(this::doQueryInfo));
        btnCheckUpdate.setOnClickListener(v -> executor.execute(this::doCheckUpdate));
        btnSelectFile.setOnClickListener(v -> openFilePicker());
        btnSelectFileSimple.setOnClickListener(v -> openFilePicker());
        btnStartUpgrade.setOnClickListener(v -> {
            if (!upgrading) executor.execute(this::doUpgrade);
        });
        btnReboot.setOnClickListener(v -> executor.execute(this::doReboot));

        // OTA 设置侧边栏
        drawerOtaSettings = findViewById(R.id.drawer_ota_settings);
        ImageButton btnCloseOtaDrawer = findViewById(R.id.btn_close_ota_drawer);
        btnOtaSettings.setOnClickListener(v -> openOtaDrawer());
        btnCloseOtaDrawer.setOnClickListener(v -> closeOtaDrawer());

        bleHelper.setRawDataListener(data -> {
            synchronized (ackLock) {
                ackData = data;
                ackReceived = true;
                ackLock.notifyAll();
            }
        });

        /* Auto-query device info on entry to detect partition mode */
        if (bleHelper.isConnected()) {
            executor.execute(this::doQueryInfo);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        executor.shutdownNow();
        bleHelper.setRawDataListener(null);
    }

    private void openOtaDrawer() {
        drawerOtaSettings.setVisibility(View.VISIBLE);
    }

    private void closeOtaDrawer() {
        drawerOtaSettings.setVisibility(View.GONE);
    }

    private void updateBleStatus() {
        boolean connected = bleHelper.isConnected();
        String statusText = connected ? "已连接" : "未连接";
        int statusColor = getColor(connected ? R.color.primary_accent : R.color.text_error);

        tvBleStatus.setText(statusText);
        tvBleStatus.setTextColor(statusColor);
        tvBleStatusSimple.setText(statusText);
        tvBleStatusSimple.setTextColor(statusColor);

        btnStartUpgrade.setEnabled(connected && firmware != null);
        if (currentBootMode == BOOT_MODE_DUAL_AB) {
            btnReboot.setEnabled(connected);
        }
        btnQueryInfo.setEnabled(connected);
    }

    /**
     * Apply UI mode based on boot_mode reported by the device.
     * Single-partition (2 MB): minimal UI — select file + upgrade only.
     * Dual-partition (8 MB): full UI with device info, cloud update, log, reboot.
     */
    private void applyUIMode(int bootMode) {
        currentBootMode = bootMode;
        boolean isDual = (bootMode == BOOT_MODE_DUAL_AB);

        uiHandler.post(() -> {
            // Device info card
            cardDeviceInfo.setVisibility(isDual ? View.VISIBLE : View.GONE);
            // Simple status bar (single-partition)
            cardSimpleStatus.setVisibility(isDual ? View.GONE : View.VISIBLE);
            // Cloud + local file row (dual)
            rowFwSourceDual.setVisibility(isDual ? View.VISIBLE : View.GONE);
            // Simple select file button (single)
            btnSelectFileSimple.setVisibility(isDual ? View.GONE : View.VISIBLE);
            // Log card
            cardLog.setVisibility(isDual ? View.VISIBLE : View.GONE);
            // Reboot button
            btnReboot.setVisibility(isDual ? View.VISIBLE : View.GONE);
            // Settings button
            btnOtaSettings.setVisibility(isDual ? View.VISIBLE : View.GONE);
        });
    }

    private void log(String msg) {
        uiHandler.post(() -> {
            tvLog.append(msg + "\n");
            scrollLog.post(() -> scrollLog.fullScroll(ScrollView.FOCUS_DOWN));
        });
    }

    private void setProgress(int pct, String text) {
        uiHandler.post(() -> {
            progressBar.setProgress(pct);
            tvProgressPct.setText(pct + "%");
            tvProgressText.setText(text);
        });
    }

    // ── BLE protocol helpers ───────────────────────────────────────────────

    private int nextSeq() {
        int s = seq;
        seq = (seq + 1) & 0xFF;
        return s;
    }

    private static int crc16Ccitt(byte[] buf, int off, int len) {
        int crc = 0xFFFF;
        for (int i = off; i < off + len; i++) {
            crc ^= (buf[i] & 0xFF) << 8;
            for (int j = 0; j < 8; j++) {
                crc = (crc & 0x8000) != 0 ? ((crc << 1) ^ 0x1021) & 0xFFFF
                                           : (crc << 1) & 0xFFFF;
            }
        }
        return crc;
    }

    private byte[] buildPacket(int cmd, int seq, byte[] data) {
        int dlen = (data != null) ? data.length : 0;
        byte[] pkt = new byte[2 + 1 + 1 + 2 + dlen + 2];
        int n = 0;
        pkt[n++] = (byte) SOF_H;
        pkt[n++] = (byte) SOF_L;
        pkt[n++] = (byte) cmd;
        pkt[n++] = (byte) seq;
        pkt[n++] = (byte) (dlen >> 8);
        pkt[n++] = (byte) dlen;
        if (dlen > 0) System.arraycopy(data, 0, pkt, n, dlen);
        n += dlen;
        int crc = crc16Ccitt(pkt, 2, 1 + 1 + 2 + dlen);
        pkt[n++] = (byte) (crc >> 8);
        pkt[n++] = (byte) crc;
        return pkt;
    }

    private byte[] transact(int cmd, byte[] data, long timeoutMs) throws Exception {
        for (int attempt = 1; attempt <= MAX_RETRY; attempt++) {
            int s = nextSeq();
            byte[] pkt = buildPacket(cmd, s, data);
            sendBlePackets(pkt);

            synchronized (ackLock) {
                ackReceived = false;
                ackData = null;
                long deadline = System.currentTimeMillis() + timeoutMs;
                while (!ackReceived && System.currentTimeMillis() < deadline) {
                    ackLock.wait(Math.max(1, deadline - System.currentTimeMillis()));
                }
                if (!ackReceived) {
                    if (attempt < MAX_RETRY) {
                        log("  [重试 " + (attempt + 1) + "/" + MAX_RETRY + "]");
                        continue;
                    }
                    throw new Exception("超时");
                }
            }

            if (ackData == null || ackData.length < 2) continue;

            int rspCmd = ackData[2] & 0xFF;
            if (rspCmd == RSP_NACK) {
                int err = (ackData.length > 6) ? ackData[6] & 0xFF : 0xFF;
                throw new Exception("NACK error=0x" + Integer.toHexString(err));
            }
            if (rspCmd == RSP_ACK) {
                int rspDlen = ((ackData[4] & 0xFF) << 8) | (ackData[5] & 0xFF);
                byte[] payload = new byte[rspDlen];
                if (rspDlen > 0)
                    System.arraycopy(ackData, 6, payload, 0, rspDlen);
                return payload;
            }
        }
        throw new Exception("No valid response");
    }

    private void sendBlePackets(byte[] data) {
        int offset = 0;
        while (offset < data.length) {
            int chunkLen = Math.min(BLE_MTU, data.length - offset);
            byte[] chunk = new byte[chunkLen];
            System.arraycopy(data, offset, chunk, 0, chunkLen);

            final byte[] toWrite = chunk;
            final Object lock = new Object();
            final boolean[] done = {false};

            uiHandler.post(() -> {
                bleHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb",
                    toWrite, success -> {
                        synchronized (lock) {
                            done[0] = true;
                            lock.notifyAll();
                        }
                    });
            });

            synchronized (lock) {
                try {
                    while (!done[0]) lock.wait(2000);
                } catch (InterruptedException e) {
                    break;
                }
            }

            offset += chunkLen;
        }
    }

    // ── High-level operations ──────────────────────────────────────────────

    private void doQueryInfo() {
        try {
            log("正在查询设备信息...");
            byte[] resp = transact(CMD_QUERY_INFO, null, 5000);
            if (resp.length >= 4) {
                int bootMode   = resp[0] & 0xFF;
                int activePart = resp[1] & 0xFF;
                int failCnt    = resp[2] & 0xFF;
                int ver        = resp[3] & 0xFF;

                long partASize = 0, partBSize = 0;
                if (resp.length >= 20) {
                    long partABase = ((long)(resp[4] & 0xFF) << 24)
                                   | ((long)(resp[5] & 0xFF) << 16)
                                   | ((long)(resp[6] & 0xFF) << 8)
                                   |  (resp[7] & 0xFF);
                    partASize = ((long)(resp[8] & 0xFF) << 24)
                              | ((long)(resp[9] & 0xFF) << 16)
                              | ((long)(resp[10] & 0xFF) << 8)
                              |  (resp[11] & 0xFF);
                    long partBBase = ((long)(resp[12] & 0xFF) << 24)
                                   | ((long)(resp[13] & 0xFF) << 16)
                                   | ((long)(resp[14] & 0xFF) << 8)
                                   |  (resp[15] & 0xFF);
                    partBSize = ((long)(resp[16] & 0xFF) << 24)
                              | ((long)(resp[17] & 0xFF) << 16)
                              | ((long)(resp[18] & 0xFF) << 8)
                              |  (resp[19] & 0xFF);
                }

                /* Apply UI mode based on boot_mode from device */
                applyUIMode(bootMode);

                String modeStr = (bootMode == BOOT_MODE_DUAL_AB) ? "双分区A/B" : "单分区";
                uiHandler.post(() -> {
                    tvProtocolVer.setText("v" + ver);
                    tvAppSize.setText((partASize / 1024) + " KB");
                });
                log("模式: " + modeStr + "  协议: v" + ver
                    + "  A容量: " + (partASize / 1024) + " KB"
                    + (bootMode == BOOT_MODE_DUAL_AB ? "  B容量: " + (partBSize / 1024) + " KB" : ""));
            }
        } catch (Exception e) {
            log("查询失败: " + e.getMessage());
        }
    }

    private void doSync() throws Exception {
        byte[] resp = transact(CMD_SYNC, null, 5000);
        int ver = (resp.length > 0) ? resp[0] & 0xFF : 0;
        log("握手成功，协议版本 v" + ver);
    }

    private void doUpgrade() {
        if (firmware == null) {
            log("请先选择固件文件");
            return;
        }
        upgrading = true;
        uiHandler.post(() -> btnStartUpgrade.setEnabled(false));
        try {
            doSync();

            int total = firmware.length;
            CRC32 crc32 = new CRC32();
            crc32.update(firmware);
            int fwCrc = (int) crc32.getValue();

            log("固件大小: " + total + " 字节 (" +
                String.format("%.1f", total / 1024.0) + " KB)");

            log("正在擦除APP分区...");
            byte[] sizeData = ByteBuffer.allocate(4).putInt(total).array();
            transact(CMD_START, sizeData, 60000);
            log("Flash 已擦除，开始传输固件数据...");

            int offset = 0;
            while (offset < total) {
                int chunkLen = Math.min(CHUNK_SIZE, total - offset);
                byte[] payload = new byte[4 + chunkLen];
                ByteBuffer.wrap(payload, 0, 4).putInt(offset);
                System.arraycopy(firmware, offset, payload, 4, chunkLen);

                transact(CMD_DATA, payload, 5000);
                offset += chunkLen;

                int pct = offset * 100 / total;
                setProgress(pct, "传输中 " + offset + "/" + total);

                if (DATA_DELAY_MS > 0)
                    Thread.sleep(DATA_DELAY_MS);
            }

            log("升级完成，正在校验...");
            byte[] crcData = ByteBuffer.allocate(4).putInt(fwCrc).array();
            transact(CMD_FINISH, crcData, 5000);
            log("校验通过！");

            setProgress(100, "升级完成");
            uiHandler.post(() -> btnReboot.setEnabled(true));

        } catch (Exception e) {
            log("升级失败: " + e.getMessage());
            setProgress(0, "升级失败");
        } finally {
            upgrading = false;
            uiHandler.post(() -> {
                btnStartUpgrade.setEnabled(firmware != null && bleHelper.isConnected());
            });
        }
    }

    private void doReboot() {
        try {
            log("正在重启设备...");
            transact(CMD_REBOOT, null, 3000);
            log("设备已重启");
        } catch (Exception e) {
            log("重启命令已发送 (设备可能已断开)");
        }
    }

    // ── Bemfa cloud API ────────────────────────────────────────────────────

    private static class BemfaResponse {
        int code;
        String msg;
        BemfaFirmwareInfo data;
    }

    private static class BemfaFirmwareInfo {
        String url;
        int version;
        String tag;
        int size;
        long unix;
    }

    private void doCheckUpdate() {
        try {
            log("正在检查云端更新...");
            String apiUrl = BEMFA_API + "?openID=" + BEMFA_OPENID
                + "&topic=" + BEMFA_TOPIC
                + "&deviceType=" + BEMFA_DEVTYPE;

            URL url = new URL(apiUrl);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("GET");
            conn.setConnectTimeout(10000);
            conn.setReadTimeout(10000);

            int respCode = conn.getResponseCode();
            if (respCode != 200) {
                log("API请求失败: HTTP " + respCode);
                return;
            }

            InputStream is = conn.getInputStream();
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            int n;
            while ((n = is.read(buf)) != -1) baos.write(buf, 0, n);
            is.close();

            String json = baos.toString("UTF-8");
            Log.d(TAG, "Bemfa response: " + json);

            Gson gson = new Gson();
            BemfaResponse resp = gson.fromJson(json, BemfaResponse.class);

            if (resp.code != 0 || resp.data == null) {
                log("云端无可用更新: " + resp.msg);
                return;
            }

            cloudFwInfo = resp.data;
            log("发现新固件: v" + cloudFwInfo.version
                + "  大小: " + cloudFwInfo.size + " 字节"
                + (cloudFwInfo.tag != null ? "  备注: " + cloudFwInfo.tag : ""));

            uiHandler.post(() -> {
                tvCloudInfo.setText("云端固件 v" + cloudFwInfo.version
                    + " (" + cloudFwInfo.size + " 字节)"
                    + (cloudFwInfo.tag != null ? " — " + cloudFwInfo.tag : ""));
                tvCloudInfo.setVisibility(TextView.VISIBLE);
            });

            log("正在下载固件...");
            downloadFirmware(cloudFwInfo.url);

        } catch (Exception e) {
            log("检查更新失败: " + e.getMessage());
        }
    }

    private void downloadFirmware(String downloadUrl) throws Exception {
        URL url = new URL(downloadUrl.trim());
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setConnectTimeout(15000);
        conn.setReadTimeout(60000);

        int respCode = conn.getResponseCode();
        if (respCode != 200) {
            log("下载失败: HTTP " + respCode);
            return;
        }

        int contentLen = conn.getContentLength();
        InputStream is = conn.getInputStream();
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        int totalRead = 0;
        int n;
        while ((n = is.read(buf)) != -1) {
            baos.write(buf, 0, n);
            totalRead += n;
            if (contentLen > 0) {
                int pct = totalRead * 100 / contentLen;
                setProgress(pct, "下载中 " + totalRead + "/" + contentLen);
            }
        }
        is.close();

        firmware = baos.toByteArray();
        log("固件下载完成: " + firmware.length + " 字节");

        File fwFile = new File(getCacheDir(), "firmware.bin");
        FileOutputStream fos = new FileOutputStream(fwFile);
        fos.write(firmware);
        fos.close();

        uiHandler.post(() -> {
            tvFileName.setText(fwFile.getName());
            tvFileSize.setText(String.format("%.1f KB", firmware.length / 1024.0));
            btnStartUpgrade.setEnabled(bleHelper.isConnected());
            btnSelectFileSimple.setText(fwFile.getName());
        });
        setProgress(0, "下载完成，等待升级");
    }

    // ── Local file picker ──────────────────────────────────────────────────

    private static final int PICK_BIN_FILE = 1001;

    private void openFilePicker() {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType("*/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(Intent.createChooser(intent, "选择固件文件"),
                               PICK_BIN_FILE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PICK_BIN_FILE && resultCode == RESULT_OK && data != null) {
            Uri uri = data.getData();
            try {
                InputStream is = getContentResolver().openInputStream(uri);
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                int n;
                while ((n = is.read(buf)) != -1) baos.write(buf, 0, n);
                is.close();
                firmware = baos.toByteArray();

                String name = uri.getLastPathSegment();
                uiHandler.post(() -> {
                    tvFileName.setText(name);
                    tvFileSize.setText(String.format("%.1f KB",
                        firmware.length / 1024.0));
                    btnStartUpgrade.setEnabled(bleHelper.isConnected());
                    btnSelectFileSimple.setText(name);
                });
                log("已加载固件: " + name + " (" + firmware.length + " 字节)");
            } catch (Exception e) {
                log("读取文件失败: " + e.getMessage());
            }
        }
    }
}
