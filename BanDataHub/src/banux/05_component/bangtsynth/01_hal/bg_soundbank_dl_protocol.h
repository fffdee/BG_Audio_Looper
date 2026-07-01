/**
 * @file bg_soundbank_dl_protocol.h
 * @brief 音源下载协议定义
 * 
 * 定义音源数据下载的数据包格式和协议常量。
 * 协议基于 "命令行 + 数据包" 模式:
 * - Shell 命令行用于控制 (下载/擦除/查询/校验)
 * - 二进制数据包用于批量数据传输
 * 
 * 数据包格式 (Host → Device):
 * +------+------+-----+-------+-------+-----------+-------+-------+
 * | SYNC | SYNC | CMD | SEQ_L | SEQ_H | LEN_L LEN_H | DATA[N] | CRC_L CRC_H |
 * | 0xAA | 0x55 | 1B  |  2B   |       |    2B     |  0~4096 |    2B       |
 * +------+------+-----+-------+-------+-----------+---------+-------------+
 * 
 * 响应包格式 (Device → Host):
 * +------+------+-----+-------+-------+--------+-------+-------+
 * | SYNC | SYNC | RSP | SEQ_L | SEQ_H | STATUS | CRC_L | CRC_H |
 * | 0xAA | 0x55 | 1B  |  2B   |       |  1B    |  2B   |       |
 * +------+------+-----+-------+-------+--------+-------+-------+
 * 
 * 编译条件: BG_TARGET_PLATFORM == BG_PLATFORM_BP10
 */

#ifndef __BG_SOUNDBANK_DL_PROTOCOL_H__
#define __BG_SOUNDBANK_DL_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 协议常量
 * ============================================ */
#define DL_SYNC_BYTE_H          0xAA
#define DL_SYNC_BYTE_L          0x55
#define DL_MAX_PAYLOAD_SIZE     4096    /* 单包最大数据量 (1个Flash扇区) */
#define DL_HEADER_SIZE          7       /* Sync(2) + Cmd(1) + Seq(2) + Len(2) */
#define DL_CRC_SIZE             2       /* CRC16 (2 bytes) */
#define DL_RESP_SIZE            8       /* Sync(2) + Rsp(1) + Seq(2) + Status(1) + CRC(2) */
#define DL_RECV_TIMEOUT_MS      5000    /* 单包接收超时 (毫秒) */
#define DL_TOTAL_TIMEOUT_MS     300000  /* 总下载超时 5 分钟 */

/* ============================================
 * 命令码 (Host → Device)
 * ============================================ */
#define DL_CMD_DATA             0x01    /* 数据包 */
#define DL_CMD_END              0x02    /* 传输结束 */
#define DL_CMD_QUERY            0x03    /* 查询状态 */
#define DL_CMD_ABORT            0x04    /* 中止传输 */

/* ============================================
 * 响应码 (Device → Host)
 * ============================================ */
#define DL_RSP_ACK              0x81    /* 确认 */
#define DL_RSP_NAK              0x82    /* 否认 */
#define DL_RSP_STATUS           0x83    /* 状态响应 */
#define DL_RSP_READY            0x84    /* 准备就绪 */

/* ============================================
 * 状态码
 * ============================================ */
#define DL_STATUS_OK            0x00    /* 成功 */
#define DL_STATUS_CRC_ERR       0x01    /* CRC 校验失败 */
#define DL_STATUS_FLASH_ERR     0x02    /* Flash 写入/擦除失败 */
#define DL_STATUS_SEQ_ERR       0x03    /* 序列号不连续 */
#define DL_STATUS_OVERFLOW      0x04    /* 数据溢出 (超出存储空间) */
#define DL_STATUS_TIMEOUT       0x05    /* 超时 */
#define DL_STATUS_ABORT         0x06    /* 已中止 */
#define DL_STATUS_BUSY          0x07    /* 设备忙 */

/* ============================================
 * 数据包头结构
 * ============================================ */
typedef struct {
    uint8_t  sync_h;        /* 0xAA */
    uint8_t  sync_l;        /* 0x55 */
    uint8_t  cmd;           /* 命令码 */
    uint16_t seq;           /* 包序列号 (little-endian) */
    uint16_t len;           /* 数据长度 (little-endian) */
} DL_PacketHeader_t;

/* ============================================
 * 响应包结构
 * ============================================ */
typedef struct {
    uint8_t  sync_h;        /* 0xAA */
    uint8_t  sync_l;        /* 0x55 */
    uint8_t  rsp;           /* 响应码 */
    uint16_t seq;           /* 包序列号 (echo) */
    uint8_t  status;        /* 状态码 */
    uint16_t crc;           /* CRC16 */
} DL_Response_t;

/* ============================================
 * 下载会话状态
 * ============================================ */
typedef struct {
    uint32_t total_size;        /* 预期总大小 */
    uint32_t bytes_received;    /* 已接收字节数 */
    uint32_t write_offset;      /* 当前写入偏移 */
    uint16_t expected_seq;      /* 期望的下一个序列号 */
    uint8_t  active;            /* 下载会话是否活跃 */
    uint8_t  last_status;       /* 最后一次操作状态 */
} DL_Session_t;

/* ============================================
 * CRC16 计算 (CCITT / XMODEM)
 * ============================================ */

/**
 * CRC16-CCITT 计算
 * @param data  数据指针
 * @param len   数据长度
 * @return CRC16 值
 */
static uint16_t dl_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    size_t i;
    int j;
    
    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * 构造响应包
 * @param resp    响应包缓冲区 (至少 DL_RESP_SIZE 字节)
 * @param rsp     响应码
 * @param seq     序列号
 * @param status  状态码
 * @return 响应包长度
 */
static uint16_t dl_build_response(uint8_t *resp, uint8_t rsp, uint16_t seq, uint8_t status)
{
    uint16_t crc;
    
    resp[0] = DL_SYNC_BYTE_H;
    resp[1] = DL_SYNC_BYTE_L;
    resp[2] = rsp;
    resp[3] = (uint8_t)(seq & 0xFF);
    resp[4] = (uint8_t)((seq >> 8) & 0xFF);
    resp[5] = status;
    
    /* CRC 计算范围: rsp + seq + status (共4字节) */
    crc = dl_crc16(&resp[2], 4);
    resp[6] = (uint8_t)(crc & 0xFF);
    resp[7] = (uint8_t)((crc >> 8) & 0xFF);
    
    return DL_RESP_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif /* __BG_SOUNDBANK_DL_PROTOCOL_H__ */
