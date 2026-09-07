/**
 * @file  driver_init.c
 * @brief BanEffector 平台驱动注册 (BanuxDriver_RegisterAll).
 *
 * 注册纳入 Banux DrvDevice/VFS 框架的板级存储与外设驱动, 由 product_def.h
 * 的 HW_DRV_* 开关门控. BanEffector(BANBOX_1_0_V2) 启用: USB_CDC / NAND /
 * PSRAM / BATTERY; 关闭: NOR / SDCARD.
 * 注意: 音频(ADC/DAC/I2S 与效果图)与 BLE 不属于 DrvDevice 驱动, 其初始化在
 * main.c 的 platformInit 回调中完成, 不在此注册.
 */
#include "driver_init.h"
#include "banux_config.h"
#include "debug.h"

#if HW_DRV_USB_CDC_EN
#include "drv_usb_cdc.h"
#endif
#if HW_DRV_FLASH_NOR_EN
#include "drv_w25qxx.h"
#endif
#if HW_DRV_FLASH_NAND_EN
#include "drv_w25n02.h"
#endif
#if HW_DRV_PSRAM_EN
#include "drv_psram.h"
#endif
#if HW_DRV_SDCARD_EN
#include "drv_sdcard.h"
#endif
#if HW_DRV_BATTERY_EN
#include "drv_battery.h"
#endif

int BanuxDriver_RegisterAll(void)
{
    int failures = 0;

#if HW_DRV_USB_CDC_EN
    if (UsbCdc_DrvRegister() != 0) failures++;
#endif
#if HW_DRV_FLASH_NOR_EN
    if (W25qxx_DrvRegister() != 0) failures++;
#endif
#if HW_DRV_FLASH_NAND_EN
    if (W25n02_DrvRegister() != 0) failures++;
#endif
#if HW_DRV_PSRAM_EN
    if (Psram_DrvRegister() != 0) failures++;
#endif
#if HW_DRV_SDCARD_EN
    if (SDCard_DrvRegister() != 0) failures++;
#endif
#if HW_DRV_BATTERY_EN
    if (Battery_DrvRegister() != 0) failures++;
#endif

    DBG("[DriverInit] Platform drivers registered, failures=%d\n", failures);
    return failures ? -failures : 0;
}
