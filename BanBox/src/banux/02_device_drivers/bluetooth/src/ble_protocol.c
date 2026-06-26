#include "ble_protocol.h"
#include "shell_io_ble.h"
<<<<<<< Updated upstream
#include "sys_param.h"
#include "effect_graph.h"
=======
<<<<<<< HEAD
#include "sys_param.h"
#include "effect_graph.h"
=======
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
#include "debug.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
<<<<<<< Updated upstream
#include "battery_drv.h"    /* battery_get_soc() for on-connect battery report */
#include "audio_looper.h"   /* loop_get_segment_state/loop_get_segment_length_pages for reconnect sync */
=======
<<<<<<< HEAD
#include "battery_drv.h"    /* battery_get_soc() for on-connect battery report */
#include "audio_looper.h"   /* loop_get_segment_state/loop_get_segment_length_pages for reconnect sync */
=======
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
#include <string.h>

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) ((TickType_t)((((uint64_t)(x)) * configTICK_RATE_HZ) / 1000))
#endif

static uint8_t g_seq_counter = 0;
static volatile uint8_t g_ack_expected_seq = 0xFF;
static volatile bool g_ack_received = false;
static volatile bool g_ack_success = false;
static BleProto_DataHandler_t g_data_handler = NULL;
static volatile bool g_syncing = false;
static volatile bool g_sync_pending = false;
static uint32_t g_sync_pending_tick = 0;
#define BLE_PROTO_SYNC_DELAY_MS 500

static volatile uint32_t g_sync_complete_tick = 0;
#define BLE_PROTO_ACK_DRAIN_COOLDOWN_MS 300

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
/* 同步提供者回调：由 05_component/ble_app 层注册，
 * 负责读取应用层参数并调用 BleProto_SendSyncFrame 发送。
 * 02 层不直接依赖 05 层头文件，通过回调解耦。 */
typedef void (*BleProto_SyncProvider_t)(void);
static BleProto_SyncProvider_t g_sync_provider = NULL;

/* Shell待处理数据回调：由 04_shell_commands 层注册，
 * 替代 extern 声明 ShellIO_BLE_ProcessPending，避免02→04跨层依赖 */
static BleProto_ShellPendingHandler_t g_shell_pending_handler = NULL;

/* BLE Send初始化回调：由 04_shell_commands 层注册，
 * 替代 extern 声明 BLE_SendInit，避免02→04跨层依赖 */
static BleProto_SendInitHandler_t g_send_init_handler = NULL;

>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
static TaskHandle_t g_sync_task_handle = NULL;

/* 前向声明：在 BleProto_Process() 中使用 */
static void ble_sync_task_fn(void *params);

#define BLE_PROTO_PENDING_ACK_MAX 8
static struct {
    uint8_t seq;
    uint8_t cmd;
    bool    valid;
} g_pending_acks[BLE_PROTO_PENDING_ACK_MAX];
static volatile uint8_t g_pending_ack_count = 0;

static const uint16_t crc16_table[256] = {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x4864,0x5845,0x6826,0x7807,0x08E0,0x18C1,0x28A2,0x38C3,
    0xC92C,0xD90D,0xE96E,0xF94F,0x89A8,0x9989,0xA9EA,0xB9CB,
    0x5A15,0x4A34,0x7A57,0x6A76,0x1A91,0x0AB0,0x3AD3,0x2AF2,
    0xDB1D,0xCB3C,0xFB5F,0xEB7E,0x9B99,0x8BB8,0xBBDB,0xABFA,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
    0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
    0xB5EA,0xA5CB,0x95A8,0x85A9,0xF56E,0xE54F,0xD52C,0xC50D,
    0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
    0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
    0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
    0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
    0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
    0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
    0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
    0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
    0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
    0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0
};

uint16_t BleProto_Crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i;
    for (i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

uint16_t BleProto_Encode(const BleProtoFrame_t *frame, uint8_t *out, uint16_t out_size)
{
    uint16_t total = BLE_PROTO_HDR_SIZE + frame->len + BLE_PROTO_CRC_SIZE;
    uint16_t i;
    uint16_t crc;

    if (out_size < total || frame->len > BLE_PROTO_MAX_PAYLOAD) {
        return 0;
    }

    out[0] = BLE_PROTO_HEADER_0;
    out[1] = BLE_PROTO_HEADER_1;
    out[2] = frame->cmd;
    out[3] = frame->seq;

    for (i = 0; i < frame->len; i++) {
        out[BLE_PROTO_HDR_SIZE + i] = frame->payload[i];
    }

    crc = BleProto_Crc16(out, BLE_PROTO_HDR_SIZE + frame->len);
    out[BLE_PROTO_HDR_SIZE + frame->len]     = (uint8_t)(crc >> 8);
    out[BLE_PROTO_HDR_SIZE + frame->len + 1] = (uint8_t)(crc & 0xFF);

    return total;
}

bool BleProto_Decode(const uint8_t *data, uint16_t len, BleProtoFrame_t *frame)
{
    uint16_t crc_calc, crc_recv;

    if (len < BLE_PROTO_HDR_SIZE + BLE_PROTO_CRC_SIZE) {
        return false;
    }

    if (data[0] != BLE_PROTO_HEADER_0 || data[1] != BLE_PROTO_HEADER_1) {
        return false;
    }

    frame->cmd = data[2];
    frame->seq = data[3];

    if (frame->cmd == BLE_CMD_ACK || frame->cmd == BLE_CMD_NACK) {
        frame->len = (len > BLE_PROTO_HDR_SIZE) ? (uint8_t)(len - BLE_PROTO_HDR_SIZE - BLE_PROTO_CRC_SIZE) : 0;
    } else {
        frame->len = (uint8_t)(len - BLE_PROTO_HDR_SIZE - BLE_PROTO_CRC_SIZE);
    }

    if (frame->len > BLE_PROTO_MAX_PAYLOAD) {
        return false;
    }

    if (len < BLE_PROTO_HDR_SIZE + frame->len + BLE_PROTO_CRC_SIZE) {
        return false;
    }

    crc_calc = BleProto_Crc16(data, BLE_PROTO_HDR_SIZE + frame->len);
    crc_recv = ((uint16_t)data[BLE_PROTO_HDR_SIZE + frame->len] << 8) |
               data[BLE_PROTO_HDR_SIZE + frame->len + 1];

    if (crc_calc != crc_recv) {
        DBG("[BLE_PROTO] CRC mismatch: calc=0x%04X recv=0x%04X\n", crc_calc, crc_recv);
        return false;
    }

    if (frame->len > 0) {
        memcpy(frame->payload, data + BLE_PROTO_HDR_SIZE, frame->len);
    }

    return true;
}

uint8_t BleProto_NextSeq(void)
{
    uint8_t seq;
    taskENTER_CRITICAL();
    g_seq_counter++;
    if (g_seq_counter == 0xFF) {
        g_seq_counter = 1;
    }
    seq = g_seq_counter;
    taskEXIT_CRITICAL();
    return seq;
}

/* 互斥量：保护 send_frame() 的 static buf 在 encode+send 全程不被其他任务覆盖。
 * 主任务 (BleProto_SendOnce / ACK) 与 sync 子任务 (send_param_noack) 都会调用
 * send_frame()。sync 任务优先级更高，可以抢占主任务的 encode 阶段并覆盖 static buf，
 * 导致主任务随后通过 BLE_Send 发出损坏帧，使 BLE 栈崩溃。
 * 懒初始化：调度器启动后第一次调用时创建，无需修改启动流程。*/
static SemaphoreHandle_t s_frame_mutex = NULL;

static int send_frame(const BleProtoFrame_t *frame)
{
    /* static: 避免每次调用在栈上分配 206 字节，防止同步任务栈溢出 */
    static uint8_t buf[BLE_PROTO_MAX_FRAME];
    uint16_t len;
    int result;

    /* mutex 应在 BleProto_Init() 中创建；若未创建（罕见情况），跳过保护 */
    if (s_frame_mutex == NULL) {
        return -1;
    }
    if (xSemaphoreTake(s_frame_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return -1; /* 200ms 内未能获取，丢弃此帧，避免死锁 */
    }

    len = BleProto_Encode(frame, buf, sizeof(buf));
    if (len == 0) {
        xSemaphoreGive(s_frame_mutex);
        return -1;
    }
    result = (BLE_Send(buf, len) == len) ? 0 : -1;
    xSemaphoreGive(s_frame_mutex);
    return result;
}

static void send_ack(uint8_t seq, uint8_t acked_cmd)
{
    BleProtoFrame_t ack = {
        .cmd = BLE_CMD_ACK,
        .seq = seq,
        .len = 1,
        .payload = { acked_cmd }
    };
    send_frame(&ack);
    DBG("[BLE_PROTO] ACK sent: seq=%d cmd=0x%02X\n", seq, acked_cmd);
}

static void send_nack(uint8_t seq)
{
    BleProtoFrame_t nack = {
        .cmd = BLE_CMD_NACK,
        .seq = seq,
        .len = 0
    };
    send_frame(&nack);
    DBG("[BLE_PROTO] NACK sent: seq=%d\n", seq);
}

int BleProto_SendOnce(uint8_t cmd, const uint8_t *payload, uint8_t payload_len)
{
    BleProtoFrame_t frame;
    uint8_t seq;

    if (payload_len > BLE_PROTO_MAX_PAYLOAD) {
        return -1;
    }

    seq = BleProto_NextSeq();
    frame.cmd = cmd;
    frame.seq = seq;
    frame.len = payload_len;
    if (payload_len > 0 && payload != NULL) {
        memcpy(frame.payload, payload, payload_len);
    }

    /* 直接发送，不等 ACK，不重试 */
    return send_frame(&frame);
}

int BleProto_SendReliable(uint8_t cmd, const uint8_t *payload, uint8_t payload_len)
{
    BleProtoFrame_t frame;
    uint8_t seq;
    int retry;
    int result;

    if (!BLE_CMD_IS_DATA(cmd)) {
        return -1;
    }

    if (payload_len > BLE_PROTO_MAX_PAYLOAD) {
        return -1;
    }

    seq = BleProto_NextSeq();
    frame.cmd = cmd;
    frame.seq = seq;
    frame.len = payload_len;
    if (payload_len > 0 && payload != NULL) {
        memcpy(frame.payload, payload, payload_len);
    }

    for (retry = 0; retry < BLE_PROTO_MAX_RETRIES; retry++) {
        g_ack_expected_seq = seq;
        g_ack_received = false;
        g_ack_success = false;

        result = send_frame(&frame);
        if (result != 0) {
            DBG("[BLE_PROTO] Send failed, retry %d/%d\n", retry + 1, BLE_PROTO_MAX_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(BLE_PROTO_ACK_TIMEOUT_MS));
            continue;
        }

        DBG("[BLE_PROTO] Sent cmd=0x%02X seq=%d, waiting ACK (retry %d/%d)\n",
            cmd, seq, retry, BLE_PROTO_MAX_RETRIES);

        {
            uint32_t start = xTaskGetTickCount();
            while (!g_ack_received) {
                vTaskDelay(pdMS_TO_TICKS(10));
                if ((xTaskGetTickCount() - start) >= pdMS_TO_TICKS(BLE_PROTO_ACK_TIMEOUT_MS)) {
                    break;
                }
            }
        }

        if (g_ack_received && g_ack_success) {
            DBG("[BLE_PROTO] ACK received for seq=%d\n", seq);
            g_ack_expected_seq = 0xFF;
            return 0;
        }

        DBG("[BLE_PROTO] ACK timeout for seq=%d, retry %d/%d\n",
            seq, retry + 1, BLE_PROTO_MAX_RETRIES);
    }

    g_ack_expected_seq = 0xFF;
    DBG("[BLE_PROTO] Failed after %d retries: cmd=0x%02X seq=%d\n",
        BLE_PROTO_MAX_RETRIES, cmd, seq);
    return -1;
}

void BleProto_OnAckReceived(uint8_t seq, uint8_t acked_cmd)
{
    if (seq == g_ack_expected_seq) {
        g_ack_received = true;
        g_ack_success = true;
        DBG("[BLE_PROTO] ACK matched: seq=%d cmd=0x%02X\n", seq, acked_cmd);
    } else {
        DBG("[BLE_PROTO] ACK seq mismatch: expected=%d got=%d\n", g_ack_expected_seq, seq);
    }
}

void BleProto_OnNackReceived(uint8_t seq)
{
    if (seq == g_ack_expected_seq) {
        g_ack_received = true;
        g_ack_success = false;
        DBG("[BLE_PROTO] NACK received: seq=%d\n", seq);
    }
}

void BleProto_Init(void)
{
    g_seq_counter = 0;
    g_ack_expected_seq = 0xFF;
    g_ack_received = false;
    g_ack_success = false;
    g_syncing = false;
    g_data_handler = NULL;
    /* 在调度器启动前创建 mutex，避免懒初始化时在临界区调用 heap 分配（UB） */
    if (s_frame_mutex == NULL) {
        s_frame_mutex = xSemaphoreCreateMutex();
    }
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
>>>>>>> Stashed changes
    /* 同步初始化 BLE Send mutex（定义在 shell_io_ble.c) */
    {
        extern void BLE_SendInit(void);
        BLE_SendInit();
<<<<<<< Updated upstream
=======
=======
    /* 同步初始化 BLE Send mutex（通过回调，避免02→04跨层依赖） */
    if (g_send_init_handler != NULL) {
        g_send_init_handler();
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    }
}

static void queue_ack(uint8_t seq, uint8_t cmd)
{
    int i;
    for (i = 0; i < BLE_PROTO_PENDING_ACK_MAX; i++) {
        if (!g_pending_acks[i].valid) {
            g_pending_acks[i].seq = seq;
            g_pending_acks[i].cmd = cmd;
            g_pending_acks[i].valid = true;
            g_pending_ack_count++;
            DBG("[BLE_PROTO] ACK queued: seq=%d cmd=0x%02X\n", seq, cmd);
            return;
        }
    }
    DBG("[BLE_PROTO] ACK queue full! Dropping seq=%d cmd=0x%02X\n", seq, cmd);
}

void BleProto_Process(void)
{
    /* ACK 队列排空：同步任务运行期间跳过。
     * send_frame() 内部的 static buf 和 BLE_Send() 均非线程安全，若主循环
     * 同时调用 send_ack → send_frame 与同步任务的 send_param_noack → send_frame
     * 并发，会导致静态缓冲区数据互相覆盖、BLE 栈状态损坏，最终 exception。
     * 同步结束后需等待冷却期（BLE 栈恢复），再排空积压 ACK。*/
    if (!g_syncing && g_pending_ack_count > 0) {
        if (g_sync_complete_tick > 0) {
            uint32_t now = xTaskGetTickCount();
            if ((now - g_sync_complete_tick) < pdMS_TO_TICKS(BLE_PROTO_ACK_DRAIN_COOLDOWN_MS)) {
                goto check_sync;
            }
            g_sync_complete_tick = 0;
        }
        int i;
        for (i = 0; i < BLE_PROTO_PENDING_ACK_MAX; i++) {
            if (g_pending_acks[i].valid) {
                send_ack(g_pending_acks[i].seq, g_pending_acks[i].cmd);
                g_pending_acks[i].valid = false;
                g_pending_ack_count--;
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
    }

    /* 同步完成后处理延迟的 Shell 命令：
     * 同步期间收到的 BLE 命令被缓冲但未处理（避免 Shell_WriteRaw 与同步帧
     * 并发调用 BLE_Send 导致 BLE 栈崩溃）。需等待冷却期结束后安全处理。*/
    if (!g_syncing && g_sync_complete_tick == 0) {
<<<<<<< Updated upstream
        extern void ShellIO_BLE_ProcessPending(void);
        ShellIO_BLE_ProcessPending();
=======
<<<<<<< HEAD
        extern void ShellIO_BLE_ProcessPending(void);
        ShellIO_BLE_ProcessPending();
=======
        if (g_shell_pending_handler != NULL) {
            g_shell_pending_handler();
        }
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    }

check_sync:

    if (g_sync_pending) {
        uint32_t now = xTaskGetTickCount();
        if ((now - g_sync_pending_tick) >= pdMS_TO_TICKS(BLE_PROTO_SYNC_DELAY_MS)) {
            g_sync_pending = false;
            /* 在独立任务中执行同步，避免阻塞主循环/音频循环 */
            if (!g_syncing && g_sync_task_handle == NULL) {
                xTaskCreate(ble_sync_task_fn, "BleSync",
                            BLE_PROTO_SYNC_TASK_STACK,
                            NULL,
                            BLE_PROTO_SYNC_TASK_PRIO,
                            &g_sync_task_handle);
            }
        }
    }
}

void BleProto_RegisterDataHandler(BleProto_DataHandler_t handler)
{
    g_data_handler = handler;
}

bool BleProto_IsSyncing(void)
{
    return g_syncing;
}

void BleProto_OnDisconnected(void)
{
    /* 请在 BLE_STACK_DISCONNECTED 事件中调用。
     * 如果同步任务应坎栏溲出而崩溃，重置状态防止重连后圆锄弹窗永不关闭。
     * 注意：任务可能仍在运行，但断开后它再调用 BLE_Send 也会失败，让它自然结束 */
    g_syncing         = false;
    g_sync_pending    = false;
    g_sync_complete_tick = 0;
    g_sync_task_handle = NULL;  /* 只清句柄，任务本身将在 BleProto_StartSync 干完后自我删除 */
    DBG("[BLE_PROTO] OnDisconnected: sync state reset\n");
}

void BleProto_RequestSync(void)
{
    g_sync_pending = true;
    g_sync_pending_tick = xTaskGetTickCount();
    DBG("[BLE_PROTO] Sync requested, will start in %dms\n", BLE_PROTO_SYNC_DELAY_MS);
}

<<<<<<< Updated upstream
static void send_param_noack(uint8_t cmd, const uint8_t *payload, uint8_t len)
=======
<<<<<<< HEAD
static void send_param_noack(uint8_t cmd, const uint8_t *payload, uint8_t len)
=======
/**
 * @brief 同步帧发送（公共 API，供 05_component/ble_app 层调用）
 * @param cmd     命令字节
 * @param payload 负载数据（可为 NULL）
 * @param len     负载长度
 * @note 包含重试机制和帧间隔控制，仅在同步任务中调用。
 */
void BleProto_SendSyncFrame(uint8_t cmd, const uint8_t *payload, uint8_t len)
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
{
    /* 静态帧缓冲：负载最大只有 200 字节，当前层尢4块 buf 共耗 > 600字节导致栈溢出。
     * send_param_noack 仅在 BleProto_StartSync (单一子任务) 中调用，用 static 安全 */
    static BleProtoFrame_t s_frame;
    int retry;
    int result;

    if (len > BLE_PROTO_MAX_PAYLOAD) return;
    s_frame.cmd = cmd;
    s_frame.seq = BleProto_NextSeq();
    s_frame.len = len;
    if (len > 0 && payload != NULL) {
        memcpy(s_frame.payload, payload, len);
    }

    /* 重试机制：最多 SYNC_SEND_RETRIES 次，全部失败则跳过该参数，继续下一个 */
    result = -1;
    for (retry = 0; retry < BLE_PROTO_SYNC_SEND_RETRIES; retry++) {
        result = send_frame(&s_frame);
        if (result == 0) {
            DBG("[BLE_PROTO] Sync 0x%02X OK (try %d)\n", cmd, retry + 1);
            break;
        }
        DBG("[BLE_PROTO] WARN: Sync 0x%02X failed, retry %d/%d\n",
            cmd, retry + 1, BLE_PROTO_SYNC_SEND_RETRIES);
        if (retry < BLE_PROTO_SYNC_SEND_RETRIES - 1) {
            vTaskDelay(pdMS_TO_TICKS(BLE_PROTO_SYNC_RETRY_DELAY_MS));
        }
    }
    if (result != 0) {
        DBG("[BLE_PROTO] SKIP: cmd=0x%02X skipped after %d retries\n",
            cmd, BLE_PROTO_SYNC_SEND_RETRIES);
    }
    /* 帧间隔：无论成败都等待。目标 50ms > 典型 BLE 连接间隔上限，确保每帧在独立事件中到达 */
    vTaskDelay(pdMS_TO_TICKS(BLE_PROTO_SYNC_FRAME_GAP_MS));
}

/* 独立任务：执行全量参数同步，结束后自我删除，不阻塞主循环 */
static void ble_sync_task_fn(void *params)
{
<<<<<<< Updated upstream
    BleProto_StartSync();
=======
<<<<<<< HEAD
    BleProto_StartSync();
=======
    g_syncing = true;
    DBG("[BLE_PROTO] Sync task started\n");

    if (g_sync_provider != NULL) {
        g_sync_provider();
    } else {
        DBG("[BLE_PROTO] WARN: No sync provider registered!\n");
    }

    g_syncing = false;
    g_sync_complete_tick = xTaskGetTickCount();
    DBG("[BLE_PROTO] Sync task completed\n");

>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    /* 先把全局句柄清掉，再 vTaskDelete，避免主循环在任务删除期间访问已失效句柄 */
    g_sync_task_handle = NULL;
    vTaskDelete(NULL);
    /* 不会执行到这里 */
}

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
>>>>>>> Stashed changes
void BleProto_StartSync(void)
{
    SysParam_t *sp = SysParam_Get();
    /* 静态中间缓冲区：避免在栈上分配 200 字节导致栈溢出。
     * BleProto_StartSync 只在永不并发的单次子任务中调用， static 安全 */
    static uint8_t buf[BLE_PROTO_MAX_PAYLOAD];

    g_syncing = true;
    DBG("[BLE_PROTO] Starting full parameter sync (fire-and-forget)...\n");

    {
        uint8_t sync_start[1] = { 6 };
        send_param_noack(BLE_CMD_SYNC_START, sync_start, 1);
    }

    {
        buf[0] = sp->volume.mic1_volume;
        buf[1] = sp->volume.mic2_volume;
        buf[2] = sp->volume.guitar1_volume;
        buf[3] = sp->volume.guitar2_volume;
        buf[4] = sp->volume.output_volume;
        send_param_noack(BLE_CMD_VOLUME, buf, 5);
    }

    {
        EffectNode_t *node = EffectGraph_FindNodeById(10);
        if (node != NULL && node->type == EFFECT_NODE_TYPE_EFFECT_DRC) {
            buf[0] = (uint8_t)(node->params.drc.threshold & 0xFF);
            buf[1] = (uint8_t)((node->params.drc.threshold >> 8) & 0xFF);
            buf[2] = node->params.drc.ratio;
            buf[3] = node->params.drc.attack;
            buf[4] = node->params.drc.release;
            send_param_noack(BLE_CMD_DRC, buf, 5);
        }
    }

    {
        EffectNode_t *node = EffectGraph_FindNodeById(12);
        if (node != NULL && node->type == EFFECT_NODE_TYPE_EFFECT_REVERB) {
            buf[0] = node->params.reverb.room_size;
            buf[1] = node->params.reverb.damping;
            buf[2] = node->params.reverb.wet_dry;
            send_param_noack(BLE_CMD_REVERB, buf, 3);
        }
    }

    {
        EffectNode_t *node = EffectGraph_FindNodeById(5);
        if (node != NULL && node->type == EFFECT_NODE_TYPE_EFFECT_EQ) {
            int band_count = node->params.eq.band_count;
            int i;
            buf[0] = (uint8_t)band_count;
            buf[1] = (uint8_t)(node->params.eq.pregain & 0xFF);
            buf[2] = (uint8_t)((node->params.eq.pregain >> 8) & 0xFF);
            for (i = 0; i < band_count && i < 10; i++) {
                int base = 3 + i * 10;
                buf[base + 0] = (uint8_t)(node->params.eq.band_gains[i] & 0xFF);
                buf[base + 1] = 0;
                buf[base + 2] = (uint8_t)(node->params.eq.band_f0[i] & 0xFF);
                buf[base + 3] = (uint8_t)((node->params.eq.band_f0[i] >> 8) & 0xFF);
                buf[base + 4] = (uint8_t)((node->params.eq.band_f0[i] >> 16) & 0xFF);
                buf[base + 5] = (uint8_t)((node->params.eq.band_f0[i] >> 24) & 0xFF);
                buf[base + 6] = (uint8_t)(node->params.eq.band_Q[i] & 0xFF);
                buf[base + 7] = (uint8_t)((node->params.eq.band_Q[i] >> 8) & 0xFF);
                buf[base + 8] = node->params.eq.band_types[i];
                buf[base + 9] = node->params.eq.band_enables[i];
            }
            send_param_noack(BLE_CMD_EQ, buf, 3 + band_count * 10);
        }
    }

    {
        buf[0] = sp->looper.tempo;
        buf[1] = sp->looper.time_signature;
        buf[2] = sp->looper.click_volume;
        buf[3] = sp->looper.overdub_mode;
        buf[4] = sp->looper.quantize;
        send_param_noack(BLE_CMD_METRONOME, buf, 5);
    }

    {
        buf[0] = sp->looper.loop_count;
        buf[1] = sp->looper.overdub_mode;
        buf[2] = sp->looper.quantize;
        buf[3] = sp->looper.click_volume;
        buf[4] = sp->looper.tempo;
        buf[5] = sp->looper.time_signature;
        buf[6] = (uint8_t)(sp->looper.fade_time & 0xFF);
        buf[7] = (uint8_t)((sp->looper.fade_time >> 8) & 0xFF);
        buf[8] = sp->looper.segment_rec_source[0];
        buf[9] = sp->looper.segment_rec_source[1];
        buf[10] = sp->looper.segment_rec_source[2];
        buf[11] = sp->looper.segment_rec_source[3];
        buf[12] = sp->looper.export_mono_mix;                               /* 声道平衡 */
        buf[13] = (uint8_t)(sp->looper.export_gain_pct & 0xFF);            /* 导出增益 lo */
        buf[14] = (uint8_t)((sp->looper.export_gain_pct >> 8) & 0xFF);     /* 导出增益 hi */
        send_param_noack(BLE_CMD_LOOPER, buf, 15);
    }

    /* 同步各段运行时状态：断连重连后 App 需要知道哪些段有内容（STOPPED）
     * payload: [s0_state, s1_state, s2_state, s3_state,
     *           s0_len_lo, s0_len_hi, s1_len_lo, s1_len_hi,
     *           s2_len_lo, s2_len_hi, s3_len_lo, s3_len_hi]   共12字节 */
    {
        uint8_t i;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            buf[i] = (uint8_t)loop_get_segment_state(i);
        }
        for (i = 0; i < MAX_SEGMENTS; i++) {
            uint32_t len_pages = loop_get_segment_length_pages(i);
            /* 截断至 uint16_t：最大 65535 页 ≈ 16MB，足够表示实际录音长度 */
            uint16_t len16 = (len_pages > 0xFFFF) ? 0xFFFF : (uint16_t)len_pages;
            buf[MAX_SEGMENTS + i * 2]     = (uint8_t)(len16 & 0xFF);
            buf[MAX_SEGMENTS + i * 2 + 1] = (uint8_t)((len16 >> 8) & 0xFF);
        }
        send_param_noack(BLE_CMD_LOOPER_SEG_STATE, buf, MAX_SEGMENTS + MAX_SEGMENTS * 2);
    }

    {
        send_param_noack(BLE_CMD_SYNC_END, NULL, 0);
    }

    /* Send current battery level immediately after sync so App shows
     * correct SOC as soon as the toolbar connection indicator turns green. */
    {
        uint8_t bat_pl[2];
        bat_pl[0] = BLE_SYSTEM_SUB_BATTERY;
        bat_pl[1] = battery_get_soc();
        send_param_noack(BLE_CMD_SYSTEM, bat_pl, 2);
    }

    /* Send current auto-LP enabled state so App can sync the toggle. */
    {
        extern uint8_t LowPower_GetEnabled(void);
        extern uint8_t LowPower_GetTimeoutMin(void);
        uint8_t lp_pl[2];
        lp_pl[0] = BLE_SYSTEM_SUB_LP_STATE;
        lp_pl[1] = LowPower_GetEnabled();
        send_param_noack(BLE_CMD_SYSTEM, lp_pl, 2);
        /* Send idle timeout */
        lp_pl[0] = BLE_SYSTEM_SUB_LP_TIMEOUT;
        lp_pl[1] = LowPower_GetTimeoutMin();
        send_param_noack(BLE_CMD_SYSTEM, lp_pl, 2);
    }

    /* Send product ID so App can identify the device and load correct feature set. */
    {
        uint8_t pid_pl[3];
        pid_pl[0] = BLE_SYSTEM_SUB_PRODUCT_ID;
        pid_pl[1] = (uint8_t)(BG_PRODUCT_ID_BANBOX & 0xFF);
        pid_pl[2] = (uint8_t)((BG_PRODUCT_ID_BANBOX >> 8) & 0xFF);
        send_param_noack(BLE_CMD_SYSTEM, pid_pl, 3);
    }

    g_syncing = false;
    g_sync_complete_tick = xTaskGetTickCount();
    DBG("[BLE_PROTO] Full parameter sync completed\n");

    vTaskDelay(pdMS_TO_TICKS(100));
<<<<<<< Updated upstream
=======
=======
/**
 * @brief 注册同步提供者回调（由 05_component/ble_app 层调用）
 * @param provider  同步提供者函数指针，在同步任务中调用
 */
void BleProto_RegisterSyncProvider(void (*provider)(void))
{
    g_sync_provider = provider;
    DBG("[BLE_PROTO] Sync provider registered\n");
}

void BleProto_RegisterShellPendingHandler(BleProto_ShellPendingHandler_t handler)
{
    g_shell_pending_handler = handler;
    DBG("[BLE_PROTO] Shell pending handler registered\n");
}

void BleProto_RegisterSendInitHandler(BleProto_SendInitHandler_t handler)
{
    g_send_init_handler = handler;
    DBG("[BLE_PROTO] Send init handler registered\n");
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}

void BleProto_OnFrameReceived(const uint8_t *data, uint16_t len)
{
    BleProtoFrame_t frame;

    if (!BleProto_Decode(data, len, &frame)) {
        DBG("[BLE_PROTO] Invalid frame, len=%d\n", len);
        return;
    }

    DBG("[BLE_PROTO] RX: cmd=0x%02X seq=%d len=%d\n", frame.cmd, frame.seq, frame.len);

    switch (frame.cmd) {
    case BLE_CMD_ACK:
        if (frame.len >= 1) {
            BleProto_OnAckReceived(frame.seq, frame.payload[0]);
        }
        break;

    case BLE_CMD_NACK:
        BleProto_OnNackReceived(frame.seq);
        break;

    case BLE_CMD_SYNC_REQ:
        DBG("[BLE_PROTO] SYNC_REQ received, queuing sync\n");
        BleProto_RequestSync();
        break;

    default:
        if (BLE_CMD_IS_DATA(frame.cmd)) {
            queue_ack(frame.seq, frame.cmd);
            if (g_data_handler != NULL) {
                g_data_handler(&frame);
            }
        }
        break;
    }
}
