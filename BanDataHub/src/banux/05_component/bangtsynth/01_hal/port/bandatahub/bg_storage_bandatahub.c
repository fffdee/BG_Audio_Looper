/**
 * @file bg_storage_bandatahub.c
 * @brief BanDataHub 平台 SD卡+PSRAM 音源存储驱动（纯合成器工程）
 *
 * 架构: SD卡(FAT32/U盘可写) → PSRAM(SF2 样本+元数据) → BanGTsynth
 *
 * 工作流程:
 *   1. Init: 通过 FAT32 从 SD 查找 SF2（如 drumset.sf2 / 4OPFM.sf2）
 *   2. 整文件写入 PSRAM 样本区 (0–6MB)
 *   3. 元数据索引写入 PSRAM (6–7MB)
 *   4. soundbank_manager / sf2_parser 从 PSRAM 读样本发声
 *   5. USB MSC 仍可把同一张 SD 当 U 盘给 PC 更新 SF2
 */

#include "bg_storage.h"
#include "bg_config.h"
#include "bg_sf2_sd.h"
#include "fat32.h"
#include "psram_heap.h"
#include "flash_devices.h"
#include "flash_bus.h"
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

typedef struct {
    char name[BG_SF2_SD_NAME_MAX];
    uint32_t size;
} Sf2Item_t;

static Sf2Item_t s_catalog[BG_SF2_SD_MAX];
static int s_catalog_count;

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

static int name_is_sf2(const char *name)
{
    int n;
    if (!name) {
        return 0;
    }
    n = (int)strlen(name);
    if (n < 4) {
        return 0;
    }
    return (name[n - 4] == '.' &&
            (name[n - 3] == 'S' || name[n - 3] == 's') &&
            (name[n - 2] == 'F' || name[n - 2] == 'f') &&
            name[n - 1] == '2');
}

static int catalog_cb(const FAT32_FileInfo_t *info, void *user)
{
    (void)user;
    if (!info || (info->attr & DIR_ATTR_DIRECTORY)) {
        return 0;
    }
    if (!name_is_sf2(info->name)) {
        return 0;
    }
    if (s_catalog_count >= BG_SF2_SD_MAX) {
        return 1;
    }
    strncpy(s_catalog[s_catalog_count].name, info->name, BG_SF2_SD_NAME_MAX - 1);
    s_catalog[s_catalog_count].name[BG_SF2_SD_NAME_MAX - 1] = '\0';
    s_catalog[s_catalog_count].size = info->size;
    s_catalog_count++;
    return 0;
}

static BG_ERR ensure_fat32(void)
{
    if (FAT32_IsCardReady()) {
        return SUCCESS;
    }
    return FAT32_Init();
}

int bg_sf2_sd_scan(void)
{
    s_catalog_count = 0;
    memset(s_catalog, 0, sizeof(s_catalog));
    if (ensure_fat32() != SUCCESS) {
        DBG_SYNTH_STORAGE("scan: FAT32 not ready\n");
        return 0;
    }
    (void)FAT32_ListDir("/", catalog_cb, NULL);
    DBG_SYNTH_STORAGE("scan: %d .sf2 file(s)\n", s_catalog_count);
    return s_catalog_count;
}

int bg_sf2_sd_count(void)
{
    return s_catalog_count;
}

const char *bg_sf2_sd_name(int index)
{
    if (index < 0 || index >= s_catalog_count) {
        return NULL;
    }
    return s_catalog[index].name;
}

uint32_t bg_sf2_sd_size(int index)
{
    if (index < 0 || index >= s_catalog_count) {
        return 0;
    }
    return s_catalog[index].size;
}

const char *bg_sf2_sd_current(void)
{
    return s_sf2_filename;
}

int bg_sf2_sd_is_loaded(void)
{
    return s_sf2_loaded ? 1 : 0;
}

static BG_ERR find_sf2_file(char *filename, uint32_t max_len)
{
    if (bg_sf2_sd_scan() <= 0) {
        DBG_SYNTH_STORAGE("No SF2 file found on SD card\n");
        return ENABLE_NOT_FOUND;
    }
    strncpy(filename, s_catalog[0].name, max_len - 1);
    filename[max_len - 1] = '\0';
    DBG_SYNTH_STORAGE("Found: %s\n", filename);
    return SUCCESS;
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
    if (file_size == 0 || file_size > SYNTH_PSRAM_SAMPLE_SIZE) {
        DBG_SYNTH_STORAGE("SF2 size invalid (max %lu)\n", (unsigned long)SYNTH_PSRAM_SAMPLE_SIZE);
        FAT32_CloseFile(&handle);
        return ENABLE_INVALID_INPUT;
    }
    
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

BG_ERR bg_sf2_sd_load_psram(const char *filename)
{
    BG_ERR ret;
    char pick[SYNTH_SF2_FILENAME_MAX];

    if (!s_psram_dev) {
        s_psram_dev = FlashDevices_GetPsramFlash();
    }
    if (!s_psram_dev || !s_psram_dev->initialized) {
        return ENABLE_DEVICE_NOT_READY;
    }
    if (ensure_fat32() != SUCCESS) {
        return ENABLE_DEVICE_NOT_READY;
    }

    if (filename && filename[0]) {
        strncpy(pick, filename, sizeof(pick) - 1);
        pick[sizeof(pick) - 1] = '\0';
    } else {
        ret = find_sf2_file(pick, sizeof(pick));
        if (ret != SUCCESS) {
            return ret;
        }
    }

    ret = load_sf2_to_psram(pick);
    if (ret != SUCCESS) {
        return ret;
    }
    strncpy(s_sf2_filename, pick, sizeof(s_sf2_filename) - 1);
    s_sf2_filename[sizeof(s_sf2_filename) - 1] = '\0';
    s_initialized = true;
    return SUCCESS;
}

/* ============================================
 * BG_Storage 驱动接口实现
 * ============================================ */

static BG_ERR bandatahub_storage_init(const char *path, BG_Storage_Mode_t mode)
{
    BG_ERR ret;

    (void)mode;

    DBG_SYNTH_STORAGE("Initializing BanDataHub SD+PSRAM storage driver...\n");

    s_psram_dev = FlashDevices_GetPsramFlash();
    if (!s_psram_dev || !s_psram_dev->initialized) {
        DBG_SYNTH_STORAGE("PSRAM device not available\n");
        return ENABLE_DEVICE_NOT_READY;
    }

    if (s_initialized && s_sf2_loaded && (!path || !path[0])) {
        return SUCCESS;
    }

    ret = bg_sf2_sd_load_psram(path);
    if (ret != SUCCESS) {
        DBG_SYNTH_STORAGE("No SF2 loaded (err=%d), driver still usable\n", ret);
        s_initialized = true;
        return SUCCESS;
    }

    DBG_SYNTH_STORAGE("BanDataHub storage driver initialized OK\n");
    return SUCCESS;
}

static BG_ERR bandatahub_storage_deinit(void)
{
    /* 不卸载 FAT32，避免打断 USB MSC 与再次选库 */
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

const BG_Storage_Driver_t bg_storage_driver_port = {
    .init     = bandatahub_storage_init,
    .deinit   = bandatahub_storage_deinit,
    .read     = bandatahub_storage_read,
    .write    = bandatahub_storage_write,
    .erase    = bandatahub_storage_erase,
    .sync     = bandatahub_storage_sync,
    .get_info = bandatahub_storage_get_info,
};
