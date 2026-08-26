/**
 * BG_Storage - 音源存储抽象层实现
 */

#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_storage.h"
#include "bg_log.h"
#include <string.h>

/* ============================================
 * 内部状态
 * ============================================ */
typedef struct {
    const BG_Storage_Driver_t *driver;  /* 当前使用的驱动 */
    BG_Storage_Mode_t mode;             /* 访问模式 */
    uint8_t initialized;                /* 初始化标志 */
} BG_Storage_State_t;

static BG_Storage_State_t g_storage_state = {
    .driver = NULL,
    .mode = BG_STORAGE_MODE_READ_ONLY,
    .initialized = 0
};

/* ============================================
 * 内部函数
 * ============================================ */

/**
 * 选择默认平台驱动
 */
static const BG_Storage_Driver_t* select_default_driver(void)
{
#if (BG_TARGET_PLATFORM == BG_PLATFORM_LINUX)
    return &bg_storage_driver_linux;
#elif (BG_TARGET_PLATFORM == BG_PLATFORM_STM32)
    return &bg_storage_driver_stm32;
#elif (BG_TARGET_PLATFORM == BG_PLATFORM_ESP32)
    return &bg_storage_driver_esp32;
#elif (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)
#if BG_CFG_USE_PORT_STORAGE
        return &bg_storage_driver_port;
#else
        return &bg_storage_driver_bp10;
#endif
#else
    #error "Unsupported platform for BG_Storage"
    return NULL;
#endif
}

/**
 * 验证偏移和大小是否合法
 */
static BG_ERR validate_range(uint32_t offset, size_t size)
{
    if (offset >= BG_STORAGE_SIZE) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage offset out of range: %u >= %u", 
                 offset, BG_STORAGE_SIZE);
        return ENABLE_INVALID_INPUT;
    }
    
    if (offset + size > BG_STORAGE_SIZE) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage read/write exceeds boundary: %u + %u > %u",
                 offset, (uint32_t)size, BG_STORAGE_SIZE);
        return ENABLE_INVALID_INPUT;
    }
    
    return SUCCESS;
}

/* ============================================
 * 公共接口实现
 * ============================================ */

static BG_ERR storage_init(const char *path, BG_Storage_Mode_t mode)
{
    BG_ERR ret;

    if (g_storage_state.initialized) {
        BG_LOG_W(BG_LOG_TAG_HAL, "Storage already initialized, deinit first");
        return ENABLE_INVALID_INPUT;
    }
    
    if (!path) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage path is NULL");
        return ENABLE_INVALID_INPUT;
    }
    
    /* 选择驱动 (如果未手动设置) */
    if (!g_storage_state.driver) {
        g_storage_state.driver = select_default_driver();
        if (!g_storage_state.driver) {
            BG_LOG_E(BG_LOG_TAG_HAL, "No storage driver available");
            return ENABLE_INVALID_INPUT;
        }
    }
    
    /* 调用驱动初始化 */
    ret = g_storage_state.driver->init(path, mode);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage driver init failed: %d", ret);
        return ret;
    }
    
    g_storage_state.mode = mode;
    g_storage_state.initialized = 1;
    
    BG_LOG_I(BG_LOG_TAG_HAL, "Storage initialized: path=%s, mode=%d, size=%uMB",
             path, mode, BG_STORAGE_SIZE / (1024*1024));
    
    return SUCCESS;
}

static BG_ERR storage_deinit(void)
{
    BG_ERR ret;

    if (!g_storage_state.initialized) {
        return SUCCESS;
    }
    
    ret = g_storage_state.driver->deinit();
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage driver deinit failed: %d", ret);
    }
    
    g_storage_state.initialized = 0;
    BG_LOG_I(BG_LOG_TAG_HAL, "Storage deinitialized");
    
    return ret;
}

static int storage_read(uint32_t offset, void *buffer, size_t size)
{
    int result;

    if (!g_storage_state.initialized) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage not initialized");
        return -1;
    }
    
    if (!buffer || size == 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Invalid read parameters");
        return -1;
    }
    
    /* 验证范围 */
    if (validate_range(offset, size) != SUCCESS) {
        return -1;
    }
    
    /* 检查访问权限 */
    if (!(g_storage_state.mode & BG_STORAGE_MODE_READ_ONLY)) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage not opened for reading");
        return -1;
    }
    
    /* 调用驱动读取 */
    result = g_storage_state.driver->read(offset, buffer, size);
    
    if (result < 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage read failed at offset %u, size %u", 
                 offset, (uint32_t)size);
    }
    
    return result;
}

static int storage_write(uint32_t offset, const void *buffer, size_t size)
{
    int result;

    if (!g_storage_state.initialized) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage not initialized");
        return -1;
    }
    
    if (!buffer || size == 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Invalid write parameters");
        return -1;
    }
    
    /* 验证范围 */
    if (validate_range(offset, size) != SUCCESS) {
        return -1;
    }
    
    /* 检查访问权限 */
    if (!(g_storage_state.mode & BG_STORAGE_MODE_WRITE_ONLY)) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage not opened for writing");
        return -1;
    }
    
    /* 调用驱动写入 */
    result = g_storage_state.driver->write(offset, buffer, size);
    
    if (result < 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage write failed at offset %u, size %u",
                 offset, (uint32_t)size);
    }
    
    return result;
}

static BG_ERR storage_erase(uint32_t offset, size_t size)
{
    BG_ERR ret;

    if (!g_storage_state.initialized) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage not initialized");
        return ENABLE_INVALID_INPUT;
    }
    
    /* 验证范围 */
    if (validate_range(offset, size) != SUCCESS) {
        return ENABLE_INVALID_INPUT;
    }
    
    /* 检查扇区对齐 */
    if (offset % BG_STORAGE_SECTOR_SIZE != 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Erase offset not aligned to sector: %u", offset);
        return ENABLE_INVALID_INPUT;
    }
    
    if (size % BG_STORAGE_SECTOR_SIZE != 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Erase size not aligned to sector: %u", (uint32_t)size);
        return ENABLE_INVALID_INPUT;
    }
    
    /* 调用驱动擦除 */
    ret = g_storage_state.driver->erase(offset, size);
    
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage erase failed at offset %u, size %u",
                 offset, (uint32_t)size);
    }
    
    return ret;
}

static BG_ERR storage_sync(void)
{
    if (!g_storage_state.initialized) {
        return ENABLE_INVALID_INPUT;
    }
    
    return g_storage_state.driver->sync();
}

static BG_ERR storage_get_info(uint32_t *total_size, uint32_t *used_size)
{
    uint32_t free_size = 0;
    BG_ERR ret;

    if (!g_storage_state.initialized) {
        return ENABLE_INVALID_INPUT;
    }
    
    ret = g_storage_state.driver->get_info(total_size, &free_size);
    
    if (ret == SUCCESS && used_size) {
        *used_size = (*total_size > free_size) ? (*total_size - free_size) : 0;
    }
    
    return ret;
}

static void storage_set_driver(const BG_Storage_Driver_t *driver)
{
    if (g_storage_state.initialized) {
        BG_LOG_W(BG_LOG_TAG_HAL, "Cannot change driver while storage is initialized");
        return;
    }
    
    g_storage_state.driver = driver;
    BG_LOG_I(BG_LOG_TAG_HAL, "Custom storage driver set");
}

/* ============================================
 * 导出接口实例
 * ============================================ */
BG_Storage_t BG_Storage = {
    .Init = storage_init,
    .DeInit = storage_deinit,
    .Read = storage_read,
    .Write = storage_write,
    .Erase = storage_erase,
    .Sync = storage_sync,
    .GetInfo = storage_get_info,
    .SetDriver = storage_set_driver
};

/* ============================================
 * 平台驱动实现在独立的 .c 文件中
 * (由 Makefile 编译链接)
 * ============================================ */

#endif /* BANGTSYNTH_EN */
