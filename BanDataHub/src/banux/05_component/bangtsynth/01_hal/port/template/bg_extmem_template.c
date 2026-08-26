/**
 * @file bg_extmem_template.c
 * @brief 移植模板：无外部 PSRAM 时，src/工作区都失败或指向同一块 SRAM。
 *
 * 有 SPI-PSRAM 时：read/write 调你的 PSRAM 驱动；
 * 音源已在同一块 RAM 时：src_read 与 read 相同。
 */
#ifndef BG_PORT_TEMPLATE
#define BG_PORT_TEMPLATE 0
#endif
#if BG_PORT_TEMPLATE

#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_extmem.h"
#include <string.h>

#ifndef BG_EXTMEM_SRAM_BYTES
#define BG_EXTMEM_SRAM_BYTES  0
#endif

#if BG_EXTMEM_SRAM_BYTES > 0
static uint8_t s_extmem[BG_EXTMEM_SRAM_BYTES];
#endif

int bg_extmem_ready(void)
{
#if BG_EXTMEM_SRAM_BYTES > 0
    return 1;
#else
    return 0;
#endif
}

int bg_extmem_read(uint32_t addr, void *buf, uint32_t len)
{
#if BG_EXTMEM_SRAM_BYTES > 0
    if (!buf || addr + len > BG_EXTMEM_SRAM_BYTES) {
        return -1;
    }
    memcpy(buf, s_extmem + addr, len);
    return 0;
#else
    (void)addr;
    (void)buf;
    (void)len;
    return -1;
#endif
}

int bg_extmem_write(uint32_t addr, const void *buf, uint32_t len)
{
#if BG_EXTMEM_SRAM_BYTES > 0
    if (!buf || addr + len > BG_EXTMEM_SRAM_BYTES) {
        return -1;
    }
    memcpy(s_extmem + addr, buf, len);
    return 0;
#else
    (void)addr;
    (void)buf;
    (void)len;
    return -1;
#endif
}

int bg_extmem_src_read(uint32_t addr, void *buf, uint32_t len)
{
    return bg_extmem_read(addr, buf, len);
}

#endif /* BANGTSYNTH_EN */
#endif /* BG_PORT_TEMPLATE */
