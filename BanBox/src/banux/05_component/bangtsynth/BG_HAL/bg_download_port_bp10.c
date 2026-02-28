/**
 * @file bg_download_port_bp10.c
 * @brief BG_Download_Port - BP10 平台实现
 * 
 * 功能:
 * - BP10平台音源数据下载接口
 * - 从 Flash Storage 分区读取预先存储的音源数据
 * 
 * 使用场景:
 * - 音源数据已通过工具预先烧录到 Flash Storage 分区
 * - soundbank_download() 通过此接口读取音源数据
 * 
 * 编译条件: BG_TARGET_PLATFORM == BG_PLATFORM_BP10
 */

#include "bg_config.h"
#if (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)

#include "bg_download_port.h"
#include "bg_log.h"
#include "BG_FlashMgr.h"
#include <string.h>
#include <stdlib.h>

/* 内部状态 */
typedef struct {
    uint32_t flash_offset;      /* Flash中的当前读取偏移 */
    uint32_t total_size;        /* 音源文件总大小 */
    uint32_t bytes_read;        /* 已读取的字节数 */
    uint8_t  active;            /* 是否正在下载 */
} BP10_DownloadState_t;

static BP10_DownloadState_t g_download_state = {0};

/**
 * 从 Flash Storage 分区读取数据
 * 
 * @param source Flash偏移地址 (十进制字符串,如 "0", "4096" 等)
 *               或音源文件标识 (如 "soundbank_0")
 * @param buffer 数据缓冲区
 * @param size   期望读取的字节数
 * @param bytes_read 实际读取的字节数 (输出)
 * @return 0=成功, <0=错误
 * 
 * 使用示例:
 *   // 从 Flash offset=0 开始读取 4096 字节
 *   bg_download_port_read("0", buffer, 4096, &n);
 */
int bg_download_port_read(const char *source, void *buffer, size_t size, size_t *bytes_read)
{
    if (!source || !buffer || !bytes_read) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[BP10] Invalid parameters\n");
        return -1;
    }
    
    /* 检查 FlashMgr 是否就绪 */
    if (!BG_FlashMgr.IsReady()) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[BP10] FlashMgr not ready\n");
        return -1;
    }
    
    /* 解析 source 参数为 Flash 偏移地址 */
    uint32_t flash_offset = 0;
    
    /* 尝试解析为数字 (十进制偏移地址) */
    char *endptr = NULL;
    flash_offset = (uint32_t)strtoul(source, &endptr, 10);
    
    /* 如果解析失败,尝试作为文件名查找 (预留扩展) */
    if (endptr == source || *endptr != '\0') {
        /* 可在此处添加音源文件名->偏移映射表查询 */
        BG_LOG_W(BG_LOG_TAG_HAL, "[BP10] Invalid source format: %s (expected offset)\n", source);
        return -1;
    }
    
    /* 检查是否是新的下载会话 */
    if (!g_download_state.active || g_download_state.flash_offset != flash_offset) {
        BG_LOG_I(BG_LOG_TAG_HAL, "[BP10] Starting download from Flash offset: %u\n", flash_offset);
        g_download_state.flash_offset = flash_offset;
        g_download_state.bytes_read = 0;
        g_download_state.active = 1;
        /* total_size 需要从音源文件头读取,暂时设为最大值 */
        g_download_state.total_size = 8 * 1024 * 1024;  /* Flash#1 总大小 */
    }
    
    /* 计算实际可读取的字节数 */
    uint32_t remaining = g_download_state.total_size - g_download_state.bytes_read;
    size_t read_size = (size < remaining) ? size : remaining;
    
    if (read_size == 0) {
        /* 下载完成 */
        BG_LOG_I(BG_LOG_TAG_HAL, "[BP10] Download completed (%u bytes)\n", 
                 g_download_state.bytes_read);
        *bytes_read = 0;
        g_download_state.active = 0;
        return 0;
    }
    
    /* 从 Flash Storage 分区读取数据 */
    uint32_t current_offset = g_download_state.flash_offset + g_download_state.bytes_read;
    
    int ret = BG_FlashMgr.ReadStorage(current_offset, buffer, read_size);
    if (ret != 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[BP10] Flash read failed at offset %u, size %u\n",
                 current_offset, read_size);
        g_download_state.active = 0;
        return -1;
    }
    
    /* 更新状态 */
    g_download_state.bytes_read += read_size;
    *bytes_read = read_size;
    
    BG_LOG_D(BG_LOG_TAG_HAL, "[BP10] Read %u bytes from Flash offset %u (total: %u)\n",
             read_size, current_offset, g_download_state.bytes_read);
    
    return 0;
}

/*
 * BP10 平台使用说明:
 * ==================
 * 
 * 1. 音源数据预烧录
 *    - 使用工具将 SF2/BGS 文件烧录到 Flash#1 的 Storage 分区
 *    - 记录烧录的起始偏移地址 (如 0x000000)
 * 
 * 2. 调用下载接口
 *    soundbank_download("0");  // 从偏移0开始读取
 * 
 * 3. 下载流程
 *    - soundbank_manager 调用 bg_download_port_read() 循环读取数据
 *    - 本实现从 Flash Storage 分区读取预烧录的音源
 *    - 自动检测下载完成并复位状态
 * 
 * 扩展功能:
 * - 可添加音源文件名映射表,支持按名称下载
 * - 可从音源文件头读取真实大小,实现精确下载
 */

#endif /* BG_TARGET_PLATFORM == BG_PLATFORM_BP10 */
