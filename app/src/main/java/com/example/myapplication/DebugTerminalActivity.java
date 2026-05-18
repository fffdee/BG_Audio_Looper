package com.example.myapplication;

import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.HorizontalScrollView;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import java.util.ArrayList;
import java.util.List;

public class DebugTerminalActivity extends BaseActivity {
    private BluetoothHelper bleHelper;

    private TextView tvRxLog;
    private ScrollView scrollRx;
    private EditText etCommand;
    private Button btnSend;
    private ImageButton btnSaveCommand;
    private LinearLayout layoutQuickCommands;

    private List<String> quickCommands = new ArrayList<>();
    private static final String PREFS_NAME = "DebugTerminalPrefs";
    private static final String KEY_QUICK_COMMANDS = "quick_commands";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_debug_terminal);
        setupBaseToolbar(true);

        bleHelper = BluetoothManager.getInstance().getBluetoothHelper();

        initViews();
        loadQuickCommands();
        setupListeners();
        setupBleListener();
    }

    @Override
    protected String getToolbarTitle() {
        return "调试终端";
    }

    private void initViews() {
        tvRxLog = findViewById(R.id.tv_rx_log);
        scrollRx = findViewById(R.id.scroll_rx);
        etCommand = findViewById(R.id.et_command);
        btnSend = findViewById(R.id.btn_send);
        btnSaveCommand = findViewById(R.id.btn_save_command);
        layoutQuickCommands = findViewById(R.id.layout_quick_commands);
    }

    private void setupListeners() {
        btnSend.setOnClickListener(v -> sendCommand());

        etCommand.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_SEND) {
                sendCommand();
                return true;
            }
            return false;
        });

        btnSaveCommand.setOnClickListener(v -> showSaveCommandDialog());
    }

    private void setupBleListener() {
        bleHelper.setBleNotifyListener((data) -> {
            runOnUiThread(() -> appendRxLog("[RX] " + data));
        });
    }

    private void sendCommand() {
        String cmd = etCommand.getText().toString().trim();
        if (cmd.isEmpty()) return;

        String cmdWithCRLF = cmd + "\r\n";
        bleHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", cmdWithCRLF.getBytes(), success -> {
            runOnUiThread(() -> {
                if (success) {
                    appendRxLog("[TX] " + cmd);
                    etCommand.setText("");
                } else {
                    appendRxLog("[TX] 发送失败: " + cmd);
                    Toast.makeText(this, "发送失败", Toast.LENGTH_SHORT).show();
                }
            });
        });
    }

    private void appendRxLog(String msg) {
        String current = tvRxLog.getText().toString();
        if (!current.isEmpty()) {
            current += "\n";
        }
        tvRxLog.setText(current + msg);
        scrollRx.post(() -> scrollRx.fullScroll(View.FOCUS_DOWN));
    }

    private void loadQuickCommands() {
        String saved = getSharedPreferences(PREFS_NAME, MODE_PRIVATE).getString(KEY_QUICK_COMMANDS, "");
        if (!saved.isEmpty()) {
            String[] cmds = saved.split("│");
            for (String cmd : cmds) {
                if (!cmd.trim().isEmpty()) {
                    quickCommands.add(cmd.trim());
                }
            }
        }
        if (quickCommands.isEmpty()) {
            quickCommands.add("help");
            quickCommands.add("status");
            quickCommands.add("version");
        }
        rebuildQuickCommandButtons();
    }

    private void saveQuickCommands() {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < quickCommands.size(); i++) {
            if (i > 0) sb.append("│");
            sb.append(quickCommands.get(i));
        }
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit()
                .putString(KEY_QUICK_COMMANDS, sb.toString())
                .apply();
    }

    private void rebuildQuickCommandButtons() {
        layoutQuickCommands.removeAllViews();
        for (int i = 0; i < quickCommands.size(); i++) {
            final String cmd = quickCommands.get(i);
            Button btn = new Button(this);
            btn.setText(cmd);
            btn.setTextSize(11);
            btn.setPadding(16, 8, 16, 8);
            btn.setBackgroundResource(R.drawable.chip_max_rec);
            btn.setTextColor(getColor(R.color.text_primary));

            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT);
            params.rightMargin = 8;
            btn.setLayoutParams(params);

            final int cmdIndex = i;
            final String cmdText = cmd;

            btn.setOnClickListener(v -> {
                etCommand.setText(cmdText);
                etCommand.setSelection(cmdText.length());
                etCommand.requestFocus();
            });

            btn.setOnLongClickListener(v -> {
                showCommandOptionsDialog(cmdIndex, cmdText);
                return true;
            });

            layoutQuickCommands.addView(btn);
        }
    }

    private void showSaveCommandDialog() {
        EditText input = new EditText(this);
        input.setHint("输入命令");
        input.setText(etCommand.getText().toString());

        new AlertDialog.Builder(this)
                .setTitle("保存快捷命令")
                .setView(input)
                .setPositiveButton("保存", (dialog, which) -> {
                    String cmd = input.getText().toString().trim();
                    if (!cmd.isEmpty()) {
                        quickCommands.add(cmd);
                        saveQuickCommands();
                        rebuildQuickCommandButtons();
                        Toast.makeText(this, "已保存: " + cmd, Toast.LENGTH_SHORT).show();
                    }
                })
                .setNegativeButton("取消", null)
                .show();
    }

    private void showCommandOptionsDialog(final int index, final String cmd) {
        new AlertDialog.Builder(this)
                .setTitle("命令: " + cmd)
                .setItems(new String[]{"发送", "删除", "取消"}, (dialog, which) -> {
                    if (which == 0) {
                        etCommand.setText(cmd);
                        etCommand.setSelection(cmd.length());
                        sendCommand();
                    } else if (which == 1) {
                        quickCommands.remove(index);
                        saveQuickCommands();
                        rebuildQuickCommandButtons();
                        Toast.makeText(this, "已删除", Toast.LENGTH_SHORT).show();
                    }
                })
                .show();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (bleHelper != null) {
            bleHelper.setBleNotifyListener(null);
        }
    }
}
