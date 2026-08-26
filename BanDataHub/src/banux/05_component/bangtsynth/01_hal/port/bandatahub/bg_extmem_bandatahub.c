/**
 * @file bg_extmem_bandatahub.c
 * @brief BanDataHub: 工作 RAM 与音源源均为板级 PSRAM
 */
#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_extmem.h"
#include "flash_devices.h"

static FlashDevice_t *psram_dev(void)
{
    return FlashDevices_GetPsramFlash();
}

#if BG_CFG_HAS_NAND
static FlashDevice_t *src_dev(void)
{
    return FlashDevices_GetNandFlash();
}
#else
static FlashDevice_t *src_dev(void)
{
    return FlashDevices_GetPsramFlash();
}
#endif

int bg_extmem_ready(void)
{
    FlashDevice_t *dev = psram_dev();
    return (dev && dev->ops && dev->ops->read) ? 1 : 0;
}

int bg_extmem_read(uint32_t addr, void *buf, uint32_t len)
{
    FlashDevice_t *dev = psram_dev();
    if (!dev || !dev->ops || !dev->ops->read || !buf || len == 0) {
        return -1;
    }
    return (dev->ops->read(dev, addr, (uint8_t *)buf, len) == 0) ? 0 : -1;
}

int bg_extmem_write(uint32_t addr, const void *buf, uint32_t len)
{
    FlashDevice_t *dev = psram_dev();
    if (!dev || !dev->ops || !dev->ops->write || !buf || len == 0) {
        return -1;
    }
    return (dev->ops->write(dev, addr, (const uint8_t *)buf, len) == 0) ? 0 : -1;
}

int bg_extmem_src_read(uint32_t addr, void *buf, uint32_t len)
{
    FlashDevice_t *dev = src_dev();
    if (!dev || !dev->ops || !dev->ops->read || !buf || len == 0) {
        return -1;
    }
    return (dev->ops->read(dev, addr, (uint8_t *)buf, len) == 0) ? 0 : -1;
}

#endif /* BANGTSYNTH_EN */
