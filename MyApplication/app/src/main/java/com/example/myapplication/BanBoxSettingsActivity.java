package com.example.myapplication;

import android.Manifest;
import androidx.appcompat.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.ScanResult;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
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
import android.view.animation.Animation;
import android.view.animation.TranslateAnimation;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.EditText;
import android.widget.Toast;
import android.widget.LinearLayout;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.viewpager2.widget.ViewPager2;
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

    // 侧边栏相关控件
    private View settingsDrawerOverlay; // 遮罩层
    private LinearLayout drawerSettings; // 侧边栏
    private SwitchCompat switchBootSound; // 开机提示音开关
    private SharedPreferences sharedPreferences; // 用于保存设置

    // 副音箱模式相关
    private ImageButton btnSpeakerMode; // 副音箱模式切换按钮
    private boolean isSecondarySpeakerMode = false; // 当前是否为副音箱模式

    // ViewPager相关
    private ViewPager2 viewPagerFunctions;
    private LinearLayout indicatorContainer;
    private FunctionPagerAdapter functionPagerAdapter;

    // 侧边栏动画常量
    private static final int DRAWER_WIDTH = 280; // 侧边栏宽度（dp）
    private static final long ANIMATION_DURATION = 300; // 动画时长（ms）

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // 获取全局BluetoothHelper实例
        bluetoothHelper = com.example.myapplication.BluetoothManager.getInstance().getBluetoothHelper();

        setContentView(R.layout.activity_banbox_settings_new);

        // 初始化SharedPreferences（保存设置）
        sharedPreferences = getSharedPreferences("BanBoxSettings", MODE_PRIVATE);

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
        Button btnTerminal = findViewById(R.id.btn_terminal);
        btnSpeakerMode = findViewById(R.id.btn_speaker_mode);

        // 初始化ViewPager
        viewPagerFunctions = findViewById(R.id.viewpager_functions);
        indicatorContainer = findViewById(R.id.indicator_container);

        // 设置ViewPager适配器
        setupFunctionViewPager();

        // ========== 初始化侧边栏相关控件 ==========
        initSideDrawer();

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

        // 终端
        if (btnTerminal != null) {
            btnTerminal.setOnClickListener(v -> {
                if (checkConnection()) {
                    showTerminalDialog();
                }
            });
        }

        // 副音箱模式切换
        if (btnSpeakerMode != null) {
            Log.d("BanBoxSettings", "btnSpeakerMode initialized successfully");
            // 从SharedPreferences加载副音箱模式状态
            isSecondarySpeakerMode = sharedPreferences.getBoolean("secondary_speaker_mode", false);
            Log.d("BanBoxSettings", "Loaded speaker mode from prefs: " + isSecondarySpeakerMode);
            updateSpeakerModeUI();

            btnSpeakerMode.setOnClickListener(v -> {
                Log.d("BanBoxSettings", "Speaker mode button clicked");
                toggleSpeakerMode();
            });
        } else {
            Log.e("BanBoxSettings", "btnSpeakerMode is null!");
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

    /**
     * 初始化侧边栏控件和交互逻辑
     */
    private void initSideDrawer() {
        // 获取控件引用
        settingsDrawerOverlay = findViewById(R.id.settings_drawer_overlay);
        drawerSettings = findViewById(R.id.drawer_settings);
        switchBootSound = findViewById(R.id.switch_boot_sound);
        View btnOpenSettings = findViewById(R.id.btn_open_settings); // 打开设置按钮
        View btnCloseSettingsDrawer = findViewById(R.id.btn_close_settings_drawer); // 关闭侧边栏按钮

        Log.d("BanBoxSettings", "settingsDrawerOverlay: " + settingsDrawerOverlay);
        Log.d("BanBoxSettings", "drawerSettings: " + drawerSettings);
        Log.d("BanBoxSettings", "switchBootSound: " + switchBootSound);
        Log.d("BanBoxSettings", "btnOpenSettings: " + btnOpenSettings);
        Log.d("BanBoxSettings", "btnCloseSettingsDrawer: " + btnCloseSettingsDrawer);

        // 从SharedPreferences加载开机提示音设置
        boolean isBootSoundEnabled = sharedPreferences.getBoolean("boot_sound_enabled", true);
        if (switchBootSound != null) {
            switchBootSound.setChecked(isBootSoundEnabled);
        }

        // 1. 打开设置按钮点击事件（右上角的设置图标）
        if (btnOpenSettings != null) {
            btnOpenSettings.setOnClickListener(v -> {
                Log.d("BanBoxSettings", "Settings button clicked");
                openSettingsDrawer();
            });
        }

        // 2. 遮罩层点击事件（点击遮罩关闭侧边栏）
        if (settingsDrawerOverlay != null) {
            settingsDrawerOverlay.setOnClickListener(v -> closeSettingsDrawer());
        }

        // 3. 关闭侧边栏按钮点击事件
        if (btnCloseSettingsDrawer != null) {
            btnCloseSettingsDrawer.setOnClickListener(v -> closeSettingsDrawer());
        }

        // 4. 开机提示音开关状态改变事件（保存设置）
        if (switchBootSound != null) {
            switchBootSound.setOnCheckedChangeListener((buttonView, isChecked) -> {
                // 保存设置到SharedPreferences
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putBoolean("boot_sound_enabled", isChecked);
                editor.apply();

                // 可选：如果蓝牙已连接，发送命令到设备更新设置
                if (bluetoothHelper.isConnected()) {
                    String cmd = isChecked ? "boot_sound on" : "boot_sound off";
                    sendBleShellCommand(cmd, success -> {
                        runOnUiThread(() -> {
                            String toastMsg = isChecked ? "已启用开机提示音" : "已禁用开机提示音";
                            if (success) {
                                Toast.makeText(BanBoxSettingsActivity.this, toastMsg, Toast.LENGTH_SHORT).show();
                            } else {
                                Toast.makeText(BanBoxSettingsActivity.this, "设置失败，请重试", Toast.LENGTH_SHORT).show();
                                // 恢复开关状态
                                switchBootSound.setChecked(!isChecked);
                            }
                        });
                    });
                } else {
                    // 未连接蓝牙，仅保存本地设置
                    String toastMsg = isChecked ? "已启用开机提示音（下次连接生效）" : "已禁用开机提示音（下次连接生效）";
                    Toast.makeText(this, toastMsg, Toast.LENGTH_SHORT).show();
                }
            });
        }
    }

    /**
     * 打开侧边栏（简化版用于测试）
     */
    private void openSettingsDrawer() {
        if (drawerSettings == null || settingsDrawerOverlay == null) {
            Log.e("BanBoxSettings", "drawerSettings or settingsDrawerOverlay is null");
            return;
        }

        Log.d("BanBoxSettings", "Opening settings drawer");

        // 显示遮罩层
        settingsDrawerOverlay.setVisibility(View.VISIBLE);

        // 直接设置侧边栏可见，不使用动画
        drawerSettings.setTranslationX(0); // 确保位置正确
        drawerSettings.setVisibility(View.VISIBLE);

        Log.d("BanBoxSettings", "Settings drawer opened (no animation)");
    }

    /**
     * 关闭侧边栏（简化版用于测试）
     */
    private void closeSettingsDrawer() {
        if (drawerSettings == null || settingsDrawerOverlay == null) return;

        Log.d("BanBoxSettings", "Closing settings drawer");

        // 隐藏遮罩层和侧边栏
        settingsDrawerOverlay.setVisibility(View.GONE);
        drawerSettings.setVisibility(View.GONE);

        Log.d("BanBoxSettings", "Settings drawer closed");
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
    public boolean checkConnection() {
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
        // 销毁时关闭侧边栏
        closeSettingsDrawer();
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

    /**
     * 切换副音箱模式
     */
    private void toggleSpeakerMode() {
        Log.d("BanBoxSettings", "toggleSpeakerMode called, current mode: " + isSecondarySpeakerMode);

        // 检查蓝牙连接状态
        if (!checkConnection()) {
            Log.d("BanBoxSettings", "Connection check failed");
            return;
        }

        isSecondarySpeakerMode = !isSecondarySpeakerMode;
        Log.d("BanBoxSettings", "New mode: " + isSecondarySpeakerMode);

        // 保存设置到SharedPreferences
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putBoolean("secondary_speaker_mode", isSecondarySpeakerMode);
        editor.apply();

        // 更新UI
        runOnUiThread(() -> updateSpeakerModeUI());

        // 发送命令到设备
        String cmd = isSecondarySpeakerMode ? "mode secondary" : "mode main";
        Log.d("BanBoxSettings", "Sending command: " + cmd);
        sendBleShellCommand(cmd, success -> {
            runOnUiThread(() -> {
                String modeText = isSecondarySpeakerMode ? "副音箱模式" : "主音箱模式";
                if (success) {
                    Toast.makeText(BanBoxSettingsActivity.this, "已切换到" + modeText, Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(BanBoxSettingsActivity.this, "切换失败，请重试", Toast.LENGTH_SHORT).show();
                    // 恢复UI状态
                    isSecondarySpeakerMode = !isSecondarySpeakerMode;
                    updateSpeakerModeUI();
                }
            });
        });
    }

    /**
     * 更新副音箱模式UI
     */
    private void updateSpeakerModeUI() {
        if (btnSpeakerMode != null) {
            try {
                // 根据模式更新图标
                int iconRes = isSecondarySpeakerMode ? R.drawable.ic_speaker_secondary : R.drawable.ic_speaker_main;
                Log.d("BanBoxSettings", "Setting icon resource: " + iconRes + " for mode: " + (isSecondarySpeakerMode ? "secondary" : "main"));
                btnSpeakerMode.setImageResource(iconRes);

                // 更新内容描述
                String description = isSecondarySpeakerMode ? "当前为副音箱模式，点击切换到主音箱" : "当前为主音箱模式，点击切换到副音箱";
                btnSpeakerMode.setContentDescription(description);

                Log.d("BanBoxSettings", "Updated speaker mode UI: " + (isSecondarySpeakerMode ? "secondary" : "main"));
            } catch (Exception e) {
                Log.e("BanBoxSettings", "Error updating speaker mode UI", e);
            }
        } else {
            Log.w("BanBoxSettings", "btnSpeakerMode is null, cannot update UI");
        }
    }

    /**
     * 重写返回键：如果侧边栏打开，先关闭侧边栏
     */
    @Override
    public void onBackPressed() {
        if (drawerSettings != null && drawerSettings.getVisibility() == View.VISIBLE) {
            closeSettingsDrawer();
        } else {
            super.onBackPressed();
        }
    }

    /**
     * 设置功能ViewPager
     */
    private void setupFunctionViewPager() {
        // 创建功能数据
        List<FunctionItem> functions = new ArrayList<>();
        functions.add(new FunctionItem("", FxControlActivity.class));
        functions.add(new FunctionItem("", EqControlActivity.class));
        functions.add(new FunctionItem("", HardwareVolumeActivity.class));
        functions.add(new FunctionItem("", MetronomeActivity.class));
        functions.add(new FunctionItem("", AudioChainDiagramActivity.class));

        // 创建适配器
        functionPagerAdapter = new FunctionPagerAdapter(this, functions);
        viewPagerFunctions.setAdapter(functionPagerAdapter);

        // 设置页面指示器
        setupIndicators(functions.size());

        // 监听页面变化
        viewPagerFunctions.registerOnPageChangeCallback(new ViewPager2.OnPageChangeCallback() {
            @Override
            public void onPageSelected(int position) {
                updateIndicators(position);
            }
        });
    }

    /**
     * 设置页面指示器
     */
    private void setupIndicators(int count) {
        indicatorContainer.removeAllViews();
        for (int i = 0; i < count; i++) {
            View indicator = new View(this);
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(12, 12);
            params.setMargins(4, 0, 4, 0);
            indicator.setLayoutParams(params);
            indicator.setBackgroundResource(R.drawable.indicator_unselected);
            indicatorContainer.addView(indicator);
        }
        updateIndicators(0);
    }

    /**
     * 更新指示器状态
     */
    private void updateIndicators(int position) {
        for (int i = 0; i < indicatorContainer.getChildCount(); i++) {
            View indicator = indicatorContainer.getChildAt(i);
            if (i == position) {
                indicator.setBackgroundResource(R.drawable.indicator_selected);
            } else {
                indicator.setBackgroundResource(R.drawable.indicator_unselected);
            }
        }
    }

    /**
     * 功能项数据类
     */
    public static class FunctionItem {
        public String title;
        public Class<?> activityClass;

        public FunctionItem(String title, Class<?> activityClass) {
            this.title = title;
            this.activityClass = activityClass;
        }
    }
}