/**
 * @file bg_mem_arena.c
 * @brief 移植模板：静态 arena。free 为 no-op（只适合启动期分配、关机一起释放）。
 *
 * 需要真正回收时改成 tlsf/heap_4，或继续用 malloc 版 bg_mem_stdlib.c。
 */
#ifndef BG_PORT_TEMPLATE
#define BG_PORT_TEMPLATE 0
#endif
#if BG_PORT_TEMPLATE

#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_mem.h"
#include <stdint.h>
#include <string.h>

#ifndef BG_MEM_ARENA_BYTES
#define BG_MEM_ARENA_BYTES  (64u * 1024u)
#endif

static uint8_t  s_arena[BG_MEM_ARENA_BYTES];
static uint32_t s_used;

void *bg_mem_alloc(size_t size)
{
    uint32_t aligned;
    uint8_t *p;

    if (size == 0) {
        return NULL;
    }
    aligned = (uint32_t)((size + 7u) & ~7u);
    if (s_used + aligned > BG_MEM_ARENA_BYTES) {
        return NULL;
    }
    p = s_arena + s_used;
    s_used += aligned;
    memset(p, 0, aligned);
    return p;
}

void bg_mem_free(void *ptr)
{
    (void)ptr;
    /* arena 不单块回收 */
}

#endif /* BANGTSYNTH_EN */
#endif /* BG_PORT_TEMPLATE */
