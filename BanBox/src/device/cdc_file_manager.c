/**
 * @file  cdc_file_manager.c
 * @brief USB CDC 文件管理器实现 — 上位机通过 CDC 管理 SD 卡文件 / 下载文件到 NAND
 *
 * 编译条件: CDC_FILE_MANAGER_EN
 */

#include "product_def.h"
#include "stdint.h"
#if CDC_FILE_MANAGER_EN && 0  /* Disabled: missing BanGTsynth HAL dependencies (BG_Log, NAND_Store*) */

#include "cdc_file_manager.h"
#include "otg_device_cdc.h"
#include "shell_io_manager.h"
#include "fat32_reader.h"
#include "nand_store.h"
#include "bg_log.h"
#include "bg_osal.h"
#include <string.h>

/* ============================================
 * 内部常量定义
 * ============================================ */

/** CRC8 多项式 (x^8 + x^2 + x + 1) */
#define CRC8_POLYNOMIAL     0x07

/** 帧解析状态机 */
typedef enum {
    FRAME_STATE_SOF = 0,    /* 等待 SOF */
    FRAME_STATE_CMD,        /* 等待 CMD */
    FRAME_STATE_LEN_L,      /* 等待 LEN_L */
    FRAME_STATE_LEN_H,      /* 等待 LEN_H */
    FRAME_STATE_PAYLOAD,    /* 接收 PAYLOAD */
    FRAME_STATE_CRC         /* 等待 CRC */
} FrameParseState_t;

/* ============================================
 * 内部数据结构
 * ============================================ */

/** 帧解析上下文 */
typedef struct {
    FrameParseState_t state;        /* 当前解析状态 */
    uint8_t cmd;                    /* 命令字 */
    uint8_t len_l;                  /* LEN 低字节原始值 (用于 CRC 验证，避免依赖结构体对齐) */
    uint8_t len_h;                  /* LEN 高字节原始值 */
    uint16_t payload_len;           /* 负载长度 (解析后) */
    uint16_t payload_received;      /* 已接收负载长度 */
    uint8_t payload[CDC_FM_MAX_PAYLOAD]; /* 负载缓冲区 */
    uint8_t crc;                    /* 接收到的 CRC */
    uint32_t last_rx_tick;          /* 最后接收时间 */
} FrameContext_t;

/** 写文件上下文 (分块写入) */
typedef struct {
    uint32_t dir_cluster;           /* 目标目录簇号 */
    char filename[256];             /* 文件名 */
    uint32_t total_size;            /* 总大小 */
    uint32_t received_size;         /* 已接收大小 */
    uint8_t buffer[4096];           /* 临时缓冲区 */
    uint16_t buffer_used;           /* 缓冲区使用量 */
} WriteFileContext_t;

/** NAND 下载上下文 */
typedef struct {
    uint8_t program;                /* MIDI 程序号 */
    uint32_t total_size;            /* 总大小 */
    uint32_t received_size;         /* 已接收大小 */
    uint8_t buffer[4096];           /* 临时缓冲区 */
    uint16_t buffer_used;           /* 缓冲区使用量 */
    char name[32];                  /* 音色名称 */
    uint16_t format;                /* 音源格式 */
} NandDownloadContext_t;

/* ============================================
 * 全局变量
 * ============================================ */

static int s_initialised = 0;                   /* 初始化标志 */
static int s_entering = 0;                      /* 1 = SOF 已嘱探，正在建立会话，允许 Process 接管 */
static CDC_FM_Mode_t s_mode = CDC_FM_MODE_NONE; /* 当前模式 */
static FrameContext_t s_frame_ctx;              /* 帧解析上下文 */
static WriteFileContext_t s_write_ctx;          /* 写文件上下文 */
static NandDownloadContext_t s_nand_ctx;        /* NAND 下载上下文 */
static uint32_t s_current_dir_cluster;          /* 当前目录簇号 (SD 模式) */
static uint8_t s_tx_frame[CDC_FM_MAX_PAYLOAD + CDC_FM_HEADER_SIZE + 1]; /* 发送帧缓冲区 (静态，避免帧切换检查中展山) */

/* ============================================
 * 内部函数声明
 * ============================================ */

static uint8_t crc8_calc(const uint8_t *data, uint16_t len);
static void send_response(uint8_t status, const uint8_t *data, uint16_t data_len);
static void reset_frame_context(void);
static void process_frame(void);
static void handle_command(uint8_t cmd, const uint8_t *payload, uint16_t payload_len);

/* SD 卡命令处理器 */
static void handle_ls(const uint8_t *payload, uint16_t payload_len);
static void handle_cd(const uint8_t *payload, uint16_t payload_len);
static void handle_read_file(const uint8_t *payload, uint16_t payload_len);
static void handle_write_begin(const uint8_t *payload, uint16_t payload_len);
static void handle_write_data(const uint8_t *payload, uint16_t payload_len);
static void handle_write_end(const uint8_t *payload, uint16_t payload_len);
static void handle_delete(const uint8_t *payload, uint16_t payload_len);
static void handle_mkdir(const uint8_t *payload, uint16_t payload_len);
static void handle_rmdir(const uint8_t *payload, uint16_t payload_len);
static void handle_rename(const uint8_t *payload, uint16_t payload_len);
static void handle_info(const uint8_t *payload, uint16_t payload_len);

/* NAND 命令处理器 */
static void handle_nand_dl_begin(const uint8_t *payload, uint16_t payload_len);
static void handle_nand_dl_data(const uint8_t *payload, uint16_t payload_len);
static void handle_nand_dl_end(const uint8_t *payload, uint16_t payload_len);
static void handle_nand_erase(const uint8_t *payload, uint16_t payload_len);
static void handle_nand_info(const uint8_t *payload, uint16_t payload_len);

/* ============================================
 * 内部函数实现
 * ============================================ */

/**
 * @brief 计算 CRC8 校验和
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return CRC8 值
 */
/* 支持初值的 CRC8 计算 (用于分段/增量计算) */
static uint8_t crc8_update(uint8_t crc, const uint8_t *data, uint16_t len)
{
    uint16_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ CRC8_POLYNOMIAL;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static uint8_t crc8_calc(const uint8_t *data, uint16_t len)
{
    return crc8_update(0, data, len);
}

/**
 * @brief 发送响应帧给上位机
 * @param status 响应状态码
 * @param data 响应数据 (可为 NULL)
 * @param data_len 响应数据长度
 */
static void send_response(uint8_t status, const uint8_t *data, uint16_t data_len)
{
    uint16_t frame_len = 0;

    /* 构建帧 */
    s_tx_frame[frame_len++] = CDC_FM_SOF;
    s_tx_frame[frame_len++] = status; /* 响应状态作为 CMD 字段 */

    /* 长度 (小端) */
    s_tx_frame[frame_len++] = (uint8_t)(data_len & 0xFF);
    s_tx_frame[frame_len++] = (uint8_t)((data_len >> 8) & 0xFF);

    /* 数据 */
    if (data && data_len > 0) {
        memcpy(&s_tx_frame[frame_len], data, data_len);
        frame_len += data_len;
    }

    /* CRC: 覆盖 status(1) + LEN_L(1) + LEN_H(1) + data(出in) = frame_len - 1 字节 */
    s_tx_frame[frame_len] = crc8_calc(&s_tx_frame[1], frame_len - 1);
    frame_len++;

    /* 发送 */
    OTG_DeviceCDC_Send(s_tx_frame, frame_len);
}

/**
 * @brief 重置帧解析上下文
 */
static void reset_frame_context(void)
{
    memset(&s_frame_ctx, 0, sizeof(s_frame_ctx));
    s_frame_ctx.state = FRAME_STATE_SOF;
}

/**
 * @brief 处理完整接收到的帧
 */
static void process_frame(void)
{
    uint8_t calculated_crc;

    /* 验证 CRC
     * 分段计算: CMD(1) + LEN_L(1) + LEN_H(1) + PAYLOAD(n)
     * 存储在 len_l/len_h 的是接收到的原始字节，避免依赖结构体对齐填充 */
    calculated_crc = crc8_update(0,                      &s_frame_ctx.cmd,   1);
    calculated_crc = crc8_update(calculated_crc,         &s_frame_ctx.len_l, 1);
    calculated_crc = crc8_update(calculated_crc,         &s_frame_ctx.len_h, 1);
    if (s_frame_ctx.payload_len > 0) {
        calculated_crc = crc8_update(calculated_crc, s_frame_ctx.payload, s_frame_ctx.payload_len);
    }

    if (calculated_crc != s_frame_ctx.crc) {
        BG_LOG_E(BG_LOG_TAG_FAT32, "Frame CRC error: expected 0x%02X, got 0x%02X",
                 calculated_crc, s_frame_ctx.crc);
        send_response(CDC_FM_RSP_ERR_CRC, NULL, 0);
        return;
    }

    /* 处理命令 */
    handle_command(s_frame_ctx.cmd, s_frame_ctx.payload, s_frame_ctx.payload_len);
}

/**
 * @brief 处理命令
 * @param cmd 命令字
 * @param payload 负载数据
 * @param payload_len 负载长度
 */
static void handle_command(uint8_t cmd, const uint8_t *payload, uint16_t payload_len)
{
    /* 检查模式 */
    if (cmd == CDC_FM_CMD_ENTER_SD || cmd == CDC_FM_CMD_ENTER_NAND || cmd == CDC_FM_CMD_EXIT) {
        /* 模式切换命令，任何时候都可以处理 */
    } else if (s_mode == CDC_FM_MODE_NONE) {
        /* 非模式切换命令，但当前不在管理模式 */
        send_response(CDC_FM_RSP_ERR_MODE, NULL, 0);
        return;
    } else if (cmd >= 0x60 && cmd <= 0x64 && s_mode != CDC_FM_MODE_NAND) {
        /* NAND 命令，但当前不是 NAND 模式 */
        send_response(CDC_FM_RSP_ERR_MODE, NULL, 0);
        return;
    } else if (cmd >= 0x10 && cmd <= 0x50 && s_mode != CDC_FM_MODE_SD) {
        /* SD 命令，但当前不是 SD 模式 */
        send_response(CDC_FM_RSP_ERR_MODE, NULL, 0);
        return;
    }

    /* 分发命令 */
    switch (cmd) {
        /* 连接管理 */
        case CDC_FM_CMD_HANDSHAKE:
            /* 握手响应 */
            send_response(CDC_FM_RSP_HANDSHAKE_ACK, NULL, 0);
            break;

        case CDC_FM_CMD_HEARTBEAT:
            /* 心跳响应 */
            send_response(CDC_FM_RSP_HEARTBEAT_ACK, NULL, 0);
            break;

        /* 模式切换 */
        case CDC_FM_CMD_ENTER_SD:
            s_mode = CDC_FM_MODE_SD;
            s_entering = 0; /* 会话已正式建立 */
            s_current_dir_cluster = FAT32_GetRootCluster();
            BG_LOG_I(BG_LOG_TAG_FAT32, "Entered SD card file management mode");
            send_response(CDC_FM_RSP_ACK, NULL, 0);
            break;

        case CDC_FM_CMD_ENTER_NAND:
            s_mode = CDC_FM_MODE_NAND;
            s_entering = 0; /* 会话已正式建立 */
            BG_LOG_I(BG_LOG_TAG_NAND, "Entered NAND flash management mode");
            send_response(CDC_FM_RSP_ACK, NULL, 0);
            break;

        case CDC_FM_CMD_EXIT:
            s_mode = CDC_FM_MODE_NONE;
            s_entering = 0;
            ShellIOManager_Unlock(); /* 释放 Shell IO 锁定 */
            BG_LOG_I(BG_LOG_TAG_FAT32, "Exited file management mode");
            send_response(CDC_FM_RSP_ACK, NULL, 0);
            break;

        /* SD 卡命令 */
        case CDC_FM_CMD_LS:
            handle_ls(payload, payload_len);
            break;
        case CDC_FM_CMD_CD:
            handle_cd(payload, payload_len);
            break;
        case CDC_FM_CMD_READ_FILE:
            handle_read_file(payload, payload_len);
            break;
        case CDC_FM_CMD_WRITE_BEGIN:
            handle_write_begin(payload, payload_len);
            break;
        case CDC_FM_CMD_WRITE_DATA:
            handle_write_data(payload, payload_len);
            break;
        case CDC_FM_CMD_WRITE_END:
            handle_write_end(payload, payload_len);
            break;
        case CDC_FM_CMD_DELETE:
            handle_delete(payload, payload_len);
            break;
        case CDC_FM_CMD_MKDIR:
            handle_mkdir(payload, payload_len);
            break;
        case CDC_FM_CMD_RMDIR:
            handle_rmdir(payload, payload_len);
            break;
        case CDC_FM_CMD_RENAME:
            handle_rename(payload, payload_len);
            break;
        case CDC_FM_CMD_INFO:
            handle_info(payload, payload_len);
            break;

        /* NAND 命令 */
        case CDC_FM_CMD_NAND_DL_BEGIN:
            handle_nand_dl_begin(payload, payload_len);
            break;
        case CDC_FM_CMD_NAND_DL_DATA:
            handle_nand_dl_data(payload, payload_len);
            break;
        case CDC_FM_CMD_NAND_DL_END:
            handle_nand_dl_end(payload, payload_len);
            break;
        case CDC_FM_CMD_NAND_ERASE:
            handle_nand_erase(payload, payload_len);
            break;
        case CDC_FM_CMD_NAND_INFO:
            handle_nand_info(payload, payload_len);
            break;

        default:
            BG_LOG_W(BG_LOG_TAG_FAT32, "Unknown command: 0x%02X", cmd);
            send_response(CDC_FM_RSP_ERR_CMD, NULL, 0);
            break;
    }
}

/* ============================================
 * SD 卡命令处理器实现
 * ============================================ */

/** ls 回调上下文 — 避免结构体尾部非对齐指针 hack */
typedef struct {
    uint8_t  *buf;      /* 输出缓冲区 */
    uint16_t  used;     /* 已写字节数 */
    uint16_t  capacity; /* 缓冲区容量 */
} LsCallbackCtx_t;

static int list_callback(const FAT32_FileInfo_t *info, void *user)
{
    LsCallbackCtx_t *ctx = (LsCallbackCtx_t *)user;
    uint16_t name_len = (uint16_t)(strlen(info->name) + 1); /* 含 null 终止符 */
    uint16_t entry_len = 1 + 4 + name_len;                  /* attr + size + name */

    /* 格式: [attr(1)][size(4)][name(n+\0)] */
    if (ctx->used + entry_len > ctx->capacity) {
        return 1; /* 缓冲区满，停止 */
    }

    ctx->buf[ctx->used] = info->attr;
    memcpy(&ctx->buf[ctx->used + 1], &info->size, 4);
    memcpy(&ctx->buf[ctx->used + 5], info->name, name_len);
    ctx->used += entry_len;

    return 0; /* 继续 */
}

static void handle_ls(const uint8_t *payload, uint16_t payload_len)
{
    BG_ERR ret;
    static uint8_t s_ls_buf[CDC_FM_MAX_PAYLOAD]; /* 静态缓冲区，避免展少4096B栈 */
    LsCallbackCtx_t ctx;

    ctx.buf      = s_ls_buf;
    ctx.used     = 0;
    ctx.capacity = (uint16_t)sizeof(s_ls_buf);

    /* 列出当前目录内容 */
    ret = FAT32_ListDirByCluster(s_current_dir_cluster, list_callback, &ctx);

    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_FAT32, "FAT32_ListDirByCluster failed: %d", ret);
        send_response(CDC_FM_RSP_ERR_IO, NULL, 0);
        return;
    }

    send_response(CDC_FM_RSP_ACK, s_ls_buf, ctx.used);
}

static void handle_cd(const uint8_t *payload, uint16_t payload_len)
{
    const char *path = (const char *)payload;
    BG_ERR ret;
    FAT32_FileInfo_t info;

    if (payload_len == 0 || payload_len >= 256) {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 查找目录 */
    ret = FAT32_FindEntryInDir(s_current_dir_cluster, path, &info);
    if (ret != SUCCESS) {
        BG_LOG_W(BG_LOG_TAG_FAT32, "Directory not found: %s", path);
        send_response(CDC_FM_RSP_ERR_NOTFOUND, NULL, 0);
        return;
    }

    if (!(info.attr & DIR_ATTR_DIRECTORY)) {
        BG_LOG_W(BG_LOG_TAG_FAT32, "Not a directory: %s", path);
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 更新当前目录 */
    s_current_dir_cluster = info.start_cluster;
    BG_LOG_I(BG_LOG_TAG_FAT32, "Changed directory to: %s (cluster %d)", path, s_current_dir_cluster);
    send_response(CDC_FM_RSP_ACK, NULL, 0);
}

static void handle_read_file(const uint8_t *payload, uint16_t payload_len)
{
    const char *filename = (const char *)payload;
    BG_ERR ret;
    FAT32_FileHandle_t handle;
    uint8_t buffer[CDC_FM_MAX_PAYLOAD - 4]; /* 预留空间给文件大小 */
    int32_t read_len;
    uint32_t total_read = 0;

    if (payload_len == 0 || payload_len >= 256) {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 打开文件 */
    ret = FAT32_OpenFileInDir(s_current_dir_cluster, filename, &handle);
    if (ret != SUCCESS) {
        BG_LOG_W(BG_LOG_TAG_FAT32, "File not found: %s", filename);
        send_response(CDC_FM_RSP_ERR_NOTFOUND, NULL, 0);
        return;
    }

    /* 读取文件内容 */
    read_len = FAT32_ReadFile(&handle, buffer + 4, sizeof(buffer) - 4);
    if (read_len < 0) {
        BG_LOG_E(BG_LOG_TAG_FAT32, "Read file failed: %d", read_len);
        FAT32_CloseFile(&handle);
        send_response(CDC_FM_RSP_ERR_IO, NULL, 0);
        return;
    }

    total_read = (uint32_t)read_len;

    /* 响应格式: [total_size(4)][data(n)] */
    memcpy(buffer, &handle.info.size, 4);
    FAT32_CloseFile(&handle);

    BG_LOG_I(BG_LOG_TAG_FAT32, "Read file: %s (%d bytes)", filename, total_read);
    send_response(CDC_FM_RSP_ACK, buffer, 4 + total_read);
}

static void handle_write_begin(const uint8_t *payload, uint16_t payload_len)
{
    const char *filename = (const char *)payload;
    uint32_t total_size;

    if (payload_len < 5 || payload_len >= 256) { /* filename + size */
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 解析参数 */
    memcpy(&total_size, &payload[payload_len - 4], 4);

    /* 初始化写文件上下文 */
    s_write_ctx.dir_cluster = s_current_dir_cluster;
    strncpy(s_write_ctx.filename, filename, payload_len - 4);
    s_write_ctx.filename[payload_len - 4] = '\0';
    s_write_ctx.total_size = total_size;
    s_write_ctx.received_size = 0;
    s_write_ctx.buffer_used = 0;

    BG_LOG_I(BG_LOG_TAG_FAT32, "Begin write file: %s (%d bytes)", s_write_ctx.filename, total_size);
    send_response(CDC_FM_RSP_ACK, NULL, 0);
}

static void handle_write_data(const uint8_t *payload, uint16_t payload_len)
{
    if (s_write_ctx.filename[0] == '\0') {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    if (s_write_ctx.buffer_used + payload_len > sizeof(s_write_ctx.buffer)) {
        BG_LOG_E(BG_LOG_TAG_FAT32, "Write buffer overflow");
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 累积数据 */
    memcpy(&s_write_ctx.buffer[s_write_ctx.buffer_used], payload, payload_len);
    s_write_ctx.buffer_used += payload_len;
    s_write_ctx.received_size += payload_len;

    send_response(CDC_FM_RSP_ACK, NULL, 0);
}

static void handle_write_end(const uint8_t *payload, uint16_t payload_len)
{
    BG_ERR ret;

    if (s_write_ctx.filename[0] == '\0') {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 写入文件 */
    ret = FAT32_WriteFile(s_write_ctx.dir_cluster, s_write_ctx.filename,
                         s_write_ctx.buffer, s_write_ctx.received_size);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_FAT32, "Write file failed: %d", ret);
        send_response(CDC_FM_RSP_ERR_IO, NULL, 0);
    } else {
        BG_LOG_I(BG_LOG_TAG_FAT32, "Write file completed: %s (%d bytes)", 
                 s_write_ctx.filename, s_write_ctx.received_size);
        send_response(CDC_FM_RSP_ACK, NULL, 0);
    }

    /* 清理上下文 */
    memset(&s_write_ctx, 0, sizeof(s_write_ctx));
}

static void handle_delete(const uint8_t *payload, uint16_t payload_len)
{
    const char *filename = (const char *)payload;
    BG_ERR ret;

    if (payload_len == 0 || payload_len >= 256) {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    ret = FAT32_DeleteFile(s_current_dir_cluster, filename);
    if (ret != SUCCESS) {
        BG_LOG_W(BG_LOG_TAG_FAT32, "Delete file failed: %s (%d)", filename, ret);
        send_response(CDC_FM_RSP_ERR_IO, NULL, 0);
    } else {
        BG_LOG_I(BG_LOG_TAG_FAT32, "Deleted file: %s", filename);
        send_response(CDC_FM_RSP_ACK, NULL, 0);
    }
}

static void handle_mkdir(const uint8_t *payload, uint16_t payload_len)
{
    const char *dirname = (const char *)payload;
    BG_ERR ret;

    if (payload_len == 0 || payload_len >= 256) {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    ret = FAT32_MkDir(s_current_dir_cluster, dirname);
    if (ret != SUCCESS) {
        BG_LOG_W(BG_LOG_TAG_FAT32, "Create directory failed: %s (%d)", dirname, ret);
        send_response(CDC_FM_RSP_ERR_IO, NULL, 0);
    } else {
        BG_LOG_I(BG_LOG_TAG_FAT32, "Created directory: %s", dirname);
        send_response(CDC_FM_RSP_ACK, NULL, 0);
    }
}

static void handle_rmdir(const uint8_t *payload, uint16_t payload_len)
{
    const char *dirname = (const char *)payload;
    BG_ERR ret;

    if (payload_len == 0 || payload_len >= 256) {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    ret = FAT32_RmDir(s_current_dir_cluster, dirname);
    if (ret != SUCCESS) {
        BG_LOG_W(BG_LOG_TAG_FAT32, "Remove directory failed: %s (%d)", dirname, ret);
        send_response(CDC_FM_RSP_ERR_IO, NULL, 0);
    } else {
        BG_LOG_I(BG_LOG_TAG_FAT32, "Removed directory: %s", dirname);
        send_response(CDC_FM_RSP_ACK, NULL, 0);
    }
}

static void handle_rename(const uint8_t *payload, uint16_t payload_len)
{
    const char *oldname;
    const char *newname;
    uint16_t oldname_len;
    BG_ERR ret;

    if (payload_len < 2) {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 解析参数: oldname\0newname\0 */
    oldname = (const char *)payload;
    oldname_len = strlen(oldname);
    if (oldname_len + 2 > payload_len) {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }
    newname = oldname + oldname_len + 1;

    ret = FAT32_Rename(s_current_dir_cluster, oldname, newname);
    if (ret != SUCCESS) {
        BG_LOG_W(BG_LOG_TAG_FAT32, "Rename failed: %s -> %s (%d)", oldname, newname, ret);
        send_response(CDC_FM_RSP_ERR_IO, NULL, 0);
    } else {
        BG_LOG_I(BG_LOG_TAG_FAT32, "Renamed: %s -> %s", oldname, newname);
        send_response(CDC_FM_RSP_ACK, NULL, 0);
    }
}

static void handle_info(const uint8_t *payload, uint16_t payload_len)
{
    FAT32_FSInfo_t fs_info;
    uint8_t response[12]; /* total(4) + free(4) + used(4) */
    BG_ERR ret;
    uint32_t cluster_size;
    uint32_t total_bytes;
    uint32_t free_bytes;
    uint32_t used_bytes;

    ret = FAT32_GetFSInfo(&fs_info);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_FAT32, "Get FS info failed: %d", ret);
        send_response(CDC_FM_RSP_ERR_IO, NULL, 0);
        return;
    }

    /* 计算容量信息 */
    cluster_size = fs_info.bpb.sectors_per_cluster * 512;
    total_bytes  = fs_info.total_clusters * cluster_size;
    free_bytes   = 0; /* FAT32 需要遍历 FAT 表才能获取，此处简化 */
    used_bytes   = total_bytes - free_bytes;

    memcpy(&response[0], &total_bytes, 4);
    memcpy(&response[4], &free_bytes, 4);
    memcpy(&response[8], &used_bytes, 4);

    send_response(CDC_FM_RSP_ACK, response, 12);
}

void CDC_FileManager_Init(void)
{
    if (s_initialised) {
        return;
    }

    /* 初始化状态 */
    s_mode = CDC_FM_MODE_NONE;
    s_entering = 0;
    reset_frame_context();
    memset(&s_write_ctx, 0, sizeof(s_write_ctx));
    memset(&s_nand_ctx, 0, sizeof(s_nand_ctx));
    s_current_dir_cluster = 0;

    s_initialised = 1;
    BG_LOG_I(BG_LOG_TAG_FAT32, "CDC File Manager initialized");
}

int CDC_FileManager_InMode(void)
{
    return (s_mode != CDC_FM_MODE_NONE || s_entering) ? 1 : 0;
}

int CDC_FileManager_CheckEnter(void)
{
    uint8_t data;
    uint16_t count;

    if (!s_initialised || s_mode != CDC_FM_MODE_NONE || s_entering) {
        return 0; /* 已在管理模式 / 正在建立会话 / 未初始化 */
    }

    /* 检查是否有数据 */
    count = OTG_DeviceCDC_GetRxCount();
    if (count == 0) {
        return 0;
    }

    /* Peek at first byte WITHOUT consuming it.
     * This allows other CDC dispatchers (e.g. CDC_Upgrade_CheckEnter for
     * 0xAA SOF) to see the byte if it doesn't match our SOF (0xAB). */
    if (OTG_DeviceCDC_PeekByte(&data) == 1) {
        if (data == CDC_FM_SOF) {
            /* SOF matched — now consume the byte */
            OTG_DeviceCDC_Receive(&data, 1);

            /* 检测到 SOF，开始帧解析 */
            reset_frame_context();
            s_frame_ctx.state = FRAME_STATE_CMD; /* SOF 已收到，等待 CMD */
            s_frame_ctx.last_rx_tick = bg_get_tick_ms();

            /* 尝试锁定 Shell IO */
            if (ShellIOManager_TryLock(SHELL_IO_CDC)) {
                s_entering = 1; /* 标记正在建立会话，允许 Process() 句愿后续负载字节 */
                BG_LOG_I(BG_LOG_TAG_FAT32, "Shell IO locked for file management");
                return 1;
            } else {
                BG_LOG_W(BG_LOG_TAG_FAT32, "Failed to lock Shell IO");
                return 0;
            }
        }
        /* 不是 SOF (0xAB)，不消费字节，留给 Shell 或其他模块处理 */
    }

    return 0;
}

void CDC_FileManager_Process(void)
{
    uint8_t data;
    uint16_t count;

    if (!s_initialised || (s_mode == CDC_FM_MODE_NONE && !s_entering)) {
        return;
    }

    /* 获取可用数据量 */
    count = OTG_DeviceCDC_GetRxCount();
    if (count == 0) {
        /* 检查超时 */
        if (bg_get_tick_ms() - s_frame_ctx.last_rx_tick > CDC_FM_RX_TIMEOUT_MS) {
            BG_LOG_W(BG_LOG_TAG_FAT32, "Frame receive timeout");
            if (s_entering) {
                /* SOF 出现后超时未收到完整 ENTER 帧，取消会话 */
                s_entering = 0;
                ShellIOManager_Unlock();
            }
            reset_frame_context();
        }
        return;
    }

    /* 处理数据 */
    while (count > 0 && OTG_DeviceCDC_Receive(&data, 1) == 1) {
        s_frame_ctx.last_rx_tick = bg_get_tick_ms();

        switch (s_frame_ctx.state) {
            case FRAME_STATE_SOF:
                if (data == CDC_FM_SOF) {
                    s_frame_ctx.state = FRAME_STATE_CMD;
                }
                break;

            case FRAME_STATE_CMD:
                s_frame_ctx.cmd = data;
                s_frame_ctx.state = FRAME_STATE_LEN_L;
                break;

            case FRAME_STATE_LEN_L:
                s_frame_ctx.len_l = data;           /* 保存原始字节用于 CRC */
                s_frame_ctx.payload_len = data;
                s_frame_ctx.state = FRAME_STATE_LEN_H;
                break;

            case FRAME_STATE_LEN_H:
                s_frame_ctx.len_h = data;           /* 保存原始字节用于 CRC */
                s_frame_ctx.payload_len |= ((uint16_t)data << 8);
                if (s_frame_ctx.payload_len > CDC_FM_MAX_PAYLOAD) {
                    BG_LOG_E(BG_LOG_TAG_FAT32, "Payload too large: %d", s_frame_ctx.payload_len);
                    reset_frame_context();
                    send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
                } else if (s_frame_ctx.payload_len == 0) {
                    s_frame_ctx.state = FRAME_STATE_CRC;
                } else {
                    s_frame_ctx.state = FRAME_STATE_PAYLOAD;
                    s_frame_ctx.payload_received = 0;
                }
                break;

            case FRAME_STATE_PAYLOAD:
                s_frame_ctx.payload[s_frame_ctx.payload_received++] = data;
                if (s_frame_ctx.payload_received >= s_frame_ctx.payload_len) {
                    s_frame_ctx.state = FRAME_STATE_CRC;
                }
                break;

            case FRAME_STATE_CRC:
                s_frame_ctx.crc = data;
                process_frame();
                reset_frame_context();
                break;
        }

        count--;
    }
}

CDC_FM_Mode_t CDC_FileManager_GetMode(void)
{
    return s_mode;
}

/* ============================================
 * NAND 命令处理器实现
 * ============================================ */

static void handle_nand_dl_begin(const uint8_t *payload, uint16_t payload_len)
{
    uint8_t program;
    uint32_t total_size;
    uint16_t format;
    const char *name;

    if (payload_len < 7) { /* program(1) + size(4) + format(2) + name(1+) */
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 解析参数 */
    program = payload[0];
    memcpy(&total_size, &payload[1], 4);
    memcpy(&format, &payload[5], 2);
    name = (const char *)&payload[7];

    if (program >= 128) {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 初始化 NAND 下载上下文 */
    s_nand_ctx.program = program;
    s_nand_ctx.total_size = total_size;
    s_nand_ctx.received_size = 0;
    s_nand_ctx.buffer_used = 0;
    s_nand_ctx.format = format;
    strncpy(s_nand_ctx.name, name, sizeof(s_nand_ctx.name) - 1);
    s_nand_ctx.name[sizeof(s_nand_ctx.name) - 1] = '\0';

    BG_LOG_I(BG_LOG_TAG_NAND, "Begin NAND download: program %d, %s (%d bytes, format %d)", 
             program, name, total_size, format);
    send_response(CDC_FM_RSP_ACK, NULL, 0);
}

static void handle_nand_dl_data(const uint8_t *payload, uint16_t payload_len)
{
    if (s_nand_ctx.name[0] == '\0') {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    if (s_nand_ctx.buffer_used + payload_len > sizeof(s_nand_ctx.buffer)) {
        BG_LOG_E(BG_LOG_TAG_NAND, "NAND download buffer overflow");
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 累积数据 */
    memcpy(&s_nand_ctx.buffer[s_nand_ctx.buffer_used], payload, payload_len);
    s_nand_ctx.buffer_used += payload_len;
    s_nand_ctx.received_size += payload_len;

    send_response(CDC_FM_RSP_ACK, NULL, 0);
}

static void handle_nand_dl_end(const uint8_t *payload, uint16_t payload_len)
{
    BG_ERR ret;

    if (s_nand_ctx.name[0] == '\0') {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    /* 存储到 NAND */
    ret = NAND_StoreProgram(s_nand_ctx.program, s_nand_ctx.buffer, s_nand_ctx.received_size,
                           s_nand_ctx.name, s_nand_ctx.format);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_NAND, "NAND store failed: %d", ret);
        send_response(CDC_FM_RSP_ERR_IO, NULL, 0);
    } else {
        BG_LOG_I(BG_LOG_TAG_NAND, "NAND download completed: program %d (%d bytes)", 
                 s_nand_ctx.program, s_nand_ctx.received_size);
        send_response(CDC_FM_RSP_ACK, NULL, 0);
    }

    /* 清理上下文 */
    memset(&s_nand_ctx, 0, sizeof(s_nand_ctx));
}

static void handle_nand_erase(const uint8_t *payload, uint16_t payload_len)
{
    uint8_t program;
    BG_ERR ret;

    if (payload_len != 1) {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    program = payload[0];
    if (program >= 128) {
        send_response(CDC_FM_RSP_ERR_PARAM, NULL, 0);
        return;
    }

    ret = NAND_DeleteProgram(program);
    if (ret != SUCCESS) {
        BG_LOG_W(BG_LOG_TAG_NAND, "NAND delete failed: program %d (%d)", program, ret);
        send_response(CDC_FM_RSP_ERR_IO, NULL, 0);
    } else {
        BG_LOG_I(BG_LOG_TAG_NAND, "NAND erased: program %d", program);
        send_response(CDC_FM_RSP_ACK, NULL, 0);
    }
}

static void handle_nand_info(const uint8_t *payload, uint16_t payload_len)
{
    uint32_t total_space, used_space, program_count;
    uint8_t response[12]; /* total(4) + used(4) + count(4) */

    NAND_GetStats(&total_space, &used_space, &program_count);

    memcpy(&response[0], &total_space, 4);
    memcpy(&response[4], &used_space, 4);
    memcpy(&response[8], &program_count, 4);

    send_response(CDC_FM_RSP_ACK, response, 12);
}

#else /* CDC_FILE_MANAGER_EN disabled - provide stubs */

/* Stub implementations for disabled CDC File Manager */
void CDC_FileManager_Init(void)
{
    /* Stub - CDC File Manager disabled */
}

uint8_t CDC_FileManager_InMode(void)
{
    return 0;  /* Not in CDC file manager mode */
}

void CDC_FileManager_CheckEnter(void)
{
    /* Stub - CDC File Manager disabled */
}

void CDC_FileManager_Process(void)
{
    /* Stub - CDC File Manager disabled */
}

#endif /* CDC_FILE_MANAGER_EN */