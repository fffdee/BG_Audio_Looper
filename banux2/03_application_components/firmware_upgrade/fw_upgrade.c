/**
 * @file  fw_upgrade.c
 * @brief BanUX firmware upgrade component facade.
 */

#include "fw_upgrade.h"
#include "app_upgrade.h"
#include "cdc_upgrade.h"
#include "dual_partition.h"
#include "spi_flash.h"
#include "reset.h"
#include "debug.h"
#include "irqn.h"
#include "core_d1088.h"
#include <nds32_intrinsic.h>

void FwUpgrade_BootInit(void)
{
    DualPart_Init();
    Boot_CheckAndJump();
}

void FwUpgrade_ConfirmBootSuccess(void)
{
    Boot_ConfirmSuccess();
}

void FwUpgrade_Init(void)
{
    App_Upgrade_Init();
    CDC_Upgrade_Init();
}

void FwUpgrade_EnterCdcMode(void)
{
    CDC_Upgrade_EnterMode();
}

int FwUpgrade_InCdcMode(void)
{
    return CDC_Upgrade_InMode();
}

int FwUpgrade_CheckCdcEnter(void)
{
    return CDC_Upgrade_CheckEnter();
}

void FwUpgrade_ProcessCdc(void)
{
    CDC_Upgrade_Process();
}

int FwUpgrade_IsActive(void)
{
    return CDC_Upgrade_IsActive();
}

int FwUpgrade_GetInfo(FwUpgradeInfo_t *info)
{
    PartFlag_t flags;
    const DualPart_Layout_t *layout;
    int flags_valid;

    if (!info) {
        return 0;
    }

    layout = DualPart_GetLayout();
    flags_valid = PartFlag_Read(&flags);

    info->part_a_base = PART_A_BASE;
    info->part_a_size = layout->part_a_usable;
    info->part_b_base = PART_B_BASE;
    info->part_b_size = layout->part_b_usable;
    info->flags_addr = layout->part_flag_addr;
    info->flags_valid = (uint8_t)(flags_valid ? 1u : 0u);
    info->active_part = flags_valid ? flags.active_part : 0u;
    info->boot_fail_cnt = flags_valid ? flags.boot_fail_cnt : 0u;
    info->boot_fail_max = BOOT_FAIL_MAX;
    info->running_part_b = (uint8_t)(Boot_IsRunningPart2() ? 1u : 0u);

    return flags_valid;
}

void FwUpgrade_RebootToBootloader(void)
{
    uint32_t magic = BURN_FLAG_MAGIC;
    uint32_t rb_mmio = 0;
    uint32_t rb_spi = 0;

    GIE_DISABLE();
    SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3);
    (void)FlashErase(BURN_FLAG_ADDR, FLASH_SECTOR_SZ);
    (void)SpiFlashWrite(BURN_FLAG_ADDR, (uint8_t *)&magic, sizeof(magic), 0);
    (void)SpiFlashRead(BURN_FLAG_ADDR, (uint8_t *)&rb_spi, sizeof(rb_spi), 100);
    DataCacheInvalidAll();
    __nds32__dsb();
    rb_mmio = *(volatile const uint32_t *)BURN_FLAG_ADDR;
    GIE_ENABLE();

    if (rb_spi != magic && rb_mmio != magic) {
        DBG("[BOOT] burn flag verify FAILED spi=0x%08X mmio=0x%08X\n",
            (unsigned)rb_spi, (unsigned)rb_mmio);
        return;
    }

    Reset_McuSystem();
}

uint32_t FwUpgrade_GetBootloaderFlag(void)
{
    uint32_t val = 0;

    if (SpiFlashRead(BURN_FLAG_ADDR, (uint8_t *)&val, sizeof(val), 100) != FLASH_NONE_ERR)
        return *(volatile const uint32_t *)BURN_FLAG_ADDR;
    return val;
}

int FwUpgrade_IsBootloaderFlagSet(void)
{
    return (FwUpgrade_GetBootloaderFlag() == BURN_FLAG_MAGIC) ? 1 : 0;
}
