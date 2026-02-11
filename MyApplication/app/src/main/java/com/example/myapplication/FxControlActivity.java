package com.example.myapplication;

import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class FxControlActivity extends AppCompatActivity {
    // DRC 参数配置
    private static final String[] DRC_PARAMS = {"Threshold", "Ratio", "Attack", "Release"};
    private static final int[] DRC_RANGES = {-60, 0, 1, 20, 1, 500, 10, 2000};
    private static final int[] DRC_DEFAULTS = {-30, 10, 100, 200};
    
    // 混响参数配置
    private static final String[] REVERB_PARAMS = {"Room", "Damp", "Wet"};
    private static final int[] REVERB_RANGES = {0, 100, 0, 100, 0, 100};
    private static final int[] REVERB_DEFAULTS = {50, 50, 30};

    private BluetoothHelper bluetoothHelper;
    
    // 效果器滑条和数值显示的引用
    private java.util.Map<Integer, java.util.Map<String, VerticalSeekBar>> effectSeekBars = new java.util.HashMap<>();
    private java.util.Map<Integer, java.util.Map<String, TextView>> effectValueTexts = new java.util.HashMap<>();
    
    // 命令队列和 Handler
    private android.os.Handler commandHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private java.util.Queue<Runnable> commandQueue = new java.util.LinkedList<>();
    private boolean isSendingCommand = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        bluetoothHelper = com.example.myapplication.BluetoothManager.getInstance().getBluetoothHelper();
        
        // 检查蓝牙连接状态
        if (bluetoothHelper == null || !bluetoothHelper.isConnected()) {
            Toast.makeText(this, "请先连接蓝牙设备", Toast.LENGTH_LONG).show();
            finish();
            return;
        }
        
        setContentView(R.layout.activity_fx_control);

        // 初始化返回按钮
        ImageButton btnBack = findViewById(R.id.btn_back);
        btnBack.setOnClickListener(v -> finish());

        // 初始化保存按钮
        ImageButton btnSave = findViewById(R.id.btn_save_fx);
        btnSave.setOnClickListener(v -> saveFxSettings());

        // 创建 DRC 滑条
        LinearLayout drcContainer = findViewById(R.id.drc_sliders_container);
        createVerticalSliders(drcContainer, 10, DRC_PARAMS, DRC_RANGES, DRC_DEFAULTS, "#00FFA3");

        // 创建混响滑条
        LinearLayout reverbContainer = findViewById(R.id.reverb_sliders_container);
        createVerticalSliders(reverbContainer, 12, REVERB_PARAMS, REVERB_RANGES, REVERB_DEFAULTS, "#00D9FF");
        
        // 设置BLE通知监听器来接收查询响应
        setupBleNotificationListener();
        
        // 查询效果器参数并同步到UI
        queryEffectParams();
    }
    
    /**
     * 查询效果器参数
     */
    private void queryEffectParams() {
        // 发送两条命令，每条结尾都带换行符
        sendQueryCommand("param -q effect 10\r\nparam -q effect 12");
    }
    
    /**
     * 发送查询命令
     */
    private void sendQueryCommand(String cmd) {
        queueCommand(() -> {
            if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
                String fullCmd = cmd + "\r\n";
                bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", 
                    fullCmd.getBytes(), success -> {
                        if (success) {
                            Log.d("FxControl", "Query sent: " + cmd);
                        } else {
                            runOnUiThread(() -> Toast.makeText(this, "查询失败", Toast.LENGTH_SHORT).show());
                        }
                        // 触发下一个命令
                        commandHandler.postDelayed(() -> {
                            isSendingCommand = false;
                            processNextCommand();
                        }, 200);
                    });
            } else {
                isSendingCommand = false;
                processNextCommand();
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        commandHandler.removeCallbacksAndMessages(null);
        commandQueue.clear();
    }

    /**
     * 设置BLE通知监听器
     */
    private void setupBleNotificationListener() {
        if (bluetoothHelper != null) {
            bluetoothHelper.setBleNotifyListener(data -> {
                android.util.Log.d("FxControl", "BLE notify received: " + data);
                // 处理效果器查询响应
                try {
                    org.json.JSONObject json = new org.json.JSONObject(data);

                    // 检查是否是完整的effect格式
                    if (json.has("effect")) {
                        org.json.JSONObject effectObj = json.getJSONObject("effect");
                        android.util.Log.d("FxControl", "Parsed effect object: " + effectObj.toString());
                        updateFxUI(effectObj);
                    }
                    // 检查是否是直接的drc或reverb格式（下位机简化格式）
                    else if (json.has("drc")) {
                        android.util.Log.d("FxControl", "Received DRC data in simplified format");
                        updateEffectParams(10, json.getJSONObject("drc"),
                            new String[]{"threshold", "ratio", "attack", "release"},
                            new String[]{"threshold", "ratio", "attack", "release"}, DRC_RANGES);
                        Toast.makeText(this, "DRC参数已同步", Toast.LENGTH_SHORT).show();
                    }
                    else if (json.has("reverb")) {
                        android.util.Log.d("FxControl", "Received Reverb data in simplified format");
                        updateEffectParams(12, json.getJSONObject("reverb"),
                            new String[]{"room_size", "damping", "wet_dry"},
                            new String[]{"room", "damp", "wet"}, REVERB_RANGES);
                        Toast.makeText(this, "混响参数已同步", Toast.LENGTH_SHORT).show();
                    }
                    else {
                        android.util.Log.d("FxControl", "No recognized effect field in JSON");
                    }
                } catch (org.json.JSONException e) {
                    android.util.Log.e("FxControl", "Failed to parse effect data: " + data, e);
                }
            });
        }
    }

    /**
     * 创建竖向滑条组
     */
    private void createVerticalSliders(LinearLayout container, int nodeId, String[] params, 
                                      int[] ranges, int[] defaults, String accentColor) {
        java.util.Map<String, VerticalSeekBar> seekBars = new java.util.HashMap<>();
        java.util.Map<String, TextView> valueTexts = new java.util.HashMap<>();
        
        for (int i = 0; i < params.length; i++) {
            String param = params[i];
            int min = ranges[i * 2];
            int max = ranges[i * 2 + 1];
            int defaultValue = defaults[i];

            // 创建单个滑条容器
            LinearLayout sliderLayout = new LinearLayout(this);
            sliderLayout.setOrientation(LinearLayout.VERTICAL);
            sliderLayout.setGravity(Gravity.CENTER);
            LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.MATCH_PARENT, 1f);
            layoutParams.setMargins(8, 0, 8, 0);
            sliderLayout.setLayoutParams(layoutParams);

            // 参数值显示
            TextView valueText = new TextView(this);
            valueText.setText(String.valueOf(defaultValue));
            valueText.setTextSize(18);
            valueText.setTextColor(Color.parseColor(accentColor));
            valueText.setGravity(Gravity.CENTER);
            valueText.setTextAlignment(View.TEXT_ALIGNMENT_CENTER);
            valueText.setPadding(0, 0, 0, 12);
            sliderLayout.addView(valueText);

            // 竖向 SeekBar
            VerticalSeekBar seekBar = new VerticalSeekBar(this);
            seekBar.setMax(max - min);
            seekBar.setProgress(defaultValue - min);
            LinearLayout.LayoutParams seekBarParams = new LinearLayout.LayoutParams(
                80, 0, 1f);
            seekBar.setLayoutParams(seekBarParams);
            
            // 参数名称标签
            TextView labelText = new TextView(this);
            labelText.setText(param);
            labelText.setTextSize(14);
            labelText.setTextColor(Color.WHITE);
            labelText.setGravity(Gravity.CENTER);
            labelText.setTextAlignment(View.TEXT_ALIGNMENT_CENTER);
            labelText.setPadding(0, 12, 0, 0);

            // 设置监听器
            seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    int value = min + progress;
                    valueText.setText(String.valueOf(value));
                }

                @Override
                public void onStartTrackingTouch(SeekBar seekBar) {}

                @Override
                public void onStopTrackingTouch(SeekBar seekBar) {
                    int value = min + seekBar.getProgress();
                    sendFxCommand(nodeId, param.toLowerCase(), value);
                }
            });

            sliderLayout.addView(seekBar);
            sliderLayout.addView(labelText);
            container.addView(sliderLayout);
            
            // 存储引用
            seekBars.put(param.toLowerCase(), seekBar);
            valueTexts.put(param.toLowerCase(), valueText);
        }
        
        effectSeekBars.put(nodeId, seekBars);
        effectValueTexts.put(nodeId, valueTexts);
    }

    /**
     * 发送 FX 命令
     */
    private void sendFxCommand(int nodeId, String param, int value) {
        queueCommand(() -> {
            if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
                String cmd = "fx " + nodeId + " " + param + " " + value + "\r\n";
                bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", 
                    cmd.getBytes(), success -> {
                        // 无论成功失败，都触发下一个命令（延迟200ms以确保设备处理完成）
                        commandHandler.postDelayed(() -> {
                            isSendingCommand = false;
                            processNextCommand();
                        }, 200);
                    });
            } else {
                // 如果未连接，标记命令完成以继续队列
                isSendingCommand = false;
                processNextCommand();
            }
        });
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
     * 更新效果器UI
     */
    private void updateFxUI(org.json.JSONObject effectObj) {
        android.util.Log.d("FxControl", "updateFxUI called with: " + effectObj.toString());
        runOnUiThread(() -> {
            try {
                android.util.Log.d("FxControl", "Running on UI thread");
                
                // 检查是否有params字段
                if (effectObj.has("params")) {
                    org.json.JSONObject paramsObj = effectObj.getJSONObject("params");
                    android.util.Log.d("FxControl", "Parsed params object: " + paramsObj.toString());
                    
                    // 处理DRC参数 (nodeId = 10)
                    if (paramsObj.has("drc")) {
                        org.json.JSONObject drcObj = paramsObj.getJSONObject("drc");
                        updateEffectParams(10, drcObj, new String[]{"threshold", "ratio", "attack", "release"}, new String[]{"threshold", "ratio", "attack", "release"}, DRC_RANGES);
                    }
                    
                    // 处理混响参数 (nodeId = 12)
                    if (paramsObj.has("reverb")) {
                        org.json.JSONObject reverbObj = paramsObj.getJSONObject("reverb");
                        updateEffectParams(12, reverbObj, new String[]{"room_size", "damping", "wet_dry"}, new String[]{"room", "damp", "wet"}, REVERB_RANGES);
                    }
                    
                    Toast.makeText(this, "效果参数已同步", Toast.LENGTH_SHORT).show();
                    android.util.Log.d("FxControl", "UI update completed");
                } else {
                    android.util.Log.w("FxControl", "No params field in effect object");
                }
            } catch (org.json.JSONException e) {
                android.util.Log.e("FxControl", "Failed to update effect UI", e);
                Toast.makeText(this, "解析效果数据失败", Toast.LENGTH_SHORT).show();
            }
        });
    }
    
    /**
     * 更新特定效果的参数 (带映射)
     */
    private void updateEffectParams(int nodeId, org.json.JSONObject paramsObj, String[] jsonKeys, String[] uiKeys, int[] ranges) {
        java.util.Map<String, VerticalSeekBar> seekBars = effectSeekBars.get(nodeId);
        java.util.Map<String, TextView> valueTexts = effectValueTexts.get(nodeId);
        
        if (seekBars == null || valueTexts == null) {
            android.util.Log.w("FxControl", "No UI references found for nodeId: " + nodeId);
            return;
        }
        
        for (int i = 0; i < jsonKeys.length && i < uiKeys.length; i++) {
            String jsonKey = jsonKeys[i];
            String uiKey = uiKeys[i];
            
            try {
                if (paramsObj.has(jsonKey)) {
                    int value = paramsObj.getInt(jsonKey);
                    android.util.Log.d("FxControl", "Updating " + jsonKey + " to " + value);
                    
                    // 找到对应的UI组件
                    VerticalSeekBar seekBar = seekBars.get(uiKey);
                    TextView valueText = valueTexts.get(uiKey);
                    
                    if (seekBar != null && valueText != null) {
                        // 计算滑条位置 (value - min)
                        int min = ranges[i * 2];
                        
                        seekBar.setProgress(value - min);
                        valueText.setText(String.valueOf(value));
                        android.util.Log.d("FxControl", "Updated " + uiKey + " UI to " + value);
                    } else {
                        android.util.Log.w("FxControl", "UI component not found for " + uiKey);
                    }
                } else {
                    android.util.Log.w("FxControl", "Param " + jsonKey + " not found in JSON");
                }
            } catch (org.json.JSONException e) {
                android.util.Log.e("FxControl", "Failed to parse param " + jsonKey, e);
            }
        }
    }

    /**
     * 保存效果参数
     */
    private void saveFxSettings() {
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            queueCommand(() -> {
                bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", 
                    "chain -S\r\n".getBytes(), success -> {
                    runOnUiThread(() -> {
                        if (success) {
                            Toast.makeText(this, "效果参数已保存", Toast.LENGTH_SHORT).show();
                        } else {
                            Toast.makeText(this, "保存失败", Toast.LENGTH_SHORT).show();
                        }
                    });
                    // 触发下一个命令
                    commandHandler.postDelayed(() -> {
                        isSendingCommand = false;
                        processNextCommand();
                    }, 200);
                });
            });
        } else {
            Toast.makeText(this, "设备未连接", Toast.LENGTH_SHORT).show();
        }
    }
}
