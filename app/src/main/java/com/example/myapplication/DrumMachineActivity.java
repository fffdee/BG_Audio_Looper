package com.example.myapplication;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.GridLayout;
import androidx.appcompat.app.AppCompatActivity;

/**
 * 鼓机Activity
 * 提供16步×8轨道的鼓机编辑和播放功能
 */
public class DrumMachineActivity extends BaseActivity {
    
    private BluetoothHelper bluetoothHelper;
    private Handler handler = new Handler(Looper.getMainLooper());
    
    // UI控件
    private TextView tvStatus;
    private TextView tvBpmDisplay;
    private TextView tvVolumeDisplay;
    private Button btnPlayStop;
    private Button btnBpmMinus, btnBpmPlus;
    private SeekBar seekbarBpm, seekbarVolume;
    private GridLayout gridDrumPad;
    
    // 鼓机参数
    private boolean isPlaying = false;
    private int currentBpm = 120;
    private int currentVolume = 80;
    private int currentStep = 0xFF;
    
    // 16步 x 8轨道 的pattern (true=启用)
    private boolean[][] patternGrid = new boolean[16][8];
    
    // 轨道信息
    private static final String[] TRACK_NAMES = {
        "Kick", "Snare", "HiHat C", "HiHat O",
        "Tom L", "Tom M", "Tom H", "Crash"
    };
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_drum_machine);
        setupBaseToolbar(true);
        
        // 获取全局BluetoothHelper实例
        bluetoothHelper = BluetoothManager.getInstance().getBluetoothHelper();
        
        // 初始化UI
        initViews();
        
        // 初始化pattern网格
        initPatternGrid();
        
        // 设置BLE数据监听
        setupBleListener();
        
        // 检查连接并查询状态
        if (checkConnection()) {
            queryDrumStatus();
        }
    }
    
    /**
     * 初始化UI控件
     */
    private void initViews() {
        tvStatus = findViewById(R.id.tv_drum_status);
        tvBpmDisplay = findViewById(R.id.tv_bpm_display);
        tvVolumeDisplay = findViewById(R.id.tv_volume_display);
        btnPlayStop = findViewById(R.id.btn_play_stop);
        btnBpmMinus = findViewById(R.id.btn_bpm_minus);
        btnBpmPlus = findViewById(R.id.btn_bpm_plus);
        seekbarBpm = findViewById(R.id.seekbar_bpm);
        seekbarVolume = findViewById(R.id.seekbar_volume);
        gridDrumPad = findViewById(R.id.grid_drum_pad);
        
        // 播放/停止按钮
        // 播放/停止按钮
        btnPlayStop.setOnClickListener(v -> togglePlayStop());
        
        // BPM控制
        btnBpmMinus.setOnClickListener(v -> adjustBpm(-1));
        btnBpmPlus.setOnClickListener(v -> adjustBpm(1));
        
        seekbarBpm.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    currentBpm = progress + 40;
                    updateBpmDisplay();
                }
            }
            
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                setBpm(currentBpm);
            }
        });
        
        // 音量控制
        seekbarVolume.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    currentVolume = progress;
                    updateVolumeDisplay();
                }
            }
            
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                setVolume(currentVolume);
            }
        });
        
        updateBpmDisplay();
        updateVolumeDisplay();
    }
    
    /**
     * 初始化pattern网格UI (16步 x 8轨道)
     * 为简化起见，这里仅创建基础结构
     * 完整实现可以使用自定义GridView或RecyclerView
     */
    private void initPatternGrid() {
        // 初始化pattern数据为全0
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 8; j++) {
                patternGrid[i][j] = false;
            }
        }
        
        // 在gridDrumPad中动态创建按钮
        // 为了简化，这里不再layoutxml中定义，改为代码生成
        // [完整实现留后]
    }
    
    /**
     * 设置BLE数据监听
     */
    private void setupBleListener() {
        bluetoothHelper.setBleNotifyListener(data -> {
            runOnUiThread(() -> {
                parseDrumStatus(data);
            });
        });
    }
    
    @Override
    protected String getToolbarTitle() {
        return "鼓机";
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (checkConnection()) {
            queryDrumStatus();
        }
    }
    
    /**
     * 检查BLE连接状态
     */
    private boolean checkConnection() {
        if (bluetoothHelper == null || !bluetoothHelper.isConnected()) {
            Toast.makeText(this, "未连接到设备", Toast.LENGTH_SHORT).show();
            return false;
        }
        return true;
    }
    
    /**
     * 发送BLE命令
     */
    private void sendCommand(String cmd, String successMsg) {
        if (!checkConnection()) return;
        
        String cmdWithCRLF = cmd + "\r\n";
        bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb",
            cmdWithCRLF.getBytes(), success -> {
                if (success && successMsg != null) {
                    runOnUiThread(() -> Toast.makeText(this, successMsg, Toast.LENGTH_SHORT).show());
                } else if (!success) {
                    runOnUiThread(() -> Toast.makeText(this, "命令发送失败", Toast.LENGTH_SHORT).show());
                }
            });
    }
    
    /**
     * 查询鼓机状态
     */
    private void queryDrumStatus() {
        sendCommand("drum -q", null);
    }
    
    /**
     * 解析鼓机状态信息 (JSON格式)
     */
    private void parseDrumStatus(String data) {
        if (data == null || data.trim().isEmpty()) return;
        
        try {
            // 简单的JSON解析
            if (data.contains("\"drum\"")) {
                // 解析playing状态
                if (data.contains("\"playing\":1")) {
                    isPlaying = true;
                } else if (data.contains("\"playing\":0")) {
                    isPlaying = false;
                }
                
                // 解析当前step
                String stepStr = extractJsonValue(data, "step");
                if (stepStr != null) {
                    try {
                        currentStep = Integer.parseInt(stepStr);
                    } catch (Exception e) {
                        // ignore
                    }
                }
                
                // 解析BPM
                String bpmStr = extractJsonValue(data, "bpm");
                if (bpmStr != null) {
                    try {
                        currentBpm = Integer.parseInt(bpmStr);
                        seekbarBpm.setProgress(currentBpm - 40);
                        updateBpmDisplay();
                    } catch (Exception e) {
                        // ignore
                    }
                }
                
                // 解析音量
                String volStr = extractJsonValue(data, "vol");
                if (volStr != null) {
                    try {
                        currentVolume = Integer.parseInt(volStr);
                        seekbarVolume.setProgress(currentVolume);
                        updateVolumeDisplay();
                    } catch (Exception e) {
                        // ignore
                    }
                }
                
                updateStatusDisplay();
            }
        } catch (Exception e) {
            android.util.Log.e("DrumMachine", "解析状态失败: " + data, e);
        }
    }
    
    /**
     * 从JSON字符串中提取指定key的值
     */
    private String extractJsonValue(String json, String key) {
        try {
            String pattern = "\"" + key + "\":";
            int idx = json.indexOf(pattern);
            if (idx == -1) return null;
            
            idx += pattern.length();
            // 跳过空格
            while (idx < json.length() && json.charAt(idx) == ' ') idx++;
            
            // 查找末尾 (逗号或大括号)
            int endIdx = idx;
            while (endIdx < json.length() && 
                   json.charAt(endIdx) != ',' && 
                   json.charAt(endIdx) != '}' &&
                   json.charAt(endIdx) != ']') {
                endIdx++;
            }
            
            return json.substring(idx, endIdx).trim();
        } catch (Exception e) {
            return null;
        }
    }
    
    /**
     * 播放/停止
     */
    private void togglePlayStop() {
        if (isPlaying) {
            sendCommand("drum -p stop", "停止播放");
            isPlaying = false;
        } else {
            sendCommand("drum -p 1", "开始播放");
            isPlaying = true;
        }
        updateStatusDisplay();
    }
    
    /**
     * 调整BPM
     */
    private void adjustBpm(int delta) {
        currentBpm = Math.max(40, Math.min(240, currentBpm + delta));
        seekbarBpm.setProgress(currentBpm - 40);
        updateBpmDisplay();
        setBpm(currentBpm);
    }
    
    /**
     * 设置BPM
     */
    private void setBpm(int bpm) {
        sendCommand("drum -b " + bpm, null);
    }
    
    /**
     * 设置音量
     */
    private void setVolume(int volume) {
        sendCommand("drum -v " + volume, null);
    }
    
    /**
     * 编辑单个步
     */
    private void editStep(int step, int track, boolean on) {
        patternGrid[step][track] = on;
        String cmd = String.format("drum -e %d %d %d", step, track, on ? 1 : 0);
        sendCommand(cmd, null);
    }
    
    /**
     * 保存Pattern到Flash
     */
    private void savePattern(int slot) {
        sendCommand("drum -s " + slot, "Pattern已保存");
    }
    
    /**
     * 加载Pattern
     */
    private void loadPattern(int slot) {
        sendCommand("drum -l " + slot, "Pattern已加载");
    }
    
    /**
     * 加载预设
     */
    private void loadPreset(int preset) {
        String[] presetNames = {"Rock", "Pop", "Funk", "Latin"};
        sendCommand("drum -P " + preset, "预设 " + presetNames[preset] + " 已加载");
    }
    
    /**
     * 更新舞台状态显示
     */
    private void updateStatusDisplay() {
        if (isPlaying) {
            tvStatus.setText("● 播放中 (Step: " + currentStep + "/16)");
            tvStatus.setTextColor(0xFF00FF88);
            btnPlayStop.setText("停止");
            btnPlayStop.setBackgroundResource(R.drawable.button_volume_glow);
        } else {
            tvStatus.setText("● 停止");
            tvStatus.setTextColor(getColor(R.color.text_secondary));
            btnPlayStop.setText("播放");
            btnPlayStop.setBackgroundResource(R.drawable.button_connect);
        }
    }
    
    /**
     * 更新BPM显示
     */
    private void updateBpmDisplay() {
        tvBpmDisplay.setText(String.valueOf(currentBpm));
    }
    
    /**
     * 更新音量显示
     */
    private void updateVolumeDisplay() {
        tvVolumeDisplay.setText(currentVolume + "%");
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (bluetoothHelper != null) {
            bluetoothHelper.setBleNotifyListener(null);
        }
        handler.removeCallbacksAndMessages(null);
    }
}
