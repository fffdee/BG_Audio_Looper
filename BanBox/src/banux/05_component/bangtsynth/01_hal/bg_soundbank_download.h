/**
 * @file bg_soundbank_download.h
 * @brief 音源下载协议模块 — 通过命令行 + 数据包下载音源到 Flash#1 (NOR2)
 *
 * 下载协议设计:
 * ┌─────────────────────────────────────────────────────────┐
 * │  1. 主机发送 Shell 命令启动下载会话:                     │
 * │     soundbank -d <total_size>                           │
 * │                                                         │
 * │  2. 设备回复就绪确认:                                    │
 * │     SBDL:READY:<sector_size>                            │
 * │                                                         │
 * │  3. 主机按块发送数据 (循环):                             │
 * │     SBDL:DATA:<seq>:<size>:<crc16>\n                    │
 * │     <binary_data...>                                    │
 * │                                                         │
 * │  4. 设备回复每块确认:                                    │
 * │     SBDL:ACK:<seq>  或  SBDL:NAK:<seq>:<error>         │
 * │                                                         │
 * │  5. 主机发送结束命令:                                    │
 * │     SBDL:END:<total_crc32>                              │
 * │                                                         │
 * │  6. 设备回复最终确认:                                    │
 * │     SBDL:DONE:<total_written>  或  SBDL:FAIL:<error>   │
 * └─────────────────────────────────────────────────────────┘
 *
 * 数据包格式:
 *   Header (ASCII):  "SBDL:DATA:<seq>:<size>:<crc16>\n"
 *   Payload (Binary): <size> 字节原始数据
 *   每块最大 4096 字节 (一个 Flash 扇区)
 */

#ifndef __BG_SOUNDBANK_DOWNLOAD_H__
#define __BG_SOUNDBANK_DOWNLOAD_H__

#include <stdint.h>
#include <stddef.h>
#include "err_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 下载配置
 * ============================================ */
#define SBDL_BLOCK_SIZE         (4096)      /* 每块数据大小 = Flash 扇区大小 */
#define SBDL_HEADER_MAX_LEN     (64)        /* 数据包头最大长度 */
#define SBDL_MAX_RETRIES        (3)         /* 单块最大重试次数 */

/* ============================================
 * 下载会话状态
 * ============================================ */
typedef enum {
    SBDL_STATE_IDLE = 0,        /* 空闲, 未开始下载 */
    SBDL_STATE_READY,           /* 已擦除, 等待数据 */
    SBDL_STATE_RECEIVING,       /* 正在接收数据块 */
    SBDL_STATE_DONE,            /* 下载完成 */
    SBDL_STATE_ERROR            /* 下载出错 */
} SBDL_State_t;

/* ============================================
 * 下载会话信息
 * ============================================ */
typedef struct {
    SBDL_State_t    state;              /* 当前状态 */
    uint32_t        total_size;         /* 预期总大小 */
    uint32_t        bytes_written;      /* 已写入字节数 */
    uint32_t        expected_seq;       /* 期望的下一个序列号 */
    uint32_t        flash_offset;       /* 当前写入的 Flash 偏移 */
    uint32_t        error_count;        /* 错误计数 */
    uint32_t        start_time_ms;      /* 下载开始时间 (ms) */
} SBDL_Session_t;

/* ============================================
 * API 接口
 * ============================================ */

/**
 * 初始化下载模块
 * 配置存储后端 (BG_FlashMgr Storage 分区 = Flash#1 NOR2)
 * @return SUCCESS 或错误码
 */
BG_ERR SBDL_Init(void);

/**
 * 开始下载会话
 * - 擦除 Flash#1 Storage 分区 (或按需擦除)
 * - 初始化会话状态
 *
 * @param total_size  预期下载总字节数
 * @return SUCCESS 或错误码
 */
BG_ERR SBDL_StartSession(uint32_t total_size);

/**
 * 接收并写入一块数据
 * - 校验 CRC16, 验证序列号
 * - 写入 Flash#1 Storage 分区
 *
 * @param seq       块序列号 (从 0 开始)
 * @param data      数据指针
 * @param size      数据字节数 (≤ SBDL_BLOCK_SIZE)
 * @param crc16     主机计算的 CRC16 校验值
 * @return SUCCESS / ENABLE_INVALID_INPUT (CRC错误) / 其他错误
 */
BG_ERR SBDL_WriteBlock(uint32_t seq, const uint8_t *data, uint32_t size, uint16_t crc16);

/**
 * 结束下载会话
 * - 验证总 CRC32 (可选)
 * - 同步 Flash
 *
 * @param total_crc32  主机计算的总 CRC32 (0 = 跳过验证)
 * @return SUCCESS 或错误码
 */
BG_ERR SBDL_EndSession(uint32_t total_crc32);

/**
 * 取消当前下载
 */
void SBDL_Cancel(void);

/**
 * 获取当前会话信息
 * @return 只读会话指针
 */
const SBDL_Session_t* SBDL_GetSession(void);

/**
 * 处理收到的原始协议数据
 * 解析 "SBDL:DATA:..." 头部 + 二进制数据
 * 
 * @param data  收到的原始字节流
 * @param len   字节数
 * @return 消费的字节数 (0 = 数据不完整需要更多)
 */
uint32_t SBDL_ProcessRawData(const uint8_t *data, uint32_t len);

/**
 * 计算 CRC16 (CCITT)
 * @param data  数据
 * @param len   长度
 * @return CRC16 值
 */
uint16_t SBDL_CRC16(const uint8_t *data, uint32_t len);

/**
 * 计算 CRC32
 * @param data  数据
 * @param len   长度
 * @return CRC32 值
 */
uint32_t SBDL_CRC32(const uint8_t *data, uint32_t len);

/**
 * 读取已下载的音源数据 (验证用)
 * @param offset  相对于 Storage 分区起始的偏移
 * @param buffer  读取缓冲区
 * @param size    读取字节数
 * @return 实际读取字节数, <0 表示错误
 */
int SBDL_ReadBack(uint32_t offset, void *buffer, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __BG_SOUNDBANK_DOWNLOAD_H__ */
