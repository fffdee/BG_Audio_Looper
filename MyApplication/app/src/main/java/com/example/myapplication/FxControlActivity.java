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
        
        // 查询效果器参数并同步到UI
        queryEffectParams();
    }
    
    /**
     * 查询效果器参数
     */
    private void queryEffectParams() {
        // 查询DRC参数 (effect_id = 1)
        sendQueryCommand("effect query 1");
        // 查询混响参数 (effect_id = 0)
        sendQueryCommand("effect query 0");
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
     * 创建竖向滑条组
     */
    private void createVerticalSliders(LinearLayout container, int nodeId, String[] params, 
                                      int[] ranges, int[] defaults, String accentColor) {
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
        }
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
