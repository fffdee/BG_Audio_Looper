/**
 * @file  boot_decision.c
 * @brief Boot decision logic for no-bootloader dual-partition scheme.
 *
 * Called VERY EARLY in main() before FreeRTOS or peripherals.
 * Decides whether to jump to Partition 2 or continue Partition 1.
 */

#include <string.h>
#include <nds32_intrinsic.h>
#include "dual_partition.h"
#include "spi_flash.h"
#include "watchdog.h"
#include "remap.h"
#include "debug.h"

/* =========================================================================
 * CRC32 (IEEE 802.3, poly = 0xEDB88320) — for partition flags
 * ========================================================================= */
static uint32_t crc32_calc(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= (uint32_t)buf[i];
        for (j = 0; j < 8u; j++)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* =========================================================================
 * Partition flag helpers
 * ========================================================================= */
static void part_flag_seal(PartFlag_t *f)
{
    f->magic = PART_FLAG_MAGIC;
    f->crc32 = crc32_calc((const uint8_t *)f,
                          sizeof(PartFlag_t) - sizeof(uint32_t));
}

static int part_flag_valid(const PartFlag_t *f)
{
    if (f->magic != PART_FLAG_MAGIC) return 0;
    return (crc32_calc((const uint8_t *)f,
                       sizeof(PartFlag_t) - sizeof(uint32_t))
            == f->crc32) ? 1 : 0;
}

int PartFlag_Read(PartFlag_t *flag)
{
    memcpy(flag, (const void *)PART_FLAG_ADDR, sizeof(PartFlag_t));
    return part_flag_valid(flag) ? 1 : 0;
}

void PartFlag_Default(PartFlag_t *flag)
{
    memset(flag, 0, sizeof(PartFlag_t));
    flag->active_part     = 0;   /* boot Partition 1 (factory) */
    flag->upgrade_pending = 0;
    flag->boot_fail_cnt   = 0;
    part_flag_seal(flag);
}

int PartFlag_Write(const PartFlag_t *flag)
{
    PartFlag_t tmp;
    memcpy(&tmp, flag, sizeof(PartFlag_t));
    part_flag_seal(&tmp);

    if (FlashErase(PART_FLAG_ADDR, FLASH_SECTOR_SZ) != FLASH_NONE_ERR)
        return 0;
    return (SpiFlashWrite(PART_FLAG_ADDR, (uint8_t *)&tmp,
                         sizeof(PartFlag_t), 0) == FLASH_NONE_ERR) ? 1 : 0;
}

/* =========================================================================
 * Runtime partition tracking
 * ========================================================================= */
static int g_running_part2 = 0;

int Boot_IsRunningPart2(void)
{
    return g_running_part2;
}

/* =========================================================================
 * Boot_JumpTo — jump to an address (never returns)
 * ========================================================================= */
static void Boot_JumpTo(uint32_t addr)
{
    typedef void (*Entry_t)(void);
    Entry_t entry;

    WDG_Disable();
    __nds32__setgie_dis();
    entry = (Entry_t)addr;
    entry();
    while (1);
}

/* =========================================================================
 * Boot_CheckAndJump — early startup decision
 * ========================================================================= */
void Boot_CheckAndJump(void)
{
    PartFlag_t flag;
    volatile const uint32_t *magic_ptr;

    if (!PartFlag_Read(&flag)) {
        /* No valid flag → first power-on or fresh chip.
         * Stay in Partition 1 (factory). */
        DBG("[BOOT] No partition flag — running Partition 1\n");
        g_running_part2 = 0;
        return;
    }

    /* If active_part == 0, stay in Partition 1 (factory). */
    if (flag.active_part == 0) {
        DBG("[BOOT] active_part=0 — running Partition 1\n");
        g_running_part2 = 0;
        return;
    }

    /* active_part == 1: want to boot Partition 2.
     * Check boot failure count first. */
    if (flag.boot_fail_cnt >= BOOT_FAIL_MAX) {
        /* Partition 2 failed too many times → revert to Partition 1. */
        DBG("[BOOT] Part 2 failed %d times — reverting to Part 1\n",
            (int)flag.boot_fail_cnt);
        flag.active_part    = 0;
        flag.boot_fail_cnt  = 0;
        PartFlag_Write(&flag);
        g_running_part2 = 0;
        return;
    }

    /* Check Partition 2 firmware validity (magic at base + 0xA4). */
    magic_ptr = (volatile const uint32_t *)(PART2_BASE + FW_VALID_MAGIC_OFFSET);
    if (*magic_ptr != FW_VALID_MAGIC) {
        /* Partition 2 has no valid firmware — stay in Partition 1. */
        DBG("[BOOT] Part 2 firmware invalid (magic=0x%08X) — staying Part 1\n",
            (unsigned)*magic_ptr);
        flag.active_part    = 0;
        flag.boot_fail_cnt  = 0;
        PartFlag_Write(&flag);
        g_running_part2 = 0;
        return;
    }

    /* Partition 2 is valid. Increment fail count before jumping.
     * The app will call Boot_ConfirmSuccess() to reset it. */
    flag.boot_fail_cnt++;
    PartFlag_Write(&flag);

    DBG("[BOOT] Jumping to Part 2 (attempt %d/%d)\n",
        (int)flag.boot_fail_cnt, BOOT_FAIL_MAX);

    /* Partition 2 firmware is physically at PART2_BASE (0x200000) but
     * the binary is linked to run at 0x000000 (= PART1_BASE).
     * Use hardware address remap so CPU accesses to
     * [0x000000, 0x000000+2MB) are served from [0x200000, 0x200000+2MB). */
    Remap_AddrRemapSet(ADDR_REMAP0, PART1_BASE, PART2_BASE,
                       (uint32_t)(PART2_SIZE / 1024UL));  /* 2048 KB */

    /* Jump to 0x000000 — CPU sees Part 1 address, HW fetches Part 2 data. */
    Boot_JumpTo(PART1_BASE);
    /* Never reaches here. */
}

/* =========================================================================
 * Boot_ConfirmSuccess — reset boot_fail_cnt to 0
 * ========================================================================= */
void Boot_ConfirmSuccess(void)
{
    PartFlag_t flag;

    if (!PartFlag_Read(&flag)) {
        /* No valid flags — nothing to confirm. */
        return;
    }

    /* Only need to confirm when running Partition 2. */
    if (flag.active_part != 1) {
        return;
    }

    if (flag.boot_fail_cnt == 0) {
        return;   /* Already confirmed. */
    }

    flag.boot_fail_cnt = 0;
    PartFlag_Write(&flag);
    g_running_part2 = 1;
    DBG("[BOOT] Boot success confirmed (Part 2, fail_cnt reset)\n");
}
