/**
 * @file    ble_app_sync.c
 * @brief   BLE 应用层参数同步与事件处理（从 02_device_drivers 层分离）
 * @author  BanGO
 *
 * 架构定位: 03_application_components/ble_app/
 *   本模块负责 BLE 连接后的应用层逻辑：
 *   1. 全量参数同步（读取 sys_param / effect_graph 等推送到 App）
 *   2. 处理来自 App 的数据命令
 *
 *   纯协议编解码留在 02_device_drivers/bluetooth/ble_protocol.c 中，
 *   通过回调函数指针解耦，02 层不直接 include 05 层头文件。
 */

#include "ble_app_sync.h"
#include "ble_protocol.h"
#include "bg_event.h"
#include "bg_event_topics.h"
#include "sys_param.h"
#include "effect_graph.h"
#include "battery_drv.h"
#include "product_features.h"

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) ((TickType_t)((((uint64_t)(x)) * configTICK_RATE_HZ) / 1000))
#endif
#include "debug.h"
#include "FreeRTOS.h"
#include "task.h"

/* ---- 低功耗管理接口（extern，避免 include 06_app 头文件） ---- */
extern uint8_t LowPower_GetEnabled(void);
extern uint8_t LowPower_GetTimeoutMin(void);

/* ================================================================
 *  全量参数同步（从 ble_protocol.c 的 BleProto_StartSync 迁移）
 * ================================================================ */

/**
 * @brief 全量参数同步 — 读取所有应用层参数并推送到 App
 * @note  在独立 FreeRTOS 任务中执行，不阻塞主循环。
 *        通过 BleProto_SendSyncFrame() 发送（02 层提供的公共 API）。
 */
static void BleApp_SyncProvider(void)
{
    SysParam_t *sp = SysParam_Get();
    static uint8_t buf[BLE_PROTO_MAX_PAYLOAD];

    DBG("[BLE_APP] Starting full parameter sync...\n");

    /* ---- SYNC_START ---- */
    {
        uint8_t sync_start[1] = { 6 };
        BleProto_SendSyncFrame(BLE_CMD_SYNC_START, sync_start, 1);
    }

    /* ---- VOLUME ---- */
    {
        buf[0] = sp->volume.mic1_volume;
        buf[1] = sp->volume.mic2_volume;
        buf[2] = sp->volume.guitar1_volume;
        buf[3] = sp->volume.guitar2_volume;
        buf[4] = sp->volume.output_volume;
        buf[5] = sp->volume.bt_max_volume;
        buf[6] = sp->volume.usb_max_volume;
        buf[7] = sp->volume.usb_out_volume;
        buf[8] = sp->volume.usb_out_mute;
        BleProto_SendSyncFrame(BLE_CMD_VOLUME, buf, 9);
    }

    /* ---- DRC ---- */
    {
        EffectNode_t *node = EffectGraph_FindNodeById(10);
        if (node != NULL && node->type == EFFECT_NODE_TYPE_EFFECT_DRC) {
            buf[0] = (uint8_t)(node->params.drc.threshold & 0xFF);
            buf[1] = (uint8_t)((node->params.drc.threshold >> 8) & 0xFF);
            buf[2] = node->params.drc.ratio;
            buf[3] = node->params.drc.attack;
            buf[4] = node->params.drc.release;
            BleProto_SendSyncFrame(BLE_CMD_DRC, buf, 5);
        }
    }

    /* ---- REVERB ---- */
    {
        EffectNode_t *node = EffectGraph_FindNodeById(12);
        if (node != NULL && node->type == EFFECT_NODE_TYPE_EFFECT_REVERB) {
            buf[0] = node->params.reverb.room_size;
            buf[1] = node->params.reverb.damping;
            buf[2] = node->params.reverb.wet_dry;
            BleProto_SendSyncFrame(BLE_CMD_REVERB, buf, 3);
        }
    }

    /* ---- EQ ---- */
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
            BleProto_SendSyncFrame(BLE_CMD_EQ, buf, 3 + band_count * 10);
        }
    }

    /* ---- SYNC_END ---- */
    {
        BleProto_SendSyncFrame(BLE_CMD_SYNC_END, NULL, 0);
    }

    /* ---- BATTERY ---- */
    {
        uint8_t bat_pl[2];
        bat_pl[0] = BLE_SYSTEM_SUB_BATTERY;
        bat_pl[1] = battery_get_soc();
        BleProto_SendSyncFrame(BLE_CMD_SYSTEM, bat_pl, 2);
    }

    /* ---- LOW POWER STATE ---- */
    {
        uint8_t lp_pl[2];
        lp_pl[0] = BLE_SYSTEM_SUB_LP_STATE;
        lp_pl[1] = LowPower_GetEnabled();
        BleProto_SendSyncFrame(BLE_CMD_SYSTEM, lp_pl, 2);
        lp_pl[0] = BLE_SYSTEM_SUB_LP_TIMEOUT;
        lp_pl[1] = LowPower_GetTimeoutMin();
        BleProto_SendSyncFrame(BLE_CMD_SYSTEM, lp_pl, 2);
    }

    /* ---- PRODUCT ID ---- */
    {
        uint8_t pid_pl[3];
        pid_pl[0] = BLE_SYSTEM_SUB_PRODUCT_ID;
        pid_pl[1] = (uint8_t)(BG_PRODUCT_ID_BANBOX & 0xFF);
        pid_pl[2] = (uint8_t)((BG_PRODUCT_ID_BANBOX >> 8) & 0xFF);
        BleProto_SendSyncFrame(BLE_CMD_SYSTEM, pid_pl, 3);
    }

    /* ---- FEATURE LIST (产品功能列表, const 字符串, 不可更改) ---- */
    {
        const char *feat = ProductFeature_GetList();
        uint8_t feat_len = (uint8_t)ProductFeature_GetLength();
        static uint8_t feat_buf[BLE_PROTO_MAX_PAYLOAD];
        feat_buf[0] = BLE_SYSTEM_SUB_FEATURE_LIST;
        /* feat_len 最大 ~175 字节, +1 子命令 = ~176, 在 200 字节限制内 */
        if (feat_len > BLE_PROTO_MAX_PAYLOAD - 1)
            feat_len = BLE_PROTO_MAX_PAYLOAD - 1;
        memcpy(&feat_buf[1], feat, feat_len);
        BleProto_SendSyncFrame(BLE_CMD_SYSTEM, feat_buf, feat_len + 1);
    }

    DBG("[BLE_APP] Full parameter sync completed\n");
    vTaskDelay(pdMS_TO_TICKS(100));
}

/* ================================================================
 *  BLE 数据命令处理（从 main.c 的 ble_data_cmd_dispatch 迁移）
 * ================================================================ */

/* 电池校准命令处理（extern，定义在 battery_calib.c） */
extern void BattCalib_HandleBleCmd(const uint8_t *payload, uint8_t len);

static void ble_app_data_handler(const BleProtoFrame_t *frame)
{
    switch (frame->cmd) {
    case BLE_CMD_BATTERY_CALIB:
        BattCalib_HandleBleCmd(frame->payload, frame->len);
        break;

    case BLE_CMD_SYSTEM:
        /* App → MCU: 请求产品功能列表 */
        if (frame->payload != NULL && frame->len >= 1 &&
            frame->payload[0] == BLE_SYSTEM_SUB_FEATURE_LIST)
        {
            const char *feat = ProductFeature_GetList();
            uint8_t feat_len = (uint8_t)ProductFeature_GetLength();
            static uint8_t feat_buf[BLE_PROTO_MAX_PAYLOAD];
            feat_buf[0] = BLE_SYSTEM_SUB_FEATURE_LIST;
            if (feat_len > BLE_PROTO_MAX_PAYLOAD - 1)
                feat_len = BLE_PROTO_MAX_PAYLOAD - 1;
            memcpy(&feat_buf[1], feat, feat_len);
            BleProto_SendReliable(BLE_CMD_SYSTEM, feat_buf, feat_len + 1);
        }
        break;

    default:
        DBG("[BLE_APP] Unhandled data cmd: 0x%02X\n", frame->cmd);
        break;
    }
}

/* ================================================================
 *  公共 API
 * ================================================================ */

void BleApp_Init(void)
{
    /* 注册参数同步提供者（02 层 ble_protocol.c 通过回调调用） */
    BleProto_RegisterSyncProvider(BleApp_SyncProvider);

    /* 注册数据命令处理器（02 层 ble_protocol.c 收到数据帧后转发） */
    BleProto_RegisterDataHandler(ble_app_data_handler);

    /* 注册 Shell 待处理数据回调（02 层 ble_protocol.c 同步完成后调用） */
    {
        extern void ShellIO_BLE_ProcessPending(void);
        BleProto_RegisterShellPendingHandler(ShellIO_BLE_ProcessPending);
    }

    /* 注册 BLE Send 初始化回调（02 层 ble_protocol.c 初始化时调用） */
    {
        extern void BLE_SendInit(void);
        BleProto_RegisterSendInitHandler(BLE_SendInit);
    }

    DBG("[BLE_APP] Initialized (sync provider + data handler + shell pending + send init registered)\n");
}

void BleApp_OnDisconnected(void)
{
    /* BanEffector (BLE-only): 无 looper/metronome 需要在断开时停止。
     * 保留此钩子以便后续添加 BLE 断开时的应用层清理。 */
    DBG("[BLE_APP] Disconnected\n");
}

void BleApp_StartSync(void)
{
    BleProto_RequestSync();
}

void BleApp_HandleDataCmd(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    BleProtoFrame_t frame;
    frame.cmd = cmd;
    frame.len = len;
    if (len > 0 && payload != NULL) {
        memcpy(frame.payload, payload, len > BLE_PROTO_MAX_PAYLOAD ? BLE_PROTO_MAX_PAYLOAD : len);
    }
    ble_app_data_handler(&frame);
}
