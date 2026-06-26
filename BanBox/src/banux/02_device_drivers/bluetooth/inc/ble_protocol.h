#ifndef __BLE_PROTOCOL_H__
#define __BLE_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>

#define BLE_PROTO_HEADER_0      0xAA
#define BLE_PROTO_HEADER_1      0x55
#define BLE_PROTO_HDR_SIZE      4
#define BLE_PROTO_CRC_SIZE      2
#define BLE_PROTO_MAX_PAYLOAD   200
#define BLE_PROTO_MAX_FRAME     (BLE_PROTO_HDR_SIZE + BLE_PROTO_MAX_PAYLOAD + BLE_PROTO_CRC_SIZE)
#define BLE_PROTO_ACK_TIMEOUT_MS 200
#define BLE_PROTO_MAX_RETRIES   5

/* 同步参数发送重试配置 */
#define BLE_PROTO_SYNC_SEND_RETRIES   3    /* 每帧最多重试次数，失败后跳过继续下一帧 */
#define BLE_PROTO_SYNC_RETRY_DELAY_MS 30   /* 每次重试之间的等待时间 (ms) */
#define BLE_PROTO_SYNC_FRAME_GAP_MS   50   /* 参数同步帧间隔 — 50ms > 典型 BLE 连接间隔，降低栈负载 */
#define BLE_PROTO_SYNC_TASK_STACK     1024 /* 同步子任务栈大小 (StackType_t 单元) -- 4KB，防止栈溢出 */
#define BLE_PROTO_SYNC_TASK_PRIO      3    /* 同步子任务优先级 */

typedef enum {
    BLE_CMD_ACK         = 0x00,
    BLE_CMD_NACK        = 0x01,
    BLE_CMD_SYNC_REQ    = 0x02,
    BLE_CMD_SYNC_START  = 0x03,
    BLE_CMD_SYNC_END    = 0x04,
    BLE_CMD_DRC         = 0x10,
    BLE_CMD_REVERB      = 0x11,
    BLE_CMD_EQ          = 0x12,
    BLE_CMD_DELAY       = 0x13,
    BLE_CMD_GAIN        = 0x14,
    BLE_CMD_LOOPER          = 0x20,
    BLE_CMD_LOOPER_SEG_STATE = 0x21,  /* MCU→App: 各段运行时状态（重连同步用）
                                        * payload: [seg0_state, seg1_state, seg2_state, seg3_state,
                                        *           seg0_len_lo, seg0_len_hi,
                                        *           seg1_len_lo, seg1_len_hi,
                                        *           seg2_len_lo, seg2_len_hi,
                                        *           seg3_len_lo, seg3_len_hi]
                                        * state: 0=INACTIVE 1=RECORDING 2=PLAYING 3=STOPPED
                                        * len: segment length_pages (uint16_t LE) */
    BLE_CMD_VOLUME      = 0x30,
    BLE_CMD_METRONOME   = 0x31,
    BLE_CMD_SYSTEM          = 0x32,
    BLE_CMD_BATTERY_CALIB   = 0x33,
    BLE_CMD_WAV_EXPORT      = 0x40,
} BleProtoCmd_t;

/* BLE_CMD_SYSTEM sub-command types */
#define BLE_SYSTEM_SUB_BATTERY  0x01
#define BLE_SYSTEM_SUB_STATE    0x02   /* MCU → App: 系统状态通知 */
#define BLE_SYSTEM_SUB_LP_STATE   0x03   /* MCU → App: 自动低功耗启用状态 (payload[1]: 1=启用, 0=禁用) */
#define BLE_SYSTEM_SUB_LP_TIMEOUT 0x04   /* MCU → App: 自动低功耗空闲超时（payload[1]: 分钟数 1-60） */
                                       /* App → MCU: 通过文本命令 "lp 1"/"lp 0" 控制 */
#define BLE_SYSTEM_SUB_PRODUCT_ID 0x05  /* MCU → App: 产品标识 (payload[1]: 产品ID低字节, payload[2]: 产品ID高字节) */
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
#define BLE_SYSTEM_SUB_FEATURE_LIST 0x06  /* MCU → App: 产品功能列表 (payload[1..N]: JSON 字符串, 不可更改) */
                                         /* App → MCU: 发送此子命令请求功能列表, MCU 回复同子命令 */
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes

/* 产品 ID 定义 */
#define BG_PRODUCT_ID_BANBOX     0x0001  /* BanBox 音效器 */

/* BLE_SYSTEM_SUB_STATE payload[1] 值 */
#define BLE_SYS_STATE_IDLE      0x00   /* 空闲态：无音频 I/O，低功耗模式 */
#define BLE_SYS_STATE_NORMAL    0x01   /* 正常态：音频系统活跃 */
#define BLE_SYS_STATE_TRANSFER  0x02   /* 数据传输态：WAV/OTA 大数据传输中，音频已静音 */

/* BLE_CMD_BATTERY_CALIB — forwarded to BattCalib_HandleBleCmd() */
#define BLE_CALIB_CMD_START      0x01u
#define BLE_CALIB_CMD_STOP       0x02u
#define BLE_CALIB_CMD_STATUS     0x03u
#define BLE_CALIB_CMD_CLEAR      0x04u
#define BLE_CALIB_CMD_STATUS_RSP 0x83u

#define BLE_CMD_IS_DATA(cmd)  ((cmd) >= 0x10)

typedef struct {
    uint8_t  cmd;
    uint8_t  seq;
    uint8_t  len;
    uint8_t  payload[BLE_PROTO_MAX_PAYLOAD];
    uint16_t crc;
} BleProtoFrame_t;

typedef void (*BleProto_AckCallback_t)(uint8_t seq, bool success);

uint16_t BleProto_Crc16(const uint8_t *data, uint16_t len);

uint16_t BleProto_Encode(const BleProtoFrame_t *frame, uint8_t *out, uint16_t out_size);

bool BleProto_Decode(const uint8_t *data, uint16_t len, BleProtoFrame_t *frame);

uint8_t BleProto_NextSeq(void);

int BleProto_SendReliable(uint8_t cmd, const uint8_t *payload, uint8_t payload_len);

/**
 * @brief 单次发送，不等待 ACK（用于批量数据流，如 WAV 导出数据包）。
 * @return 0=发送成功, -1=BLE 未就绪或发送失败（调用方下次 tick 重试）
 */
int BleProto_SendOnce(uint8_t cmd, const uint8_t *payload, uint8_t payload_len);

void BleProto_OnAckReceived(uint8_t seq, uint8_t acked_cmd);

void BleProto_OnNackReceived(uint8_t seq);

void BleProto_Init(void);

void BleProto_Process(void);

<<<<<<< Updated upstream
void BleProto_StartSync(void);
=======
<<<<<<< HEAD
void BleProto_StartSync(void);
=======
/**
 * @brief 注册同步提供者回调（由 05_component/ble_app 层调用）
 * @param provider  同步提供者函数指针，在同步任务中调用
 * @note  02 层不直接依赖 05 层头文件，通过回调解耦。
 */
void BleProto_RegisterSyncProvider(void (*provider)(void));

/**
 * @brief 同步帧发送（公共 API，供 05_component/ble_app 层调用）
 * @param cmd     命令字节
 * @param payload 负载数据（可为 NULL）
 * @param len     负载长度
 * @note 包含重试机制和帧间隔控制，仅在同步任务中调用。
 */
void BleProto_SendSyncFrame(uint8_t cmd, const uint8_t *payload, uint8_t len);
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes

void BleProto_RequestSync(void);

bool BleProto_IsSyncing(void);

/**
 * @brief BLE 断开时调用。清除同步任务完整状态，防止重连后状态残留。
 */
void BleProto_OnDisconnected(void);

typedef void (*BleProto_DataHandler_t)(const BleProtoFrame_t *frame);
void BleProto_RegisterDataHandler(BleProto_DataHandler_t handler);

<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
/**
 * @brief 注册 Shell 待处理数据回调（由 04_shell_commands 层调用）
 * @param handler  ShellIO_BLE_ProcessPending 函数指针
 * @note  02 层不直接依赖 04 层头文件，通过回调解耦。
 */
typedef void (*BleProto_ShellPendingHandler_t)(void);
void BleProto_RegisterShellPendingHandler(BleProto_ShellPendingHandler_t handler);

/**
 * @brief 注册 BLE Send 初始化回调（由 04_shell_commands 层调用）
 * @param handler  BLE_SendInit 函数指针
 * @note  02 层不直接依赖 04 层头文件，通过回调解耦。
 */
typedef void (*BleProto_SendInitHandler_t)(void);
void BleProto_RegisterSendInitHandler(BleProto_SendInitHandler_t handler);

>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
#endif
