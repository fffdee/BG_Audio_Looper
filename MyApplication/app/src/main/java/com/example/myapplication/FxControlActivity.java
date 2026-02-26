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
    /**
     * 设置BLE通知监听器
     */
    private void setupBleNotificationListener() {
        if (bluetoothHelper != null) {
            bluetoothHelper.setBleNotifyListener(data -> {
                android.util.Log.d("FxControl", "BLE notify received: " + data);
                // 处理效果器查询响应 - 现在使用二进制格式而不是JSON
                try {
                    // 尝试解析二进制数据，可能包含多个数据包
                    if (parseBinaryFxData(data)) {
                        android.util.Log.d("FxControl", "Successfully parsed binary effect data");
                    } else {
                        android.util.Log.w("FxControl", "Failed to parse binary effect data, trying JSON fallback");
                        // 回退到JSON解析（用于兼容性）
                        parseJsonFxData(data);
                    }
                } catch (Exception e) {
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

    /**
     * 解析二进制格式的效果器数据
     * DRC: [type(1)=0x10][length(1)=6][threshold(2)][ratio(2)][attack(2)][release(2)]
     * Reverb: [type(1)=0x11][length(1)=3][room_size(1)][damping(1)][wet_dry(1)]
     */

    private boolean parseBinaryFxData(String data) {
        try {
            // 将十六进制字符串转换为字节数组
            if (data.startsWith("0x") || data.contains(" ")) {
                // 如果是空格分隔的十六进制字符串
                String[] hexParts = data.replace("0x", "").split("\\s+");
                byte[] bytes = new byte[hexParts.length];
                for (int i = 0; i < hexParts.length; i++) {
                    bytes[i] = (byte) Integer.parseInt(hexParts[i], 16);
                }
                return parseBinaryFxData(bytes);
            } else if (data.matches("[0-9A-Fa-f]+")) {
                // 如果是连续的十六进制字符串（没有空格）
                int len = data.length();
                if (len % 2 != 0) {
                    android.util.Log.w("FxControl", "Invalid hex string length: " + len);
                    return false;
                }
                byte[] bytes = new byte[len / 2];
                for (int i = 0; i < len; i += 2) {
                    bytes[i / 2] = (byte) Integer.parseInt(data.substring(i, i + 2), 16);
                }
                return parseBinaryFxData(bytes);
            } else {
                // 如果是原始字符串，假设是字节数据
                byte[] bytes = data.getBytes();
                return parseBinaryFxData(bytes);
            }
        } catch (Exception e) {
            android.util.Log.e("FxControl", "Failed to convert data to bytes", e);
            return false;
        }
    }

    /**
     * 解析二进制字节数组格式的效果器数据
     * 支持解析多个连续的数据包
     */
    private boolean parseBinaryFxData(byte[] data) {
        if (data == null || data.length < 5) {
            return false;
        }

        int idx = 0;
        boolean parsedAny = false;

        // 循环解析所有数据包，直到数据处理完毕
        while (idx < data.length) {
            // 检查是否还有足够的数据用于一个完整的数据包
            if (idx + 4 >= data.length) { // 需要至少header(2) + type(1) + length(1)
                break;
            }

            // 检查header: 0xAA 0x55
            if (data[idx] != (byte)0xAA || data[idx + 1] != (byte)0x55) {
                android.util.Log.w("FxControl", "Invalid header at position " + idx + ": " +
                    String.format("%02X %02X", data[idx], data[idx + 1]));
                break; // 如果header不匹配，停止解析
            }

            byte type = data[idx + 2];
            byte length = data[idx + 3];

            // 检查是否有足够的数据
            int packetLength = 4 + length; // header(2) + type(1) + length(1) + data(length)
            if (idx + packetLength > data.length) {
                android.util.Log.w("FxControl", "Incomplete packet at position " + idx +
                    ", need " + packetLength + " bytes, have " + (data.length - idx));
                break;
            }

            // 解析数据包
            boolean parsed = false;
            if (type == 0x10 && length == 8) { // DRC: threshold(2)+ratio(2)+attack(2)+release(2)=8 bytes
                parsed = parseBinaryDrcData(data, idx + 4);
                android.util.Log.d("FxControl", "Parsed DRC packet");
            } else if (type == 0x11 && length == 3) { // Reverb: room(1)+damping(1)+wet-dry(1)=3 bytes
                parsed = parseBinaryReverbData(data, idx + 4);
                android.util.Log.d("FxControl", "Parsed Reverb packet");
            } else {
                android.util.Log.w("FxControl", "Unknown FX type: " + String.format("%02X", type) +
                    " or invalid length: " + length + " at position " + idx);
            }

            if (parsed) {
                parsedAny = true;
            }

            // 移动到下一个数据包
            idx += packetLength;
        }

        return parsedAny;
    }

    private boolean parseBinaryDrcData(byte[] data, int startIdx) {
        int idx = startIdx;
        // threshold (2 bytes, little endian)
        int threshold = (data[idx++] & 0xFF) | ((data[idx++] & 0xFF) << 8);
        // ratio (2 bytes, little endian)
        int ratio = (data[idx++] & 0xFF) | ((data[idx++] & 0xFF) << 8);
        // attack (2 bytes, little endian)
        int attack = (data[idx++] & 0xFF) | ((data[idx++] & 0xFF) << 8);
        // release (2 bytes, little endian)
        int release = (data[idx++] & 0xFF) | ((data[idx++] & 0xFF) << 8);

        runOnUiThread(() -> {
            try {
                // 获取DRC节点的SeekBar Map
                java.util.Map<String, VerticalSeekBar> drcSeekBars = effectSeekBars.get(10);
                if (drcSeekBars != null) {
                    android.util.Log.d("FxControl", "DRC SeekBars found, updating values");
                    // 设置实际值
                    VerticalSeekBar thresholdBar = drcSeekBars.get("threshold");
                    if (thresholdBar != null) {
                        int progress = Math.max(DRC_RANGES[0], Math.min(DRC_RANGES[1], threshold / 100)); // threshold范围-60到0
                        thresholdBar.setProgress(progress - DRC_RANGES[0]);
                        android.util.Log.d("FxControl", "DRC Threshold set to progress: " + (progress - DRC_RANGES[0]));
                    } else {
                        android.util.Log.w("FxControl", "DRC Threshold SeekBar not found");
                    }
                    
                    VerticalSeekBar ratioBar = drcSeekBars.get("ratio");
                    if (ratioBar != null) {
                        int progress = Math.max(DRC_RANGES[2], Math.min(DRC_RANGES[3], ratio / 10)); // ratio范围1到20
                        ratioBar.setProgress(progress - DRC_RANGES[2]);
                        android.util.Log.d("FxControl", "DRC Ratio set to progress: " + (progress - DRC_RANGES[2]));
                    } else {
                        android.util.Log.w("FxControl", "DRC Ratio SeekBar not found");
                    }
                    
                    VerticalSeekBar attackBar = drcSeekBars.get("attack");
                    if (attackBar != null) {
                        int progress = Math.max(DRC_RANGES[4], Math.min(DRC_RANGES[5], attack)); // attack范围1到500
                        attackBar.setProgress(progress - DRC_RANGES[4]);
                        android.util.Log.d("FxControl", "DRC Attack set to progress: " + (progress - DRC_RANGES[4]));
                    } else {
                        android.util.Log.w("FxControl", "DRC Attack SeekBar not found");
                    }
                    
                    VerticalSeekBar releaseBar = drcSeekBars.get("release");
                    if (releaseBar != null) {
                        int progress = Math.max(DRC_RANGES[6], Math.min(DRC_RANGES[7], release)); // release范围10到2000
                        releaseBar.setProgress(progress - DRC_RANGES[6]);
                        android.util.Log.d("FxControl", "DRC Release set to progress: " + (progress - DRC_RANGES[6]));
                    } else {
                        android.util.Log.w("FxControl", "DRC Release SeekBar not found");
                    }
                }

                Toast.makeText(this, "DRC参数已同步", Toast.LENGTH_SHORT).show();
            } catch (Exception e) {
                android.util.Log.e("FxControl", "Error updating DRC UI with binary data", e);
            }
        });

        return true;
    }

    private boolean parseBinaryReverbData(byte[] data, int startIdx) {
        int idx = startIdx;
        byte roomSize = data[idx++];
        byte damping = data[idx++];
        byte wetDry = data[idx++];

        runOnUiThread(() -> {
            try {
                // 获取Reverb节点的SeekBar Map
                java.util.Map<String, VerticalSeekBar> reverbSeekBars = effectSeekBars.get(12);
                if (reverbSeekBars != null) {
                    android.util.Log.d("FxControl", "Reverb SeekBars found, updating values");
                    // 设置实际值
                    VerticalSeekBar roomBar = reverbSeekBars.get("room");
                    if (roomBar != null) {
                        roomBar.setProgress(roomSize & 0xFF);
                        android.util.Log.d("FxControl", "Reverb Room set to progress: " + (roomSize & 0xFF));
                    } else {
                        android.util.Log.w("FxControl", "Reverb Room SeekBar not found");
                    }
                    
                    VerticalSeekBar dampBar = reverbSeekBars.get("damp");
                    if (dampBar != null) {
                        dampBar.setProgress(damping & 0xFF);
                        android.util.Log.d("FxControl", "Reverb Damp set to progress: " + (damping & 0xFF));
                    } else {
                        android.util.Log.w("FxControl", "Reverb Damp SeekBar not found");
                    }
                    
                    VerticalSeekBar wetBar = reverbSeekBars.get("wet");
                    if (wetBar != null) {
                        wetBar.setProgress(wetDry & 0xFF);
                        android.util.Log.d("FxControl", "Reverb Wet set to progress: " + (wetDry & 0xFF));
                    } else {
                        android.util.Log.w("FxControl", "Reverb Wet SeekBar not found");
                    }
                }

                Toast.makeText(this, "混响参数已同步", Toast.LENGTH_SHORT).show();
            } catch (Exception e) {
                android.util.Log.e("FxControl", "Error updating Reverb UI with binary data", e);
            }
        });

        return true;
    }

    /**
     * 解析二进制EQ数据（如果收到的话）
     */
    private boolean parseBinaryEqData(byte[] data, int startIdx, int length) {
        // FxControlActivity主要处理DRC和Reverb，EQ数据由EqControlActivity处理
        // 这里只是为了完整性，如果收到EQ数据则简单记录
        android.util.Log.d("FxControl", "Received EQ data packet (length=" + length + "), forwarding not implemented");
        return true; // 返回true以避免错误日志
    }

    /**
     * 解析JSON格式的效果器数据（回退方法，用于兼容性）
     */
    private void parseJsonFxData(String data) {
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
            android.util.Log.e("FxControl", "Failed to parse JSON effect data: " + data, e);
        }
    }
}
