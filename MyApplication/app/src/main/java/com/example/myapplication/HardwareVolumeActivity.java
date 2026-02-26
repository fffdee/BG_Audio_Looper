package com.example.myapplication;

import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class HardwareVolumeActivity extends AppCompatActivity {
    private static final String[] CHANNELS = {"Guitar1", "Guitar2", "Mic1", "Mic2", "Output"};
    private static final String[] CMD_OPTS = {"-g1", "-g2", "-m1", "-m2", "-o"};
    private static final int[] DEFAULT_VALUES = {50, 50, 50, 50, 80};
    private static final String[] CHANNEL_COLORS = {"#FF6B6B", "#4ECDC4", "#45B7D1", "#FFA07A", "#98D8C8"};
    
    private BluetoothHelper bluetoothHelper;
    
    // 音量滑条和数值显示的引用
    private VerticalSeekBar[] volumeSeekBars = new VerticalSeekBar[5];
    private TextView[] volumeValueTexts = new TextView[5];
    
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
        
        setContentView(R.layout.activity_hardware_volume);

        // 初始化返回按钮
        ImageButton btnBack = findViewById(R.id.btn_back_volume);
        btnBack.setOnClickListener(v -> finish());

        // 初始化保存按钮
        ImageButton btnSave = findViewById(R.id.btn_save_volume);
        btnSave.setOnClickListener(v -> saveVolumeSettings());

        // 创建音量滑条
        LinearLayout volumeContainer = findViewById(R.id.volume_sliders_container);
        createVolumeSliders(volumeContainer);
        
        // 设置BLE通知监听器来接收查询响应
        setupBleNotificationListener();
        
        // 查询音量参数并同步到UI
        queryVolumeParams();
    }
    
    /**
     * 设置BLE通知监听器
     */
    private void setupBleNotificationListener() {
        if (bluetoothHelper != null) {
            bluetoothHelper.setBleNotifyListener(data -> {
                android.util.Log.d("VolumeControl", "BLE notify received: " + data);
                // 首先尝试解析二进制数据
                if (parseBinaryVolumeData(data)) {
                    android.util.Log.d("VolumeControl", "Parsed binary volume data successfully");
                } else {
                    // 如果二进制解析失败，回退到JSON解析
                    android.util.Log.d("VolumeControl", "Binary parsing failed, trying JSON parsing");
                    parseJsonVolumeData(data);
                }
            });
        }
    }
    
    /**
     * 解析二进制音量数据
     * 下位机格式: [0xAA][0x55][type(1)][length(1)][mic1(1)][mic2(1)][guitar1(1)][guitar2(1)][output(1)]
     * Android顺序: [guitar1][guitar2][mic1][mic2][output]
     */
    private boolean parseBinaryVolumeData(String data) {
        try {
            // 将十六进制字符串转换为字节数组
            byte[] bytes = hexStringToByteArray(data);
            if (bytes.length < 9) { // header(2) + type(1) + length(1) + 5 channels
                android.util.Log.w("VolumeControl", "Binary data too short: " + bytes.length);
                return false;
            }

            // 检查header
            if ((bytes[0] & 0xFF) != 0xAA || (bytes[1] & 0xFF) != 0x55) {
                android.util.Log.w("VolumeControl", "Invalid header: " + (bytes[0] & 0xFF) + ", " + (bytes[1] & 0xFF));
                return false;
            }

            int type = bytes[2] & 0xFF;
            int length = bytes[3] & 0xFF;

            if (type != 0x01) { // 音量数据类型
                android.util.Log.d("VolumeControl", "Not volume data type: " + type);
                return false;
            }

            if (length != 5 || bytes.length < 9) {
                android.util.Log.w("VolumeControl", "Invalid volume data length: " + length);
                return false;
            }

            // 下位机顺序: mic1, mic2, guitar1, guitar2, output
            // Android顺序: guitar1, guitar2, mic1, mic2, output
            int[] volumeValues = new int[5];
            volumeValues[0] = bytes[6] & 0xFF; // guitar1 (下位机第3个)
            volumeValues[1] = bytes[7] & 0xFF; // guitar2 (下位机第4个)
            volumeValues[2] = bytes[4] & 0xFF; // mic1 (下位机第1个)
            volumeValues[3] = bytes[5] & 0xFF; // mic2 (下位机第2个)
            volumeValues[4] = bytes[8] & 0xFF; // output (下位机第5个)

            android.util.Log.d("VolumeControl", "Parsed binary volume: guitar1=" + volumeValues[0] +
                ", guitar2=" + volumeValues[1] + ", mic1=" + volumeValues[2] +
                ", mic2=" + volumeValues[3] + ", output=" + volumeValues[4]);

            updateVolumeUI(volumeValues);
            return true;

        } catch (Exception e) {
            android.util.Log.e("VolumeControl", "Failed to parse binary volume data: " + data, e);
            return false;
        }
    }
    
    /**
     * 解析JSON音量数据（回退方法）
     */
    private void parseJsonVolumeData(String data) {
        try {
            org.json.JSONObject json = new org.json.JSONObject(data);
            if (json.has("volume")) {
                org.json.JSONObject volumeObj = json.getJSONObject("volume");
                android.util.Log.d("VolumeControl", "Parsed JSON volume object: " + volumeObj.toString());
                updateVolumeUI(volumeObj);
            } else {
                android.util.Log.d("VolumeControl", "No volume field in JSON");
            }
        } catch (org.json.JSONException e) {
            android.util.Log.e("VolumeControl", "Failed to parse JSON volume data: " + data, e);
        }
    }
    
    /**
     * 将十六进制字符串转换为字节数组
     */
    private byte[] hexStringToByteArray(String hex) {
        int len = hex.length();
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] = (byte) ((Character.digit(hex.charAt(i), 16) << 4)
                    + Character.digit(hex.charAt(i + 1), 16));
        }
        return data;
    }
    
    /**
     * 更新音量UI（二进制数据版本）
     */
    private void updateVolumeUI(int[] volumeValues) {
        android.util.Log.d("VolumeControl", "updateVolumeUI called with binary data");
        runOnUiThread(() -> {
            try {
                android.util.Log.d("VolumeControl", "Running on UI thread for binary data");
                
                for (int i = 0; i < volumeValues.length && i < volumeSeekBars.length; i++) {
                    int value = volumeValues[i];
                    android.util.Log.d("VolumeControl", "Updating channel " + i + " to " + value);
                    
                    // 更新滑条
                    if (volumeSeekBars[i] != null) {
                        volumeSeekBars[i].setProgress(value);
                        android.util.Log.d("VolumeControl", "SeekBar " + i + " updated to " + value);
                    } else {
                        android.util.Log.e("VolumeControl", "SeekBar " + i + " is null!");
                    }
                    
                    // 更新数值显示
                    if (volumeValueTexts[i] != null) {
                        volumeValueTexts[i].setText(String.valueOf(value));
                        android.util.Log.d("VolumeControl", "TextView " + i + " updated to " + value);
                    } else {
                        android.util.Log.e("VolumeControl", "TextView " + i + " is null!");
                    }
                    
                    android.util.Log.d("VolumeControl", "Updated " + CHANNELS[i] + " to " + value);
                }
                
                Toast.makeText(this, "音量参数已同步", Toast.LENGTH_SHORT).show();
                android.util.Log.d("VolumeControl", "UI update completed for binary data");
            } catch (Exception e) {
                android.util.Log.e("VolumeControl", "Failed to update volume UI with binary data", e);
                Toast.makeText(this, "解析音量数据失败", Toast.LENGTH_SHORT).show();
            }
        });
    }
    
    /**
     * 更新音量UI
     */
    private void updateVolumeUI(org.json.JSONObject volumeObj) {
        android.util.Log.d("VolumeControl", "updateVolumeUI called with: " + volumeObj.toString());
        runOnUiThread(() -> {
            try {
                android.util.Log.d("VolumeControl", "Running on UI thread");
                // 映射设备返回的字段名到UI索引
                String[] deviceFields = {"guitar1", "guitar2", "mic1", "mic2", "output"};
                
                for (int i = 0; i < deviceFields.length && i < volumeSeekBars.length; i++) {
                    if (volumeObj.has(deviceFields[i])) {
                        int value = volumeObj.getInt(deviceFields[i]);
                        android.util.Log.d("VolumeControl", "Updating " + deviceFields[i] + " to " + value);
                        
                        // 更新滑条
                        if (volumeSeekBars[i] != null) {
                            volumeSeekBars[i].setProgress(value);
                            android.util.Log.d("VolumeControl", "SeekBar " + i + " updated to " + value);
                        } else {
                            android.util.Log.e("VolumeControl", "SeekBar " + i + " is null!");
                        }
                        
                        // 更新数值显示
                        if (volumeValueTexts[i] != null) {
                            volumeValueTexts[i].setText(String.valueOf(value));
                            android.util.Log.d("VolumeControl", "TextView " + i + " updated to " + value);
                        } else {
                            android.util.Log.e("VolumeControl", "TextView " + i + " is null!");
                        }
                        
                        android.util.Log.d("VolumeControl", "Updated " + CHANNELS[i] + " to " + value);
                    } else {
                        android.util.Log.w("VolumeControl", "Volume object missing field: " + deviceFields[i]);
                    }
                }
                
                Toast.makeText(this, "音量参数已同步", Toast.LENGTH_SHORT).show();
                android.util.Log.d("VolumeControl", "UI update completed");
            } catch (org.json.JSONException e) {
                android.util.Log.e("VolumeControl", "Failed to update volume UI", e);
                Toast.makeText(this, "解析音量数据失败", Toast.LENGTH_SHORT).show();
            }
        });
    }
    
    /**
     * 查询音量参数
     */
    private void queryVolumeParams() {
        android.util.Log.d("VolumeControl", "queryVolumeParams called");
        sendQueryCommand("param -q volume");
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
                        android.util.Log.d("VolumeControl", "Query sent: " + cmd);
                    } else {
                        runOnUiThread(() -> Toast.makeText(this, "查询音量参数失败", Toast.LENGTH_SHORT).show());
                    }
                });
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        commandHandler.removeCallbacksAndMessages(null);
        commandQueue.clear();
    }

    /**
     * 创建音量滑条组
     */
    private void createVolumeSliders(LinearLayout container) {
        for (int i = 0; i < CHANNELS.length; i++) {
            final int index = i;
            String channel = CHANNELS[i];
            int defaultValue = DEFAULT_VALUES[i];
            String color = CHANNEL_COLORS[i];

            // 创建单个滑条容器
            LinearLayout sliderLayout = new LinearLayout(this);
            sliderLayout.setOrientation(LinearLayout.VERTICAL);
            sliderLayout.setGravity(Gravity.CENTER);
            LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.MATCH_PARENT, 1f);
            layoutParams.setMargins(8, 0, 8, 0);
            sliderLayout.setLayoutParams(layoutParams);

            // 音量值显示
            TextView valueText = new TextView(this);
            valueText.setText(String.valueOf(defaultValue));
            valueText.setTextSize(20);
            valueText.setTextColor(Color.parseColor(color));
            valueText.setGravity(Gravity.CENTER);
            valueText.setTextAlignment(View.TEXT_ALIGNMENT_CENTER);
            valueText.setPadding(0, 0, 0, 12);
            valueText.setTypeface(null, android.graphics.Typeface.BOLD);
            sliderLayout.addView(valueText);
            
            // 保存引用
            volumeValueTexts[i] = valueText;
            android.util.Log.d("VolumeControl", "Created TextView for " + channel + " at index " + i);

            // 竖向 SeekBar
            VerticalSeekBar seekBar = new VerticalSeekBar(this);
            seekBar.setMax(100);
            seekBar.setProgress(defaultValue);
            LinearLayout.LayoutParams seekBarParams = new LinearLayout.LayoutParams(
                80, 0, 1f);
            seekBar.setLayoutParams(seekBarParams);
            
            // 保存引用
            volumeSeekBars[i] = seekBar;
            android.util.Log.d("VolumeControl", "Created SeekBar for " + channel + " at index " + i);

            // 通道名称标签
            TextView labelText = new TextView(this);
            labelText.setText(channel);
            labelText.setTextSize(14);
            labelText.setTextColor(Color.WHITE);
            labelText.setGravity(Gravity.CENTER);
            labelText.setTextAlignment(View.TEXT_ALIGNMENT_CENTER);
            labelText.setPadding(0, 12, 0, 0);

            // 设置监听器
            seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    valueText.setText(String.valueOf(progress));
                }

                @Override
                public void onStartTrackingTouch(SeekBar seekBar) {}

                @Override
                public void onStopTrackingTouch(SeekBar seekBar) {
                    int value = seekBar.getProgress();
                    sendVolumeCommand(index, value);
                    Toast.makeText(HardwareVolumeActivity.this, 
                        channel + " 音量: " + value, Toast.LENGTH_SHORT).show();
                }
            });

            sliderLayout.addView(seekBar);
            sliderLayout.addView(labelText);
            container.addView(sliderLayout);
        }
    }

    /**
     * 发送音量命令
     */
    private void sendVolumeCommand(int index, int value) {
        queueCommand(() -> {
            if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
                String cmd = "audio " + CMD_OPTS[index] + " " + value + "\r\n";
                bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", 
                    cmd.getBytes(), null);
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
            // 延迟 150ms 再发送下一个命令
            commandHandler.postDelayed(() -> {
                isSendingCommand = false;
                processNextCommand();
            }, 150);
        } else {
            isSendingCommand = false;
        }
    }

    /**
     * 保存音量参数
     */
    private void saveVolumeSettings() {
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            queueCommand(() -> {
                bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", 
                    "audio -S\r\n".getBytes(), success -> {
                    runOnUiThread(() -> {
                        if (success) {
                            Toast.makeText(this, "音量参数已保存", Toast.LENGTH_SHORT).show();
                        } else {
                            Toast.makeText(this, "保存失败", Toast.LENGTH_SHORT).show();
                        }
                    });
                });
            });
        } else {
            Toast.makeText(this, "设备未连接", Toast.LENGTH_SHORT).show();
        }
    }
}
