#ifndef __BLE_MIDI_H__
#define __BLE_MIDI_H__

#include <ble_midi_config.h>  // 包含配置文件
#include "type.h"
#include "bt_config.h"        // 包含BLE_SUPPORT定义

#if (BLE_SUPPORT && BLE_MIDI_ENABLE)

//***********************************************************************************
// BLE MIDI Data Processing Functions
//***********************************************************************************

/**
 * @brief 发送BLE MIDI数据包
 * @param conn_handle 连接句柄
 * @param midi_data MIDI数据
 * @param midi_len MIDI数据长度
 * @return 0=成功, 其他=失败
 */
int ble_midi_send_data(uint16_t conn_handle, uint8_t *midi_data, uint8_t midi_len);

/**
 * @brief 解析BLE MIDI数据包
 * @param ble_midi_data BLE MIDI数据包
 * @param ble_midi_len BLE MIDI数据包长度
 * @param midi_data 输出的MIDI数据
 * @param midi_len 输出的MIDI数据长度
 * @return 解析的MIDI消息数量
 */
int ble_midi_parse_data(uint8_t *ble_midi_data, uint8_t ble_midi_len, 
                       uint8_t *midi_data, uint8_t *midi_len);

//***********************************************************************************
// Standard MIDI Message Functions
//***********************************************************************************

/**
 * @brief 发送MIDI Note On消息
 * @param conn_handle 连接句柄
 * @param channel MIDI通道 (0-15)
 * @param note 音符 (0-127)
 * @param velocity 力度 (0-127)
 */
int midi_send_note_on(uint16_t conn_handle, uint8_t channel, uint8_t note, uint8_t velocity);

/**
 * @brief 发送MIDI Note Off消息
 * @param conn_handle 连接句柄
 * @param channel MIDI通道 (0-15)
 * @param note 音符 (0-127)
 * @param velocity 力度 (0-127)
 */
int midi_send_note_off(uint16_t conn_handle, uint8_t channel, uint8_t note, uint8_t velocity);

/**
 * @brief 发送MIDI Control Change消息
 * @param conn_handle 连接句柄
 * @param channel MIDI通道 (0-15)
 * @param controller 控制器编号 (0-127)
 * @param value 控制器值 (0-127)
 */
int midi_send_control_change(uint16_t conn_handle, uint8_t channel, uint8_t controller, uint8_t value);

/**
 * @brief 发送MIDI Program Change消息
 * @param conn_handle 连接句柄
 * @param channel MIDI通道 (0-15)
 * @param program 程序编号 (0-127)
 */
int midi_send_program_change(uint16_t conn_handle, uint8_t channel, uint8_t program);

/**
 * @brief 发送MIDI Pitch Bend消息
 * @param conn_handle 连接句柄
 * @param channel MIDI通道 (0-15)
 * @param pitch_bend 弯音值 (0-16383, 8192为中心)
 */
int midi_send_pitch_bend(uint16_t conn_handle, uint8_t channel, uint16_t pitch_bend);

/**
 * @brief 解析接收到的MIDI消息
 * @param midi_data MIDI数据
 * @param midi_len MIDI数据长度
 */
void midi_parse_message(uint8_t *midi_data, uint8_t midi_len);

/**
 * @brief 处理接收到的BLE MIDI数据
 * @param conn_handle 连接句柄
 * @param handle 特征句柄
 * @param data 接收到的数据
 * @param data_len 数据长度
 */
void ble_midi_handle_received_data(uint16_t conn_handle, uint16_t handle, uint8_t *data, uint8_t data_len);

//***********************************************************************************
// BLE MIDI Demo Functions (仅在BLE_MIDI_DEMO_ENABLE=1时编译)
//***********************************************************************************

#if (BLE_MIDI_DEMO_ENABLE)

/**
 * @brief BLE MIDI演示函数 - 发送测试音符
 * @param conn_handle 连接句柄
 */
void ble_midi_demo_send_notes(uint16_t conn_handle);

/**
 * @brief 启动BLE MIDI演示定时器
 * @param conn_handle 连接句柄
 */
void ble_midi_start_demo_timer(uint16_t conn_handle);

/**
 * @brief 停止BLE MIDI演示定时器
 */
void ble_midi_stop_demo_timer(void);

/**
 * @brief BLE MIDI演示处理函数 - 需要在主循环中定期调用
 */
void ble_midi_demo_handler(void);

/**
 * @brief BLE MIDI简化演示函数 - 直接发送Note 60
 * @param conn_handle 连接句柄
 */
void ble_midi_demo_send_note60(uint16_t conn_handle);

#endif // BLE_MIDI_DEMO_ENABLE

//***********************************************************************************
// BLE MIDI 通用测试函数（独立于Demo宏定义）
//***********************************************************************************

/**
 * @brief BLE MIDI强制测试函数 - 用于调试
 * 此函数独立于任何Demo配置，只要BLE MIDI功能启用就可以使用
 */
void ble_midi_force_test(void);

//***********************************************************************************
// BLE MIDI 蜜雪冰城Demo Functions (仅在BLE_MIDI_MIXUE_DEMO_ENABLE=1时编译)
//***********************************************************************************

#if (BLE_MIDI_MIXUE_DEMO_ENABLE)

/**
 * @brief 启动蜜雪冰城Demo
 * @param conn_handle 连接句柄
 */
void ble_midi_start_mixue_demo(uint16_t conn_handle);

/**
 * @brief 停止蜜雪冰城Demo
 */
void ble_midi_stop_mixue_demo(void);

/**
 * @brief 蜜雪冰城Demo处理函数 - 需要在主循环中定期调用
 */
void ble_midi_mixue_demo_handler(void);

#endif // BLE_MIDI_MIXUE_DEMO_ENABLE

/**
 * @brief 发送库乐队兼容的MIDI Note（包含完整的Note On/Off序列）
 * @param conn_handle 连接句柄
 * @param channel MIDI通道 (0-15)
 * @param note 音符 (0-127)
 * @param velocity 力度 (0-127)
 * @param duration_ms 音符持续时间（毫秒）
 */
int midi_send_note_garageband_compatible(uint16_t conn_handle, uint8_t channel, uint8_t note, uint8_t velocity, uint16_t duration_ms);

/**
 * @brief 发送MIDI All Notes Off消息（紧急停止所有声音）
 * @param conn_handle 连接句柄
 * @param channel MIDI通道 (0-15)
 */
int midi_send_all_notes_off(uint16_t conn_handle, uint8_t channel);

/**
 * @brief 发送MIDI All Sound Off消息（立即停止所有声音）
 * @param conn_handle 连接句柄  
 * @param channel MIDI通道 (0-15)
 */
int midi_send_all_sound_off(uint16_t conn_handle, uint8_t channel);

/**
 * @brief 库乐队兼容性测试函数
 * 发送一系列标准MIDI消息来测试兼容性
 */
void ble_midi_garageband_test(void);

//***********************************************************************************
// MIDI 1.0 Data Structures and Enums
//***********************************************************************************

/**
 * MIDI消息类型枚举
 */
typedef enum {
    // Channel Voice Messages (0x80-0xEF)
    MIDI_NOTE_OFF           = 0x80,
    MIDI_NOTE_ON            = 0x90,
    MIDI_POLY_AFTERTOUCH    = 0xA0,
    MIDI_CONTROL_CHANGE     = 0xB0,
    MIDI_PROGRAM_CHANGE     = 0xC0,
    MIDI_CHANNEL_AFTERTOUCH = 0xD0,
    MIDI_PITCH_BEND         = 0xE0,
    
    // System Common Messages (0xF0-0xF7)
    MIDI_SYSTEM_EXCLUSIVE   = 0xF0,
    MIDI_TIME_CODE_QUARTER  = 0xF1,
    MIDI_SONG_POSITION      = 0xF2,
    MIDI_SONG_SELECT        = 0xF3,
    MIDI_TUNE_REQUEST       = 0xF6,
    MIDI_SYSTEM_EXCLUSIVE_END = 0xF7,
    
    // System Real-Time Messages (0xF8-0xFF)
    MIDI_TIMING_CLOCK       = 0xF8,
    MIDI_START              = 0xFA,
    MIDI_CONTINUE           = 0xFB,
    MIDI_STOP               = 0xFC,
    MIDI_ACTIVE_SENSING     = 0xFE,
    MIDI_SYSTEM_RESET       = 0xFF
} midi_message_type_t;

/**
 * MIDI控制器类型枚举（常用CC）
 */
typedef enum {
    MIDI_CC_BANK_SELECT_MSB     = 0,
    MIDI_CC_MODULATION          = 1,
    MIDI_CC_BREATH_CONTROLLER   = 2,
    MIDI_CC_FOOT_CONTROLLER     = 4,
    MIDI_CC_PORTAMENTO_TIME     = 5,
    MIDI_CC_DATA_ENTRY_MSB      = 6,
    MIDI_CC_VOLUME              = 7,
    MIDI_CC_BALANCE             = 8,
    MIDI_CC_PAN                 = 10,
    MIDI_CC_EXPRESSION          = 11,
    MIDI_CC_BANK_SELECT_LSB     = 32,
    MIDI_CC_DATA_ENTRY_LSB      = 38,
    MIDI_CC_SUSTAIN_PEDAL       = 64,
    MIDI_CC_PORTAMENTO          = 65,
    MIDI_CC_SOSTENUTO_PEDAL     = 66,
    MIDI_CC_SOFT_PEDAL          = 67,
    MIDI_CC_LEGATO_PEDAL        = 68,
    MIDI_CC_HOLD_2_PEDAL        = 69,
    MIDI_CC_SOUND_VARIATION     = 70,
    MIDI_CC_SOUND_TIMBRE        = 71,
    MIDI_CC_SOUND_RELEASE_TIME  = 72,
    MIDI_CC_SOUND_ATTACK_TIME   = 73,
    MIDI_CC_SOUND_BRIGHTNESS    = 74,
    MIDI_CC_REVERB_LEVEL        = 91,
    MIDI_CC_CHORUS_LEVEL        = 93,
    MIDI_CC_ALL_SOUND_OFF       = 120,
    MIDI_CC_ALL_CONTROLLERS_OFF = 121,
    MIDI_CC_LOCAL_KEYBOARD      = 122,
    MIDI_CC_ALL_NOTES_OFF       = 123,
    MIDI_CC_OMNI_MODE_OFF       = 124,
    MIDI_CC_OMNI_MODE_ON        = 125,
    MIDI_CC_MONO_MODE_ON        = 126,
    MIDI_CC_POLY_MODE_ON        = 127
} midi_cc_type_t;

/**
 * MIDI消息结构体
 */
typedef struct {
    uint8_t status;         // 状态字节
    uint8_t data1;          // 数据字节1
    uint8_t data2;          // 数据字节2（某些消息不使用）
    uint8_t channel;        // MIDI通道 (0-15)
    uint8_t length;         // 消息长度 (1-3字节)
} ble_midi_message_t;

/**
 * MIDI SYSEX消息结构体
 */
typedef struct {
    uint8_t *data;          // SYSEX数据指针
    uint16_t length;        // SYSEX数据长度
    uint8_t manufacturer_id; // 制造商ID
    uint8_t device_id;      // 设备ID
    bool is_complete;       // 是否完整接收
} ble_midi_sysex_t;

/**
 * MIDI事件回调函数类型定义
 */
typedef void (*ble_midi_note_callback_t)(uint16_t conn_handle, uint8_t channel, uint8_t note, uint8_t velocity);
typedef void (*ble_midi_cc_callback_t)(uint16_t conn_handle, uint8_t channel, uint8_t controller, uint8_t value);
typedef void (*ble_midi_program_change_callback_t)(uint16_t conn_handle, uint8_t channel, uint8_t program);
typedef void (*ble_midi_pitch_bend_callback_t)(uint16_t conn_handle, uint8_t channel, uint16_t value);
typedef void (*ble_midi_aftertouch_callback_t)(uint16_t conn_handle, uint8_t channel, uint8_t pressure);
typedef void (*ble_midi_poly_aftertouch_callback_t)(uint16_t conn_handle, uint8_t channel, uint8_t note, uint8_t pressure);
typedef void (*ble_midi_sysex_callback_t)(uint16_t conn_handle, const ble_midi_sysex_t *sysex);
typedef void (*ble_midi_realtime_callback_t)(uint16_t conn_handle, uint8_t message);

/**
 * BLE MIDI回调函数集合
 */
typedef struct {
    ble_midi_note_callback_t note_on;
    ble_midi_note_callback_t note_off;
    ble_midi_cc_callback_t control_change;
    ble_midi_program_change_callback_t program_change;
    ble_midi_pitch_bend_callback_t pitch_bend;
    ble_midi_aftertouch_callback_t channel_aftertouch;
    ble_midi_poly_aftertouch_callback_t poly_aftertouch;
    ble_midi_sysex_callback_t sysex;
    ble_midi_realtime_callback_t realtime;
} ble_midi_callbacks_t;

//***********************************************************************************
// MIDI SYSEX Functions
//***********************************************************************************

/**
 * @brief 发送MIDI System Exclusive (SYSEX)消息
 * @param conn_handle 连接句柄
 * @param data SYSEX数据指针（不包括0xF0和0xF7）
 * @param length 数据长度
 * @return 0=成功, 其他=失败
 */
int midi_send_sysex(uint16_t conn_handle, const uint8_t *data, uint16_t length);

/**
 * @brief 发送完整的MIDI SYSEX消息（包括制造商ID等）
 * @param conn_handle 连接句柄
 * @param manufacturer_id 制造商ID
 * @param device_id 设备ID
 * @param data SYSEX数据指针
 * @param length 数据长度
 * @return 0=成功, 其他=失败
 */
int midi_send_sysex_complete(uint16_t conn_handle, uint8_t manufacturer_id, uint8_t device_id,
                            const uint8_t *data, uint16_t length);

/**
 * @brief 发送设备查询SYSEX消息
 * @param conn_handle 连接句柄
 * @return 0=成功, 其他=失败
 */
int midi_send_device_inquiry(uint16_t conn_handle);

/**
 * @brief 发送通用SYSEX响应消息
 * @param conn_handle 连接句柄
 * @param manufacturer_id 制造商ID
 * @param device_id 设备ID
 * @param product_id 产品ID
 * @param version 版本号
 * @return 0=成功, 其他=失败
 */
int midi_send_device_identity_reply(uint16_t conn_handle, uint8_t manufacturer_id, 
                                  uint8_t device_id, uint16_t product_id, uint32_t version);

//***********************************************************************************
// Extended MIDI Functions
//***********************************************************************************

/**
 * @brief 发送MIDI Channel Aftertouch消息
 * @param conn_handle 连接句柄
 * @param channel MIDI通道 (0-15)
 * @param pressure 压力值 (0-127)
 * @return 0=成功, 其他=失败
 */
int midi_send_channel_aftertouch(uint16_t conn_handle, uint8_t channel, uint8_t pressure);

/**
 * @brief 发送MIDI Polyphonic Aftertouch消息
 * @param conn_handle 连接句柄
 * @param channel MIDI通道 (0-15)
 * @param note 音符编号 (0-127)
 * @param pressure 压力值 (0-127)
 * @return 0=成功, 其他=失败
 */
int midi_send_poly_aftertouch(uint16_t conn_handle, uint8_t channel, uint8_t note, uint8_t pressure);

/**
 * @brief 发送MIDI实时消息
 * @param conn_handle 连接句柄
 * @param message 实时消息类型 (0xF8-0xFF)
 * @return 0=成功, 其他=失败
 */
int midi_send_realtime(uint16_t conn_handle, uint8_t message);

/**
 * @brief 发送MIDI时钟消息
 * @param conn_handle 连接句柄
 * @return 0=成功, 其他=失败
 */
int midi_send_clock(uint16_t conn_handle);

/**
 * @brief 发送MIDI开始消息
 * @param conn_handle 连接句柄
 * @return 0=成功, 其他=失败
 */
int midi_send_start(uint16_t conn_handle);

/**
 * @brief 发送MIDI停止消息
 * @param conn_handle 连接句柄
 * @return 0=成功, 其他=失败
 */
int midi_send_stop(uint16_t conn_handle);

/**
 * @brief 发送MIDI继续消息
 * @param conn_handle 连接句柄
 * @return 0=成功, 其他=失败
 */
int midi_send_continue(uint16_t conn_handle);

/**
 * @brief 发送MIDI歌曲位置消息
 * @param conn_handle 连接句柄
 * @param position 歌曲位置 (0-16383)
 * @return 0=成功, 其他=失败
 */
int midi_send_song_position(uint16_t conn_handle, uint16_t position);

/**
 * @brief 发送MIDI歌曲选择消息
 * @param conn_handle 连接句柄
 * @param song 歌曲编号 (0-127)
 * @return 0=成功, 其他=失败
 */
int midi_send_song_select(uint16_t conn_handle, uint8_t song);

/**
 * @brief 发送MIDI调音请求消息
 * @param conn_handle 连接句柄
 * @return 0=成功, 其他=失败
 */
int midi_send_tune_request(uint16_t conn_handle);

//***********************************************************************************
// MIDI Utility Functions
//***********************************************************************************

/**
 * @brief 注册BLE MIDI事件回调函数
 * @param callbacks BLE MIDI事件回调函数集合
 */
void ble_midi_register_callbacks(const ble_midi_callbacks_t *callbacks);

/**
 * @brief 注册SYSEX消息回调函数
 * @param callback SYSEX回调函数指针
 */
void ble_midi_register_sysex_callback(ble_midi_sysex_callback_t callback);

/**
 * @brief 获取MIDI消息长度
 * @param status MIDI状态字节
 * @return 消息总长度（包括状态字节）
 */
uint8_t midi_get_message_length(uint8_t status);

/**
 * @brief 检查是否为MIDI实时消息
 * @param byte 要检查的字节
 * @return true: 是实时消息, false: 不是
 */
bool midi_is_realtime_message(uint8_t byte);

/**
 * @brief 检查是否为MIDI状态字节
 * @param byte 要检查的字节
 * @return true: 是状态字节, false: 不是
 */
bool midi_is_status_byte(uint8_t byte);

/**
 * @brief 从MIDI消息中提取通道
 * @param status MIDI状态字节
 * @return MIDI通道 (0-15)，如果不是通道消息返回0xFF
 */
uint8_t midi_get_channel(uint8_t status);

/**
 * @brief 构建MIDI消息
 * @param message 输出的MIDI消息结构体
 * @param status 状态字节
 * @param data1 数据字节1
 * @param data2 数据字节2
 * @return 消息长度
 */
uint8_t midi_build_message(ble_midi_message_t *message, uint8_t status, uint8_t data1, uint8_t data2);

//***********************************************************************************
/**
 * @brief 启用/禁用 SYSEX 功能
 * @param enable true=启用, false=禁用
 */
void ble_midi_enable_sysex(bool enable);

/**
 * @brief 发送MIDI系统复位消息
 */
void ble_midi_send_system_reset(void);

//***********************************************************************************
// BLE MIDI Clock Synchronization Functions
//***********************************************************************************

/**
 * @brief 时钟同步回调函数类型
 * @param message_type 消息类型 (0xF8, 0xFA, 0xFB, 0xFC等)
 * @param timestamp 时间戳
 */
typedef void (*ble_midi_clock_callback_t)(uint8_t message_type, uint32_t timestamp);

/**
 * @brief 注册时钟同步回调函数
 * @param callback 时钟回调函数
 */
void ble_midi_register_clock_callback(ble_midi_clock_callback_t callback);

/**
 * @brief 启用/禁用时钟同步
 * @param enable true=启用, false=禁用
 */
void ble_midi_enable_clock_sync(bool enable);

/**
 * @brief 获取当前BPM（基于时钟间隔）
 * @return BPM值，0表示无效
 */
uint16_t ble_midi_get_current_bpm(void);

/**
 * @brief 获取时钟同步状态
 * @return true=同步中, false=未同步
 */
bool ble_midi_is_clock_synced(void);

/**
 * @brief 获取歌曲位置
 * @return 当前歌曲位置（beat为单位）
 */
uint32_t ble_midi_get_song_position(void);

/**
 * @brief 时钟同步演示初始化
 */
void ble_midi_clock_sync_demo_init(void);

/**
 * @brief 发送时钟同步测试序列
 * @param conn_handle 连接句柄
 */
void ble_midi_send_clock_test_sequence(uint16_t conn_handle);

/**
 * @brief 示例时钟同步回调函数
 * @param message_type 消息类型
 * @param timestamp 时间戳
 */
void example_clock_callback(uint8_t message_type, uint32_t timestamp);

//***********************************************************************************
// BLE MIDI Debug Control Functions
//***********************************************************************************

/**
 * @brief 设置BLE MIDI调试开关
 * @param enable true=启用, false=禁用
 */
void ble_midi_set_debug_enable(bool enable);

/**
 * @brief 设置BLE MIDI详细调试开关
 * @param enable true=启用, false=禁用
 */
void ble_midi_set_verbose_debug(bool enable);

/**
 * @brief 设置BLE MIDI时钟调试开关
 * @param enable true=启用, false=禁用
 */
void ble_midi_set_clock_debug(bool enable);

/**
 * @brief 设置BLE MIDI SYSEX调试开关
 * @param enable true=启用, false=禁用
 */
void ble_midi_set_sysex_debug(bool enable);

/**
 * @brief 获取当前调试状态
 */
void ble_midi_get_debug_status(void);

//***********************************************************************************
#endif // BLE_SUPPORT && BLE_MIDI_ENABLE

#endif // __BLE_MIDI_H__
