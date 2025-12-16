#ifndef __BLE_MIDI_CONFIG_H__
#define __BLE_MIDI_CONFIG_H__

//***********************************************************************************
// BLE MIDI Configuration
//***********************************************************************************

/**
 * BLE MIDI功能总开关
 * 1 = 启用BLE MIDI功能
 * 0 = 禁用BLE MIDI功能
 * 
 * 注意：此功能需要 BLE_SUPPORT=1 才能工作
 */
#ifndef BLE_MIDI_ENABLE
#define BLE_MIDI_ENABLE    1
#endif

/**
 * BLE MIDI演示测试开关
 * 1 = 启用演示测试功能（包括定时器发送音符）
 * 0 = 禁用演示测试功能
 * 
 * 注意：此功能仅在 BLE_MIDI_ENABLE=1 时有效
 */
#ifndef BLE_MIDI_DEMO_ENABLE
#define BLE_MIDI_DEMO_ENABLE    0
#endif

//***********************************************************************************
// BLE MIDI Demo Configuration (仅在BLE_MIDI_DEMO_ENABLE=1时有效)
//***********************************************************************************

/**
 * 演示音符发送间隔（毫秒）
 * 默认每1000ms（1秒）发送一个音符
 * 注意：这里假设系统tick频率为1000Hz
 */
#ifndef BLE_MIDI_DEMO_INTERVAL_MS
#define BLE_MIDI_DEMO_INTERVAL_MS    1000
#endif

/**
 * 演示发送的MIDI音符
 * 默认发送Note 60 (Middle C)
 */
#ifndef BLE_MIDI_DEMO_NOTE
#define BLE_MIDI_DEMO_NOTE    60
#endif

/**
 * 演示MIDI通道
 * 默认通道0（MIDI通道1）
 */
#ifndef BLE_MIDI_DEMO_CHANNEL
#define BLE_MIDI_DEMO_CHANNEL    0
#endif

/**
 * 演示MIDI力度
 * 默认力度100
 */
#ifndef BLE_MIDI_DEMO_VELOCITY
#define BLE_MIDI_DEMO_VELOCITY    100
#endif

//***********************************************************************************
// 蜜雪冰城Demo Configuration (第二个Demo)
//***********************************************************************************

/**
 * 蜜雪冰城Demo开关
 * 1 = 启用蜜雪冰城旋律演示
 * 0 = 禁用蜜雪冰城旋律演示
 * 
 * 注意：此功能仅在 BLE_MIDI_ENABLE=1 时有效
 */
#ifndef BLE_MIDI_MIXUE_DEMO_ENABLE
#define BLE_MIDI_MIXUE_DEMO_ENABLE    1
#endif

/**
 * 蜜雪冰城Demo音符播放间隔（毫秒）
 * 默认100ms，作为基础时值单位，实际播放时间=基础时值×持续倍数
 * 注意：这个值控制每个基本时值的长度，不同音符会有不同倍数
 */
#ifndef BLE_MIDI_MIXUE_NOTE_INTERVAL_MS
#define BLE_MIDI_MIXUE_NOTE_INTERVAL_MS    100
#endif

/**
 * 蜜雪冰城Demo MIDI通道
 * 默认通道1（与普通Demo区分）
 */
#ifndef BLE_MIDI_MIXUE_CHANNEL
#define BLE_MIDI_MIXUE_CHANNEL    1
#endif

/**
 * 蜜雪冰城Demo MIDI力度
 * 默认力度120（稍强一些）
 */
#ifndef BLE_MIDI_MIXUE_VELOCITY
#define BLE_MIDI_MIXUE_VELOCITY    120
#endif

//***********************************************************************************
// BLE MIDI Service Configuration
//***********************************************************************************

/**
 * BLE MIDI服务名称
 * 默认为"MV MIDI"，最大长度受BLE广播包限制
 */
#ifndef BLE_MIDI_SERVICE_NAME
#define BLE_MIDI_SERVICE_NAME    "MV MIDI"
#endif

/**
 * BLE MIDI服务名称最大长度
 * 默认20字节，受BLE广播数据包大小限制
 */
#ifndef BLE_MIDI_SERVICE_NAME_MAX_LEN
#define BLE_MIDI_SERVICE_NAME_MAX_LEN    20
#endif

/**
 * BLE MIDI设备名称
 * 默认为"MV-BT15 MIDI"，用于BLE设备识别
 */
#ifndef BLE_MIDI_DEVICE_NAME
#define BLE_MIDI_DEVICE_NAME    "MV-BT15 MIDI"
#endif

/**
 * BLE MIDI设备名称最大长度
 * 默认31字节，符合BLE标准限制
 */
#ifndef BLE_MIDI_DEVICE_NAME_MAX_LEN
#define BLE_MIDI_DEVICE_NAME_MAX_LEN    31
#endif

/**
 * BLE MIDI设备名称广播最大长度
 * 默认8字节，受BLE Legacy广播包大小限制（31字节总共）
 * 
 * BLE Legacy广播包结构分析：
 * - Flags: 3字节 [2, 0x01, 0x06]
 * - MIDI UUID: 18字节 [17, 0x07, + 16字节UUID]
 * - 设备名称: 2+N字节 [长度, 0x09, + N字节名称]
 * - 总计：3 + 18 + 2 + N = 23 + N ≤ 31
 * - 因此N ≤ 8字节
 * 
 * 注意：如果设备名称超过8字节，将被自动截断
 */
#ifndef BLE_MIDI_ADV_NAME_MAX_LEN
#define BLE_MIDI_ADV_NAME_MAX_LEN    8
#endif

//***********************************************************************************
// BLE MIDI Protocol Configuration
//***********************************************************************************

/**
 * BLE MIDI特征句柄
 * 
 * 根据GATT表配置：
 * - 0x14: BLE MIDI数据特征句柄（第四个服务，用于发送MIDI数据）
 * - 0x15: BLE MIDI配置句柄（第四个服务，用于启用通知）
 * - 0x0B: AB00服务句柄（第三个服务，非MIDI服务）
 * 
 * 注意：必须使用0x14作为MIDI数据发送句柄！
 */
#ifndef BLE_MIDI_CHARACTERISTIC_HANDLE
#define BLE_MIDI_CHARACTERISTIC_HANDLE    0x14
#endif

/**
 * BLE MIDI最大数据包长度
 * 默认20字节（包含BLE MIDI头部）
 */
#ifndef BLE_MIDI_MAX_PACKET_SIZE
#define BLE_MIDI_MAX_PACKET_SIZE    20
#endif

/**
 * MIDI数据最大长度
 * 默认18字节（20字节包 - 2字节BLE MIDI头部）
 */
#ifndef BLE_MIDI_MAX_MIDI_DATA_SIZE
#define BLE_MIDI_MAX_MIDI_DATA_SIZE    18
#endif

//***********************************************************************************
// MIDI SYSEX Configuration
//***********************************************************************************

/**
 * SYSEX消息最大长度
 * BLE MIDI标准建议不超过20字节的有效载荷
 * 减去BLE MIDI头部(2字节)和SYSEX开始/结束(2字节)，实际数据约16字节
 */
#ifndef BLE_MIDI_MAX_SYSEX_SIZE
#define BLE_MIDI_MAX_SYSEX_SIZE        64
#endif

/**
 * SYSEX接收超时时间（毫秒）
 * 如果SYSEX消息在此时间内未完成接收，将被丢弃
 */
#ifndef BLE_MIDI_SYSEX_TIMEOUT_MS
#define BLE_MIDI_SYSEX_TIMEOUT_MS      5000
#endif

//***********************************************************************************
// Debug Configuration
//***********************************************************************************

/**
 * BLE MIDI调试输出开关
 * 1 = 启用调试输出
 * 0 = 禁用调试输出
 */
#ifndef BLE_MIDI_DEBUG_ENABLE
#define BLE_MIDI_DEBUG_ENABLE    1
#endif

/**
 * BLE MIDI强制测试开关
 * 1 = 启用强制测试（每5秒发送一次测试音符，用于调试）
 * 0 = 禁用强制测试（推荐，避免不必要的调试输出）
 * 
 * 注意：强制测试独立于Demo功能，主要用于开发调试
 */
#ifndef BLE_MIDI_FORCE_TEST_ENABLE
#define BLE_MIDI_FORCE_TEST_ENABLE    0
#endif

#endif /* __BLE_MIDI_CONFIG_H__ */
