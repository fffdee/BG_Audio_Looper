/**
 * @file  upgrade.c
 * @brief Firmware upgrade handler — USB CDC only, A/B dual-partition
 *        with watchdog fallback.
 *
 * Flash layout (dual-partition mode, BOOT_CURRENT_MODE == BOOT_MODE_DUAL_AB):
 *   0x00000000 - 0x0003FFFF  Bootloader      (256 KB)
 *   0x00040000 - 0x0023FFFF  Partition A      (2 MB)
 *   0x00240000 - 0x0043FFFF  Partition B      (2 MB)
 *   0x00440000 - 0x00440FFF  Partition flags  (4 KB)
 *
 * Packet format (big-endian multi-byte fields):
 *   [SOF:1][CMD:1][SEQ:2][LEN:2][DATA:len][CRC16:2]
 *   CRC16-CCITT over CMD+SEQ+LEN+DATA
 */

#include <string.h>
#include <nds32_intrinsic.h>
#include "upgrade.h"
#include "otg_device_cdc.h"
#include "otg_device_standard_request.h"
#include "spi_flash.h"
#include "watchdog.h"
#include "timer.h"
#include "debug.h"
#include "remap.h"
#include "irqn.h"
#include "core_d1088.h"
#include "nds32_defs.h"

/* =========================================================================
 * CRC32 (IEEE 802.3, poly = 0xEDB88320) – used for partition flags
 * ========================================================================= */

/* Direct UART1 write for diagnostics — bypasses DBG/printf.
 * Works even with interrupts disabled (poll-based). */
#define DIAG_UART1_STATUS  (*(volatile uint32_t *)0x40006014)
#define DIAG_UART1_TX      (*(volatile uint32_t *)0x40006018)
static inline void diag_putc(char c)
{
    while (!(DIAG_UART1_STATUS & (1u << 9))) ;  /* wait for TX FIFO ready */
    DIAG_UART1_TX = (uint32_t)(unsigned char)c;
}

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
 * Dynamic flash layout
 *
 * Flash capacity may be smaller than the default layout assumes (8 MB).
 * If so, addresses like PART_FLAG_ADDR (0x440000) and PART_B_BASE (0x240000)
 * may wrap around, causing PartFlag writes to overwrite firmware data.
 * We detect the actual flash size at runtime and adjust accordingly.
 * ========================================================================= */
static uint32_t g_part_flag_addr;     /* actual address of partition flags  */
static uint32_t g_flash_capacity;     /* flash size in bytes               */
static uint32_t g_part_a_usable;      /* usable A partition size (bytes)   */
static uint32_t g_part_b_usable;      /* usable B partition size (bytes)   */

void PartFlag_Init(void)
{
    /* Use internal ROM capacity (no external NOR Flash detection) */
    g_flash_capacity = INTERNAL_ROM_CAPACITY;

    DBG("[BOOT] Internal ROM: Capacity=%u bytes (%u KB)\n",
        (unsigned)g_flash_capacity, (unsigned)(g_flash_capacity / 1024));

    /* Compute safe PartFlag address: use last 4KB sector of flash.
     * This guarantees it won't wrap regardless of flash size. */
    if (g_flash_capacity >= PART_FLAG_ADDR + FLASH_SECTOR_SZ) {
        /* Flash large enough — use default address */
        g_part_flag_addr = PART_FLAG_ADDR;
    } else {
        /* Flash too small — place PartFlag at last sector */
        g_part_flag_addr = (g_flash_capacity - FLASH_SECTOR_SZ)
                           & ~(FLASH_SECTOR_SZ - 1u);
        DBG("[BOOT] WARNING: Flash too small for default layout!\n");
        DBG("[BOOT] PartFlag moved: 0x%08X -> 0x%08X\n",
            (unsigned)PART_FLAG_ADDR, (unsigned)g_part_flag_addr);
    }

    /* Compute usable Partition B size (clamped to actual flash boundary) */
    if (g_flash_capacity >= PART_B_BASE + PART_B_SIZE) {
        g_part_b_usable = PART_B_SIZE;
    } else if (g_flash_capacity > PART_B_BASE) {
        g_part_b_usable = g_flash_capacity - PART_B_BASE;
        /* Shrink further if PartFlag is inside Partition B */
        if (g_part_flag_addr >= PART_B_BASE &&
            g_part_flag_addr < PART_B_BASE + g_part_b_usable) {
            g_part_b_usable = g_part_flag_addr - PART_B_BASE;
        }
        DBG("[BOOT] Partition B usable: %u KB (of %u KB)\n",
            (unsigned)(g_part_b_usable / 1024),
            (unsigned)(PART_B_SIZE / 1024));
    } else {
        g_part_b_usable = 0;
        DBG("[BOOT] WARNING: Flash too small for Partition B "
            "(need >= %u KB, have %u KB)\n",
            (unsigned)((PART_B_BASE + PART_B_SIZE) / 1024),
            (unsigned)(g_flash_capacity / 1024));
    }

    /* Compute usable Partition A size */
    if (g_flash_capacity >= PART_A_BASE + PART_A_SIZE) {
        g_part_a_usable = PART_A_SIZE;
    } else if (g_flash_capacity > PART_A_BASE) {
        g_part_a_usable = g_flash_capacity - PART_A_BASE;
        DBG("[BOOT] Partition A usable: %u KB (of %u KB)\n",
            (unsigned)(g_part_a_usable / 1024),
            (unsigned)(PART_A_SIZE / 1024));
    } else {
        g_part_a_usable = 0;
        DBG("[BOOT] FATAL: Flash too small for Partition A!\n");
    }

    /* Validate layout */
    if (g_part_flag_addr >= PART_A_BASE &&
        g_part_flag_addr < PART_A_BASE + PART_A_SIZE) {
        DBG("[BOOT] FATAL: PartFlag@0x%08X overlaps Partition A!\n",
            (unsigned)g_part_flag_addr);
    }
    if (g_part_flag_addr >= PART_B_BASE &&
        g_part_flag_addr < PART_B_BASE + PART_B_SIZE) {
        DBG("[BOOT] PartFlag@0x%08X is inside Partition B region, "
            "B usable size reduced\n", (unsigned)g_part_flag_addr);
    }

    /* Summary */
    {
        uint8_t caps = PartFlag_GetCaps();
        DBG("[BOOT] Capability: %s mode (caps=0x%02X)\n",
            (caps & CAPS_DUAL_PART) ? "DUAL-PARTITION" : "SINGLE-PARTITION",
            (unsigned)caps);
    }
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
    if (g_part_flag_addr == 0u) {
        /* PartFlag_Init not yet called — use default address as fallback */
        memcpy(flag, (const void *)PART_FLAG_ADDR, sizeof(PartFlag_t));
    } else {
        DataCacheInvalidAll();
        __nds32__dsb();
        memcpy(flag, (const void *)g_part_flag_addr, sizeof(PartFlag_t));
    }
    return part_flag_valid(flag) ? 1 : 0;
}

void PartFlag_Default(PartFlag_t *flag)
{
    memset(flag, 0, sizeof(PartFlag_t));
    flag->active_part = 0;   /* boot A */
    part_flag_seal(flag);
}

int PartFlag_Write(const PartFlag_t *flag)
{
    PartFlag_t tmp;
    uint32_t addr = (g_part_flag_addr != 0u) ? g_part_flag_addr : PART_FLAG_ADDR;
    memcpy(&tmp, flag, sizeof(PartFlag_t));
    part_flag_seal(&tmp);          /* always re-seal before writing */

    /* 直接调用 SpiFlashErase 避免 FlashErase 容量检查陷阱
     *
     * IsSuspend=0（阻塞模式）！原因：
     *   SpiFlashErase/SpiFlashWrite 在 IsSuspend=1 时会进入
     *   suspend/resume 循环，resume 连续失败 5 次后调用
     *   vTaskDelay(1) 让出 CPU。但此函数会在
     *   Boot_CheckAndJumpIfNeeded() 中被调用——此时 FreeRTOS
     *   尚未启动，vTaskDelay 访问未初始化的调度器数据结构，
     *   导致堆链表损坏，最终在 uxListRemove 触发 Bus Error
     *   (R0=0x4 损坏指针)。IsSuspend=0 时函数仅阻塞轮询
     *   等待完成，不调用 vTaskDelay，在 FreeRTOS 启动前后均安全。
     *   分区标志仅 4KB 单扇区擦除 + 20 字节写入，阻塞时间
     *   约 100~400ms，可接受。 */
    SpiFlashErase(SECTOR_ERASE, addr / FLASH_SECTOR_SZ, 0);
    return (SpiFlashWrite(addr, (uint8_t *)&tmp,
                         sizeof(PartFlag_t), 0) == FLASH_NONE_ERR) ? 1 : 0;
}

uint8_t PartFlag_GetCaps(void)
{
    uint8_t caps = 0;
    if (g_part_a_usable > 0 && g_part_b_usable > 0) {
        /* Both partitions available — true dual-partition */
        caps |= CAPS_DUAL_PART | CAPS_SAFE_UPDATE;
    } else if (g_part_a_usable > 0 || g_part_b_usable > 0) {
        /* Only one partition — single-partition mode, no safe update */
        /* (caps remains 0 for single-partition) */
    }
    caps |= CAPS_CRC_VERIFY;  /* We always verify firmware integrity */
    return caps;
}

/* =========================================================================
 * Boot decision
 * ========================================================================= */
void Boot_JumpTo(uint32_t addr)
{
    /* ---- Phase 1: Quiesce all hardware ---- */
    WDG_Disable();

    /* Stop Timer2 (bootloader's 1ms tick) */
    Timer_Pause(TIMER2, 1);
    Timer_InterruptFlagClear(TIMER2, UPDATE_INTERRUPT_SRC);

    /* Disable all NVIC interrupt sources (INT_MASK2 = 0) */
    __nds32__mtsr(0x0, NDS32_SR_INT_MASK2);

    /* Disable global interrupts */
    __nds32__setgie_dis();
    __nds32__dsb();

    /* ---- Phase 2: Invalidate D-cache, keep I-Cache ----
     * CRITICAL: Do NOT invalidate I-Cache. */
    DataCacheInvalidAll();

    /* ---- Phase 3: Copy APP's .data from flash to SRAM ---- */
    {
        const BootInfo_t *info = (const BootInfo_t *)(addr + BOOT_INFO_OFFSET);

        diag_putc('P');  /* Phase 3 entered */

        if (info->magic == BOOT_INFO_MAGIC)
        {
            uint32_t i;
            uint32_t nwords;
            volatile uint32_t *dst;
            const volatile uint32_t *src;

            nwords = (info->data_end - info->data_vma + 3u) / 4u;
            dst    = (volatile uint32_t *)info->data_vma;
            src    = (const volatile uint32_t *)info->data_lma;
            for (i = 0; i < nwords; i++)
                dst[i] = src[i];

            diag_putc('d');  /* .data copied */

            /* Also clear .bss in SRAM */
            {
                uint32_t bss_nwords;
                volatile uint32_t *bss_dst;
                bss_nwords = (info->bss_end - info->bss_vma + 3u) / 4u;
                bss_dst    = (volatile uint32_t *)info->bss_vma;
                for (i = 0; i < bss_nwords; i++)
                    bss_dst[i] = 0u;

                diag_putc('z');  /* .bss cleared */
            }

            /* Write handoff magic so APP's __c_init() skips .data copy
             * AND .bss clear. */
            *(volatile uint32_t *)BOOT_HANDOFF_ADDR = BOOT_HANDOFF_MAGIC;
            diag_putc('H');  /* handoff magic written */
        }
        else
        {
            /* BootInfo magic 不匹配 — APP 固件未嵌入 BootInfo 结构。
             * APP 的 C runtime 启动代码（_start/__c_init）会自行处理
             * .data 拷贝和 .bss 清零，此处直接跳转即可。 */
            diag_putc('?');
            DBG("[BOOT] No BootInfo at 0x%08X, APP will self-init .data/.bss\n",
                (unsigned)(addr + BOOT_INFO_OFFSET));
        }
    }

    /* ---- Phase 4: Jump to APP ---- */
    {
        typedef void (*Entry_t)(void);
        Entry_t entry = (Entry_t)addr;
        diag_putc('J');  /* about to jump */
        entry();
    }

    /* Should never reach here */
    while (1);
}

void Boot_CheckAndJumpIfNeeded(void)
{
    /* ── Burn flag check (Flash, one-time) ──
     * APP writes BURN_FLAG_MAGIC to Flash before rebooting to request
     * bootloader stay.  Must check and clear BEFORE any other boot logic.
     * This is a one-time flag: after reading, erase it so next reboot
     * jumps to APP normally. */
    {
        uint32_t flag_val = *(volatile const uint32_t *)BURN_FLAG_ADDR;
        if (flag_val == BURN_FLAG_MAGIC) {
            DBG("[BOOT] Burn flag detected @0x%08X — staying in upgrade mode\n",
                (unsigned)BURN_FLAG_ADDR);
            /* Erase the flag (one-time)
             * IsSuspend=0 (blocking): Boot_CheckAndJumpIfNeeded() is called
             * BEFORE FreeRTOS scheduler starts.  IsSuspend=1 would invoke
             * vTaskDelay() on an uninitialized scheduler → Bus Error crash.
             * Same fix as PartFlag_Write() above. */
            SpiFlashErase(SECTOR_ERASE, BURN_FLAG_SECTOR, 0);
            DBG("[BOOT] Burn flag cleared\n");
            return;  /* Skip APP jump, fall through to USB CDC upgrade task */
        }
    }

#if (BOOT_CURRENT_MODE == BOOT_MODE_DUAL_AB)
    PartFlag_t flag;
    uint32_t   jump_addr;

    if (!PartFlag_Read(&flag)) {
        DBG("[BOOT] No partition flag – booting A\n");
        jump_addr = PART_A_BASE;
    } else {
        uint32_t backup_addr =
            (flag.active_part == 0) ? PART_B_BASE : PART_A_BASE;
        jump_addr =
            (flag.active_part == 1) ? PART_B_BASE : PART_A_BASE;

        /* Safety: if active part is B but B is unavailable, force to A */
        if (flag.active_part == 1 && g_part_b_usable == 0) {
            DBG("[BOOT] WARNING: Active=B but B unavailable, forcing A\n");
            flag.active_part   = 0;
            flag.boot_fail_cnt = 0;
            PartFlag_Write(&flag);
            jump_addr    = PART_A_BASE;
            backup_addr  = PART_B_BASE;
        }

        if (flag.boot_fail_cnt >= BOOT_FAIL_MAX) {
            /* Safety: only switch to other partition if it's available */
            uint8_t other_part = (flag.active_part == 0) ? 1u : 0u;
            uint32_t other_usable = (other_part == 0)
                                    ? g_part_a_usable : g_part_b_usable;
            if (other_usable > 0) {
                DBG("[BOOT] Active part %d failed %d times, switching to %d\n",
                    (int)flag.active_part, (int)flag.boot_fail_cnt,
                    (int)other_part);
                flag.active_part    = other_part;
                flag.boot_fail_cnt  = 1;
                PartFlag_Write(&flag);
            } else {
                DBG("[BOOT] Active part %d failed, but other partition "
                    "unavailable — staying\n", (int)flag.active_part);
                /* Don't switch, but don't keep incrementing either */
                flag.boot_fail_cnt = BOOT_FAIL_MAX;
                PartFlag_Write(&flag);
                return;  /* Stay in bootloader for safety */
            }
            jump_addr   = (flag.active_part == 1) ? PART_B_BASE : PART_A_BASE;
            backup_addr = (flag.active_part == 0) ? PART_B_BASE : PART_A_BASE;
        } else {
            flag.boot_fail_cnt++;
            PartFlag_Write(&flag);
            DBG("[BOOT] Active part %d (attempt %d/%d) -> 0x%08X\n",
                (int)flag.active_part, (int)flag.boot_fail_cnt,
                BOOT_FAIL_MAX, (unsigned)jump_addr);
        }
        (void)backup_addr;
    }

    /* Firmware validity check
     *
     * CRITICAL: Invalidate D-cache before reading flash to ensure we get
     * fresh data, not stale cache lines from a previous boot/JUMP.
     *
     * 检查策略（宽松但安全）：
     *   1. 优先：FW_VALID_MAGIC ("BGPF") at offset 0xA4 → 最可靠
     *   2. 回退：首字为有效 NDS32 指令（非 0xFFFFFFFF 且非 PART_FLAG_MAGIC）
     *   3. 拒绝：首字为 PART_FLAG_MAGIC ("BGPW") → 说明该区域被分区标志污染
     */
    DataCacheInvalidAll();
    __nds32__dsb();

    /* Diagnostic: dump first word at key addresses */
    {
        volatile const uint32_t *pa = (volatile const uint32_t *)PART_A_BASE;
        volatile const uint32_t *pb = (volatile const uint32_t *)PART_B_BASE;
        volatile const uint32_t *pf = (volatile const uint32_t *)g_part_flag_addr;
        DBG("[BOOT] Flash dump: A@0x%08X=0x%08X  B@0x%08X=0x%08X  "
            "Flag@0x%08X=0x%08X\n",
            (unsigned)PART_A_BASE, (unsigned)*pa,
            (unsigned)PART_B_BASE, (unsigned)*pb,
            (unsigned)g_part_flag_addr, (unsigned)*pf);
    }

    {
        volatile const uint32_t *first_word =
            (volatile const uint32_t *)jump_addr;
        volatile const uint32_t *fw_magic =
            (volatile const uint32_t *)(jump_addr + FW_VALID_MAGIC_OFFSET);

        DBG("[BOOT] Checking firmware @ 0x%08X = 0x%08X (magic@0xA4=0x%08X)\n",
            (unsigned)jump_addr, (unsigned)*first_word,
            (unsigned)*fw_magic);

        /* Accept firmware if BGPF magic present at offset 0xA4 */
        int valid = (*fw_magic == FW_VALID_MAGIC);

        /* Fallback: accept if first word looks like a valid instruction
         * (non-empty AND not partition flag magic) */
        if (!valid && *first_word != 0xFFFFFFFFu &&
            *first_word != PART_FLAG_MAGIC) {
            valid = 1;
            DBG("[BOOT] No BGPF magic, but first word looks valid → accept\n");
        }

        if (!valid) {
            /* Try the other partition (only if it exists) */
            if (jump_addr == PART_B_BASE && g_part_a_usable > 0) {
                DBG("[BOOT] B not valid – trying A\n");
                flag.active_part     = 0;
                flag.boot_fail_cnt   = 0;
                PartFlag_Write(&flag);
                jump_addr = PART_A_BASE;
                first_word = (volatile const uint32_t *)PART_A_BASE;
                fw_magic = (volatile const uint32_t *)
                           (PART_A_BASE + FW_VALID_MAGIC_OFFSET);
                DBG("[BOOT] Checking A @ 0x%08X = 0x%08X (magic@0xA4=0x%08X)\n",
                    (unsigned)jump_addr, (unsigned)*first_word,
                    (unsigned)*fw_magic);

                valid = (*fw_magic == FW_VALID_MAGIC);
                if (!valid && *first_word != 0xFFFFFFFFu &&
                    *first_word != PART_FLAG_MAGIC) {
                    valid = 1;
                    DBG("[BOOT] A: no BGPF magic, but first word valid → accept\n");
                }
            } else if (jump_addr == PART_A_BASE && g_part_b_usable > 0) {
                DBG("[BOOT] A not valid – trying B\n");
                flag.active_part     = 1;
                flag.boot_fail_cnt   = 0;
                PartFlag_Write(&flag);
                jump_addr = PART_B_BASE;
                first_word = (volatile const uint32_t *)PART_B_BASE;
                fw_magic = (volatile const uint32_t *)
                           (PART_B_BASE + FW_VALID_MAGIC_OFFSET);
                DBG("[BOOT] Checking B @ 0x%08X = 0x%08X (magic@0xA4=0x%08X)\n",
                    (unsigned)jump_addr, (unsigned)*first_word,
                    (unsigned)*fw_magic);

                valid = (*fw_magic == FW_VALID_MAGIC);
                if (!valid && *first_word != 0xFFFFFFFFu &&
                    *first_word != PART_FLAG_MAGIC) {
                    valid = 1;
                    DBG("[BOOT] B: no BGPF magic, but first word valid → accept\n");
                }
            }
            if (!valid) {
                DBG("[BOOT] No valid firmware in any partition – "
                    "staying in bootloader\n");
                return;
            }
        }
    }

    /* Partition B remap */
    if (jump_addr == PART_B_BASE) {
        Remap_AddrRemapSet(ADDR_REMAP0, PART_A_BASE, PART_B_BASE,
                           (uint32_t)(PART_A_SIZE / 1024UL));
        jump_addr = PART_A_BASE;
        DBG("[BOOT] Remap 0x040000->0x240000, jumping to 0x040000 (Part B)\n");
    }

    DBG("[BOOT] Jumping to 0x%08X ...\n", (unsigned)jump_addr);
    Boot_JumpTo(jump_addr);   /* never returns when jumping */
#else
    /* Single-partition mode: check firmware validity, then jump to A.
     * If no valid firmware found, stay in bootloader for USB CDC upgrade. */
    {
        volatile const uint32_t *first_word =
            (volatile const uint32_t *)PART_A_BASE;
        volatile const uint32_t *fw_magic =
            (volatile const uint32_t *)(PART_A_BASE + FW_VALID_MAGIC_OFFSET);

        DataCacheInvalidAll();
        __nds32__dsb();

        DBG("[BOOT] Single-partition: checking firmware @ 0x%08X = 0x%08X "
            "(magic@0xA4=0x%08X)\n",
            (unsigned)PART_A_BASE, (unsigned)*first_word,
            (unsigned)*fw_magic);

        /* Accept firmware if BGPF magic present at offset 0xA4 */
        int valid = (*fw_magic == FW_VALID_MAGIC);

        /* Fallback: accept if first word looks like a valid instruction
         * (non-empty AND not partition flag magic) */
        if (!valid && *first_word != 0xFFFFFFFFu &&
            *first_word != PART_FLAG_MAGIC) {
            valid = 1;
            DBG("[BOOT] No BGPF magic, but first word looks valid → accept\n");
        }

        if (!valid) {
            DBG("[BOOT] No valid firmware at Part A — "
                "staying in bootloader for upgrade\n");
            return;  /* Fall through to USB CDC upgrade task */
        }

        DBG("[BOOT] Jumping to 0x%08X ...\n", (unsigned)PART_A_BASE);
        Boot_JumpTo(PART_A_BASE);
    }
#endif
}

/* =========================================================================
 * CDC channel I/O
 * ========================================================================= */
static uint16_t cdc_rx_read(uint8_t *buf, uint16_t max)
{
    return (OTG_DeviceCDC_GetRxCount() > 0)
           ? (uint16_t)OTG_DeviceCDC_Receive(buf, max)
           : 0u;
}

static void cdc_tx_write(const uint8_t *buf, uint16_t len)
{
    OTG_DeviceCDC_Send((uint8_t *)buf, len);
}

static int cdc_rx_available(void)
{
    return (OTG_DeviceCDC_GetRxCount() > 0) ? 1 : 0;
}

/* =========================================================================
 * CRC16-CCITT  (poly=0x1021, init=0xFFFF)
 * ========================================================================= */
static uint16_t calc_crc16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint8_t  i;
    while (len--) {
        crc ^= (uint16_t)(*buf++) << 8;
        for (i = 0; i < 8; i++)
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U)
                                   : (uint16_t)(crc << 1);
    }
    return crc;
}

/* =========================================================================
 * Transmit helpers
 * ========================================================================= */
#define TX_BUF_MAX  (1u + 1u + 2u + 2u + UPG_MAX_CHUNK + 4u + 2u)

static void send_pkt(uint8_t cmd, uint16_t seq,
                     const uint8_t *data, uint16_t dlen)
{
    uint8_t  buf[TX_BUF_MAX];
    uint16_t n = 0, crc;

    buf[n++] = UPG_SOF;
    buf[n++] = cmd;
    buf[n++] = (uint8_t)(seq >> 8);
    buf[n++] = (uint8_t)(seq);
    buf[n++] = (uint8_t)(dlen >> 8);
    buf[n++] = (uint8_t)(dlen);
    if (dlen && data) { memcpy(buf + n, data, dlen); }
    n = (uint16_t)(n + dlen);

    crc = calc_crc16(buf + 1, (uint16_t)(5u + dlen));
    buf[n++] = (uint8_t)(crc >> 8);
    buf[n++] = (uint8_t)(crc);
    cdc_tx_write(buf, n);
}

#define SEND_ACK(seq)          send_pkt(RSP_ACK,  seq, NULL, 0)
#define SEND_ACKD(seq, d, l)   send_pkt(RSP_ACK,  seq, d,    l)
#define SEND_NACK(seq, err)    do { uint8_t _e=(err); send_pkt(RSP_NACK, seq, &_e, 1); } while(0)

/* =========================================================================
 * Packet parser
 * ========================================================================= */
#define PKT_DATA_MAX  (UPG_MAX_CHUNK + 4u)

typedef struct {
    uint8_t  cmd;
    uint16_t seq;
    uint16_t len;
    uint8_t  data[PKT_DATA_MAX];
} UpgPkt_t;

typedef enum {
    PS_SOF, PS_CMD, PS_SEQ_H, PS_SEQ_L,
    PS_LEN_H, PS_LEN_L, PS_DATA, PS_CRC_H, PS_CRC_L
} ParserSt_t;

typedef struct {
    ParserSt_t st;
    uint16_t   di;
    uint8_t    crc_hi;
    UpgPkt_t   pkt;
} Parser_t;

static Parser_t g_parser;

static void parser_reset(void)
{
    g_parser.st     = PS_SOF;
    g_parser.di     = 0;
    g_parser.crc_hi = 0;
}

static int parser_verify(const UpgPkt_t *pkt, uint16_t recv_crc)
{
    uint8_t tmp[5u + PKT_DATA_MAX];
    tmp[0] = pkt->cmd;
    tmp[1] = (uint8_t)(pkt->seq >> 8);
    tmp[2] = (uint8_t)(pkt->seq);
    tmp[3] = (uint8_t)(pkt->len >> 8);
    tmp[4] = (uint8_t)(pkt->len);
    if (pkt->len) memcpy(tmp + 5, pkt->data, pkt->len);
    return (calc_crc16(tmp, (uint16_t)(5u + pkt->len)) == recv_crc) ? 1 : -1;
}

/* 1=packet ready, -1=CRC error, 0=need more bytes */
static int parse_poll(void)
{
    uint8_t b;
    while (cdc_rx_available()) {
        if (cdc_rx_read(&b, 1) != 1) break;
        switch (g_parser.st) {
        case PS_SOF:   if (b == UPG_SOF) g_parser.st = PS_CMD;                    break;
        case PS_CMD:   g_parser.pkt.cmd  = b;  g_parser.st = PS_SEQ_H;            break;
        case PS_SEQ_H: g_parser.pkt.seq  = (uint16_t)b << 8; g_parser.st = PS_SEQ_L; break;
        case PS_SEQ_L: g_parser.pkt.seq |= b;  g_parser.st = PS_LEN_H;            break;
        case PS_LEN_H: g_parser.pkt.len  = (uint16_t)b << 8; g_parser.st = PS_LEN_L; break;
        case PS_LEN_L:
            g_parser.pkt.len |= b; g_parser.di = 0;
            if (g_parser.pkt.len > PKT_DATA_MAX) { g_parser.st = PS_SOF; break; }
            g_parser.st = (g_parser.pkt.len == 0) ? PS_CRC_H : PS_DATA;
            break;
        case PS_DATA:
            g_parser.pkt.data[g_parser.di++] = b;
            if (g_parser.di >= g_parser.pkt.len) g_parser.st = PS_CRC_H;
            break;
        case PS_CRC_H: g_parser.crc_hi = b; g_parser.st = PS_CRC_L; break;
        case PS_CRC_L: {
            uint16_t recv = ((uint16_t)g_parser.crc_hi << 8) | b;
            g_parser.st = PS_SOF;
            return parser_verify(&g_parser.pkt, recv);
        }
        default: g_parser.st = PS_SOF; break;
        }
    }
    return 0;
}

/* =========================================================================
 * Flash helpers
 *
 * NOTE: 直接使用 SpiFlashErase/SpiFlashWrite 而不是 FlashErase 封装。
 *       FlashErase 内部会读取全局 flash 容量做边界检查，在 bootloader
 *       场景下该全局变量可能未被正确填充导致返回 ERASE_FLASH_ERR
 *       (参考 BT_Audio_APP/bt_obex_upgrade.c 的实现)。
 *
 *       IsSuspend=1：擦写过程可被 USB 中断暂停，避免 CDC host 超时断开。
 * ========================================================================= */
#define FLASH_BLOCK_SZ   0x10000UL   /* 64 KB block erase unit */

static void flash_service_usb(void)
{
    /* Service USB so CDC host doesn't disconnect mid-erase/write */
    OTG_DeviceRequestProcess();
    OTG_DeviceCDC_Task();
}

static int flash_erase(uint32_t offset, uint32_t size)
{
    uint32_t cur = offset;
    uint32_t end = offset + size;
    volatile uint32_t d;

    /* 4KB 对齐起点与终点 */
    cur &= ~(FLASH_SECTOR_SZ - 1u);
    end = (end + FLASH_SECTOR_SZ - 1u) & ~(FLASH_SECTOR_SZ - 1u);

    while (cur < end) {
        /* 优先 64KB block erase（要求 64KB 对齐且剩余 >= 64KB） */
        if (((cur & (FLASH_BLOCK_SZ - 1u)) == 0u) &&
            ((end - cur) >= FLASH_BLOCK_SZ)) {
            SpiFlashErase(BLOCK_ERASE, cur / FLASH_BLOCK_SZ, 1);
            cur += FLASH_BLOCK_SZ;
        } else {
            SpiFlashErase(SECTOR_ERASE, cur / FLASH_SECTOR_SZ, 1);
            cur += FLASH_SECTOR_SZ;
        }
        flash_service_usb();
        /* 短延时让 USB 状态机跑完 */
        d = 10000UL; while (d--);
    }
    return 1;
}

static int flash_write(uint32_t addr, const uint8_t *data, uint16_t len)
{
    return (SpiFlashWrite(addr, (uint8_t *)data, len, 1) == FLASH_NONE_ERR) ? 1 : 0;
}

/* =========================================================================
 * Upgrade session state
 * ========================================================================= */
typedef enum { UPG_IDLE, UPG_WRITING, UPG_DONE } UpgState_t;

static struct {
    UpgState_t state;
    uint32_t   total_size;
    uint32_t   written;
    uint32_t   fw_base;     /* write target base address */
    uint32_t   fw_max;      /* write target max size */
    uint8_t    target_part; /* 0=A, 1=B */
} g_session;

/* =========================================================================
 * Core upgrade state machine
 * ========================================================================= */
/* Helper: compute write target (backup partition) from current active partition.
 * In dual-partition mode, we always write to the partition that is NOT active.
 * If the backup partition is unavailable (e.g. B on a small flash),
 * fall back to writing to A (overwrite current firmware). */
static void upgrade_compute_target(void)
{
#if (BOOT_CURRENT_MODE == BOOT_MODE_DUAL_AB)
    PartFlag_t flag;
    if (PartFlag_Read(&flag) && flag.active_part == 1u) {
        /* Active = B → write to A (backup) */
        g_session.target_part = 0;
        g_session.fw_base     = PART_A_BASE;
        g_session.fw_max      = g_part_a_usable;
    } else {
        /* Active = A (or no valid flag) → write to B (backup) */
        g_session.target_part = 1;
        g_session.fw_base     = PART_B_BASE;
        g_session.fw_max      = g_part_b_usable;
        /* If B is unavailable (flash too small), fall back to A */
        if (g_part_b_usable == 0) {
            DBG("[UPG] WARNING: Part B unavailable, falling back to A\n");
            g_session.target_part = 0;
            g_session.fw_base     = PART_A_BASE;
            g_session.fw_max      = g_part_a_usable;
        }
    }
#else
    g_session.target_part = 0;
    g_session.fw_base     = PART_A_BASE;
    g_session.fw_max      = PART_A_SIZE;
#endif
    DBG("[UPG] Target: Part %c @ 0x%08X max=%u KB\n",
        g_session.target_part ? 'B' : 'A',
        (unsigned)g_session.fw_base,
        (unsigned)(g_session.fw_max / 1024));
}

static void upgrade_run(void)
{
    UpgPkt_t *pkt = &g_parser.pkt;
    uint32_t  offset;
    uint16_t  dlen;
    int       rc;

    rc = parse_poll();
    if (rc == 0) return;  /* no complete packet yet */
    if (rc < 0) {
        DBG("[UPG] CRC err\n");
        SEND_NACK(pkt->seq, UPG_ERR_CRC);
        return;
    }

    switch (pkt->cmd) {

    /* ── SYNC ─────────────────────────────────────────── */
    case CMD_SYNC: {
        uint8_t ver = UPG_VERSION;
        DBG("[UPG] SYNC\n");
        g_session.state      = UPG_IDLE;
        g_session.written    = 0;
        g_session.total_size = 0;
        upgrade_compute_target();  /* recompute backup partition */
        parser_reset();
        SEND_ACKD(pkt->seq, &ver, 1);
        break;
    }

    /* ── QUERY_INFO ──────────────────────────────────────────── */
    case CMD_QUERY_INFO: {
        PartFlag_t flag;
        DevInfo_t  info;
        memset(&info, 0, sizeof(info));
        info.protocol_ver = UPG_VERSION;
        info.boot_mode    = (PartFlag_GetCaps() & CAPS_DUAL_PART)
                            ? BOOT_MODE_DUAL_AB : BOOT_MODE_SINGLE;
        info.part_a_base  = PART_A_BASE;
        info.part_a_size  = g_part_a_usable;
        info.part_b_base  = PART_B_BASE;
        info.part_b_size  = g_part_b_usable;
        if (PartFlag_Read(&flag)) {
            info.active_part    = flag.active_part;
            info.boot_fail_cnt  = flag.boot_fail_cnt;
        } else {
            info.active_part = 0;
        }
        /* When the physical backup partition is unavailable (e.g. B on a
         * 2 MB flash), we report the EFFECTIVE backup partition info so
         * the host can proceed with the download.  The bootloader's
         * upgrade_compute_target() will redirect writes to the fallback. */
        if (info.active_part == 0 && g_part_b_usable == 0) {
            /* Active=A, B unavailable → effective backup is A */
            info.part_b_base = PART_A_BASE;
            info.part_b_size = g_part_a_usable;
            DBG("[UPG] QUERY_INFO: B unavailable, reporting A as backup\n");
        } else if (info.active_part == 1 && g_part_a_usable == 0) {
            /* Active=B, A unavailable → effective backup is B (unlikely) */
            info.part_a_base = PART_B_BASE;
            info.part_a_size = g_part_b_usable;
        }
        DBG("[UPG] QUERY_INFO: active=%d A=%uKB B=%uKB\n",
            (int)info.active_part,
            (unsigned)(info.part_a_size / 1024),
            (unsigned)(info.part_b_size / 1024));
        SEND_ACKD(pkt->seq, (uint8_t *)&info, (uint16_t)sizeof(info));
        break;
    }

    /* ── SET_PART ───────────────────────────────────────────────── */
    case CMD_SET_PART: {
        PartFlag_t flag;
        if (pkt->len < 1) { SEND_NACK(pkt->seq, UPG_ERR_PARAM); break; }
        /* Safety: reject partition switch if dual-partition not available */
        if (!(PartFlag_GetCaps() & CAPS_DUAL_PART)) {
            DBG("[UPG] SET_PART rejected: dual-partition not available\n");
            SEND_NACK(pkt->seq, UPG_ERR_CAPS); break;
        }
        if (!PartFlag_Read(&flag)) PartFlag_Default(&flag);
        flag.active_part    = pkt->data[0] ? 1u : 0u;
        flag.boot_fail_cnt  = 0;
        if (!PartFlag_Write(&flag)) {
            SEND_NACK(pkt->seq, UPG_ERR_FLASH); break;
        }
        DBG("[UPG] SET_PART -> %d\n", (int)flag.active_part);
        upgrade_compute_target();  /* recompute after partition change */
        SEND_ACK(pkt->seq);
        break;
    }

    /* ── GET_CAPS ─────────────────────────────────────────────── */
    case CMD_GET_CAPS: {
        CapInfo_t caps;
        memset(&caps, 0, sizeof(caps));
        caps.protocol_ver   = UPG_VERSION;
        caps.caps_flags     = PartFlag_GetCaps();
        caps.effective_mode = (caps.caps_flags & CAPS_DUAL_PART)
                              ? BOOT_MODE_DUAL_AB : BOOT_MODE_SINGLE;
        caps.reserved       = 0;
        caps.flash_capacity = g_flash_capacity;
        caps.part_a_usable  = g_part_a_usable;
        caps.part_b_usable  = g_part_b_usable;
        DBG("[UPG] GET_CAPS: flags=0x%02X mode=%d flash=%uKB A=%uKB B=%uKB\n",
            (unsigned)caps.caps_flags, (int)caps.effective_mode,
            (unsigned)(caps.flash_capacity / 1024),
            (unsigned)(caps.part_a_usable / 1024),
            (unsigned)(caps.part_b_usable / 1024));
        SEND_ACKD(pkt->seq, (uint8_t *)&caps, (uint16_t)sizeof(caps));
        break;
    }

    /* ── REBOOT ───────────────────────────────────────── */
    case CMD_REBOOT:
        DBG("[UPG] REBOOT\n");
        SEND_ACK(pkt->seq);
        Boot_JumpTo(0u);   /* jump to reset vector — reboots via boot ROM */
        break;

    /* ── ERASE ────────────────────────────────────────── */
    case CMD_ERASE:
        DBG("[UPG] ERASE base=0x%08X sz=0x%X\n",
            (unsigned)g_session.fw_base, (unsigned)g_session.fw_max);
        if (!flash_erase(g_session.fw_base, g_session.fw_max)) {
            SEND_NACK(pkt->seq, UPG_ERR_FLASH); break;
        }
        g_session.state = UPG_IDLE;
        SEND_ACK(pkt->seq);
        break;

    /* ── START ────────────────────────────────────────── */
    case CMD_START:
        if (pkt->len < 4) { SEND_NACK(pkt->seq, UPG_ERR_PARAM); break; }
        g_session.total_size = ((uint32_t)pkt->data[0] << 24)
                      | ((uint32_t)pkt->data[1] << 16)
                      | ((uint32_t)pkt->data[2] <<  8)
                      |  (uint32_t)pkt->data[3];
        DBG("[UPG] START total=%u to 0x%08X (part %c, max=%u KB)\n",
            (unsigned)g_session.total_size, (unsigned)g_session.fw_base,
            g_session.target_part ? 'B' : 'A',
            (unsigned)(g_session.fw_max / 1024));
        if (g_session.total_size == 0 || g_session.total_size > g_session.fw_max) {
            SEND_NACK(pkt->seq, UPG_ERR_SIZE); break;
        }
        if (!flash_erase(g_session.fw_base, g_session.total_size)) {
            SEND_NACK(pkt->seq, UPG_ERR_FLASH); break;
        }
        g_session.written = 0;
        g_session.state   = UPG_WRITING;
        SEND_ACK(pkt->seq);
        break;

    /* ── DATA ─────────────────────────────────────────── */
    case CMD_DATA:
        if (g_session.state != UPG_WRITING) {
            SEND_NACK(pkt->seq, UPG_ERR_STATE); break;
        }
        if (pkt->len < 5) { SEND_NACK(pkt->seq, UPG_ERR_PARAM); break; }
        offset = ((uint32_t)pkt->data[0] << 24)
               | ((uint32_t)pkt->data[1] << 16)
               | ((uint32_t)pkt->data[2] <<  8)
               |  (uint32_t)pkt->data[3];
        dlen   = (uint16_t)(pkt->len - 4u);
        if ((offset + dlen) > g_session.total_size) {
            SEND_NACK(pkt->seq, UPG_ERR_SIZE); break;
        }
        if (!flash_write(g_session.fw_base + offset, pkt->data + 4, dlen)) {
            SEND_NACK(pkt->seq, UPG_ERR_FLASH); break;
        }
        g_session.written += dlen;
        DBG("[UPG] DATA off=0x%X len=%u  %u/%u\n",
            (unsigned)offset, dlen,
            (unsigned)g_session.written, (unsigned)g_session.total_size);
        SEND_ACK(pkt->seq);
        break;

    /* ── FINISH ───────────────────────────────────────── */
    case CMD_FINISH: {
#if (BOOT_CURRENT_MODE == BOOT_MODE_DUAL_AB)
        PartFlag_t flag;
        if (g_session.state != UPG_WRITING) {
            SEND_NACK(pkt->seq, UPG_ERR_STATE); break;
        }
        DBG("[UPG] FINISH: verifying firmware write to Part %c @ 0x%08X\n",
            g_session.target_part ? 'B' : 'A', (unsigned)g_session.fw_base);

        /* Verify firmware was written correctly.
         * Lenient check: accept BGPF magic OR valid first word. */
        DataCacheInvalidAll();
        __nds32__dsb();
        {
            volatile const uint32_t *fw_magic_check =
                (volatile const uint32_t *)(g_session.fw_base + FW_VALID_MAGIC_OFFSET);
            volatile const uint32_t *fw_first =
                (volatile const uint32_t *)g_session.fw_base;
            int fw_ok = (*fw_magic_check == FW_VALID_MAGIC) ||
                        (*fw_first != 0xFFFFFFFFu &&
                         *fw_first != PART_FLAG_MAGIC);
            DBG("[UPG] Verify: @0x%08X=0x%08X magic@0xA4=0x%08X → %s\n",
                (unsigned)g_session.fw_base, (unsigned)*fw_first,
                (unsigned)*fw_magic_check, fw_ok ? "OK" : "FAIL");
            if (!fw_ok) {
                DBG("[UPG] FATAL: firmware write verification FAILED!\n");
                SEND_NACK(pkt->seq, UPG_ERR_FLASH); break;
            }
        }

        DBG("[UPG] FINISH: updating partition flags → active=%d\n",
            (int)g_session.target_part);
        if (!PartFlag_Read(&flag)) PartFlag_Default(&flag);
        flag.active_part     = g_session.target_part;  /* boot from written partition */
        flag.boot_fail_cnt   = 0u;
        if (!PartFlag_Write(&flag)) {
            SEND_NACK(pkt->seq, UPG_ERR_FLASH); break;
        }
        /* Verify partition flag did NOT corrupt firmware area */
        DataCacheInvalidAll();
        __nds32__dsb();
        {
            volatile const uint32_t *fw_first_after =
                (volatile const uint32_t *)g_session.fw_base;
            if (*fw_first_after == PART_FLAG_MAGIC ||
                *fw_first_after == 0xFFFFFFFFu) {
                DBG("[UPG] FATAL: PartFlag write corrupted firmware!\n");
                DBG("[UPG] Flag@0x%08X overwrote FW@0x%08X\n",
                    (unsigned)g_part_flag_addr, (unsigned)g_session.fw_base);
                SEND_NACK(pkt->seq, UPG_ERR_FLASH); break;
            }
        }
#else
        if (g_session.state != UPG_WRITING) {
            SEND_NACK(pkt->seq, UPG_ERR_STATE); break;
        }
        DBG("[UPG] FINISH: verifying firmware write to Part A @ 0x%08X\n",
            (unsigned)g_session.fw_base);

        /* Verify firmware was written correctly */
        DataCacheInvalidAll();
        __nds32__dsb();
        {
            volatile const uint32_t *fw_magic_check =
                (volatile const uint32_t *)(g_session.fw_base + FW_VALID_MAGIC_OFFSET);
            volatile const uint32_t *fw_first =
                (volatile const uint32_t *)g_session.fw_base;
            int fw_ok = (*fw_magic_check == FW_VALID_MAGIC) ||
                        (*fw_first != 0xFFFFFFFFu &&
                         *fw_first != PART_FLAG_MAGIC);
            DBG("[UPG] Verify: @0x%08X=0x%08X magic@0xA4=0x%08X → %s\n",
                (unsigned)g_session.fw_base, (unsigned)*fw_first,
                (unsigned)*fw_magic_check, fw_ok ? "OK" : "FAIL");
            if (!fw_ok) {
                DBG("[UPG] FATAL: firmware write verification FAILED!\n");
                SEND_NACK(pkt->seq, UPG_ERR_FLASH); break;
            }
        }
        DBG("[UPG] FINISH written=%u\n", (unsigned)g_session.written);
#endif
        g_session.state = UPG_DONE;
        SEND_ACK(pkt->seq);
        break;
    }

    /* ── JUMP ─────────────────────────────────────────── */
    case CMD_JUMP: {
        uint32_t jump_addr;
        DBG("[UPG] JUMP\n");
        SEND_ACK(pkt->seq);
#if (BOOT_CURRENT_MODE == BOOT_MODE_DUAL_AB)
        {
            PartFlag_t flag;
            if (PartFlag_Read(&flag) && flag.active_part == 1u)
                jump_addr = PART_B_BASE;
            else
                jump_addr = PART_A_BASE;
        }
        /* Partition B 需要设置硬件 Remap：
         * APP 固件链接在 0x040000 运行，Remap 将 0x040000 映射到 0x240000 */
        if (jump_addr == PART_B_BASE) {
            Remap_AddrRemapSet(ADDR_REMAP0, PART_A_BASE, PART_B_BASE,
                               (uint32_t)(PART_A_SIZE / 1024UL));
            jump_addr = PART_A_BASE;
            DBG("[UPG] Remap 0x040000->0x240000, jumping to 0x040000\n");
        }
#else
        jump_addr = PART_A_BASE;
#endif
        Boot_JumpTo(jump_addr);
        break;
    }

    default:
        DBG("[UPG] Unknown cmd 0x%02X\n", pkt->cmd);
        SEND_NACK(pkt->seq, UPG_ERR_PARAM);
        break;
    }
}

/* =========================================================================
 * Public API
 * ========================================================================= */
void Upgrade_Init(void)
{
    memset(&g_session, 0, sizeof(g_session));
    memset(&g_parser, 0, sizeof(g_parser));
    g_session.state = UPG_IDLE;
    upgrade_compute_target();  /* set initial write target */
    DBG("[UPG] Init OK (mode=%d, CDC only)\n", BOOT_CURRENT_MODE);
}

void Upgrade_Process(void)
{
    upgrade_run();
}

int Upgrade_IsActive(void)
{
    return (g_session.state == UPG_WRITING) ? 1 : 0;
}
