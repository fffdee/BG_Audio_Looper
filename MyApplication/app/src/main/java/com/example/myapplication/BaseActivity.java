package com.example.myapplication;

import android.os.Bundle;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
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

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        // 在 setContentView 之前应用主题
        ThemeManager.applyTheme(this);
        super.onCreate(savedInstanceState);
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
}
