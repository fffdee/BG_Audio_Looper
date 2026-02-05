/**
 *****************************************************************************
 * @file     bt_vfs_driver.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     07-January-2026
 * @brief    Bluetooth VFS driver implementation
 *****************************************************************************
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "bt_vfs_driver.h"
#include "bt_config.h"
#include "bt_manager.h"
#include "debug.h"

/*******************************************************************************
 * Private Variables
 ******************************************************************************/
static VfsNode_t *g_BtNode = NULL;
static VfsNode_t *g_BleNode = NULL;

/* 澶栭儴钃濈墮鍏ㄥ眬鍙橀噺澹版槑 */
extern BT_CONFIGURATION_PARAMS *btStackConfigParams;
extern BT_MANAGER_ST btManager;

/* BT鍙傛暟鑺傜偣 */
static VfsNode_t *g_BtParams[BT_PARAM_MAX] = {NULL};

/* BLE鍙傛暟鑺傜偣 */
static VfsNode_t *g_BleParams[BLE_PARAM_MAX] = {NULL};

/* 鍙傛暟鍚嶇О琛�*/
static const char *g_BtParamNames[BT_PARAM_MAX] = {
    "status",
    "name",
    "mac",
    "volume",
    "connected_device",
    "rssi",
    "codec"
};

static const char *g_BleParamNames[BLE_PARAM_MAX] = {
    "status",
    "name",
    "mac",
    "advertising",
    "tx_power",
    "interval",
    "mtu"
};

/*******************************************************************************
 * BT Parameter Read/Write Functions (VFS callbacks)
 ******************************************************************************/

static int BtParam_Read(char *buf, uint16_t maxLen, void *userData)
{
    BtParamId_t param_id;
    int ret = 0;
    
    if (!buf) return -1;
    
    /* 浠巙serData涓幏鍙栧弬鏁癐D */
    param_id = (BtParamId_t)(uintptr_t)userData;
    
    switch (param_id) {
        case BT_PARAM_STATUS: {
            /* BT鐘舵�: 0=None, 1=Connecting, 2=Connected, 3=Streaming */
            int status = 0;  /* TODO: 浠嶨etA2dpState()鑾峰彇瀹為檯鐘舵� */
            const char *status_str[] = {"None", "Connecting", "Connected", "Streaming"};
            if (status >= 0 && status < 4) {
                ret = snprintf(buf, maxLen, "%s", status_str[status]);
            } else {
                ret = snprintf(buf, maxLen, "Unknown");
            }
            break;
        }
        
        case BT_PARAM_NAME: {
            /* 钃濈墮鍚嶇О */
            if (btStackConfigParams != NULL) {
                ret = snprintf(buf, maxLen, "%s", (char*)btStackConfigParams->bt_LocalDeviceName);
            } else {
                ret = snprintf(buf, maxLen, "N/A");
            }
            break;
        }
        
        case BT_PARAM_MAC: {
            /* MAC鍦板潃 */
            ret = snprintf(buf, maxLen, "%02X:%02X:%02X:%02X:%02X:%02X",
                          btManager.btDevAddr[0],
                          btManager.btDevAddr[1],
                          btManager.btDevAddr[2],
                          btManager.btDevAddr[3],
                          btManager.btDevAddr[4],
                          btManager.btDevAddr[5]);
            break;
        }
        
        case BT_PARAM_VOLUME: {
            /* 褰撳墠闊抽噺 */
            ret = snprintf(buf, maxLen, "%d", btManager.volGain);
            break;
        }
        
        case BT_PARAM_CONNECTED_DEV: {
            /* 宸茶繛鎺ヨ澶囧悕绉�*/
            /* TODO: 浠嶨etA2dpState()鍜宐tManager鑾峰彇瀹為檯鐘舵�鍜岃澶囧悕 */
            ret = snprintf(buf, maxLen, "None");
            break;
        }
        
        case BT_PARAM_RSSI: {
            /* 淇″彿寮哄害 (RSSI) */
            /* TODO: 瀹炵幇RSSI璇诲彇 */
            ret = snprintf(buf, maxLen, "-50");
            break;
        }
        
        case BT_PARAM_CODEC: {
            /* 缂栬В鐮佸櫒绫诲瀷 */
            ret = snprintf(buf, maxLen, "SBC");
            break;
        }
        
        default:
            return -1;
    }
    
    return ret;
}

static int BtParam_Write(const char *buf, void *userData)
{
    BtParamId_t param_id;
    
    if (!buf) return -1;
    
    param_id = (BtParamId_t)(uintptr_t)userData;
    
    switch (param_id) {
        case BT_PARAM_NAME: {
            /* 璁剧疆钃濈墮鍚嶇О */
            if (btStackConfigParams != NULL) {
                strncpy((char*)btStackConfigParams->bt_LocalDeviceName, buf, BT_NAME_SIZE - 1);
                btStackConfigParams->bt_LocalDeviceName[BT_NAME_SIZE - 1] = '\0';
            }
            /* TODO: 璋冪敤BT API鏇存柊鍚嶇О */
            return 0;
        }
        
        case BT_PARAM_VOLUME: {
            /* 璁剧疆闊抽噺 */
            int vol = atoi(buf);
            if (vol < 0) vol = 0;
            if (vol > 15) vol = 15;  /* HFP volGain鑼冨洿鏄�-15 */
            btManager.volGain = (uint8_t)vol;
            /* TODO: 璋冪敤BT API鏇存柊闊抽噺 */
            return 0;
        }
        
        /* 鍏朵粬鍙傛暟鍙 */
        case BT_PARAM_STATUS:
        case BT_PARAM_MAC:
        case BT_PARAM_CONNECTED_DEV:
        case BT_PARAM_RSSI:
        case BT_PARAM_CODEC:
            return -2;  /* 鍙鍙傛暟 */
        
        default:
            return -1;
    }
}

/*******************************************************************************
 * BLE Parameter Read/Write Functions (VFS callbacks)
 ******************************************************************************/

static int BleParam_Read(char *buf, uint16_t maxLen, void *userData)
{
    BleParamId_t param_id;
    int ret = 0;
    
    if (!buf) return -1;
    
    param_id = (BleParamId_t)(uintptr_t)userData;
    
    switch (param_id) {
        case BLE_PARAM_STATUS: {
            /* BLE杩炴帴鐘舵� */
            /* TODO: 浠嶣LE绠＄悊鍣ㄨ幏鍙栫姸鎬�*/
            ret = snprintf(buf, maxLen, "Idle");
            break;
        }
        
        case BLE_PARAM_NAME: {
            /* BLE骞挎挱鍚嶇О */
            /* TODO: 浠嶣LE绠＄悊鍣ㄨ幏鍙栧悕绉�*/
            ret = snprintf(buf, maxLen, "BG_BLE");
            break;
        }
        
        case BLE_PARAM_MAC: {
            /* BLE MAC鍦板潃 */
            /* TODO: 浠嶣LE绠＄悊鍣ㄨ幏鍙朚AC */
            ret = snprintf(buf, maxLen, "00:00:00:00:00:00");
            break;
        }
        
        case BLE_PARAM_ADVERTISING: {
            /* 骞挎挱鐘舵� */
            /* TODO: 浠嶣LE绠＄悊鍣ㄨ幏鍙栧箍鎾姸鎬�*/
            ret = snprintf(buf, maxLen, "Off");
            break;
        }
        
        case BLE_PARAM_TX_POWER: {
            /* 鍙戝皠鍔熺巼 */
            ret = snprintf(buf, maxLen, "0");
            break;
        }
        
        case BLE_PARAM_INTERVAL: {
            /* 杩炴帴闂撮殧 */
            ret = snprintf(buf, maxLen, "7.5");
            break;
        }
        
        case BLE_PARAM_MTU: {
            /* MTU澶у皬 */
            ret = snprintf(buf, maxLen, "23");
            break;
        }
        
        default:
            return -1;
    }
    
    return ret;
}

static int BleParam_Write(const char *buf, void *userData)
{
    BleParamId_t param_id;
    
    if (!buf) return -1;
    
    param_id = (BleParamId_t)(uintptr_t)userData;
    
    switch (param_id) {
        case BLE_PARAM_NAME: {
            /* 璁剧疆BLE鍚嶇О */
            /* TODO: 璋冪敤BLE API鏇存柊鍚嶇О */
            return 0;
        }
        
        case BLE_PARAM_ADVERTISING: {
            /* 鎺у埗骞挎挱 */
            if (strncmp(buf, "On", 2) == 0 || strncmp(buf, "1", 1) == 0) {
                /* 寮�惎骞挎挱 */
                /* TODO: 璋冪敤BLE API寮�惎骞挎挱 */
            } else {
                /* 鍏抽棴骞挎挱 */
                /* TODO: 璋冪敤BLE API鍏抽棴骞挎挱 */
            }
            return 0;
        }
        
        case BLE_PARAM_TX_POWER: {
            /* 璁剧疆鍙戝皠鍔熺巼 */
            /* int power = atoi(buf); */ /* TODO: 鑾峰彇鍔熺巼鍊煎悗璋冪敤BLE API */
            /* TODO: 璋冪敤BLE API璁剧疆鍔熺巼 */
            return 0;
        }
        
        /* 鍏朵粬鍙傛暟鍙 */
        case BLE_PARAM_STATUS:
        case BLE_PARAM_MAC:
        case BLE_PARAM_INTERVAL:
        case BLE_PARAM_MTU:
            return -2;  /* 鍙鍙傛暟 */
        
        default:
            return -1;
    }
}

/*******************************************************************************
 * Public API Implementation
 ******************************************************************************/

int BtVfs_Init(void)
{
    DBG("[BtVfs] Initializing BT VFS driver...\n");
    return 0;
}

int BleVfs_Init(void)
{
    DBG("[BleVfs] Initializing BLE VFS driver...\n");
    return 0;
}

VfsNode_t* BtVfs_Mount(VfsNode_t *parent)
{
    int i;
    
    if (!parent) {
        DBG("[BtVfs] ERROR: Parent node is NULL\n");
        return NULL;
    }
    
    DBG("[BtVfs] Mounting BT device to VFS...\n");
    
    /* 鍒涘缓bt璁惧鑺傜偣 */
    g_BtNode = Vfs_CreateDir(parent, "bt");
    if (!g_BtNode) {
        DBG("[BtVfs] ERROR: Failed to create bt node\n");
        return NULL;
    }
    
    /* 鍒涘缓鍙傛暟鑺傜偣 */
    for (i = 0; i < BT_PARAM_MAX; i++) {
        VfsParamSet_t setFunc = NULL;
        
        /* 鍙湁name鍜寁olume鍙啓 */
        if (i == BT_PARAM_NAME || i == BT_PARAM_VOLUME) {
            setFunc = BtParam_Write;
        }
        
        g_BtParams[i] = Vfs_CreateParam(g_BtNode, g_BtParamNames[i], 
                                        NULL, /* description */
                                        BtParam_Read, 
                                        setFunc,
                                        (void*)(uintptr_t)i);
        if (!g_BtParams[i]) {
            DBG("[BtVfs] ERROR: Failed to create param: %s\n", g_BtParamNames[i]);
            continue;
        }
    }
    
    DBG("[BtVfs] BT device mounted successfully\n");
    return g_BtNode;
}

VfsNode_t* BleVfs_Mount(VfsNode_t *parent)
{
    int i;
    
    if (!parent) {
        DBG("[BleVfs] ERROR: Parent node is NULL\n");
        return NULL;
    }
    
    DBG("[BleVfs] Mounting BLE device to VFS...\n");
    
    /* 鍒涘缓ble璁惧鑺傜偣 */
    g_BleNode = Vfs_CreateDir(parent, "ble");
    if (!g_BleNode) {
        DBG("[BleVfs] ERROR: Failed to create ble node\n");
        return NULL;
    }
    
    /* 鍒涘缓鍙傛暟鑺傜偣 */
    for (i = 0; i < BLE_PARAM_MAX; i++) {
        VfsParamSet_t setFunc = NULL;
        
        /* name, advertising, tx_power鍙啓 */
        if (i == BLE_PARAM_NAME || i == BLE_PARAM_ADVERTISING || i == BLE_PARAM_TX_POWER) {
            setFunc = BleParam_Write;
        }
        
        g_BleParams[i] = Vfs_CreateParam(g_BleNode, g_BleParamNames[i], 
                                         NULL, /* description */
                                         BleParam_Read, 
                                         setFunc,
                                         (void*)(uintptr_t)i);
        if (!g_BleParams[i]) {
            DBG("[BleVfs] ERROR: Failed to create param: %s\n", g_BleParamNames[i]);
            continue;
        }
    }
    
    DBG("[BleVfs] BLE device mounted successfully\n");
    return g_BleNode;
}

int BtVfs_Unmount(void)
{
    int i;
    
    if (!g_BtNode) return 0;
    
    DBG("[BtVfs] Unmounting BT device...\n");
    
    /* 鍒犻櫎鍙傛暟鑺傜偣 */
    for (i = 0; i < BT_PARAM_MAX; i++) {
        if (g_BtParams[i]) {
            Vfs_RemoveNode(g_BtParams[i]);
            g_BtParams[i] = NULL;
        }
    }
    
    /* 鍒犻櫎璁惧鑺傜偣 */
    Vfs_RemoveNode(g_BtNode);
    g_BtNode = NULL;
    
    DBG("[BtVfs] BT device unmounted\n");
    return 0;
}

int BleVfs_Unmount(void)
{
    int i;
    
    if (!g_BleNode) return 0;
    
    DBG("[BleVfs] Unmounting BLE device...\n");
    
    /* 鍒犻櫎鍙傛暟鑺傜偣 */
    for (i = 0; i < BLE_PARAM_MAX; i++) {
        if (g_BleParams[i]) {
            Vfs_RemoveNode(g_BleParams[i]);
            g_BleParams[i] = NULL;
        }
    }
    
    /* 鍒犻櫎璁惧鑺傜偣 */
    Vfs_RemoveNode(g_BleNode);
    g_BleNode = NULL;
    
    DBG("[BleVfs] BLE device unmounted\n");
    return 0;
}

/**
 * @brief  榛樿鎸傝浇BT/BLE鍒�driver鐩綍
 * @detail 璇ュ嚱鏁扮敱椹卞姩妗嗘灦璋冪敤锛屽湪BT/BLE鍒濆鍖栧悗灏嗚澶囨寕杞藉埌VFS
 * @return BT_VFS_OK 鎴愬姛, BT_VFS_ERROR 澶辫触
 */
int BtVfsDriver_MountDefault(void)
{
    VfsNode_t *driver_node = NULL;
    int ret = BT_VFS_OK;
    
    DBG("[BtVfsDriver] Mounting BT/BLE to /driver...\n");
    
    /* 鏌ユ壘鎴栧垱寤�driver鐩綍 */
    driver_node = Vfs_FindNode("/driver");
    if (!driver_node) {
        driver_node = Vfs_CreateDir(Vfs_GetRoot(), "driver");
        if (!driver_node) {
            DBG("[BtVfsDriver] ERROR: Failed to create /driver node\n");
            return BT_VFS_ERROR;
        }
        DBG("[BtVfsDriver] Created /driver directory\n");
    }
    
    /* 鍒濆鍖朆T鍜孊LE椹卞姩 */
    if (BtVfs_Init() != 0) {
        DBG("[BtVfsDriver] WARNING: BT VFS init failed\n");
        /* 涓嶈繑鍥為敊璇紝缁х画灏濊瘯鎸傝浇 */
    }
    
    if (BleVfs_Init() != 0) {
        DBG("[BtVfsDriver] WARNING: BLE VFS init failed\n");
        /* 涓嶈繑鍥為敊璇紝缁х画灏濊瘯鎸傝浇 */
    }
    
    /* 鎸傝浇BT璁惧 */
    if (BtVfs_Mount(driver_node) == NULL) {
        DBG("[BtVfsDriver] WARNING: Failed to mount BT device\n");
        ret = BT_VFS_ERROR;
    } else {
        DBG("[BtVfsDriver] BT device mounted to /driver/bt\n");
    }
    
    /* 鎸傝浇BLE璁惧 */
    if (BleVfs_Mount(driver_node) == NULL) {
        DBG("[BtVfsDriver] WARNING: Failed to mount BLE device\n");
        ret = BT_VFS_ERROR;
    } else {
        DBG("[BtVfsDriver] BLE device mounted to /driver/ble\n");
    }
    
    return ret;
}
