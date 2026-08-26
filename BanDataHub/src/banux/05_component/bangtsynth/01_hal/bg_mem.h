/**
 * @file bg_mem.h
 * @brief 合成器堆分配抽象。平台可映射到 malloc、静态 arena 或 PSRAM 堆。
 *
 * 音频回调/ISR 内不要调用 bg_mem_alloc。
 */
#ifndef BG_MEM_H
#define BG_MEM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *bg_mem_alloc(size_t size);
void  bg_mem_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* BG_MEM_H */
