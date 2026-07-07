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
#include "flash_bus.h"
#include "product_def.h"
#include "debug.h"

typedef int (*DrvStaticRegisterFn_t)(void);
typedef int (*DrvStaticProbeFn_t)(void);

typedef struct {
    const char             *name;
    DrvStaticRegisterFn_t   register_fn;
    DrvStaticProbeFn_t      probe_fn;
} DrvStaticEntry_t;

/*******************************************************************************
 * 静态注册表探测函数
 ******************************************************************************/

static int DrvProbeValidId(uint8_t mfg_id, uint8_t dev_id)
{
    return (mfg_id != 0x00u && mfg_id != 0xFFu &&
            dev_id != 0x00u && dev_id != 0xFFu);
}

static int DrvProbeW25n02Id(uint8_t dev_id)
{
    return (dev_id == W25N02_DEV_ID || dev_id == 0xAAu);
}

static int DrvProbeNorDevice(const char *name)
{
    FlashDevice_t *dev = FlashBus_GetDeviceByName(name);

    if (!dev || !dev->initialized || dev->type != FLASH_TYPE_NOR) {
        return 0;
    }

    switch (dev->info.dev_id) {
        case W25QXX_DEV_Q32:
        case W25QXX_DEV_Q64:
        case W25QXX_DEV_Q128:
        case W25QXX_DEV_Q256:
            return DrvProbeValidId(dev->info.mfg_id, dev->info.dev_id);
        default:
            return 0;
    }
}

static int DrvProbeW25qxx(void)
{
#if HW_FLASH0_EN
    if (DrvProbeNorDevice("flash0_sys")) {
        return 1;
    }
#endif

#if HW_FLASH1_EN
    if (DrvProbeNorDevice("flash1_stor")) {
        return 1;
    }
#endif

    return 0;
}

static int DrvProbeW25n02(void)
{
    FlashDevice_t *dev = FlashBus_GetDeviceByName("nand0");

    return (dev && dev->initialized &&
            dev->type == FLASH_TYPE_NAND &&
            DrvProbeValidId(dev->info.mfg_id, dev->info.dev_id) &&
            DrvProbeW25n02Id(dev->info.dev_id));
}

static int DrvProbePsram(void)
{
    FlashDevice_t *dev = FlashBus_GetDeviceByName("psram0");

    return (dev && dev->initialized &&
            dev->type == FLASH_TYPE_PSRAM &&
            DrvProbeValidId(dev->info.mfg_id, dev->info.dev_id) &&
            dev->info.dev_id == PSRAM64H_KNOWN_KGD);
}

static int DrvProbeSdCard(void)
{
    FlashDevice_t *dev = FlashBus_GetDeviceByName("sdcard0");

    return (dev && dev->initialized &&
            dev->type == FLASH_TYPE_SDCARD &&
            dev->info.block_count > 0u &&
            dev->info.block_size > 0u);
}

static int DrvRegisterBtVfs(void)
{
    VfsNode_t *driverDir;
    VfsNode_t *btNode;
    int ret;

    ret = BtVfs_Init();
    if (ret != 0) {
        return ret;
    }

    driverDir = Vfs_FindNode("/driver");
    if (!driverDir) {
        DBG("[DrvInit] ERROR: /driver not found\n");
        return -1;
    }

    btNode = BtVfs_Mount(driverDir);
    return btNode ? 0 : -1;
}

static int DrvRegisterBleVfs(void)
{
    VfsNode_t *driverDir;
    VfsNode_t *bleNode;
    int ret;

    ret = BleVfs_Init();
    if (ret != 0) {
        return ret;
    }

    driverDir = Vfs_FindNode("/driver");
    if (!driverDir) {
        DBG("[DrvInit] ERROR: /driver not found\n");
        return -1;
    }

    bleNode = BleVfs_Mount(driverDir);
    return bleNode ? 0 : -1;
}

static const DrvStaticEntry_t g_drv_static_table[] = {
#if HW_DRV_FLASH_NOR_EN
    { "W25Qxx NOR Flash", W25qxx_DrvRegister, DrvProbeW25qxx },
#endif
#if HW_DRV_FLASH_NAND_EN
    { "W25N02 NAND Flash", W25n02_DrvRegister, DrvProbeW25n02 },
#endif
#if HW_DRV_PSRAM_EN
    { "ESP-PSRAM64H", Psram_DrvRegister, DrvProbePsram },
#endif
#if HW_DRV_SDCARD_EN
    { "SD Card", SDCard_DrvRegister, DrvProbeSdCard },
#endif
#if HW_DRV_BATTERY_EN
    { "Battery", Battery_DrvRegister, NULL },
#endif
#if HW_DRV_USB_CDC_EN
    { "USB CDC", UsbCdc_DrvRegister, NULL },
#endif
#if HW_DRV_BT_EN
    { "Bluetooth", DrvRegisterBtVfs, NULL },
    { "BLE", DrvRegisterBleVfs, NULL },
#endif
    { NULL, NULL, NULL }
};

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
    int skipped = 0;
    unsigned int i;
    
    DBG("[DrvInit] Starting driver registration...\n");

    /* FlashDevices_Init() is idempotent and provides the probed ID/status data
     * used by the static driver table below. */
#if HW_DRV_FLASH_NOR_EN || HW_DRV_FLASH_NAND_EN || HW_DRV_PSRAM_EN || HW_DRV_SDCARD_EN
    ret = FlashDevices_Init();
    if (ret != FLASH_OK) {
        DBG("[DrvInit] FlashDevices_Init failed: %d\n", ret);
    }
#endif

    for (i = 0; i < (unsigned int)(sizeof(g_drv_static_table) / sizeof(g_drv_static_table[0])); i++) {
        const DrvStaticEntry_t *entry = &g_drv_static_table[i];

        if (!entry->register_fn) {
            continue;
        }

        if (entry->probe_fn && !entry->probe_fn()) {
            skipped++;
            DBG("[DrvInit] %s not detected, skip registration\n", entry->name);
            continue;
        }

        DBG("[DrvInit] Registering %s driver...\n", entry->name);
        ret = entry->register_fn();
        if (ret == 0) {
            total++;
            DBG("[DrvInit] %s registered OK\n", entry->name);
        } else {
            failed++;
            DBG("[DrvInit] %s registration FAILED (ret=%d)\n", entry->name, ret);
        }
    }
    
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
    
    DBG("[DrvInit] Registration complete: %d success, %d skipped, %d failed\n",
        total, skipped, failed);
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
