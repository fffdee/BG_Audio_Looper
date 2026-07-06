package com.example.myapplication;

import android.util.Log;
import java.util.zip.CRC32;

public class BleProtocol {
    private static final String TAG = "BleProtocol";

    public static final byte HEADER_0 = (byte) 0xAA;
    public static final byte HEADER_1 = (byte) 0x55;
    public static final int HDR_SIZE = 4;
    public static final int CRC_SIZE = 2;
    public static final int MAX_PAYLOAD = 200;
    public static final int MAX_RETRIES = 5;
    public static final long ACK_TIMEOUT_MS = 300;

    public static final byte CMD_ACK        = 0x00;
    public static final byte CMD_NACK       = 0x01;
    public static final byte CMD_SYNC_REQ   = 0x02;
    public static final byte CMD_SYNC_START = 0x03;
    public static final byte CMD_SYNC_END   = 0x04;
    public static final byte CMD_DRC        = 0x10;
    public static final byte CMD_REVERB     = 0x11;
    public static final byte CMD_EQ         = 0x12;
    public static final byte CMD_DELAY      = 0x13;
    public static final byte CMD_GAIN       = 0x14;
    public static final byte CMD_LOOPER          = 0x20;
    public static final byte CMD_LOOPER_SEG_STATE  = 0x21;  /* MCU→App: 各段运行时状态（重连同步用）
                                                              * payload: [s0,s1,s2,s3,
                                                              *           s0_len_lo,s0_len_hi, s1_len_lo,s1_len_hi,
                                                              *           s2_len_lo,s2_len_hi, s3_len_lo,s3_len_hi]
                                                              * state: 0=INACTIVE 1=RECORDING 2=PLAYING 3=STOPPED
                                                              * len: 段长度（pages, uint16_t LE） */
    public static final byte CMD_VOLUME          = 0x30;
    public static final byte CMD_METRONOME  = 0x31;
    public static final byte CMD_SYSTEM          = 0x32;
    public static final byte CMD_BATTERY_CALIB   = 0x33;
    public static final byte CMD_WAV_EXPORT       = 0x40;

    /* CMD_SYSTEM sub-types */
    public static final byte SYSTEM_SUB_BATTERY   = 0x01;
    public static final byte SYSTEM_SUB_STATE     = 0x02;
    public static final byte SYSTEM_SUB_LP_STATE   = 0x03;
    public static final byte SYSTEM_SUB_LP_TIMEOUT = 0x04;
    public static final byte SYSTEM_SUB_PRODUCT_ID = 0x05;

    /* Product IDs (match MCU BG_PRODUCT_ID_xxx) */
    public static final int PRODUCT_ID_BANBOX = 0x0001;

    /* SYS_STATE values (payload[1] when sub-type == SYSTEM_SUB_STATE) */
    public static final byte SYS_STATE_IDLE       = 0x00;
    public static final byte SYS_STATE_NORMAL     = 0x01;
    public static final byte SYS_STATE_TRANSFER   = 0x02;

    /* Looper runtime owner modes (CMD_LOOPER payload extension) */
    public static final int LOOPER_RUN_MODE_IDLE    = 0;
    public static final int LOOPER_RUN_MODE_OFFLINE = 1;
    public static final int LOOPER_RUN_MODE_ONLINE  = 2;

    /* CMD_BATTERY_CALIB sub-types (App → MCU) */
    public static final byte CALIB_CMD_START      = 0x01;
    public static final byte CALIB_CMD_STOP       = 0x02;
    public static final byte CALIB_CMD_STATUS     = 0x03;
    public static final byte CALIB_CMD_CLEAR      = 0x04;
    /* CMD_BATTERY_CALIB sub-types (MCU → App) */
    public static final byte CALIB_CMD_STATUS_RSP = (byte) 0x83;

    public static boolean isDataCmd(byte cmd) {
        return (cmd & 0xFF) >= 0x10;
    }

    public static class Frame {
        public byte cmd;
        public byte seq;
        public byte len;
        public byte[] payload;

        public Frame(byte cmd, byte seq, byte[] payload, int payloadLen) {
            this.cmd = cmd;
            this.seq = seq;
            this.len = (byte) payloadLen;
            this.payload = new byte[payloadLen];
            if (payload != null && payloadLen > 0) {
                System.arraycopy(payload, 0, this.payload, 0, payloadLen);
            }
        }
    }

    private static final int[] CRC16_TABLE = {
        0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
        0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
        0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
        0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
        0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
        0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
        0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
        0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
        0x4864,0x5845,0x6826,0x7807,0x08E0,0x18C1,0x28A2,0x38C3,
        0xC92C,0xD90D,0xE96E,0xF94F,0x89A8,0x9989,0xA9EA,0xB9CB,
        0x5A15,0x4A34,0x7A57,0x6A76,0x1A91,0x0AB0,0x3AD3,0x2AF2,
        0xDB1D,0xCB3C,0xFB5F,0xEB7E,0x9B99,0x8BB8,0xBBDB,0xABFA,
        0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
        0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
        0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
        0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
        0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
        0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
        0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
        0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
        0xB5EA,0xA5CB,0x95A8,0x85A9,0xF56E,0xE54F,0xD52C,0xC50D,
        0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
        0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
        0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
        0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
        0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
        0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
        0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
        0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
        0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
        0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
        0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0
    };

    public static int crc16(byte[] data, int offset, int len) {
        int crc = 0xFFFF;
        for (int i = 0; i < len; i++) {
            crc = ((crc << 8) ^ CRC16_TABLE[((crc >> 8) ^ (data[offset + i] & 0xFF)) & 0xFF]) & 0xFFFF;
        }
        return crc;
    }

    public static byte[] encode(Frame frame) {
        int totalLen = HDR_SIZE + (frame.len & 0xFF) + CRC_SIZE;
        byte[] out = new byte[totalLen];
        out[0] = HEADER_0;
        out[1] = HEADER_1;
        out[2] = frame.cmd;
        out[3] = frame.seq;
        int payloadLen = frame.len & 0xFF;
        if (payloadLen > 0 && frame.payload != null) {
            System.arraycopy(frame.payload, 0, out, HDR_SIZE, payloadLen);
        }
        int crc = crc16(out, 0, HDR_SIZE + payloadLen);
        out[HDR_SIZE + payloadLen] = (byte) ((crc >> 8) & 0xFF);
        out[HDR_SIZE + payloadLen + 1] = (byte) (crc & 0xFF);
        return out;
    }

    public static Frame decode(byte[] data, int offset, int len) {
        if (len < HDR_SIZE + CRC_SIZE) return null;
        if (data[offset] != HEADER_0 || data[offset + 1] != HEADER_1) return null;

        byte cmd = data[offset + 2];
        byte seq = data[offset + 3];
        int payloadLen = len - HDR_SIZE - CRC_SIZE;
        if (payloadLen < 0 || payloadLen > MAX_PAYLOAD) return null;

        int crcCalc = crc16(data, offset, HDR_SIZE + payloadLen);
        int crcRecv = ((data[offset + HDR_SIZE + payloadLen] & 0xFF) << 8) |
                      (data[offset + HDR_SIZE + payloadLen + 1] & 0xFF);
        if (crcCalc != crcRecv) {
            Log.w(TAG, String.format("CRC mismatch: calc=0x%04X recv=0x%04X", crcCalc, crcRecv));
            return null;
        }

        byte[] payload = new byte[payloadLen];
        if (payloadLen > 0) {
            System.arraycopy(data, offset + HDR_SIZE, payload, 0, payloadLen);
        }
        return new Frame(cmd, seq, payload, payloadLen);
    }

    public static byte[] buildAck(byte seq, byte ackedCmd) {
        Frame ack = new Frame(CMD_ACK, seq, new byte[]{ackedCmd}, 1);
        return encode(ack);
    }

    public static byte[] buildNack(byte seq) {
        Frame nack = new Frame(CMD_NACK, seq, null, 0);
        return encode(nack);
    }

    public static byte[] buildSyncReq(byte seq) {
        Frame req = new Frame(CMD_SYNC_REQ, seq, null, 0);
        return encode(req);
    }

    public static String cmdName(byte cmd) {
        switch (cmd) {
            case CMD_ACK:        return "ACK";
            case CMD_NACK:       return "NACK";
            case CMD_SYNC_REQ:   return "SYNC_REQ";
            case CMD_SYNC_START: return "SYNC_START";
            case CMD_SYNC_END:   return "SYNC_END";
            case CMD_DRC:        return "DRC";
            case CMD_REVERB:     return "REVERB";
            case CMD_EQ:         return "EQ";
            case CMD_DELAY:      return "DELAY";
            case CMD_GAIN:       return "GAIN";
            case CMD_LOOPER:          return "LOOPER";
            case CMD_LOOPER_SEG_STATE: return "LOOPER_SEG_STATE";
            case CMD_VOLUME:          return "VOLUME";
            case CMD_METRONOME:  return "METRONOME";
            case CMD_SYSTEM:        return "SYSTEM";
            case CMD_BATTERY_CALIB: return "BATTERY_CALIB";
            case CMD_WAV_EXPORT:    return "WAV_EXPORT";
            default:             return String.format("0x%02X", cmd);
        }
    }
}
