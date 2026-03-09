package com.example.myapplication;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.MenuItem;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Looper Control Activity
 * Maintains state machine on App side, sends -r/-p/-t commands based on segment state.
 *
 * State flow: INACTIVE -> (r) -> RECORDING -> (p) -> PLAYING -> (t) -> STOPPED -> (p) -> PLAYING...
 */
public class LooperControlActivity extends AppCompatActivity {

    private static final String BLE_UUID = "0000ab01-0000-1000-8000-00805f9b34fb";
    private static final int SEG_COUNT = 4;

    // Segment state enum
    private enum SegState { INACTIVE, RECORDING, PLAYING, STOPPED }

    private BluetoothHelper bluetoothHelper;
    private final Handler handler = new Handler(Looper.getMainLooper());

    // App-side state tracking
    private final SegState[] segStates = {
        SegState.INACTIVE, SegState.INACTIVE, SegState.INACTIVE, SegState.INACTIVE
    };

    // Color constants
    private static final int COLOR_INACTIVE  = Color.parseColor("#00D9FF");   // Cyan
    private static final int COLOR_RECORDING = Color.parseColor("#FF4444");   // Red
    private static final int COLOR_PLAYING   = Color.parseColor("#00FFA3");   // Green
    private static final int COLOR_STOPPED   = Color.parseColor("#888888");   // Gray

    private static final int TINT_INACTIVE  = Color.parseColor("#1A3040");
    private static final int TINT_RECORDING = Color.parseColor("#3D1010");
    private static final int TINT_PLAYING   = Color.parseColor("#0D3020");
    private static final int TINT_STOPPED   = Color.parseColor("#222222");

    // UI views - segment cards
    private LinearLayout[] segCards  = new LinearLayout[SEG_COUNT];
    private TextView[]     tvSegName  = new TextView[SEG_COUNT];
    private TextView[]     tvSegState = new TextView[SEG_COUNT];
    private TextView[]     tvSegHint  = new TextView[SEG_COUNT];

    // UI views - other
    private android.widget.Button btnLooperClear;
    private android.widget.Button btnLooperErase;
    private android.widget.Button btnLooperReset;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_looper_control);

        bluetoothHelper = BluetoothManager.getInstance().getBluetoothHelper();

        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        initViews();
        setupListeners();
        setupBleListener();
    }

    @Override
    protected void onResume() {
        super.onResume();
        setupBleListener();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void initViews() {
        int[] cardIds = {
            R.id.card_seg0, R.id.card_seg1, R.id.card_seg2, R.id.card_seg3
        };
        int[][] subIds = {
            {R.id.tv_seg0_name, R.id.tv_seg0_state, R.id.tv_seg0_hint},
            {R.id.tv_seg1_name, R.id.tv_seg1_state, R.id.tv_seg1_hint},
            {R.id.tv_seg2_name, R.id.tv_seg2_state, R.id.tv_seg2_hint},
            {R.id.tv_seg3_name, R.id.tv_seg3_state, R.id.tv_seg3_hint},
        };
        for (int i = 0; i < SEG_COUNT; i++) {
            segCards[i]   = findViewById(cardIds[i]);
            tvSegName[i]  = findViewById(subIds[i][0]);
            tvSegState[i] = findViewById(subIds[i][1]);
            tvSegHint[i]  = findViewById(subIds[i][2]);
        }
        btnLooperClear  = findViewById(R.id.btn_looper_clear);
        btnLooperErase  = findViewById(R.id.btn_looper_erase);
        btnLooperReset  = findViewById(R.id.btn_looper_reset);

        for (int i = 0; i < SEG_COUNT; i++) refreshSegUI(i);
    }

    private void setupListeners() {
        for (int i = 0; i < SEG_COUNT; i++) {
            final int idx = i;
            segCards[i].setOnClickListener(v -> onSegmentCardClick(idx));
        }

        btnLooperClear.setOnClickListener(v -> {
            sendCommand("looper -c", "Sent: Clear all segments");
            for (int i = 0; i < SEG_COUNT; i++) { segStates[i] = SegState.INACTIVE; refreshSegUI(i); }
        });

        btnLooperErase.setOnClickListener(v -> showConfirmDialog(
            "Erase Flash",
            "This will erase all recorded data (~20s). Continue?",
            () -> {
                sendCommand("looper -e", "Sent: Erase chip");
                for (int i = 0; i < SEG_COUNT; i++) { segStates[i] = SegState.INACTIVE; refreshSegUI(i); }
            }
        ));

        btnLooperReset.setOnClickListener(v -> showConfirmDialog(
            "Looper Reset",
            "This will reset all states and erase Flash. Continue?",
            () -> {
                sendCommand("looper -R", "Sent: Reset");
                for (int i = 0; i < SEG_COUNT; i++) { segStates[i] = SegState.INACTIVE; refreshSegUI(i); }
            }
        ));
    }

    private void onSegmentCardClick(int idx) {
        if (!checkConnection()) return;

        SegState current = segStates[idx];
        final String cmd;
        final SegState nextState;

        switch (current) {
            case INACTIVE:
                cmd = "looper -r " + idx;
                nextState = SegState.RECORDING;
                break;
            case RECORDING:
                cmd = "looper -p " + idx;
                nextState = SegState.PLAYING;
                break;
            case PLAYING:
                cmd = "looper -t " + idx;
                nextState = SegState.STOPPED;
                break;
            case STOPPED:
            default:
                cmd = "looper -p " + idx;
                nextState = SegState.PLAYING;
                break;
        }

        sendCommandWithCallback(cmd, success -> {
            if (success) {
                segStates[idx] = nextState;
                refreshSegUI(idx);
            }
        });
    }

    private void refreshSegUI(int idx) {
        SegState state = segStates[idx];
        String stateLabel, hintLabel;
        int textColor, bgTint;

        switch (state) {
            case RECORDING:
                stateLabel = "REC";
                hintLabel  = "Tap to stop & play";
                textColor  = COLOR_RECORDING;
                bgTint     = TINT_RECORDING;
                break;
            case PLAYING:
                stateLabel = "PLAY";
                hintLabel  = "Tap to pause";
                textColor  = COLOR_PLAYING;
                bgTint     = TINT_PLAYING;
                break;
            case STOPPED:
                stateLabel = "PAUSED";
                hintLabel  = "Tap to resume";
                textColor  = COLOR_STOPPED;
                bgTint     = TINT_STOPPED;
                break;
            default:
                stateLabel = "READY";
                hintLabel  = "Tap to record";
                textColor  = COLOR_INACTIVE;
                bgTint     = TINT_INACTIVE;
                break;
        }

        tvSegName[idx].setText("LOOP " + (idx + 1));
        tvSegName[idx].setTextColor(textColor);
        tvSegState[idx].setText(stateLabel);
        tvSegState[idx].setTextColor(textColor);
        tvSegHint[idx].setText(hintLabel);
        segCards[idx].setBackgroundTintList(ColorStateList.valueOf(bgTint));
    }

    private void setupBleListener() {
        final StringBuilder accum = new StringBuilder();
        bluetoothHelper.setBleNotifyListener(data -> {
            if (data == null || data.isEmpty()) return;
            runOnUiThread(() -> {
                accum.append(data);
                String buf = accum.toString();
                if (buf.contains("Seg[3]:")) {
                    parseLooperStatus(buf);
                    accum.setLength(0);
                } else if (buf.contains("\r\n") &&
                        (buf.contains("RECORDING") || buf.contains("PLAYING")
                         || buf.contains("STOPPED") || buf.contains("INACTIVE")
                         || buf.contains("Error:"))) {
                    accum.setLength(0);
                }
            });
        });
    }

    private void parseLooperStatus(String raw) {
        StringBuilder summary = new StringBuilder();
        for (String line : raw.split("\n")) {
            line = line.trim();
            if (line.startsWith("State:") || line.startsWith("ErasePend:")
                    || line.startsWith("Segments:")) {
                if (summary.length() > 0) summary.append("  ");
                summary.append(line);
            }
        }

        Pattern p = Pattern.compile("Seg\\[(\\d)\\]:\\s*(\\w+)");
        Matcher m = p.matcher(raw);
        while (m.find()) {
            int    segIdx   = Integer.parseInt(m.group(1));
            String stateStr = m.group(2).toUpperCase();
            if (segIdx < 0 || segIdx >= SEG_COUNT) continue;
            SegState parsed;
            switch (stateStr) {
                case "RECORDING": parsed = SegState.RECORDING; break;
                case "PLAYING":   parsed = SegState.PLAYING;   break;
                case "STOPPED":   parsed = SegState.STOPPED;   break;
                default:          parsed = SegState.INACTIVE;  break;
            }
            segStates[segIdx] = parsed;
            refreshSegUI(segIdx);
        }
        // Note: Removed auto-sync after command (looper -s), rely on parseLooperStatus only
    }

    private boolean checkConnection() {
        if (!bluetoothHelper.isConnected()) {
            Toast.makeText(this, "Connect Bluetooth first", Toast.LENGTH_SHORT).show();
            return false;
        }
        return true;
    }

    private void sendCommand(String cmd, String successMsg) {
        if (!bluetoothHelper.isConnected()) return;
        bluetoothHelper.writeCharacteristic(BLE_UUID, (cmd + "\r\n").getBytes(), success -> {
            if (success && successMsg != null)
                runOnUiThread(() -> Toast.makeText(this, successMsg, Toast.LENGTH_SHORT).show());
            else if (!success)
                runOnUiThread(() -> Toast.makeText(this, "Send failed", Toast.LENGTH_SHORT).show());
        });
    }

    private void sendCommandWithCallback(String cmd, SuccessCallback cb) {
        if (!checkConnection()) return;
        bluetoothHelper.writeCharacteristic(BLE_UUID, (cmd + "\r\n").getBytes(), success -> {
            if (!success)
                runOnUiThread(() -> Toast.makeText(this, "Send failed", Toast.LENGTH_SHORT).show());
            if (cb != null) runOnUiThread(() -> cb.onResult(success));
        });
    }

    private void showConfirmDialog(String title, String message, Runnable onConfirm) {
        new AlertDialog.Builder(this)
                .setTitle(title).setMessage(message)
                .setPositiveButton("OK", (d, w) -> onConfirm.run())
                .setNegativeButton("Cancel", null).show();
    }

    interface SuccessCallback { void onResult(boolean success); }
}
