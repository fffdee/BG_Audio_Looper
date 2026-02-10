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
        
        // 查询音量参数并同步到UI
        queryVolumeParams();
    }
    
    /**
     * 查询音量参数
     */
    private void queryVolumeParams() {
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

            // 竖向 SeekBar
            VerticalSeekBar seekBar = new VerticalSeekBar(this);
            seekBar.setMax(100);
            seekBar.setProgress(defaultValue);
            LinearLayout.LayoutParams seekBarParams = new LinearLayout.LayoutParams(
                80, 0, 1f);
            seekBar.setLayoutParams(seekBarParams);

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
