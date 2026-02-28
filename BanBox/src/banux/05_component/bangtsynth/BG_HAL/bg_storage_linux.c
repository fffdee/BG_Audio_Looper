#include "bg_config.h"
#if (BG_TARGET_PLATFORM == BG_PLATFORM_LINUX)

/**
 * BG_Storage Linux 平台驱动
 * 
 * 功能:
 * - 使用文件系统存储音源数据
 * - 支持固定 16MB 大小的 bin 文件
 * - 支持读写操作
 */

#include "bg_storage.h"
#include "bg_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* ============================================
 * Linux 驱动内部状态
 * ============================================ */
static struct {
    FILE *file;
    char path[256];
    BG_Storage_Mode_t mode;
} g_linux_storage = {
    .file = NULL,
    .mode = BG_STORAGE_MODE_READ_ONLY
};

/* ============================================
 * Linux 驱动实现
 * ============================================ */

static BG_ERR linux_storage_init(const char *path, BG_Storage_Mode_t mode)
{
    if (g_linux_storage.file) {
        BG_LOG_W(BG_LOG_TAG_HAL, "Linux storage already opened");
        return ENABLE_INVALID_INPUT;
    }
    
    /* 确定打开模式 */
    const char *fopen_mode = NULL;
    switch (mode) {
        case BG_STORAGE_MODE_READ_ONLY:
            fopen_mode = "rb";
            break;
        case BG_STORAGE_MODE_WRITE_ONLY:
            fopen_mode = "wb";
            break;
        case BG_STORAGE_MODE_READ_WRITE:
            /* 先尝试 r+b (文件必须存在), 失败则 w+b (创建新文件) */
            fopen_mode = "r+b";
            g_linux_storage.file = fopen(path, fopen_mode);
            if (!g_linux_storage.file) {
                fopen_mode = "w+b";
            }
            break;
        default:
            BG_LOG_E(BG_LOG_TAG_HAL, "Invalid storage mode: %d", mode);
            return ENABLE_INVALID_INPUT;
    }
    
    /* 打开文件 */
    if (!g_linux_storage.file) {
        g_linux_storage.file = fopen(path, fopen_mode);
    }
    
    if (!g_linux_storage.file) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Failed to open storage file: %s (errno=%d: %s)",
                 path, errno, strerror(errno));
        return ENABLE_INVALID_INPUT;
    }
    
    /* 读写模式或只写模式: 确保文件大小为 16MB */
    if (mode != BG_STORAGE_MODE_READ_ONLY) {
        /* 获取当前文件大小 */
        fseek(g_linux_storage.file, 0, SEEK_END);
        long current_size = ftell(g_linux_storage.file);
        
        if (current_size < BG_STORAGE_SIZE) {
            BG_LOG_I(BG_LOG_TAG_HAL, "Expanding storage file from %ld to %u bytes",
                     current_size, BG_STORAGE_SIZE);
            
            /* 扩展文件到 16MB (写入 0 填充) */
            uint8_t zero_buffer[4096] = {0};
            size_t remaining = BG_STORAGE_SIZE - current_size;
            
            while (remaining > 0) {
                size_t write_size = (remaining > sizeof(zero_buffer)) ? 
                                   sizeof(zero_buffer) : remaining;
                
                if (fwrite(zero_buffer, 1, write_size, g_linux_storage.file) != write_size) {
                    BG_LOG_E(BG_LOG_TAG_HAL, "Failed to expand storage file");
                    fclose(g_linux_storage.file);
                    g_linux_storage.file = NULL;
                    return ENABLE_INVALID_INPUT;
                }
                
                remaining -= write_size;
            }
            
            fflush(g_linux_storage.file);
        }
        
        fseek(g_linux_storage.file, 0, SEEK_SET);
    }
    
    /* 保存状态 */
    strncpy(g_linux_storage.path, path, sizeof(g_linux_storage.path) - 1);
    g_linux_storage.mode = mode;
    
    BG_LOG_I(BG_LOG_TAG_HAL, "Linux storage opened: %s (mode=%s)",
             path, fopen_mode);
    
    return SUCCESS;
}

static BG_ERR linux_storage_deinit(void)
{
    if (!g_linux_storage.file) {
        return SUCCESS;
    }
    
    fclose(g_linux_storage.file);
    g_linux_storage.file = NULL;
    
    BG_LOG_I(BG_LOG_TAG_HAL, "Linux storage closed: %s", g_linux_storage.path);
    
    return SUCCESS;
}

static int linux_storage_read(uint32_t offset, void *buffer, size_t size)
{
    if (!g_linux_storage.file) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage file not opened");
        return -1;
    }
    
    /* 定位到偏移 */
    if (fseek(g_linux_storage.file, offset, SEEK_SET) != 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Failed to seek to offset %u: %s",
                 offset, strerror(errno));
        return -1;
    }
    
    /* 读取数据 */
    size_t read_count = fread(buffer, 1, size, g_linux_storage.file);
    
    if (read_count != size) {
        if (feof(g_linux_storage.file)) {
            BG_LOG_W(BG_LOG_TAG_HAL, "Read reached EOF: requested %u, got %u",
                     (uint32_t)size, (uint32_t)read_count);
        } else if (ferror(g_linux_storage.file)) {
            BG_LOG_E(BG_LOG_TAG_HAL, "Read error at offset %u", offset);
            return -1;
        }
    }
    
    return (int)read_count;
}

static int linux_storage_write(uint32_t offset, const void *buffer, size_t size)
{
    if (!g_linux_storage.file) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Storage file not opened");
        return -1;
    }
    
    /* 定位到偏移 */
    if (fseek(g_linux_storage.file, offset, SEEK_SET) != 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Failed to seek to offset %u: %s",
                 offset, strerror(errno));
        return -1;
    }
    
    /* 写入数据 */
    size_t write_count = fwrite(buffer, 1, size, g_linux_storage.file);
    
    if (write_count != size) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Write error at offset %u: requested %u, wrote %u",
                 offset, (uint32_t)size, (uint32_t)write_count);
        return -1;
    }
    
    return (int)write_count;
}

static BG_ERR linux_storage_erase(uint32_t offset, size_t size)
{
    /* Linux 文件系统不需要擦除,直接写 0 */
    uint8_t zero_buffer[BG_STORAGE_SECTOR_SIZE] = {0};
    
    size_t remaining = size;
    uint32_t current_offset = offset;
    
    while (remaining > 0) {
        size_t erase_size = (remaining > sizeof(zero_buffer)) ? 
                           sizeof(zero_buffer) : remaining;
        
        int result = linux_storage_write(current_offset, zero_buffer, erase_size);
        if (result < 0) {
            BG_LOG_E(BG_LOG_TAG_HAL, "Erase failed at offset %u", current_offset);
            return ENABLE_INVALID_INPUT;
        }
        
        current_offset += erase_size;
        remaining -= erase_size;
    }
    
    return SUCCESS;
}

static BG_ERR linux_storage_sync(void)
{
    if (!g_linux_storage.file) {
        return ENABLE_INVALID_INPUT;
    }
    
    if (fflush(g_linux_storage.file) != 0) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Failed to flush storage file: %s",
                 strerror(errno));
        return ENABLE_INVALID_INPUT;
    }
    
    /* 同步到磁盘 */
    int fd = fileno(g_linux_storage.file);
    if (fd >= 0) {
        fsync(fd);
    }
    
    return SUCCESS;
}

static BG_ERR linux_storage_get_info(uint32_t *total_size, uint32_t *free_size)
{
    if (!g_linux_storage.file) {
        return ENABLE_INVALID_INPUT;
    }
    
    /* 总容量固定为 16MB (模拟硬件Flash大小) */
    if (total_size) {
        *total_size = BG_STORAGE_SIZE;
    }
    
    /* Linux 文件系统可用空间查询 (可选实现) */
    if (free_size) {
        *free_size = 0;  /* 不实现可用空间查询 */
    }
    
    return SUCCESS;
}

/* ============================================
 * 导出驱动实例
 * ============================================ */
const BG_Storage_Driver_t bg_storage_driver_linux = {
    .init = linux_storage_init,
    .deinit = linux_storage_deinit,
    .read = linux_storage_read,
    .write = linux_storage_write,
    .erase = linux_storage_erase,
    .sync = linux_storage_sync,
    .get_info = linux_storage_get_info
};

#endif /* BG_TARGET_PLATFORM == BG_PLATFORM_LINUX */
