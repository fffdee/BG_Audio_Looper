/**
 *****************************************************************************
 * @file     bt_vfs_driver.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     07-January-2026
 * @brief    Bluetooth VFS driver - BT (Classic Bluetooth/A2DP) and BLE (Bluetooth Low Energy)
 *****************************************************************************
 */

#ifndef __BT_VFS_DRIVER_H__
#define __BT_VFS_DRIVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"
#include "drv_device.h"
#include "vfs.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* BT VFS返回码 */
#define BT_VFS_OK     0
#define BT_VFS_ERROR -1

/* BT设备参数 */
typedef enum {
    BT_PARAM_STATUS = 0,      /* 连接状态 (只读) */
    BT_PARAM_NAME,            /* 蓝牙名称 */
    BT_PARAM_MAC,             /* MAC地址 (只读) */
    BT_PARAM_VOLUME,          /* 音量 */
    BT_PARAM_CONNECTED_DEV,   /* 已连接设备名 (只读) */
    BT_PARAM_RSSI,            /* 信号强度 (只读) */
    BT_PARAM_CODEC,           /* 编解码器 (只读) */
    BT_PARAM_MAX
} BtParamId_t;

/* BLE设备参数 */
typedef enum {
    BLE_PARAM_STATUS = 0,     /* 连接状态 (只读) */
    BLE_PARAM_NAME,           /* BLE广播名称 */
    BLE_PARAM_MAC,            /* MAC地址 (只读) */
    BLE_PARAM_ADVERTISING,    /* 广播状态 */
    BLE_PARAM_TX_POWER,       /* 发射功率 */
    BLE_PARAM_INTERVAL,       /* 连接间隔 (只读) */
    BLE_PARAM_MTU,            /* MTU大小 (只读) */
    BLE_PARAM_MAX
} BleParamId_t;

/*******************************************************************************
 * API Functions
 ******************************************************************************/

/**
 * @brief  初始化BT设备驱动
 * @return 0 成功, -1 失败
 */
int BtVfs_Init(void);

/**
 * @brief  初始化BLE设备驱动
 * @return 0 成功, -1 失败
 */
int BleVfs_Init(void);

/**
 * @brief  将BT设备挂载到VFS
 * @param  parent: 父节点
 * @return VFS节点指针，失败返回NULL
 */
VfsNode_t* BtVfs_Mount(VfsNode_t *parent);

/**
 * @brief  将BLE设备挂载到VFS
 * @param  parent: 父节点
 * @return VFS节点指针，失败返回NULL
 */
VfsNode_t* BleVfs_Mount(VfsNode_t *parent);

/**
 * @brief  卸载BT设备
 * @return 0 成功, -1 失败
 */
int BtVfs_Unmount(void);

/**
 * @brief  卸载BLE设备
 * @return 0 成功, -1 失败
 */
int BleVfs_Unmount(void);

/**
 * @brief  默认挂载BT/BLE到/driver目录（驱动框架初始化时调用）
 * @return BT_VFS_OK 成功, BT_VFS_ERROR 失败
 */
int BtVfsDriver_MountDefault(void);

#ifdef __cplusplus
}
#endif

#endif /* __BT_VFS_DRIVER_H__ */
