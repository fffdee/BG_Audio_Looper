package com.example.myapplication;

import android.os.Bundle;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.lifecycle.Observer;

/**
 * 所有 Activity 的基类
 * - 自动应用白天/黑夜主题
 * - 提供统一的 Toolbar 设置（含主题切换按钮 + 蓝牙状态指示）
 * - 通过 LiveData 自动同步蓝牙连接状态
 */
public class BaseActivity extends AppCompatActivity {

    protected Toolbar baseToolbar;
    private ImageButton btnThemeToggle;
    private View bleStatusDot;
    private TextView tvBleStatus;
    private TextView tvBattery;
    private AlertDialog syncingDialog;
    private AlertDialog transferDialog;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        // 在 setContentView 之前应用主题
        ThemeManager.applyTheme(this);
        super.onCreate(savedInstanceState);
        // 监听参数同步状态，连接时自动弹出/关闭同步提示
        observeSyncState();
        // 监听系统状态，数据传输时弹出警告
        observeSysState();
    }

    /**
     * 子类在 setContentView 之后调用，初始化统一 Toolbar
     * @param showBack 是否显示返回按钮
     */
    protected void setupBaseToolbar(boolean showBack) {
        baseToolbar = findViewById(R.id.base_toolbar);
        if (baseToolbar == null) return;

        setSupportActionBar(baseToolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayShowTitleEnabled(false);
        }

        // 返回按钮
        ImageButton btnBack = findViewById(R.id.btn_toolbar_back);
        if (btnBack != null) {
            if (showBack) {
                btnBack.setVisibility(View.VISIBLE);
                btnBack.setOnClickListener(v -> onBackPressed());
            } else {
                btnBack.setVisibility(View.GONE);
            }
        }

        // 标题
        TextView tvTitle = findViewById(R.id.tv_toolbar_title);
        if (tvTitle != null) {
            tvTitle.setText(getToolbarTitle());
        }

        // 主题切换按钮
        btnThemeToggle = findViewById(R.id.btn_theme_toggle);
        if (btnThemeToggle != null) {
            updateThemeIcon();
            btnThemeToggle.setOnClickListener(v -> {
                ThemeManager.toggleTheme(this);
                // AppCompatDelegate 会自动 recreate
            });
        }

        // 蓝牙状态指示
        bleStatusDot = findViewById(R.id.ble_status_dot);
        tvBleStatus = findViewById(R.id.tv_ble_status);
        observeBleState();

        // 电量显示
        tvBattery = findViewById(R.id.tv_battery_level);
        observeBatteryLevel();

        // 处理系统状态栏遮挡（Android 15+ 强制 edge-to-edge）
        ViewCompat.setOnApplyWindowInsetsListener(baseToolbar, (v, insets) -> {
            int statusBarH = insets.getInsets(WindowInsetsCompat.Type.statusBars()).top;
            v.setPadding(0, statusBarH, 0, 0);
            ViewGroup.LayoutParams lp = v.getLayoutParams();
            if (lp != null && statusBarH > 0) {
                TypedValue tv2 = new TypedValue();
                int barH;
                if (getTheme().resolveAttribute(android.R.attr.actionBarSize, tv2, true)) {
                    barH = TypedValue.complexToDimensionPixelSize(tv2.data, getResources().getDisplayMetrics());
                } else {
                    barH = (int)(56 * getResources().getDisplayMetrics().density);
                }
                lp.height = statusBarH + barH;
                v.setLayoutParams(lp);
            }
            return insets;
        });
    }

    /**
     * 子类重写此方法返回 Toolbar 标题
     */
    protected String getToolbarTitle() {
        return "";
    }

    /**
     * 设置 Toolbar 标题（运行时动态更新）
     */
    protected void setToolbarTitle(String title) {
        TextView tvTitle = findViewById(R.id.tv_toolbar_title);
        if (tvTitle != null) {
            tvTitle.setText(title);
        }
    }

    private void updateThemeIcon() {
        if (btnThemeToggle == null) return;
        boolean isNight = ThemeManager.isNightMode(this);
        // 夜间模式显示太阳图标（点击切换到白天），白天模式显示月亮图标
        btnThemeToggle.setImageResource(isNight ? R.drawable.ic_theme_sun : R.drawable.ic_theme_moon);
    }

    private void observeBleState() {
        BluetoothManager.getInstance().getConnectionState().observe(this, state -> {
            if (state == null) return;
            updateBleStatusUI(state);
        });
    }

    private void observeBatteryLevel() {
        BluetoothManager.getInstance().getBatteryLevel().observe(this, soc -> {
            if (tvBattery == null || soc == null) return;
            if (soc < 0) {
                tvBattery.setText("--");
            } else {
                tvBattery.setText(soc + "%");
            }
        });
    }

    private void updateBleStatusUI(BleConnectionState state) {
        if (bleStatusDot != null) {
            int color = state.isConnected() ? 0xFF4CAF50 : 0xFFFF4444;
            bleStatusDot.getBackground().setTint(color);
        }
        if (tvBleStatus != null) {
            if (state.isConnected()) {
                String name = state.getDeviceName();
                tvBleStatus.setText(name != null ? name : "已连接");
                tvBleStatus.setTextColor(0xFF4CAF50);
            } else {
                tvBleStatus.setText("未连接");
                tvBleStatus.setTextColor(0xFFFF4444);
            }
        }
    }

    // ── 参数同步弹窗 ──────────────────────────────────────────────────────────

    private void observeSyncState() {
        BluetoothManager.getInstance().getSyncingState().observe(this, isSyncing -> {
            if (Boolean.TRUE.equals(isSyncing)) {
                showSyncingDialog();
            } else {
                dismissSyncingDialog();
            }
        });
    }

    private void showSyncingDialog() {
        if (isFinishing() || isDestroyed()) return;
        if (syncingDialog != null && syncingDialog.isShowing()) return;

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.HORIZONTAL);
        layout.setPadding(60, 50, 60, 50);
        layout.setGravity(Gravity.CENTER_VERTICAL);

        ProgressBar pb = new ProgressBar(this);
        layout.addView(pb, new LinearLayout.LayoutParams(80, 80));

        TextView tv = new TextView(this);
        tv.setText("正在同步设备参数...");
        tv.setTextSize(16);
        tv.setPadding(30, 0, 0, 0);
        layout.addView(tv);

        syncingDialog = new AlertDialog.Builder(this)
                .setView(layout)
                .setCancelable(false)
                .create();
        syncingDialog.show();
    }

    private void dismissSyncingDialog() {
        if (syncingDialog != null && syncingDialog.isShowing()) {
            syncingDialog.dismiss();
        }
        syncingDialog = null;
    }

    // ── 数据传输态警告弹窗 ────────────────────────────────────────────────────

    private void observeSysState() {
        BluetoothManager.getInstance().getSysState().observe(this, state -> {
            if (state == null) return;
            if (state == BleProtocol.SYS_STATE_TRANSFER) {
                showTransferDialog();
            } else {
                dismissTransferDialog();
            }
        });
    }

    private void showTransferDialog() {
        if (isFinishing() || isDestroyed()) return;
        if (transferDialog != null && transferDialog.isShowing()) return;

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.HORIZONTAL);
        layout.setPadding(60, 50, 60, 50);
        layout.setGravity(Gravity.CENTER_VERTICAL);

        ProgressBar pb = new ProgressBar(this);
        layout.addView(pb, new LinearLayout.LayoutParams(80, 80));

        TextView tv = new TextView(this);
        tv.setText("正在传输数据，请勿进行其他操作...");
        tv.setTextSize(16);
        tv.setPadding(30, 0, 0, 0);
        layout.addView(tv);

        transferDialog = new AlertDialog.Builder(this)
                .setView(layout)
                .setCancelable(false)
                .create();
        transferDialog.show();
    }

    private void dismissTransferDialog() {
        if (transferDialog != null && transferDialog.isShowing()) {
            transferDialog.dismiss();
        }
        transferDialog = null;
    }
}
