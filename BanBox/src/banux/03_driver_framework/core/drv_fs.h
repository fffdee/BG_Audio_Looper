/**
 *****************************************************************************
 * @file     drv_fs.h
 * @author   BG Card Team
 * @version  V2.0.0
 * @date     04-January-2026
 * @brief    Driver file system adaptation layer - VFS-based driver directory management
 *****************************************************************************
 * @attention
 *
 * This module is VFS driver layer adaptation, providing:
 * 1. /driver directory and subdirectory management
 * 2. Driver parameter node registration
 * 3. Backward compatible API (DrvFs_* maps to Vfs_*)
 *
 * Directory structure:
 *   /driver
 *       ├── spi
 *       │   ├── st7735
 *       │   └── w25q64
 *       ├── i2c
 *       ├── i2s
 *       ├── sdio
 *       ├── power
 *       └── usb
 *
 *****************************************************************************
 */

#ifndef __DRV_FS_H__
#define __DRV_FS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "vfs.h"

/*******************************************************************************
 * Type definitions (backward compatible API)
 ******************************************************************************/
typedef VfsNode_t       FsNode_t;
typedef VfsNodeType_t   FsNodeType_t;
typedef VfsError_t      FsError_t;
typedef VfsParamGet_t   FsParamGet_t;
typedef VfsParamSet_t   FsParamSet_t;
typedef VfsListCallback_t FsListCallback_t;

/* Node type compatibility definitions */
#define FS_NODE_DIR     VFS_NODE_DIR
#define FS_NODE_PARAM   VFS_NODE_PARAM
#define FS_NODE_DEV     VFS_NODE_DEV

/* Error code compatibility definitions */
#define FS_OK                   VFS_OK
#define FS_ERR_NOT_FOUND        VFS_ERR_NOT_FOUND
#define FS_ERR_NOT_DIR          VFS_ERR_NOT_DIR
#define FS_ERR_NOT_PARAM        VFS_ERR_NOT_PARAM
#define FS_ERR_READ_ONLY        VFS_ERR_READ_ONLY
#define FS_ERR_NO_MEMORY        VFS_ERR_NO_MEMORY
#define FS_ERR_NAME_TOO_LONG    VFS_ERR_NAME_TOO_LONG
#define FS_ERR_DIR_FULL         VFS_ERR_DIR_FULL
#define FS_ERR_ALREADY_EXISTS   VFS_ERR_ALREADY_EXISTS
#define FS_ERR_INVALID_PATH     VFS_ERR_INVALID_PATH

/* Configuration compatibility definitions */
#define DRV_FS_MAX_PATH_LEN     VFS_MAX_PATH_LEN
#define DRV_FS_MAX_NAME_LEN     VFS_MAX_NAME_LEN
#define DRV_FS_MAX_CHILDREN     VFS_MAX_CHILDREN
#define DRV_FS_MAX_PARAM_LEN    VFS_MAX_PARAM_LEN
#define DRV_FS_MAX_NODES        VFS_MAX_NODES

/*******************************************************************************
 * API compatibility macros (map to VFS)
 ******************************************************************************/
#define DrvFs_GetRoot()             Vfs_GetRoot()
#define DrvFs_GetCwd()              Vfs_GetCwd()
#define DrvFs_GetCwdPath(b,l)       Vfs_GetCwdPath(b,l)
#define DrvFs_Cd(p)                 Vfs_Cd(p)
#define DrvFs_FindNode(p)           Vfs_FindNode(p)
#define DrvFs_CreateDir(p,n)        Vfs_CreateDir(p,n)
#define DrvFs_CreateParam(p,n,d,g,s,u)  Vfs_CreateParam(p,n,d,g,s,u)
#define DrvFs_CreateDevice(p,n,u)   Vfs_CreateDevice(p,n,u)
#define DrvFs_ReadParam(n,b,l)      Vfs_ReadParam(n,b,l)
#define DrvFs_WriteParam(n,v)       Vfs_WriteParam(n,v)
#define DrvFs_ListDir(n,c,u)        Vfs_ListDir(n,c,u)
#define DrvFs_RemoveNode(n)         Vfs_RemoveNode(n)
#define DrvFs_GetTypeName(t)        Vfs_GetTypeName(t)

/*******************************************************************************
 * Driver file system dedicated API
 ******************************************************************************/

/**
 * @brief  Initialize driver file system (create /driver and subdirectories)
 * @return FS_OK success, others failure
 * @note   Must call Vfs_Init() first
 */
FsError_t DrvFs_Init(void);

/**
 * @brief  Get /driver directory node
 */
FsNode_t* DrvFs_GetDriverDir(void);

/**
 * @brief  Get /driver/spi directory node
 */
FsNode_t* DrvFs_GetSpiDir(void);

/**
 * @brief  Get /driver/i2c directory node
 */
FsNode_t* DrvFs_GetI2cDir(void);

/**
 * @brief  Get /driver/i2s directory node
 */
FsNode_t* DrvFs_GetI2sDir(void);

/**
 * @brief  Get /driver/sdio directory node
 */
FsNode_t* DrvFs_GetSdioDir(void);

/**
 * @brief  Get /driver/power directory node
 */
FsNode_t* DrvFs_GetPowerDir(void);

/**
 * @brief  Get /driver/usb directory node
 */
FsNode_t* DrvFs_GetUsbDir(void);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_FS_H__ */
