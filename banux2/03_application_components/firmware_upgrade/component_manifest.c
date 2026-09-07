#include "banux_component.h"
#include "fw_upgrade.h"

#ifndef BANUX_APP_CDC_UPGRADE_AUTO_ENTER
#define BANUX_APP_CDC_UPGRADE_AUTO_ENTER 0
#endif

static int FirmwareUpgrade_ComponentInit(void)
{
    FwUpgrade_Init();
    return 0;
}

static void FirmwareUpgrade_ComponentProcess(void)
{
#if BANUX_APP_CDC_UPGRADE_AUTO_ENTER
    if (FwUpgrade_InCdcMode() || FwUpgrade_CheckCdcEnter()) {
        FwUpgrade_ProcessCdc();
    }
#else
    if (FwUpgrade_InCdcMode()) {
        FwUpgrade_ProcessCdc();
    }
#endif
}

BANUX_COMPONENT_DEFINE_EX(g_banux_component_firmware_upgrade,
                          "firmware_upgrade", "1.0.0",
                          BANUX_COMPONENT_APPLICATION, 1,
                          "application firmware upgrade engine",
                          FirmwareUpgrade_ComponentInit,
                          FirmwareUpgrade_ComponentProcess);
