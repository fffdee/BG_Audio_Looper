/**
 * @file  upgrade.h
 * @brief Firmware upgrade protocol — bootloader side (USB CDC only)
 *
 * Bootloader is MINIMAL — no BT/BLE stack, no audio.  It only:
 *   1. Reads partition flags to decide which partition to run.
 *   2. Optionally accepts USB CDC upgrade for factory flashing.
 *
 * Flash layout (chip = 8 MB / 0x800000):
 *   0x000000 - 0x03FFFF  Bootloader      (256 KB) – USB CDC upgrade only
 *   0x040000 - 0x23FFFF  Partition A     (2 MB)
 *   0x240000 - 0x43FFFF  Partition B     (2 MB)
 *   0x440000 - 0x440FFF  Partition flags (4 KB)
 *   0x441000 - 0x7FFFFF  System data     (~3.75 MB)
 *
 * USB CDC packet format (all multi-byte fields big-endian):
 *   [SOF:1][CMD:1][SEQ:2][LEN:2][DATA:len][CRC16:2]
 *   CRC16-CCITT (poly=0x1021, init=0xFFFF) covers CMD+SEQ+LEN+DATA
 */

#ifndef __UPGRADE_H__
#define __UPGRADE_H__

#include "type.h"

/* ── Boot mode ── */
#define BOOT_MODE_SINGLE   0   /* Legacy single-partition                    */
#define BOOT_MODE_DUAL_AB  1   /* A/B dual-partition (equal peers)          */
#define BOOT_CURRENT_MODE  BOOT_MODE_DUAL_AB

/* ── Flash partition layout ── */
#define BOOTLOADER_SIZE      0x00040000UL  /* 256 KB                         */

#define PART_A_BASE          0x00040000UL  /* Partition A base               */
#define PART_A_SIZE          0x00200000UL  /* Partition A: 2 MB              */
#define PART_B_BASE          0x00240000UL  /* Partition B base               */
#define PART_B_SIZE          0x00200000UL  /* Partition B: 2 MB              */

#define PART_FLAG_ADDR       0x00440000UL  /* Partition flag sector (4 KB)   */
#define PART_FLAG_MAGIC      0x42475057UL  /* "BGPW"                         */

#define FLASH_SECTOR_SZ      0x1000UL      /* 4 KB erase unit                */

/* Consecutive boot failures before falling back to the other partition */
#define BOOT_FAIL_MAX        3

/* ── Firmware validity signature ── */
#define FW_VALID_MAGIC        0x42475046UL  /* "BGPF"                        */
#define FW_VALID_MAGIC_OFFSET 0x000000A4UL  /* offset into partition          */

/* ── Boot info (embedded in APP's .stub_section) ── */
#define BOOT_INFO_MAGIC       0x42474F46UL  /* "BGOF" — Boot Info            */
#define BOOT_INFO_OFFSET      0x00000104UL  /* offset from partition base     */

typedef struct {
    uint32_t magic;          /* BOOT_INFO_MAGIC ("BGOF")                    */
    uint32_t data_lma;       /* .data Load Memory Address (in flash)        */
    uint32_t data_vma;       /* .data Virtual Memory Address (in SRAM)      */
    uint32_t data_end;       /* .data end VMA (data_end - data_vma = size)  */
    uint32_t bss_vma;        /* .bss Virtual Memory Address (in SRAM)       */
    uint32_t bss_end;        /* .bss end VMA (bss_end - bss_vma = size)     */
} BootInfo_t;

/* ── Bootloader handoff (SRAM magic) ── */
#define BOOT_HANDOFF_ADDR     0x20000000UL
#define BOOT_HANDOFF_MAGIC    0xDEADBEEFUL

/* ── Partition flag structure ── */
typedef struct {
    uint32_t magic;           /* Must equal PART_FLAG_MAGIC                  */
    uint8_t  active_part;     /* 0 = A is active/primary, 1 = B is active   */
    uint8_t  reserved1;
    uint8_t  boot_fail_cnt;   /* Incremented before each jump; reset on OK  */
    uint8_t  reserved2;
    uint32_t crc32;           /* CRC32 of all preceding bytes in struct      */
} PartFlag_t;

/* ── Protocol framing (USB CDC) ── */
#define UPG_SOF          0xAAU
#define UPG_VERSION      0x03U  /* v3: dual-partition equal peers, CDC only  */
#define UPG_MAX_CHUNK    256U

/* Commands: Host -> Bootloader (USB CDC) */
#define CMD_SYNC         0x01U
#define CMD_START        0x02U
#define CMD_DATA         0x03U
#define CMD_FINISH       0x04U
#define CMD_JUMP         0x05U
#define CMD_ERASE        0x06U
#define CMD_QUERY_INFO   0x07U
#define CMD_SET_PART     0x08U
#define CMD_REBOOT       0x09U

/* Responses */
#define RSP_ACK          0xA1U
#define RSP_NACK         0xA2U

/* NACK error codes */
#define UPG_ERR_CRC      0x01U
#define UPG_ERR_FLASH    0x02U
#define UPG_ERR_SIZE     0x03U
#define UPG_ERR_STATE    0x04U
#define UPG_ERR_PARAM    0x05U

/* ── Device info (CMD_QUERY_INFO ACK payload) ── */
typedef struct {
    uint8_t  boot_mode;       /* BOOT_MODE_DUAL_AB                           */
    uint8_t  active_part;     /* 0=A active, 1=B active                      */
    uint8_t  boot_fail_cnt;
    uint8_t  protocol_ver;    /* UPG_VERSION                                 */
    uint32_t part_a_base;
    uint32_t part_a_size;
    uint32_t part_b_base;
    uint32_t part_b_size;
} DevInfo_t;

/* ── Public partition API ── */
int  PartFlag_Read(PartFlag_t *flag);
int  PartFlag_Write(const PartFlag_t *flag);
void PartFlag_Default(PartFlag_t *flag);

/* ── Boot decision ── */
void Boot_CheckAndJumpIfNeeded(void);
void Boot_JumpTo(uint32_t addr);

/* ── USB CDC upgrade API ── */
void Upgrade_Init(void);
void Upgrade_Process(void);
int  Upgrade_IsActive(void);  /* 1 if currently writing firmware */

#endif /* __UPGRADE_H__ */
