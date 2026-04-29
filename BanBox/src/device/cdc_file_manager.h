/**
 * @file  cdc_file_manager.h
 * @brief USB CDC 文件管理器 — 上位机通过 CDC 管理 SD 卡文件 / 下载文件到 NAND
 *
 * 两种工作模式:
 *   CDC_FM_MODE_SD   : SD 卡文件管理 (ls/read/write/delete/mkdir 等完整操作)
 *   CDC_FM_MODE_NAND : NAND Flash 管理 (仅下载文件到 NAND)
 *
 * 模式切换流程:
 *   1. 上位机发送 CDC_FM_CMD_ENTER_SD 或 CDC_FM_CMD_ENTER_NAND
 *   2. 下位机收到后:
 *      a. 停止 Shell 系统 (锁定 CDC IO)
 *      b. 进入文件管理通信循环
 *      c. 响应 ACK 给上位机
 *   3. 上位机发送文件操作命令, 下位机执行并返回结果
 *   4. 上位机发送 CDC_FM_CMD_EXIT 或 USB 断开 → 退出管理模式, 恢复 Shell
 *
 * 二进制协议帧格式:
 *   [SOF 0xAB] [CMD 1B] [LEN_L 1B] [LEN_H 1B] [PAYLOAD 0~64KB] [CRC8 1B]
 *
 * 编译条件: CDC_FILE_MANAGER_EN
 */
#ifndef __CDC_FILE_MANAGER_H__
#define __CDC_FILE_MANAGER_H__

#include "product_def.h"

#ifdef CDC_FILE_MANAGER_EN

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 协议常量
 * ============================================ */

/** 帧起始标识 */
#define CDC_FM_SOF              0xAB

/** 命令字 */
#define CDC_FM_CMD_HANDSHAKE    0x01   /* 握手命令 */
#define CDC_FM_CMD_HEARTBEAT    0x02   /* 心跳命令 */
#define CDC_FM_CMD_ENTER_SD     0x03   /* 进入 SD 卡管理模式 */
#define CDC_FM_CMD_ENTER_NAND   0x04   /* 进入 NAND 管理模式 */
#define CDC_FM_CMD_EXIT         0x05   /* 退出管理模式 */
#define CDC_FM_CMD_LS           0x10   /* 列目录 */
#define CDC_FM_CMD_CD           0x11   /* 切换目录 */
#define CDC_FM_CMD_READ_FILE    0x20   /* 读取文件 */
#define CDC_FM_CMD_WRITE_BEGIN  0x30   /* 开始写文件 (创建/覆盖) */
#define CDC_FM_CMD_WRITE_DATA   0x31   /* 写文件数据块 */
#define CDC_FM_CMD_WRITE_END    0x32   /* 结束写文件 */
#define CDC_FM_CMD_DELETE       0x40   /* 删除文件 */
#define CDC_FM_CMD_MKDIR        0x41   /* 创建目录 */
#define CDC_FM_CMD_RMDIR        0x42   /* 删除目录 */
#define CDC_FM_CMD_RENAME       0x43   /* 重命名文件/目录 */
#define CDC_FM_CMD_INFO         0x50   /* 获取存储信息 (容量/剩余) */

/* NAND 专用命令 */
#define CDC_FM_CMD_NAND_DL_BEGIN  0x60 /* 开始下载到 NAND */
#define CDC_FM_CMD_NAND_DL_DATA   0x61 /* NAND 下载数据块 */
#define CDC_FM_CMD_NAND_DL_END    0x62 /* 结束 NAND 下载 */
#define CDC_FM_CMD_NAND_ERASE     0x63 /* 擦除 NAND 音色区 */
#define CDC_FM_CMD_NAND_INFO      0x64 /* NAND 存储信息 */

/** 响应状态码 */
#define CDC_FM_RSP_HANDSHAKE_ACK 0x81  /* 握手响应 */
#define CDC_FM_RSP_HEARTBEAT_ACK 0x82  /* 心跳响应 */
#define CDC_FM_RSP_ACK          0x00   /* 操作成功 */
#define CDC_FM_RSP_ERR_CMD      0x01   /* 未知命令 */
#define CDC_FM_RSP_ERR_CRC      0x02   /* CRC 校验失败 */
#define CDC_FM_RSP_ERR_IO       0x03   /* 存储 IO 错误 */
#define CDC_FM_RSP_ERR_NOTFOUND 0x04   /* 文件/目录不存在 */
#define CDC_FM_RSP_ERR_FULL     0x05   /* 存储空间不足 */
#define CDC_FM_RSP_ERR_MODE     0x06   /* 当前模式不支持此操作 */
#define CDC_FM_RSP_ERR_BUSY     0x07   /* 设备忙 */
#define CDC_FM_RSP_ERR_PARAM    0x08   /* 参数错误 */

/** 协议参数 */
#define CDC_FM_MAX_PAYLOAD      4096   /* 单帧最大负载 (4KB) */
#define CDC_FM_HEADER_SIZE      4      /* SOF + CMD + LEN_L + LEN_H */
#define CDC_FM_RX_TIMEOUT_MS    5000   /* 接收超时 */

/* ============================================
 * 管理模式枚举
 * ============================================ */
typedef enum {
    CDC_FM_MODE_NONE = 0,   /* 未进入管理模式 */
    CDC_FM_MODE_SD,         /* SD 卡文件管理 */
    CDC_FM_MODE_NAND        /* NAND Flash 管理 (仅下载) */
} CDC_FM_Mode_t;

/* ============================================
 * 公共 API
 * ============================================ */

/**
 * @brief  初始化 CDC 文件管理器
 *         在 USB CDC 就绪后调用一次
 */
void CDC_FileManager_Init(void);

/**
 * @brief  检查是否处于文件管理模式
 * @return 1 = 已进入管理模式, 0 = 正常 Shell 模式
 */
int CDC_FileManager_InMode(void);

/**
 * @brief  文件管理模式主处理函数
 *         在 main loop 中当 CDC_FileManager_InMode() == 1 时调用
 *         处理上位机命令并执行文件操作
 */
void CDC_FileManager_Process(void);

/**
 * @brief  检测上位机 ENTER 命令 (在 Shell 正常模式下周期调用)
 *         如果检测到 0xAB 开头的 ENTER_SD / ENTER_NAND 帧, 自动进入管理模式
 * @return 1 = 刚进入管理模式, 0 = 无变化
 */
int CDC_FileManager_CheckEnter(void);

/**
 * @brief  获取当前管理模式
 * @return CDC_FM_Mode_t
 */
CDC_FM_Mode_t CDC_FileManager_GetMode(void);

#ifdef __cplusplus
}
#endif

#endif /* CDC_FILE_MANAGER_EN */

#endif /* __CDC_FILE_MANAGER_H__ */
