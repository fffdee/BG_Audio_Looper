/**
 * @file bg_osal_baremetal.c
 * @brief 移植模板：无 RTOS。队列为单槽轮询；延时为忙等。
 *
 * 把关中断改成你芯片的 PRIMASK / cpsid。
 * 在 1ms 定时器中断里调用 bg_tick_increment()。
 */
#ifndef BG_PORT_TEMPLATE
#define BG_PORT_TEMPLATE 0
#endif
#if BG_PORT_TEMPLATE

#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_osal.h"
#include <string.h>
#include <stdlib.h>

#ifndef BG_OSAL_BARE_QUEUE_MAX
#define BG_OSAL_BARE_QUEUE_MAX  8
#endif

typedef struct {
    uint32_t depth;
    uint32_t item_size;
    uint32_t head;
    uint32_t count;
    uint8_t *storage;
} bare_queue_t;

static volatile uint32_t g_bg_tick_ms;

void bg_tick_increment(void)
{
    g_bg_tick_ms++;
}

uint32_t bg_get_tick_ms(void)
{
    return g_bg_tick_ms;
}

void bg_critical_enter(void)
{
    /* TODO: disable IRQ */
}

void bg_critical_exit(void)
{
    /* TODO: enable IRQ */
}

void bg_memory_barrier(void)
{
    __asm__ volatile("" ::: "memory");
}

bg_mutex_t bg_mutex_create(void)
{
    return (bg_mutex_t)(void *)1;
}

void bg_mutex_delete(bg_mutex_t mutex)
{
    (void)mutex;
}

int bg_mutex_lock(bg_mutex_t mutex, uint32_t timeout)
{
    (void)mutex;
    (void)timeout;
    bg_critical_enter();
    return BG_OSAL_OK;
}

int bg_mutex_unlock(bg_mutex_t mutex)
{
    (void)mutex;
    bg_critical_exit();
    return BG_OSAL_OK;
}

bg_queue_t bg_queue_create(uint32_t depth, uint32_t item_size)
{
    bare_queue_t *q;
    if (depth == 0 || item_size == 0) {
        return NULL;
    }
    if (depth > BG_OSAL_BARE_QUEUE_MAX) {
        depth = BG_OSAL_BARE_QUEUE_MAX;
    }
    q = (bare_queue_t *)malloc(sizeof(*q));
    if (!q) {
        return NULL;
    }
    q->storage = (uint8_t *)malloc(depth * item_size);
    if (!q->storage) {
        free(q);
        return NULL;
    }
    q->depth = depth;
    q->item_size = item_size;
    q->head = 0;
    q->count = 0;
    return (bg_queue_t)q;
}

void bg_queue_delete(bg_queue_t queue)
{
    bare_queue_t *q = (bare_queue_t *)queue;
    if (!q) {
        return;
    }
    free(q->storage);
    free(q);
}

int bg_queue_send(bg_queue_t queue, const void *item, uint32_t timeout)
{
    bare_queue_t *q = (bare_queue_t *)queue;
    uint32_t tail;
    (void)timeout;
    if (!q || !item) {
        return BG_OSAL_ERR;
    }
    bg_critical_enter();
    if (q->count >= q->depth) {
        bg_critical_exit();
        return BG_OSAL_TIMEOUT;
    }
    tail = (q->head + q->count) % q->depth;
    memcpy(q->storage + tail * q->item_size, item, q->item_size);
    q->count++;
    bg_critical_exit();
    return BG_OSAL_OK;
}

int bg_queue_recv(bg_queue_t queue, void *item, uint32_t timeout)
{
    bare_queue_t *q = (bare_queue_t *)queue;
    (void)timeout;
    if (!q || !item) {
        return BG_OSAL_ERR;
    }
    bg_critical_enter();
    if (q->count == 0) {
        bg_critical_exit();
        return BG_OSAL_TIMEOUT;
    }
    memcpy(item, q->storage + q->head * q->item_size, q->item_size);
    q->head = (q->head + 1) % q->depth;
    q->count--;
    bg_critical_exit();
    return BG_OSAL_OK;
}

int bg_task_create(bg_task_func_t func, const char *name,
                   uint32_t stack_size, void *param,
                   uint32_t priority, bg_task_t *handle)
{
    (void)func;
    (void)name;
    (void)stack_size;
    (void)param;
    (void)priority;
    if (handle) {
        *handle = NULL;
    }
    return BG_OSAL_ERR; /* 裸机请在主循环驱动，不要创建任务 */
}

void bg_task_delete(bg_task_t handle)
{
    (void)handle;
}

void bg_task_delay(uint32_t ms)
{
    uint32_t start = bg_get_tick_ms();
    while ((uint32_t)(bg_get_tick_ms() - start) < ms) {
        /* busy wait; 需 bg_tick_increment */
    }
}

#endif /* BANGTSYNTH_EN */
#endif /* BG_PORT_TEMPLATE */
