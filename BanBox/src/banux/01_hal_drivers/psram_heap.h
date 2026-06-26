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
 * 注：此文件从 bangtsynth/02_core/fat32/ 移出，作为通用 PSRAM 堆管理接口。
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

/* ============================================
 * PSRAM 堆区域定义
 * ============================================ */

/** PSRAM 堆起始地址 (7MB) */
#define PSRAM_HEAP_BASE     (7u * 1024u * 1024u)

/** PSRAM 堆大小 (1MB) */
#define PSRAM_HEAP_SIZE     (1u * 1024u * 1024u)

/** 无效句柄 */
#define PSRAM_HEAP_NULL     (0xFFFFFFFFu)

/** 最大命名分配记录数 */
#define PSRAM_HEAP_MAX_RECORDS  16

/* ============================================
 * 类型定义
 * ============================================ */

/** PSRAM 地址类型（24-bit 物理地址） */
typedef uint32_t psram_ptr_t;

/** 单条命名分配记录 */
typedef struct {
    char        tag[16];   /**< 分配标签（最多15字符） */
    psram_ptr_t addr;      /**< 起始地址 */
    uint32_t    size;      /**< 请求字节数（未对齐） */
} PSRAM_AllocRecord_t;

/* ============================================
 * 接口函数
 * ============================================ */

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
