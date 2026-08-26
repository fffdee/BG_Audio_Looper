/**
 * @file bg_mem_stdlib.c
 * @brief BanDataHub：堆分配走 libc malloc（SRAM）
 */
#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_mem.h"
#include <stdlib.h>

void *bg_mem_alloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    return malloc(size);
}

void bg_mem_free(void *ptr)
{
    free(ptr);
}

#endif /* BANGTSYNTH_EN */
