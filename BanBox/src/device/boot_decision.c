/**
 * @file  boot_decision.c
 * @brief Boot decision logic for bootloader + dual-partition scheme.
 *
 * With a separate bootloader, the bootloader has already decided which
 * partition to run and performed the address remap before jumping here.
 * This module provides:
 *   1. Boot_CheckAndJump() — currently a no-op (bootloader handles this),
 *      but kept for API compatibility.
 *   2. Boot_ConfirmSuccess() — resets boot_fail_cnt after successful startup.
 *   3. Boot_IsRunningPart2() — detects if running from Partition B via remap.
 *   4. Partition flag read/write helpers.
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
    flag->active_part     = 0;   /* boot Partition A */
    flag->reserved1       = 0;
    flag->boot_fail_cnt   = 0;
    flag->reserved2       = 0;
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
 * Boot_CheckAndJump — with bootloader, this is handled by bootloader.
 * The APP just needs to detect which partition it's running on.
 * ========================================================================= */
void Boot_CheckAndJump(void)
{
    PartFlag_t flag;

    /* With bootloader, the jump decision is already made.
     * Just detect which partition we're running on. */
    if (PartFlag_Read(&flag) && flag.active_part == 1) {
        g_running_part2 = 1;
        DBG("[BOOT] Running from Partition B (via remap)\n");
    } else {
        g_running_part2 = 0;
        DBG("[BOOT] Running from Partition A\n");
    }
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

    /* Only need to confirm when running Partition B. */
    if (flag.active_part != 1) {
        return;
    }

    if (flag.boot_fail_cnt == 0) {
        return;   /* Already confirmed. */
    }

    flag.boot_fail_cnt = 0;
    PartFlag_Write(&flag);
    g_running_part2 = 1;
    DBG("[BOOT] Boot success confirmed (Part B, fail_cnt reset)\n");
}
