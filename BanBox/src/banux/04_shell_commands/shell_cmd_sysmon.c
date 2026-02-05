/**
 *****************************************************************************
 * @file     shell_cmd_sysmon.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     06-January-2026
 * @brief    System Monitor Shell Commands - CPU, Memory, Task Statistics
 *****************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bg_shell.h"
#include "FreeRTOS.h"
#include "task.h"

/*******************************************************************************
 * Static command function declarations
 ******************************************************************************/
static int Opt_MemInfo(int argc, char *argv[]);
static int Opt_CpuStats(int argc, char *argv[]);
static int Opt_TaskInfo(int argc, char *argv[]);
static int Opt_SysInfo(int argc, char *argv[]);
static int Opt_QueryJSON(int argc, char *argv[]);

/*******************************************************************************
 * Static buffers for formatting output (to avoid dynamic allocation)
 ******************************************************************************/
static char g_sysmon_buffer[1024];  /* Shared buffer for all sysmon commands */

/*******************************************************************************
 * Module options definition
 ******************************************************************************/
static const ShellOpt_t g_SysmonOpts[] = {
    OPT("m", "memory",  NULL, "Show memory usage",          Opt_MemInfo),
    OPT("c", "cpu",     NULL, "Show CPU usage statistics",  Opt_CpuStats),
    OPT("t", "tasks",   NULL, "Show task information",      Opt_TaskInfo),
    OPT("s", "sysinfo", NULL, "Show system information",    Opt_SysInfo),
    OPT("q", "query",   NULL, "Query system info (JSON)",   Opt_QueryJSON),
    OPT_END()
};

/*******************************************************************************
 * Module definition
 ******************************************************************************/
static const ShellModule_t g_SysmonModule = {
    "sysmon",
    "System Monitor - CPU/Memory/Task statistics",
    MOD_CAT_DEBUG,
    g_SysmonOpts,
    5
};

/*******************************************************************************
 * Public API
 ******************************************************************************/

/**
 * @brief Register system monitor commands to shell
 */
void ShellCmdSysmon_Register(void)
{
    Shell_RegisterModule(&g_SysmonModule);
}

/*******************************************************************************
 * Command implementations
 ******************************************************************************/

/**
 * @brief Show memory usage
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success
 */
static int Opt_MemInfo(int argc, char *argv[])
{
    size_t free_heap;
    size_t min_ever_free;
    
    (void)argc;
    (void)argv;
    
    free_heap = xPortGetFreeHeapSize();
    min_ever_free = xPortGetMinimumEverFreeHeapSize();
    
    Shell_Printf("\n=== Memory Usage ===\n");
    Shell_Printf("Current Free:      %u bytes\n", (unsigned int)free_heap);
    Shell_Printf("Minimum Ever Free: %u bytes\n", (unsigned int)min_ever_free);
    Shell_Printf("Total Heap Size:   %u bytes\n", (unsigned int)configTOTAL_HEAP_SIZE);
    Shell_Printf("Used:              %u bytes (%.1f%%)\n", 
                 (unsigned int)(configTOTAL_HEAP_SIZE - free_heap),
                 (float)(configTOTAL_HEAP_SIZE - free_heap) * 100.0f / configTOTAL_HEAP_SIZE);
    Shell_Printf("Peak Used:         %u bytes (%.1f%%)\n", 
                 (unsigned int)(configTOTAL_HEAP_SIZE - min_ever_free),
                 (float)(configTOTAL_HEAP_SIZE - min_ever_free) * 100.0f / configTOTAL_HEAP_SIZE);
    Shell_Printf("\n");
    
    return 0;
}

/**
 * @brief Show CPU usage statistics
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success
 */
static int Opt_CpuStats(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    #if (configGENERATE_RUN_TIME_STATS == 1)
    
    /* Use static buffer instead of dynamic allocation */
    memset(g_sysmon_buffer, 0, sizeof(g_sysmon_buffer));
    vTaskGetRunTimeStats(g_sysmon_buffer);
    
    Shell_Printf("\n=== CPU Usage Statistics ===\n");
    Shell_Printf("Task            \tAbs Time\tPercent\n");
    Shell_Printf("-----------------------------------------------\n");
    Shell_Printf("%s\n", g_sysmon_buffer);
    
    #else
    
    Shell_Printf("Error: configGENERATE_RUN_TIME_STATS not enabled\n");
    Shell_Printf("Please define configGENERATE_RUN_TIME_STATS=1 in FreeRTOSConfig.h\n");
    
    #endif
    
    return 0;
}

/**
 * @brief Show task information
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success
 */
static int Opt_TaskInfo(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    #if (configUSE_TRACE_FACILITY == 1)
    
    /* Use static buffer instead of dynamic allocation */
    memset(g_sysmon_buffer, 0, sizeof(g_sysmon_buffer));
    vTaskList(g_sysmon_buffer);
    
    Shell_Printf("\n=== Task Information ===\n");
    Shell_Printf("Task            \tState\tPrio\tStack\tNum\n");
    Shell_Printf("-----------------------------------------------\n");
    Shell_Printf("%s\n", g_sysmon_buffer);
    Shell_Printf("\nState: X=Running, R=Ready, B=Blocked, S=Suspended, D=Deleted\n");
    Shell_Printf("Stack: Free stack space in words (4 bytes each)\n");
    Shell_Printf("\n");
    
    #else
    
    Shell_Printf("Error: configUSE_TRACE_FACILITY not enabled\n");
    Shell_Printf("Please define configUSE_TRACE_FACILITY=1 in FreeRTOSConfig.h\n");
    
    #endif
    
    return 0;
}

/**
 * @brief Show system information
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success
 */
static int Opt_SysInfo(int argc, char *argv[])
{
    size_t free_heap;
    
    (void)argc;
    (void)argv;
    
    free_heap = xPortGetFreeHeapSize();
    
    Shell_Printf("\n=== System Information ===\n");
    Shell_Printf("RTOS:              FreeRTOS\n");
    Shell_Printf("Version:           %s\n", tskKERNEL_VERSION_NUMBER);
    Shell_Printf("Tick Rate:         %u Hz\n", (unsigned int)configTICK_RATE_HZ);
    Shell_Printf("CPU Clock:         %u MHz\n", (unsigned int)(configCPU_CLOCK_HZ / 1000000));
    Shell_Printf("Max Priority:      %u\n", (unsigned int)configMAX_PRIORITIES);
    Shell_Printf("Minimal Stack:     %u words\n", (unsigned int)configMINIMAL_STACK_SIZE);
    Shell_Printf("\n");
    
    Shell_Printf("=== Memory Status ===\n");
    Shell_Printf("Heap Total:        %u bytes\n", (unsigned int)configTOTAL_HEAP_SIZE);
    Shell_Printf("Heap Free:         %u bytes (%.1f%%)\n", 
                 (unsigned int)free_heap,
                 (float)free_heap * 100.0f / configTOTAL_HEAP_SIZE);
    Shell_Printf("\n");
    
    Shell_Printf("=== Features ===\n");
    #if (configUSE_PREEMPTION == 1)
    Shell_Printf("Preemption:        Enabled\n");
    #else
    Shell_Printf("Preemption:        Disabled\n");
    #endif
    
    #if (configUSE_IDLE_HOOK == 1)
    Shell_Printf("Idle Hook:         Enabled\n");
    #else
    Shell_Printf("Idle Hook:         Disabled\n");
    #endif
    
    #if (configUSE_TICK_HOOK == 1)
    Shell_Printf("Tick Hook:         Enabled\n");
    #else
    Shell_Printf("Tick Hook:         Disabled\n");
    #endif
    
    #if (configGENERATE_RUN_TIME_STATS == 1)
    Shell_Printf("Runtime Stats:     Enabled\n");
    #else
    Shell_Printf("Runtime Stats:     Disabled\n");
    #endif
    
    #if (configUSE_TRACE_FACILITY == 1)
    Shell_Printf("Trace Facility:    Enabled\n");
    #else
    Shell_Printf("Trace Facility:    Disabled\n");
    #endif
    
    Shell_Printf("\n");
    
    return 0;
}

/**
 * @brief Query system information in JSON format
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success
 */
static int Opt_QueryJSON(int argc, char *argv[])
{
    size_t free_heap;
    size_t min_ever_free;
    
    (void)argc;
    (void)argv;
    
    free_heap = xPortGetFreeHeapSize();
    min_ever_free = xPortGetMinimumEverFreeHeapSize();
    
    Shell_Printf("{\"status\":\"ok\",\"system\":{");
    Shell_Printf("\"memory\":{");
    Shell_Printf("\"free\":%u,", (unsigned int)free_heap);
    Shell_Printf("\"min_free\":%u,", (unsigned int)min_ever_free);
    Shell_Printf("\"total\":%u,", (unsigned int)configTOTAL_HEAP_SIZE);
    Shell_Printf("\"used\":%u", (unsigned int)(configTOTAL_HEAP_SIZE - free_heap));
    Shell_Printf("},");
    Shell_Printf("\"tasks\":{");
    Shell_Printf("\"count\":%lu,", (unsigned long)uxTaskGetNumberOfTasks());
    Shell_Printf("\"tick_rate\":%lu", (unsigned long)configTICK_RATE_HZ);
    Shell_Printf("}");
    Shell_Printf("}}\n");
    
    return 0;
}
