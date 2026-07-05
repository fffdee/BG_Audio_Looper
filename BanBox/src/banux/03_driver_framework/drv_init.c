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
#include "drv_w25qxx.h"
#include "drv_w25n02.h"
#include "drv_psram.h"
#include "drv_sdcard.h"
#include "drv_battery.h"
#include "drv_usb_cdc.h"
#include "bt_vfs_driver.h"
#include "shell_fs.h"
#include "bg_flash_manager.h"
#include "BG_FlashMgr.h"
#include "flash_devices.h"
#include "debug.h"

/*******************************************************************************
 * 驱动框架初始化函数
 ******************************************************************************/

/**
 * @brief  初始化驱动文件系统
 * @retval 0-成功, <0-失败
 */
int DrvFramework_Init(void)
{
    int ret;
    
    /* 1. 初始化VFS核心 */
    ret = Vfs_Init();
    if (ret != VFS_OK) {
        DBG("[DrvInit] VFS init failed!\n");
        return -1;
    }
    
    /* 2. 初始化驱动文件系统（创建/driver目录） */
    ret = DrvFs_Init();
    if (ret != FS_OK) {
        DBG("[DrvInit] DrvFs init failed!\n");
        return -2;
    }
    
    /* 3. 初始化Shell文件系统（创建/bin目录） */
    ret = ShellFs_Init();
    if (ret != VFS_OK) {
        DBG("[DrvInit] ShellFs init failed!\n");
        return -3;
    }
    
    /* 4. 初始化设备管理系统 */
    ret = DrvDevice_Init();
    if (ret != 0) {
        return -4;
    }
    
    return 0;
}

/**
 * @brief  注册所有硬件驱动到框架
 * @retval 0-成功, <0-失败
 * 
 * @note   调用顺序:
 *         1. DrvFramework_Init() - 初始化框架
 *         2. DrvFramework_RegisterAll() - 注册所有驱动
 *         3. 使用Shell命令查看: drivers, ls /driver
 */
int DrvFramework_RegisterAll(void)
{
    int ret;
    int total = 0;
    int failed = 0;
    
    DBG("[DrvInit] Starting driver registration...\n");

    /* Flash 管理器已在 DrvFramework_FullInit() 中初始化，此处跳过 */

    /* 注册NOR Flash驱动 (W25Qxx) */
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
#endif /* HW_DRV_FLASH_NOR_EN */

/* 注册NAND Flash驱动 (W25N02) */
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
#endif /* HW_DRV_FLASH_NAND_EN */
    
/* 注册PSRAM驱动 (ESP-PSRAM64H) */
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
#endif /* HW_DRV_PSRAM_EN */

/* 注册SD Card驱动 */
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
#endif /* HW_DRV_SDCARD_EN */
    
    /* 注册电池管理驱动 */
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
#endif /* HW_DRV_BATTERY_EN */
    
    /* 注册USB CDC驱动 */
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
#endif /* HW_DRV_USB_CDC_EN */
    
    /* 初始化并挂载蓝牙设备到VFS */
#if HW_DRV_BT_EN
    DBG("[DrvInit] Initializing Bluetooth VFS drivers...\n");
    
    /* 初始化BT驱动 */
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
    
    /* 初始化BLE驱动 */
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
#endif /* HW_DRV_BT_EN */
    
    /* 注册系统命令到 /bin */
    DBG("[DrvInit] Registering /bin commands...\n");
    ShellFs_RegisterAllCommands();
    DBG("[DrvInit] /bin commands registered OK\n");

    /* EffectGraph VFS 和 ShellCmdAudioVfs 初始化已移至 main.c（05_component 层），
     * 解耦 03_driver_framework 对 05_component 的直接依赖 */

    /* 初始化蓝牙VFS（创建/bluetooth目录） */
    /* 注意：BT/BLE设备在应用启动后再初始化，这里跳过以避免卡住 */
    DBG("[DrvInit] Initializing Bluetooth VFS...\n");
    /* 临时跳过BtVfsDriver_MountDefault()以防止初始化卡住 */
    /* ret = BtVfsDriver_MountDefault();
    if (ret == BT_VFS_OK) {
        DBG("[DrvInit] Bluetooth VFS mounted OK\n");
    } else {
        DBG("[DrvInit] Bluetooth VFS mount deferred (bluetooth not ready)\n");
    } */
    DBG("[DrvInit] Bluetooth VFS deferred (will init after scheduler starts)\n");
    
    /* TODO: 添加更多驱动注册
     * - Audio Codec
     */
    
    DBG("[DrvInit] Registration complete: %d success, %d failed\n", total, failed);
    return (failed > 0) ? -1 : 0;
}

/**
 * @brief  驱动框架完整初始化
 * @retval 0-成功, <0-失败
 * 
 * @note   一步完成：框架初始化 + 驱动注册
 */
int DrvFramework_FullInit(void)
{
    int ret;
    
    ret = DrvFramework_Init();
    if (ret != 0) {
        DBG("[DrvInit] WARNING: VFS init failed, but continuing with Flash initialization...\n");
    }
    
    /* 即使VFS失败，也要初始化Flash管理器（Flash不依赖VFS）*/
    DBG("[DrvInit] Initializing Flash Managers (critical for audio looper)...\n");
    BG_flash_manager.Init();
    BG_FlashMgr.Init();
    DBG("[DrvInit] Flash Managers initialized OK\n");
    
    /* 如果VFS已就绪，继续注册其他驱动 */
    if (ret == 0) {
        ret = DrvFramework_RegisterAll();
        if (ret != 0) {
            return ret;
        }
    }
    
    return 0;
}
