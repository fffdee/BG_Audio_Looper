/**
 * @file bg_osal.h
 * @brief BanGTsynth OS 抽象层 (OS Abstraction Layer)
 *
 * 隔离所有 RTOS/OS 依赖:
 *   - 互斥锁
 *   - 队列/消息
 *   - 任务
 *   - 时间/延时
 *
 * 移植步骤:
 *   1. 在 port/<平台>/bg_osal_port.c 中实现本头文件声明的所有函数
 *   2. 裸机平台可用临界区 + 轮询替代
 *
 * C89 兼容 (NDS32 LE 编译器)
 */

#ifndef BG_OSAL_H__
#define BG_OSAL_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 不透明句柄类型
 * ============================================ */

/** 互斥锁句柄 */
typedef void *bg_mutex_t;

/** 队列句柄 */
typedef void *bg_queue_t;

/** 任务句柄 */
typedef void *bg_task_t;

/** 信号量句柄 */
typedef void *bg_sem_t;

/** 任务函数原型 */
typedef void (*bg_task_func_t)(void *param);

/* ============================================
 * 超时常量
 * ============================================ */

/** 不等待, 立即返回 */
#define BG_OSAL_NO_WAIT       0

/** 永久等待 */
#define BG_OSAL_WAIT_FOREVER  0xFFFFFFFFUL

/* ============================================
 * 返回值
 * ============================================ */
#define BG_OSAL_OK          0
#define BG_OSAL_ERR        (-1)
#define BG_OSAL_TIMEOUT    (-2)

/* ============================================
 * 互斥锁操作
 * ============================================ */

/**
 * 创建互斥锁
 * @return 互斥锁句柄, NULL 表示失败
 */
bg_mutex_t bg_mutex_create(void);

/**
 * 删除互斥锁
 * @param mutex 互斥锁句柄
 */
void bg_mutex_delete(bg_mutex_t mutex);

/**
 * 锁定互斥锁
 * @param mutex   互斥锁句柄
 * @param timeout 超时 (ms), BG_OSAL_WAIT_FOREVER = 永久等待
 * @return BG_OSAL_OK 成功, BG_OSAL_TIMEOUT 超时
 */
int bg_mutex_lock(bg_mutex_t mutex, uint32_t timeout);

/**
 * 解锁互斥锁
 * @param mutex 互斥锁句柄
 * @return BG_OSAL_OK 成功
 */
int bg_mutex_unlock(bg_mutex_t mutex);

/* ============================================
 * 队列操作
 * ============================================ */

/**
 * 创建消息队列
 * @param depth     队列深度 (消息数量)
 * @param item_size 每条消息的字节大小
 * @return 队列句柄, NULL 表示失败
 */
bg_queue_t bg_queue_create(uint32_t depth, uint32_t item_size);

/**
 * 删除消息队列
 * @param queue 队列句柄
 */
void bg_queue_delete(bg_queue_t queue);

/**
 * 发送消息到队列尾部
 * @param queue   队列句柄
 * @param item    消息指针 (会拷贝 item_size 字节)
 * @param timeout 超时 (ms)
 * @return BG_OSAL_OK 成功, BG_OSAL_TIMEOUT 超时/队满
 */
int bg_queue_send(bg_queue_t queue, const void *item, uint32_t timeout);

/**
 * 从队列头部接收消息
 * @param queue   队列句柄
 * @param item    输出缓冲区
 * @param timeout 超时 (ms)
 * @return BG_OSAL_OK 成功, BG_OSAL_TIMEOUT 超时/队空
 */
int bg_queue_recv(bg_queue_t queue, void *item, uint32_t timeout);

/* ============================================
 * 任务操作
 * ============================================ */

/**
 * 创建任务
 * @param func       任务入口函数
 * @param name       任务名称 (调试用)
 * @param stack_size 栈大小 (字节)
 * @param param      传递给任务的参数
 * @param priority   优先级 (0=最低)
 * @param handle     输出: 任务句柄
 * @return BG_OSAL_OK 成功
 */
int bg_task_create(bg_task_func_t func, const char *name,
                   uint32_t stack_size, void *param,
                   uint32_t priority, bg_task_t *handle);

/**
 * 删除任务
 * @param handle 任务句柄, NULL 删除自身
 */
void bg_task_delete(bg_task_t handle);

/**
 * 任务延时
 * @param ms 毫秒
 */
void bg_task_delay(uint32_t ms);

/* ============================================
 * 时间操作
 * ============================================ */

/**
 * 获取系统 tick 计数 (毫秒级精度)
 * @return tick 数 (单位由平台实现定义, 通常 = ms)
 */
uint32_t bg_get_tick_ms(void);

/**
 * 递增系统 tick 计数 (每 1ms 调用一次)
 *
 * 在硬件定时器中断 (Timer2Interrupt) 中调用。
 * 使合成器模块的 bg_get_tick_ms() 独立于 FreeRTOS 调度器。
 */
void bg_tick_increment(void);

/* ============================================
 * 临界区
 * ============================================ */

/**
 * 进入临界区 (禁止中断 / 禁止调度)
 */
void bg_critical_enter(void);

/**
 * 退出临界区
 */
void bg_critical_exit(void);

/**
 * 数据同步屏障 (Data Synchronization Barrier)
 * 确保所有之前的内存写入对其他 CPU/任务可见
 */
void bg_memory_barrier(void);

#ifdef __cplusplus
}
#endif

#endif /* BG_OSAL_H__ */
