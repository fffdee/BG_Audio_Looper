/**
 *****************************************************************************
 * @file     drv_init.c
 * @author   BG Card Team  
 * @version  V2.0.0
 * @date     04-January-2026
 * @brief    驱动框架初始化 - 注册所有硬件驱动
 *****************************************************************************
 */

#include "drv_init.h"
#include "vfs.h"
#include "drv_fs.h"
#include "drv_device.h"
#include "drv_psram.h"
#include "drv_sdcard.h"
#include "drv_usb_cdc.h"
#include "shell_fs.h"
#include "bg_flash_manager.h"
#include "BG_FlashMgr.h"
#include "flash_devices.h"
#include "debug.h"
#include "product_def.h"

#if HW_DRV_LCD_EN
#include "drv_st7735.h"
#endif

#if HW_DRV_FLASH_NOR_EN
#include "drv_w25qxx.h"
#endif

#if HW_DRV_FLASH_NAND_EN
#include "drv_w25n02.h"
#endif

#if HW_DRV_BATTERY_EN
#include "drv_battery.h"
#endif

#if HW_DRV_ENCODER_EN
#include "drv_encoder.h"
#endif

#if HW_DRV_BT_EN
#include "bt_vfs_driver.h"
#endif

#if EFFECT_GRAPHICS_EN
#include "effect_graph.h"
#include "effect_graph_vfs.h"
#include "shell_cmd_audio_vfs.h"
#endif

int DrvFramework_Init(void)
{
    int ret;
    
    ret = Vfs_Init();
    if (ret != VFS_OK) {
        DBG("[DrvInit] VFS init failed!\n");
        return -1;
    }
    
    ret = DrvFs_Init();
    if (ret != FS_OK) {
        DBG("[DrvInit] DrvFs init failed!\n");
        return -2;
    }
    
    ret = ShellFs_Init();
    if (ret != VFS_OK) {
        DBG("[DrvInit] ShellFs init failed!\n");
        return -3;
    }
    
    ret = DrvDevice_Init();
    if (ret != 0) {
        return -4;
    }
    
    return 0;
}

int DrvFramework_RegisterAll(void)
{
    int ret;
    int total = 0;
    int failed = 0;
    
    DBG("[DrvInit] Starting driver registration...\n");
    
#if HW_DRV_LCD_EN
    DBG("[DrvInit] Registering ST7735 LCD driver...\n");
    ret = St7735_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] ST7735 registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] ST7735 registration FAILED\n");
    }
#endif

#if HW_DRV_SSD1306_EN
    DBG("[DrvInit] SSD1306 OLED driver - direct init (no VFS registration needed)\n");
    total++;
#endif

#if HW_DRV_ENCODER_EN
    DBG("[DrvInit] Registering Rotary Encoder driver...\n");
    ret = Encoder_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] Rotary Encoder registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] Rotary Encoder registration FAILED\n");
    }
#endif

#if HW_DRV_FLASH_NOR_EN
    DBG("[DrvInit] Registering W25Qxx Flash driver...\n");
    ret = W25qxx_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] W25Qxx registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] W25Qxx registration FAILED\n");
    }
#endif

#if HW_DRV_FLASH_NAND_EN
    DBG("[DrvInit] Registering W25N02 NAND Flash driver...\n");
    ret = W25n02_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] W25N02 NAND registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] W25N02 NAND registration FAILED\n");
    }
#endif

#if HW_DRV_PSRAM_EN
    DBG("[DrvInit] Registering ESP-PSRAM64H driver...\n");
    ret = Psram_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] ESP-PSRAM64H registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] ESP-PSRAM64H registration FAILED\n");
    }
#endif

#if HW_DRV_SDCARD_EN
    DBG("[DrvInit] Registering SD Card driver...\n");
    ret = SDCard_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] SD Card registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] SD Card registration FAILED\n");
    }
#endif

#if HW_DRV_BATTERY_EN
    DBG("[DrvInit] Registering Battery driver...\n");
    ret = Battery_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] Battery registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] Battery registration FAILED\n");
    }
#endif

#if HW_DRV_USB_CDC_EN
    DBG("[DrvInit] Registering USB CDC driver...\n");
    ret = UsbCdc_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] USB CDC registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] USB CDC registration FAILED\n");
    }
#endif

#if HW_DRV_BT_EN
    DBG("[DrvInit] Initializing Bluetooth VFS drivers...\n");
    ret = BtVfs_Init();
    if (ret == 0) {
        VfsNode_t *driverDir = Vfs_FindNode("/driver");
        if (driverDir) {
            VfsNode_t *btNode = BtVfs_Mount(driverDir);
            if (btNode) {
                total++;
                DBG("[DrvInit] BT device mounted at /driver/bt\n");
            } else {
                failed++;
                DBG("[DrvInit] BT mount FAILED\n");
            }
        } else {
            failed++;
            DBG("[DrvInit] ERROR: /driver not found\n");
        }
    } else {
        failed++;
        DBG("[DrvInit] BT init FAILED\n");
    }
    
    ret = BleVfs_Init();
    if (ret == 0) {
        VfsNode_t *driverDir = Vfs_FindNode("/driver");
        if (driverDir) {
            VfsNode_t *bleNode = BleVfs_Mount(driverDir);
            if (bleNode) {
                total++;
                DBG("[DrvInit] BLE device mounted at /driver/ble\n");
            } else {
                failed++;
                DBG("[DrvInit] BLE mount FAILED\n");
            }
        } else {
            failed++;
            DBG("[DrvInit] ERROR: /driver not found\n");
        }
    } else {
        failed++;
        DBG("[DrvInit] BLE init FAILED\n");
    }
#endif

    DBG("[DrvInit] Registering /bin commands...\n");
    ShellFs_RegisterAllCommands();
    DBG("[DrvInit] /bin commands registered OK\n");

#if EFFECT_GRAPHICS_EN
    DBG("[DrvInit] Initializing Audio Graph VFS...\n");
    ret = EffectGraphVfs_MountDefault();
    if (ret == GRAPH_VFS_OK) {
        DBG("[DrvInit] Audio Graph VFS mounted OK\n");
    } else {
        DBG("[DrvInit] Audio Graph VFS mount deferred (graph not ready)\n");
    }
#if USE_EFFECT_GRAPH_VFS
    ShellCmdAudioVfs_Register();
#endif
#endif

#if !HW_DRV_BT_EN
    DBG("[DrvInit] Bluetooth VFS skipped (HW_DRV_BT_EN=0)\n");
#endif
    
    DBG("[DrvInit] Registration complete: %d success, %d failed\n", total, failed);
    return (failed > 0) ? -1 : 0;
}

int DrvFramework_FullInit(void)
{
    int ret;
    
    ret = DrvFramework_Init();
    if (ret != 0) {
        DBG("[DrvInit] WARNING: VFS init failed, but continuing with Flash initialization...\n");
    }
    
    DBG("[DrvInit] Initializing Flash Managers...\n");
    BG_flash_manager.Init();
    /* BG_FlashMgr.Init() 移到 power_on() 中调用，因为 SD 卡初始化需要 vTaskDelay()，
       在 RTOS 调度器启动前调用会导致死锁 */
    DBG("[DrvInit] Flash Manager (bg_flash_manager) initialized OK\n");
    DBG("[DrvInit] BG_FlashMgr init deferred to power_on() (requires RTOS)\n");
    
    if (ret == 0) {
        ret = DrvFramework_RegisterAll();
        if (ret != 0) {
            return ret;
        }
    }
    
    return 0;
}
