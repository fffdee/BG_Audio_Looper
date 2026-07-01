/**
 * @file psram_heap.h
 * @brief PSRAM 通用堆内存管理器
 *
 * 在 PSRAM 的顶部 1MB 区域（0x700000-0x7FFFFF）实现线性（bump）分配器。
 * 提供分配、读写、填充接口，供 FAT32 等模块使用大缓冲区而不占用 SRAM。
 *
 * 地址布局：
 *   0x000000 - 0x5FFFFF : 音频音符缓冲池 (psram_buffer 管理)
 *   0x600000 - 0x6FFFFF : 保留 / psram_buffer 管理元数据
 *   0x700000 - 0x7FFFFF : PSRAM 通用堆 (本模块管理，1MB)
 *
 * 注：此文件的副本也存在于 01_hal_drivers/psram_heap.h，
 * 供非 bangtsynth 模块（如 shell_cmd_psram.c）使用。
 * 两份文件保持同步，由 #ifndef 保护宏防止重复声明。
 */

#ifndef __PSRAM_HEAP_H__
#define __PSRAM_HEAP_H__

#include "product_def.h"

#if HW_DRV_PSRAM_EN

#include <stdint.h>
#include <stdbool.h>
#include "err_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PSRAM_HEAP_BASE     (7u * 1024u * 1024u)
#define PSRAM_HEAP_SIZE     (1u * 1024u * 1024u)
#define PSRAM_HEAP_NULL     (0xFFFFFFFFu)
#define PSRAM_HEAP_MAX_RECORDS  16

typedef uint32_t psram_ptr_t;

typedef struct {
    char        tag[16];
    psram_ptr_t addr;
    uint32_t    size;
} PSRAM_AllocRecord_t;

BG_ERR PSRAM_HeapInit(void);
void PSRAM_HeapReset(void);
psram_ptr_t PSRAM_HeapAlloc(uint32_t size);
psram_ptr_t PSRAM_HeapAllocTagged(uint32_t size, const char *tag);
void PSRAM_HeapFree(psram_ptr_t ptr, uint32_t size);
BG_ERR PSRAM_HeapRead(psram_ptr_t addr, void *buf, uint32_t len);
BG_ERR PSRAM_HeapWrite(psram_ptr_t addr, const void *buf, uint32_t len);
BG_ERR PSRAM_HeapMemset(psram_ptr_t addr, uint8_t val, uint32_t len);
uint32_t PSRAM_HeapGetFree(void);
uint32_t PSRAM_HeapGetUsed(void);
bool PSRAM_HeapIsInitialized(void);
void PSRAM_HeapGetRecords(const PSRAM_AllocRecord_t **records, uint32_t *count);

#ifdef __cplusplus
}
#endif

#endif /* HW_DRV_PSRAM_EN */

#endif /* __PSRAM_HEAP_H__ */
