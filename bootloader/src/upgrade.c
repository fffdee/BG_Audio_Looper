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
    flag->active_part = 0;   /* boot A */
    part_flag_seal(flag);
}

int PartFlag_Write(const PartFlag_t *flag)
{
    PartFlag_t tmp;
    memcpy(&tmp, flag, sizeof(PartFlag_t));
    part_flag_seal(&tmp);          /* always re-seal before writing */

    if (FlashErase(PART_FLAG_ADDR, FLASH_SECTOR_SZ) != FLASH_NONE_ERR)
        return 0;
    return (SpiFlashWrite(PART_FLAG_ADDR, (uint8_t *)&tmp,
                         sizeof(PartFlag_t), 0) == FLASH_NONE_ERR) ? 1 : 0;
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
            diag_putc('?');  /* BootInfo magic mismatch! */
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

        if (flag.boot_fail_cnt >= BOOT_FAIL_MAX) {
            DBG("[BOOT] Active part %d failed %d times, switching to %d\n",
                (int)flag.active_part, (int)flag.boot_fail_cnt,
                (int)(1 - flag.active_part));
            flag.active_part    = (flag.active_part == 0) ? 1u : 0u;
            flag.boot_fail_cnt  = 1;
            PartFlag_Write(&flag);
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

    /* Firmware validity check */
    {
        volatile const uint32_t *magic_ptr =
            (volatile const uint32_t *)(jump_addr + FW_VALID_MAGIC_OFFSET);

        DBG("[BOOT] Checking magic @ 0x%08X = 0x%08X (expect 0x%08X)\n",
            (unsigned)(jump_addr + FW_VALID_MAGIC_OFFSET),
            (unsigned)*magic_ptr, (unsigned)FW_VALID_MAGIC);

        if (*magic_ptr != FW_VALID_MAGIC) {
            if (jump_addr != PART_A_BASE) {
                DBG("[BOOT] B partition has no firmware magic – "
                    "resetting flags to A\n");
                flag.active_part     = 0;
                flag.boot_fail_cnt   = 0;
                PartFlag_Write(&flag);
                jump_addr = PART_A_BASE;
                magic_ptr = (volatile const uint32_t *)
                            (PART_A_BASE + FW_VALID_MAGIC_OFFSET);
            }
            if (*magic_ptr != FW_VALID_MAGIC) {
                DBG("[BOOT] No valid firmware found (magic=0x%08X) – "
                    "staying in bootloader\n", (unsigned)*magic_ptr);
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
    /* Single-partition legacy mode: just jump to A */
    Boot_JumpTo(PART_A_BASE);
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
 * ========================================================================= */
static int flash_erase(uint32_t offset, uint32_t size)
{
    uint32_t aligned = (size + FLASH_SECTOR_SZ - 1u) & ~(FLASH_SECTOR_SZ - 1u);
    uint32_t cur = offset;
    uint32_t end = offset + aligned;
    volatile uint32_t d;

    while (cur < end) {
        if (FlashErase(cur, FLASH_SECTOR_SZ) != FLASH_NONE_ERR) return 0;
        cur += FLASH_SECTOR_SZ;
        /* Service USB so CDC host doesn't disconnect mid-erase */
        OTG_DeviceRequestProcess();
        OTG_DeviceCDC_Task();
        d = 10000UL; while (d--);
    }
    return 1;
}

static int flash_write(uint32_t addr, const uint8_t *data, uint16_t len)
{
    return (SpiFlashWrite(addr, (uint8_t *)data, len, 0) == FLASH_NONE_ERR) ? 1 : 0;
}

/* =========================================================================
 * Upgrade session state
 * ========================================================================= */
typedef enum { UPG_IDLE, UPG_WRITING, UPG_DONE } UpgState_t;

static struct {
    UpgState_t state;
    uint32_t   total_size;
    uint32_t   written;
} g_session;

/* =========================================================================
 * Core upgrade state machine
 * ========================================================================= */
static void upgrade_run(void)
{
    UpgPkt_t *pkt = &g_parser.pkt;
    uint32_t  offset;
    uint16_t  dlen;
    int       rc;

    /* Determine write-target base for this mode */
#if (BOOT_CURRENT_MODE == BOOT_MODE_DUAL_AB)
    const uint32_t fw_base = PART_B_BASE;
    const uint32_t fw_max  = PART_B_SIZE;
#else
    const uint32_t fw_base = PART_A_BASE;
    const uint32_t fw_max  = PART_A_SIZE;
#endif

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
        info.boot_mode    = BOOT_CURRENT_MODE;
        info.part_a_base  = PART_A_BASE;
        info.part_a_size  = PART_A_SIZE;
        info.part_b_base  = PART_B_BASE;
        info.part_b_size  = PART_B_SIZE;
        if (PartFlag_Read(&flag)) {
            info.active_part    = flag.active_part;
            info.boot_fail_cnt  = flag.boot_fail_cnt;
        } else {
            info.active_part = 0;
        }
        DBG("[UPG] QUERY_INFO\n");
        SEND_ACKD(pkt->seq, (uint8_t *)&info, (uint16_t)sizeof(info));
        break;
    }

    /* ── SET_PART ───────────────────────────────────────────────── */
    case CMD_SET_PART: {
        PartFlag_t flag;
        if (pkt->len < 1) { SEND_NACK(pkt->seq, UPG_ERR_PARAM); break; }
        if (!PartFlag_Read(&flag)) PartFlag_Default(&flag);
        flag.active_part    = pkt->data[0] ? 1u : 0u;
        flag.boot_fail_cnt  = 0;
        if (!PartFlag_Write(&flag)) {
            SEND_NACK(pkt->seq, UPG_ERR_FLASH); break;
        }
        DBG("[UPG] SET_PART -> %d\n", (int)flag.active_part);
        SEND_ACK(pkt->seq);
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
            (unsigned)fw_base, (unsigned)fw_max);
        if (!flash_erase(fw_base, fw_max)) {
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
        DBG("[UPG] START total=%u to 0x%08X\n",
            (unsigned)g_session.total_size, (unsigned)fw_base);
        if (g_session.total_size == 0 || g_session.total_size > fw_max) {
            SEND_NACK(pkt->seq, UPG_ERR_SIZE); break;
        }
        if (!flash_erase(fw_base, g_session.total_size)) {
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
        if (!flash_write(fw_base + offset, pkt->data + 4, dlen)) {
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
        DBG("[UPG] FINISH: updating partition flags\n");
        if (!PartFlag_Read(&flag)) PartFlag_Default(&flag);
        flag.active_part     = 1u;   /* USB CDC always writes to B; boot B */
        flag.boot_fail_cnt   = 0u;
        if (!PartFlag_Write(&flag)) {
            SEND_NACK(pkt->seq, UPG_ERR_FLASH); break;
        }
#else
        if (g_session.state != UPG_WRITING) {
            SEND_NACK(pkt->seq, UPG_ERR_STATE); break;
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
