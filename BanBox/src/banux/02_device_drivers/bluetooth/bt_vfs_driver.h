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

/* BT VFS return codes */
#define BT_VFS_OK     0
#define BT_VFS_ERROR -1

/* BT device parameters */
typedef enum {
    BT_PARAM_STATUS = 0,      /* Connection status (read-only) */
    BT_PARAM_NAME,            /* Bluetooth name */
    BT_PARAM_MAC,             /* MAC address (read-only) */
    BT_PARAM_VOLUME,          /* Volume */
    BT_PARAM_CONNECTED_DEV,   /* Connected device name (read-only) */
    BT_PARAM_RSSI,            /* Signal strength (read-only) */
    BT_PARAM_CODEC,           /* Codec (read-only) */
    BT_PARAM_MAX
} BtParamId_t;

/* BLE device parameters */
typedef enum {
    BLE_PARAM_STATUS = 0,     /* Connection status (read-only) */
    BLE_PARAM_NAME,           /* BLE broadcast name */
    BLE_PARAM_MAC,            /* MAC address (read-only) */
    BLE_PARAM_ADVERTISING,    /* Advertising status */
    BLE_PARAM_TX_POWER,       /* Transmit power */
    BLE_PARAM_INTERVAL,       /* Connection interval (read-only) */
    BLE_PARAM_MTU,            /* MTU size (read-only) */
    BLE_PARAM_MAX
} BleParamId_t;

/*******************************************************************************
 * API Functions
 ******************************************************************************/

/**
 * @brief  Initialize BT device driver
 * @return 0 success, -1 failure
 */
int BtVfs_Init(void);

/**
 * @brief  Initialize BLE device driver
 * @return 0 success, -1 failure
 */
int BleVfs_Init(void);

/**
 * @brief  Mount BT device to VFS
 * @param  parent: Parent node
 * @return VFS node pointer, NULL on failure
 */
VfsNode_t* BtVfs_Mount(VfsNode_t *parent);

/**
 * @brief  Mount BLE device to VFS
 * @param  parent: Parent node
 * @return VFS node pointer, NULL on failure
 */
VfsNode_t* BleVfs_Mount(VfsNode_t *parent);

/**
 * @brief  Unmount BT device
 * @return 0 success, -1 failure
 */
int BtVfs_Unmount(void);

/**
 * @brief  Unmount BLE device
 * @return 0 success, -1 failure
 */
int BleVfs_Unmount(void);

/**
 * @brief  Default mount BT/BLE to /driver directory (called during driver framework initialization)
 * @return BT_VFS_OK success, BT_VFS_ERROR failure
 */
int BtVfsDriver_MountDefault(void);

#ifdef __cplusplus
}
#endif

#endif /* __BT_VFS_DRIVER_H__ */
