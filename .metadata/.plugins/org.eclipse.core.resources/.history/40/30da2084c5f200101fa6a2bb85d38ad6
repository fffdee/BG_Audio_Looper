/**
 *****************************************************************************
 * @file     drv_fs.c
 * @author   BG Card Team
 * @version  V2.0.0
 * @date     04-January-2026
 * @brief    驱动文件系统适配层实现 - 创建/driver目录结构
 *****************************************************************************
 */

#include "drv_fs.h"
#include "debug.h"

/*******************************************************************************
 * 静态变量 - 驱动目录快捷访问指针
 ******************************************************************************/
static FsNode_t *g_DriverDir = NULL;
static FsNode_t *g_SpiDir = NULL;
static FsNode_t *g_I2cDir = NULL;
static FsNode_t *g_I2sDir = NULL;
static FsNode_t *g_SdioDir = NULL;
static FsNode_t *g_PowerDir = NULL;
static FsNode_t *g_UsbDir = NULL;
static bool      g_DrvFsInitialized = FALSE;

/*******************************************************************************
 * 公共API实现
 ******************************************************************************/

FsError_t DrvFs_Init(void)
{
    if (g_DrvFsInitialized) return FS_OK;
    
    FsNode_t *root = Vfs_GetRoot();
    if (!root) {
        DBG("[DrvFs] ERROR: VFS not initialized!\n");
        return FS_ERR_NOT_FOUND;
    }
    
    DBG("[DrvFs] Creating /driver directory structure...\n");
    
    /* 创建 /driver */
    g_DriverDir = Vfs_CreateDir(root, "driver");
    if (!g_DriverDir) {
        DBG("[DrvFs] ERROR: Failed to create /driver\n");
        return FS_ERR_NO_MEMORY;
    }
    
    /* 创建子目录 */
    g_SpiDir = Vfs_CreateDir(g_DriverDir, "spi");
    if (!g_SpiDir) return FS_ERR_NO_MEMORY;
    
    g_I2cDir = Vfs_CreateDir(g_DriverDir, "i2c");
    if (!g_I2cDir) return FS_ERR_NO_MEMORY;
    
    g_I2sDir = Vfs_CreateDir(g_DriverDir, "i2s");
    if (!g_I2sDir) return FS_ERR_NO_MEMORY;
    
    g_SdioDir = Vfs_CreateDir(g_DriverDir, "sdio");
    if (!g_SdioDir) return FS_ERR_NO_MEMORY;
    
    g_PowerDir = Vfs_CreateDir(g_DriverDir, "power");
    if (!g_PowerDir) return FS_ERR_NO_MEMORY;
    
    g_UsbDir = Vfs_CreateDir(g_DriverDir, "usb");
    if (!g_UsbDir) return FS_ERR_NO_MEMORY;
    
    DBG("[DrvFs] /driver structure created successfully\n");
    g_DrvFsInitialized = TRUE;
    return FS_OK;
}

FsNode_t* DrvFs_GetDriverDir(void)
{
    return g_DriverDir;
}

FsNode_t* DrvFs_GetSpiDir(void)
{
    return g_SpiDir;
}

FsNode_t* DrvFs_GetI2cDir(void)
{
    return g_I2cDir;
}

FsNode_t* DrvFs_GetI2sDir(void)
{
    return g_I2sDir;
}

FsNode_t* DrvFs_GetSdioDir(void)
{
    return g_SdioDir;
}

FsNode_t* DrvFs_GetPowerDir(void)
{
    return g_PowerDir;
}

FsNode_t* DrvFs_GetUsbDir(void)
{
    return g_UsbDir;
}
