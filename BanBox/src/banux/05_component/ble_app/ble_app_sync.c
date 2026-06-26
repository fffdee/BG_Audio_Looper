/**
 * @file    ble_app_sync.c
 * @brief   BLE 应用层参数同步与事件处理（从 02_device_drivers 层分离）
 * @author  BanGO
 *
 * 架构定位: 05_component/ble_app/
 *   本模块负责 BLE 连接后的应用层逻辑：
 *   1. 全量参数同步（读取 sys_param / effect_graph / audio_looper 等推送到 App）
 *   2. BLE 断开时停止 Looper/节拍器
 *   3. 处理来自 App 的数据命令
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
#include "audio_looper.h"
#include "metronome.h"
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

    /* ---- METRONOME ---- */
    {
        buf[0] = sp->looper.tempo;
        buf[1] = sp->looper.time_signature;
        buf[2] = sp->looper.click_volume;
        buf[3] = sp->looper.overdub_mode;
        buf[4] = sp->looper.quantize;
        BleProto_SendSyncFrame(BLE_CMD_METRONOME, buf, 5);
    }

    /* ---- LOOPER ---- */
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
        buf[12] = sp->looper.export_mono_mix;
        buf[13] = (uint8_t)(sp->looper.export_gain_pct & 0xFF);
        buf[14] = (uint8_t)((sp->looper.export_gain_pct >> 8) & 0xFF);
        BleProto_SendSyncFrame(BLE_CMD_LOOPER, buf, 15);
    }

    /* ---- LOOPER SEGMENT STATE ---- */
    {
        uint8_t i;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            buf[i] = (uint8_t)loop_get_segment_state(i);
        }
        for (i = 0; i < MAX_SEGMENTS; i++) {
            uint32_t len_pages = loop_get_segment_length_pages(i);
            uint16_t len16 = (len_pages > 0xFFFF) ? 0xFFFF : (uint16_t)len_pages;
            buf[MAX_SEGMENTS + i * 2]     = (uint8_t)(len16 & 0xFF);
            buf[MAX_SEGMENTS + i * 2 + 1] = (uint8_t)((len16 >> 8) & 0xFF);
        }
        BleProto_SendSyncFrame(BLE_CMD_LOOPER_SEG_STATE, buf, MAX_SEGMENTS + MAX_SEGMENTS * 2);
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
 *  BLE 断开事件处理（从 ble_app_callback.c 迁移）
 * ================================================================ */

static void on_ble_disconnected(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    uint8_t i;
    (void)topic; (void)data; (void)size;

    /* 蓝牙断开时仅停止正在播放或录制的段（不动 INACTIVE/STOPPED 段，不清数据） */
    for (i = 0; i < MAX_SEGMENTS; i++) {
        SegmentState_t segState = loop_get_segment_state(i);
        if (segState == SEGMENT_PLAYING || segState == SEGMENT_RECORDING) {
            loop_set_segment_stopped(i);
        }
    }

    metronome_disable();
    DBG("[BLE_APP] Disconnect: looper+metronome stopped\n");
}

/* 编译期静态订阅 EVT_BLE_DISCONNECTED 事件 */
BG_EVT_SUB(EVT_BLE_DISCONNECTED, on_ble_disconnected);

/* ================================================================
 *  BLE 数据命令处理（从 main.c 的 ble_data_cmd_dispatch 迁移）
 * ================================================================ */

/* 电池校准命令处理（extern，定义在 battery_calib.c） */
extern void BattCalib_HandleBleCmd(const uint8_t *payload, uint8_t len);

/* WAV 导出命令处理（extern，定义在 looper_wav_ble_export.c） */
extern void LooperWavBle_HandleCommand(const uint8_t *payload, uint8_t len);

static void ble_app_data_handler(const BleProtoFrame_t *frame)
{
    switch (frame->cmd) {
    case BLE_CMD_BATTERY_CALIB:
        BattCalib_HandleBleCmd(frame->payload, frame->len);
        break;

    case BLE_CMD_WAV_EXPORT:
        LooperWavBle_HandleCommand(frame->payload, frame->len);
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
    /* 直接调用事件处理（也可通过事件系统自动触发） */
    on_ble_disconnected(EVT_BLE_DISCONNECTED, NULL, 0);
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
