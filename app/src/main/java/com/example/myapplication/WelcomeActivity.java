package com.example.myapplication;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.ScanResult;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import java.util.ArrayList;
import java.util.List;

public class WelcomeActivity extends BaseActivity {
    private BluetoothHelper bluetoothHelper;

    private static final int REQUEST_BLUETOOTH_PERMISSIONS = 1;
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner bluetoothLeScanner;
    private boolean isScanning = false;
    private Handler handler = new Handler(Looper.getMainLooper());
    private List<String> deviceList = new ArrayList<>();
    private ArrayAdapter<String> deviceAdapter;
    private AlertDialog scanDialog;

    private TextView tvConnectionStatus;
    private TextView tvDeviceName;
    private Button btnBluetoothConnect;
    private View statusIndicator;
    private View appSettingsOverlay;
    private View drawerAppSettings;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_welcome_mecha);
        setupBaseToolbar(false);

        // 获取全局BluetoothHelper实例
        bluetoothHelper = com.example.myapplication.BluetoothManager.getInstance().getBluetoothHelper();

        // 初始化蓝牙
        android.bluetooth.BluetoothManager bluetoothManager =
                (android.bluetooth.BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
        bluetoothAdapter = bluetoothManager.getAdapter();
        if (bluetoothAdapter != null) {
            bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();
        }

        // 初始化状态UI控件
        tvConnectionStatus = findViewById(R.id.tv_connection_status);
        tvDeviceName = findViewById(R.id.tv_device_name);
        btnBluetoothConnect = findViewById(R.id.btn_bluetooth_connect);
        statusIndicator = findViewById(R.id.status_indicator);

        // 初始化侧边栏
        appSettingsOverlay = findViewById(R.id.app_settings_overlay);
        drawerAppSettings = findViewById(R.id.drawer_app_settings);
        if (appSettingsOverlay != null) {
            appSettingsOverlay.setOnClickListener(v -> closeAppSettingsDrawer());
        }

        ImageButton enterButton = findViewById(R.id.btn_enter);
        enterButton.setOnClickListener(v -> {
            Intent intent = new Intent(WelcomeActivity.this, HomeActivity.class);
            startActivity(intent);
            finish();
        });

        ImageButton banboxSettingsButton = findViewById(R.id.btn_banbox_settings);
        banboxSettingsButton.setOnClickListener(v -> {
            if (!bluetoothHelper.isConnected()) {
                Toast.makeText(WelcomeActivity.this, "请先连接蓝牙设备", Toast.LENGTH_SHORT).show();
                return;
            }
            Intent intent = new Intent(WelcomeActivity.this, BanBoxSettingsActivity.class);
            startActivity(intent);
        });

        // 连接按钮点击事件
        btnBluetoothConnect.setOnClickListener(v -> {
            if (bluetoothHelper.isConnected()) {
                // 已连接则断开
                new AlertDialog.Builder(this)
                        .setTitle("断开连接")
                        .setMessage("确定要断开当前设备吗？")
                        .setPositiveButton("断开", (dialog, which) -> bluetoothHelper.disconnect())
                        .setNegativeButton("取消", null)
                        .show();
            } else {
                // 未连接则打开扫描弹窗
                showBluetoothScanDialog();
            }
        });

        // 监听 BLE 连接状态
        bluetoothHelper.setOnConnectionChangedListener(new BluetoothHelper.OnConnectionChangedListener() {
            @Override
            public void onConnected(String deviceName, android.bluetooth.BluetoothGatt gatt) {
                runOnUiThread(() -> {
                    Toast.makeText(WelcomeActivity.this, "连接成功: " + deviceName, Toast.LENGTH_LONG).show();
                    updateConnectionStatusUI();
                    invalidateOptionsMenu();
                    if (scanDialog != null && scanDialog.isShowing()) {
                        scanDialog.dismiss();
                    }
                });
            }
            @Override
            public void onDisconnected() {
                runOnUiThread(() -> {
                    Toast.makeText(WelcomeActivity.this, "已断开连接", Toast.LENGTH_SHORT).show();
                    updateConnectionStatusUI();
                    invalidateOptionsMenu();
                    if (scanDialog != null && scanDialog.isShowing()) {
                        scanDialog.dismiss();
                    }
                });
            }
        });

        // 初始化状态显示
        updateConnectionStatusUI();
    }

    @Override
    protected String getToolbarTitle() {
        return "BanBox 音频系统";
    }

    @Override
    protected void onResume() {
        super.onResume();
        // 每次返回页面时刷新蓝牙连接状态
        updateConnectionStatusUI();
        invalidateOptionsMenu(); // 刷新菜单图标
    }

    /**
     * 更新连接状态UI
     */
    private void updateConnectionStatusUI() {
        if (bluetoothHelper.isConnected()) {
            tvConnectionStatus.setText("● 已连接");
            tvConnectionStatus.setTextColor(0xFF00FF88);
            if (statusIndicator != null) {
                statusIndicator.setBackgroundTintList(
                        android.content.res.ColorStateList.valueOf(0xFF00FF88));
            }
            String deviceInfo = bluetoothHelper.getConnectedDevice();
            if (deviceInfo != null && !deviceInfo.isEmpty()) {
                tvDeviceName.setText(deviceInfo);
            }
            btnBluetoothConnect.setText("断开连接");
            btnBluetoothConnect.setBackgroundResource(R.drawable.button_volume_glow);
        } else {
            tvConnectionStatus.setText("● 未连接");
            tvConnectionStatus.setTextColor(0xFFFF4444);
            if (statusIndicator != null) {
                statusIndicator.setBackgroundTintList(
                        android.content.res.ColorStateList.valueOf(0xFFFF4444));
            }
            tvDeviceName.setText("BanBox 设备");
            btnBluetoothConnect.setText("连接蓝牙设备");
            btnBluetoothConnect.setBackgroundResource(R.drawable.button_connect);
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.welcome_menu, menu);
        MenuItem bluetoothItem = menu.findItem(R.id.menu_bluetooth);
        if (bluetoothItem != null) {
            bluetoothItem.setIcon(android.R.drawable.ic_menu_preferences);
            bluetoothItem.setTitle("设置");
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_bluetooth) {
            openAppSettingsDrawer();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    // 显示蓝牙扫描对话框
    private void showBluetoothScanDialog() {
        // 检查蓝牙权限
        if (!checkBluetoothPermissions()) {
            requestBluetoothPermissions();
            return;
        }

        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
            Toast.makeText(this, "请先启用蓝牙", Toast.LENGTH_SHORT).show();
            return;
        }

        // 检查是否已连接
        if (bluetoothHelper.isConnected()) {
            new AlertDialog.Builder(this)
                    .setTitle("蓝牙连接状态")
                    .setMessage("已连接到设备：" + bluetoothHelper.getConnectedDevice())
                    .setPositiveButton("断开连接", (dialog, which) -> {
                        bluetoothHelper.disconnect();
                    })
                    .setNegativeButton("确定", null)
                    .show();
            return;
        }

        // 创建扫描对话框（机甲风格）
        AlertDialog.Builder builder = new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_NoActionBar);

        // 加载自定义机甲风格布局
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_bluetooth_scan_mecha, null);
        builder.setView(dialogView);

        TextView statusText = dialogView.findViewById(R.id.tv_status);
        ListView deviceListView = dialogView.findViewById(R.id.lv_devices);
        Button rescanButton = dialogView.findViewById(R.id.btn_rescan);
        Button closeButton = dialogView.findViewById(R.id.btn_close);

        deviceList.clear();
        deviceAdapter = new ArrayAdapter<String>(this, R.layout.bluetooth_device_item, android.R.id.text1, deviceList);
        deviceListView.setAdapter(deviceAdapter);

        // 设置列表项点击监听器，用于连接设备
        deviceListView.setOnItemClickListener((parent, view, position, id) -> {
            String deviceInfo = deviceList.get(position);
            String mac = deviceInfo.replaceAll(".*\\((.*)\\)", "$1");
            BluetoothDevice device = bluetoothAdapter.getRemoteDevice(mac);
            AlertDialog connectDialog = new AlertDialog.Builder(this)
                    .setTitle("连接设备")
                    .setMessage("是否连接到：" + deviceInfo)
                    .setCancelable(false)
                    .create();
            connectDialog.setButton(AlertDialog.BUTTON_POSITIVE, "连接", (dialog, which) -> {
                Toast.makeText(this, "正在连接到 " + deviceInfo, Toast.LENGTH_SHORT).show();
                bluetoothHelper.connect(this, device);
                stopBleScan();
                // 不关闭弹窗，等待连接回调
            });
            connectDialog.setButton(AlertDialog.BUTTON_NEGATIVE, "取消", (dialog, which) -> {
                connectDialog.dismiss();
            });
            connectDialog.show();
        });

        // 关闭按钮
        closeButton.setOnClickListener(v -> {
            stopBleScan();
            if (scanDialog != null && scanDialog.isShowing()) {
                scanDialog.dismiss();
            }
        });

        // 重新扫描按钮
        rescanButton.setOnClickListener(v -> {
            stopBleScan();
            startBleScanWithTimeout(20000, statusText, rescanButton);
        });

        scanDialog = builder.create();
        scanDialog.show();

        // 自动开始扫描20秒
        startBleScanWithTimeout(20000, statusText, rescanButton);
    }

    // 检查蓝牙权限
    private boolean checkBluetoothPermissions() {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED &&
                ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }

    // 请求蓝牙权限
    private void requestBluetoothPermissions() {
        ActivityCompat.requestPermissions(this,
                new String[]{Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.ACCESS_FINE_LOCATION},
                REQUEST_BLUETOOTH_PERMISSIONS);
    }

    // 开始BLE扫描
    private void startBleScan() {
        if (isScanning) return;

        deviceList.clear();
        deviceAdapter.notifyDataSetChanged();

        if (bluetoothLeScanner != null) {
            isScanning = true;
            bluetoothLeScanner.startScan(scanCallback);
            Toast.makeText(this, "开始扫描BLE设备...", Toast.LENGTH_SHORT).show();
        }
    }

    // 停止BLE扫描
    private void stopBleScan() {
        if (!isScanning) return;

        if (bluetoothLeScanner != null) {
            bluetoothLeScanner.stopScan(scanCallback);
            isScanning = false;
            Toast.makeText(this, "扫描完成", Toast.LENGTH_SHORT).show();
        }
    }

    // BLE扫描回调
    private ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            super.onScanResult(callbackType, result);
            BluetoothDevice device = result.getDevice();

            runOnUiThread(() -> {
                // 使用统一的过滤工具类检查是否是BG设备
                String deviceInfo = BleDeviceFilter.getDeviceInfo(device.getName(), device.getAddress());
                if (deviceInfo != null && !deviceList.contains(deviceInfo)) {
                    deviceList.add(deviceInfo);
                    deviceAdapter.notifyDataSetChanged();
                }
            });
        }

        @Override
        public void onScanFailed(int errorCode) {
            super.onScanFailed(errorCode);
            Log.e("BLE", "扫描失败: " + errorCode);
            runOnUiThread(() -> Toast.makeText(WelcomeActivity.this, "扫描失败", Toast.LENGTH_SHORT).show());
        }
    };

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_BLUETOOTH_PERMISSIONS) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                showBluetoothScanDialog();
            } else {
                Toast.makeText(this, "需要蓝牙权限才能扫描设备", Toast.LENGTH_SHORT).show();
            }
        }
    }

    // 开始BLE扫描并设置超时
    private void startBleScanWithTimeout(long timeoutMs, TextView statusText, Button rescanButton) {
        if (isScanning) return;

        deviceList.clear();
        deviceAdapter.notifyDataSetChanged();
        statusText.setText("扫描中...");
        rescanButton.setEnabled(false);

        startBleScan();

        // 设置超时停止
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            runOnUiThread(() -> {
                if (isScanning) {
                    stopBleScan();
                    statusText.setText("扫描完成");
                    rescanButton.setEnabled(true);
                    rescanButton.setOnClickListener(v -> startBleScanWithTimeout(timeoutMs, statusText, rescanButton));
                }
            });
        }, timeoutMs);
    }

    // 更新扫描对话框
    private void updateScanDialog() {
        if (scanDialog != null && scanDialog.isShowing()) {
            deviceList.clear();
            if (bluetoothHelper.getConnectedDevice() != null) {
                deviceList.add(bluetoothHelper.getConnectedDevice());
            }
            deviceAdapter.notifyDataSetChanged();
            View dialogView = scanDialog.getWindow().getDecorView();
            TextView statusText = dialogView.findViewById(R.id.tv_status);
            Button rescanButton = dialogView.findViewById(R.id.btn_rescan);
            if (statusText != null) {
                statusText.setText("已连接: " + bluetoothHelper.getConnectedDevice());
            }
            if (rescanButton != null) {
                rescanButton.setEnabled(false);
                rescanButton.setText("重新扫描");
            }
            scanDialog.setButton(AlertDialog.BUTTON_POSITIVE, "断开连接", (dialog, which) -> {
                bluetoothHelper.disconnect();
            });
        }
    }

    // 断开设备连接由 BluetoothHelper 统一管理

    private void openAppSettingsDrawer() {
        if (drawerAppSettings == null || appSettingsOverlay == null) return;
        appSettingsOverlay.setVisibility(View.VISIBLE);
        drawerAppSettings.setVisibility(View.VISIBLE);
        drawerAppSettings.setTranslationX(drawerAppSettings.getWidth());
        drawerAppSettings.animate().translationX(0).setDuration(250).start();

        View btnClose = findViewById(R.id.btn_close_app_settings_drawer);
        if (btnClose != null) {
            btnClose.setOnClickListener(v -> closeAppSettingsDrawer());
        }

        View btnDebugTerminal = findViewById(R.id.btn_drawer_debug_terminal);
        if (btnDebugTerminal != null) {
            btnDebugTerminal.setOnClickListener(v -> {
                closeAppSettingsDrawer();
                startActivity(new Intent(WelcomeActivity.this, DebugTerminalActivity.class));
            });
        }

        View btnOtaUpgrade = findViewById(R.id.btn_drawer_ota_upgrade);
        if (btnOtaUpgrade != null) {
            btnOtaUpgrade.setOnClickListener(v -> {
                closeAppSettingsDrawer();
                startActivity(new Intent(WelcomeActivity.this, FirmwareUpgradeActivity.class));
            });
        }

        View btnBatteryCurve = findViewById(R.id.btn_drawer_battery_curve);
        if (btnBatteryCurve != null) {
            btnBatteryCurve.setOnClickListener(v -> {
                closeAppSettingsDrawer();
                startActivity(new Intent(WelcomeActivity.this, BatteryCurveActivity.class));
            });
        }

        // 低功耗设置
        Switch swLp = findViewById(R.id.sw_drawer_lp_enable);
        LinearLayout layoutLpTimeout = (LinearLayout) findViewById(R.id.layout_drawer_lp_timeout);
        TextView tvLpTimeout = findViewById(R.id.tv_drawer_lp_timeout_value);
        Button btnLpDec = findViewById(R.id.btn_drawer_lp_timeout_dec);
        Button btnLpInc = findViewById(R.id.btn_drawer_lp_timeout_inc);
        SharedPreferences lpPrefs = getSharedPreferences("ota_settings", MODE_PRIVATE);
        final int[] lpTimeout = {lpPrefs.getInt("lp_timeout_min", 5)};
        boolean cachedLp = lpPrefs.getBoolean("lp_enable", true);
        if (swLp != null) {
            swLp.setChecked(cachedLp);
            tvLpTimeout.setText(lpTimeout[0] + " 分钟");
            layoutLpTimeout.setVisibility(cachedLp ? View.VISIBLE : View.GONE);
            swLp.setOnCheckedChangeListener((btn, isChecked) -> {
                if (bluetoothHelper.isConnected()) {
                    bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb",
                            ("lp " + (isChecked ? "1" : "0") + "\r\n").getBytes(), null);
                }
                getSharedPreferences("ota_settings", MODE_PRIVATE).edit().putBoolean("lp_enable", isChecked).apply();
                layoutLpTimeout.setVisibility(isChecked ? View.VISIBLE : View.GONE);
            });
            btnLpDec.setOnClickListener(v -> {
                if (lpTimeout[0] > 1) {
                    lpTimeout[0]--;
                    tvLpTimeout.setText(lpTimeout[0] + " 分钟");
                    if (bluetoothHelper.isConnected()) {
                        bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb",
                                ("lp -t " + lpTimeout[0] + "\r\n").getBytes(), null);
                    }
                    getSharedPreferences("ota_settings", MODE_PRIVATE).edit().putInt("lp_timeout_min", lpTimeout[0]).apply();
                }
            });
            btnLpInc.setOnClickListener(v -> {
                if (lpTimeout[0] < 60) {
                    lpTimeout[0]++;
                    tvLpTimeout.setText(lpTimeout[0] + " 分钟");
                    if (bluetoothHelper.isConnected()) {
                        bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb",
                                ("lp -t " + lpTimeout[0] + "\r\n").getBytes(), null);
                    }
                    getSharedPreferences("ota_settings", MODE_PRIVATE).edit().putInt("lp_timeout_min", lpTimeout[0]).apply();
                }
            });
        }

        try {
            android.content.pm.PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
            TextView tvAppVersion = findViewById(R.id.tv_drawer_app_version);
            if (tvAppVersion != null) {
                tvAppVersion.setText(pi.versionName != null ? pi.versionName : "1.0.0");
            }
            TextView tvBuildNumber = findViewById(R.id.tv_drawer_build_number);
            if (tvBuildNumber != null) {
                tvBuildNumber.setText(String.valueOf(pi.versionCode));
            }
            TextView tvAppName = findViewById(R.id.tv_drawer_app_name);
            if (tvAppName != null) {
                tvAppName.setText(getString(pi.applicationInfo.labelRes));
            }
        } catch (Exception e) {
            android.util.Log.w("WelcomeActivity", "Failed to get package info", e);
        }
    }

    private void closeAppSettingsDrawer() {
        if (drawerAppSettings == null || appSettingsOverlay == null) return;
        drawerAppSettings.animate()
                .translationX(drawerAppSettings.getWidth())
                .setDuration(250)
                .withEndAction(() -> {
                    drawerAppSettings.setVisibility(View.GONE);
                    appSettingsOverlay.setVisibility(View.GONE);
                }).start();
    }

    private void showTerminalDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("BanBox BLE终端");
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(32, 32, 32, 32);

        android.widget.ScrollView scrollView = new android.widget.ScrollView(this);
        scrollView.setFillViewport(true);
        scrollView.setVerticalScrollBarEnabled(true);
        TextView rxBox = new TextView(this);
        rxBox.setHint("接收区");
        rxBox.setMinHeight(200);
        rxBox.setPadding(8, 8, 8, 8);
        rxBox.setBackgroundColor(0xFFEFEFEF);
        rxBox.setTextIsSelectable(true);
        scrollView.addView(rxBox, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        layout.addView(scrollView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));

        LinearLayout inputLayout = new LinearLayout(this);
        inputLayout.setOrientation(LinearLayout.HORIZONTAL);
        inputLayout.setPadding(0, 16, 0, 0);
        EditText txBox = new EditText(this);
        txBox.setHint("输入命令...");
        txBox.setMinHeight(80);
        txBox.setPadding(8, 8, 8, 8);
        LinearLayout.LayoutParams txParams = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        txParams.rightMargin = 16;
        inputLayout.addView(txBox, txParams);
        Button sendBtn = new Button(this);
        sendBtn.setText("发送");
        inputLayout.addView(sendBtn, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        layout.addView(inputLayout, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        builder.setView(layout);
        builder.setNegativeButton("关闭", null);
        AlertDialog dialog = builder.create();
        dialog.show();

        sendBtn.setOnClickListener(v -> {
            String cmd = txBox.getText().toString();
            if (cmd.isEmpty()) return;
            String cmdWithCRLF = cmd + "\r\n";
            bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", cmdWithCRLF.getBytes(), success -> {
                runOnUiThread(() -> {
                    if (success) {
                        rxBox.append("[TX] " + cmd + "\n");
                        txBox.setText("");
                    } else {
                        rxBox.append("[TX] 发送失败\n");
                    }
                    scrollView.post(() -> scrollView.fullScroll(View.FOCUS_DOWN));
                });
            });
        });

        bluetoothHelper.setBleNotifyListener((data) -> {
            runOnUiThread(() -> {
                rxBox.append("[RX] " + data + "\n");
                scrollView.post(() -> scrollView.fullScroll(View.FOCUS_DOWN));
            });
        });
    }

    @Override
    public void onBackPressed() {
        if (drawerAppSettings != null && drawerAppSettings.getVisibility() == View.VISIBLE) {
            closeAppSettingsDrawer();
        } else {
            super.onBackPressed();
        }
    }
}