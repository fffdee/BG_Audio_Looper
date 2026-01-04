/**
 *****************************************************************************
 * @file     shell_io_ble.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     16-December-2025
 * @brief    Shell BLE SPP IO閫傞厤鍣ㄥ疄鐜�
 *****************************************************************************
 */

#include "shell_io_ble.h"
#include "shell_io_manager.h"
#include "ble_api.h"
#include <string.h>

/* BLE Notify Handle - AB02特征值句柄 */
#define BLE_SHELL_NOTIFY_HANDLE    0x0008

/*******************************************************************************
 * BLE鎺ユ敹缂撳啿鍖�
 ******************************************************************************/
#define BLE_RX_BUF_SIZE     256

static uint8_t  g_BleRxBuf[BLE_RX_BUF_SIZE];
static uint16_t g_BleRxHead = 0;
static uint16_t g_BleRxTail = 0;
static uint16_t g_BleRxCount = 0;

/*******************************************************************************
 * BLE IO閫傞厤鍑芥暟
 ******************************************************************************/

static uint16_t BLE_Send(uint8_t *data, uint16_t len)
{
    uint16_t sent = 0;
    uint16_t chunk_size;
    const uint16_t max_len = 20;  /* BLE默认MTU，每次最多发送20字节 */
    int result;
    
    /* 分包发送 */
    while (sent < len)
    {
        chunk_size = (len - sent) > max_len ? max_len : (len - sent);
        
        result = GattServerNotify(BLE_SHELL_NOTIFY_HANDLE, data + sent, chunk_size);
        if (result != 0)
        {
            /* 发送失败，返回已发送字节数 */
            break;
        }
        sent += chunk_size;
    }
    
    return sent;
}

static uint16_t BLE_Recv(uint8_t *data, uint16_t maxLen)
{
    uint16_t count = 0;
    
    while(g_BleRxCount > 0 && count < maxLen)
    {
        data[count++] = g_BleRxBuf[g_BleRxTail];
        g_BleRxTail = (g_BleRxTail + 1) % BLE_RX_BUF_SIZE;
        g_BleRxCount--;
    }
    
    return count;
}

static uint16_t BLE_Available(void)
{
    return g_BleRxCount;
}

/*******************************************************************************
 * BLE IO鎺ュ彛瀹炰緥
 ******************************************************************************/
static const ShellIO_t g_BLE_IO = {
    .name      = "BLE-SPP",
    .send      = BLE_Send,
    .recv      = BLE_Recv,
    .available = BLE_Available
};

/*******************************************************************************
 * 鍏叡鍑芥暟
 ******************************************************************************/

const ShellIO_t* ShellIO_BLE_Get(void)
{
    return &g_BLE_IO;
}

void ShellIO_BLE_Init(void)
{
    // 娓呯┖鎺ユ敹缂撳啿鍖�
    g_BleRxHead = 0;
    g_BleRxTail = 0;
    g_BleRxCount = 0;
    
    Shell_Init();
    Shell_SetIO(&g_BLE_IO);
    Shell_RegisterAllModules();
}

void ShellIO_BLE_OnDataReceived(uint8_t *data, uint16_t len)
{
	uint16_t i;
    
    /* 将BLE接收到的数据放入缓冲区 */
    for(i = 0; i < len; i++)
    {
        if(g_BleRxCount < BLE_RX_BUF_SIZE)
        {
            g_BleRxBuf[g_BleRxHead] = data[i];
            g_BleRxHead = (g_BleRxHead + 1) % BLE_RX_BUF_SIZE;
            g_BleRxCount++;
        }
        else
        {
            /* 缓冲区满，丢弃数据 */
            break;
        }
    }
    
    /* 通知IO管理器BLE有活动 */
    ShellIOManager_UpdateActivity(SHELL_IO_BLE);
}
