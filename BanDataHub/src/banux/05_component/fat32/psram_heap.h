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
 */

#ifndef __PSRAM_HEAP_H__
#define __PSRAM_HEAP_H__

#include "product_def.h"

#if HW_DRV_PSRAM_EN  /* 改为硬件能力宏，让 PSRAM 堆管理跟硬件而非功能方案绑定 */

#include <stdint.h>
#include <stdbool.h>

/* BG_ERR 错误码定义（原 err_handle.h 属于 bangtsynth，已移除，此处内联） */
#ifndef BG_ERR_DEFINED
#define BG_ERR_DEFINED
typedef enum {
    SUCCESS = 0,
    ENABLE_INVALID_INPUT,
    ENABLE_OUT_OF_MEMORY,
    ENABLE_NOT_FOUND,
    ENABLE_IO_ERROR,
} BG_ERR;
#endif

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

/**
 * @brief 初始化 PSRAM 堆管理器
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_HeapInit(void);

/**
 * @brief 重置堆分配游标（释放所有已分配块）
 */
void PSRAM_HeapReset(void);

/**
 * @brief 从 PSRAM 堆分配内存
 * @param size 请求字节数
 * @return PSRAM 地址，失败返回 PSRAM_HEAP_NULL
 */
psram_ptr_t PSRAM_HeapAlloc(uint32_t size);

/**
 * @brief 带标签分配——同时记录到内存表（最多 PSRAM_HEAP_MAX_RECORDS 条）
 * @param size 请求字节数
 * @param tag  标签字符串（最多 15 字符，自动截断）
 * @return PSRAM 地址，失败返回 PSRAM_HEAP_NULL
 */
psram_ptr_t PSRAM_HeapAllocTagged(uint32_t size, const char *tag);

/**
 * @brief 释放 PSRAM 堆内存（仅线性分配器标记，不合并）
 * @note 若释放的是最近一次分配，则回收空间；否则为空操作
 * @param ptr 之前由 PSRAM_HeapAlloc 返回的地址
 * @param size 分配时的大小
 */
void PSRAM_HeapFree(psram_ptr_t ptr, uint32_t size);

/**
 * @brief 从 PSRAM 读取数据到 SRAM 缓冲区
 * @param addr  PSRAM 地址
 * @param buf   目标 SRAM 缓冲区
 * @param len   读取字节数
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_HeapRead(psram_ptr_t addr, void *buf, uint32_t len);

/**
 * @brief 将 SRAM 缓冲区数据写入 PSRAM
 * @param addr  PSRAM 目标地址
 * @param buf   源 SRAM 缓冲区
 * @param len   写入字节数
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_HeapWrite(psram_ptr_t addr, const void *buf, uint32_t len);

/**
 * @brief 将 PSRAM 区域填充为指定值
 * @param addr  PSRAM 起始地址
 * @param val   填充值
 * @param len   填充字节数
 * @return SUCCESS 或错误码
 */
BG_ERR PSRAM_HeapMemset(psram_ptr_t addr, uint8_t val, uint32_t len);

/**
 * @brief 获取当前剩余可用字节数
 * @return 剩余字节数
 */
uint32_t PSRAM_HeapGetFree(void);

/**
 * @brief 获取当前已用字节数
 * @return 已用字节数
 */
uint32_t PSRAM_HeapGetUsed(void);

/**
 * @brief 检查堆管理器是否已初始化
 * @return true = 已初始化
 */
bool PSRAM_HeapIsInitialized(void);

/**
 * @brief 获取所有命名分配记录
 * @param records 输出：指向内部记录数组的指针（只读）
 * @param count   输出：有效记录数
 */
void PSRAM_HeapGetRecords(const PSRAM_AllocRecord_t **records, uint32_t *count);

#ifdef __cplusplus
}
#endif

#endif /* HW_DRV_PSRAM_EN */

#endif /* __PSRAM_HEAP_H__ */