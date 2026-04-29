/**
 * @file shell_cmd_psram.c
 * @brief PSRAM 内存管理 Shell 命令模块实现
 *
 * 提供以下命令:
 *   psram                显示 PSRAM 堆内存使用情况
 *
 * 编译条件: HW_CMD_PSRAM_EN (随硬件能力自动启用)
 */

#include "product_def.h"

#if HW_CMD_PSRAM_EN

#include "shell_cmd_psram.h"
#include "bg_shell.h"
#include "psram_heap.h"
#include <string.h>
#include <stdio.h>

/* ============================================
 * 内部函数声明
 * ============================================ */

static int psram_info_cmd(int argc, char *argv[]);

/* ============================================
 * 命令选项定义
 * ============================================ */

static const ShellOpt_t psram_options[] = {
    OPT("", "", "", "Show PSRAM heap memory usage", psram_info_cmd),
    OPT_END()
};

/* ============================================
 * 模块定义
 * ============================================ */

DEFINE_MODULE(psram, "PSRAM heap memory report", MOD_CAT_DEBUG, psram_options);

/* ============================================
 * 公共接口实现
 * ============================================ */

int ShellCmdPsram_Register(void)
{
    return Shell_RegisterModule(&_mod_psram) ? 0 : -1;
}

/* ============================================
 * 命令处理器实现
 * ============================================ */

static int psram_info_cmd(int argc, char *argv[])
{
    const PSRAM_AllocRecord_t *records;
    uint32_t count;
    uint32_t used_kb;
    uint32_t free_kb;
    uint32_t total_kb;
    uint32_t used_pct;
    uint32_t i;

    (void)argc; (void)argv;

    if (!PSRAM_HeapIsInitialized()) {
        BG_ERR init_ret = PSRAM_HeapInit();
        if (init_ret != SUCCESS) {
            Shell_Printf("PSRAM_HeapInit failed: %d\r\n", init_ret);
            return -1;
        }
    }

    used_kb  = PSRAM_HeapGetUsed() / 1024u;
    free_kb  = PSRAM_HeapGetFree() / 1024u;
    total_kb = PSRAM_HEAP_SIZE    / 1024u;
    used_pct = (PSRAM_HeapGetUsed() * 100u) / PSRAM_HEAP_SIZE;

    Shell_Print("PSRAM Heap Memory Report\r\n");
    Shell_Print("--------------------------------------\r\n");
    Shell_Printf("  Base  : 0x%06X\r\n", (unsigned)PSRAM_HEAP_BASE);
    Shell_Printf("  Total : %u KB\r\n",  (unsigned)total_kb);
    Shell_Printf("  Used  : %u KB  (%u%%)\r\n", (unsigned)used_kb, (unsigned)used_pct);
    Shell_Printf("  Free  : %u KB\r\n",  (unsigned)free_kb);

    PSRAM_HeapGetRecords(&records, &count);
    if (count == 0u) {
        Shell_Print("  (no tagged allocations)\r\n");
    } else {
        Shell_Print("--------------------------------------\r\n");
        Shell_Print("  #   Tag             Addr      Size\r\n");
        Shell_Print("--------------------------------------\r\n");
        for (i = 0u; i < count; i++) {
            Shell_Printf("  %-2u  %-15s 0x%06X  %u B\r\n",
                         (unsigned)i,
                         records[i].tag,
                         (unsigned)records[i].addr,
                         (unsigned)records[i].size);
        }
    }
    Shell_Print("--------------------------------------\r\n");
    return 0;
}

#endif /* HW_CMD_PSRAM_EN */