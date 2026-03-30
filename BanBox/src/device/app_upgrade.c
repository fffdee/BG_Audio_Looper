/**
 * @file  app_upgrade.c
 * @brief User-firmware BLE OTA engine + partition management.
 *
 * Design
 * 鈹€鈹€鈹€鈹€鈹€鈹€
 * The bootloader carries NO BT stack.  All BLE OTA logic lives here.
 *
 * Upgrade flow (equal-peer A/B):
 *   Running on partition X (active_part=X).
 *   Backup partition Y = 1-X.
 *
 *   1. SYNC   鈫?ACK with OTA_VERSION
 *   2. ERASE  鈫?erase backup partition Y in flash
 *   3. START  鈫?validate size 鈮?partition size
 *   4. DATA   鈫?write 256-byte chunks to Y at PART_Y_BASE + offset
 *   5. FINISH 鈫?check FW_VALID_MAGIC at Y+0xA4, update partition flags
 *               (active_part = Y, boot_fail_cnt = 0), then reboot.
 *   Bootloader reads new active_part = Y, applies remap if needed, jumps.
 *
 * Packet format (shared with bootloader USB CDC protocol):
 *   [SOF:1][CMD:1][SEQ:2][LEN:2][DATA:N][CRC16:2]
 *   CRC16-CCITT (poly=0x1021, init=0xFFFF) over CMD+SEQ+LEN+DATA
 */

#include <string.h>
#include <nds32_intrinsic.h>
#include "app_upgrade.h"
#include "spi_flash.h"
#include "watchdog.h"
#include "debug.h"

/* 鈹€鈹€ Flash layout (MUST match bootloader/src/upgrade.h) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
#define PART_A_BASE       0x00040000UL
#define PART_A_SIZE       0x00200000UL   /* 2 MB */
#define PART_B_BASE       0x00240000UL
#define PART_B_SIZE       0x00200000UL   /* 2 MB */
#define PART_FLAG_ADDR    0x00440000UL
#define PART_FLAG_MAGIC   0x42475057UL   /* "BGPW" */
#define FLASH_SECTOR_SZ   0x1000UL       /* 4 KB   */

/* 鈹€鈹€ Firmware validity signature (MUST match bootloader/src/upgrade.h) 鈹€鈹€鈹€鈹€ */
/* Vector table = (9 exception + 32 HW) 脳 4B = 0xA4B.                       */
/* .stub_section starts at partition_base + 0xA4.                            */
#define FW_VALID_MAGIC        0x42475046UL  /* "BGPF" */
#define FW_VALID_MAGIC_OFFSET 0x000000A4UL

/* Place the magic word so the bootloader can validate this firmware image. */
#ifdef BOOTLOADER_EN
__attribute__((section(".stub_section")))
__attribute__((used))
static const uint32_t fw_valid_magic = FW_VALID_MAGIC;
#endif /* BOOTLOADER_EN */

/* 鈹€鈹€ Partition flag structure (MUST match bootloader/src/upgrade.h) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
typedef struct {
    uint32_t magic;
    uint8_t  active_part;     /* 0=A running, 1=B running               */
    uint8_t  reserved1;
    uint8_t  boot_fail_cnt;
    uint8_t  reserved2;
    uint32_t crc32;
} PartFlag_t;

/* 鈹€鈹€ CRC32 IEEE 802.3 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
static uint32_t crc32_calc(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL, i, j;
    for (i = 0; i < len; i++) {
        crc ^= (uint32_t)buf[i];
        for (j = 0; j < 8u; j++)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFUL;
}

static int part_flag_valid(const PartFlag_t *f)
{
    if (f->magic != PART_FLAG_MAGIC) return 0;
    return (crc32_calc((const uint8_t *)f,
                sizeof(PartFlag_t) - sizeof(uint32_t)) == f->crc32) ? 1 : 0;
}

static void part_flag_seal(PartFlag_t *f)
{
    f->magic = PART_FLAG_MAGIC;
    f->crc32 = crc32_calc((const uint8_t *)f,
                           sizeof(PartFlag_t) - sizeof(uint32_t));
}

static int part_flag_write(const PartFlag_t *f)
{
    PartFlag_t tmp;
    memcpy(&tmp, f, sizeof(PartFlag_t));
    part_flag_seal(&tmp);
    if (FlashErase(PART_FLAG_ADDR, FLASH_SECTOR_SZ) != FLASH_NONE_ERR)
        return 0;
    return (SpiFlashWrite(PART_FLAG_ADDR, (uint8_t *)&tmp,
                          sizeof(PartFlag_t), 0) == FLASH_NONE_ERR) ? 1 : 0;
}

/* 鈹€鈹€ Which partition is currently running? 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
/* Determined at compile time via the link address.
 * If we are linked at PART_A_BASE we are running from A (or via remap鈫払). */
static uint8_t current_active_part(void)
{
    PartFlag_t flag;
    memcpy(&flag, (const void *)PART_FLAG_ADDR, sizeof(PartFlag_t));
    if (part_flag_valid(&flag))
        return flag.active_part;
    return 0;  /* default: A */
}

static uint32_t backup_part_base(void)
{
    return (current_active_part() == 0) ? PART_B_BASE : PART_A_BASE;
}

static uint32_t backup_part_size(void)
{
    return PART_A_SIZE;  /* both partitions are the same size */
}

/* 鈹€鈹€ CRC16-CCITT 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
static uint16_t crc16(const uint8_t *buf, uint16_t len)
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

/* 鈹€鈹€ OTA packet parser state 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
typedef enum {
    PS_SOF = 0, PS_CMD, PS_SEQ_H, PS_SEQ_L,
    PS_LEN_H, PS_LEN_L, PS_DATA, PS_CRC_H, PS_CRC_L
} ParseState_t;

#define OTA_RX_BUF   512U   /* max incoming packet incl. header + CRC       */

typedef struct {
    ParseState_t state;
    uint8_t      buf[OTA_RX_BUF];
    uint16_t     pos;
    uint16_t     seq;
    uint16_t     data_len;
} OtaParser_t;

/* 鈹€鈹€ OTA engine state 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
typedef enum {
    OTA_IDLE = 0,
    OTA_ERASING,
    OTA_READY,
    OTA_WRITING,
    OTA_COMMIT_PENDING,
} OtaEngineState_t;

static struct {
    OtaEngineState_t  eng_state;
    OtaParser_t       parser;
    void (*send)(const uint8_t *data, uint16_t len);
    uint32_t          total_size;
    uint32_t          written;
    uint8_t           do_erase;   /* deferred to App_OTA_Process */
    uint8_t           do_commit;  /* deferred to App_OTA_Process */
} g_ota;

/* 鈹€鈹€ Transmit helpers 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
static void ota_send_ack(uint16_t seq, const uint8_t *payload, uint16_t plen)
{
    uint8_t  buf[8 + plen];
    uint16_t total_len = plen;
    uint16_t crc_val;
    uint16_t idx = 0;

    buf[idx++] = OTA_SOF;
    buf[idx++] = OTA_RSP_ACK;
    buf[idx++] = (uint8_t)(seq >> 8);
    buf[idx++] = (uint8_t)(seq & 0xFF);
    buf[idx++] = (uint8_t)(total_len >> 8);
    buf[idx++] = (uint8_t)(total_len & 0xFF);
    if (plen && payload)
        memcpy(buf + idx, payload, plen);
    idx += plen;
    crc_val = crc16(buf + 1, (uint16_t)(idx - 1));
    buf[idx++] = (uint8_t)(crc_val >> 8);
    buf[idx++] = (uint8_t)(crc_val & 0xFF);
    if (g_ota.send)
        g_ota.send(buf, idx);
}

static void ota_send_nack(uint16_t seq, uint8_t err)
{
    uint8_t  buf[8];
    uint16_t crc_val;
    uint16_t idx = 0;

    buf[idx++] = OTA_SOF;
    buf[idx++] = OTA_RSP_NACK;
    buf[idx++] = (uint8_t)(seq >> 8);
    buf[idx++] = (uint8_t)(seq & 0xFF);
    buf[idx++] = 0;
    buf[idx++] = 1;
    buf[idx++] = err;
    crc_val = crc16(buf + 1, (uint16_t)(idx - 1));
    buf[idx++] = (uint8_t)(crc_val >> 8);
    buf[idx++] = (uint8_t)(crc_val & 0xFF);
    if (g_ota.send)
        g_ota.send(buf, idx);
}

/* 鈹€鈹€ Packet dispatch 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
static void ota_dispatch(uint8_t cmd, uint16_t seq,
                          const uint8_t *data, uint16_t dlen)
{
    switch (cmd) {

    case OTA_CMD_SYNC: {
        uint8_t ver = OTA_VERSION;
        g_ota.eng_state = OTA_IDLE;
        g_ota.written   = 0;
        g_ota.total_size = 0;
        DBG("[OTA] SYNC\n");
        ota_send_ack(seq, &ver, 1);
        break;
    }

    case OTA_CMD_QUERY: {
        PartFlag_t flag;
        uint8_t    rsp[4];
        memcpy(&flag, (const void *)PART_FLAG_ADDR, sizeof(PartFlag_t));
        rsp[0] = OTA_VERSION;
        rsp[1] = part_flag_valid(&flag) ? flag.active_part : 0u;
        rsp[2] = part_flag_valid(&flag) ? flag.boot_fail_cnt : 0u;
        rsp[3] = 0;
        DBG("[OTA] QUERY active=%d\n", (int)rsp[1]);
        ota_send_ack(seq, rsp, sizeof(rsp));
        break;
    }

    case OTA_CMD_ERASE: {
        /* Request deferred erase (flash ops are slow; do in Process()) */
        DBG("[OTA] ERASE backup @ 0x%08X\n",
            (unsigned)backup_part_base());
        g_ota.eng_state = OTA_ERASING;
        g_ota.do_erase  = 1;
        ota_send_ack(seq, NULL, 0);  /* ACK immediately; erase is async */
        break;
    }

    case OTA_CMD_START: {
        uint32_t sz;
        if (dlen < 4) { ota_send_nack(seq, OTA_ERR_PARAM); break; }
        sz = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
           | ((uint32_t)data[2] <<  8) |  (uint32_t)data[3];
        if (sz == 0 || sz > backup_part_size()) {
            DBG("[OTA] START: bad size %lu\n", (unsigned long)sz);
            ota_send_nack(seq, OTA_ERR_SIZE); break;
        }
        g_ota.total_size = sz;
        g_ota.written    = 0;
        g_ota.eng_state  = OTA_WRITING;
        DBG("[OTA] START size=%lu\n", (unsigned long)sz);
        ota_send_ack(seq, NULL, 0);
        break;
    }

    case OTA_CMD_DATA: {
        uint32_t offset;
        uint16_t chunk;
        if (g_ota.eng_state != OTA_WRITING) {
            ota_send_nack(seq, OTA_ERR_STATE); break;
        }
        if (dlen < 5) { ota_send_nack(seq, OTA_ERR_PARAM); break; }
        offset = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
               | ((uint32_t)data[2] <<  8) |  (uint32_t)data[3];
        chunk  = (uint16_t)(dlen - 4);
        if (offset + chunk > backup_part_size()) {
            ota_send_nack(seq, OTA_ERR_SIZE); break;
        }
        if (SpiFlashWrite(backup_part_base() + offset,
                          (uint8_t *)(data + 4), chunk, 0) != FLASH_NONE_ERR) {
            DBG("[OTA] DATA write fail @ offset %lu\n", (unsigned long)offset);
            ota_send_nack(seq, OTA_ERR_FLASH); break;
        }
        g_ota.written += chunk;
        ota_send_ack(seq, NULL, 0);
        break;
    }

    case OTA_CMD_FINISH: {
        volatile const uint32_t *magic_ptr =
            (volatile const uint32_t *)(backup_part_base() + FW_VALID_MAGIC_OFFSET);
        if (*magic_ptr != FW_VALID_MAGIC) {
            DBG("[OTA] FINISH: firmware magic not found (0x%08X)\n",
                (unsigned)*magic_ptr);
            ota_send_nack(seq, OTA_ERR_FLASH); break;
        }
        DBG("[OTA] FINISH: magic OK, committing flags and rebooting\n");
        g_ota.eng_state = OTA_COMMIT_PENDING;
        g_ota.do_commit = 1;
        ota_send_ack(seq, NULL, 0);  /* ACK before reboot */
        break;
    }

    default:
        ota_send_nack(seq, OTA_ERR_STATE);
        break;
    }
}

/* 鈹€鈹€ Parser 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
static void parser_feed(const uint8_t *in, uint16_t len)
{
    OtaParser_t *p = &g_ota.parser;
    uint16_t i;

    for (i = 0; i < len; i++) {
        uint8_t b = in[i];
        switch (p->state) {
        case PS_SOF:
            if (b == OTA_SOF) { p->pos = 0; p->buf[p->pos++] = b;
                                p->state = PS_CMD; }
            break;
        case PS_CMD:
            p->buf[p->pos++] = b; p->state = PS_SEQ_H; break;
        case PS_SEQ_H:
            p->buf[p->pos++] = b; p->seq = (uint16_t)(b << 8);
            p->state = PS_SEQ_L; break;
        case PS_SEQ_L:
            p->buf[p->pos++] = b; p->seq |= b; p->state = PS_LEN_H; break;
        case PS_LEN_H:
            p->buf[p->pos++] = b; p->data_len = (uint16_t)(b << 8);
            p->state = PS_LEN_L; break;
        case PS_LEN_L:
            p->buf[p->pos++] = b; p->data_len |= b;
            if (p->data_len == 0) p->state = PS_CRC_H;
            else if (p->data_len >= OTA_RX_BUF - 8u) p->state = PS_SOF;
            else p->state = PS_DATA;
            break;
        case PS_DATA:
            p->buf[p->pos++] = b;
            if ((p->pos - 6u) >= p->data_len) p->state = PS_CRC_H;
            break;
        case PS_CRC_H:
            p->buf[p->pos++] = b; p->state = PS_CRC_L; break;
        case PS_CRC_L: {
            uint16_t recv_crc, calc_crc;
            p->buf[p->pos++] = b;
            recv_crc = (uint16_t)((p->buf[p->pos - 2] << 8) | b);
            /* CRC covers bytes [1 .. pos-3] (CMD through DATA) */
            calc_crc = crc16(p->buf + 1, (uint16_t)(p->pos - 3));
            if (recv_crc != calc_crc) {
                DBG("[OTA] CRC error recv=0x%04X calc=0x%04X\n",
                    recv_crc, calc_crc);
                ota_send_nack(p->seq, OTA_ERR_CRC);
            } else {
                ota_dispatch(p->buf[1], p->seq,
                             p->buf + 6, p->data_len);
            }
            p->state = PS_SOF;
            break;
        }
        }
    }
}

/* =========================================================================
 *  Public API
 * ========================================================================= */

void App_OTA_Init(void (*send_fn)(const uint8_t *data, uint16_t len))
{
    memset(&g_ota, 0, sizeof(g_ota));
    g_ota.send = send_fn;
    DBG("[OTA] Engine initialised\n");
}

void App_OTA_OnData(const uint8_t *data, uint16_t len)
{
    parser_feed(data, len);
}

void App_OTA_Process(void)
{
    if (g_ota.do_erase) {
        uint32_t base = backup_part_base();
        uint32_t size = backup_part_size();
        uint32_t addr;
        DBG("[OTA] Erasing backup partition 0x%08X size 0x%08X\n",
            (unsigned)base, (unsigned)size);
        g_ota.do_erase = 0;
        for (addr = base; addr < base + size; addr += FLASH_SECTOR_SZ) {
            if (FlashErase(addr, FLASH_SECTOR_SZ) != FLASH_NONE_ERR) {
                DBG("[OTA] Erase failed @ 0x%08X\n", (unsigned)addr);
                g_ota.eng_state = OTA_IDLE;
                return;
            }
        }
        g_ota.eng_state = OTA_READY;
        g_ota.written   = 0;
        DBG("[OTA] Erase done\n");
    }

    if (g_ota.do_commit) {
        /* Flip active_part: backup becomes active, old active becomes backup */
        PartFlag_t flag;
        uint8_t    new_active;
        g_ota.do_commit = 0;

        memcpy(&flag, (const void *)PART_FLAG_ADDR, sizeof(PartFlag_t));
        if (!part_flag_valid(&flag))
            memset(&flag, 0, sizeof(flag));

        new_active       = (flag.active_part == 0) ? 1u : 0u;
        flag.active_part = new_active;
        flag.boot_fail_cnt = 0;
        part_flag_seal(&flag);   /* sets magic + crc32 */

        if (!part_flag_write(&flag)) {
            DBG("[OTA] COMMIT: flag write failed!\n");
            g_ota.eng_state = OTA_IDLE;
            return;
        }
        DBG("[OTA] COMMIT: active_part now %d, rebooting...\n",
            (int)new_active);

        /* Short delay so the BLE ACK drains before reset */
        {
            volatile uint32_t d = 500000UL;
            while (d--);
        }

        /* Clean reset: disable WDG stall, disable interrupts, jump to 0 */
        WDG_Disable();
        __nds32__setgie_dis();
        {
            typedef void (*Entry_t)(void);
            ((Entry_t)0)();
        }
        while (1);
    }
}

/* =========================================================================
 *  App_ConfirmBootSuccess
 *  Called on clean startup to reset boot_fail_cnt.
 * ========================================================================= */
void App_ConfirmBootSuccess(void)
{
    PartFlag_t flag;
    memcpy(&flag, (const void *)PART_FLAG_ADDR, sizeof(PartFlag_t));
    if (!part_flag_valid(&flag)) {
        DBG("[APP] No valid partition flag – skip boot confirm\n");
        return;
    }
    if (flag.boot_fail_cnt == 0) return;  /* already clean */

    flag.boot_fail_cnt = 0;
    if (!part_flag_write(&flag))
        DBG("[APP] ERROR: boot confirm write failed\n");
    else
        DBG("[APP] Boot confirmed (active=%d)\n", (int)flag.active_part);
}
