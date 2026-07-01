/**
 * @file shell_cmd_speedtest.c
 * @brief PSRAM & SD Card 读写速度测试 Shell 命令
 * 
 * 命令:
 *   speedtest psram [size]     PSRAM 读写速度测试
 *   speedtest sd    [size]     SD卡 读写速度测试
 *   speedtest all   [size]     同时测试 PSRAM 和 SD卡
 * 
 * 编译条件: HW_DRV_PSRAM_EN || HW_DRV_SDCARD_EN
 */

#include "product_def.h"

#if HW_DRV_PSRAM_EN || HW_DRV_SDCARD_EN

#include "bg_shell.h"
#include "bg_config.h"
#include <string.h>
#include <stdio.h>

/* HAL 依赖 */
#if HW_DRV_PSRAM_EN
#include "../../02_device_drivers/flash/flash_devices.h"
#include "../../02_device_drivers/flash/flash_bus.h"
#include "../../05_component/fat32/psram_heap.h"
#endif

#if HW_DRV_SDCARD_EN
#include "../../01_hal_drivers/sdio/hal_sdio.h"
#include "../../05_component/fat32/fat32.h"
#endif

#include "FreeRTOS.h"
#include "task.h"

/* ============================================
 * 内部函数声明
 * ============================================ */

static int speedtest_psram_cmd(int argc, char *argv[]);
static int speedtest_sd_cmd(int argc, char *argv[]);
static int speedtest_all_cmd(int argc, char *argv[]);

/* ============================================
 * 默认测试数据大小
 * ============================================ */

#define SPEEDTEST_DEFAULT_SIZE_KB   (1u * 1024u)      /* 默认: 1MB */
#define SPEEDTEST_MAX_SIZE_KB       (4u * 1024u)      /* 最大: 4MB */
#define SPEEDTEST_MIN_SIZE_KB       16u                /* 最小: 16KB */
#define SPEEDTEST_BUF_SIZE          512u               /* 单次传输块大小 */

#if HW_DRV_PSRAM_EN
#define SPEEDTEST_PSRAM_MAX_KB      (6u * 1024u)      /* PSRAM 样本区: 6MB */
#endif

#if HW_DRV_SDCARD_EN
#define SPEEDTEST_SD_BLOCK_SIZE     512u
#endif

/* ============================================
 * 命令选项定义
 * ============================================ */

static const ShellOpt_t speedtest_options[] = {
    OPT("psram", "[size_kb]", "PSRAM read/write speed test (default 1024 KB)", speedtest_psram_cmd),
    OPT("sd",    "[size_kb]", "SD card read/write speed test (default 1024 KB)", speedtest_sd_cmd),
    OPT("all",   "[size_kb]", "Test both PSRAM and SD card", speedtest_all_cmd),
    OPT_END()
};

/* ============================================
 * 模块定义
 * ============================================ */

DEFINE_MODULE(speedtest, "PSRAM & SD Card speed benchmark", MOD_CAT_DEBUG, speedtest_options);

/* ============================================
 * 公共接口实现
 * ============================================ */

int ShellCmdSpeedTest_Register(void)
{
    return Shell_RegisterModule(&_mod_speedtest) ? 0 : -1;
}

/* ============================================
 * 辅助函数
 * ============================================ */

/**
 * @brief 解析大小参数 (KB)
 */
static uint32_t parse_size_kb(int argc, char *argv[])
{
    uint32_t size_kb = SPEEDTEST_DEFAULT_SIZE_KB;
    if (argc >= 2) {
        int val = atoi(argv[1]);
        if (val > 0) {
            size_kb = (uint32_t)val;
        }
    }
    if (size_kb < SPEEDTEST_MIN_SIZE_KB) {
        size_kb = SPEEDTEST_MIN_SIZE_KB;
    }
    if (size_kb > SPEEDTEST_MAX_SIZE_KB) {
        size_kb = SPEEDTEST_MAX_SIZE_KB;
    }
    return size_kb;
}

/**
 * @brief 获取 FreeRTOS tick 微秒时间戳
 */
static uint32_t get_time_us(void)
{
    TickType_t ticks = xTaskGetTickCount();
    return (uint32_t)(ticks * portTICK_PERIOD_MS * 1000);
}

/* ============================================
 * PSRAM 测速实现
 * ============================================ */

#if HW_DRV_PSRAM_EN

static int speedtest_psram_cmd(int argc, char *argv[])
{
    uint32_t size_kb = parse_size_kb(argc, argv);
    uint32_t size_bytes = size_kb * 1024u;
    uint32_t psram_addr_base = 0x100000u;  /* 测试区: 1MB 起始, 避开前1MB */
    uint32_t i, rounds, elapsed_us;
    uint32_t write_speed_kbps, read_speed_kbps;
    uint8_t test_buf[SPEEDTEST_BUF_SIZE];
    FlashDevice_t *psram_dev;
    uint32_t t_start, t_end;

    (void)argc; (void)argv;

    /* 获取 PSRAM 设备 */
    psram_dev = FlashDevices_GetPsramFlash();
    if (!psram_dev || !psram_dev->initialized) {
        Shell_Print("ERROR: PSRAM device not available\r\n");
        return -1;
    }

    Shell_Print("========================================\r\n");
    Shell_Print("  PSRAM Speed Test\r\n");
    Shell_Print("========================================\r\n");
    Shell_Printf("  Test size : %lu KB (%lu bytes)\r\n", 
                 (unsigned long)size_kb, (unsigned long)size_bytes);
    Shell_Printf("  Block size: %u bytes\r\n", SPEEDTEST_BUF_SIZE);
    Shell_Printf("  PSRAM size: %lu MB\r\n", 
                 (unsigned long)(psram_dev->info.total_size / (1024*1024)));
    Shell_Print("----------------------------------------\r\n");

    rounds = size_bytes / SPEEDTEST_BUF_SIZE;

    /* 填充测试数据 */
    for (i = 0; i < SPEEDTEST_BUF_SIZE; i++) {
        test_buf[i] = (uint8_t)(i & 0xFF);
    }

    /* --- PSRAM 写入速度测试 --- */
    Shell_Print("  Testing PSRAM WRITE...\r\n");
    t_start = get_time_us();
    
    for (i = 0; i < rounds; i++) {
        FlashStatus_t ret = psram_dev->ops->write(psram_dev, 
            psram_addr_base + i * SPEEDTEST_BUF_SIZE, test_buf, SPEEDTEST_BUF_SIZE);
        if (ret != FLASH_OK) {
            Shell_Printf("  ERROR: PSRAM write failed at block %lu\r\n", (unsigned long)i);
            return -1;
        }
    }
    
    t_end = get_time_us();
    elapsed_us = t_end - t_start;
    write_speed_kbps = (elapsed_us > 0) ? 
        (size_bytes * 1000000u / elapsed_us / 1024u) : 0;

    Shell_Printf("  WRITE: %lu KB in %lu us → %lu KB/s\r\n", 
                 (unsigned long)size_kb, (unsigned long)elapsed_us, 
                 (unsigned long)write_speed_kbps);

    /* --- PSRAM 读取速度测试 --- */
    memset(test_buf, 0, SPEEDTEST_BUF_SIZE);
    Shell_Print("  Testing PSRAM READ...\r\n");
    t_start = get_time_us();
    
    for (i = 0; i < rounds; i++) {
        FlashStatus_t ret = psram_dev->ops->read(psram_dev,
            psram_addr_base + i * SPEEDTEST_BUF_SIZE, test_buf, SPEEDTEST_BUF_SIZE);
        if (ret != FLASH_OK) {
            Shell_Printf("  ERROR: PSRAM read failed at block %lu\r\n", (unsigned long)i);
            return -1;
        }
    }
    
    t_end = get_time_us();
    elapsed_us = t_end - t_start;
    read_speed_kbps = (elapsed_us > 0) ? 
        (size_bytes * 1000000u / elapsed_us / 1024u) : 0;

    Shell_Printf("  READ:  %lu KB in %lu us → %lu KB/s\r\n", 
                 (unsigned long)size_kb, (unsigned long)elapsed_us, 
                 (unsigned long)read_speed_kbps);

    /* --- 数据验证 --- */
    {
        uint32_t verify_errors = 0;
        for (i = 0; i < SPEEDTEST_BUF_SIZE; i++) {
            if (test_buf[i] != (uint8_t)(i & 0xFF)) {
                verify_errors++;
            }
        }
        Shell_Printf("  Verify: %s (%lu errors in first block)\r\n",
                     (verify_errors == 0) ? "PASS" : "FAIL",
                     (unsigned long)verify_errors);
    }

    Shell_Print("========================================\r\n");
    Shell_Printf("  PSRAM  Write: %lu KB/s  Read: %lu KB/s\r\n",
                 (unsigned long)write_speed_kbps, (unsigned long)read_speed_kbps);
    Shell_Print("========================================\r\n");

    return 0;
}

#endif /* HW_DRV_PSRAM_EN */

/* ============================================
 * SD卡 测速实现
 * ============================================ */

#if HW_DRV_SDCARD_EN

static int speedtest_sd_cmd(int argc, char *argv[])
{
    uint32_t size_kb = parse_size_kb(argc, argv);
    uint32_t size_bytes = size_kb * 1024u;
    uint32_t total_blocks;
    uint32_t start_block = 0x10000u;  /* 测试区: 从 32MB 偏移开始 */
    uint32_t i, elapsed_us;
    uint32_t write_speed_kbps, read_speed_kbps;
    uint8_t test_buf[SPEEDTEST_BUF_SIZE];
    uint8_t verify_buf[SPEEDTEST_BUF_SIZE];
    uint32_t t_start, t_end;
    HAL_SD_CardInfo_t sd_info;
    uint32_t blocks_per_round;
    uint32_t rounds;

    (void)argc; (void)argv;

    /* 检测 SD 卡 */
    if (!HAL_SD_Detect()) {
        Shell_Print("ERROR: No SD card detected\r\n");
        return -1;
    }

    /* 初始化 SD 卡 */
    if (HAL_SD_Init() != HAL_SD_OK) {
        Shell_Print("ERROR: SD card init failed\r\n");
        return -1;
    }

    /* 获取 SD 卡信息 */
    if (HAL_SD_GetInfo(&sd_info) != HAL_SD_OK) {
        Shell_Print("ERROR: Cannot get SD card info\r\n");
        return -1;
    }

    total_blocks = size_bytes / SPEEDTEST_BUF_SIZE;
    blocks_per_round = SPEEDTEST_BUF_SIZE / SPEEDTEST_SD_BLOCK_SIZE;  /* = 1 */
    rounds = size_bytes / SPEEDTEST_BUF_SIZE;

    Shell_Print("========================================\r\n");
    Shell_Print("  SD Card Speed Test\r\n");
    Shell_Print("========================================\r\n");
    Shell_Printf("  Test size : %lu KB (%lu bytes)\r\n", 
                 (unsigned long)size_kb, (unsigned long)size_bytes);
    Shell_Printf("  Block size: %u bytes (%u SD blocks)\r\n", 
                 SPEEDTEST_BUF_SIZE, blocks_per_round);
    Shell_Printf("  SD  type  : %s\r\n", 
                 (sd_info.type == HAL_SD_CARD_TYPE_SDHC) ? "SDHC" : 
                 (sd_info.type == HAL_SD_CARD_TYPE_SDXC) ? "SDXC" : "SDSC");
    Shell_Printf("  SD  size  : %llu MB\r\n",
                 (unsigned long long)(sd_info.capacity_bytes / (1024*1024)));
    Shell_Print("----------------------------------------\r\n");

    /* 填充测试数据 */
    for (i = 0; i < SPEEDTEST_BUF_SIZE; i++) {
        test_buf[i] = (uint8_t)((i * 7 + 13) & 0xFF);
    }

    /* --- SD卡 写入速度测试 --- */
    Shell_Print("  Testing SD CARD WRITE...\r\n");
    t_start = get_time_us();
    
    for (i = 0; i < rounds; i++) {
        HAL_SD_Error_t ret = HAL_SD_WriteBlocks(
            start_block + i * blocks_per_round, test_buf, blocks_per_round);
        if (ret != HAL_SD_OK) {
            Shell_Printf("  ERROR: SD write failed at block %lu\r\n", 
                         (unsigned long)(start_block + i * blocks_per_round));
            return -1;
        }
    }
    
    t_end = get_time_us();
    elapsed_us = t_end - t_start;
    write_speed_kbps = (elapsed_us > 0) ? 
        (size_bytes * 1000000u / elapsed_us / 1024u) : 0;

    Shell_Printf("  WRITE: %lu KB in %lu us → %lu KB/s\r\n", 
                 (unsigned long)size_kb, (unsigned long)elapsed_us, 
                 (unsigned long)write_speed_kbps);

    /* --- SD卡 读取速度测试 --- */
    memset(verify_buf, 0, SPEEDTEST_BUF_SIZE);
    Shell_Print("  Testing SD CARD READ...\r\n");
    t_start = get_time_us();
    
    for (i = 0; i < rounds; i++) {
        HAL_SD_Error_t ret = HAL_SD_ReadBlocks(
            start_block + i * blocks_per_round, verify_buf, blocks_per_round);
        if (ret != HAL_SD_OK) {
            Shell_Printf("  ERROR: SD read failed at block %lu\r\n",
                         (unsigned long)(start_block + i * blocks_per_round));
            return -1;
        }
    }
    
    t_end = get_time_us();
    elapsed_us = t_end - t_start;
    read_speed_kbps = (elapsed_us > 0) ? 
        (size_bytes * 1000000u / elapsed_us / 1024u) : 0;

    Shell_Printf("  READ:  %lu KB in %lu us → %lu KB/s\r\n", 
                 (unsigned long)size_kb, (unsigned long)elapsed_us, 
                 (unsigned long)read_speed_kbps);

    /* --- 数据验证 --- */
    {
        uint32_t verify_errors = 0;
        for (i = 0; i < SPEEDTEST_BUF_SIZE; i++) {
            if (verify_buf[i] != (uint8_t)((i * 7 + 13) & 0xFF)) {
                verify_errors++;
            }
        }
        Shell_Printf("  Verify: %s (%lu errors in first block)\r\n",
                     (verify_errors == 0) ? "PASS" : "FAIL",
                     (unsigned long)verify_errors);
    }

    Shell_Print("========================================\r\n");
    Shell_Printf("  SD Card Write: %lu KB/s  Read: %lu KB/s\r\n",
                 (unsigned long)write_speed_kbps, (unsigned long)read_speed_kbps);
    Shell_Print("========================================\r\n");

    return 0;
}

#endif /* HW_DRV_SDCARD_EN */

/* ============================================
 * 综合测试实现
 * ============================================ */

static int speedtest_all_cmd(int argc, char *argv[])
{
    Shell_Print("\r\n============ PSRAM + SD Card Speed Test ============\r\n\r\n");

#if HW_DRV_PSRAM_EN
    speedtest_psram_cmd(argc, argv);
    Shell_Print("\r\n");
#endif

#if HW_DRV_SDCARD_EN
    speedtest_sd_cmd(argc, argv);
#endif

    Shell_Print("\r\n============ Speed Test Complete ============\r\n");
    return 0;
}

/* ============================================
 * 公共头文件接口
 * ============================================ */

#endif /* HW_DRV_PSRAM_EN || HW_DRV_SDCARD_EN */
