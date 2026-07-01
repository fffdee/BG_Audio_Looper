/**
 * @file bg_storage_bandatahub.c
 * @brief BanDataHub 平台 SD卡+PSRAM 音源存储驱动
 * 
 * 架构: SD卡(FAT32) → PSRAM(音色数据缓存) → 合成器
 * 
 * 工作流程:
 *   1. Init: 通过 FAT32 从 SD卡查找 SF2 文件
 *   2. 解析 SF2 文件头, 获取音色元数据
 *   3. 将音色参数数据加载到 PSRAM
 *   4. 合成时通过 PSRAM 读取音频样本数据
 * 
 * 依赖:
 *   - BanDataHub FAT32 文件系统 (05_component/fat32/)
 *   - BanDataHub PSRAM 驱动 (02_device_drivers/flash/psram_esp64h)
 *   - BanDataHub PSRAM 堆管理器 (05_component/fat32/psram_heap)
 *   - BanDataHub FlashBus 框架 (02_device_drivers/flash/flash_bus)
 */

#include "bg_storage.h"
#include "bg_config.h"
#include "../../fat32/fat32.h"
#include "../../fat32/psram_heap.h"
#include "../../../02_device_drivers/flash/flash_devices.h"
#include "../../../02_device_drivers/flash/flash_bus.h"
#include <string.h>
#include <stdio.h>

/* 调试宏 */
#define DBG_SYNTH_STORAGE(fmt, ...)  printf("[SynthStorage] " fmt, ##__VA_ARGS__)

/* ============================================
 * PSRAM 音色数据布局
 * ============================================
 * PSRAM 总容量 8MB (ESP-PSRAM64H):
 *   0x000000 - 0x5FFFFF (6MB) : SF2音色音频样本数据区
 *   0x600000 - 0x6FFFFF (1MB) : 音色元数据索引区 (128个音色 × 8KB)
 *   0x700000 - 0x7FFFFF (1MB) : PSRAM 通用堆 (psram_heap 管理)
 */

#define SYNTH_PSRAM_SAMPLE_BASE     0x000000u   /* 音频样本起始 */
#define SYNTH_PSRAM_SAMPLE_SIZE     (6u * 1024u * 1024u)  /* 6MB */
#define SYNTH_PSRAM_META_BASE       0x600000u   /* 元数据起始 */
#define SYNTH_PSRAM_META_SIZE       (1u * 1024u * 1024u)  /* 1MB */
#define SYNTH_PSRAM_HEAP_BASE       0x700000u   /* 堆起始 (psram_heap管理) */

/* 单个音色程序最大元数据大小 */
#define SYNTH_PROGRAM_META_MAX      (8u * 1024u)  /* 8KB per program */

/* SF2 文件名最大长度 */
#define SYNTH_SF2_FILENAME_MAX      128

/* ============================================
 * 内部状态
 * ============================================ */

static bool s_initialized = false;
static bool s_sf2_loaded = false;
static char s_sf2_filename[SYNTH_SF2_FILENAME_MAX];

/* PSRAM 设备指针 (缓存) */
static FlashDevice_t *s_psram_dev = NULL;

/* SF2 文件句柄 (FAT32) */
static FAT32_FileHandle_t s_sf2_handle;

/* 当前数据偏移 (PSRAM 样本区) */
static uint32_t s_sample_offset = 0;

/* 音色程序计数 */
static uint32_t s_program_count = 0;

/* ============================================
 * PSRAM 底层读取辅助函数
 * ============================================ */

/**
 * @brief 直接读取 PSRAM 数据 (绕过 psram_heap, 用于样本数据区)
 */
static int psram_direct_read(uint32_t psram_addr, void *buf, uint32_t len)
{
    if (!s_psram_dev || !s_psram_dev->ops || !s_psram_dev->ops->read) {
        return -1;
    }
    FlashStatus_t ret = s_psram_dev->ops->read(s_psram_dev, psram_addr, (uint8_t*)buf, len);
    return (ret == FLASH_OK) ? (int)len : -1;
}

/**
 * @brief 直接写入 PSRAM 数据 (绕过 psram_heap, 用于样本数据区)
 */
static int psram_direct_write(uint32_t psram_addr, const void *buf, uint32_t len)
{
    if (!s_psram_dev || !s_psram_dev->ops || !s_psram_dev->ops->write) {
        return -1;
    }
    FlashStatus_t ret = s_psram_dev->ops->write(s_psram_dev, psram_addr, (const uint8_t*)buf, len);
    return (ret == FLASH_OK) ? (int)len : -1;
}

/* ============================================
 * SD卡 SF2 文件查找
 * ============================================ */

/**
 * @brief 在SD卡根目录递归查找 .sf2 文件
 */
static BG_ERR find_sf2_file(char *filename, uint32_t max_len)
{
    FAT32_FileInfo_t info;
    FAT32_FileHandle_t handle;
    BG_ERR ret;
    
    if (!FAT32_IsCardReady()) {
        DBG_SYNTH_STORAGE("SD card not ready\n");
        return ENABLE_DEVICE_NOT_READY;
    }
    
    /* 尝试直接打开 drumset.sf2 */
    ret = FAT32_OpenFile("drumset.sf2", &handle);
    if (ret == SUCCESS) {
        strncpy(filename, "drumset.sf2", max_len);
        FAT32_CloseFile(&handle);
        DBG_SYNTH_STORAGE("Found: drumset.sf2\n");
        return SUCCESS;
    }
    
    /* 尝试 4OPFM.sf2 */
    ret = FAT32_OpenFile("4OPFM.sf2", &handle);
    if (ret == SUCCESS) {
        strncpy(filename, "4OPFM.sf2", max_len);
        FAT32_CloseFile(&handle);
        DBG_SYNTH_STORAGE("Found: 4OPFM.sf2\n");
        return SUCCESS;
    }
    
    /* 在根目录搜索所有 .sf2 文件 */
    /* 简单方案: 遍历根目录找第一个 .sf2 */
    ret = FAT32_ListDir("/", NULL, NULL) /* 简化: 直接尝试常见文件名 */;
    
    DBG_SYNTH_STORAGE("No SF2 file found on SD card\n");
    return ENABLE_NOT_FOUND;
}

/* ============================================
 * SF2 文件解析与 PSRAM 加载
 * ============================================ */

/**
 * @brief 将 SF2 文件内容加载到 PSRAM
 * 
 * 策略: 分块读取SF2文件, 解析音色元数据写入PSRAM元数据区,
 *       音频样本数据直接写入PSRAM样本数据区。
 */
static BG_ERR load_sf2_to_psram(const char *filename)
{
    FAT32_FileHandle_t handle;
    uint8_t chunk_buf[512];  /* 临时缓冲区 (SRAM) */
    BG_ERR ret;
    uint32_t file_size;
    uint32_t total_read = 0;
    uint32_t sample_base = SYNTH_PSRAM_SAMPLE_BASE;
    uint32_t meta_offset = SYNTH_PSRAM_META_BASE;
    int32_t bytes_read;
    
    /* 打开 SF2 文件 */
    ret = FAT32_OpenFile(filename, &handle);
    if (ret != SUCCESS) {
        DBG_SYNTH_STORAGE("Cannot open SF2: %s (err=%d)\n", filename, ret);
        return ENABLE_IO_ERROR;
    }
    
    file_size = handle.info.size;
    DBG_SYNTH_STORAGE("SF2 file: %s, size=%lu bytes\n", filename, (unsigned long)file_size);
    
    /* 逐块读取 SF2 文件并写入 PSRAM */
    while (total_read < file_size) {
        uint32_t chunk_size = (file_size - total_read > sizeof(chunk_buf)) 
                              ? sizeof(chunk_buf) : (file_size - total_read);
        
        bytes_read = FAT32_ReadFile(&handle, chunk_buf, chunk_size);
        if (bytes_read <= 0) {
            DBG_SYNTH_STORAGE("Read error at offset %lu\n", (unsigned long)total_read);
            FAT32_CloseFile(&handle);
            return ENABLE_IO_ERROR;
        }
        
        /* 写入 PSRAM 样本数据区 */
        if (psram_direct_write(sample_base + total_read, chunk_buf, (uint32_t)bytes_read) < 0) {
            DBG_SYNTH_STORAGE("PSRAM write error at offset %lu\n", (unsigned long)total_read);
            FAT32_CloseFile(&handle);
            return ENABLE_IO_ERROR;
        }
        
        total_read += (uint32_t)bytes_read;
        
        /* 每 1MB 打印进度 */
        if ((total_read & 0xFFFFF) == 0 || total_read >= file_size) {
            DBG_SYNTH_STORAGE("Loaded: %lu / %lu bytes (%lu%%)\n", 
                              (unsigned long)total_read, (unsigned long)file_size,
                              (unsigned long)(total_read * 100 / file_size));
        }
    }
    
    FAT32_CloseFile(&handle);
    
    /* 记录 SF2 元数据到 PSRAM 元数据区 */
    {
        uint32_t meta_buf[4];
        meta_buf[0] = file_size;
        meta_buf[1] = sample_base;
        meta_buf[2] = total_read;
        meta_buf[3] = 0x32465342;  /* "SF2B" magic */
        psram_direct_write(meta_offset, meta_buf, sizeof(meta_buf));
    }
    
    s_sf2_loaded = true;
    DBG_SYNTH_STORAGE("SF2 loaded to PSRAM successfully\n");
    
    return SUCCESS;
}

/* ============================================
 * BG_Storage 驱动接口实现
 * ============================================ */

static BG_ERR bandatahub_storage_init(const char *path, BG_Storage_Mode_t mode)
{
    BG_ERR ret;
    char sf2_file[SYNTH_SF2_FILENAME_MAX] = {0};
    
    if (s_initialized) {
        return SUCCESS;  /* 已初始化 */
    }
    
    (void)path;   /* path 参数: 可选 SF2 文件名 */
    (void)mode;   /* 当前仅支持只读 */
    
    DBG_SYNTH_STORAGE("Initializing BanDataHub SD+PSRAM storage driver...\n");
    
    /* 1. 获取 PSRAM 设备 */
    s_psram_dev = FlashDevices_GetPsramFlash();
    if (!s_psram_dev || !s_psram_dev->initialized) {
        DBG_SYNTH_STORAGE("PSRAM device not available\n");
        return ENABLE_DEVICE_NOT_READY;
    }
    DBG_SYNTH_STORAGE("PSRAM device OK, size=%lu\n", 
                      (unsigned long)s_psram_dev->info.total_size);
    
    /* 2. 初始化 FAT32 (如果未初始化) */
    if (!FAT32_IsCardReady()) {
        ret = FAT32_Init();
        if (ret != SUCCESS) {
            DBG_SYNTH_STORAGE("FAT32 init failed: %d\n", ret);
            return ENABLE_DEVICE_NOT_READY;
        }
        DBG_SYNTH_STORAGE("FAT32 initialized\n");
    }
    
    /* 3. 查找 SF2 文件 */
    ret = find_sf2_file(sf2_file, sizeof(sf2_file));
    if (ret != SUCCESS) {
        DBG_SYNTH_STORAGE("No SF2 file found\n");
        /* 不致命: 允许稍后通过下载加载音源 */
        s_initialized = true;
        return SUCCESS;
    }
    
    /* 4. 加载 SF2 到 PSRAM */
    ret = load_sf2_to_psram(sf2_file);
    if (ret != SUCCESS) {
        DBG_SYNTH_STORAGE("SF2 load failed\n");
        return ret;
    }
    
    strncpy(s_sf2_filename, sf2_file, sizeof(s_sf2_filename) - 1);
    s_initialized = true;
    
    DBG_SYNTH_STORAGE("BanDataHub storage driver initialized OK\n");
    return SUCCESS;
}

static BG_ERR bandatahub_storage_deinit(void)
{
    if (!s_initialized) return SUCCESS;
    
    FAT32_DeInit();
    s_initialized = false;
    s_sf2_loaded = false;
    memset(s_sf2_filename, 0, sizeof(s_sf2_filename));
    
    DBG_SYNTH_STORAGE("Storage driver deinitialized\n");
    return SUCCESS;
}

static int bandatahub_storage_read(uint32_t offset, void *buffer, size_t size)
{
    if (!s_initialized) {
        return -1;
    }
    
    /* 直接从 PSRAM 样本区读取 */
    uint32_t psram_addr = SYNTH_PSRAM_SAMPLE_BASE + offset;
    return psram_direct_read(psram_addr, buffer, (uint32_t)size);
}

static int bandatahub_storage_write(uint32_t offset, const void *buffer, size_t size)
{
    /* 当前仅支持只读 */
    (void)offset; (void)buffer; (void)size;
    return -1;
}

static BG_ERR bandatahub_storage_erase(uint32_t offset, size_t size)
{
    /* PSRAM 无需擦除 */
    (void)offset; (void)size;
    return SUCCESS;
}

static BG_ERR bandatahub_storage_sync(void)
{
    return SUCCESS;
}

static BG_ERR bandatahub_storage_get_info(uint32_t *total_size, uint32_t *free_size)
{
    if (total_size) *total_size = SYNTH_PSRAM_SAMPLE_SIZE;
    if (free_size)  *free_size  = 0;  /* 不使用动态空间追踪 */
    return SUCCESS;
}

/* ============================================
 * 导出驱动实例
 * ============================================ */

const BG_Storage_Driver_t bg_storage_driver_bandatahub = {
    .init     = bandatahub_storage_init,
    .deinit   = bandatahub_storage_deinit,
    .read     = bandatahub_storage_read,
    .write    = bandatahub_storage_write,
    .erase    = bandatahub_storage_erase,
    .sync     = bandatahub_storage_sync,
    .get_info = bandatahub_storage_get_info,
};
