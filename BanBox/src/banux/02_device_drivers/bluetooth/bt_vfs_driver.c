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

/* 外部蓝牙全局变量声明 */
extern BT_CONFIGURATION_PARAMS *btStackConfigParams;
extern BT_MANAGER_ST btManager;

/* BT参数节点 */
static VfsNode_t *g_BtParams[BT_PARAM_MAX] = {NULL};

/* BLE参数节点 */
static VfsNode_t *g_BleParams[BLE_PARAM_MAX] = {NULL};

/* 参数名称表 */
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
    
    /* 从userData中获取参数ID */
    param_id = (BtParamId_t)(uintptr_t)userData;
    
    switch (param_id) {
        case BT_PARAM_STATUS: {
            /* BT状态: 0=None, 1=Connecting, 2=Connected, 3=Streaming */
            int status = 0;  /* TODO: 从GetA2dpState()获取实际状态 */
            const char *status_str[] = {"None", "Connecting", "Connected", "Streaming"};
            if (status >= 0 && status < 4) {
                ret = snprintf(buf, maxLen, "%s", status_str[status]);
            } else {
                ret = snprintf(buf, maxLen, "Unknown");
            }
            break;
        }
        
        case BT_PARAM_NAME: {
            /* 蓝牙名称 */
            if (btStackConfigParams != NULL) {
                ret = snprintf(buf, maxLen, "%s", (char*)btStackConfigParams->bt_LocalDeviceName);
            } else {
                ret = snprintf(buf, maxLen, "N/A");
            }
            break;
        }
        
        case BT_PARAM_MAC: {
            /* MAC地址 */
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
            /* 当前音量 */
            ret = snprintf(buf, maxLen, "%d", btManager.volGain);
            break;
        }
        
        case BT_PARAM_CONNECTED_DEV: {
            /* 已连接设备名称 */
            /* TODO: 从GetA2dpState()和btManager获取实际状态和设备名 */
            ret = snprintf(buf, maxLen, "None");
            break;
        }
        
        case BT_PARAM_RSSI: {
            /* 信号强度 (RSSI) */
            /* TODO: 实现RSSI读取 */
            ret = snprintf(buf, maxLen, "-50");
            break;
        }
        
        case BT_PARAM_CODEC: {
            /* 编解码器类型 */
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
            /* 设置蓝牙名称 */
            if (btStackConfigParams != NULL) {
                strncpy((char*)btStackConfigParams->bt_LocalDeviceName, buf, BT_NAME_SIZE - 1);
                btStackConfigParams->bt_LocalDeviceName[BT_NAME_SIZE - 1] = '\0';
            }
            /* TODO: 调用BT API更新名称 */
            return 0;
        }
        
        case BT_PARAM_VOLUME: {
            /* 设置音量 */
            int vol = atoi(buf);
            if (vol < 0) vol = 0;
            if (vol > 15) vol = 15;  /* HFP volGain范围是0-15 */
            btManager.volGain = (uint8_t)vol;
            /* TODO: 调用BT API更新音量 */
            return 0;
        }
        
        /* 其他参数只读 */
        case BT_PARAM_STATUS:
        case BT_PARAM_MAC:
        case BT_PARAM_CONNECTED_DEV:
        case BT_PARAM_RSSI:
        case BT_PARAM_CODEC:
            return -2;  /* 只读参数 */
        
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
            /* BLE连接状态 */
            /* TODO: 从BLE管理器获取状态 */
            ret = snprintf(buf, maxLen, "Idle");
            break;
        }
        
        case BLE_PARAM_NAME: {
            /* BLE广播名称 */
            /* TODO: 从BLE管理器获取名称 */
            ret = snprintf(buf, maxLen, "BG_BLE");
            break;
        }
        
        case BLE_PARAM_MAC: {
            /* BLE MAC地址 */
            /* TODO: 从BLE管理器获取MAC */
            ret = snprintf(buf, maxLen, "00:00:00:00:00:00");
            break;
        }
        
        case BLE_PARAM_ADVERTISING: {
            /* 广播状态 */
            /* TODO: 从BLE管理器获取广播状态 */
            ret = snprintf(buf, maxLen, "Off");
            break;
        }
        
        case BLE_PARAM_TX_POWER: {
            /* 发射功率 */
            ret = snprintf(buf, maxLen, "0");
            break;
        }
        
        case BLE_PARAM_INTERVAL: {
            /* 连接间隔 */
            ret = snprintf(buf, maxLen, "7.5");
            break;
        }
        
        case BLE_PARAM_MTU: {
            /* MTU大小 */
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
            /* 设置BLE名称 */
            /* TODO: 调用BLE API更新名称 */
            return 0;
        }
        
        case BLE_PARAM_ADVERTISING: {
            /* 控制广播 */
            if (strncmp(buf, "On", 2) == 0 || strncmp(buf, "1", 1) == 0) {
                /* 开启广播 */
                /* TODO: 调用BLE API开启广播 */
            } else {
                /* 关闭广播 */
                /* TODO: 调用BLE API关闭广播 */
            }
            return 0;
        }
        
        case BLE_PARAM_TX_POWER: {
            /* 设置发射功率 */
            /* int power = atoi(buf); */ /* TODO: 获取功率值后调用BLE API */
            /* TODO: 调用BLE API设置功率 */
            return 0;
        }
        
        /* 其他参数只读 */
        case BLE_PARAM_STATUS:
        case BLE_PARAM_MAC:
        case BLE_PARAM_INTERVAL:
        case BLE_PARAM_MTU:
            return -2;  /* 只读参数 */
        
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
    
    /* 创建bt设备节点 */
    g_BtNode = Vfs_CreateDir(parent, "bt");
    if (!g_BtNode) {
        DBG("[BtVfs] ERROR: Failed to create bt node\n");
        return NULL;
    }
    
    /* 创建参数节点 */
    for (i = 0; i < BT_PARAM_MAX; i++) {
        VfsParamSet_t setFunc = NULL;
        
        /* 只有name和volume可写 */
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
    
    /* 创建ble设备节点 */
    g_BleNode = Vfs_CreateDir(parent, "ble");
    if (!g_BleNode) {
        DBG("[BleVfs] ERROR: Failed to create ble node\n");
        return NULL;
    }
    
    /* 创建参数节点 */
    for (i = 0; i < BLE_PARAM_MAX; i++) {
        VfsParamSet_t setFunc = NULL;
        
        /* name, advertising, tx_power可写 */
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
    
    /* 删除参数节点 */
    for (i = 0; i < BT_PARAM_MAX; i++) {
        if (g_BtParams[i]) {
            Vfs_RemoveNode(g_BtParams[i]);
            g_BtParams[i] = NULL;
        }
    }
    
    /* 删除设备节点 */
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
    
    /* 删除参数节点 */
    for (i = 0; i < BLE_PARAM_MAX; i++) {
        if (g_BleParams[i]) {
            Vfs_RemoveNode(g_BleParams[i]);
            g_BleParams[i] = NULL;
        }
    }
    
    /* 删除设备节点 */
    Vfs_RemoveNode(g_BleNode);
    g_BleNode = NULL;
    
    DBG("[BleVfs] BLE device unmounted\n");
    return 0;
}

/**
 * @brief  默认挂载BT/BLE到/driver目录
 * @detail 该函数由驱动框架调用，在BT/BLE初始化后将设备挂载到VFS
 * @return BT_VFS_OK 成功, BT_VFS_ERROR 失败
 */
int BtVfsDriver_MountDefault(void)
{
    VfsNode_t *driver_node = NULL;
    int ret = BT_VFS_OK;
    
    DBG("[BtVfsDriver] Mounting BT/BLE to /driver...\n");
    
    /* 查找或创建/driver目录 */
    driver_node = Vfs_FindNode("/driver");
    if (!driver_node) {
        driver_node = Vfs_CreateDir(Vfs_GetRoot(), "driver");
        if (!driver_node) {
            DBG("[BtVfsDriver] ERROR: Failed to create /driver node\n");
            return BT_VFS_ERROR;
        }
        DBG("[BtVfsDriver] Created /driver directory\n");
    }
    
    /* 初始化BT和BLE驱动 */
    if (BtVfs_Init() != 0) {
        DBG("[BtVfsDriver] WARNING: BT VFS init failed\n");
        /* 不返回错误，继续尝试挂载 */
    }
    
    if (BleVfs_Init() != 0) {
        DBG("[BtVfsDriver] WARNING: BLE VFS init failed\n");
        /* 不返回错误，继续尝试挂载 */
    }
    
    /* 挂载BT设备 */
    if (BtVfs_Mount(driver_node) == NULL) {
        DBG("[BtVfsDriver] WARNING: Failed to mount BT device\n");
        ret = BT_VFS_ERROR;
    } else {
        DBG("[BtVfsDriver] BT device mounted to /driver/bt\n");
    }
    
    /* 挂载BLE设备 */
    if (BleVfs_Mount(driver_node) == NULL) {
        DBG("[BtVfsDriver] WARNING: Failed to mount BLE device\n");
        ret = BT_VFS_ERROR;
    } else {
        DBG("[BtVfsDriver] BLE device mounted to /driver/ble\n");
    }
    
    return ret;
}
