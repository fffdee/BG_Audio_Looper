/**
 *****************************************************************************
 * @file     drv_init.c
 * @author   BG Card Team  
 * @version  V2.0.0
 * @date     04-January-2026
 * @brief    Driver framework initialization - register all hardware drivers
 *****************************************************************************
 */

#include "drv_init.h"
#include "vfs.h"
#include "drv_fs.h"
#include "drv_device.h"
#include "drv_st7735.h"
#include "drv_w25qxx.h"
#include "drv_battery.h"
#include "drv_usb_cdc.h"
#include "bt_vfs_driver.h"
#include "shell_fs.h"
#include "effect_graph.h"
#include "effect_graph_vfs.h"
#include "shell_cmd_audio_vfs.h"
#include "bg_flash_manager.h"
#include "BG_FlashMgr.h"
#include "debug.h"

/*******************************************************************************
 * Driver framework initialization functions
 ******************************************************************************/

/**
 * @brief  Initialize driver file system
 * @retval 0-success, <0-failure
 */
int DrvFramework_Init(void)
{
    int ret;
    
    /* 1. Initialize VFS core */
    ret = Vfs_Init();
    if (ret != VFS_OK) {
        DBG("[DrvInit] VFS init failed!\n");
        return -1;
    }
    
    /* 2. Initialize driver file system (create /driver directory) */
    ret = DrvFs_Init();
    if (ret != FS_OK) {
        DBG("[DrvInit] DrvFs init failed!\n");
        return -2;
    }
    
    /* 3. Initialize Shell file system (create /bin directory) */
    ret = ShellFs_Init();
    if (ret != VFS_OK) {
        DBG("[DrvInit] ShellFs init failed!\n");
        return -3;
    }
    
    /* 4. Initialize device management system */
    ret = DrvDevice_Init();
    if (ret != 0) {
        return -4;
    }
    
    return 0;
}

/**
 * @brief  Register all hardware drivers to framework
 * @retval 0-success, <0-failure
 * 
 * @note   Call sequence:
 *         1. DrvFramework_Init() - Initialize framework
 *         2. DrvFramework_RegisterAll() - Register all drivers
 *         3. Use Shell commands to view: drivers, ls /driver
 */
int DrvFramework_RegisterAll(void)
{
    int ret;
    int total = 0;
    int failed = 0;
    
    DBG("[DrvInit] Starting driver registration...\n");
    
    /* Initialize Flash manager (before other drivers that need Flash) */
    DBG("[DrvInit] Initializing Flash Manager...\n");
    BG_flash_manager.Init();
    total++;
    DBG("[DrvInit] Flash Manager initialized OK\n");
    
    /* Initialize BG_FlashMgr (application layer interface used by Looper) */
    DBG("[DrvInit] Initializing BG_FlashMgr...\n");
    BG_FlashMgr.Init();
    total++;
    DBG("[DrvInit] BG_FlashMgr initialized OK\n");
    
    /* Register ST7735 LCD driver */
    DBG("[DrvInit] Registering ST7735 LCD driver...\n");
    ret = St7735_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] ST7735 registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] ST7735 registration FAILED\n");
    }
    
    /* Register W25Qxx Flash driver */
    DBG("[DrvInit] Registering W25Qxx Flash driver...\n");
    ret = W25qxx_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] W25Qxx registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] W25Qxx registration FAILED\n");
    }
    
    /* Register battery management driver */
    DBG("[DrvInit] Registering Battery driver...\n");
    ret = Battery_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] Battery registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] Battery registration FAILED\n");
    }
    
    /* Register USB CDC driver */
    DBG("[DrvInit] Registering USB CDC driver...\n");
    ret = UsbCdc_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] USB CDC registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] USB CDC registration FAILED\n");
    }
    
    /* Initialize and mount Bluetooth devices to VFS */
    DBG("[DrvInit] Initializing Bluetooth VFS drivers...\n");
    
    /* Initialize BT driver */
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
    
    /* Initialize BLE driver */
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
    
    /* Register system commands to /bin */
    DBG("[DrvInit] Registering /bin commands...\n");
    ShellFs_RegisterAllCommands();
    DBG("[DrvInit] /bin commands registered OK\n");

    /* Initialize audio effect graph VFS (create /audio directory) */
    DBG("[DrvInit] Initializing Audio Graph VFS...\n");
    ret = EffectGraphVfs_MountDefault();
    if (ret == GRAPH_VFS_OK) {
        DBG("[DrvInit] Audio Graph VFS mounted OK\n");
    } else {
        DBG("[DrvInit] Audio Graph VFS mount deferred (graph not ready)\n");
    }
    
    /* Register audio VFS Shell commands */
#if USE_EFFECT_GRAPH_VFS
    ShellCmdAudioVfs_Register();
#endif

    /* Initialize Bluetooth VFS (create /bluetooth directory) */
    DBG("[DrvInit] Initializing Bluetooth VFS...\n");
    ret = BtVfsDriver_MountDefault();
    if (ret == BT_VFS_OK) {
        DBG("[DrvInit] Bluetooth VFS mounted OK\n");
    } else {
        DBG("[DrvInit] Bluetooth VFS mount deferred (bluetooth not ready)\n");
    }
    
    /* TODO: Add more driver registrations
     * - Audio Codec
     */
    
    DBG("[DrvInit] Registration complete: %d success, %d failed\n", total, failed);
    return (failed > 0) ? -1 : 0;
}

/**
 * @brief  Driver framework full initialization
 * @retval 0-success, <0-failure
 * 
 * @note   One-step completion: framework initialization + driver registration
 */
int DrvFramework_FullInit(void)
{
    int ret;
    
    ret = DrvFramework_Init();
    if (ret != 0) {
        DBG("[DrvInit] WARNING: VFS init failed, but continuing with Flash initialization...\n");
    }
    
    /* Even if VFS fails, initialize Flash manager (Flash does not depend on VFS)*/
    DBG("[DrvInit] Initializing Flash Managers (critical for audio looper)...\n");
    BG_flash_manager.Init();
    BG_FlashMgr.Init();
    DBG("[DrvInit] Flash Managers initialized OK\n");
    
    /* If VFS is ready, continue registering other drivers */
    if (ret == 0) {
        ret = DrvFramework_RegisterAll();
        if (ret != 0) {
            return ret;
        }
    }
    
    return 0;
}
