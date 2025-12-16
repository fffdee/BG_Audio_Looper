#include "ble_midi.h"

#include <string.h>
#include "ble_api.h"
#include "debug.h"
#include "rtos_api.h"
#include "task.h"  // 鍖呭惈 xTaskGetTickCount 鍑芥暟
#include "bt_config.h"  // 鍖呭惈BLE_SUPPORT瀹氫箟
#if (BLE_SUPPORT && BLE_MIDI_ENABLE)

//***********************************************************************************
// BLE MIDI Debug Control Macros
//***********************************************************************************

// BLE MIDI 璋冭瘯鎺у埗 - 鍙互鍦ㄧ紪璇戞椂鎴栬繍琛屾椂鎺у埗
#ifndef BLE_MIDI_DEBUG_ENABLE
#define BLE_MIDI_DEBUG_ENABLE    1  // 1=鍚敤璋冭瘯, 0=绂佺敤璋冭瘯
#endif

#ifndef BLE_MIDI_VERBOSE_DEBUG
#define BLE_MIDI_VERBOSE_DEBUG   1  // 1=璇︾粏璋冭瘯, 0=绠�崟璋冭瘯
#endif

#ifndef BLE_MIDI_CLOCK_DEBUG
#define BLE_MIDI_CLOCK_DEBUG     1  // 1=鏃堕挓璋冭瘯, 0=绂佺敤鏃堕挓璋冭瘯
#endif

#ifndef BLE_MIDI_SYSEX_DEBUG
#define BLE_MIDI_SYSEX_DEBUG     1  // 1=SYSEX璋冭瘯, 0=绂佺敤SYSEX璋冭瘯
#endif

// 杩愯鏃惰皟璇曟帶鍒跺彉閲�
static uint8_t g_ble_midi_debug_enabled = 1;
static uint8_t g_ble_midi_verbose_debug = 1;
static uint8_t g_ble_midi_clock_debug = 1;
static uint8_t g_ble_midi_sysex_debug = 1;

// 璋冭瘯瀹忓畾涔�
#if BLE_MIDI_DEBUG_ENABLE
    #define BLE_MIDI_DBG(fmt, ...) \
        do { \
            if (g_ble_midi_debug_enabled) { \
                DBG("[BLE_MIDI] " fmt, ##__VA_ARGS__); \
            } \
        } while(0)
        
    #if BLE_MIDI_VERBOSE_DEBUG
        #define BLE_MIDI_VERBOSE_DBG(fmt, ...) \
            do { \
                if (g_ble_midi_debug_enabled && g_ble_midi_verbose_debug) { \
                    DBG("[BLE_MIDI_V] " fmt, ##__VA_ARGS__); \
                } \
            } while(0)
    #else
        #define BLE_MIDI_VERBOSE_DBG(fmt, ...)
    #endif
    
    #if BLE_MIDI_CLOCK_DEBUG
        #define BLE_MIDI_CLOCK_DBG(fmt, ...) \
            do { \
                if (g_ble_midi_debug_enabled && g_ble_midi_clock_debug) { \
                    DBG("[BLE_MIDI_CLK] " fmt, ##__VA_ARGS__); \
                } \
            } while(0)
    #else
        #define BLE_MIDI_CLOCK_DBG(fmt, ...)
    #endif
    
    #if BLE_MIDI_SYSEX_DEBUG
        #define BLE_MIDI_SYSEX_DBG(fmt, ...) \
            do { \
                if (g_ble_midi_debug_enabled && g_ble_midi_sysex_debug) { \
                    DBG("[BLE_MIDI_SYX] " fmt, ##__VA_ARGS__); \
                } \
            } while(0)
    #else
        #define BLE_MIDI_SYSEX_DBG(fmt, ...)
    #endif
#else
    #define BLE_MIDI_DBG(fmt, ...)
    #define BLE_MIDI_VERBOSE_DBG(fmt, ...)
    #define BLE_MIDI_CLOCK_DBG(fmt, ...)
    #define BLE_MIDI_SYSEX_DBG(fmt, ...)
#endif

/**
 * @brief 璁剧疆BLE MIDI璋冭瘯寮�叧
 * @param enable 1=鍚敤, 0=绂佺敤
 */
void ble_midi_set_debug_enable(uint8_t enable)
{
    g_ble_midi_debug_enabled = enable;
    BLE_MIDI_DBG("Debug %s\n", enable ? "enabled" : "disabled");
}

/**
 * @brief 璁剧疆BLE MIDI璇︾粏璋冭瘯寮�叧
 * @param enable 1=鍚敤, 0=绂佺敤
 */
void ble_midi_set_verbose_debug(uint8_t enable)
{
    g_ble_midi_verbose_debug = enable;
    BLE_MIDI_DBG("Verbose debug %s\n", enable ? "enabled" : "disabled");
}

/**
 * @brief 璁剧疆BLE MIDI鏃堕挓璋冭瘯寮�叧
 * @param enable 1=鍚敤, 0=绂佺敤
 */
void ble_midi_set_clock_debug(uint8_t enable)
{
    g_ble_midi_clock_debug = enable;
    BLE_MIDI_DBG("Clock debug %s\n", enable ? "enabled" : "disabled");
}

/**
 * @brief 璁剧疆BLE MIDI SYSEX璋冭瘯寮�叧
 * @param enable 1=鍚敤, 0=绂佺敤
 */
void ble_midi_set_sysex_debug(uint8_t enable)
{
    g_ble_midi_sysex_debug = enable;
    BLE_MIDI_DBG("SYSEX debug %s\n", enable ? "enabled" : "disabled");
}

/**
 * @brief 鑾峰彇褰撳墠璋冭瘯鐘舵�
 */
void ble_midi_get_debug_status(void)
{
    BLE_MIDI_DBG("=== BLE MIDI Debug Status ===\n");
    BLE_MIDI_DBG("General Debug: %s\n", g_ble_midi_debug_enabled ? "ON" : "OFF");
    BLE_MIDI_DBG("Verbose Debug: %s\n", g_ble_midi_verbose_debug ? "ON" : "OFF");
    BLE_MIDI_DBG("Clock Debug: %s\n", g_ble_midi_clock_debug ? "ON" : "OFF");
    BLE_MIDI_DBG("SYSEX Debug: %s\n", g_ble_midi_sysex_debug ? "ON" : "OFF");
    BLE_MIDI_DBG("============================\n");
}

// 澶栭儴鍙橀噺澹版槑
extern uint8_t BleConnectFlag;

// 鍏ㄥ眬杩炴帴鍙ユ焺鍙橀噺锛堢敤浜庡悇Demo鍜屾祴璇曞嚱鏁板叡浜級
static uint16_t g_ble_midi_conn_handle = 0;

//***********************************************************************************
// BLE MIDI Internal Functions and Variables
//***********************************************************************************

// SYSEX 鍥炶皟鍑芥暟绫诲瀷瀹氫箟
typedef void (*ble_midi_sysex_callback_t)(uint16_t conn_handle, const ble_midi_sysex_t *sysex);

// SYSEX 鐩稿叧鍙橀噺
static ble_midi_sysex_callback_t g_sysex_callback = NULL;
static uint8_t g_sysex_buffer[BLE_MIDI_MAX_SYSEX_SIZE];
static uint16_t g_sysex_length = 0;
static uint8_t g_sysex_receiving = 0;

// MIDI鏃堕挓鍚屾鐘舵�
typedef struct {
    uint8_t clock_sync_enabled;     // 鏄惁鍚敤鏃堕挓鍚屾
    uint32_t last_clock_time;    // 涓婃鏀跺埌鏃堕挓鐨勬椂闂�
    uint32_t clock_interval_ms;  // 鏃堕挓闂撮殧锛堟绉掞級
    uint16_t clock_counter;      // 鏃堕挓璁℃暟鍣�
    uint8_t transport_running;      // 浼犺緭鏄惁杩愯涓�
    uint32_t song_position;      // 姝屾洸浣嶇疆
} ble_midi_clock_sync_t;

static ble_midi_clock_sync_t g_clock_sync = {0};

// 鏃堕挓鍚屾鍥炶皟鍑芥暟绫诲瀷
typedef void (*ble_midi_clock_callback_t)(uint8_t message_type, uint32_t timestamp);
static ble_midi_clock_callback_t g_clock_callback = NULL;

// 鍑芥暟鍓嶅悜澹版槑
static uint8_t process_sysex_byte(uint8_t data_byte);
static void handle_sysex_complete(uint16_t conn_handle);
static void handle_midi_clock_message(uint8_t message_type, uint32_t timestamp);
int ble_midi_send_device_inquiry(uint16_t conn_handle);
int ble_midi_send_sysex_complete(uint16_t conn_handle, uint8_t manufacturer_id, uint8_t device_id, uint8_t *data, uint8_t data_len);
void example_sysex_callback(uint16_t conn_handle, const ble_midi_sysex_t *sysex);

// 鑾峰彇BLE MIDI鏃堕棿鎴筹紙13浣嶏紝浠ユ绉掍负鍗曚綅锛�
static uint16_t get_ble_midi_timestamp(void)
{
    // BLE MIDI鏃堕棿鎴虫槸浠庤繛鎺ュ缓绔嬫椂寮�鐨勭浉瀵规椂闂存埑锛堟绉掞級
    // 鏍囧噯瑕佹眰13浣嶆椂闂存埑锛岃寖鍥�-8191ms锛屽惊鐜娇鐢�
    uint32_t tick = xTaskGetTickCount();
    
    // 灏唗ick杞崲涓烘绉�
    // configTICK_RATE_HZ瀹氫箟浜嗘瘡绉掔殑tick鏁�
    uint32_t timestamp_ms = (tick * 1000) / configTICK_RATE_HZ;
    
    // BLE MIDI鏃堕棿鎴虫槸13浣嶏紝鍊艰寖鍥�-8191锛�.192绉掑惊鐜級
    uint16_t ble_timestamp = (uint16_t)(timestamp_ms & 0x1FFF);
    
    return ble_timestamp;
}

// 鏋勫缓BLE MIDI鏁版嵁鍖呭ご閮�
static uint8_t build_ble_midi_header(uint8_t *header, uint16_t timestamp)
{
    // BLE MIDI鏍煎紡: [Header][Timestamp Low][MIDI Data...]
    // Header: bit7=1, bit6-0=timestamp high 6 bits (6浣嶏紝涓嶆槸7浣�)
    // Timestamp Low: bit7=1, bit6-0=timestamp low 7 bits
    
    header[0] = 0x80 | ((timestamp >> 7) & 0x3F); // Header瀛楄妭锛氶珮6浣嶆椂闂存埑
    header[1] = 0x80 | (timestamp & 0x7F);        // Timestamp Low瀛楄妭锛氫綆7浣嶆椂闂存埑
    return 2; // 杩斿洖澶撮儴闀垮害
}

//***********************************************************************************
// BLE MIDI Data Processing Functions
//***********************************************************************************

/**
 * @brief 鍙戦�BLE MIDI鏁版嵁鍖�
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param midi_data MIDI鏁版嵁
 * @param midi_len MIDI鏁版嵁闀垮害
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_data(uint16_t conn_handle, uint8_t *midi_data, uint8_t midi_len)
{
    // 妫�煡杩炴帴鐘舵�
    if (!BleConnectFlag)
    {
        BLE_MIDI_DBG("Send: Connection not available - BleConnectFlag=%d, conn_handle=0x%04X\n", 
            BleConnectFlag, conn_handle);
        return -2; // 杩炴帴涓嶅彲鐢�
    }
    
    if (!midi_data || midi_len == 0 || midi_len > BLE_MIDI_MAX_MIDI_DATA_SIZE)
    {
        BLE_MIDI_DBG("Send: Invalid parameters\n");
        return -1;
    }

    uint8_t ble_midi_packet[BLE_MIDI_MAX_PACKET_SIZE];
    uint16_t timestamp = get_ble_midi_timestamp();
    
    // 鏋勫缓BLE MIDI澶撮儴
    uint8_t header_len = build_ble_midi_header(ble_midi_packet, timestamp);
    
    // 澶嶅埗MIDI鏁版嵁
    memcpy(&ble_midi_packet[header_len], midi_data, midi_len);
    
    uint8_t total_len = header_len + midi_len;
    
    // 璇︾粏璋冭瘯淇℃伅 - 鏄剧ず瀹屾暣鐨凚LE MIDI鍖呭唴瀹�
    BLE_MIDI_VERBOSE_DBG("Send: timestamp=0x%04X, MIDI len=%d, total len=%d\n",
        timestamp, midi_len, total_len);
    BLE_MIDI_VERBOSE_DBG("Packet: ");
    int i;
    for (i = 0; i < total_len; i++) {
        BLE_MIDI_VERBOSE_DBG("0x%02X ", ble_midi_packet[i]);
    }
    BLE_MIDI_VERBOSE_DBG("\n");
    
    // 鍙戦�鍒癕IDI鐗瑰緛锛屼娇鐢ㄤ笌ble_app_callback.c鐩稿悓鐨勮皟鐢ㄦ柟寮�
    uint32_t result = att_server_notify(BLE_MIDI_CHARACTERISTIC_HANDLE, ble_midi_packet, total_len);
    
    if (result == 0) {
        BLE_MIDI_DBG("Send: Success\n");
    } else {
        BLE_MIDI_DBG("Send: Failed with result=0x%08X\n", (unsigned int)result);
    }
    
    return (result == 0) ? 0 : -1; // 杞崲涓烘湡鏈涚殑杩斿洖鍊兼牸寮�
}

/**
 * @brief 瑙ｆ瀽BLE MIDI鏁版嵁鍖�
 * @param ble_midi_data BLE MIDI鏁版嵁鍖�
 * @param ble_midi_len BLE MIDI鏁版嵁鍖呴暱搴�
 * @param midi_data 杈撳嚭鐨凪IDI鏁版嵁
 * @param midi_len 杈撳嚭鐨凪IDI鏁版嵁闀垮害
 * @return 瑙ｆ瀽鐨凪IDI娑堟伅鏁伴噺
 */
int ble_midi_parse_data(uint8_t *ble_midi_data, uint8_t ble_midi_len, 
                       uint8_t *midi_data, uint8_t *midi_len)
{
    if (!ble_midi_data || ble_midi_len < 2 || !midi_data || !midi_len)
    {
        return 0;
    }

    uint8_t pos = 0;
    uint8_t midi_pos = 0;
    uint16_t timestamp = 0;
    int message_count = 0;

    // 瑙ｆ瀽澶撮儴鍜屾椂闂存埑
    if (ble_midi_data[0] & 0x80) // Header瀛楄妭
    {
        timestamp = ((ble_midi_data[0] & 0x3F) << 7);
        pos = 1;
        
        if (pos < ble_midi_len && (ble_midi_data[pos] & 0x80)) // Timestamp Low瀛楄妭
        {
            timestamp |= (ble_midi_data[pos] & 0x7F);
            pos++;
        }
    }

    BLE_MIDI_VERBOSE_DBG("Parse: timestamp=0x%04X, data len=%d\n", timestamp, ble_midi_len - pos);

    // 瑙ｆ瀽MIDI鏁版嵁
    while (pos < ble_midi_len)
    {
        if (ble_midi_data[pos] & 0x80) // 鏂扮殑鏃堕棿鎴虫垨鐘舵�瀛楄妭
        {
            if ((ble_midi_data[pos] & 0xF0) == 0x80) // 鍙兘鏄柊鐨勬椂闂存埑
            {
                // 璺宠繃鏃堕棿鎴�
                pos++;
                continue;
            }
        }
        
        // 澶勭悊 SYSEX 鏁版嵁
        if (ble_midi_data[pos] == 0xF0 || ble_midi_data[pos] == 0xF7 || g_sysex_receiving)
        {
            if (process_sysex_byte(ble_midi_data[pos]))
            {
                // SYSEX 娑堟伅瀹屾垚锛屽鐞嗗畠
                // 娉ㄦ剰锛氳繖閲岄渶瑕佽繛鎺ュ彞鏌勶紝浣嗗綋鍓嶅嚱鏁版病鏈夋彁渚�
                // 鍦ㄥ疄闄呭簲鐢ㄤ腑锛屽簲璇ヤ粠鏇撮珮灞傚嚱鏁颁紶閫掕繛鎺ュ彞鏌�
                BLE_MIDI_SYSEX_DBG("Parse: SYSEX message completed in parser\n");
                // handle_sysex_complete(conn_handle); // 闇�杩炴帴鍙ユ焺
            }
            pos++;
            continue;
        }
        
        // 澶嶅埗MIDI鏁版嵁
        midi_data[midi_pos++] = ble_midi_data[pos++];
        message_count++;
    }

    *midi_len = midi_pos;
    return message_count;
}

//***********************************************************************************
// Standard MIDI Message Functions
//***********************************************************************************

/**
 * @brief 鍙戦�BLE MIDI Note On娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param channel MIDI閫氶亾 (0-15)
 * @param note 闊崇 (0-127)
 * @param velocity 鍔涘害 (0-127)
 */
int ble_midi_send_note_on(uint16_t conn_handle, uint8_t channel, uint8_t note, uint8_t velocity)
{
    uint8_t midi_msg[3];
    midi_msg[0] = 0x90 | (channel & 0x0F); // Note On + Channel
    midi_msg[1] = note & 0x7F;
    midi_msg[2] = velocity & 0x7F;
    
    BLE_MIDI_DBG("Note On: CH=%d, Note=%d, Vel=%d\n", channel, note, velocity);
    return ble_midi_send_data(conn_handle, midi_msg, 3);
}

/**
 * @brief 鍙戦�BLE MIDI Note Off娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param channel MIDI閫氶亾 (0-15)
 * @param note 闊崇 (0-127)
 * @param velocity 鍔涘害 (0-127)
 */
int ble_midi_send_note_off(uint16_t conn_handle, uint8_t channel, uint8_t note, uint8_t velocity)
{
    uint8_t midi_msg[3];
    midi_msg[0] = 0x80 | (channel & 0x0F); // Note Off + Channel
    midi_msg[1] = note & 0x7F;
    midi_msg[2] = velocity & 0x7F;
    
    BLE_MIDI_DBG("Note Off: CH=%d, Note=%d, Vel=%d\n", channel, note, velocity);
    return ble_midi_send_data(conn_handle, midi_msg, 3);
}

/**
 * @brief 鍙戦�BLE MIDI Control Change娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param channel MIDI閫氶亾 (0-15)
 * @param controller 鎺у埗鍣ㄧ紪鍙�(0-127)
 * @param value 鎺у埗鍣ㄥ� (0-127)
 */
int ble_midi_send_control_change(uint16_t conn_handle, uint8_t channel, uint8_t controller, uint8_t value)
{
    uint8_t midi_msg[3];
    midi_msg[0] = 0xB0 | (channel & 0x0F); // Control Change + Channel
    midi_msg[1] = controller & 0x7F;
    midi_msg[2] = value & 0x7F;
    
    BLE_MIDI_DBG("CC: CH=%d, CC=%d, Val=%d\n", channel, controller, value);
    return ble_midi_send_data(conn_handle, midi_msg, 3);
}

/**
 * @brief 鍙戦�BLE MIDI Program Change娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param channel MIDI閫氶亾 (0-15)
 * @param program 绋嬪簭缂栧彿 (0-127)
 */
int ble_midi_send_program_change(uint16_t conn_handle, uint8_t channel, uint8_t program)
{
    uint8_t midi_msg[2];
    midi_msg[0] = 0xC0 | (channel & 0x0F); // Program Change + Channel
    midi_msg[1] = program & 0x7F;
    
    BLE_MIDI_DBG("PC: CH=%d, Program=%d\n", channel, program);
    return ble_midi_send_data(conn_handle, midi_msg, 2);
}

/**
 * @brief 鍙戦�BLE MIDI Pitch Bend娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param channel MIDI閫氶亾 (0-15)
 * @param pitch_bend 寮煶鍊�(0-16383, 8192涓轰腑蹇�
 */
int ble_midi_send_pitch_bend(uint16_t conn_handle, uint8_t channel, uint16_t pitch_bend)
{
    uint8_t midi_msg[3];
    midi_msg[0] = 0xE0 | (channel & 0x0F); // Pitch Bend + Channel
    midi_msg[1] = pitch_bend & 0x7F;       // LSB
    midi_msg[2] = (pitch_bend >> 7) & 0x7F; // MSB
    
    BLE_MIDI_DBG("Pitch Bend: CH=%d, Bend=%d\n", channel, pitch_bend);
    return ble_midi_send_data(conn_handle, midi_msg, 3);
}

/**
 * @brief 鍙戦�MIDI Channel Aftertouch娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param channel MIDI閫氶亾 (0-15)
 * @param pressure 鍘嬪姏鍊�(0-127)
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_channel_aftertouch(uint16_t conn_handle, uint8_t channel, uint8_t pressure)
{
    uint8_t midi_msg[2];
    midi_msg[0] = 0xD0 | (channel & 0x0F); // Channel Aftertouch + Channel
    midi_msg[1] = pressure & 0x7F;
    
    BLE_MIDI_DBG("MIDI Channel Aftertouch: CH=%d, Pressure=%d\n", channel, pressure);
    return ble_midi_send_data(conn_handle, midi_msg, 2);
}

/**
 * @brief 鍙戦�MIDI Polyphonic Aftertouch娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param channel MIDI閫氶亾 (0-15)
 * @param note 闊崇缂栧彿 (0-127)
 * @param pressure 鍘嬪姏鍊�(0-127)
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_poly_aftertouch(uint16_t conn_handle, uint8_t channel, uint8_t note, uint8_t pressure)
{
    uint8_t midi_msg[3];
    midi_msg[0] = 0xA0 | (channel & 0x0F); // Poly Aftertouch + Channel
    midi_msg[1] = note & 0x7F;
    midi_msg[2] = pressure & 0x7F;
    
    BLE_MIDI_DBG("MIDI Poly Aftertouch: CH=%d, Note=%d, Pressure=%d\n", channel, note, pressure);
    return ble_midi_send_data(conn_handle, midi_msg, 3);
}

/**
 * @brief 鍙戦�MIDI瀹炴椂娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param message 瀹炴椂娑堟伅绫诲瀷 (0xF8-0xFF)
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_realtime(uint16_t conn_handle, uint8_t message)
{
    uint8_t midi_msg[1];
    midi_msg[0] = message;
    
    BLE_MIDI_CLOCK_DBG("Realtime: Message=0x%02X\n", message);
    return ble_midi_send_data(conn_handle, midi_msg, 1);
}

/**
 * @brief 鍙戦�MIDI鏃堕挓娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_clock(uint16_t conn_handle)
{
    return ble_midi_send_realtime(conn_handle, 0xF8);
}

/**
 * @brief 鍙戦�BLE MIDI寮�娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_start(uint16_t conn_handle)
{
    return ble_midi_send_realtime(conn_handle, 0xFA);
}

/**
 * @brief 鍙戦�BLE MIDI鍋滄娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_stop(uint16_t conn_handle)
{
    return ble_midi_send_realtime(conn_handle, 0xFC);
}

/**
 * @brief 鍙戦�BLE MIDI缁х画娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_continue(uint16_t conn_handle)
{
    return ble_midi_send_realtime(conn_handle, 0xFB);
}

/**
 * @brief 鍙戦�MIDI All Notes Off娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param channel MIDI閫氶亾 (0-15)
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_all_notes_off(uint16_t conn_handle, uint8_t channel)
{
    return ble_midi_send_control_change(conn_handle, channel, 123, 0);
}

/**
 * @brief 鍙戦�BLE MIDI All Sound Off娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param channel MIDI閫氶亾 (0-15)
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_all_sound_off(uint16_t conn_handle, uint8_t channel)
{
    return ble_midi_send_control_change(conn_handle, channel, 120, 0);
}

/**
 * @brief 鍙戦�BLE MIDI姝屾洸浣嶇疆娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param position 姝屾洸浣嶇疆 (0-16383)
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_song_position(uint16_t conn_handle, uint16_t position)
{
    uint8_t midi_msg[3];
    midi_msg[0] = 0xF2; // Song Position Pointer
    midi_msg[1] = position & 0x7F;       // LSB
    midi_msg[2] = (position >> 7) & 0x7F; // MSB
    
    BLE_MIDI_CLOCK_DBG("MIDI Song Position: Position=%d\n", position);
    return ble_midi_send_data(conn_handle, midi_msg, 3);
}

/**
 * @brief 鍙戦�MIDI姝屾洸閫夋嫨娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param song 姝屾洸缂栧彿 (0-127)
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_song_select(uint16_t conn_handle, uint8_t song)
{
    uint8_t midi_msg[2];
    midi_msg[0] = 0xF3; // Song Select
    midi_msg[1] = song & 0x7F;
    
    BLE_MIDI_DBG("MIDI Song Select: Song=%d\n", song);
    return ble_midi_send_data(conn_handle, midi_msg, 2);
}

/**
 * @brief 鍙戦�MIDI璋冮煶璇锋眰娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_tune_request(uint16_t conn_handle)
{
    uint8_t midi_msg[1];
    midi_msg[0] = 0xF6; // Tune Request
    
    BLE_MIDI_DBG("MIDI Tune Request\n");
    return ble_midi_send_data(conn_handle, midi_msg, 1);
}

/**
 * @brief 鍙戦�MIDI绯荤粺澶嶄綅娑堟伅
 */
void ble_midi_send_system_reset(void)
{
    if (!BleConnectFlag) {
        BLE_MIDI_DBG("System Reset: Not connected\n");
        return;
    }
    
    BLE_MIDI_DBG("馃攧 Sending MIDI System Reset\n");
    ble_midi_send_realtime(g_ble_midi_conn_handle, 0xFF);
}

//***********************************************************************************
// MIDI Utility Functions
//***********************************************************************************

/**
 * @brief 鑾峰彇MIDI娑堟伅闀垮害
 * @param status MIDI鐘舵�瀛楄妭
 * @return 娑堟伅鎬婚暱搴︼紙鍖呮嫭鐘舵�瀛楄妭锛�
 */
uint8_t ble_midi_get_message_length(uint8_t status)
{
    uint8_t message_type = status & 0xF0;
    
    switch (message_type)
    {
        case 0x80: // Note Off
        case 0x90: // Note On
        case 0xA0: // Polyphonic Aftertouch
        case 0xB0: // Control Change
        case 0xE0: // Pitch Bend
            return 3;
            
        case 0xC0: // Program Change
        case 0xD0: // Channel Aftertouch
            return 2;
            
        default:
            if (status >= 0xF0)
            {
                switch (status)
                {
                    case 0xF0: // System Exclusive
                        return 0; // Variable length
                    case 0xF1: // Time Code Quarter Frame
                    case 0xF3: // Song Select
                        return 2;
                    case 0xF2: // Song Position Pointer
                        return 3;
                    case 0xF6: // Tune Request
                    case 0xF7: // End of Exclusive
                    case 0xF8: // Timing Clock
                    case 0xFA: // Start
                    case 0xFB: // Continue
                    case 0xFC: // Stop
                    case 0xFE: // Active Sensing
                    case 0xFF: // System Reset
                        return 1;
                    default:
                        return 1;
                }
            }
            return 1;
    }
}

/**
 * @brief 妫�煡鏄惁涓篗IDI瀹炴椂娑堟伅
 * @param byte 瑕佹鏌ョ殑瀛楄妭
 * @return 1: 鏄疄鏃舵秷鎭� 0: 涓嶆槸
 */
uint8_t ble_midi_is_realtime_message(uint8_t byte)
{
    return (byte >= 0xF8 && byte <= 0xFF);
}

/**
 * @brief 妫�煡鏄惁涓篗IDI鐘舵�瀛楄妭
 * @param byte 瑕佹鏌ョ殑瀛楄妭
 * @return 1: 鏄姸鎬佸瓧鑺� 0: 涓嶆槸
 */
uint8_t ble_midi_is_status_byte(uint8_t byte)
{
    return (byte & 0x80) != 0;
}

/**
 * @brief 浠嶮IDI娑堟伅涓彁鍙栭�閬�
 * @param status MIDI鐘舵�瀛楄妭
 * @return MIDI閫氶亾 (0-15)锛屽鏋滀笉鏄�閬撴秷鎭繑鍥�xFF
 */
uint8_t ble_midi_get_channel(uint8_t status)
{
    if ((status >= 0x80 && status <= 0xEF))
    {
        return status & 0x0F;
    }
    return 0xFF; // 涓嶆槸閫氶亾娑堟伅
}

/**
 * @brief 鏋勫缓MIDI娑堟伅
 * @param message 杈撳嚭鐨凪IDI娑堟伅缁撴瀯浣�
 * @param status 鐘舵�瀛楄妭
 * @param data1 鏁版嵁瀛楄妭1
 * @param data2 鏁版嵁瀛楄妭2
 * @return 娑堟伅闀垮害
 */
uint8_t ble_midi_build_message(ble_midi_message_t *message, uint8_t status, uint8_t data1, uint8_t data2)
{
    if (!message)
        return 0;
    
    message->status = status;
    message->data1 = data1;
    message->data2 = data2;
    message->channel = ble_midi_get_channel(status);
    message->length = ble_midi_get_message_length(status);
    
    return message->length;
}

//***********************************************************************************
// Enhanced SYSEX Support Functions  
//***********************************************************************************

/**
 * @brief SYSEX 娴嬭瘯鍜屾紨绀哄嚱鏁�
 * @param conn_handle 杩炴帴鍙ユ焺
 */
void ble_midi_sysex_demo(uint16_t conn_handle)
{
    BLE_MIDI_SYSEX_DBG("馃帥锔�BLE MIDI SYSEX Demo: Starting comprehensive test...\n");
    
    // 1. 娉ㄥ唽绀轰緥鍥炶皟
    ble_midi_register_sysex_callback(example_sysex_callback);
    
    // 2. 鍙戦�璁惧鏌ヨ
    BLE_MIDI_SYSEX_DBG("馃搵 Sending device identity inquiry...\n");
    ble_midi_send_device_inquiry(conn_handle);
    
    // 3. 鍙戦�鑷畾涔夊埗閫犲晢娑堟伅
    BLE_MIDI_SYSEX_DBG("馃彮 Sending custom manufacturer message...\n");
    uint8_t custom_data[] = {0x7D, 0x00, 0x01, 0x10}; // 鑾峰彇鐗堟湰淇℃伅
    ble_midi_send_sysex_complete(conn_handle, 0x7D, 0x00, custom_data, sizeof(custom_data));
    
    BLE_MIDI_SYSEX_DBG("鉁�SYSEX Demo completed\n");
}

/**
 * @brief 鍚敤SYSEX鍔熻兘鐨勪究鎹锋帴鍙�
 */
void ble_midi_initialize_sysex_with_defaults(void)
{
    ble_midi_enable_sysex(1);
    ble_midi_register_sysex_callback(example_sysex_callback);
    BLE_MIDI_SYSEX_DBG("馃帥锔�BLE MIDI SYSEX functionality enabled with default callback\n");
}

//***********************************************************************************
// BLE MIDI SYSEX Support Functions
//***********************************************************************************

/**
 * @brief 娉ㄥ唽 SYSEX 鍥炶皟鍑芥暟
 * @param callback SYSEX 鍥炶皟鍑芥暟鎸囬拡
 */
void ble_midi_register_sysex_callback(ble_midi_sysex_callback_t callback)
{
    g_sysex_callback = callback;
    BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX: Callback registered\n");
}

/**
 * @brief 鍚敤 SYSEX 鏀寔
 * @param enable 1=鍚敤, 0=绂佺敤
 */
void ble_midi_enable_sysex(uint8_t enable)
{
    if (enable)
    {
        BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX: Enabled\n");
    }
    else
    {
        g_sysex_receiving = 0;
        g_sysex_length = 0;
        BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX: Disabled\n");
    }
}

/**
 * @brief 鍙戦�瀹屾暣鐨�SYSEX 娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param manufacturer_id 鍒堕�鍟咺D
 * @param device_id 璁惧ID
 * @param data SYSEX 鏁版嵁锛堜笉鍖呭惈 F0/F7锛�
 * @param data_len 鏁版嵁闀垮害
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_sysex_complete(uint16_t conn_handle, uint8_t manufacturer_id, uint8_t device_id, uint8_t *data, uint8_t data_len)
{
    if (!data || data_len == 0 || data_len > (BLE_MIDI_MAX_SYSEX_SIZE - 2))
    {
        BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX Send: Invalid parameters\n");
        return -1;
    }

    uint8_t sysex_packet[BLE_MIDI_MAX_SYSEX_SIZE];
    uint8_t pos = 0;
    
    // 鏋勫缓瀹屾暣鐨�SYSEX 娑堟伅
    sysex_packet[pos++] = 0xF0;  // SYSEX Start
    
    // 澶嶅埗鏁版嵁
    memcpy(&sysex_packet[pos], data, data_len);
    pos += data_len;
    
    sysex_packet[pos++] = 0xF7;  // SYSEX End
    
    BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX Send: manufacturer=0x%02X, device=0x%02X, data_len=%d, total_len=%d\n",
        manufacturer_id, device_id, data_len, pos);
    
    return ble_midi_send_data(conn_handle, sysex_packet, pos);
}

/**
 * @brief 鍙戦�璁惧韬唤鏌ヨ璇锋眰
 * @param conn_handle 杩炴帴鍙ユ焺
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_device_inquiry(uint16_t conn_handle)
{
    uint8_t inquiry_data[] = {
        0x7E,       // Universal System Exclusive
        0x7F,       // Device ID: All devices
        0x06,       // General Information
        0x01        // Identity Request
    };
    
    BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX: Sending device identity inquiry\n");
    return ble_midi_send_sysex_complete(conn_handle, 0x7E, 0x7F, inquiry_data, sizeof(inquiry_data));
}

/**
 * @brief 鍙戦�璁惧韬唤鍥炲
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param manufacturer_id 鍒堕�鍟咺D
 * @param device_id 璁惧ID
 * @param product_id 浜у搧ID
 * @param version 鐗堟湰鍙�(32浣�
 * @return 0=鎴愬姛, 鍏朵粬=澶辫触
 */
int ble_midi_send_device_identity_reply(uint16_t conn_handle, uint8_t manufacturer_id, uint8_t device_id, uint16_t product_id, uint32_t version)
{
    uint8_t reply_data[] = {
        0x7E,                           // Universal System Exclusive
        device_id,                      // Device ID
        0x06,                           // General Information
        0x02,                           // Identity Reply
        manufacturer_id,                // Manufacturer ID
        (product_id & 0x7F),           // Product ID LSB
        ((product_id >> 7) & 0x7F),    // Product ID MSB
        (version & 0x7F),              // Version LSB
        ((version >> 7) & 0x7F),       // Version
        ((version >> 14) & 0x7F),      // Version
        ((version >> 21) & 0x7F)       // Version MSB
    };
    
    BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX: Sending device identity reply - mfg=0x%02X, dev=0x%02X, prod=0x%04X, ver=0x%08lX\n",
        manufacturer_id, device_id, product_id, (unsigned long)version);
    
    return ble_midi_send_sysex_complete(conn_handle, 0x7E, device_id, reply_data, sizeof(reply_data));
}

/**
 * @brief 澶勭悊 SYSEX 鏁版嵁瀛楄妭
 * @param data_byte 鏁版嵁瀛楄妭
 * @return 1=SYSEX娑堟伅瀹屾垚, 0=缁х画鎺ユ敹
 */
static uint8_t process_sysex_byte(uint8_t data_byte)
{
    if (data_byte == 0xF0) // SYSEX Start
    {
        g_sysex_receiving = 1;
        g_sysex_length = 0;
        BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX: Start receiving\n");
        return 0;
    }
    else if (data_byte == 0xF7) // SYSEX End
    {
        if (g_sysex_receiving)
        {
            BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX: End receiving, total length=%d\n", g_sysex_length);
            return 1; // SYSEX 娑堟伅瀹屾垚
        }
        return 0;
    }
    else if (g_sysex_receiving)
    {
        // 鎺ユ敹 SYSEX 鏁版嵁瀛楄妭
        if (g_sysex_length < BLE_MIDI_MAX_SYSEX_SIZE)
        {
            g_sysex_buffer[g_sysex_length++] = data_byte;
        }
        else
        {
            BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX: Buffer overflow, resetting\n");
            g_sysex_receiving = 0;
            g_sysex_length = 0;
        }
    }
    
    return 0;
}

/**
 * @brief 澶勭悊瀹屾垚鐨�SYSEX 娑堟伅
 * @param conn_handle 杩炴帴鍙ユ焺
 */
static void handle_sysex_complete(uint16_t conn_handle)
{
    if (!g_sysex_receiving || g_sysex_length == 0)
        return;
    
    // 鏋勫缓 SYSEX 鏁版嵁缁撴瀯
    ble_midi_sysex_t sysex = {
        .data = g_sysex_buffer,
        .length = g_sysex_length,
        .manufacturer_id = (g_sysex_length > 0) ? g_sysex_buffer[0] : 0,
        .device_id = (g_sysex_length > 1) ? g_sysex_buffer[1] : 0
    };
    
    BLE_MIDI_SYSEX_DBG("BLE MIDI SYSEX: Processing complete message - mfg=0x%02X, dev=0x%02X, len=%d\n",
        sysex.manufacturer_id, sysex.device_id, sysex.length);
    
    // 璋冪敤鐢ㄦ埛鍥炶皟
    if (g_sysex_callback)
    {
        g_sysex_callback(conn_handle, &sysex);
    }
    
    // 閲嶇疆鎺ユ敹鐘舵�
    g_sysex_receiving = 0;
    g_sysex_length = 0;
}

/**
 * @brief 绀轰緥 SYSEX 鍥炶皟鍑芥暟
 * @param conn_handle 杩炴帴鍙ユ焺
 * @param sysex SYSEX 鏁版嵁
 */
void example_sysex_callback(uint16_t conn_handle, const ble_midi_sysex_t *sysex)
{
    if (!sysex || !sysex->data)
        return;
    
    BLE_MIDI_SYSEX_DBG("馃帥锔�SYSEX Callback: mfg=0x%02X, dev=0x%02X, len=%d\n",
        sysex->manufacturer_id, sysex->device_id, sysex->length);
    
    // 澶勭悊鏍囧噯璁惧鏌ヨ
    if (sysex->length >= 4 && 
        sysex->data[0] == 0x7E && // Universal System Exclusive
        sysex->data[2] == 0x06 && // General Information
        sysex->data[3] == 0x01)   // Identity Request
    {
        BLE_MIDI_SYSEX_DBG("馃攳 Device identity inquiry received, sending reply\n");
        ble_midi_send_device_identity_reply(conn_handle, 0x7D, 0x00, 0x1234, 0x01000000);
    }
    // 澶勭悊鑷畾涔夊埗閫犲晢娑堟伅
    else if (sysex->manufacturer_id == 0x7D) // Educational use
    {
        BLE_MIDI_SYSEX_DBG("馃彮 Educational manufacturer message received\n");
        // 杩欓噷鍙互娣诲姞鑷畾涔夊鐞嗛�杈�
    }
    else
    {
        BLE_MIDI_SYSEX_DBG("馃摜 Unknown SYSEX message (mfg=0x%02X)\n", sysex->manufacturer_id);
    }
}
#endif // BLE_SUPPORT && BLE_MIDI_ENABLE

//***********************************************************************************
// BLE MIDI Demo Functions (鍗曞厓娴嬭瘯)
//***********************************************************************************

#if (BLE_MIDI_DEMO_ENABLE)

static uint16_t demo_conn_handle = 0;
static uint32_t demo_last_tick = 0;

/**
 * @brief BLE MIDI婕旂ず鍑芥暟 - 鍙戦�娴嬭瘯闊崇
 */
void ble_midi_demo_send_notes(uint16_t conn_handle)
{
    BLE_MIDI_DBG("BLE MIDI Demo: Sending test notes...\n");
    
    // 鍙戦�C澶ц皟闊抽樁
    uint8_t notes[] = {60, 62, 64, 65, 67, 69, 71, 72}; // C4-C5
    
    for (int i = 0; i < 8; i++)
    {
        ble_midi_send_note_on(conn_handle, 0, notes[i], 100);
        // 瀹為檯搴旂敤涓繖閲屽簲璇ユ湁寤舵椂
        ble_midi_send_note_off(conn_handle, 0, notes[i], 0);
    }
}

/**
 * @brief 鍚姩BLE MIDI婕旂ず
 */
void ble_midi_start_demo_timer(uint16_t conn_handle)
{
    demo_conn_handle = conn_handle;
    g_ble_midi_conn_handle = conn_handle;
    demo_last_tick = xTaskGetTickCount();
    
    BLE_MIDI_DBG("BLE MIDI Demo: Starting! handle=0x%04X\n", conn_handle);
}

/**
 * @brief 鍋滄BLE MIDI婕旂ず
 */
void ble_midi_stop_demo_timer(void)
{
    demo_conn_handle = 0;
    demo_last_tick = 0;
    g_ble_midi_conn_handle = 0;
    
    BLE_MIDI_DBG("BLE MIDI Demo: Stopped\n");
}

/**
 * @brief BLE MIDI婕旂ず澶勭悊鍑芥暟
 */
void ble_midi_demo_handler(void)
{
    static uint32_t debug_counter = 0;
    debug_counter++;
    
    if (!BleConnectFlag || demo_conn_handle == 0)
    {
        return;
    }
    
    uint32_t current_tick = xTaskGetTickCount();
    uint32_t elapsed_ms;
    
    if (current_tick >= demo_last_tick)
    {
        elapsed_ms = current_tick - demo_last_tick;
    }
    else
    {
        elapsed_ms = (0xFFFFFFFF - demo_last_tick) + current_tick + 1;
    }
    
    if (elapsed_ms >= BLE_MIDI_DEMO_INTERVAL_MS)
    {
        demo_last_tick = current_tick;
        
        BLE_MIDI_DBG("BLE MIDI Demo: Sending Note %d\n", BLE_MIDI_DEMO_NOTE);
        int result = ble_midi_send_note_on(demo_conn_handle, BLE_MIDI_DEMO_CHANNEL, 
                                     BLE_MIDI_DEMO_NOTE, BLE_MIDI_DEMO_VELOCITY);
        if (result == 0)
        {
            BLE_MIDI_DBG("BLE MIDI Demo: Note sent successfully\n");
        }
        else
        {
            BLE_MIDI_DBG("BLE MIDI Demo: Failed to send note\n");
        }
    }
}

/**
 * @brief 绠�寲婕旂ず鍑芥暟 - 鐩存帴鍙戦�Note 60
 */
void ble_midi_demo_send_note60(uint16_t conn_handle)
{
    BLE_MIDI_DBG("BLE MIDI Demo: Sending Note 60 immediately\n");
    
    int result = ble_midi_send_note_on(conn_handle, BLE_MIDI_DEMO_CHANNEL, 
                                 BLE_MIDI_DEMO_NOTE, BLE_MIDI_DEMO_VELOCITY);
    if (result == 0)
    {
        BLE_MIDI_DBG("BLE MIDI Demo: Note sent successfully\n");
    }
    else
    {
        BLE_MIDI_DBG("BLE MIDI Demo: Failed to send note\n");
    }
}

#endif // BLE_MIDI_DEMO_ENABLE

//***********************************************************************************
// BLE MIDI 铚滈洩鍐板煄Demo Functions
//***********************************************************************************

#if (BLE_MIDI_MIXUE_DEMO_ENABLE)

// 铚滈洩鍐板煄涓婚鏃嬪緥
static const uint8_t mixue_melody[] = {
    66, 69, 69, 71,    // "浣犵埍鎴�
    69, 66, 62, 62, 64, // "鎴戠埍浣�
    66, 66, 64, 64,    // "铚滈洩鍐板煄"
    62,                // 浼戞
    67, 67,            // "鐢滆湝"
    67, 71,            // "浣犵埍鎴戝憖"
    69, 66, 64,        // "浣犵埍鎴�
    69, 66, 62, 62, 64, // "鎴戠埍浣�
    66, 66, 64, 64, 62, 62, // "铚滈洩鍐板煄鐢滆湝铚�
};

static const uint8_t mixue_durations[] = {
    2, 2, 3, 2,        // "浣犵埍鎴�
    2, 2, 2, 2, 3,     // "鎴戠埍浣�
    2, 2, 2, 2,        // "铚滈洩鍐板煄"
    6,                 // 浼戞
    2, 2,              // "鐢滆湝"
    2, 4,              // "浣犵埍鎴戝憖"
    2, 2, 4,           // "浣犵埍鎴�
    2, 2, 2, 2, 3,     // "鎴戠埍浣�
    2, 2, 2, 2, 4, 6,  // "铚滈洩鍐板煄鐢滆湝铚�
};

#define MIXUE_MELODY_LENGTH (sizeof(mixue_melody) / sizeof(mixue_melody[0]))

static uint16_t mixue_conn_handle = 0;
static uint32_t mixue_last_tick = 0;
static uint8_t mixue_current_note = 0;
static uint8_t mixue_demo_playing = 0;

/**
 * @brief 鍚姩铚滈洩鍐板煄Demo
 */
void ble_midi_start_mixue_demo(uint16_t conn_handle)
{
    mixue_conn_handle = conn_handle;
    g_ble_midi_conn_handle = conn_handle;
    mixue_last_tick = xTaskGetTickCount();
    mixue_current_note = 0;
    mixue_demo_playing = 1;
    
    BLE_MIDI_DBG("馃崷 铚滈洩鍐板煄Demo: Starting! handle=0x%04X\n", conn_handle);
    
    // 绔嬪嵆鎾斁绗竴涓煶绗�
    if (BleConnectFlag && mixue_current_note < MIXUE_MELODY_LENGTH)
    {
        uint8_t note = mixue_melody[mixue_current_note];
        BLE_MIDI_DBG("馃幍 铚滈洩鍐板煄Demo: Playing first note %d\n", note);
        ble_midi_send_note_on(mixue_conn_handle, BLE_MIDI_MIXUE_CHANNEL, note, BLE_MIDI_MIXUE_VELOCITY);
    }
}

/**
 * @brief 鍋滄铚滈洩鍐板煄Demo
 */
void ble_midi_stop_mixue_demo(void)
{
    mixue_demo_playing = 0;
    mixue_conn_handle = 0;
    mixue_last_tick = 0;
    mixue_current_note = 0;
    
    BLE_MIDI_DBG("馃崷 铚滈洩鍐板煄Demo: Stopped\n");
}

/**
 * @brief 铚滈洩鍐板煄Demo澶勭悊鍑芥暟
 */
void ble_midi_mixue_demo_handler(void)
{
    if (!mixue_demo_playing || !BleConnectFlag)
        return;
    
    uint32_t current_tick = xTaskGetTickCount();
    uint32_t elapsed_ms;
    
    if (current_tick >= mixue_last_tick)
    {
        elapsed_ms = current_tick - mixue_last_tick;
    }
    else
    {
        elapsed_ms = (0xFFFFFFFF - mixue_last_tick) + current_tick + 1;
    }
    
    uint32_t current_note_total_duration = BLE_MIDI_MIXUE_NOTE_INTERVAL_MS * 
        ((mixue_current_note < MIXUE_MELODY_LENGTH) ? mixue_durations[mixue_current_note] : 1);
    
    if (elapsed_ms >= current_note_total_duration)
    {
        mixue_last_tick = current_tick;
        
        // 鍙戦�Note Off缁欏綋鍓嶉煶绗�
        if (mixue_current_note < MIXUE_MELODY_LENGTH)
        {
            uint8_t current_note_value = mixue_melody[mixue_current_note];
            ble_midi_send_note_off(mixue_conn_handle, BLE_MIDI_MIXUE_CHANNEL, current_note_value, 0);
        }
        
        // 绉诲姩鍒颁笅涓�釜闊崇
        mixue_current_note++;
        
        // 妫�煡鏄惁鎾斁瀹屾墍鏈夐煶绗�
        if (mixue_current_note >= MIXUE_MELODY_LENGTH)
        {
            BLE_MIDI_DBG("馃幍 铚滈洩鍐板煄Demo: Melody finished! Restarting...\n");
            mixue_current_note = 0; // 閲嶆柊寮�鎾斁
        }
        
        // 鎾斁鏂扮殑闊崇
        if (mixue_current_note < MIXUE_MELODY_LENGTH)
        {
            uint8_t new_note = mixue_melody[mixue_current_note];
            BLE_MIDI_DBG("馃幍 铚滈洩鍐板煄Demo: Playing note %d\n", new_note);
            
            ble_midi_send_note_on(mixue_conn_handle, BLE_MIDI_MIXUE_CHANNEL, 
                             new_note, BLE_MIDI_MIXUE_VELOCITY);
        }
    }
}

#endif // BLE_MIDI_MIXUE_DEMO_ENABLE

//***********************************************************************************
// BLE MIDI 寮哄埗娴嬭瘯鍑芥暟 - 鐢ㄤ簬璋冭瘯
//***********************************************************************************

void ble_midi_force_test(void)
{
    static uint32_t test_counter = 0;
    static uint32_t last_test_tick = 0;
    
    test_counter++;
    uint32_t current_tick = xTaskGetTickCount();
    
    // 姣�绉掑己鍒跺彂閫佷竴娆￠煶绗﹁繘琛屾祴璇�
    uint32_t elapsed_ms;
    if (current_tick >= last_test_tick)
    {
        elapsed_ms = current_tick - last_test_tick;
    }
    else
    {
        elapsed_ms = (0xFFFFFFFF - last_test_tick) + current_tick + 1;
    }
    
    if (elapsed_ms >= 5000)  // 5绉�
    {
        last_test_tick = current_tick;
        
        BLE_MIDI_DBG("BLE MIDI Force Test: counter=%lu, current_tick=%lu\n",
            (unsigned long)test_counter, (unsigned long)current_tick);
        
        if (BleConnectFlag && g_ble_midi_conn_handle != 0)
        {
            BLE_MIDI_DBG("BLE MIDI Force Test: Sending test note\n");
            int result = ble_midi_send_note_on(g_ble_midi_conn_handle, 0, 72, 120); // Note C5
            BLE_MIDI_DBG("BLE MIDI Force Test: Send result = %d\n", result);
        }
        else
        {
            BLE_MIDI_DBG("BLE MIDI Force Test: Not connected\n");
        }
    }
}

/**
 * @brief 瑙ｆ瀽鎺ユ敹鍒扮殑BLE MIDI娑堟伅
 */
void ble_midi_parse_message(uint8_t *midi_data, uint8_t midi_len)
{
    if (!midi_data || midi_len == 0)
        return;

    uint8_t status = midi_data[0];
    uint8_t channel = status & 0x0F;
    uint8_t message_type = status & 0xF0;

    // 棣栧厛妫�煡鏄惁涓哄疄鏃舵秷鎭紙鏃堕挓鍚屾鐩稿叧锛�
    if (ble_midi_is_realtime_message(status)) {
        uint32_t timestamp = xTaskGetTickCount(); // 鑾峰彇鎺ユ敹鏃堕棿鎴�
        handle_midi_clock_message(status, timestamp);
        
        // 缁х画澶勭悊鍏朵粬瀹炴椂娑堟伅绫诲瀷
        switch (status) {
            case 0xF8: BLE_MIDI_CLOCK_DBG("鈴�MIDI Clock received\n"); break;
            case 0xFA: BLE_MIDI_CLOCK_DBG("鈻讹笍  MIDI Start received\n"); break;
            case 0xFB: BLE_MIDI_CLOCK_DBG("鈴革笍  MIDI Continue received\n"); break;
            case 0xFC: BLE_MIDI_CLOCK_DBG("鈴癸笍  MIDI Stop received\n"); break;
            case 0xFE: BLE_MIDI_DBG("馃挀 MIDI Active Sensing received\n"); break;
            case 0xFF: BLE_MIDI_DBG("馃攧 MIDI System Reset received\n"); break;
        }
        return;
    }

    // 澶勭悊閫氶亾娑堟伅
    if (message_type == 0x80 && midi_len >= 3) {
        BLE_MIDI_DBG("Note Off: CH=%d, Note=%d, Vel=%d\n", channel, midi_data[1], midi_data[2]);
    } else if (message_type == 0x90 && midi_len >= 3) {
        BLE_MIDI_DBG("Note On: CH=%d, Note=%d, Vel=%d\n", channel, midi_data[1], midi_data[2]);
    } else if (message_type == 0xB0 && midi_len >= 3) {
        BLE_MIDI_DBG("Control Change: CH=%d, CC=%d, Val=%d\n", channel, midi_data[1], midi_data[2]);
    } else if (message_type == 0xC0 && midi_len >= 2) {
        BLE_MIDI_DBG("Program Change: CH=%d, Program=%d\n", channel, midi_data[1]);
    } else if (message_type == 0xE0 && midi_len >= 3) {
        uint16_t pitch_bend = midi_data[1] | (midi_data[2] << 7);
        BLE_MIDI_DBG("Pitch Bend: CH=%d, Bend=%d\n", channel, pitch_bend);
    } else if (status == 0xF0) {
        BLE_MIDI_SYSEX_DBG("SYSEX Start\n");
    } else if (status == 0xF7) {
        BLE_MIDI_SYSEX_DBG("SYSEX End\n");
    } else if (status == 0xF2 && midi_len >= 3) {
        // Song Position Pointer
        uint16_t position = midi_data[1] | (midi_data[2] << 7);
        g_clock_sync.song_position = position;
        BLE_MIDI_CLOCK_DBG("馃搷 Song Position: %d\n", position);
    } else if (status == 0xF3 && midi_len >= 2) {
        BLE_MIDI_DBG("馃幍 Song Select: %d\n", midi_data[1]);
    } else {
        BLE_MIDI_DBG("Unknown MIDI Message: 0x%02X\n", status);
    }
}

/**
 * @brief 澶勭悊鎺ユ敹鍒扮殑BLE MIDI鏁版嵁
 */
void ble_midi_handle_received_data(uint16_t conn_handle, uint16_t handle, uint8_t *data, uint8_t data_len)
{
    // 妫�煡鏄惁鏄疢IDI鐗瑰緛鏁版嵁
	int i;
    if (handle == BLE_MIDI_CHARACTERISTIC_HANDLE)
    {
        BLE_MIDI_DBG("Received BLE MIDI Data: len=%d\n", data_len);
        
        // 棣栧厛妫�煡鏄惁鍖呭惈 SYSEX 鏁版嵁
        uint8_t has_sysex = 0;
        for (i = 0; i < data_len; i++)
        {
            if (data[i] == 0xF0 || data[i] == 0xF7 || g_sysex_receiving)
            {
                has_sysex = 1;
                break;
            }
        }
        
        if (has_sysex)
        {
            BLE_MIDI_SYSEX_DBG("Processing SYSEX data\n");
            // 閫愬瓧鑺傚鐞嗭紝妫�祴 SYSEX 娑堟伅
            for (i = 0; i < data_len; i++)
            {
                if (process_sysex_byte(data[i]))
                {
                    // SYSEX 娑堟伅瀹屾垚
                    handle_sysex_complete(conn_handle);
                }
            }
        }
        else
        {
            // 澶勭悊甯歌 MIDI 娑堟伅
            uint8_t midi_data[BLE_MIDI_MAX_MIDI_DATA_SIZE];
            uint8_t midi_len = 0;
            int msg_count = ble_midi_parse_data(data, data_len, midi_data, &midi_len);
            
            if (msg_count > 0 && midi_len > 0)
            {
                BLE_MIDI_VERBOSE_DBG("Parsed %d MIDI messages, total MIDI data length: %d\n", msg_count, midi_len);
                
                // 瑙ｆ瀽骞跺鐞哅IDI娑堟伅
                ble_midi_parse_message(midi_data, midi_len);
                
                // 绀轰緥锛氬洖鏄炬帴鏀跺埌鐨凪IDI鏁版嵁
                ble_midi_send_data(conn_handle, midi_data, midi_len);
            }
        }
    }
}

//***********************************************************************************
// BLE MIDI Clock Synchronization and Timing Support
//***********************************************************************************

/**
 * @brief 娉ㄥ唽鏃堕挓鍚屾鍥炶皟鍑芥暟
 * @param callback 鏃堕挓鍥炶皟鍑芥暟
 */
void ble_midi_register_clock_callback(ble_midi_clock_callback_t callback)
{
    g_clock_callback = callback;
    BLE_MIDI_CLOCK_DBG("BLE MIDI Clock: Callback registered\n");
}

/**
 * @brief 鍚敤/绂佺敤鏃堕挓鍚屾
 * @param enable 1=鍚敤, 0=绂佺敤
 */
void ble_midi_enable_clock_sync(uint8_t enable)
{
    g_clock_sync.clock_sync_enabled = enable;
    if (enable) {
        g_clock_sync.last_clock_time = xTaskGetTickCount();
        g_clock_sync.clock_counter = 0;
        g_clock_sync.transport_running = 0;
        BLE_MIDI_CLOCK_DBG("BLE MIDI Clock: Synchronization enabled\n");
    } else {
        BLE_MIDI_CLOCK_DBG("BLE MIDI Clock: Synchronization disabled\n");
    }
}

/**
 * @brief 澶勭悊MIDI鏃堕挓娑堟伅
 * @param message_type 娑堟伅绫诲瀷 (0xF8, 0xFA, 0xFB, 0xFC绛�
 * @param timestamp 鏃堕棿鎴�
 */
static void handle_midi_clock_message(uint8_t message_type, uint32_t timestamp)
{
    if (!g_clock_sync.clock_sync_enabled) {
        return;
    }
    
    switch (message_type) {
        case 0xF8: // Timing Clock (24 PPQ)
            {
                uint32_t current_time = xTaskGetTickCount();
                if (g_clock_sync.last_clock_time > 0) {
                    g_clock_sync.clock_interval_ms = current_time - g_clock_sync.last_clock_time;
                }
                g_clock_sync.last_clock_time = current_time;
                g_clock_sync.clock_counter++;
                
                BLE_MIDI_CLOCK_DBG("MIDI Clock: #%d, interval=%lums\n", 
                    g_clock_sync.clock_counter, 
                    (unsigned long)g_clock_sync.clock_interval_ms);
            }
            break;
            
        case 0xFA: // Start
            g_clock_sync.transport_running = 1;
            g_clock_sync.clock_counter = 0;
            g_clock_sync.song_position = 0;
            BLE_MIDI_CLOCK_DBG("MIDI Transport: Start\n");
            break;
            
        case 0xFB: // Continue
            g_clock_sync.transport_running = 1;
            BLE_MIDI_CLOCK_DBG("MIDI Transport: Continue from position %lu\n", 
                (unsigned long)g_clock_sync.song_position);
            break;
            
        case 0xFC: // Stop
            g_clock_sync.transport_running = 0;
            BLE_MIDI_CLOCK_DBG("MIDI Transport: Stop\n");
            break;
            
        case 0xF2: // Song Position Pointer
            // 姝屾洸浣嶇疆鍦∕IDI鏁版嵁涓紝杩欓噷鍙槸鍗犱綅
            BLE_MIDI_CLOCK_DBG("MIDI Song Position received\n");
            break;
            
        default:
            BLE_MIDI_CLOCK_DBG("MIDI Realtime: Unknown message 0x%02X\n", message_type);
            break;
    }
    
    // 璋冪敤鐢ㄦ埛鍥炶皟
    if (g_clock_callback) {
        g_clock_callback(message_type, timestamp);
    }
}

/**
 * @brief 鑾峰彇褰撳墠BPM锛堝熀浜庢椂閽熼棿闅旓級
 * @return BPM鍊硷紝0琛ㄧず鏃犳晥
 */
uint16_t ble_midi_get_current_bpm(void)
{
    if (!g_clock_sync.clock_sync_enabled || g_clock_sync.clock_interval_ms == 0) {
        return 0;
    }
    
    // MIDI鏃堕挓鏄�4 PPQ (Pulses Per Quarter note)
    // BPM = 60000 / (clock_interval_ms * 24)
    uint32_t bpm = 60000 / (g_clock_sync.clock_interval_ms * 24);
    return (uint16_t)bpm;
}

/**
 * @brief 鑾峰彇鏃堕挓鍚屾鐘舵�
 * @return 1=鍚屾涓� 0=鏈悓姝�
 */
uint8_t ble_midi_is_clock_synced(void)
{
    return g_clock_sync.clock_sync_enabled && g_clock_sync.transport_running;
}

/**
 * @brief 鑾峰彇姝屾洸浣嶇疆
 * @return 褰撳墠姝屾洸浣嶇疆锛坆eat涓哄崟浣嶏級
 */
uint32_t ble_midi_get_song_position(void)
{
    return g_clock_sync.song_position;
}

/**
 * @brief 绀轰緥鏃堕挓鍚屾鍥炶皟鍑芥暟
 * @param message_type 娑堟伅绫诲瀷
 * @param timestamp 鏃堕棿鎴�
 */
void example_clock_callback(uint8_t message_type, uint32_t timestamp)
{
    switch (message_type) {
        case 0xF8: // Clock
            // 姣�4涓椂閽熻剦鍐�= 1涓洓鍒嗛煶绗�
            if (g_clock_sync.clock_counter % 24 == 0) {
                BLE_MIDI_CLOCK_DBG("馃幍 Beat: %d (BPM: %d)\n",
                    g_clock_sync.clock_counter / 24, 
                    ble_midi_get_current_bpm());
            }
            break;
            
        case 0xFA: // Start
        case 0xFB: // Continue
        case 0xFC: // Stop
            BLE_MIDI_CLOCK_DBG("馃帥锔�Transport state changed: 0x%02X\n", message_type);
            break;
    }
}

/**
 * @brief 鏃堕挓鍚屾婕旂ず鍒濆鍖�
 */
void ble_midi_clock_sync_demo_init(void)
{
    ble_midi_enable_clock_sync(1);
    ble_midi_register_clock_callback(example_clock_callback);
    BLE_MIDI_CLOCK_DBG("鈴�BLE MIDI Clock Sync Demo initialized\n");
}

/**
 * @brief 鍙戦�鏃堕挓鍚屾娴嬭瘯搴忓垪
 * @param conn_handle 杩炴帴鍙ユ焺
 */
void ble_midi_send_clock_test_sequence(uint16_t conn_handle)
{
	int i;
    BLE_MIDI_CLOCK_DBG("鈴�Sending MIDI clock test sequence...\n");
    
    // 鍙戦�寮�娑堟伅
    ble_midi_send_start(conn_handle);
    
    // 鍙戦�鍑犱釜鏃堕挓鑴夊啿锛堟ā鎷�20 BPM锛�
    // 120 BPM = 2 beats/sec = 48 clocks/sec = ~20.8ms闂撮殧
    for (i = 0; i < 96; i++) { // 4涓洓鍒嗛煶绗�
        ble_midi_send_clock(conn_handle);
        // 鍦ㄥ疄闄呭簲鐢ㄤ腑杩欓噷搴旇鏈夌簿纭殑瀹氭椂寤惰繜
    }
    
    // 鍙戦�鍋滄娑堟伅
    ble_midi_send_stop(conn_handle);
    
    BLE_MIDI_CLOCK_DBG("鈴�Clock test sequence completed\n");
}
