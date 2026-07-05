/**
 *****************************************************************************
 * @file     shell_cmd_flash.c
 * @brief    Flash / PSRAM / SD Card 测试 Shell 命令（直接阻塞执行，无后台任务）
 *
 * nandtest           — 完整测试: BBM扫描 -> 功能验证 -> 速度测试(4块)
 * nandtest -b        — 仅坏块扫描
 * nandtest -p [N]    — 仅速度测试 N 块 (默认4块)
 *
 * psramtest          — 完整测试: 功能验证 -> 速度测试(512KB)
 * psramtest -p [KB]  — 仅速度测试 N KB (默认512KB)
 *
 * sdtest             — 完整测试: 功能验证 -> 速度测试(1024块)
 * sdtest -p [N]      — 仅速度测试 N 块 (默认1024块)
 *****************************************************************************
 */

#include "shell_cmd_flash.h"
#include "bg_shell.h"
#include "product_def.h"

#if FLASH_TEST_EN

#include "flash_test.h"
#include "looper_storage.h"  /* 存储抽象层 */
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * 前置声明
 *===========================================================================*/

/*===========================================================================
 * NAND Flash 命令
 *===========================================================================*/

/* nandtest (无选项) — 完整测试 */
static int Nand_Opt_Full(int argc, char *argv[])
{
    NandTestResult_t result;
    (void)argc; (void)argv;

    Shell_Print("[nandtest] === Full NAND test (BBM + Functional + Speed) ===\r\n");
    NandFlash_BBMTest();
    NandFlash_Test();
    memset(&result, 0, sizeof(result));
    NandFlash_SpeedTest(4u, &result);

    /* 诊断信息 */
    Shell_Printf("[nandtest] DEBUG: sizeof(NandTestResult_t)=%u, bad_block_count=%u\r\n",
                 (unsigned int)sizeof(NandTestResult_t), result.bad_block_count);

    if (result.test_passed) {
        Shell_Printf("[nandtest] Write: %lu.%lu KB/s  Read: %lu.%lu KB/s  Bad: %u\r\n",
                     (unsigned long)result.seq_write_speed_kbs,
                     (unsigned long)((result.seq_write_speed_kbs - (float)(unsigned long)result.seq_write_speed_kbs) * 10.0f),
                     (unsigned long)result.seq_read_speed_kbs,
                     (unsigned long)((result.seq_read_speed_kbs - (float)(unsigned long)result.seq_read_speed_kbs) * 10.0f),
                     result.bad_block_count);
    } else {
        Shell_Print("[nandtest] Speed test step failed.\r\n");
    }
    return 0;
}

/* nandtest -b — 仅坏块扫描 */
static int Nand_Opt_BBM(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("[nandtest] Scanning bad blocks...\r\n");
    NandFlash_BBMTest();
    return 0;
}

/* nandtest -p [N] — 仅速度测试 N 块 */
static int Nand_Opt_Perf(int argc, char *argv[])
{
    uint8_t          blocks = 4u;
    NandTestResult_t result;

    if (argc >= 2 && argv[1] != NULL) {
        int v = atoi(argv[1]);
        if (v > 0 && v <= 32) {
            blocks = (uint8_t)v;
        }
    }

    Shell_Printf("[nandtest] Speed test: %u block(s) x 128 KB\r\n", blocks);
    memset(&result, 0, sizeof(result));
    NandFlash_SpeedTest(blocks, &result);

    if (result.test_passed) {
        Shell_Printf("[nandtest] Write: %lu.%lu KB/s (%lu ms)\r\n",
                     (unsigned long)result.seq_write_speed_kbs,
                     (unsigned long)((result.seq_write_speed_kbs - (float)(unsigned long)result.seq_write_speed_kbs) * 10.0f),
                     (unsigned long)result.write_time_ms);
        Shell_Printf("[nandtest] Read : %lu.%lu KB/s (%lu ms)\r\n",
                     (unsigned long)result.seq_read_speed_kbs,
                     (unsigned long)((result.seq_read_speed_kbs - (float)(unsigned long)result.seq_read_speed_kbs) * 10.0f),
                     (unsigned long)result.read_time_ms);
        Shell_Printf("[nandtest] Size : %lu KB  Bad: %u\r\n",
                     (unsigned long)(result.test_size_bytes / 1024u),
                     result.bad_block_count);
    } else {
        Shell_Print("[nandtest] Speed test failed.\r\n");
    }
    return result.test_passed ? 0 : -1;
}

static const ShellOpt_t g_FlashOpts[] = {
    OPT("f", "full", NULL,     "Full test: BBM + functional + speed (4 blks)", Nand_Opt_Full),
    OPT("b", "bbm",  NULL,     "Bad block scan only",                           Nand_Opt_BBM),
    OPT("p", "perf", "[blks]", "Speed test N blocks (default=4)",               Nand_Opt_Perf),
    OPT_END()
};

static const ShellModule_t g_FlashModule = {
    "nandtest",
    "NAND Flash test (blocking). No args = full test.",
    MOD_CAT_DEBUG,
    g_FlashOpts,
    3
};

/*===========================================================================
 * PSRAM 命令
 *===========================================================================*/

/* psramtest (无选项) — 完整测试 */
static int Psram_Opt_Full(int argc, char *argv[])
{
    PsramTestResult_t result;
    (void)argc; (void)argv;

    Shell_Print("[psramtest] === Full PSRAM test (Functional + Speed 512KB) ===\r\n");
    PsramFlash_Test();
    memset(&result, 0, sizeof(result));
    PsramFlash_SpeedTest(512u, &result);

    if (result.test_passed) {
        Shell_Printf("[psramtest] Write: %.1f KB/s  Read: %.1f KB/s\r\n",
                     result.seq_write_speed_kbs, result.seq_read_speed_kbs);
    } else {
        Shell_Print("[psramtest] Speed test step failed.\r\n");
    }
    return 0;
}

/* psramtest -p [KB] — 仅速度测试 */
static int Psram_Opt_Perf(int argc, char *argv[])
{
    uint32_t          size_kb = 512u;
    PsramTestResult_t result;

    if (argc >= 2 && argv[1] != NULL) {
        int v = atoi(argv[1]);
        if (v > 0 && v <= 7168) {
            size_kb = (uint32_t)v;
        }
    }

    Shell_Printf("[psramtest] Speed test: %lu KB\r\n", (unsigned long)size_kb);
    memset(&result, 0, sizeof(result));
    PsramFlash_SpeedTest(size_kb, &result);

    if (result.test_passed) {
        Shell_Printf("[psramtest] Write: %.1f KB/s (%lu ms)\r\n",
                     result.seq_write_speed_kbs, (unsigned long)result.write_time_ms);
        Shell_Printf("[psramtest] Read : %.1f KB/s (%lu ms)\r\n",
                     result.seq_read_speed_kbs,  (unsigned long)result.read_time_ms);
    } else {
        Shell_Print("[psramtest] Speed test failed.\r\n");
    }
    return result.test_passed ? 0 : -1;
}

static const ShellOpt_t g_PsramOpts[] = {
    OPT("f", "full", NULL,       "Full test: functional + speed (512KB)", Psram_Opt_Full),
    OPT("p", "perf", "[size_kb]","Speed test N KB (default=512)",         Psram_Opt_Perf),
    OPT_END()
};

static const ShellModule_t g_PsramModule = {
    "psramtest",
    "PSRAM test (blocking). No args = full test.",
    MOD_CAT_DEBUG,
    g_PsramOpts,
    2
};

/*===========================================================================
 * SD Card 命令
 *===========================================================================*/

/* sdtest (无选项) — 完整测试 */
static int SDCard_Opt_Full(int argc, char *argv[])
{
    SDCardTestResult_t result;
    (void)argc; (void)argv;

    Shell_Print("[sdtest] === Full SD Card test (Functional + Speed 512KB) ===\r\n");
    SDCardFlash_Test();
    memset(&result, 0, sizeof(result));
    SDCardFlash_SpeedTest(1024u, &result);

    if (result.test_passed) {
        Shell_Printf("[sdtest] Write: %.1f KB/s  Read: %.1f KB/s  Card: %lu MB\r\n",
                     result.seq_write_speed_kbs,
                     result.seq_read_speed_kbs,
                     (unsigned long)result.card_capacity_mb);
    } else {
        Shell_Print("[sdtest] Speed test step failed.\r\n");
    }
    return 0;
}

/* sdtest -p [N] — 仅速度测试 N 块 */
static int SDCard_Opt_Perf(int argc, char *argv[])
{
    uint32_t           blocks = 1024u;
    SDCardTestResult_t result;

    if (argc >= 2 && argv[1] != NULL) {
        int v = atoi(argv[1]);
        if (v > 0 && v <= 100000) {
            blocks = (uint32_t)v;
        }
    }

    Shell_Printf("[sdtest] Speed test: %lu blocks (%lu KB)\r\n",
                 (unsigned long)blocks, (unsigned long)(blocks / 2u));
    memset(&result, 0, sizeof(result));
    SDCardFlash_SpeedTest(blocks, &result);

    if (result.test_passed) {
        Shell_Printf("[sdtest] Write: %.1f KB/s (%lu ms)\r\n",
                     result.seq_write_speed_kbs, (unsigned long)result.write_time_ms);
        Shell_Printf("[sdtest] Read : %.1f KB/s (%lu ms)\r\n",
                     result.seq_read_speed_kbs,  (unsigned long)result.read_time_ms);
    } else {
        Shell_Print("[sdtest] Speed test failed.\r\n");
    }
    return result.test_passed ? 0 : -1;
}

static const ShellOpt_t g_SDCardOpts[] = {
    OPT("f", "full", NULL,       "Full test: functional + speed (1024 blks)", SDCard_Opt_Full),
    OPT("p", "perf", "[blocks]", "Speed test N blocks (default=1024)",         SDCard_Opt_Perf),
    OPT_END()
};

static const ShellModule_t g_SDCardModule = {
    "sdtest",
    "SD Card test (blocking). No args = full test.",
    MOD_CAT_DEBUG,
    g_SDCardOpts,
    2
};

/*===========================================================================
 * NOR Flash 命令
 *===========================================================================*/

/* nortest (无选项) — 完整测试 */
static int Nor_Opt_Full(int argc, char *argv[])
{
    NorTestResult_t res;
    (void)argc; (void)argv;
    Shell_Print("[nortest] === Full NOR Flash test ===\r\n");
    FlashNewDriver_Test();
    Shell_Print("\r\n[nortest] Running speed test...\r\n");
    NorFlash_SpeedTest(512u, &res);
    Shell_Printf("[nortest] Write(no erase): %.1f KB/s  Write(w/ erase): %.1f KB/s  Read: %.1f KB/s\r\n",
                 res.pure_write_speed_kbs, res.write_with_erase_speed_kbs, res.seq_read_speed_kbs);
    return 0;
}

/* nortest -q — 快速测试 */
static int Nor_Opt_Quick(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("[nortest] Quick NOR Flash test...\r\n");
    FlashNewDriver_QuickTest();
    return 0;
}

/* nortest -s — 速度测试 */
static int Nor_Opt_Speed(int argc, char *argv[])
{
    NorTestResult_t res;
    (void)argc; (void)argv;
    NorFlash_SpeedTest(512u, &res);
    Shell_Printf("[nortest] Write(no erase): %.1f KB/s  Write(w/ erase): %.1f KB/s  Read: %.1f KB/s\r\n",
                 res.pure_write_speed_kbs, res.write_with_erase_speed_kbs, res.seq_read_speed_kbs);
    return 0;
}

static const ShellOpt_t g_NorOpts[] = {
    OPT("f", "full",  NULL, "Full NOR Flash test",       Nor_Opt_Full),
    OPT("q", "quick", NULL, "Quick single-byte R/W",     Nor_Opt_Quick),
    OPT("s", "speed", NULL, "Sequential R/W speed test", Nor_Opt_Speed),
    OPT_END()
};

static const ShellModule_t g_NorModule = {
    "nortest",
    "NOR Flash test (blocking). No args = full test.",
    MOD_CAT_DEBUG,
    g_NorOpts,
    2
};

#endif /* FLASH_TEST_EN */

/*===========================================================================
 * 注册入口
 *===========================================================================*/

void ShellCmdFlash_Register(void)
{
#if FLASH_TEST_EN
    Shell_RegisterModule(&g_FlashModule);
    Shell_RegisterModule(&g_PsramModule);
    Shell_RegisterModule(&g_SDCardModule);
    Shell_RegisterModule(&g_NorModule);
#endif /* FLASH_TEST_EN */
}

/*===========================================================================
 * 存储抽象层测试命令已合并到 looper 命令中
 * 使用: looper --storage-init/--storage-info/--storage-bench/--storage-overdub
 *===========================================================================*/
