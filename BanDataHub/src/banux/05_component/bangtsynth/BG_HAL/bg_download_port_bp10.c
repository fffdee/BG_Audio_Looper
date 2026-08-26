#include "bangtsynth_legacy.h"
#if BANGTSYNTH_LEGACY

/**
 * @file bg_download_port_bp10.c
 * @brief BG_Download_Port — BP10 平台实现 (数据包协议)
 * 
 * 功能:
 * - 通过 Shell IO (USB CDC / BLE SPP) 接收音源数据包
 * - 解析 DL 协议数据包 (同步头 + 命令 + 序列号 + 数据 + CRC)
 * - 发送 ACK/NAK 响应
 * - 提供 bg_download_port_read() 接口供 soundbank_download() 调用
 * 
 * 协议流程:
 * 1. Shell 命令 "sb -d <size>" 启动下载会话
 * 2. 设备发送 READY 响应
 * 3. 主机发送 DL_CMD_DATA 数据包 (每包最大 4096 字节)
 * 4. 设备校验 CRC → 返回 ACK/NAK
 * 5. soundbank_download() 将有效载荷写入 Flash
 * 6. 主机发送 DL_CMD_END 结束传输
 * 
 * 编译条件: BG_TARGET_PLATFORM == BG_PLATFORM_BP10
 */

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "bg_config.h"
#if (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)

#include "bg_download_port.h"
#include "bg_soundbank_dl_protocol.h"
#include "bg_log.h"
#include "bg_shell.h"
#include <string.h>

/* FreeRTOS for vTaskDelay */
#include "FreeRTOS.h"
#include "task.h"

/* ============================================
 * 内部配置
 * ============================================ */
#define DL_POLL_INTERVAL_MS     1       /* IO 轮询间隔 */
#define DL_SYNC_SCAN_MAX        8192    /* 同步头最大扫描字节数 */

/* ============================================
 * 接收缓冲区
 * Header(7) + Payload(4096) + CRC(2) = 4105 max
 * ============================================ */
#define DL_RX_BUF_SIZE  (DL_HEADER_SIZE + DL_MAX_PAYLOAD_SIZE + DL_CRC_SIZE)
static uint8_t g_rx_buf[DL_RX_BUF_SIZE];

/* ============================================
 * 下载会话状态
 * ============================================ */
static DL_Session_t g_dl_session = {0, 0, 0, 0, 0, 0};
static uint8_t g_dl_end_flag = 0;

/* ============================================
 * 内部函数声明
 * ============================================ */
static int dl_recv_bytes(uint8_t *buf, uint16_t need, uint32_t timeout_ms);
static int dl_wait_sync(uint32_t timeout_ms);
static int dl_recv_packet(uint8_t *cmd, uint16_t *seq, uint8_t *payload,
                          uint16_t *payload_len, uint32_t timeout_ms);
static void dl_send_response(uint8_t rsp, uint16_t seq, uint8_t status);

/* ============================================
 * 下载会话管理 (供 Shell 命令调用)
 * ============================================ */

/**
 * 初始化下载会话
 * @param total_size  预期总字节数 (0=未知大小)
 */
void bg_download_port_session_init(uint32_t total_size)
{
    memset(&g_dl_session, 0, sizeof(g_dl_session));
    g_dl_session.total_size = total_size;
    g_dl_session.active = 1;
    g_dl_end_flag = 0;

    BG_LOG_I(BG_LOG_TAG_HAL, "[DL] Session init, expect %u bytes\n", total_size);
}

/**
 * 结束下载会话
 */
void bg_download_port_session_deinit(void)
{
    g_dl_session.active = 0;
    g_dl_end_flag = 0;
    BG_LOG_I(BG_LOG_TAG_HAL, "[DL] Session closed, received %u bytes\n",
             g_dl_session.bytes_received);
}

/**
 * 获取下载会话信息 (只读)
 */
const DL_Session_t* bg_download_port_get_session(void)
{
    return &g_dl_session;
}

/* ============================================
 * bg_download_port_read — 数据包协议接收
 *
 * soundbank_download() 在循环中调用此函数。
 * 每次调用接收一个完整数据包, 返回有效载荷。
 * 返回 bytes_read=0 表示传输结束。
 * ============================================ */

int bg_download_port_read(const char *source, void *buffer, size_t size, size_t *bytes_read)
{
    uint8_t cmd;
    uint16_t seq;
    uint16_t payload_len;
    int ret;

    (void)source;  /* BP10 通过 Shell IO 接收, 忽略 source */

    if (!buffer || !bytes_read) {
        return -1;
    }

    *bytes_read = 0;

    /* 检查会话状态 */
    if (!g_dl_session.active) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[DL] No active session\n");
        return -1;
    }

    /* 已收到 END 命令 → 返回 0 字节表示完成 */
    if (g_dl_end_flag) {
        return 0;
    }

    /* 接收一个数据包 */
    payload_len = 0;
    ret = dl_recv_packet(&cmd, &seq, (uint8_t *)buffer, &payload_len, DL_RECV_TIMEOUT_MS);
    if (ret < 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[DL] Packet receive failed\n");
        g_dl_session.last_status = DL_STATUS_TIMEOUT;
        return -1;
    }

    /* 按命令类型处理 */
    switch (cmd) {

    case DL_CMD_DATA:
        /* 序列号检查 */
        if (seq != g_dl_session.expected_seq) {
            BG_LOG_W(BG_LOG_TAG_HAL, "[DL] Seq mismatch: expect %u got %u\n",
                     g_dl_session.expected_seq, seq);
            dl_send_response(DL_RSP_NAK, seq, DL_STATUS_SEQ_ERR);
            return -1;
        }
        /* 溢出检查 */
        if (g_dl_session.total_size > 0 &&
            g_dl_session.bytes_received + payload_len > g_dl_session.total_size) {
            BG_LOG_E(BG_LOG_TAG_HAL, "[DL] Data overflow!\n");
            dl_send_response(DL_RSP_NAK, seq, DL_STATUS_OVERFLOW);
            return -1;
        }
        /* 更新会话 */
        g_dl_session.bytes_received += payload_len;
        g_dl_session.expected_seq = seq + 1;
        g_dl_session.last_status = DL_STATUS_OK;
        *bytes_read = payload_len;

        dl_send_response(DL_RSP_ACK, seq, DL_STATUS_OK);

        BG_LOG_D(BG_LOG_TAG_HAL, "[DL] Pkt#%u OK %u B (total %u/%u)\n",
                 seq, payload_len,
                 g_dl_session.bytes_received, g_dl_session.total_size);
        break;

    case DL_CMD_END:
        g_dl_end_flag = 1;
        *bytes_read = 0;
        g_dl_session.last_status = DL_STATUS_OK;
        dl_send_response(DL_RSP_ACK, seq, DL_STATUS_OK);
        BG_LOG_I(BG_LOG_TAG_HAL, "[DL] END received, total %u bytes\n",
                 g_dl_session.bytes_received);
        break;

    case DL_CMD_ABORT:
        g_dl_session.active = 0;
        g_dl_session.last_status = DL_STATUS_ABORT;
        dl_send_response(DL_RSP_ACK, seq, DL_STATUS_ABORT);
        BG_LOG_W(BG_LOG_TAG_HAL, "[DL] Aborted by host\n");
        return -1;

    case DL_CMD_QUERY:
        /* 状态查询 — 回复进度, 不产生数据 */
        dl_send_response(DL_RSP_STATUS, seq, DL_STATUS_OK);
        /* 继续等待下一个数据包 */
        return bg_download_port_read(source, buffer, size, bytes_read);

    default:
        BG_LOG_W(BG_LOG_TAG_HAL, "[DL] Unknown cmd 0x%02X\n", cmd);
        dl_send_response(DL_RSP_NAK, seq, DL_STATUS_CRC_ERR);
        return -1;
    }

    return 0;
}

/* ============================================
 * 底层收发
 * ============================================ */

/**
 * 从 Shell IO 阻塞接收 need 个字节 (带超时)
 */
static int dl_recv_bytes(uint8_t *buf, uint16_t need, uint32_t timeout_ms)
{
    uint16_t received = 0;
    uint32_t elapsed  = 0;
    uint16_t n;

    while (received < need && elapsed < timeout_ms) {
        n = Shell_RecvRaw(buf + received, need - received);
        if (n > 0) {
            received += n;
        } else {
            vTaskDelay(DL_POLL_INTERVAL_MS);
            elapsed += DL_POLL_INTERVAL_MS;
        }
    }

    return (received >= need) ? 0 : -1;
}

/**
 * 等待同步头 0xAA 0x55, 丢弃其它字节
 */
static int dl_wait_sync(uint32_t timeout_ms)
{
    uint8_t byte;
    uint32_t elapsed = 0;
    uint32_t scanned = 0;
    uint16_t n;
    int state = 0;  /* 0=等待0xAA, 1=等待0x55 */

    while (elapsed < timeout_ms && scanned < DL_SYNC_SCAN_MAX) {
        n = Shell_RecvRaw(&byte, 1);
        if (n == 0) {
            vTaskDelay(DL_POLL_INTERVAL_MS);
            elapsed += DL_POLL_INTERVAL_MS;
            continue;
        }
        scanned++;

        if (state == 0) {
            if (byte == DL_SYNC_BYTE_H) {
                state = 1;
            }
        } else {
            if (byte == DL_SYNC_BYTE_L) {
                return 0;  /* 找到同步头 */
            } else if (byte == DL_SYNC_BYTE_H) {
                state = 1; /* 连续 0xAA, 继续等 0x55 */
            } else {
                state = 0; /* 重新搜索 */
            }
        }
    }
    return -1;
}

/**
 * 接收并解析一个完整数据包
 */
static int dl_recv_packet(uint8_t *cmd, uint16_t *seq, uint8_t *payload,
                          uint16_t *payload_len, uint32_t timeout_ms)
{
    uint8_t hdr[5];   /* cmd(1) + seq(2) + len(2) */
    uint8_t crc_buf[2];
    uint16_t pkt_len;
    uint16_t crc_recv;
    uint16_t crc_calc;
    uint16_t crc_data_len;
    int ret;

    /* 1. 等待同步头 */
    ret = dl_wait_sync(timeout_ms);
    if (ret < 0) {
        return -1;
    }

    /* 2. 接收头部 */
    ret = dl_recv_bytes(hdr, 5, timeout_ms);
    if (ret < 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[DL] Header timeout\n");
        return -1;
    }

    *cmd = hdr[0];
    *seq = (uint16_t)hdr[1] | ((uint16_t)hdr[2] << 8);
    pkt_len = (uint16_t)hdr[3] | ((uint16_t)hdr[4] << 8);

    /* 3. 有效载荷长度检查 */
    if (pkt_len > DL_MAX_PAYLOAD_SIZE) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[DL] Payload too large: %u\n", pkt_len);
        return -1;
    }

    /* 4. 接收有效载荷 */
    if (pkt_len > 0) {
        ret = dl_recv_bytes(payload, pkt_len, timeout_ms);
        if (ret < 0) {
            BG_LOG_E(BG_LOG_TAG_HAL, "[DL] Payload timeout (%u B)\n", pkt_len);
            return -1;
        }
    }
    *payload_len = pkt_len;

    /* 5. 接收 CRC */
    ret = dl_recv_bytes(crc_buf, 2, timeout_ms);
    if (ret < 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[DL] CRC timeout\n");
        return -1;
    }
    crc_recv = (uint16_t)crc_buf[0] | ((uint16_t)crc_buf[1] << 8);

    /* 6. 验证 CRC: 范围 = hdr(5) + payload(N) */
    crc_data_len = 5 + pkt_len;
    memcpy(g_rx_buf, hdr, 5);
    if (pkt_len > 0) {
        memcpy(g_rx_buf + 5, payload, pkt_len);
    }
    crc_calc = dl_crc16(g_rx_buf, crc_data_len);

    if (crc_calc != crc_recv) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[DL] CRC fail: calc=0x%04X recv=0x%04X\n",
                 crc_calc, crc_recv);
        dl_send_response(DL_RSP_NAK, *seq, DL_STATUS_CRC_ERR);
        return -1;
    }

    return 0;
}

/**
 * 发送响应包
 */
static void dl_send_response(uint8_t rsp, uint16_t seq, uint8_t status)
{
    uint8_t resp_buf[DL_RESP_SIZE];
    uint16_t len;

    len = dl_build_response(resp_buf, rsp, seq, status);
    Shell_WriteRaw(resp_buf, len);
}

#endif /* BG_TARGET_PLATFORM == BG_PLATFORM_BP10 */

#endif /* BANGTSYNTH_EN */

#endif /* BANGTSYNTH_LEGACY */
