#include "bangtsynth_legacy.h"
#if BANGTSYNTH_LEGACY

/**
 * @file psram_heap.c
 * @brief PSRAM 通用堆内存管理器实现
 *
 * 线性（bump）分配器，管理 PSRAM 顶部 1MB 区域。
 * 通过 FlashDevice SPI 驱动进行读写访问。
 *
 * 注：头文件已迁移至 01_hal_drivers/psram_heap.h，
 * 此 .c 文件保留在 bangtsynth 目录仅为兼容自动生成的 Makefile 编译列表。
 */

#include "product_def.h"

#if HW_DRV_PSRAM_EN && !defined(BANDATAHUB)

#include "psram_heap.h"
#include "flash_devices.h"
#include <string.h>

/* ============================================
 * 内部常量
 * ============================================ */

#define PSRAM_HEAP_ALIGN    4
#define PSRAM_MEMSET_CHUNK  64

/* ============================================
 * 内部状态
 * ============================================ */

typedef struct {
    uint32_t cursor;
    uint32_t last_alloc;
    uint32_t last_size;
    bool     initialized;
} PSRAM_HeapState_t;

static PSRAM_HeapState_t g_heap;

static PSRAM_AllocRecord_t g_records[PSRAM_HEAP_MAX_RECORDS];
static uint32_t g_record_count = 0u;

static FlashDevice_t *psram_get_dev(void)
{
    return FlashDevices_GetPsramFlash();
}

BG_ERR PSRAM_HeapInit(void)
{
    FlashDevice_t *dev = psram_get_dev();
    if (!dev) {
        return ENABLE_NOT_FOUND;
    }
    g_heap.cursor      = PSRAM_HEAP_BASE;
    g_heap.last_alloc  = PSRAM_HEAP_NULL;
    g_heap.last_size   = 0;
    g_heap.initialized = true;
    g_record_count     = 0;
    return SUCCESS;
}

void PSRAM_HeapReset(void)
{
    g_heap.cursor     = PSRAM_HEAP_BASE;
    g_heap.last_alloc = PSRAM_HEAP_NULL;
    g_heap.last_size  = 0;
    g_record_count    = 0;
}

psram_ptr_t PSRAM_HeapAlloc(uint32_t size)
{
    psram_ptr_t addr;
    uint32_t aligned_size;
    if (!g_heap.initialized || size == 0) {
        return PSRAM_HEAP_NULL;
    }
    aligned_size = (size + PSRAM_HEAP_ALIGN - 1u) & ~((uint32_t)(PSRAM_HEAP_ALIGN - 1u));
    if (g_heap.cursor + aligned_size > PSRAM_HEAP_BASE + PSRAM_HEAP_SIZE) {
        return PSRAM_HEAP_NULL;
    }
    addr = g_heap.cursor;
    g_heap.cursor    += aligned_size;
    g_heap.last_alloc = addr;
    g_heap.last_size  = aligned_size;
    return addr;
}

void PSRAM_HeapFree(psram_ptr_t ptr, uint32_t size)
{
    uint32_t aligned_size;
    if (!g_heap.initialized || ptr == PSRAM_HEAP_NULL) {
        return;
    }
    aligned_size = (size + PSRAM_HEAP_ALIGN - 1u) & ~((uint32_t)(PSRAM_HEAP_ALIGN - 1u));
    if (ptr == g_heap.last_alloc && aligned_size == g_heap.last_size) {
        g_heap.cursor    -= aligned_size;
        g_heap.last_alloc = PSRAM_HEAP_NULL;
        g_heap.last_size  = 0;
    }
}

BG_ERR PSRAM_HeapRead(psram_ptr_t addr, void *buf, uint32_t len)
{
    FlashDevice_t *dev = psram_get_dev();
    FlashStatus_t st;
    if (!g_heap.initialized || !dev || !buf || len == 0) {
        return ENABLE_INVALID_INPUT;
    }
    if (addr < PSRAM_HEAP_BASE || addr + len > PSRAM_HEAP_BASE + PSRAM_HEAP_SIZE) {
        return ENABLE_INVALID_INPUT;
    }
    st = dev->ops->read(dev, addr, (uint8_t *)buf, len);
    return (st == FLASH_OK) ? SUCCESS : ENABLE_IO_ERROR;
}

BG_ERR PSRAM_HeapWrite(psram_ptr_t addr, const void *buf, uint32_t len)
{
    FlashDevice_t *dev = psram_get_dev();
    FlashStatus_t st;
    if (!g_heap.initialized || !dev || !buf || len == 0) {
        return ENABLE_INVALID_INPUT;
    }
    if (addr < PSRAM_HEAP_BASE || addr + len > PSRAM_HEAP_BASE + PSRAM_HEAP_SIZE) {
        return ENABLE_INVALID_INPUT;
    }
    st = dev->ops->write(dev, addr, (const uint8_t *)buf, len);
    return (st == FLASH_OK) ? SUCCESS : ENABLE_IO_ERROR;
}

BG_ERR PSRAM_HeapMemset(psram_ptr_t addr, uint8_t val, uint32_t len)
{
    FlashDevice_t *dev = psram_get_dev();
    uint8_t tmp[PSRAM_MEMSET_CHUNK];
    uint32_t written = 0;
    uint32_t chunk;
    FlashStatus_t st;
    if (!g_heap.initialized || !dev || len == 0) {
        return ENABLE_INVALID_INPUT;
    }
    if (addr < PSRAM_HEAP_BASE || addr + len > PSRAM_HEAP_BASE + PSRAM_HEAP_SIZE) {
        return ENABLE_INVALID_INPUT;
    }
    memset(tmp, val, sizeof(tmp));
    while (written < len) {
        chunk = len - written;
        if (chunk > (uint32_t)sizeof(tmp)) {
            chunk = (uint32_t)sizeof(tmp);
        }
        st = dev->ops->write(dev, addr + written, tmp, chunk);
        if (st != FLASH_OK) {
            return ENABLE_IO_ERROR;
        }
        written += chunk;
    }
    return SUCCESS;
}

uint32_t PSRAM_HeapGetFree(void)
{
    if (!g_heap.initialized) return 0u;
    return (PSRAM_HEAP_BASE + PSRAM_HEAP_SIZE) - g_heap.cursor;
}

bool PSRAM_HeapIsInitialized(void)
{
    return g_heap.initialized;
}

uint32_t PSRAM_HeapGetUsed(void)
{
    if (!g_heap.initialized) return 0u;
    return g_heap.cursor - PSRAM_HEAP_BASE;
}

psram_ptr_t PSRAM_HeapAllocTagged(uint32_t size, const char *tag)
{
    psram_ptr_t addr = PSRAM_HeapAlloc(size);
    if (addr != PSRAM_HEAP_NULL && tag != NULL &&
        g_record_count < PSRAM_HEAP_MAX_RECORDS) {
        PSRAM_AllocRecord_t *rec = &g_records[g_record_count];
        uint32_t i;
        for (i = 0u; i < (uint32_t)(sizeof(rec->tag) - 1u) && tag[i] != '\0'; i++) {
            rec->tag[i] = tag[i];
        }
        rec->tag[i] = '\0';
        rec->addr   = addr;
        rec->size   = size;
        g_record_count++;
    }
    return addr;
}

void PSRAM_HeapGetRecords(const PSRAM_AllocRecord_t **records, uint32_t *count)
{
    if (records != NULL) *records = g_records;
    if (count != NULL) *count = g_record_count;
}

#endif /* HW_DRV_PSRAM_EN */

#endif /* BANGTSYNTH_LEGACY */
