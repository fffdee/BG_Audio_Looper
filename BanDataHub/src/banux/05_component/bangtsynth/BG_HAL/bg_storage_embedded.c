/**
 * @file bg_storage_embedded.c
 * @brief BG_Storage 驱动 — 内嵌 C 数组音源 (只读)
 * 
 * 将编译期嵌入的 SF2 数据 (const uint8_t 数组) 直接映射为
 * BG_Storage 驱动, 实现零拷贝只读访问。
 * 无需 Flash 读取, 直接从 ROM/Flash Code 区域 memcpy。
 * 
 * 用于:
 *   - 默认内置音源 (开机即可用, 无需下载)
 *   - 测试音源
 * 
 * 编译条件: BANGTSYNTH_EN
 */

#include "bg_config.h"
#include "bg_storage.h"
#include "bg_log.h"
#include <string.h>

#ifdef BANGTSYNTH_EN

/* ============================================
 * 内嵌音源数据 (由 sf2_to_c_converter.py 生成)
 * ============================================ */

    #include "tip_data.h"
    #define EMBEDDED_SF2_DATA       tip_data
    #define EMBEDDED_SF2_SIZE       TIP_SIZE
    #define EMBEDDED_SF2_NAME       "Thrift Store Spinet Piano"

/* ============================================
 * 内部状态
 * ============================================ */
static const uint8_t *g_emb_data = NULL;
static uint32_t       g_emb_size = 0;
static uint8_t        g_emb_initialized = 0;

/* ============================================
 * 驱动接口实现
 * ============================================ */

static BG_ERR emb_storage_init(const char *path, BG_Storage_Mode_t mode)
{
    (void)path;

    if (mode != BG_STORAGE_MODE_READ_ONLY) {
        BG_LOG_W(BG_LOG_TAG_HAL, "[EMB] Embedded storage is read-only\n");
        /* 仍然允许, 写入操作会返回错误 */
    }

    g_emb_data = EMBEDDED_SF2_DATA;
    g_emb_size = EMBEDDED_SF2_SIZE;
    g_emb_initialized = 1;

    BG_LOG_I(BG_LOG_TAG_HAL, "[EMB] %s loaded, size=%u bytes\n", EMBEDDED_SF2_NAME, g_emb_size);

    return SUCCESS;
}

static BG_ERR emb_storage_deinit(void)
{
    g_emb_initialized = 0;
    BG_LOG_I(BG_LOG_TAG_HAL, "[EMB] Embedded storage deinit\n");
    return SUCCESS;
}

static int emb_storage_read(uint32_t offset, void *buffer, size_t size)
{
    if (!g_emb_initialized || !buffer || size == 0) {
        return -1;
    }

    if (offset >= g_emb_size) {
        return -1;
    }

    /* 限制读取范围 */
    if (offset + size > g_emb_size) {
        size = g_emb_size - offset;
    }

    memcpy(buffer, g_emb_data + offset, size);
    return (int)size;
}

static int emb_storage_write(uint32_t offset, const void *buffer, size_t size)
{
    (void)offset; (void)buffer; (void)size;
    BG_LOG_W(BG_LOG_TAG_HAL, "[EMB] Write not supported on embedded storage\n");
    return -1;
}

static BG_ERR emb_storage_erase(uint32_t offset, size_t size)
{
    (void)offset; (void)size;
    BG_LOG_W(BG_LOG_TAG_HAL, "[EMB] Erase not supported on embedded storage\n");
    return ENABLE_INVALID_INPUT;
}

static BG_ERR emb_storage_sync(void)
{
    return SUCCESS;
}

static BG_ERR emb_storage_get_info(uint32_t *total_size, uint32_t *free_size)
{
    if (total_size) {
        *total_size = g_emb_size;
    }
    if (free_size) {
        *free_size = 0;  /* 只读, 无可用空间 */
    }
    return SUCCESS;
}

/* ============================================
 * 导出内嵌驱动实例
 * ============================================ */
const BG_Storage_Driver_t bg_storage_driver_embedded = {
    .init     = emb_storage_init,
    .deinit   = emb_storage_deinit,
    .read     = emb_storage_read,
    .write    = emb_storage_write,
    .erase    = emb_storage_erase,
    .sync     = emb_storage_sync,
    .get_info = emb_storage_get_info
};

#endif /* BANGTSYNTH_EN */
