/**
 * @file bg_storage_bp10.c
 * @brief BG_Storage 驱动 — BP10 平台实现
 * 
 * 将 BG_Storage 抽象层映射到 BP10 的 BG_FlashMgr Storage 分区 (Flash#1, 8MB)。
 * 音源数据存放在 Flash#1 的 Storage 分区中。
 * 
 * 编译条件: BG_TARGET_PLATFORM == BG_PLATFORM_BP10
 */

#include "product_def.h"

#if BANGTSYNTH_EN

#include "bg_config.h"
#if (BG_TARGET_PLATFORM == BG_PLATFORM_BP10) && !defined(BANDATAHUB)

#include "bg_storage.h"
#include "bg_log.h"
#include "BG_FlashMgr.h"
#include <string.h>

/* ============================================
 * BP10 Storage 配置
 * ============================================ */
#define BP10_STORAGE_TOTAL_SIZE     (8 * 1024 * 1024)   /* Flash#1: 8MB */
#define BP10_STORAGE_SECTOR_SIZE    (4096)                /* 4KB 扇区 */

/* ============================================
 * 内部状态
 * ============================================ */
static uint8_t g_bp10_storage_initialized = 0;

/* ============================================
 * 驱动接口实现
 * ============================================ */

/**
 * 初始化 BP10 存储
 * BP10 的 BG_FlashMgr 在 main.c 中已经初始化,
 * 此处仅检查就绪状态。
 */
static BG_ERR bp10_storage_init(const char *path, BG_Storage_Mode_t mode)
{
    (void)path;   /* BP10 使用固定的 Storage 分区, 忽略 path */
    (void)mode;   /* Flash 天然支持读写, 忽略 mode */

    if (!BG_FlashMgr.IsReady()) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[BP10] FlashMgr not ready!\n");
        return ENABLE_INVALID_INPUT;
    }

    g_bp10_storage_initialized = 1;
    BG_LOG_I(BG_LOG_TAG_HAL, "[BP10] Storage driver initialized (Flash#1, %dMB)\n",
             BP10_STORAGE_TOTAL_SIZE / (1024 * 1024));

    return SUCCESS;
}

/**
 * 反初始化
 */
static BG_ERR bp10_storage_deinit(void)
{
    g_bp10_storage_initialized = 0;
    BG_LOG_I(BG_LOG_TAG_HAL, "[BP10] Storage driver deinitialized\n");
    return SUCCESS;
}

/**
 * 从 Storage 分区读取数据
 */
static int bp10_storage_read(uint32_t offset, void *buffer, size_t size)
{
    if (!g_bp10_storage_initialized || !buffer || size == 0) {
        return -1;
    }

    if (offset + size > BP10_STORAGE_TOTAL_SIZE) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[BP10] Read out of range: off=0x%X, size=%u\n",
                 offset, (uint32_t)size);
        return -1;
    }

    int32_t ret = BG_FlashMgr.ReadStorage(offset, (uint8_t *)buffer, (uint32_t)size);
    if (ret != 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[BP10] ReadStorage failed: off=0x%X, size=%u, ret=%d\n",
                 offset, (uint32_t)size, ret);
        return -1;
    }

    return (int)size;
}

/**
 * 向 Storage 分区写入数据
 * 注意: NOR Flash 写入前需要先擦除, 调用方应自行擦除
 */
static int bp10_storage_write(uint32_t offset, const void *buffer, size_t size)
{
    if (!g_bp10_storage_initialized || !buffer || size == 0) {
        return -1;
    }

    if (offset + size > BP10_STORAGE_TOTAL_SIZE) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[BP10] Write out of range: off=0x%X, size=%u\n",
                 offset, (uint32_t)size);
        return -1;
    }

    int32_t ret = BG_FlashMgr.WriteStorage(offset, (const uint8_t *)buffer, (uint32_t)size);
    if (ret != 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[BP10] WriteStorage failed: off=0x%X, size=%u, ret=%d\n",
                 offset, (uint32_t)size, ret);
        return -1;
    }

    return (int)size;
}

/**
 * 擦除 Storage 分区的指定扇区
 */
static BG_ERR bp10_storage_erase(uint32_t offset, size_t size)
{
    uint32_t erased = 0;

    if (!g_bp10_storage_initialized) {
        return ENABLE_INVALID_INPUT;
    }

    if (offset % BP10_STORAGE_SECTOR_SIZE != 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "[BP10] Erase offset not aligned: 0x%X\n", offset);
        return ENABLE_INVALID_INPUT;
    }

    /* 按扇区逐个擦除 */
    while (erased < size) {
        int32_t ret = BG_FlashMgr.EraseStorageSector(offset + erased);
        if (ret != 0) {
            BG_LOG_E(BG_LOG_TAG_HAL, "[BP10] EraseStorageSector failed at 0x%X\n",
                     offset + erased);
            return ENABLE_INVALID_INPUT;
        }
        erased += BP10_STORAGE_SECTOR_SIZE;
    }

    return SUCCESS;
}

/**
 * 同步 — NOR Flash 写入即同步, 无需额外操作
 */
static BG_ERR bp10_storage_sync(void)
{
    return SUCCESS;
}

/**
 * 获取 Storage 分区信息
 */
static BG_ERR bp10_storage_get_info(uint32_t *total_size, uint32_t *free_size)
{
    if (total_size) {
        *total_size = BP10_STORAGE_TOTAL_SIZE;
    }
    if (free_size) {
        *free_size = BG_FlashMgr.GetStorageFreeSpace();
    }
    return SUCCESS;
}

/* ============================================
 * 导出 BP10 驱动实例
 * ============================================ */
const BG_Storage_Driver_t bg_storage_driver_bp10 = {
    .init     = bp10_storage_init,
    .deinit   = bp10_storage_deinit,
    .read     = bp10_storage_read,
    .write    = bp10_storage_write,
    .erase    = bp10_storage_erase,
    .sync     = bp10_storage_sync,
    .get_info = bp10_storage_get_info
};

#endif /* BG_TARGET_PLATFORM == BG_PLATFORM_BP10 */

#endif /* BANGTSYNTH_EN */
