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
#include "drv_st7735.h"
#include "drv_w25qxx.h"
#include "drv_battery.h"
#include "drv_usb_cdc.h"
#include "bt_vfs_driver.h"
#include "shell_fs.h"
#include "effect_graph_vfs.h"
#include "shell_cmd_audio_vfs.h"
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
    
    /* 注册ST7735 LCD驱动 */
    DBG("[DrvInit] Registering ST7735 LCD driver...\n");
    ret = St7735_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] ST7735 registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] ST7735 registration FAILED\n");
    }
    
    /* 注册W25Qxx Flash驱动 */
    DBG("[DrvInit] Registering W25Qxx Flash driver...\n");
    ret = W25qxx_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] W25Qxx registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] W25Qxx registration FAILED\n");
    }
    
    /* 注册电池管理驱动 */
    DBG("[DrvInit] Registering Battery driver...\n");
    ret = Battery_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] Battery registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] Battery registration FAILED\n");
    }
    
    /* 注册USB CDC驱动 */
    DBG("[DrvInit] Registering USB CDC driver...\n");
    ret = UsbCdc_DrvRegister();
    if (ret == 0) {
        total++;
        DBG("[DrvInit] USB CDC registered OK\n");
    } else {
        failed++;
        DBG("[DrvInit] USB CDC registration FAILED\n");
    }
    
    /* 初始化并挂载蓝牙设备到VFS */
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
    
    /* 注册系统命令到 /bin */
    DBG("[DrvInit] Registering /bin commands...\n");
    ShellFs_RegisterAllCommands();
    DBG("[DrvInit] /bin commands registered OK\n");

    /* 初始化音频效果图VFS（创建/audio目录） */
    DBG("[DrvInit] Initializing Audio Graph VFS...\n");
    ret = EffectGraphVfs_MountDefault();
    if (ret == GRAPH_VFS_OK) {
        DBG("[DrvInit] Audio Graph VFS mounted OK\n");
    } else {
        DBG("[DrvInit] Audio Graph VFS mount deferred (graph not ready)\n");
    }
    
    /* 注册audio VFS Shell命令 */
    ShellCmdAudioVfs_Register();

    /* 初始化蓝牙VFS（创建/bluetooth目录） */
    DBG("[DrvInit] Initializing Bluetooth VFS...\n");
    ret = BtVfsDriver_MountDefault();
    if (ret == BT_VFS_OK) {
        DBG("[DrvInit] Bluetooth VFS mounted OK\n");
    } else {
        DBG("[DrvInit] Bluetooth VFS mount deferred (bluetooth not ready)\n");
    }
    
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
        return ret;
    }
    
    ret = DrvFramework_RegisterAll();
    if (ret != 0) {
        return ret;
    }
    
    return 0;
}
