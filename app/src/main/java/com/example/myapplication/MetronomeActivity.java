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
import androidx.appcompat.app.AppCompatActivity;

/**
 * 节拍器Activity
 * 提供节拍器的控制和设置功能
 */
public class MetronomeActivity extends BaseActivity {
    
    private BluetoothHelper bluetoothHelper;
    private Handler handler = new Handler(Looper.getMainLooper());
    
    // UI控件
    private TextView tvMetronomeStatus;
    private TextView tvBpmDisplay;
    private TextView tvBeatsDisplay;
    private TextView tvVolumeDisplay;
    private Button btnMetroToggle;
    private Button btnBpmMinus, btnBpmPlus;
    private Button btnBeatsMinus, btnBeatsPlus;
    private SeekBar seekbarBpm, seekbarBeats, seekbarVolume;
    
    // 节拍器参数
    private boolean isMetronomeOn = false;
    private int currentBpm = 120;  // 默认120 BPM
    private int currentBeats = 4;  // 默认4拍
    private int currentVolume = 80; // 默认80%音量
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_metronome);
        setupBaseToolbar(true);
        
        // 获取全局BluetoothHelper实例
        bluetoothHelper = BluetoothManager.getInstance().getBluetoothHelper();
        
        // 初始化UI
        initViews();
        
        // 设置BLE数据监听
        setupBleListener();
        
        // 检查连接并查询状态
        if (checkConnection()) {
            BleParamCache paramCache = BleParamCache.getInstance();
            if (paramCache.isSyncComplete()) {
                applyCachedMetronomeParams();
            } else {
                queryMetronomeStatus();
            }
        }
    }
    
    /**
     * 设置BLE数据监听
     */
    private void setupBleListener() {
        bluetoothHelper.setBleNotifyListener(data -> {
            runOnUiThread(() -> {
                parseMetronomeStatus(data);
            });
        });
    }
    
    @Override
    protected String getToolbarTitle() {
        return "节拍器";
    }

    @Override
    protected void onResume() {
        super.onResume();
        // 页面重新获得焦点时，优先读缓存，避免重复查询造成音频卡顿
        if (checkConnection()) {
            BleParamCache paramCache = BleParamCache.getInstance();
            if (paramCache.isSyncComplete()) {
                applyCachedMetronomeParams();
            } else {
                queryMetronomeStatus();
            }
        }
    }
    
    private void initViews() {
        tvMetronomeStatus = findViewById(R.id.tv_metronome_status);
        tvBpmDisplay = findViewById(R.id.tv_bpm_display);
        tvBeatsDisplay = findViewById(R.id.tv_beats_display);
        tvVolumeDisplay = findViewById(R.id.tv_volume_display);
        btnMetroToggle = findViewById(R.id.btn_metro_toggle);
        btnBpmMinus = findViewById(R.id.btn_bpm_minus);
        btnBpmPlus = findViewById(R.id.btn_bpm_plus);
        btnBeatsMinus = findViewById(R.id.btn_beats_minus);
        btnBeatsPlus = findViewById(R.id.btn_beats_plus);
        seekbarBpm = findViewById(R.id.seekbar_bpm);
        seekbarBeats = findViewById(R.id.seekbar_beats);
        seekbarVolume = findViewById(R.id.seekbar_volume);
        
        // 启动/停止按钮
        btnMetroToggle.setOnClickListener(v -> toggleMetronome());
        
        // BPM按钮
        btnBpmMinus.setOnClickListener(v -> adjustBpm(-1));
        btnBpmPlus.setOnClickListener(v -> adjustBpm(1));
        
        // 拍数按钮
        btnBeatsMinus.setOnClickListener(v -> adjustBeats(-1));
        btnBeatsPlus.setOnClickListener(v -> adjustBeats(1));
        
        // BPM SeekBar
        seekbarBpm.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    currentBpm = progress + 60; // 60-200
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
        
        // 拍数 SeekBar
        seekbarBeats.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    currentBeats = progress + 2; // 2-8
                    updateBeatsDisplay();
                }
            }
            
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                setBeats(currentBeats);
            }
        });
        
        // 音量 SeekBar
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
        
        // 初始显示
        updateBpmDisplay();
        updateBeatsDisplay();
        updateVolumeDisplay();
        updateStatusDisplay();
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
     * 从连接时同步缓存中直接读取节拍器参数（无需重新查询设备）
     * 设备发送顺序: [tempo, time_signature, click_volume, overdub_mode, quantize]
     */
    private void applyCachedMetronomeParams() {
        int[] params = BleParamCache.getInstance().getMetronomeParams();
        if (params == null) {
            queryMetronomeStatus();
            return;
        }
        currentBpm    = Math.max(60,  Math.min(200, params[0]));
        currentBeats  = Math.max(2,   Math.min(8,   params[1]));
        currentVolume = Math.max(0,   Math.min(100, params[2]));
        seekbarBpm.setProgress(currentBpm - 60);
        seekbarBeats.setProgress(currentBeats - 2);
        seekbarVolume.setProgress(currentVolume);
        updateBpmDisplay();
        updateBeatsDisplay();
        updateVolumeDisplay();
        // isMetronomeOn 运行状态不在同步协议中，保持页面默认值
    }

    /**
     * 查询节拍器状态
     */
    private void queryMetronomeStatus() {
        sendCommand("param -q metronome", null);
        // 状态信息会通过BLE监听器接收并解析
    }
    
    /**
     * 解析节拍器状态信息
     */
    private void parseMetronomeStatus(String data) {
        if (data == null || data.trim().isEmpty()) return;
        
        String response = data.trim();
        android.util.Log.d("Metronome", "接收到状态数据: " + response);
        
        // 解析状态信息
        // 支持多种可能的响应格式
        try {
            // 检查是否包含状态信息
            if (response.toLowerCase().contains("metro")) {
                // 解析开关状态
                if (response.toLowerCase().contains("on")) {
                    isMetronomeOn = true;
                } else if (response.toLowerCase().contains("off")) {
                    isMetronomeOn = false;
                }
                
                // 解析BPM - 支持多种格式
                String bpmValue = extractValue(response, "bpm") != null ? 
                    extractValue(response, "bpm") : extractValue(response, "BPM:");
                if (bpmValue != null) {
                    currentBpm = Integer.parseInt(bpmValue);
                    seekbarBpm.setProgress(currentBpm - 60);
                    updateBpmDisplay();
                }
                
                // 解析拍数 - 支持多种格式
                String beatsValue = extractValue(response, "beats") != null ? 
                    extractValue(response, "beats") : extractValue(response, "Beats:");
                if (beatsValue != null) {
                    currentBeats = Integer.parseInt(beatsValue);
                    seekbarBeats.setProgress(currentBeats - 2);
                    updateBeatsDisplay();
                }
                
                // 解析音量 - 支持多种格式
                String volumeValue = extractValue(response, "vol") != null ? 
                    extractValue(response, "vol") : extractValue(response, "Volume:");
                if (volumeValue != null) {
                    currentVolume = Integer.parseInt(volumeValue);
                    seekbarVolume.setProgress(currentVolume);
                    updateVolumeDisplay();
                }
                
                // 更新UI状态
                updateStatusDisplay();
                android.util.Log.d("Metronome", "状态已更新: ON=" + isMetronomeOn + ", BPM=" + currentBpm + ", Beats=" + currentBeats + ", Vol=" + currentVolume);
            }
        } catch (Exception e) {
            android.util.Log.e("Metronome", "解析状态信息失败: " + response, e);
        }
    }
    
    /**
     * 从响应字符串中提取数值
     */
    private String extractValue(String response, String key) {
        try {
            String lowerResponse = response.toLowerCase();
            String lowerKey = key.toLowerCase();
            
            int startIndex = lowerResponse.indexOf(lowerKey);
            if (startIndex == -1) return null;
            
            // 移动到键的末尾
            startIndex += lowerKey.length();
            
            // 跳过冒号、空格等分隔符
            while (startIndex < response.length() && 
                   !Character.isDigit(response.charAt(startIndex))) {
                startIndex++;
            }
            
            if (startIndex >= response.length()) return null;
            
            // 提取数字
            int endIndex = startIndex;
            while (endIndex < response.length() && 
                   Character.isDigit(response.charAt(endIndex))) {
                endIndex++;
            }
            
            if (endIndex > startIndex) {
                return response.substring(startIndex, endIndex).trim();
            }
        } catch (Exception e) {
            android.util.Log.e("Metronome", "提取数值失败: " + key + " from: " + response, e);
        }
        return null;
    }
    
    /**
     * 切换节拍器开关
     */
    private void toggleMetronome() {
        if (isMetronomeOn) {
            sendCommand("metro off", "节拍器已停止");
            isMetronomeOn = false;
        } else {
            sendCommand("metro on", "节拍器已启动");
            isMetronomeOn = true;
        }
        updateStatusDisplay();
    }
    
    /**
     * 调整BPM
     */
    private void adjustBpm(int delta) {
        currentBpm = Math.max(60, Math.min(200, currentBpm + delta));
        seekbarBpm.setProgress(currentBpm - 60);
        updateBpmDisplay();
        setBpm(currentBpm);
    }
    
    /**
     * 设置BPM
     */
    private void setBpm(int bpm) {
        sendCommand("metro bpm " + bpm, null);
    }
    
    /**
     * 调整拍数
     */
    private void adjustBeats(int delta) {
        currentBeats = Math.max(2, Math.min(8, currentBeats + delta));
        seekbarBeats.setProgress(currentBeats - 2);
        updateBeatsDisplay();
        setBeats(currentBeats);
    }
    
    /**
     * 设置拍数
     */
    private void setBeats(int beats) {
        sendCommand("metro beats " + beats, null);
    }
    
    /**
     * 设置音量
     */
    private void setVolume(int volume) {
        sendCommand("metro vol " + volume, null);
    }
    
    /**
     * 更新BPM显示
     */
    private void updateBpmDisplay() {
        tvBpmDisplay.setText(String.valueOf(currentBpm));
    }
    
    /**
     * 更新拍数显示
     */
    private void updateBeatsDisplay() {
        tvBeatsDisplay.setText(currentBeats + "/4");
    }
    
    /**
     * 更新音量显示
     */
    private void updateVolumeDisplay() {
        tvVolumeDisplay.setText(currentVolume + "%");
    }
    
    /**
     * 更新状态显示
     */
    private void updateStatusDisplay() {
        if (isMetronomeOn) {
            tvMetronomeStatus.setText("● 运行中");
            tvMetronomeStatus.setTextColor(0xFF00FF88);
            btnMetroToggle.setText("停止");
            btnMetroToggle.setBackgroundResource(R.drawable.button_volume_glow);
        } else {
            tvMetronomeStatus.setText("● 已停止");
            tvMetronomeStatus.setTextColor(getColor(R.color.text_secondary));
            btnMetroToggle.setText("启动");
            btnMetroToggle.setBackgroundResource(R.drawable.button_connect);
        }
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        // 清理BLE监听器
        if (bluetoothHelper != null) {
            bluetoothHelper.setBleNotifyListener(null);
        }
        // 清理其他资源
        handler.removeCallbacksAndMessages(null);
    }
}
