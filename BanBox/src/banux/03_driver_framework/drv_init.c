/**
 *****************************************************************************
 * @file     drv_init.c
 * @author   BG Card Team  
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    驱动框架初始化 - 注册所有硬件驱动
 *****************************************************************************
 */

#include "drv_init.h"
#include "drv_fs.h"
#include "drv_device.h"
#include "drv_st7735.h"
#include "drv_w25qxx.h"
#include "drv_battery.h"
#include "drv_usb_cdc.h"
#include "debug.h"  /* For DBG macro */

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
    
    /* 1. 初始化驱动文件系统 */
    ret = DrvFs_Init();
    if (ret != 0) {
        return -1;
    }
    
    /* 2. 初始化设备管理系统 */
    ret = DrvDevice_Init();
    if (ret != 0) {
        return -2;
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
    
    /* TODO: 添加更多驱动注册
     * - Audio Codec
     * - Bluetooth
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
