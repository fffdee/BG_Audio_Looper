/**
 * @file looper_wav_ble_export.h
 * @brief Looper 录音段通过 BLE 导出 WAV 功能
 *
 * 功能：
 * - 单段导出：将指定段的音频数据通过 BLE 逐包发送到 App，App 端生成 WAV 文件
 * - 多段混音导出：将多个长度相同的段混音后导出
 * - 导出格式可选单声道或立体声
 * - 录制段为立体声 → 导出单声道时取左声道
 * - 录制段为单声道 → 导出立体声时复制到两个声道
 *
 * BLE 导出协议 (BLE_CMD_WAV_EXPORT = 0x40):
 *
 * [App → MCU] 子命令 0x01: 导出请求
 *   payload[0]    = 0x01 (子命令)
 *   payload[1]    = segment_mask (bit0~3 分别对应 seg0~seg3)
 *   payload[2]    = output_channels (1=mono, 2=stereo)
 *
 * [MCU → App] 子命令 0x02: 导出开始 (含 WAV 头)
 *   payload[0]    = 0x02 (子命令)
 *   payload[1..4] = total_packets (uint32_t LE, 数据包总数)
 *   payload[5..8] = total_data_bytes (uint32_t LE, 音频数据总字节数)
 *   payload[9..52]= WAV_Header (44 字节)
 *
 * [MCU → App] 子命令 0x03: 数据包
 *   payload[0]    = 0x03 (子命令)
 *   payload[1..4] = packet_index (uint32_t LE)
 *   payload[5..N] = PCM data (最多 192 字节)
 *
 * [MCU → App] 子命令 0x04: 导出完成
 *   payload[0]    = 0x04 (子命令)
 *   payload[1]    = result (0=OK, 1=error)
 *
 * [App → MCU] 子命令 0x05: 取消导出
 *   payload[0]    = 0x05 (子命令)
 */

#ifndef __LOOPER_WAV_BLE_EXPORT_H__
#define __LOOPER_WAV_BLE_EXPORT_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 子命令定义
 * ============================================ */
#define WAV_BLE_SUBCMD_EXPORT_REQ     0x01  /* App → MCU: 导出请求 */
#define WAV_BLE_SUBCMD_EXPORT_START   0x02  /* MCU → App: 导出开始 */
#define WAV_BLE_SUBCMD_DATA_PACKET    0x03  /* MCU → App: 数据包 */
#define WAV_BLE_SUBCMD_EXPORT_END     0x04  /* MCU → App: 导出完成 */
#define WAV_BLE_SUBCMD_CANCEL         0x05  /* App → MCU: 取消导出 */

/* 每个 BLE 数据包中的 PCM 数据最大字节数
 * BLE_Send 单次 GattServerNotify 最多 200 字节，超出则分块且尾块频繁失败。
 * 帧总长 = HDR(4) + payload + CRC(2) ≤ 200
 * payload = subcmd(1) + packet_index(4) + pcm_data ≤ 194
 * pcm_data ≤ 189，取 188 对齐到 4 字节（一个立体声采样 = 4 字节）
 * 注意：send_frame 使用 static buf，BLE_Send 异步写，不能 burst 连发。*/
#define WAV_BLE_DATA_PER_PACKET   188

/* 导出结果码 */
#define WAV_BLE_RESULT_OK         0
#define WAV_BLE_RESULT_ERROR      1
#define WAV_BLE_RESULT_CANCELLED  2
#define WAV_BLE_RESULT_BUSY       3
#define WAV_BLE_RESULT_NO_DATA    4
#define WAV_BLE_RESULT_LEN_MISMATCH 5  /* 多段长度不一致 */

/* ============================================
 * 公共接口
 * ============================================ */

/**
 * @brief 处理 App 发来的 WAV 导出命令
 * @param payload BLE 帧 payload
 * @param len     payload 长度
 *
 * 由 BLE 数据处理回调调用，当 cmd == BLE_CMD_WAV_EXPORT 时
 */
void LooperWavBle_HandleCommand(const uint8_t *payload, uint8_t len);

/**
 * @brief 导出处理 tick（在主循环中调用）
 *
 * 每次调用发送一个 BLE 数据包。
 * 导出不在进行时立即返回。
 */
void LooperWavBle_ProcessTick(void);

/**
 * @brief 查询导出是否正在进行
 * @return true = 导出中, false = 空闲
 */
bool LooperWavBle_IsBusy(void);

#ifdef __cplusplus
}
#endif

#endif /* __LOOPER_WAV_BLE_EXPORT_H__ */
