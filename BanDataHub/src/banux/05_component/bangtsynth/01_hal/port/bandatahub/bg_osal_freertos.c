/**
 * @file bg_osal_freertos.c
 * @brief bg_osal — FreeRTOS 平台实现 (BP10 / 通用 FreeRTOS)
 *
 * C89 兼容
 */

#include "product_def.h"

#if BANGTSYNTH_EN

#include "bg_config.h"

/* 仅 BanDataHub 使用本端口；BanBox/BP10 使用 port/bp10 */
#if defined(BANDATAHUB)

#include "bg_osal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/* ============================================
 * 超时转换辅助
 * ============================================ */

static TickType_t ms_to_ticks(uint32_t ms)
{
    if (ms == BG_OSAL_WAIT_FOREVER) {
        return portMAX_DELAY;
    }
    if (ms == 0) {
        return 0;
    }
    return (TickType_t)(ms / portTICK_PERIOD_MS);
}

/* ============================================
 * 互斥锁
 * ============================================ */

bg_mutex_t bg_mutex_create(void)
{
    return (bg_mutex_t)xSemaphoreCreateMutex();
}

void bg_mutex_delete(bg_mutex_t mutex)
{
    if (mutex) {
        vSemaphoreDelete((SemaphoreHandle_t)mutex);
    }
}

int bg_mutex_lock(bg_mutex_t mutex, uint32_t timeout)
{
    if (!mutex) {
        return BG_OSAL_ERR;
    }
    if (xSemaphoreTake((SemaphoreHandle_t)mutex, ms_to_ticks(timeout)) == pdTRUE) {
        return BG_OSAL_OK;
    }
    return BG_OSAL_TIMEOUT;
}

int bg_mutex_unlock(bg_mutex_t mutex)
{
    if (!mutex) {
        return BG_OSAL_ERR;
    }
    xSemaphoreGive((SemaphoreHandle_t)mutex);
    return BG_OSAL_OK;
}

/* ============================================
 * 队列
 * ============================================ */

bg_queue_t bg_queue_create(uint32_t depth, uint32_t item_size)
{
    return (bg_queue_t)xQueueCreate((UBaseType_t)depth, (UBaseType_t)item_size);
}

void bg_queue_delete(bg_queue_t queue)
{
    if (queue) {
        vQueueDelete((QueueHandle_t)queue);
    }
}

int bg_queue_send(bg_queue_t queue, const void *item, uint32_t timeout)
{
    if (!queue || !item) {
        return BG_OSAL_ERR;
    }
    if (xQueueSend((QueueHandle_t)queue, item, ms_to_ticks(timeout)) == pdTRUE) {
        return BG_OSAL_OK;
    }
    return BG_OSAL_TIMEOUT;
}

int bg_queue_recv(bg_queue_t queue, void *item, uint32_t timeout)
{
    if (!queue || !item) {
        return BG_OSAL_ERR;
    }
    if (xQueueReceive((QueueHandle_t)queue, item, ms_to_ticks(timeout)) == pdTRUE) {
        return BG_OSAL_OK;
    }
    return BG_OSAL_TIMEOUT;
}

/* ============================================
 * 任务
 * ============================================ */

int bg_task_create(bg_task_func_t func, const char *name,
                   uint32_t stack_size, void *param,
                   uint32_t priority, bg_task_t *handle)
{
    BaseType_t ret;
    /* FreeRTOS stack size 以 word 为单位, bg_osal 以字节为单位 */
    UBaseType_t stack_words = (UBaseType_t)(stack_size / sizeof(StackType_t));
    if (stack_words < configMINIMAL_STACK_SIZE) {
        stack_words = configMINIMAL_STACK_SIZE;
    }

    ret = xTaskCreate((TaskFunction_t)func, name,
                      stack_words, param,
                      (UBaseType_t)priority,
                      (TaskHandle_t *)handle);
    return (ret == pdPASS) ? BG_OSAL_OK : BG_OSAL_ERR;
}

void bg_task_delete(bg_task_t handle)
{
    vTaskDelete((TaskHandle_t)handle);
}

void bg_task_delay(uint32_t ms)
{
    vTaskDelay(ms_to_ticks(ms));
}

/* ============================================
 * 时间 — 独立 tick 计数器，不依赖 FreeRTOS 调度器
 * 由硬件 Timer2Interrupt (1ms) 调用 bg_tick_increment() 驱动
 * ============================================ */

static volatile uint32_t g_bg_tick_ms = 0;

void bg_tick_increment(void)
{
    g_bg_tick_ms++;
}

uint32_t bg_get_tick_ms(void)
{
    return g_bg_tick_ms;
}

/* ============================================
 * 临界区
 * ============================================ */

void bg_critical_enter(void)
{
    taskENTER_CRITICAL();
}

void bg_critical_exit(void)
{
    taskEXIT_CRITICAL();
}

/* ============================================
 * 内存屏障 (NDS32 DSB)
 * ============================================ */

void bg_memory_barrier(void)
{
    __asm__ volatile("dsb" ::: "memory");
}

#endif /* BG_PLATFORM_BP10 */
#endif /* BANGTSYNTH_EN */
