/**
 * @file  dual_partition.h
 * @brief No-Bootloader dual-partition layout and data structures.
 *
 * Architecture (NO separate bootloader)
 * ─────────────────────────────────────
 * The chip boots from 0x000000 where Partition 1 (factory firmware) lives.
 * Early in startup, boot_check() inspects partition flags:
 *   - If Partition 2 has valid firmware and is marked active →
 *     address remap + jump to Partition 2.
 *   - Otherwise → continue running Partition 1.
 *
 * Partition 1 is NEVER re-written after factory programming.
 * USB CDC / BLE OTA upgrades always write to Partition 2.
 *
 * Flash layout (chip = 8 MB / 0x800000):
 *   0x000000 - 0x1FFFFF  Partition 1 (Factory)     2 MB  — safety fallback
 *   0x200000 - 0x3FFFFF  Partition 2 (Upgrade)     2 MB  — USB/BLE target
 *   0x400000 - 0x400FFF  Partition flags            4 KB
 *   0x401000 - 0x7FFFFF  System data / BT config  ~4 MB
 */
#ifndef __DUAL_PARTITION_H__
#define __DUAL_PARTITION_H__

#include "type.h"

/* ── Flash partition layout ──────────────────────────────────────────────── */
#define PART1_BASE          0x00000000UL  /* Partition 1 (factory) base      */
#define PART1_SIZE          0x00200000UL  /* Partition 1: 2 MB               */
#define PART2_BASE          0x00200000UL  /* Partition 2 (upgrade) base      */
#define PART2_SIZE          0x00200000UL  /* Partition 2: 2 MB               */

#define PART_FLAG_ADDR      0x00400000UL  /* Partition flags sector (4 KB)   */
#define PART_FLAG_MAGIC     0x42475057UL  /* "BGPW"                          */

#define FLASH_SECTOR_SZ     0x1000UL      /* 4 KB erase unit                 */

/* ── Firmware validity signature ─────────────────────────────────────────── */
/* Vector table = (9 exception + 32 HW) × 4B = 0xA4 bytes.
 * .stub_section at partition_base + 0xA4 holds FW_VALID_MAGIC
 * to distinguish valid firmware from blank flash.                           */
#define FW_VALID_MAGIC          0x42475046UL  /* "BGPF"                      */
#define FW_VALID_MAGIC_OFFSET   0x000000A4UL

/* ── Boot failure threshold ──────────────────────────────────────────────── */
#define BOOT_FAIL_MAX       3

/* ── Partition flag structure (stored at PART_FLAG_ADDR) ─────────────────── */
/* CRC32 (IEEE 802.3) covers all fields EXCEPT the crc32 field itself.       */
typedef struct {
    uint32_t magic;           /* Must equal PART_FLAG_MAGIC                  */
    uint8_t  active_part;     /* 0 = Partition 1 (factory), 1 = Partition 2  */
    uint8_t  upgrade_pending; /* 1 = reboot-to-part1 needed for re-upgrade   */
    uint8_t  boot_fail_cnt;   /* Incremented before jump to Part 2           */
    uint8_t  reserved;
    uint32_t crc32;           /* CRC32 of preceding 8 bytes                  */
} PartFlag_t;

/* ── Protocol constants (shared by USB CDC and BLE OTA engines) ──────────── */
#define UPG_SOF             0xAAU
#define UPG_VERSION         0x04U   /* v4: no-bootloader dual-partition      */
#define UPG_MAX_CHUNK       256U

/* Commands: Host → Device */
#define CMD_SYNC            0x01U
#define CMD_START           0x02U
#define CMD_DATA            0x03U
#define CMD_FINISH          0x04U
#define CMD_JUMP            0x05U
#define CMD_ERASE           0x06U
#define CMD_QUERY_INFO      0x07U
#define CMD_SET_PART        0x08U
#define CMD_REBOOT          0x09U

/* Responses */
#define RSP_ACK             0xA1U
#define RSP_NACK            0xA2U

/* NACK error codes */
#define UPG_ERR_CRC         0x01U
#define UPG_ERR_FLASH       0x02U
#define UPG_ERR_SIZE        0x03U
#define UPG_ERR_STATE       0x04U
#define UPG_ERR_PARAM       0x05U
#define UPG_ERR_WRONG_PART  0x06U  /* Cannot upgrade while on Partition 2   */

/* ── Device info (CMD_QUERY_INFO ACK payload) ────────────────────────────── */
typedef struct {
    uint8_t  boot_mode;       /* 2 = no-bootloader dual-partition            */
    uint8_t  active_part;     /* 0 = Part 1 running, 1 = Part 2 running      */
    uint8_t  boot_fail_cnt;
    uint8_t  protocol_ver;    /* UPG_VERSION (0x04)                          */
    uint32_t part1_base;
    uint32_t part1_size;
    uint32_t part2_base;
    uint32_t part2_size;
} DevInfo_t;

#define BOOT_MODE_NO_BL_DUAL  2   /* No-bootloader dual-partition mode       */

/* ── Partition flag helpers (implemented in boot_decision.c) ─────────────── */
int  PartFlag_Read(PartFlag_t *flag);
int  PartFlag_Write(const PartFlag_t *flag);
void PartFlag_Default(PartFlag_t *flag);

/* ── Boot decision (implemented in boot_decision.c) ──────────────────────── */
/**
 * @brief  Check partition flags and jump to Partition 2 if valid.
 *         Call VERY EARLY in main(), before FreeRTOS / peripherals.
 *         If Partition 2 is active and valid, this function never returns.
 *         If it returns, we continue running Partition 1.
 */
void Boot_CheckAndJump(void);

/**
 * @brief  Confirm this boot was successful (reset boot_fail_cnt to 0).
 *         Call once early in app startup after basic HW init.
 */
void Boot_ConfirmSuccess(void);

/**
 * @brief  Returns 1 if currently running from Partition 2 (via remap).
 *         Used by upgrade engine to refuse writes when on Partition 2.
 */
int Boot_IsRunningPart2(void);

#endif /* __DUAL_PARTITION_H__ */
