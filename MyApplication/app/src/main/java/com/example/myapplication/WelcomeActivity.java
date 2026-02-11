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
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import java.util.ArrayList;
import java.util.List;

public class WelcomeActivity extends AppCompatActivity {
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

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_welcome_mecha);

        // 获取全局BluetoothHelper实例
        bluetoothHelper = com.example.myapplication.BluetoothManager.getInstance().getBluetoothHelper();

        // 设置Toolbar
        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

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

        ImageButton enterButton = findViewById(R.id.btn_enter);
        enterButton.setOnClickListener(v -> {
            Intent intent = new Intent(WelcomeActivity.this, HomeActivity.class);
            startActivity(intent);
            finish();
        });

        ImageButton banboxSettingsButton = findViewById(R.id.btn_banbox_settings);
        banboxSettingsButton.setOnClickListener(v -> {
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
        // 根据连接状态改变图标
        MenuItem bluetoothItem = menu.findItem(R.id.menu_bluetooth);
        if (bluetoothItem != null) {
            if (bluetoothHelper.isConnected()) {
                bluetoothItem.setIcon(android.R.drawable.ic_menu_manage);
                bluetoothItem.setTitle("蓝牙已连接");
            } else {
                bluetoothItem.setIcon(android.R.drawable.ic_menu_manage);
                bluetoothItem.setTitle("蓝牙连接");
            }
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_bluetooth) {
            showBluetoothScanDialog();
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
}