#include "shell_io_ble.h"
uint16_t BLE_Send(uint8_t *data, uint16_t len);
#include "FreeRTOS.h"
#include "task.h"
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) ((TickType_t)((((uint64_t)(x)) * configTICK_RATE_HZ) / 1000))
#endif

static TaskHandle_t g_BleNotifyTestTaskHandle = NULL;

static void BleNotifyTestTask(void *param)
{
    static char msg[32];  /* 使用静态变量减少栈占用 */
    uint32_t counter = 0;
    
    while (1)
    {
        snprintf(msg, sizeof(msg), "[BLE_NOTIFY_TEST] %lu\r\n", (unsigned long)counter++);
        BLE_Send((uint8_t *)msg, strlen(msg));
        vTaskDelay(pdMS_TO_TICKS(2000));  /* 增加到2秒，给BLE底层更多时间 */
    }
}

void BLE_StartNotifyTest(void)
{
    if (g_BleNotifyTestTaskHandle == NULL)
    {
        /* 栈大小从256改为1024，防止栈溢出 */
        xTaskCreate(BleNotifyTestTask, "BleNotifyTest", 1024, NULL, tskIDLE_PRIORITY + 1, &g_BleNotifyTestTaskHandle);
    }
}

void BLE_StopNotifyTest(void)
{
    if (g_BleNotifyTestTaskHandle)
    {
        vTaskDelete(g_BleNotifyTestTaskHandle);
        g_BleNotifyTestTaskHandle = NULL;
    }
}

/**
 *****************************************************************************
 * @file     shell_io_ble.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     16-December-2025
 * @brief    Shell BLE SPP IO
 *****************************************************************************
 */

#include "shell_io_ble.h"
#include "shell_io_manager.h"
#include "ble_api.h"
#include <string.h>
#include "debug.h"

/* External BLE stack function */
extern int att_server_can_send(void);

#define BLE_SHELL_NOTIFY_HANDLE    0x0008

#include "bg_shell.h"
void BLE_ShellEcho(const char *str)
{
    if (str == NULL) return;
    if (str[0] == '\0') return;
    Shell_Print(str);
}

#define BLE_RX_BUF_SIZE     256

static uint8_t  g_BleRxBuf[BLE_RX_BUF_SIZE];
static uint16_t g_BleRxHead = 0;
static uint16_t g_BleRxTail = 0;
static uint16_t g_BleRxCount = 0;
uint8_t g_BLE_CCCD_Enabled = 0;  /* CCCD状态缓存，避免频繁调用att_server_can_send() - 对外暴露 */

uint16_t BLE_Send(uint8_t *data, uint16_t len)
{
    uint16_t sent = 0;
    uint16_t chunk_size;
    const uint16_t max_len = 23;
    int result;

    // 主动检查CCCD/Notify状态
    if (att_server_can_send() == 0) {
        DBG("[BLE_TX] WARN: CCCD not ready (att_server_can_send=0), skipping send\n");
        return 0;
    }

    while (sent < len)
    {
        chunk_size = (len - sent) > max_len ? max_len : (len - sent);
        result = GattServerNotify(BLE_SHELL_NOTIFY_HANDLE, data + sent, chunk_size);
        if (result != 0)
        {
            DBG("[BLE_TX] ERROR: GattServerNotify failed at offset %d, result=%d\n", sent, result);
            break;
        }
        sent += chunk_size;
    }

    DBG("[BLE_TX] Completed: sent=%d/%d\n", sent, len);
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

static const ShellIO_t g_BLE_IO = {
    .name      = "BLE-SPP",
    .send      = BLE_Send,
    .recv      = BLE_Recv,
    .available = BLE_Available
};

const ShellIO_t* ShellIO_BLE_Get(void)
{
    return &g_BLE_IO;
}

void ShellIO_BLE_Init(void)
{
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
    
    /* 打印收到的原始命令 */
    DBG("[BLE_RX] Received %d bytes: \"", len);
    for (i = 0; i < len && i < 128; i++) {
        if (data[i] >= 32 && data[i] < 127) {
            DBG("%c", data[i]);
        } else if (data[i] == '\r') {
            DBG("<CR>");
        } else if (data[i] == '\n') {
            DBG("<LF>");
        } else {
            DBG("[0x%02X]", data[i]);
        }
    }
    DBG("\"\n");
    
    /* 
     * 重要修正：接收命令(Write操作)不需要检查CCCD状态！
     * - 命令接收通过Write特征值0x0006 (ATT_CHARACTERISTIC_AB01_01_VALUE_HANDLE)
     * - CCCD仅控制Notify操作(0x0008/0x000b的通知发送)
     * - 只有在BLE_Send()中发送响应时才需要检查att_server_can_send()
     * - 否则客户端发送命令后无法得到处理,导致"命令行失效"问题
     */
    
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
            DBG("[SHELL_BLE] ERROR: RX buffer full! Dropped %d bytes\n", len - i);
            break;
        }
    }
    
    DBG("[SHELL_BLE] Data buffered, new Count=%d\n", g_BleRxCount);
    
    ShellIOManager_UpdateActivity(SHELL_IO_BLE);
    
    DBG("[SHELL_BLE] Switching to BLE IO...\n");
    ShellIOManager_SwitchIO(SHELL_IO_BLE);
    
    DBG("[SHELL_BLE] Calling Shell_Process()...\n");
    Shell_Process();
    DBG("[SHELL_BLE] Shell_Process() completed\n");
}
