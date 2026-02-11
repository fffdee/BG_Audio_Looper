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
 **********************3201000*******************************************************
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

/* BLE Notify Handle - AB02特征值句柄 */
#define BLE_SHELL_NOTIFY_HANDLE    0x0008

/* BLE Tick functions for delay */
extern uint32_t BLE_GetTick(void);
extern uint8_t BLE_IsDelayElapsed(uint32_t start_tick, uint32_t delay_ms);

/* BLE sync command buffer mechanism */
static uint8_t g_ble_sync_pending = 0;
static uint32_t g_ble_sync_start_tick = 0;
static char g_ble_sync_buffer[1024];
static int g_ble_sync_len = 0;

/* Flag to indicate if current command is a sync command (contains -q) */
uint8_t g_is_sync_command = 0;

/* BLE response delay configuration (in milliseconds) */
#define BLE_SYNC_DELAY_MS  1000  // 1 second delay for sync commands

/* Function to check and send delayed BLE sync response */
void BLE_CheckSyncResponse(void) {
    if (g_ble_sync_pending && BLE_IsDelayElapsed(g_ble_sync_start_tick, BLE_SYNC_DELAY_MS)) {
        DBG("[BLE_SYNC] Sending delayed sync response after %dms\n", BLE_SYNC_DELAY_MS);
        BLE_Send((uint8_t *)g_ble_sync_buffer, g_ble_sync_len);
        g_ble_sync_pending = 0;
        g_ble_sync_len = 0;
        DBG("[BLE_SYNC] Delayed sync response sent\n");
    }
}

/* Function to buffer sync command response */
void BLE_BufferSyncResponse(const char *data, int len) {
    /* Check if we have enough space for the new data */
    if (g_ble_sync_len + len >= sizeof(g_ble_sync_buffer)) {
        DBG("[BLE_SYNC] ERROR: Response too large (%d + %d >= %lu)\n", g_ble_sync_len, len, (unsigned long)sizeof(g_ble_sync_buffer));
        return;
    }

    /* Append the new data to the buffer */
    memcpy(g_ble_sync_buffer + g_ble_sync_len, data, len);
    g_ble_sync_len += len;
    g_ble_sync_buffer[g_ble_sync_len] = '\0';  /* Ensure null termination */

    /* Only set pending and start tick on first call */
    if (!g_ble_sync_pending) {
        g_ble_sync_pending = 1;
        g_ble_sync_start_tick = BLE_GetTick();
        DBG("[BLE_SYNC] Sync response buffering started\n");
    }

    DBG("[BLE_SYNC] Sync response buffered (%d bytes total)\n", g_ble_sync_len);
}

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
uint8_t g_BLE_CCCD_Enabled = 0;  /* CCCD状态缓存，避免频繁调用att_server_can_send() - 对外暴露->send */

uint16_t BLE_Send(uint8_t *data, uint16_t len)
{
    uint16_t sent = 0;
    uint16_t chunk_size;
    const uint16_t max_len = 250;
    int result;
    int retry_count;
    const int max_retries = 3;

    // 主动检查CCCD/Notify状态
   if (att_server_can_send() == 0) {
       DBG("[BLE_TX] WARN: CCCD not ready (att_server_can_send=0), skipping send\n");
       return 0;
   }

    while (sent < len)
    {
        chunk_size = (len - sent) > max_len ? max_len : (len - sent);

        // 重试机制：每个块最多重试max_retries次
        for (retry_count = 0; retry_count < max_retries; retry_count++)
        {
            result = GattServerNotify(BLE_SHELL_NOTIFY_HANDLE, data + sent, chunk_size);
            if (result == 0)
            {
                // 发送成功
                sent += chunk_size;
                DBG("[BLE_TX] Chunk sent: offset=%d, size=%d, total_sent=%d/%d\n", sent - chunk_size, chunk_size, sent, len);
                break;
            }
            else
            {
                DBG("[BLE_TX] ERROR: GattServerNotify failed at offset %d, size=%d, result=%d, retry=%d/%d\n",
                    sent, chunk_size, result, retry_count + 1, max_retries);

                if (retry_count < max_retries - 1)
                {
                    // 等待一小段时间后重试 (10ms)
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }

        if (result != 0)
        {
            // 所有重试都失败，停止发送
            DBG("[BLE_TX] ERROR: Failed to send chunk after %d retries, stopping transmission\n", max_retries);
            break;
        }

        // 在发送下一个块之前增加延迟，避免BLE协议栈过载
        if (sent < len)
        {
            vTaskDelay(pdMS_TO_TICKS(5));  // 5ms delay between chunks
        }
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
    char cmd_str[256] = {0};
    
    /* 打印收到的原始命令 */
    DBG("[BLE_RX] Received %d bytes: \"", len);
    for (i = 0; i < len && i < 128; i++) {
        if (data[i] >= 32 && data[i] < 127) {
            DBG("%c", data[i]);
            if (i < sizeof(cmd_str) - 1) cmd_str[i] = data[i];
        } else if (data[i] == '\r') {
            DBG("<CR>");
        } else if (data[i] == '\n') {
            DBG("<LF>");
        } else {
            DBG("[0x%02X]", data[i]);
        }
    }
    DBG("\"\n");
    
    /* Check if this is a sync command (contains -q) */
    g_is_sync_command = (strstr(cmd_str, " -q") != NULL);
    if (g_is_sync_command) {
        DBG("[BLE_SYNC] Detected sync command: %s\n", cmd_str);
    }
    
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
    
    /* Reset sync command flag after processing */
    g_is_sync_command = 0;
}
