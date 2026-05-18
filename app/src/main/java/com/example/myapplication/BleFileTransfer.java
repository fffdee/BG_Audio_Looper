package com.example.myapplication;

import java.util.function.Consumer;

/**
 * BLE分块文件传输工具类
 * 用于通过BLE以分块方式传输文件，遵守MTU限制(≤250字节)
 * 
 * 协议:
 *   Header: 0xAA 0x55 (同步头, 2B)
 *   Command: (1B)
 *     0x01 = DATA (数据包)
 *     0x02 = END (传输结束)
 *     0x03 = QUERY (查询)
 *     0x04 = ABORT (中止)
 *   Sequence: (2B, little-endian)
 *   Length: (2B, little-endian)
 *   Payload: (0-4096B)
 *   CRC16-CCITT: (2B, little-endian)
 * 
 * 分块后单包大小: 7(header) + payload + 2(CRC) ≤ 250
 * => payload ≤ 241B
 * 实际使用: 240B payload per packet (安全边界)
 */
public class BleFileTransfer {
    
    public static final int MTU_SIZE = 250;
    public static final int HEADER_SIZE = 7;
    public static final int CRC_SIZE = 2;
    public static final int MAX_PAYLOAD_PER_PACKET = 240;
    
    // Proto commands
    public static final byte CMD_DATA = 0x01;
    public static final byte CMD_END = 0x02;
    public static final byte CMD_QUERY = 0x03;
    public static final byte CMD_ABORT = 0x04;
    
    // Responses
    public static final byte RSP_ACK = (byte) 0x81;
    public static final byte RSP_NAK = (byte) 0x82;
    public static final byte RSP_STATUS = (byte) 0x83;
    public static final byte RSP_READY = (byte) 0x84;
    
    // Status codes
    public static final byte STATUS_OK = 0x00;
    public static final byte STATUS_CRC_ERR = 0x01;
    public static final byte STATUS_FLASH_ERR = 0x02;
    
    private BluetoothHelper bluetoothHelper;
    private byte[] fileData;
    private long totalSize;
    private int packetSize;
    private Consumer<Integer> progressCallback;
    private Consumer<String> errorCallback;
    
    public BleFileTransfer(BluetoothHelper helper) {
        this.bluetoothHelper = helper;
        this.packetSize = MAX_PAYLOAD_PER_PACKET;
    }
    
    /**
     * 开始文件传输
     * @param filename 文件名 (用于设备端识别)
     * @param data 文件数据
     * @param progressCallback 进度回调 (字节数)
     * @param errorCallback 错误回调
     */
    public void startTransfer(String filename, byte[] data, 
                             Consumer<Integer> progressCallback,
                             Consumer<String> errorCallback) {
        this.fileData = data;
        this.totalSize = data.length;
        this.progressCallback = progressCallback;
        this.errorCallback = errorCallback;
        
        // 第一步: 发送初始化命令通知设备
        // (可选)
        
        // 第二步: 分块发送数据
        transferChunks();
    }
    
    /**
     * 分块发送文件数据
     */
    private void transferChunks() {
        int sequenceNum = 0;
        int offset = 0;
        
        while (offset < fileData.length) {
            int chunkLen = Math.min(packetSize, (int)(fileData.length - offset));
            
            // 构造数据包
            byte[] packet = buildPacket(CMD_DATA, sequenceNum, 
                fileData, offset, chunkLen);
            
            if (packet == null) {
                if (errorCallback != null) {
                    errorCallback.accept("Failed to build packet");
                }
                return;
            }
            
            // 发送数据包
            bluetoothHelper.writeCharacteristic(
                "0000ab01-0000-1000-8000-00805f9b34fb",
                packet, success -> {
                    if (!success) {
                        if (errorCallback != null) {
                            errorCallback.accept("BLE write failed");
                        }
                    }
                });
            
            offset += chunkLen;
            sequenceNum++;
            
            // 更新进度
            if (progressCallback != null) {
                final int currentOffset = offset;
                progressCallback.accept(currentOffset);
            }
            
            // 休眠以确保设备有时间处理 (MTU =250, 传输延迟~5ms)
            try {
                Thread.sleep(10);
            } catch (InterruptedException e) {
                // ignore
            }
        }
        
        // 发送END命令
        byte[] endPacket = buildPacket(CMD_END, sequenceNum, null, 0, 0);
        if (endPacket != null) {
            bluetoothHelper.writeCharacteristic(
                "0000ab01-0000-1000-8000-00805f9b34fb",
                endPacket, success -> {
                    if (success && progressCallback != null) {
                        progressCallback.accept((int)totalSize);
                    }
                });
        }
    }
    
    /**
     * 构造数据包
     * @param cmd 命令码
     * @param seq 序列号
     * @param payload 数据指针 (可为null)
     * @param offset 数据偏移
     * @param len 数据长度
     * @return 完整数据包字节数组
     */
    private byte[] buildPacket(byte cmd, int seq, byte[] payload, int offset, int len) {
        byte[] packet = new byte[HEADER_SIZE + len + CRC_SIZE];
        
        // Sync header
        packet[0] = (byte) 0xAA;
        packet[1] = (byte) 0x55;
        
        // Command
        packet[2] = cmd;
        
        // Sequence (little-endian)
        packet[3] = (byte) (seq & 0xFF);
        packet[4] = (byte) ((seq >> 8) & 0xFF);
        
        // Length (little-endian)
        packet[5] = (byte) (len & 0xFF);
        packet[6] = (byte) ((len >> 8) & 0xFF);
        
        // Payload
        if (payload != null && len > 0) {
            System.arraycopy(payload, offset, packet, HEADER_SIZE, len);
        }
        
        // CRC16-CCITT 计算 (Header + Payload)
        int crc = calculateCRC16(packet, 0, HEADER_SIZE + len);
        packet[HEADER_SIZE + len] = (byte) (crc & 0xFF);
        packet[HEADER_SIZE + len + 1] = (byte) ((crc >> 8) & 0xFF);
        
        return packet;
    }
    
    /**
     * CRC16-CCITT (XMODEM) 计算
     * Poly: 0x1021, Init: 0xFFFF
     */
    private int calculateCRC16(byte[] data, int offset, int len) {
        int crc = 0xFFFF;
        
        for (int i = offset; i < offset + len; i++) {
            crc ^= (data[i] & 0xFF) << 8;
            for (int j = 0; j < 8; j++) {
                if ((crc & 0x8000) != 0) {
                    crc = ((crc << 1) ^ 0x1021) & 0xFFFF;
                } else {
                    crc = (crc << 1) & 0xFFFF;
                }
            }
        }
        
        return crc;
    }
    
    /**
     * 中止传输
     */
    public void abort() {
        byte[] abortPacket = buildPacket(CMD_ABORT, 0, null, 0, 0);
        if (abortPacket != null) {
            bluetoothHelper.writeCharacteristic(
                "0000ab01-0000-1000-8000-00805f9b34fb",
                abortPacket, success -> {
                    // ignore
                });
        }
    }
    
    /**
     * 设置单包Payload大小 (默认240)
     * @param size 字节数
     */
    public void setPacketSize(int size) {
        if (size > MAX_PAYLOAD_PER_PACKET) {
            this.packetSize = MAX_PAYLOAD_PER_PACKET;
        } else {
            this.packetSize = size;
        }
    }
}
