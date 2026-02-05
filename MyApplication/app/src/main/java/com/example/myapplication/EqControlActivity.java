package com.example.myapplication;

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
public class EqControlActivity extends AppCompatActivity {

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
    private Button btnSave;
    private ImageButton btnBack;

    // 数据结构
    private List<List<EqCurveView.EqBand>> nodesBands; // 每个节点的所有频段
    private int currentNode = 0; // 当前选中的节点
    private int currentBand = 0; // 当前选中的频段

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
        
        // 检查蓝牙连接状态
        if (bluetoothHelper == null || !bluetoothHelper.isConnected()) {
            Toast.makeText(this, "请先连接蓝牙设备", Toast.LENGTH_LONG).show();
            finish();
            return;
        }
        
        setContentView(R.layout.activity_eq_control);

        // 初始化数据结构
        initData();

        // 初始化 UI 控件
        initViews();

        // 设置监听器
        setupListeners();

        // 从 SharedPreferences 加载配置
        loadEqConfigFromPreferences();

        // 加载当前节点的频段数据
        loadCurrentNodeBands();
        
        // 暂时禁用查询，避免覆盖本地配置
        queryEqParams();
        
        // 调试：打印当前频段数量
        android.util.Log.d("EqControl", "Node 0 bands count: " + nodesBands.get(0).size());
        android.util.Log.d("EqControl", "Node 1 bands count: " + nodesBands.get(1).size());
    }
    
    /**
     * 查询EQ参数
     */
    private void queryEqParams() {
        // 查询所有EQ节点参数
        sendQueryCommand("graph query eq");
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
    protected void onDestroy() {
        super.onDestroy();
        // 清理命令队列
        commandHandler.removeCallbacksAndMessages(null);
        commandQueue.clear();
    }

    /**
     * 初始化数据结构
     */
    private void initData() {
        nodesBands = new ArrayList<>();
        for (int i = 0; i < NODE_NAMES.length; i++) {
            List<EqCurveView.EqBand> bands = new ArrayList<>();
            // 为每个节点只添加 1 个默认频段
            bands.add(new EqCurveView.EqBand(true, 0, 1000, 1.0f, 0)); // 默认中频
            nodesBands.add(bands);
        }
    }

    /**
     * 初始化 UI 控件
     */
    private void initViews() {
        eqCurveView = findViewById(R.id.eq_curve_view);
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
        btnBack = findViewById(R.id.btn_back);

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
    }

    /**
     * 设置监听器
     */
    private void setupListeners() {
        // 返回按钮
        btnBack.setOnClickListener(v -> finish());

        // 保存按钮
        btnSave.setOnClickListener(v -> saveEqSettings());

        // 节点选择
        spinnerNode.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                currentNode = position;
                loadCurrentNodeBands();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });

        // 频段选择
        spinnerBand.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
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

        // 添加频段按钮
        btnAddBand.setOnClickListener(v -> addBand());

        // 删除频段按钮
        btnRemoveBand.setOnClickListener(v -> removeBand());
    }

    /**
     * 加载当前节点的频段数据
     */
    private void loadCurrentNodeBands() {
        List<EqCurveView.EqBand> bands = nodesBands.get(currentNode);
        eqCurveView.setBands(bands);

        // 更新频段选择器
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
        List<String> bandNames = new ArrayList<>();
        for (int i = 0; i < bands.size(); i++) {
            bandNames.add("Band " + (i + 1));
        }

        ArrayAdapter<String> bandAdapter = new ArrayAdapter<>(this,
            R.layout.spinner_item, bandNames);
        bandAdapter.setDropDownViewResource(R.layout.spinner_dropdown_item);
        spinnerBand.setAdapter(bandAdapter);
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
     * 添加频段
     */
    private void addBand() {
        List<EqCurveView.EqBand> bands = nodesBands.get(currentNode);
        if (bands.size() >= 9) { // 限制最大频段数
            Toast.makeText(this, "最多只能添加 9 个频段", Toast.LENGTH_SHORT).show();
            return;
        }

        // 添加默认频段
        EqCurveView.EqBand newBand = new EqCurveView.EqBand(true, 0, 1000, 1.0f, 0);
        bands.add(newBand);
        eqCurveView.addBand(newBand);

        // 更新 UI
        updateBandSpinner();
        currentBand = bands.size() - 1;
        spinnerBand.setSelection(currentBand);
        loadCurrentBandParameters();
        
        // 发送enable=1命令
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            int effectId = EFFECT_IDS[currentNode];
            String command = String.format("fx %d band%d_enable 1\r\n", effectId, currentBand);
            sendBatchBleCommands(command);
        }
    }

    /**
     * 删除频段
     */
    private void removeBand() {
        List<EqCurveView.EqBand> bands = nodesBands.get(currentNode);
        if (bands.size() <= 1) { // 最少保留 1 个频段
            Toast.makeText(this, "至少需要保留 1 个频段", Toast.LENGTH_SHORT).show();
            return;
        }

        // 发送enable=0命令禁用该频段
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            int effectId = EFFECT_IDS[currentNode];
            String command = String.format("fx %d band%d_enable 0\r\n", effectId, currentBand);
            sendBatchBleCommands(command);
        }

        bands.remove(currentBand);
        eqCurveView.removeBand(currentBand);

        // 更新 UI
        updateBandSpinner();
        if (currentBand >= bands.size()) {
            currentBand = bands.size() - 1;
        }
        spinnerBand.setSelection(currentBand);
        loadCurrentBandParameters();
    }

    /**
     * 保存 EQ 设置到 SharedPreferences 和设备
     */
    private void saveEqSettings() {
        // 保存到 SharedPreferences
        saveEqConfigToPreferences();

        // 发送保存命令到设备
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            // 发送所有节点的所有频段配置
            for (int node = 0; node < nodesBands.size(); node++) {
                List<EqCurveView.EqBand> bands = nodesBands.get(node);
                for (int band = 0; band < bands.size(); band++) {
                    sendEqCommand(node, band, bands.get(band));
                }
            }
            
            // 发送保存命令（类似 chain -S）
            bluetoothHelper.writeCharacteristic(
                "0000ab01-0000-1000-8000-00805f9b34fb",
                "eq_save\r\n".getBytes(),
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
        commandQueue.offer(command);
        processNextCommand();
    }

    /**
     * 处理下一个命令
     */
    private void processNextCommand() {
        if (isSendingCommand || commandQueue.isEmpty()) {
            return;
        }

        isSendingCommand = true;
        Runnable command = commandQueue.poll();
        if (command != null) {
            command.run();
        } else {
            isSendingCommand = false;
        }
    }

    /**
     * 发送单个 BLE 命令
     */
    private void sendBleCommand(String command) {
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            String fullCommand = command + "\r\n";
            bluetoothHelper.writeCharacteristic(
                "0000ab01-0000-1000-8000-00805f9b34fb",
                fullCommand.getBytes(),
                success -> {
                    if (!success) {
                        android.util.Log.e("EQ", "Failed to send command: " + command);
                    }
                    // 无论成功失败，都触发下一个命令（延迟200ms以确保设备处理完成）
                    commandHandler.postDelayed(() -> {
                        isSendingCommand = false;
                        processNextCommand();
                    }, 200);
                }
            );
        } else {
            // 如果未连接，标记命令完成以继续队列
            isSendingCommand = false;
            processNextCommand();
        }
    }

    /**
     * 批量发送 BLE 命令（多条命令合并成一个字符串）
     */
    private void sendBatchBleCommands(String batchCommands) {
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            android.util.Log.d("EQ", "Sending batch commands: " + batchCommands.replace("\r\n", " | "));
            bluetoothHelper.writeCharacteristic(
                "0000ab01-0000-1000-8000-00805f9b34fb",
                batchCommands.getBytes(),
                success -> {
                    if (!success) {
                        android.util.Log.e("EQ", "Failed to send batch commands");
                    } else {
                        android.util.Log.d("EQ", "Batch commands sent successfully");
                    }
                }
            );
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
        // -12dB - +12dB
        return (progress - 12) * 1.0f;
    }

    private int progressFromGain(float gain) {
        return (int) (gain + 12);
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
}
