/**
 *****************************************************************************
 * @file     shell_io_manager.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     16-December-2025
 * @brief    Shell IO管理器实现 - 自动切换CDC/BLE接口并提供访问保护
 *****************************************************************************
 */

#include "shell_io_manager.h"
#include "shell_io_cdc.h"
#include "shell_io_ble.h"
#include <string.h>

/*******************************************************************************
 * 外部依赖 - 系统Tick获取
 ******************************************************************************/
/* 使用FreeRTOS的tick */
#include "FreeRTOS.h"
#include "task.h"
#define GET_TICK_MS()   (xTaskGetTickCount() * portTICK_PERIOD_MS)

/*******************************************************************************
 * 全局变量
 ******************************************************************************/
static ShellIOManager_t g_io_manager;

/*******************************************************************************
 * 内部函数声明
 ******************************************************************************/
static uint8_t CheckCDCAvailable(void);
static uint8_t CheckBLEAvailable(void);
static void SwitchToIO(ShellIOType_t io_type);
static uint8_t IsTimeout(uint32_t start_tick, uint32_t timeout_ms);

/*******************************************************************************
 * API实现
 ******************************************************************************/

void ShellIOManager_Init(void)
{
    memset(&g_io_manager, 0, sizeof(g_io_manager));
    g_io_manager.active_io = SHELL_IO_NONE;
    g_io_manager.state = SHELL_IO_STATE_IDLE;
    g_io_manager.last_activity_tick = GET_TICK_MS();
    g_io_manager.lock_tick = 0;
    g_io_manager.cdc_pending = 0;
    g_io_manager.ble_pending = 0;
    
    /* 初始化Shell系统 */
    Shell_Init();
    Shell_RegisterAllModules();
    
    /* 默认使用CDC接口 */
    Shell_SetIO(ShellIO_CDC_Get());
    g_io_manager.active_io = SHELL_IO_CDC;
}

void ShellIOManager_Process(void)
{
    uint8_t cdc_has_data;
    uint8_t ble_has_data;
    uint32_t current_tick;
    
    current_tick = GET_TICK_MS();
    
    /* 检查各接口是否有数据 */
    cdc_has_data = CheckCDCAvailable();
    ble_has_data = CheckBLEAvailable();
    
    /* 更新pending标志 */
    if (cdc_has_data) g_io_manager.cdc_pending = 1;
    if (ble_has_data) g_io_manager.ble_pending = 1;
    
    /* 检查锁定超时（防止死锁） */
    if (g_io_manager.state == SHELL_IO_STATE_LOCKED)
    {
        if (IsTimeout(g_io_manager.lock_tick, SHELL_IO_LOCK_TIMEOUT))
        {
            /* 锁定超时，强制解锁 */
            g_io_manager.state = SHELL_IO_STATE_ACTIVE;
        }
    }
    
    /* 根据状态处理 */
    switch (g_io_manager.state)
    {
        case SHELL_IO_STATE_IDLE:
            /* 空闲状态：检测哪个接口有数据 */
            if (cdc_has_data)
            {
                SwitchToIO(SHELL_IO_CDC);
                g_io_manager.state = SHELL_IO_STATE_ACTIVE;
                g_io_manager.last_activity_tick = current_tick;
            }
            else if (ble_has_data)
            {
                SwitchToIO(SHELL_IO_BLE);
                g_io_manager.state = SHELL_IO_STATE_ACTIVE;
                g_io_manager.last_activity_tick = current_tick;
            }
            break;
            
        case SHELL_IO_STATE_ACTIVE:
            /* 活跃状态：优先处理当前接口，但可以切换 */
            if (g_io_manager.active_io == SHELL_IO_CDC)
            {
                if (cdc_has_data)
                {
                    /* CDC有数据，继续处理 */
                    g_io_manager.last_activity_tick = current_tick;
                }
                else if (ble_has_data && IsTimeout(g_io_manager.last_activity_tick, SHELL_IO_TIMEOUT_MS))
                {
                    /* CDC超时且BLE有数据，切换到BLE */
                    SwitchToIO(SHELL_IO_BLE);
                    g_io_manager.last_activity_tick = current_tick;
                }
                else if (!cdc_has_data && !ble_has_data && 
                         IsTimeout(g_io_manager.last_activity_tick, SHELL_IO_TIMEOUT_MS))
                {
                    /* 两边都没数据且超时，回到空闲 */
                    g_io_manager.state = SHELL_IO_STATE_IDLE;
                }
            }
            else if (g_io_manager.active_io == SHELL_IO_BLE)
            {
                if (ble_has_data)
                {
                    /* BLE有数据，继续处理 */
                    g_io_manager.last_activity_tick = current_tick;
                }
                else if (cdc_has_data && IsTimeout(g_io_manager.last_activity_tick, SHELL_IO_TIMEOUT_MS))
                {
                    /* BLE超时且CDC有数据，切换到CDC */
                    SwitchToIO(SHELL_IO_CDC);
                    g_io_manager.last_activity_tick = current_tick;
                }
                else if (!cdc_has_data && !ble_has_data && 
                         IsTimeout(g_io_manager.last_activity_tick, SHELL_IO_TIMEOUT_MS))
                {
                    /* 两边都没数据且超时，回到空闲 */
                    g_io_manager.state = SHELL_IO_STATE_IDLE;
                }
            }
            break;
            
        case SHELL_IO_STATE_LOCKED:
            /* 锁定状态：只处理当前接口，不切换 */
            /* 更新活动时间防止被强制解锁 */
            if ((g_io_manager.active_io == SHELL_IO_CDC && cdc_has_data) ||
                (g_io_manager.active_io == SHELL_IO_BLE && ble_has_data))
            {
                g_io_manager.lock_tick = current_tick;
            }
            break;
    }
    
    /* 调用Shell处理函数 */
    Shell_Process();
    
    /* 清除已处理的pending标志 */
    if (g_io_manager.active_io == SHELL_IO_CDC && !cdc_has_data)
    {
        g_io_manager.cdc_pending = 0;
    }
    if (g_io_manager.active_io == SHELL_IO_BLE && !ble_has_data)
    {
        g_io_manager.ble_pending = 0;
    }
}

ShellIOType_t ShellIOManager_GetActiveIO(void)
{
    return g_io_manager.active_io;
}

ShellIOState_t ShellIOManager_GetState(void)
{
    return g_io_manager.state;
}

uint8_t ShellIOManager_TryLock(ShellIOType_t io_type)
{
    /* 如果已经被其他接口锁定，返回失败 */
    if (g_io_manager.state == SHELL_IO_STATE_LOCKED && 
        g_io_manager.active_io != io_type)
    {
        return 0;
    }
    
    /* 锁定 */
    g_io_manager.state = SHELL_IO_STATE_LOCKED;
    g_io_manager.active_io = io_type;
    g_io_manager.lock_tick = GET_TICK_MS();
    
    /* 切换到对应接口 */
    SwitchToIO(io_type);
    
    return 1;
}

void ShellIOManager_Unlock(void)
{
    if (g_io_manager.state == SHELL_IO_STATE_LOCKED)
    {
        g_io_manager.state = SHELL_IO_STATE_ACTIVE;
        g_io_manager.last_activity_tick = GET_TICK_MS();
    }
}

uint8_t ShellIOManager_SwitchIO(ShellIOType_t io_type)
{
    /* 锁定状态不允许切换 */
    if (g_io_manager.state == SHELL_IO_STATE_LOCKED)
    {
        return 0;
    }
    
    SwitchToIO(io_type);
    g_io_manager.state = SHELL_IO_STATE_ACTIVE;
    g_io_manager.last_activity_tick = GET_TICK_MS();
    
    return 1;
}

void ShellIOManager_UpdateActivity(ShellIOType_t io_type)
{
    if (g_io_manager.active_io == io_type)
    {
        g_io_manager.last_activity_tick = GET_TICK_MS();
        
        /* 如果是锁定状态，也更新锁定时间 */
        if (g_io_manager.state == SHELL_IO_STATE_LOCKED)
        {
            g_io_manager.lock_tick = GET_TICK_MS();
        }
    }
}

const char* ShellIOManager_GetIOName(ShellIOType_t io_type)
{
    switch (io_type)
    {
        case SHELL_IO_CDC: return "CDC";
        case SHELL_IO_BLE: return "BLE";
        default: return "NONE";
    }
}

/*******************************************************************************
 * 内部函数实现
 ******************************************************************************/

static uint8_t CheckCDCAvailable(void)
{
    const ShellIO_t *cdc_io = ShellIO_CDC_Get();
    if (cdc_io && cdc_io->available)
    {
        return (cdc_io->available() > 0) ? 1 : 0;
    }
    return 0;
}

static uint8_t CheckBLEAvailable(void)
{
    const ShellIO_t *ble_io = ShellIO_BLE_Get();
    if (ble_io && ble_io->available)
    {
        return (ble_io->available() > 0) ? 1 : 0;
    }
    return 0;
}

static void SwitchToIO(ShellIOType_t io_type)
{
    if (g_io_manager.active_io == io_type)
    {
        return;  /* 已经是该接口 */
    }
    
    switch (io_type)
    {
        case SHELL_IO_CDC:
            Shell_SetIO(ShellIO_CDC_Get());
            break;
        case SHELL_IO_BLE:
            Shell_SetIO(ShellIO_BLE_Get());
            break;
        default:
            return;
    }
    
    g_io_manager.active_io = io_type;
}

static uint8_t IsTimeout(uint32_t start_tick, uint32_t timeout_ms)
{
    uint32_t current = GET_TICK_MS();
    uint32_t elapsed;
    
    /* 处理溢出 */
    if (current >= start_tick)
    {
        elapsed = current - start_tick;
    }
    else
    {
        elapsed = (0xFFFFFFFF - start_tick) + current + 1;
    }
    
    return (elapsed >= timeout_ms) ? 1 : 0;
}
