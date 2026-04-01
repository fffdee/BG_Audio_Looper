package com.example.myapplication;

import android.bluetooth.BluetoothGatt;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.ImageButton;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * EQ 控制 Activity
 * 提供均衡器参数调节界面，支持多个节点和频段
 * 
 * 固件协议说明（2026-02-02 更新）：
 * - 设置 band_enable=1 会自动触发固件更新 filter_count 并重新配置滤波器系数
 * - 修改任何参数（type/f0/Q/gain）后会立即重新计算滤波器系数
 * - 不再需要手动发送 filter_count 命令（固件会根据启用的 band 自动计算）
 * - 命令必须按顺序发送：type → f0 → Q → gain → enable（enable 触发应用）
 */
public class EqControlActivity extends BaseActivity {

    // UI 控件
    private EqCurveView eqCurveView;
    private Spinner spinnerNode;
    private Spinner spinnerBand;
    private Spinner spinnerFilterType;
    private Switch switchBandEnable;
    private SeekBar seekbarFreq;
    private SeekBar seekbarQ;
    private SeekBar seekbarGain;
    private TextView textFreqValue;
    private TextView textQValue;
    private TextView textGainValue;
    private Button btnAddBand;
    private Button btnRemoveBand;
    private ImageButton btnSave;
    private ImageButton btnImportEq;
    private ImageButton btnExportEq;
    private Button btnSyncEq;
    private ImageButton btnOpenPresets;
    private ImageButton btnResetEq;
    
    // 侧边栏相关
    private View drawerPresets;
    private View drawerOverlay;
    private ImageButton btnCloseDrawer;
    private TextView tvCurrentNode;
    private TextView[] drawerPresetNameViews = new TextView[6];
    private Button[] drawerLoadButtons = new Button[6];
    private Button[] drawerSaveButtons = new Button[6];
    private ImageButton[] drawerResetButtons = new ImageButton[6];
    private int currentPreset = -1; // 当前选中的预设槽位，-1表示无选中

    // 数据结构
    private List<List<EqCurveView.EqBand>> nodesBands; // 每个节点的所有频段
    private int currentNode = 0; // 当前选中的节点
    private int currentBand = 0; // 当前选中的频段
    
    // 预设存储：每个节点6个预设槽位
    private List<List<List<EqCurveView.EqBand>>> nodePresets; // [node][preset][band]
    private List<String[]> nodePresetNames; // [node][6个预设名称]
    
    // 防止Spinner更新时触发事件的标志
    private boolean isUpdatingSpinner = false;

    // 蓝牙助手
    private BluetoothHelper bluetoothHelper;

    // 滤波器类型选项
    private static final String[] FILTER_TYPES = {
        "Peaking", "Low Shelf", "High Shelf", "Low Pass", "High Pass", "Band Pass", "Notch"
    };

    /**
     * 2026-02-04 新架构:
     * 4个独立ADC EQ + 1个USB/BT EQ
     * 节点名称和固件Effect ID映射:
     *   - 乐器左声道EQ (Effect ID 4) -> NODE_ID_EQ_GUITAR_L
     *   - 乐器右声道EQ (Effect ID 5) -> NODE_ID_EQ_GUITAR_R
     *   - 麦克风左声道EQ (Effect ID 6) -> NODE_ID_EQ_MIC_L
     *   - 麦克风右声道EQ (Effect ID 7) -> NODE_ID_EQ_MIC_R
     *   - USB/BT EQ (Effect ID 14) -> NODE_ID_USB_BT_EQ
     */
    private static final String[] NODE_NAMES = {
        "乐器L EQ",
        "乐器R EQ",
        "麦克风L EQ",
        "麦克风R EQ",
        "USB/BT EQ"
    };
    
    // 实际效果器 ID 映射 (对应固件中的 EffectId_t)
    private static final int[] EFFECT_IDS = {4, 5, 6, 7, 14};

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // 初始化蓝牙助手
        bluetoothHelper = com.example.myapplication.BluetoothManager.getInstance().getBluetoothHelper();
        
        setContentView(R.layout.activity_eq_control);
        setupBaseToolbar(true);

        try {
            // 检查蓝牙连接状态（非阻断性）
            if (bluetoothHelper == null) {
                android.util.Log.e("EQ_CRASH", "bluetoothHelper is null");
                Toast.makeText(this, "蓝牙初始化失败", Toast.LENGTH_LONG).show();
                finish();
                return;
            }
            
            if (!bluetoothHelper.isConnected()) {
                android.util.Log.w("EQ_WARN", "蓝牙未连接，页面只读");
                Toast.makeText(this, "蓝牙未连接，页面为只读状态", Toast.LENGTH_SHORT).show();
            }
            
            // 设置蓝牙连接状态监听
            bluetoothHelper.setOnConnectionChangedListener(new BluetoothHelper.OnConnectionChangedListener() {
                @Override
                public void onConnected(String deviceName, BluetoothGatt gatt) {
                    android.util.Log.d("EQ_BLE_MONITOR", "Connected to: " + deviceName);
                    runOnUiThread(() -> {
                        Toast.makeText(EqControlActivity.this, "蓝牙已连接: " + deviceName, Toast.LENGTH_SHORT).show();
                        // 刷新UI状态
                        updateConnectionUI(true);
                    });
                }

                @Override
                public void onDisconnected() {
                    runOnUiThread(() -> {
                        Toast.makeText(EqControlActivity.this, "蓝牙连接已断开", Toast.LENGTH_LONG).show();
                        updateConnectionUI(false);
                        finish();
                    });
                }
            });

            // 初始化数据结构
            initData();

            // 初始化 UI 控件
            initViews();
        } catch (Exception e) {
            android.util.Log.e("EQ_CRASH", "onCreate initialization error", e);
            Toast.makeText(this, "初始化失败: " + e.getMessage(), Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        // 设置监听器
        setupListeners();

        // 设置BLE通知监听器来接收查询响应
        setupBleNotificationListener();

        // 从 SharedPreferences 加载配置
        loadEqConfigFromPreferences();
        
        // 延迟加载预设配置，避免阻塞UI初始化
        new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(() -> {
            loadPresetsFromPreferences();
            updateDrawerPresetButtons();
        }, 100);

        // 加载当前节点的频段数据
        loadCurrentNodeBands();
        
        // 查询EQ参数并同步到UI
        queryEqParams();
    }
    
    /**
     * 查询EQ参数
     */
    private void queryEqParams() {
        // 一次发送所有节点的查询命令，每条命令结尾都带换行符
        StringBuilder cmdBuilder = new StringBuilder();
        for (int i = 0; i < EFFECT_IDS.length; i++) {
            cmdBuilder.append("param -q effect ").append(EFFECT_IDS[i]).append("\r\n");
        }
        sendQueryCommand(cmdBuilder.toString());
    }
    
    /**
     * 发送查询命令
     */
    private void sendQueryCommand(String cmd) {
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            String fullCmd = cmd + "\r\n";
            bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", 
                fullCmd.getBytes(), success -> {
                    if (success) {
                        android.util.Log.d("EqControl", "Query sent: " + cmd);
                    } else {
                        runOnUiThread(() -> Toast.makeText(this, "查询EQ参数失败", Toast.LENGTH_SHORT).show());
                    }
                });
        }
    }

    @Override
    protected String getToolbarTitle() {
        return "均衡器设置";
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (bluetoothHelper != null) {
            boolean isConnected = bluetoothHelper.isConnected();
            updateConnectionUI(isConnected);
            if (!isConnected) {
                Toast.makeText(this, "蓝牙未连接，页面为只读状态", Toast.LENGTH_SHORT).show();
            }
        }
    }

    /**
     * 更新连接状态UI
     */
    private void updateConnectionUI(boolean isConnected) {
        android.util.Log.d("EQ_UI", "更新连接状态UI: " + isConnected);
        // 可以在这里更新标题栏、按钮状态等
        // 例如：禁用/启用同步按钮
        if (btnSyncEq != null) {
            btnSyncEq.setEnabled(isConnected);
            btnSyncEq.setAlpha(isConnected ? 1.0f : 0.5f);
        }
        if (btnSave != null) {
            btnSave.setEnabled(isConnected);
            btnSave.setAlpha(isConnected ? 1.0f : 0.5f);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // 移除蓝牙连接监听
        if (bluetoothHelper != null) {
            bluetoothHelper.setOnConnectionChangedListener(null);
        }
        // 清理命令队列
        commandHandler.removeCallbacksAndMessages(null);
        commandQueue.clear();
        isSendingCommand = false;
        android.util.Log.d("EQ_QUEUE", "Activity销毁，队列已清空");
    }

    /**
     * 初始化数据结构
     */
    private void initData() {
        nodesBands = new ArrayList<>(NODE_NAMES.length);
        nodePresets = new ArrayList<>(NODE_NAMES.length);
        nodePresetNames = new ArrayList<>(NODE_NAMES.length);
        
        // 预定义频率分布，避免重复计算
        float[] defaultFreqs = {32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
        
        for (int i = 0; i < NODE_NAMES.length; i++) {
            List<EqCurveView.EqBand> bands = new ArrayList<>(10);
            // 为每个节点添加 10 个频段
            // 第一个频段默认启用，其余禁用
            for (int j = 0; j < 10; j++) {
                bands.add(new EqCurveView.EqBand(
                    j == 0,  // 只有第一个频段默认启用
                    0,       // type
                    defaultFreqs[j], // 使用预定义频率
                    1.0f,    // Q
                    0        // gain
                ));
            }
            nodesBands.add(bands);
            
            // 为每个节点初始化6个空预设槽位
            List<List<EqCurveView.EqBand>> presets = new ArrayList<>(6);
            for (int p = 0; p < 6; p++) {
                presets.add(null);
            }
            nodePresets.add(presets);
            
            // 为每个节点初始化6个默认预设名称
            String[] presetNames = new String[6];
            for (int p = 0; p < 6; p++) {
                presetNames[p] = "预设 " + (p + 1);
            }
            nodePresetNames.add(presetNames);
        }
    }

    /**
     * 初始化 UI 控件
     */
    private void initViews() {
        try {
            eqCurveView = findViewById(R.id.eq_curve_view);
            if (eqCurveView == null) {
                android.util.Log.e("EQ_INIT", "eq_curve_view not found in layout");
                throw new NullPointerException("eq_curve_view not found");
            }
            
            spinnerNode = findViewById(R.id.spinner_node);
            spinnerBand = findViewById(R.id.spinner_band);
            spinnerFilterType = findViewById(R.id.spinner_filter_type);
            switchBandEnable = findViewById(R.id.switch_band_enable);
            seekbarFreq = findViewById(R.id.seekbar_freq);
            seekbarQ = findViewById(R.id.seekbar_q);
            seekbarGain = findViewById(R.id.seekbar_gain);
            textFreqValue = findViewById(R.id.text_freq_value);
            textQValue = findViewById(R.id.text_q_value);
            textGainValue = findViewById(R.id.text_gain_value);
            btnAddBand = findViewById(R.id.btn_add_band);
            btnRemoveBand = findViewById(R.id.btn_remove_band);
            btnSave = findViewById(R.id.btn_save);
            btnImportEq = findViewById(R.id.btn_import_eq);
            btnExportEq = findViewById(R.id.btn_export_eq);
            btnSyncEq = findViewById(R.id.btn_sync_eq);
            btnOpenPresets = findViewById(R.id.btn_open_presets);
            btnResetEq = findViewById(R.id.btn_reset_eq);
            
            // 初始化侧边栏
            drawerPresets = findViewById(R.id.drawer_presets);
            drawerOverlay = findViewById(R.id.drawer_overlay);
            btnCloseDrawer = findViewById(R.id.btn_close_drawer);
            tvCurrentNode = findViewById(R.id.tv_current_node);
            
            // 初始化侧边栏预设名称TextView
            drawerPresetNameViews[0] = findViewById(R.id.tv_preset_1_name);
            drawerPresetNameViews[1] = findViewById(R.id.tv_preset_2_name);
        drawerPresetNameViews[2] = findViewById(R.id.tv_preset_3_name);
        drawerPresetNameViews[3] = findViewById(R.id.tv_preset_4_name);
        drawerPresetNameViews[4] = findViewById(R.id.tv_preset_5_name);
        drawerPresetNameViews[5] = findViewById(R.id.tv_preset_6_name);
        
        // 初始化侧边栏预设按钮
        drawerLoadButtons[0] = findViewById(R.id.btn_drawer_preset_1_load);
        drawerLoadButtons[1] = findViewById(R.id.btn_drawer_preset_2_load);
        drawerLoadButtons[2] = findViewById(R.id.btn_drawer_preset_3_load);
        drawerLoadButtons[3] = findViewById(R.id.btn_drawer_preset_4_load);
        drawerLoadButtons[4] = findViewById(R.id.btn_drawer_preset_5_load);
        drawerLoadButtons[5] = findViewById(R.id.btn_drawer_preset_6_load);
        
        drawerSaveButtons[0] = findViewById(R.id.btn_drawer_preset_1_save);
        drawerSaveButtons[1] = findViewById(R.id.btn_drawer_preset_2_save);
        drawerSaveButtons[2] = findViewById(R.id.btn_drawer_preset_3_save);
        drawerSaveButtons[3] = findViewById(R.id.btn_drawer_preset_4_save);
        drawerSaveButtons[4] = findViewById(R.id.btn_drawer_preset_5_save);
        drawerSaveButtons[5] = findViewById(R.id.btn_drawer_preset_6_save);
        
        // 初始化侧边栏重置按钮
        drawerResetButtons[0] = findViewById(R.id.btn_reset_preset_1);
        drawerResetButtons[1] = findViewById(R.id.btn_reset_preset_2);
        drawerResetButtons[2] = findViewById(R.id.btn_reset_preset_3);
        drawerResetButtons[3] = findViewById(R.id.btn_reset_preset_4);
        drawerResetButtons[4] = findViewById(R.id.btn_reset_preset_5);
        drawerResetButtons[5] = findViewById(R.id.btn_reset_preset_6);
        
        // 隐藏添加/删除按钮，因为固定10个频段
        btnAddBand.setVisibility(View.GONE);
        btnRemoveBand.setVisibility(View.GONE);

        // 设置节点选择器
        ArrayAdapter<String> nodeAdapter = new ArrayAdapter<>(this,
            R.layout.spinner_item, NODE_NAMES);
        nodeAdapter.setDropDownViewResource(R.layout.spinner_dropdown_item);
        spinnerNode.setAdapter(nodeAdapter);

        // 设置滤波器类型选择器
        ArrayAdapter<String> filterAdapter = new ArrayAdapter<>(this,
            R.layout.spinner_item, FILTER_TYPES);
        filterAdapter.setDropDownViewResource(R.layout.spinner_dropdown_item);
        spinnerFilterType.setAdapter(filterAdapter);
        } catch (Exception e) {
            android.util.Log.e("EQ_INIT", "initViews error", e);
            throw new RuntimeException("UI initialization failed: " + e.getMessage(), e);
        }
    }

    /**
     * 设置监听器
     */
    private void setupListeners() {
        // 保存按钮
        btnSave.setOnClickListener(v -> saveEqSettings());

        // 导入EQ数据按钮
        btnImportEq.setOnClickListener(v -> importEqData());

        // 导出EQ数据按钮
        btnExportEq.setOnClickListener(v -> exportEqData());

        // 节点选择
        spinnerNode.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                currentNode = position;
                currentPreset = -1; // 重置预设选择
                loadCurrentNodeBands();
                if (tvCurrentNode != null) {
                    tvCurrentNode.setText("当前节点：" + NODE_NAMES[currentNode]);
                }
                updateDrawerPresetButtons(); // 更新侧边栏预设按钮状态
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });

        // 频段选择 - 在setupListeners中设置初始监听器
        spinnerBand.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                android.util.Log.d("EqControl", "Band spinner onItemSelected (initial): position=" + position);
                currentBand = position;
                loadCurrentBandParameters();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });

        // 滤波器类型选择
        spinnerFilterType.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                updateBandParameter(0, position); // type
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });

        // 频段启用开关
        switchBandEnable.setOnCheckedChangeListener((buttonView, isChecked) -> {
            updateBandParameter(1, isChecked ? 1 : 0); // enable
            // 更新曲线视图（重新加载启用的频段）
            List<EqCurveView.EqBand> bands = nodesBands.get(currentNode);
            List<EqCurveView.EqBand> enabledBands = new ArrayList<>();
            for (EqCurveView.EqBand band : bands) {
                if (band.enable) {
                    enabledBands.add(band);
                }
            }
            eqCurveView.setBands(enabledBands);
        });

        // 频率 SeekBar
        seekbarFreq.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    float freq = freqFromProgress(progress);
                    textFreqValue.setText(String.format("%.0f Hz", freq));
                    // 只更新显示，不发送命令
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                // 松手时发送最终值
                float freq = freqFromProgress(seekBar.getProgress());
                updateBandParameter(2, freq); // f0
            }
        });

        // Q 值 SeekBar
        seekbarQ.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    float q = qFromProgress(progress);
                    textQValue.setText(String.format("%.2f", q));
                    // 只更新显示，不发送命令
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                // 松手时发送最终值
                float q = qFromProgress(seekBar.getProgress());
                updateBandParameter(3, q); // Q
            }
        });

        // 增益 SeekBar
        seekbarGain.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    float gain = gainFromProgress(progress);
                    textGainValue.setText(String.format("%.1f dB", gain));
                    // 只更新显示，不发送命令
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                // 松手时发送最终值
                float gain = gainFromProgress(seekBar.getProgress());
                updateBandParameter(4, gain); // gain
            }
        });

        // 同步EQ按钮
        btnSyncEq.setOnClickListener(v -> syncCurrentNodeEq());
        
        // 重置EQ按钮
        btnResetEq.setOnClickListener(v -> resetCurrentNodeEq());
        
        // 设置侧边栏按钮监听器
        btnOpenPresets.setOnClickListener(v -> openDrawer());
        btnCloseDrawer.setOnClickListener(v -> closeDrawer());
        drawerOverlay.setOnClickListener(v -> closeDrawer());
        
        // 设置侧边栏预设按钮监听器
        for (int i = 0; i < 6; i++) {
            final int index = i;
            drawerLoadButtons[i].setOnClickListener(v -> loadPreset(index));
            drawerSaveButtons[i].setOnClickListener(v -> savePreset(index));
            drawerResetButtons[i].setOnClickListener(v -> resetPreset(index));
            drawerPresetNameViews[i].setOnClickListener(v -> editPresetName(index));
        }
        
        updateDrawerPresetButtons();
    }

    /**
     * 加载当前节点的频段数据
     */
    private void loadCurrentNodeBands() {
        List<EqCurveView.EqBand> bands = nodesBands.get(currentNode);
        
        // 只将启用的频段传递给曲线视图（优化：避免频繁创建新列表）
        List<EqCurveView.EqBand> enabledBands = new ArrayList<>(bands.size());
        for (EqCurveView.EqBand band : bands) {
            if (band.enable) {
                enabledBands.add(band);
            }
        }
        eqCurveView.setBands(enabledBands);

        // 更新频段选择器（显示所有10个频段）
        updateBandSpinner();

        // 加载第一个频段的参数
        if (!bands.isEmpty()) {
            currentBand = 0;
            spinnerBand.setSelection(0);
            loadCurrentBandParameters();
        }
    }

    /**
     * 更新频段选择器
     */
    private void updateBandSpinner() {
        List<EqCurveView.EqBand> bands = nodesBands.get(currentNode);
        
        // 优化：使用固定数组避免重复创建String
        String[] bandNames = new String[bands.size()];
        for (int i = 0; i < bands.size(); i++) {
            bandNames[i] = "Band " + (i + 1);
        }

        android.util.Log.d("EqControl", "updateBandSpinner: creating " + bandNames.length + " band names");
        
        // 暂时移除监听器
        spinnerBand.setOnItemSelectedListener(null);
        
        ArrayAdapter<String> bandAdapter = new ArrayAdapter<>(this,
            R.layout.spinner_item, bandNames);
        bandAdapter.setDropDownViewResource(R.layout.spinner_dropdown_item);
        spinnerBand.setAdapter(bandAdapter);
        
        // 恢复监听器
        spinnerBand.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                android.util.Log.d("EqControl", "Band spinner onItemSelected: position=" + position);
                currentBand = position;
                loadCurrentBandParameters();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
        
        android.util.Log.d("EqControl", "updateBandSpinner: adapter set and listener restored");
    }

    /**
     * 加载当前频段的参数到 UI
     */
    private void loadCurrentBandParameters() {
        if (currentBand >= nodesBands.get(currentNode).size()) return;

        EqCurveView.EqBand band = nodesBands.get(currentNode).get(currentBand);

        // 更新 UI 控件
        switchBandEnable.setChecked(band.enable);
        spinnerFilterType.setSelection(band.type);

        // 频率
        seekbarFreq.setProgress(progressFromFreq(band.f0));
        textFreqValue.setText(String.format("%.0f Hz", band.f0));

        // Q 值
        seekbarQ.setProgress(progressFromQ(band.Q));
        textQValue.setText(String.format("%.2f", band.Q));

        // 增益
        seekbarGain.setProgress(progressFromGain(band.gain));
        textGainValue.setText(String.format("%.1f dB", band.gain));

        // 设置曲线视图的选中频段
        eqCurveView.setSelectedBand(currentBand);
    }

    /**
     * 更新频段参数
     * @param paramType 0:type, 1:enable, 2:f0, 3:Q, 4:gain
     * @param value 参数值
     */
    private void updateBandParameter(int paramType, float value) {
        if (currentBand >= nodesBands.get(currentNode).size()) return;

        EqCurveView.EqBand band = nodesBands.get(currentNode).get(currentBand);
        boolean needUpdate = false;

        switch (paramType) {
            case 0: // type
                if (band.type != (int) value) {
                    band.type = (int) value;
                    needUpdate = true;
                }
                break;
            case 1: // enable
                boolean newEnable = value > 0;
                if (band.enable != newEnable) {
                    band.enable = newEnable;
                    needUpdate = true;
                }
                break;
            case 2: // f0
                if (Math.abs(band.f0 - value) > 1) { // 差异大于 1Hz 才更新
                    band.f0 = value;
                    needUpdate = true;
                }
                break;
            case 3: // Q
                if (Math.abs(band.Q - value) > 0.01f) {
                    band.Q = value;
                    needUpdate = true;
                }
                break;
            case 4: // gain
                // 限制增益范围在-12到+12dB之间
                float clampedGain = Math.max(-12.0f, Math.min(12.0f, value));
                if (Math.abs(band.gain - clampedGain) > 0.1f) {
                    band.gain = clampedGain;
                    needUpdate = true;
                }
                break;
        }

        // 只有当参数确实变化时才更新和发送
        if (needUpdate) {
            // 更新曲线视图
            eqCurveView.updateBand(currentBand, band);

            // 发送蓝牙命令
            sendSingleEqParameter(currentNode, currentBand, band, paramType);
        }
    }

    /**
     * 保存 EQ 设置到 SharedPreferences 和设备
     */
    private void saveEqSettings() {
        // 保存到 SharedPreferences
        saveEqConfigToPreferences();

        // 发送保存命令到设备
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            // 直接发送 chain -S 保存命令
            bluetoothHelper.writeCharacteristic(
                "0000ab01-0000-1000-8000-00805f9b34fb",
                "chain -S\r\n".getBytes(),
                success -> runOnUiThread(() -> {
                    if (success) {
                        Toast.makeText(this, "EQ 设置已保存到设备", Toast.LENGTH_LONG).show();
                    } else {
                        Toast.makeText(this, "保存到设备失败", Toast.LENGTH_SHORT).show();
                    }
                })
            );
        } else {
            Toast.makeText(this, "已保存到本地（设备未连接）", Toast.LENGTH_SHORT).show();
        }
    }

    // 命令队列和 Handler
    private android.os.Handler commandHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private java.util.Queue<Runnable> commandQueue = new java.util.LinkedList<>();
    private boolean isSendingCommand = false;
    
    // 队列大小限制
    private static final int MAX_QUEUE_SIZE = 200;  // 最大队列大小

    /**
     * 发送单个 EQ 参数
     */
    /**
     * 发送单个 EQ 参数
     * 2026-02-04 更新: 使用 effect set 命令，基于 Effect ID
     */
    private void sendSingleEqParameter(int node, int band, EqCurveView.EqBand eqBand, int paramType) {
        if (bluetoothHelper == null || !bluetoothHelper.isConnected()) {
            return;
        }

        int effectId = EFFECT_IDS[node];
        String command = null;

        switch (paramType) {
            case 0: // type
                command = String.format("fx %d band%d_type %d\r\n", effectId, band, eqBand.type);
                break;
            case 1: // enable
                command = String.format("fx %d band%d_enable %d\r\n", effectId, band, eqBand.enable ? 1 : 0);
                break;
            case 2: // f0
                command = String.format("fx %d band%d_f0 %.0f\r\n", effectId, band, eqBand.f0);
                break;
            case 3: // Q
                command = String.format("fx %d band%d_Q %.0f\r\n", effectId, band, eqBand.Q * 100);
                break;
            case 4: // gain
                // 增益精度0.1dB，发送时 1dB=10
                float clampedGain = Math.max(-12.0f, Math.min(12.0f, eqBand.gain));
                command = String.format("fx %d band%d %.0f\r\n", effectId, band, clampedGain * 10);
                break;
        }

        if (command != null) {
            sendBatchBleCommands(command);
        }
    }

    /**
     * 发送 EQ 命令到设备（带队列机制）
     * 注意：enable 命令会触发固件自动更新 filter_count 并重新配置滤波器，所以必须放在最后
     */
    /**
     * 发送 EQ 命令到设备
     * 2026-02-04 更新: 使用 effect set 命令，基于 Effect ID
     */
    private void sendEqCommand(int node, int band, EqCurveView.EqBand eqBand) {
        if (bluetoothHelper == null || !bluetoothHelper.isConnected()) {
            return;
        }

        // 使用效果器 ID (4=乐器L, 5=乐器R, 6=麦克风L, 7=麦克风R, 14=USB/BT)
        int effectId = EFFECT_IDS[node];
        
        // 只发送两条命令：增益参数 + enable
        StringBuilder commands = new StringBuilder();
        
        // 设置增益
        commands.append(String.format("fx %d band%d %.0f\r\n", 
            effectId, band, eqBand.gain * 10));
        
        // 设置 enable（固件会自动更新 filter_count 并重新配置滤波器系数）
        commands.append(String.format("fx %d band%d_enable %d\r\n", 
            effectId, band, eqBand.enable ? 1 : 0));
        
        // 一次性发送命令
        sendBatchBleCommands(commands.toString());
    }

    /**
     * 将命令加入队列
     */
    private void queueCommand(Runnable command) {
        // 检查队列大小限制
        if (commandQueue.size() >= MAX_QUEUE_SIZE) {
            android.util.Log.w("EQ_QUEUE", "⚠️ 队列已满(" + commandQueue.size() + "/" + MAX_QUEUE_SIZE + ")，丢弃旧命令");
            // 移除最旧的命令为新命令腾出空间
            commandQueue.poll();
        }
        
        commandQueue.offer(command);
        android.util.Log.d("EQ_QUEUE", "命令加入队列，当前队列大小: " + commandQueue.size() + ", isSendingCommand: " + isSendingCommand);
        processNextCommand();
    }

    /**
     * 处理下一个命令（只写模式，不等待回复）
     */
    private void processNextCommand() {
        if (isSendingCommand) {
            android.util.Log.d("EQ_QUEUE", "正在发送命令，跳过处理");
            return;
        }
        
        if (commandQueue.isEmpty()) {
            android.util.Log.d("EQ_QUEUE", "队列为空，处理完成");
            return;
        }

        isSendingCommand = true;
        Runnable command = commandQueue.poll();
        android.util.Log.d("EQ_QUEUE", "开始处理命令，剩余队列大小: " + commandQueue.size());
        
        if (command != null) {
            command.run();
        } else {
            android.util.Log.e("EQ_QUEUE", "命令为null，重置状态");
            isSendingCommand = false;
        }
    }

    private void sendBleCommand(String command) {
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            String fullCommand = command + "\r\n";
            android.util.Log.d("EQ_QUEUE", "发送命令: " + command);
            
            bluetoothHelper.writeCharacteristic(
                "0000ab01-0000-1000-8000-00805f9b34fb",
                fullCommand.getBytes(),
                success -> {
                    if (success) {
                        android.util.Log.d("EQ_QUEUE", "✓ 命令发送成功: " + command);
                    } else {
                        android.util.Log.e("EQ_QUEUE", "✗ 命令发送失败: " + command);
                    }
                    // [只写模式] 不等待下位机回复，200ms延迟避免BLE拥塞
                    commandHandler.postDelayed(() -> {
                        isSendingCommand = false;
                        processNextCommand();
                    }, 200);
                }
            );
        } else {
            android.util.Log.e("EQ_QUEUE", "蓝牙未连接，跳过命令: " + command);
            isSendingCommand = false;
            processNextCommand();
        }
    }

    /**
     * 批量发送 BLE 命令（多条命令合并成一个字符串）
     */
    private void sendBatchBleCommands(String batchCommands) {
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            int cmdCount = batchCommands.split("\r\n").length;
            android.util.Log.d("EQ_BATCH", "准备发送 " + cmdCount + " 条命令");
            android.util.Log.d("EQ_BATCH", "命令内容: " + batchCommands.replace("\r\n", " | "));
            
            bluetoothHelper.writeCharacteristic(
                "0000ab01-0000-1000-8000-00805f9b34fb",
                batchCommands.getBytes(),
                success -> {
                    if (!success) {
                        android.util.Log.e("EQ_BATCH", "❌ 批量命令发送失败");
                        runOnUiThread(() -> 
                            Toast.makeText(this, "命令发送失败，请检查蓝牙连接", Toast.LENGTH_SHORT).show()
                        );
                    } else {
                        android.util.Log.d("EQ_BATCH", "✓ 批量命令已发送到设备");
                        runOnUiThread(() -> 
                            Toast.makeText(this, "同步完成", Toast.LENGTH_SHORT).show()
                        );
                    }
                }
            );
        } else {
            android.util.Log.e("EQ_BATCH", "蓝牙未连接，无法发送命令");
            Toast.makeText(this, "蓝牙未连接", Toast.LENGTH_SHORT).show();
        }
    }

    // 工具方法：进度条与参数转换

    private float freqFromProgress(int progress) {
        // 20Hz - 20kHz 对数映射
        float minFreq = 20f;
        float maxFreq = 20000f;
        float ratio = progress / 100f;
        return (float) (minFreq * Math.pow(maxFreq / minFreq, ratio));
    }

    private int progressFromFreq(float freq) {
        float minFreq = 20f;
        float maxFreq = 20000f;
        float ratio = (float) (Math.log(freq / minFreq) / Math.log(maxFreq / minFreq));
        return (int) (ratio * 100);
    }

    private float qFromProgress(int progress) {
        // 0.1 - 50.0
        return 0.1f + (progress / 490f) * 49.9f;
    }

    private int progressFromQ(float q) {
        return (int) ((q - 0.1f) / 49.9f * 490);
    }

    private float gainFromProgress(int progress) {
        // -12dB - +12dB，240个步进，每步0.1dB
        return (progress - 120) * 0.1f;
    }

    private int progressFromGain(float gain) {
        // 将增益转换为progress（0-240）
        return (int) ((gain + 12.0f) * 10);
    }

    /**
     * 保存 EQ 配置到 SharedPreferences
     */
    private void saveEqConfigToPreferences() {
        android.content.SharedPreferences prefs = getSharedPreferences("EqConfig", MODE_PRIVATE);
        android.content.SharedPreferences.Editor editor = prefs.edit();

        // 保存所有节点的配置
        for (int node = 0; node < nodesBands.size(); node++) {
            List<EqCurveView.EqBand> bands = nodesBands.get(node);
            editor.putInt("node_" + node + "_band_count", bands.size());

            for (int band = 0; band < bands.size(); band++) {
                EqCurveView.EqBand eqBand = bands.get(band);
                String prefix = "node_" + node + "_band_" + band;
                editor.putBoolean(prefix + "_enable", eqBand.enable);
                editor.putInt(prefix + "_type", eqBand.type);
                editor.putFloat(prefix + "_f0", eqBand.f0);
                editor.putFloat(prefix + "_Q", eqBand.Q);
                editor.putFloat(prefix + "_gain", eqBand.gain);
            }
        }

        editor.apply();
    }

    /**
     * 从 SharedPreferences 加载 EQ 配置
     */
    private void loadEqConfigFromPreferences() {
        android.content.SharedPreferences prefs = getSharedPreferences("EqConfig", MODE_PRIVATE);

        // 加载所有节点的配置
        for (int node = 0; node < nodesBands.size(); node++) {
            int bandCount = prefs.getInt("node_" + node + "_band_count", -1);
            if (bandCount <= 0) {
                // 没有保存的配置，保持默认的1个频段
                continue;
            }

            List<EqCurveView.EqBand> bands = nodesBands.get(node);
            bands.clear();

            for (int band = 0; band < bandCount; band++) {
                String prefix = "node_" + node + "_band_" + band;
                boolean enable = prefs.getBoolean(prefix + "_enable", true);
                int type = prefs.getInt(prefix + "_type", 0);
                float f0 = prefs.getFloat(prefix + "_f0", 1000);
                float Q = prefs.getFloat(prefix + "_Q", 1.0f);
                float gain = prefs.getFloat(prefix + "_gain", 0);

                bands.add(new EqCurveView.EqBand(enable, type, f0, Q, gain));
            }
        }
    }

    /**
     * 从设备读取 EQ 配置（可选功能）
     */
    private void loadEqConfigFromDevice() {
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            // 发送读取命令
            bluetoothHelper.writeCharacteristic(
                "0000ab01-0000-1000-8000-00805f9b34fb",
                "eq_get\r\n".getBytes(),
                success -> {
                    if (success) {
                        Toast.makeText(this, "正在从设备读取配置...", Toast.LENGTH_SHORT).show();
                    }
                }
            );
        }
    }
    
    /**
     * 同步当前节点的全部EQ信息到设备
     */
    private void syncCurrentNodeEq() {
        if (bluetoothHelper == null || !bluetoothHelper.isConnected()) {
            Toast.makeText(this, "请先连接蓝牙设备", Toast.LENGTH_SHORT).show();
            return;
        }
        
        List<EqCurveView.EqBand> bands = nodesBands.get(currentNode);
        if (bands.isEmpty()) {
            Toast.makeText(this, "当前节点没有频段", Toast.LENGTH_SHORT).show();
            return;
        }
        
        // 发送当前节点的所有频段配置（包括禁用的频段）
        int effectId = EFFECT_IDS[currentNode];
        
        // 构建所有命令（每个band的5个参数合并，用换行符分隔）
        java.util.List<String> commandList = new java.util.ArrayList<>();
        for (int band = 0; band < 10; band++) {
            EqCurveView.EqBand eqBand = bands.get(band);
            
            // 将每个band的5个参数合并成一条命令串，用\r\n分隔每条命令
            String combinedCmd = String.format(
                "fx %d band%d_type %d\r\nfx %d band%d_f0 %.0f\r\nfx %d band%d_Q %.0f\r\nfx %d band%d %.0f\r\nfx %d band%d_enable %d\r\n",
                effectId, band, eqBand.type,
                effectId, band, eqBand.f0,
                effectId, band, eqBand.Q * 100,
                effectId, band, eqBand.gain * 10,
                effectId, band, eqBand.enable ? 1 : 0
            );
            commandList.add(combinedCmd);
        }
        
        android.util.Log.d("EQ_SYNC", "========== 开始同步 ==========");
        android.util.Log.d("EQ_SYNC", "节点: " + NODE_NAMES[currentNode] + " (EffectId=" + effectId + ")");
        android.util.Log.d("EQ_SYNC", "命令总数: " + commandList.size());
        android.util.Log.d("EQ_SYNC", "当前队列状态 - 大小: " + commandQueue.size() + ", isSendingCommand: " + isSendingCommand);
        
        Toast.makeText(this, "正在同步节点 " + NODE_NAMES[currentNode] + " 的全部EQ参数...", Toast.LENGTH_LONG).show();
        
        // 使用队列机制逐条发送命令
        for (int i = 0; i < commandList.size(); i++) {
            final String cmd = commandList.get(i);
            final int cmdIndex = i + 1;
            queueCommand(() -> {
                android.util.Log.d("EQ_SYNC", "执行命令 " + cmdIndex + "/" + commandList.size() + ": " + cmd);
                sendBleCommand(cmd);
            });
        }
        
        // 最后一条命令发送后显示完成提示
        queueCommand(() -> {
            android.util.Log.d("EQ_SYNC", "所有EQ命令已发送完成");
            runOnUiThread(() -> 
                Toast.makeText(this, "同步完成", Toast.LENGTH_SHORT).show()
            );
            // 重置标志，避免阻塞后续命令
            isSendingCommand = false;
            processNextCommand();
        });
    }

    /**
     * 导出EQ数据到下位机（使用原有的同步功能）
     */
    private void exportEqData() {
        android.util.Log.d("EQ_EXPORT", "导出EQ数据到下位机 - 节点: " + NODE_NAMES[currentNode]);
        syncCurrentNodeEq();
    }

    /**
     * 从下位机导入EQ数据
     */
    private void importEqData() {
        if (bluetoothHelper == null || !bluetoothHelper.isConnected()) {
            Toast.makeText(this, "请先连接蓝牙设备", Toast.LENGTH_SHORT).show();
            return;
        }

        android.util.Log.d("EQ_IMPORT", "从下位机导入EQ数据 - 节点: " + NODE_NAMES[currentNode]);

        // 发送查询命令获取当前节点的EQ参数
        int effectId = EFFECT_IDS[currentNode];
        String queryCmd = String.format("param effect %d -q\r\n", effectId);

        Toast.makeText(this, "正在从下位机读取节点 " + NODE_NAMES[currentNode] + " 的EQ数据...", Toast.LENGTH_LONG).show();

        // 发送查询命令
        sendBleCommand(queryCmd);

        android.util.Log.d("EQ_IMPORT", "发送查询命令: " + queryCmd);
    }
    
    /**
     * 重置当前节点的EQ参数
     */
    private void resetCurrentNodeEq() {
        new android.app.AlertDialog.Builder(this)
            .setTitle("重置EQ")
            .setMessage("确定要重置当前节点 \"" + NODE_NAMES[currentNode] + "\" 的所有EQ参数吗？\n\n所有频段将恢复默认值。")
            .setPositiveButton("重置", (dialog, which) -> {
                // 预定义频率分布
                float[] defaultFreqs = {32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
                
                List<EqCurveView.EqBand> bands = nodesBands.get(currentNode);
                bands.clear();
                
                // 重新创建10个默认频段
                for (int j = 0; j < 10; j++) {
                    bands.add(new EqCurveView.EqBand(
                        j == 0,  // 只有第一个频段默认启用
                        0,       // type = Peaking
                        defaultFreqs[j],
                        1.0f,    // Q = 1.0
                        0        // gain = 0dB
                    ));
                }
                
                // 重置预设选择
                currentPreset = -1;
                
                // 刷新UI
                loadCurrentNodeBands();
                
                // 保存到本地
                saveEqConfigToPreferences();
                
                Toast.makeText(this, "已重置 " + NODE_NAMES[currentNode] + " 的EQ参数", Toast.LENGTH_SHORT).show();
                
                android.util.Log.d("EQ_RESET", "节点 " + NODE_NAMES[currentNode] + " 已重置为默认参数");
            })
            .setNegativeButton("取消", null)
            .show();
    }
    
    /**
     * 重置指定的预设槽位
     */
    private void resetPreset(int presetIndex) {
        String[] presetNames = nodePresetNames.get(currentNode);
        String presetName = presetNames[presetIndex];
        
        new android.app.AlertDialog.Builder(this)
            .setTitle("重置预设")
            .setMessage("确定要清空预设 \"" + presetName + "\" 吗？\n\n此操作不可恢复！")
            .setPositiveButton("清空", (dialog, which) -> {
                // 清空指定预设槽位
                List<List<EqCurveView.EqBand>> presets = nodePresets.get(currentNode);
                presets.set(presetIndex, null);
                
                // 重置预设名称为默认值
                presetNames[presetIndex] = "预设 " + (presetIndex + 1);
                
                // 如果清空的是当前选中的预设，重置选中状态
                if (currentPreset == presetIndex) {
                    currentPreset = -1;
                }
                
                // 更新UI
                updateDrawerPresetButtons();
                
                // 保存到SharedPreferences
                savePresetsToPreferences();
                savePresetNamesToPreferences();
                
                Toast.makeText(this, "已清空预设 " + (presetIndex + 1), Toast.LENGTH_SHORT).show();
                
                android.util.Log.d("EQ_RESET", "节点 " + NODE_NAMES[currentNode] + " 的预设 " + (presetIndex + 1) + " 已清空");
            })
            .setNegativeButton("取消", null)
            .show();
    }
    
    /**
     * 打开预设侧边栏
     */
    private void openDrawer() {
        drawerPresets.setVisibility(View.VISIBLE);
        drawerOverlay.setVisibility(View.VISIBLE);
        tvCurrentNode.setText("当前节点：" + NODE_NAMES[currentNode]);
        updateDrawerPresetButtons();
        
        // 添加滑入动画
        drawerPresets.setTranslationX(drawerPresets.getWidth());
        drawerPresets.animate().translationX(0).setDuration(250).start();
    }
    
    /**
     * 关闭预设侧边栏
     */
    private void closeDrawer() {
        drawerPresets.animate().translationX(drawerPresets.getWidth()).setDuration(250)
            .withEndAction(() -> {
                drawerPresets.setVisibility(View.GONE);
                drawerOverlay.setVisibility(View.GONE);
            }).start();
    }
    
    /**
     * 更新侧边栏预设按钮状态
     */
    private void updateDrawerPresetButtons() {
        List<List<EqCurveView.EqBand>> presets = nodePresets.get(currentNode);
        String[] presetNames = nodePresetNames.get(currentNode);
        for (int i = 0; i < 6; i++) {
            boolean hasPreset = presets.get(i) != null;
            // 更新预设名称
            drawerPresetNameViews[i].setText(presetNames[i]);
            // 设置按钮是否可用
            drawerLoadButtons[i].setEnabled(hasPreset);
            drawerLoadButtons[i].setAlpha(hasPreset ? 1.0f : 0.5f);
        }
    }
    
    /**
     * 编辑预设名称
     */
    private void editPresetName(int presetIndex) {
        String[] presetNames = nodePresetNames.get(currentNode);
        String currentName = presetNames[presetIndex];
        
        // 创建输入框
        android.widget.EditText input = new android.widget.EditText(this);
        input.setText(currentName);
        input.setSelection(currentName.length());
        input.setSingleLine(true);
        input.setHint("请输入预设名称");
        
        new android.app.AlertDialog.Builder(this)
            .setTitle("编辑预设名称")
            .setView(input)
            .setPositiveButton("确定", (dialog, which) -> {
                String newName = input.getText().toString().trim();
                if (!newName.isEmpty()) {
                    presetNames[presetIndex] = newName;
                    drawerPresetNameViews[presetIndex].setText(newName);
                    savePresetNamesToPreferences();
                    Toast.makeText(this, "已更新预设名称", Toast.LENGTH_SHORT).show();
                }
            })
            .setNegativeButton("取消", null)
            .show();
    }
    
    /**
     * 加载预设
     */
    private void loadPreset(int presetIndex) {
        List<List<EqCurveView.EqBand>> presets = nodePresets.get(currentNode);
        List<EqCurveView.EqBand> preset = presets.get(presetIndex);
        
        if (preset == null) {
            Toast.makeText(this, "槽位 " + (presetIndex + 1) + " 为空", Toast.LENGTH_SHORT).show();
            return;
        }
        
        // 加载预设配置到当前节点
        List<EqCurveView.EqBand> currentBands = nodesBands.get(currentNode);
        currentBands.clear();
        for (EqCurveView.EqBand band : preset) {
            // 深拷贝
            currentBands.add(new EqCurveView.EqBand(band.enable, band.type, band.f0, band.Q, band.gain));
        }
        
        currentPreset = presetIndex;
        loadCurrentNodeBands();
        updateDrawerPresetButtons();
        closeDrawer();
        Toast.makeText(this, "已加载预设 " + (presetIndex + 1), Toast.LENGTH_SHORT).show();
    }
    
    /**
     * 保存当前配置到预设
     */
    private void savePreset(int presetIndex) {
        new android.app.AlertDialog.Builder(this)
            .setTitle("保存预设")
            .setMessage("确定要保存当前配置到预设 " + (presetIndex + 1) + " 吗？")
            .setPositiveButton("保存", (dialog, which) -> {
                // 深拷贝当前配置
                List<EqCurveView.EqBand> currentBands = nodesBands.get(currentNode);
                List<EqCurveView.EqBand> preset = new ArrayList<>();
                for (EqCurveView.EqBand band : currentBands) {
                    preset.add(new EqCurveView.EqBand(band.enable, band.type, band.f0, band.Q, band.gain));
                }
                
                // 保存到预设槽位
                List<List<EqCurveView.EqBand>> presets = nodePresets.get(currentNode);
                presets.set(presetIndex, preset);
                
                currentPreset = presetIndex;
                updateDrawerPresetButtons();
                
                // 保存到SharedPreferences
                savePresetsToPreferences();
                
                Toast.makeText(this, "已保存到预设 " + (presetIndex + 1), Toast.LENGTH_LONG).show();
            })
            .setNegativeButton("取消", null)
            .show();
    }
    
    /**
     * 保存预设到SharedPreferences
     */
    private void savePresetsToPreferences() {
        android.content.SharedPreferences prefs = getSharedPreferences("EqConfig", MODE_PRIVATE);
        android.content.SharedPreferences.Editor editor = prefs.edit();
        
        // 保存所有节点的所有预设
        for (int node = 0; node < nodePresets.size(); node++) {
            List<List<EqCurveView.EqBand>> presets = nodePresets.get(node);
            for (int preset = 0; preset < presets.size(); preset++) {
                List<EqCurveView.EqBand> bands = presets.get(preset);
                
                if (bands == null) {
                    editor.putBoolean("node_" + node + "_preset_" + preset + "_exists", false);
                    continue;
                }
                
                editor.putBoolean("node_" + node + "_preset_" + preset + "_exists", true);
                editor.putInt("node_" + node + "_preset_" + preset + "_band_count", bands.size());
                
                for (int band = 0; band < bands.size(); band++) {
                    EqCurveView.EqBand eqBand = bands.get(band);
                    String prefix = "node_" + node + "_preset_" + preset + "_band_" + band;
                    editor.putBoolean(prefix + "_enable", eqBand.enable);
                    editor.putInt(prefix + "_type", eqBand.type);
                    editor.putFloat(prefix + "_f0", eqBand.f0);
                    editor.putFloat(prefix + "_Q", eqBand.Q);
                    editor.putFloat(prefix + "_gain", eqBand.gain);
                }
            }
        }
        
        editor.apply();
    }
    
    /**
     * 从SharedPreferences加载预设
     */
    private void loadPresetsFromPreferences() {
        android.content.SharedPreferences prefs = getSharedPreferences("EqConfig", MODE_PRIVATE);
        
        for (int node = 0; node < nodePresets.size(); node++) {
            List<List<EqCurveView.EqBand>> presets = nodePresets.get(node);
            for (int preset = 0; preset < presets.size(); preset++) {
                boolean exists = prefs.getBoolean("node_" + node + "_preset_" + preset + "_exists", false);
                
                if (!exists) {
                    presets.set(preset, null);
                    continue;
                }
                
                int bandCount = prefs.getInt("node_" + node + "_preset_" + preset + "_band_count", 0);
                List<EqCurveView.EqBand> bands = new ArrayList<>();
                
                for (int band = 0; band < bandCount; band++) {
                    String prefix = "node_" + node + "_preset_" + preset + "_band_" + band;
                    boolean enable = prefs.getBoolean(prefix + "_enable", false);
                    int type = prefs.getInt(prefix + "_type", 0);
                    float f0 = prefs.getFloat(prefix + "_f0", 1000);
                    float Q = prefs.getFloat(prefix + "_Q", 1.0f);
                    float gain = prefs.getFloat(prefix + "_gain", 0);
                    
                    bands.add(new EqCurveView.EqBand(enable, type, f0, Q, gain));
                }
                
                presets.set(preset, bands);
            }
        }
        
        // 加载预设名称
        loadPresetNamesFromPreferences();
    }
    
    /**
     * 保存预设名称到SharedPreferences
     */
    private void savePresetNamesToPreferences() {
        android.content.SharedPreferences prefs = getSharedPreferences("EqConfig", MODE_PRIVATE);
        android.content.SharedPreferences.Editor editor = prefs.edit();
        
        for (int node = 0; node < nodePresetNames.size(); node++) {
            String[] names = nodePresetNames.get(node);
            for (int preset = 0; preset < names.length; preset++) {
                editor.putString("node_" + node + "_preset_" + preset + "_name", names[preset]);
            }
        }
        
        editor.apply();
    }
    
    /**
     * 从SharedPreferences加载预设名称
     */
    private void loadPresetNamesFromPreferences() {
        android.content.SharedPreferences prefs = getSharedPreferences("EqConfig", MODE_PRIVATE);
        
        for (int node = 0; node < nodePresetNames.size(); node++) {
            String[] names = nodePresetNames.get(node);
            for (int preset = 0; preset < names.length; preset++) {
                String defaultName = "预设 " + (preset + 1);
                names[preset] = prefs.getString("node_" + node + "_preset_" + preset + "_name", defaultName);
            }
        }
    }

    /**
     * 设置BLE通知监听器
     */
    private void setupBleNotificationListener() {
        if (bluetoothHelper != null) {
            bluetoothHelper.setBleNotifyListener(data -> {
                android.util.Log.d("EqControl", "BLE notify received: " + data);
                // 处理EQ查询响应 - 现在使用二进制格式而不是JSON
                try {
                    // 尝试解析二进制数据
                    if (parseBinaryEqData(data)) {
                        android.util.Log.d("EqControl", "Successfully parsed binary EQ data");
                        Toast.makeText(this, "EQ参数已同步", Toast.LENGTH_SHORT).show();
                    } else {
                        android.util.Log.w("EqControl", "Failed to parse binary EQ data, trying JSON fallback");
                        // 回退到JSON解析（用于兼容性）
                        parseJsonEqData(data);
                    }
                } catch (Exception e) {
                    android.util.Log.e("EqControl", "Failed to parse EQ data: " + data, e);
                }
            });
        }
    }

    /**
     * 更新EQ UI
     */
    private void updateEqUI(org.json.JSONObject effectObj) {
        android.util.Log.d("EqControl", "updateEqUI called with: " + effectObj.toString());
        runOnUiThread(() -> {
            try {
                android.util.Log.d("EqControl", "Running on UI thread");

                // 检查是否有params字段
                if (effectObj.has("params")) {
                    org.json.JSONObject paramsObj = effectObj.getJSONObject("params");
                    android.util.Log.d("EqControl", "Parsed params object: " + paramsObj.toString());

                    // 处理EQ参数
                    if (paramsObj.has("eq")) {
                        org.json.JSONObject eqObj = paramsObj.getJSONObject("eq");
                        updateEqParams(eqObj);
                    } else {
                        android.util.Log.w("EqControl", "No eq field in params object");
                    }

                    Toast.makeText(this, "EQ参数已同步", Toast.LENGTH_SHORT).show();
                    android.util.Log.d("EqControl", "UI update completed");
                } else {
                    android.util.Log.w("EqControl", "No params field in effect object");
                }
            } catch (org.json.JSONException e) {
                android.util.Log.e("EqControl", "Failed to update EQ UI", e);
                Toast.makeText(this, "解析EQ数据失败", Toast.LENGTH_SHORT).show();
            }
        });
    }

    /**
     * 更新EQ参数
     */
    private void updateEqParams(org.json.JSONObject eqObj) {
        try {
            // 获取band_count
            int bandCount = eqObj.getInt("band_count");
            android.util.Log.d("EqControl", "Band count: " + bandCount);

            // 获取pregain
            int pregain = eqObj.getInt("pregain");
            android.util.Log.d("EqControl", "Pregain: " + pregain);

            // 获取bands数组
            if (eqObj.has("bands")) {
                org.json.JSONArray bandsArray = eqObj.getJSONArray("bands");
                android.util.Log.d("EqControl", "Bands array length: " + bandsArray.length());

                // 清空当前节点的频段数据
                List<EqCurveView.EqBand> currentBands = nodesBands.get(currentNode);
                currentBands.clear();

                // 解析每个频段
                for (int i = 0; i < bandsArray.length() && i < bandCount; i++) {
                    org.json.JSONObject bandObj = bandsArray.getJSONObject(i);

                    EqCurveView.EqBand band = new EqCurveView.EqBand();
                    band.gain = bandObj.getInt("gain");
                    band.f0 = bandObj.getInt("f0");
                    band.Q = bandObj.getInt("Q") / 100.0f; // Q值在固件中是整数，需要转换为浮点数
                    band.type = bandObj.getInt("type");
                    band.enable = bandObj.getBoolean("enabled");

                    currentBands.add(band);
                    android.util.Log.d("EqControl", "Added band " + i + ": gain=" + band.gain +
                                     ", f0=" + band.f0 + ", Q=" + band.Q + ", type=" + band.type +
                                     ", enable=" + band.enable);
                }

                // 更新曲线视图
                eqCurveView.setBands(currentBands);
                eqCurveView.invalidate();

                // 更新频段选择器
                updateBandSpinner();

                // 如果有选中的频段，更新UI控件
                if (currentBand >= 0 && currentBand < currentBands.size()) {
                    loadCurrentBandParameters();
                }

                android.util.Log.d("EqControl", "EQ parameters updated successfully");
            } else {
                android.util.Log.w("EqControl", "No bands array in EQ object");
            }
        } catch (org.json.JSONException e) {
            android.util.Log.e("EqControl", "Failed to parse EQ params", e);
        }
    }

    /**
     * 解析二进制格式的EQ数据
     * 格式: [type(1)][length(1)][band1_freq(2)][band1_gain(2)]...[bandN_freq(2)][bandN_gain(2)]
     * type = 0x12 for EQ
     */
    private boolean parseBinaryEqData(String data) {
        try {
            // 将十六进制字符串转换为字节数组
            if (data.startsWith("0x") || data.contains(" ")) {
                // 如果是空格分隔的十六进制字符串
                String[] hexParts = data.replace("0x", "").split("\\s+");
                byte[] bytes = new byte[hexParts.length];
                for (int i = 0; i < hexParts.length; i++) {
                    bytes[i] = (byte) Integer.parseInt(hexParts[i], 16);
                }
                return parseBinaryEqData(bytes);
            } else if (data.matches("[0-9A-Fa-f]+")) {
                // 如果是连续的十六进制字符串（没有空格）
                int len = data.length();
                if (len % 2 != 0) {
                    android.util.Log.w("EqControl", "Invalid hex string length: " + len);
                    return false;
                }
                byte[] bytes = new byte[len / 2];
                for (int i = 0; i < len; i += 2) {
                    bytes[i / 2] = (byte) Integer.parseInt(data.substring(i, i + 2), 16);
                }
                return parseBinaryEqData(bytes);
            } else {
                // 如果是原始字符串，假设是字节数据
                byte[] bytes = data.getBytes();
                return parseBinaryEqData(bytes);
            }
        } catch (Exception e) {
            android.util.Log.e("EqControl", "Failed to convert data to bytes", e);
            return false;
        }
    }

    /**
     * 解析二进制字节数组格式的EQ数据
     * 支持解析多个连续的数据包
     * 固件格式: [AA][55][12][length][band_count][pregain(2)][bands...]
     * 每个band: gain(2), f0(4), Q(2), type(1), enabled(1) = 10字节
     */
    private boolean parseBinaryEqData(byte[] data) {
        if (data == null || data.length < 8) { // 需要至少8字节: header(2) + type(1) + length(1) + band_count(1) + pregain(2) + 至少一个band的部分数据
            return false;
        }

        int idx = 0;
        boolean parsedAny = false;

        // 循环解析所有数据包，直到数据处理完毕
        while (idx < data.length) {
            // 检查是否还有足够的数据用于一个完整的数据包
            if (idx + 7 >= data.length) { // 需要至少header(2) + type(1) + length(1) + band_count(1) + pregain(2)
                break;
            }

            // 检查header: 0xAA 0x55
            if (data[idx] != (byte)0xAA || data[idx + 1] != (byte)0x55) {
                android.util.Log.w("EqControl", "Invalid header at position " + idx + ": " +
                    String.format("%02X %02X", data[idx], data[idx + 1]));
                break; // 如果header不匹配，停止解析
            }

            byte type = data[idx + 2];
            if (type != 0x12) { // EQ type
                android.util.Log.w("EqControl", "Invalid type at position " + idx + ": " + String.format("%02X", type));
                break; // 如果不是EQ类型，停止解析
            }

            byte length = data[idx + 3];

            // 检查数据包长度是否合理
            int packetLength = 4 + length; // header(2) + type(1) + length(1) + data(length)
            if (idx + packetLength > data.length) {
                android.util.Log.w("EqControl", "Incomplete EQ packet at position " + idx +
                    ", need " + packetLength + " bytes, have " + (data.length - idx));
                break;
            }

            // 解析EQ数据
            int parseIdx = idx + 4; // 跳过header + type + length

            // 读取band_count
            int bandCount = data[parseIdx++] & 0xFF;

            // 读取pregain (2 bytes, little endian, signed)
            int pregain = (data[parseIdx++] & 0xFF) | ((data[parseIdx++] & 0xFF) << 8);
            if (pregain > 32767) pregain -= 65536;
            float pregainDb = pregain / 100.0f;

            // 检查是否有足够的数据用于所有bands
            int expectedDataLength = 3 + (bandCount * 10); // band_count(1) + pregain(2) + bands(10*band_count)
            if (length != expectedDataLength) {
                android.util.Log.w("EqControl", "Invalid EQ data length: expected " + expectedDataLength +
                    " bytes for " + bandCount + " bands, got " + length + " bytes at position " + idx);
                idx += packetLength; // 跳过这个包
                continue;
            }

            // 检查是否还有足够的数据
            if (parseIdx + (bandCount * 10) > idx + packetLength) {
                android.util.Log.w("EqControl", "Not enough data for " + bandCount + " EQ bands at position " + idx);
                idx += packetLength; // 跳过这个包
                continue;
            }

            // 解析所有bands
            final List<EqCurveView.EqBand> parsedBands = new ArrayList<>();
            for (int i = 0; i < bandCount; i++) {
                // 读取gain (2 bytes, little endian, signed)
                int gain = (data[parseIdx++] & 0xFF) | ((data[parseIdx++] & 0xFF) << 8);
                if (gain > 32767) gain -= 65536;
                float gainDb = gain / 100.0f;

                // 读取f0 (4 bytes, little endian)
                int f0 = (data[parseIdx++] & 0xFF) |
                        ((data[parseIdx++] & 0xFF) << 8) |
                        ((data[parseIdx++] & 0xFF) << 16) |
                        ((data[parseIdx++] & 0xFF) << 24);

                // 读取Q (2 bytes, little endian)
                int qRaw = (data[parseIdx++] & 0xFF) | ((data[parseIdx++] & 0xFF) << 8);
                float q = qRaw / 100.0f;

                // 读取type (1 byte)
                int filterType = data[parseIdx++] & 0xFF;

                // 读取enabled (1 byte)
                boolean enabled = (data[parseIdx++] & 0xFF) != 0;

                // 创建band对象
                EqCurveView.EqBand band = new EqCurveView.EqBand(enabled, filterType, f0, q, gainDb);
                parsedBands.add(band);

                android.util.Log.d("EqControl", "Parsed band " + i + ": enabled=" + enabled +
                    ", type=" + filterType + ", f0=" + f0 + "Hz, Q=" + q + ", gain=" + gainDb + "dB");
            }

            // 在UI线程中更新数据和UI
            runOnUiThread(() -> {
                try {
                    // 更新当前节点的bands
                    List<EqCurveView.EqBand> currentBands = nodesBands.get(currentNode);

                    // 只更新解析到的bands数量
                    for (int i = 0; i < parsedBands.size() && i < currentBands.size(); i++) {
                        EqCurveView.EqBand sourceBand = parsedBands.get(i);
                        EqCurveView.EqBand targetBand = currentBands.get(i);

                        // 更新所有参数
                        targetBand.enable = sourceBand.enable;
                        targetBand.type = sourceBand.type;
                        targetBand.f0 = sourceBand.f0;
                        targetBand.Q = sourceBand.Q;
                        targetBand.gain = sourceBand.gain;
                    }

                    // 刷新UI
                    loadCurrentNodeBands();
                    loadCurrentBandParameters();

                    android.util.Log.d("EqControl", "Successfully updated EQ UI with " + parsedBands.size() + " bands");

                } catch (Exception e) {
                    android.util.Log.e("EqControl", "Error updating UI with binary EQ data", e);
                }
            });

            parsedAny = true;

            // 移动到下一个数据包
            idx += packetLength;
        }

        return parsedAny;
    }

    /**
     * 解析JSON格式的EQ数据（回退方法，用于兼容性）
     */
    private void parseJsonEqData(String data) {
        try {
            org.json.JSONObject json = new org.json.JSONObject(data);

            // 检查是否是完整的effect格式
            if (json.has("effect")) {
                org.json.JSONObject effectObj = json.getJSONObject("effect");
                android.util.Log.d("EqControl", "Parsed effect object: " + effectObj.toString());
                updateEqUI(effectObj);
            }
            // 检查是否是直接的eq格式（下位机简化格式）
            else if (json.has("eq")) {
                android.util.Log.d("EqControl", "Received EQ data in simplified format");
                updateEqParams(json.getJSONObject("eq"));
                Toast.makeText(this, "EQ参数已同步", Toast.LENGTH_SHORT).show();
            }
            else {
                android.util.Log.d("EqControl", "No recognized effect field in JSON");
            }
        } catch (org.json.JSONException e) {
            android.util.Log.e("EqControl", "Failed to parse JSON EQ data: " + data, e);
        }
    }
}
