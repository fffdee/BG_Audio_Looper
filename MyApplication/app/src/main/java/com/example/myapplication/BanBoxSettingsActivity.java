package com.example.myapplication;

import android.Manifest;
import androidx.appcompat.app.AlertDialog;
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
import android.widget.ListView;
import android.widget.TextView;
import android.widget.EditText;
import android.view.Gravity;
import android.widget.Toast;
import android.widget.LinearLayout;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import java.util.ArrayList;
import java.util.List;

public class BanBoxSettingsActivity extends AppCompatActivity {
    private BluetoothHelper bluetoothHelper;
    private static final int REQUEST_BLUETOOTH_PERMISSIONS = 1;
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner bluetoothLeScanner;
    private boolean isScanning = false;
    private Handler handler = new Handler(Looper.getMainLooper());
    private List<String> deviceList = new ArrayList<>();
    private ArrayAdapter<String> deviceAdapter;
    private AlertDialog scanDialog;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // 获取全局BluetoothHelper实例
        bluetoothHelper = com.example.myapplication.BluetoothManager.getInstance().getBluetoothHelper();

        setContentView(R.layout.activity_banbox_settings_new);

        // 初始化蓝牙适配器
        android.bluetooth.BluetoothManager bluetoothManager = 
                (android.bluetooth.BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
        bluetoothAdapter = bluetoothManager.getAdapter();
        if (bluetoothAdapter != null) {
            bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();
        }

        // 初始化UI控件
        TextView tvStatus = findViewById(R.id.tv_connection_status);
        TextView tvDeviceName = findViewById(R.id.tv_device_name);
        Button btnConnect = findViewById(R.id.btn_connect);
        Button btnFxControl = findViewById(R.id.btn_fx_control);
        Button btnEqControl = findViewById(R.id.btn_eq_control);
        Button btnVolumeControl = findViewById(R.id.btn_volume_control);
        Button btnTerminal = findViewById(R.id.btn_terminal);
        Button btnAudioChain = findViewById(R.id.btn_audio_chain);

        if (tvStatus != null) {
            updateConnectionStatus(tvStatus, tvDeviceName, btnConnect);
        }

        // 连接按钮
        if (btnConnect != null) {
            btnConnect.setOnClickListener(v -> {
                if (bluetoothHelper.isConnected()) {
                    bluetoothHelper.disconnect();
                    updateConnectionStatus(tvStatus, tvDeviceName, btnConnect);
                    Toast.makeText(this, "已断开连接", Toast.LENGTH_SHORT).show();
                } else {
                    showScanDialog();
                }
            });
        }

        // 效果控制
        if (btnFxControl != null) {
            btnFxControl.setOnClickListener(v -> {
                if (checkConnection()) {
                    startActivity(new Intent(this, FxControlActivity.class));
                }
            });
        }

        // 均衡器
        if (btnEqControl != null) {
            btnEqControl.setOnClickListener(v -> {
                if (checkConnection()) {
                    startActivity(new Intent(this, EqControlActivity.class));
                }
            });
        }

        // 音量控制
        if (btnVolumeControl != null) {
            btnVolumeControl.setOnClickListener(v -> {
                if (checkConnection()) {
                    startActivity(new Intent(this, HardwareVolumeActivity.class));
                }
            });
        }

        // 终端
        if (btnTerminal != null) {
            btnTerminal.setOnClickListener(v -> {
                if (checkConnection()) {
                    showTerminalDialog();
                }
            });
        }

        // 音频链路图
        if (btnAudioChain != null) {
            btnAudioChain.setOnClickListener(v -> {
                if (checkConnection()) {
                    startActivity(new Intent(this, AudioChainDiagramActivity.class));
                }
            });
        }

        // 监听蓝牙连接状态
        bluetoothHelper.setOnConnectionChangedListener(new BluetoothHelper.OnConnectionChangedListener() {
            @Override
            public void onConnected(String deviceName, android.bluetooth.BluetoothGatt gatt) {
                runOnUiThread(() -> {
                    Toast.makeText(BanBoxSettingsActivity.this, "连接成功: " + deviceName, Toast.LENGTH_SHORT).show();
                    updateConnectionStatus(tvStatus, tvDeviceName, btnConnect);
                    invalidateOptionsMenu();
                    if (scanDialog != null && scanDialog.isShowing()) {
                        scanDialog.dismiss();
                    }
                });
            }

            @Override
            public void onDisconnected() {
                runOnUiThread(() -> {
                    Toast.makeText(BanBoxSettingsActivity.this, "已断开连接", Toast.LENGTH_SHORT).show();
                    updateConnectionStatus(tvStatus, tvDeviceName, btnConnect);
                    invalidateOptionsMenu();
                    if (scanDialog != null && scanDialog.isShowing()) {
                        scanDialog.dismiss();
                    }
                });
            }
        });
    }

    // 发送命令到AB01特征（0x0006）
    private void sendBleShellCommand(String cmd, java.util.function.Consumer<Boolean> callback) {
        String cmdWithCRLF = cmd + "\r\n";
        bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", cmdWithCRLF.getBytes(), callback);
    }

    // 终端命令行弹窗
    private void showTerminalDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("BanBox BLE终端");
        // 垂直布局：接收区(ScrollView+TextView) + 输入区(EditText+Button)
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(32, 32, 32, 32);

        // 接收区（独立ScrollView）
        android.widget.ScrollView scrollView = new android.widget.ScrollView(this);
        scrollView.setFillViewport(true);
        scrollView.setScrollbarFadingEnabled(false);
        scrollView.setVerticalScrollBarEnabled(true);
        TextView rxBox = new TextView(this);
        rxBox.setHint("接收区");
        rxBox.setMinHeight(200);
        rxBox.setMaxHeight(600);
        rxBox.setPadding(8,8,8,8);
        rxBox.setBackgroundColor(0xFFEFEFEF);
        rxBox.setTextIsSelectable(true);
        rxBox.setVerticalScrollBarEnabled(true);
        scrollView.addView(rxBox, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        ));
        layout.addView(scrollView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                0, 1f // 权重1，自动填满剩余空间
        ));

        // 输入区（横向LinearLayout，EditText+Button）
        LinearLayout inputLayout = new LinearLayout(this);
        inputLayout.setOrientation(LinearLayout.HORIZONTAL);
        inputLayout.setPadding(0, 16, 0, 0);
        EditText txBox = new EditText(this);
        txBox.setHint("输入命令...");
        txBox.setMinHeight(80);
        txBox.setPadding(8,8,8,8);
        LinearLayout.LayoutParams txParams = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        txParams.rightMargin = 16;
        inputLayout.addView(txBox, txParams);
        Button sendBtn = new Button(this);
        sendBtn.setText("发送");
        inputLayout.addView(sendBtn, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        ));
        layout.addView(inputLayout, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        ));
        builder.setView(layout);
        builder.setNegativeButton("关闭", null);
        AlertDialog dialog = builder.create();
        dialog.show();

        // 自动订阅 notify（打开弹窗时）
        enableBleNotify();

        // 发送命令
        sendBtn.setOnClickListener(v -> {
            String cmd = txBox.getText().toString();
            if (cmd.isEmpty()) return;
            sendBleShellCommand(cmd, success -> {
                runOnUiThread(() -> {
                    if (success) {
                        rxBox.append("[TX] " + cmd + "\n");
                        txBox.setText("");
                    } else {
                        rxBox.append("[TX] 发送失败\n");
                    }
                    // 滚动到底部
                    scrollView.post(() -> scrollView.fullScroll(android.view.View.FOCUS_DOWN));
                });
            });
        });

        // 接收数据监听（Notify）
        bluetoothHelper.setBleNotifyListener((data) -> {
            runOnUiThread(() -> {
                rxBox.append("[RX] " + data + "\n");
                // 滚动到底部
                scrollView.post(() -> scrollView.fullScroll(android.view.View.FOCUS_DOWN));
            });
        });
    }

     
    private void updateConnectionStatus(TextView tvStatus, TextView tvDeviceName, Button btnConnect) {
        if (bluetoothHelper.isConnected()) {
            tvStatus.setText("● 已连接");
            tvStatus.setTextColor(0xFF00FF88);
            String deviceInfo = bluetoothHelper.getConnectedDevice();
            if (deviceInfo != null && !deviceInfo.isEmpty()) {
                tvDeviceName.setText(deviceInfo);
            }
            btnConnect.setText("断开");
            btnConnect.setBackgroundResource(R.drawable.button_volume_glow);
        } else {
            tvStatus.setText("● 未连接");
            tvStatus.setTextColor(0xFFFF4444);
            tvDeviceName.setText("BanBox 音频处理器");
            btnConnect.setText("连接");
            btnConnect.setBackgroundResource(R.drawable.button_connect);
        }
    }

    /**
     * 显示扫描对话框
     */
    private void showScanDialog() {
        showBluetoothScanDialog();
    }

    /**
     * 检查蓝牙连接状态
     */
    private boolean checkConnection() {
        if (!bluetoothHelper.isConnected()) {
            Toast.makeText(this, "请先连接蓝牙设备", Toast.LENGTH_SHORT).show();
            return false;
        }
        return true;
    }

    // 自动订阅 notify（BluetoothHelper已在连接时自动订阅，此处无需额外操作）
    private void enableBleNotify() {
        // 注意：BluetoothHelper.onServicesDiscovered()已自动订阅AB02和AB03的notify
        // 无需在此重复订阅，否则会导致混乱
        Log.d("BLE", "Notify already enabled by BluetoothHelper");
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
    public boolean onOptionsItemSelected(android.view.MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            // 返回到主页面
            Intent intent = new Intent(BanBoxSettingsActivity.this, WelcomeActivity.class);
            startActivity(intent);
            finish();
            return true;
        } else if (item.getItemId() == R.id.menu_bluetooth) {
            showBluetoothScanDialog();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    // 显示蓝牙扫描对话框
    // 完全同步 WelcomeActivity 的蓝牙连接弹窗逻辑
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

        // 加载自定义布局
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
            runOnUiThread(() -> Toast.makeText(BanBoxSettingsActivity.this, "扫描失败", Toast.LENGTH_SHORT).show());
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

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (isScanning) {
            stopBleScan();
        }
    }

    // 更新扫描对话框
    private void updateScanDialog() {
        if (scanDialog != null && scanDialog.isShowing()) {
            deviceList.clear();
            if (bluetoothHelper.getConnectedDevice() != null) {
                deviceList.add(bluetoothHelper.getConnectedDevice());
            }
            deviceAdapter.notifyDataSetChanged();
            scanDialog.setButton(AlertDialog.BUTTON_POSITIVE, "断开连接", (dialog, which) -> {
                bluetoothHelper.disconnect();
            });
        }
    }

    // 断开设备连接由 BluetoothHelper 统一管理

    // 彻底移除 showBluetoothStatusDialog 旧实现，已由 showBluetoothScanDialog 统一管理

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
}