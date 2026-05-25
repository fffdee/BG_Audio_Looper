package com.example.myapplication;

import android.bluetooth.BluetoothGatt;
import android.os.Bundle;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import java.util.ArrayList;
import java.util.List;

public class AudioChainDiagramActivity extends BaseActivity {
    private BluetoothHelper bluetoothHelper;
    private AudioChainView audioChainView;
    private int hardwareVariant = 0; // 默认硬件变体 00

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // 获取全局BluetoothHelper实例
        bluetoothHelper = com.example.myapplication.BluetoothManager.getInstance().getBluetoothHelper();

        setContentView(R.layout.activity_audio_chain_diagram);
        setupBaseToolbar(true);

        audioChainView = findViewById(R.id.audio_chain_view);

        // 重置视图按钮
        ImageButton btnResetView = findViewById(R.id.btn_reset_view);
        btnResetView.setOnClickListener(v -> {
            audioChainView.resetView();
            Toast.makeText(this, "已重置视图", Toast.LENGTH_SHORT).show();
        });

        // 检测硬件版本
        detectHardwareVariant();

        // 显示音频链路图
        displayAudioChain();
    }

    @Override
    protected String getToolbarTitle() {
        return "音频信号链路图";
    }

    /**
     * 检测硬件变体
     * 从 BLE 设备的 MAC 地址中提取第三个字节
     */
    private void detectHardwareVariant() {
        try {
            BluetoothGatt gatt = bluetoothHelper.getBluetoothGatt();
            if (gatt != null && gatt.getDevice() != null) {
                String address = gatt.getDevice().getAddress(); // 格式: XX:XX:XX:XX:XX:XX
                
                // 解析 MAC 地址
                String[] parts = address.split(":");
                if (parts.length >= 6) {
                    // 检查最后两字节是否为 42:47
                    if (parts[4].equalsIgnoreCase("42") && parts[5].equalsIgnoreCase("47")) {
                        hardwareVariant = 0;
                    } else {
                        try {
                            hardwareVariant = Integer.parseInt(parts[2], 16);
                        } catch (NumberFormatException e) {
                            hardwareVariant = 0;
                        }
                    }
                }
                
                Toast.makeText(this, "硬件变体: 0x" + 
                    String.format("%02X", hardwareVariant), Toast.LENGTH_SHORT).show();
            }
        } catch (Exception e) {
            Toast.makeText(this, "无法获取设备地址", Toast.LENGTH_SHORT).show();
        }
    }

    /**
     * 显示音频链路图
     * 根据硬件变体选择对应的音频链路配置
     */
    private void displayAudioChain() {
        switch (hardwareVariant) {
            case 0x00:
            default:
                displayDefaultAudioChain();
                break;
        }
    }

    /**
     * 显示默认音频链路图 (硬件变体 0x00)
     * 基于 effect_graph_config.h 中的 DEFAULT_NODES_CONFIG
     */
    private void displayDefaultAudioChain() {
        List<AudioChainView.AudioNode> nodes = new ArrayList<>();
        List<AudioChainView.AudioEdge> edges = new ArrayList<>();
        
        // 布局参数
        float startX = 50;
        float startY = 50;
        float xGap = 800;
        float yGap = 350;
        
        // 第1列: ADC 输入源 (Source - 绿色)
        nodes.add(new AudioChainView.AudioNode(0, "ADC0\nGuitar", 0, startX, startY)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        nodes.add(new AudioChainView.AudioNode(1, "ADC1\nMic", 0, startX, startY + yGap * 2)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        
        // 第2列: L/R 分离后的 EQ (Effect - 蓝色)
        nodes.add(new AudioChainView.AudioNode(4, "EQ\nGuitar L", 1, startX + xGap, startY - yGap / 2)
            .setChannels(1).setBitDepth(32).setSampleRate(48000));
        nodes.add(new AudioChainView.AudioNode(5, "EQ\nGuitar R", 1, startX + xGap, startY + yGap / 2)
            .setChannels(1).setBitDepth(32).setSampleRate(48000));
        nodes.add(new AudioChainView.AudioNode(6, "EQ\nMic L", 1, startX + xGap, startY + yGap * 1.5f)
            .setChannels(1).setBitDepth(32).setSampleRate(48000));
        nodes.add(new AudioChainView.AudioNode(7, "EQ\nMic R", 1, startX + xGap, startY + yGap * 2.5f)
            .setChannels(1).setBitDepth(32).setSampleRate(48000));
        
        // 第3列: ADC Mixer (Mixer - 橙色)
        nodes.add(new AudioChainView.AudioNode(8, "ADC\nMixer", 2, startX + xGap * 2, startY + yGap)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        
        // 第4列: 动态处理 (Effect - 蓝色)
        nodes.add(new AudioChainView.AudioNode(9, "Expander", 1, startX + xGap * 3, startY + yGap)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        nodes.add(new AudioChainView.AudioNode(10, "DRC", 1, startX + xGap * 4, startY + yGap)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        
        // 第5列: Pre-Reverb Mixer + Looper Play (Mixer + Source)
        nodes.add(new AudioChainView.AudioNode(11, "Pre-Reverb\nMixer", 2, startX + xGap * 5, startY + yGap)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        nodes.add(new AudioChainView.AudioNode(19, "Looper\nPlay", 0, startX + xGap * 4.5f, startY + yGap * 3)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        
        // 第6列: Reverb (Effect - 蓝色)
        nodes.add(new AudioChainView.AudioNode(12, "Reverb", 1, startX + xGap * 6, startY + yGap)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        
        // USB/BT 输入路径 (下方)
        nodes.add(new AudioChainView.AudioNode(2, "USB In", 0, startX, startY + yGap * 5)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        nodes.add(new AudioChainView.AudioNode(3, "BT In", 0, startX, startY + yGap * 6)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        nodes.add(new AudioChainView.AudioNode(18, "Metronome", 0, startX, startY + yGap * 7)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        
        nodes.add(new AudioChainView.AudioNode(13, "USB/BT\nMixer", 2, startX + xGap * 2, startY + yGap * 6)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        nodes.add(new AudioChainView.AudioNode(14, "USB/BT\nEQ", 1, startX + xGap * 3, startY + yGap * 6)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        
        // 第7列: Final Mixer (Mixer - 橙色)
        nodes.add(new AudioChainView.AudioNode(15, "Final\nMixer", 2, startX + xGap * 7, startY + yGap * 3.5f)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        
        // 第8列: 输出 (Sink - 红色)
        nodes.add(new AudioChainView.AudioNode(16, "DAC Out", 3, startX + xGap * 8, startY + yGap * 3)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        nodes.add(new AudioChainView.AudioNode(17, "USB Out", 3, startX + xGap * 8, startY + yGap * 4)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        
        // Looper Record (从 ADC Mixer)
        nodes.add(new AudioChainView.AudioNode(20, "Looper\nRecord", 3, startX + xGap * 2, startY + yGap * 8)
            .setChannels(2).setBitDepth(32).setSampleRate(48000));
        
        // 连接关系
        // ADC0 -> EQ L/R
        edges.add(new AudioChainView.AudioEdge(0, 4));
        edges.add(new AudioChainView.AudioEdge(0, 5));
        
        // ADC1 -> EQ L/R
        edges.add(new AudioChainView.AudioEdge(1, 6));
        edges.add(new AudioChainView.AudioEdge(1, 7));
        
        // EQ -> ADC Mixer
        edges.add(new AudioChainView.AudioEdge(4, 8));
        edges.add(new AudioChainView.AudioEdge(5, 8));
        edges.add(new AudioChainView.AudioEdge(6, 8));
        edges.add(new AudioChainView.AudioEdge(7, 8));
        
        // ADC Mixer -> Expander -> DRC -> Pre-Reverb Mixer
        edges.add(new AudioChainView.AudioEdge(8, 9));
        edges.add(new AudioChainView.AudioEdge(9, 10));
        edges.add(new AudioChainView.AudioEdge(10, 11));
        
        // Looper Play -> Pre-Reverb Mixer
        edges.add(new AudioChainView.AudioEdge(19, 11));
        
        // Pre-Reverb Mixer -> Reverb
        edges.add(new AudioChainView.AudioEdge(11, 12));
        
        // USB/BT 路径
        edges.add(new AudioChainView.AudioEdge(2, 13));
        edges.add(new AudioChainView.AudioEdge(3, 13));
        edges.add(new AudioChainView.AudioEdge(18, 13));
        edges.add(new AudioChainView.AudioEdge(13, 14));
        
        // Reverb + USB/BT EQ -> Final Mixer
        edges.add(new AudioChainView.AudioEdge(12, 15));
        edges.add(new AudioChainView.AudioEdge(14, 15));
        
        // Final Mixer -> Outputs
        edges.add(new AudioChainView.AudioEdge(15, 16));
        edges.add(new AudioChainView.AudioEdge(15, 17));
        
        // ADC Mixer -> Looper Record
        edges.add(new AudioChainView.AudioEdge(8, 20));
        
        audioChainView.setAudioChain(nodes, edges);
    }
}
