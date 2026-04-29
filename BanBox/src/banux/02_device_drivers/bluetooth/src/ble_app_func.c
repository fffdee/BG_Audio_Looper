

#include "type.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "ble_api.h"
#include "ble_app_func.h"
#include "bt_app_func.h"
#include "bt_manager.h"
#include "chip_info.h"
#include "rtos_api.h"  // for osPortMalloc/osPortFree
//#include "app_config.h"
extern BT_CONFIGURATION_PARAMS		*btStackConfigParams;

#ifdef CFG_FUNC_AI
#include "ai.h"
#endif

#include "ble_process.h"
#include "shell_io_ble.h"
#include "debug.h"
#include "bg_event.h"      /* 事件发布-订阅系统 */
#if (BLE_SUPPORT == ENABLE)

// BLE advertisement data template - name will be replaced dynamically
static const uint8_t advertisement_data_template[] = {
	0x02, 0x01, 0x02,		//flag:LE General Disconverable
	0x03, 0x03, 0x00, 0xab,	//16bit service UUIDs
	13,  0x09, 'B', 'G','-','c','t','r','l','-','0','0','0','0',//  BG-ctrl-0000 (12 chars)
};

// BLE profile data template - name will be replaced dynamically
static const uint8_t profile_data_template[] =
{
    // 0x0001 PRIMARY_SERVICE-GAP_SERVICE
    0x0a, 0x00, 0x02, 0xf0, 0x01, 0x00, 0x00, 0x28, 0x00, 0x18,
    // 0x0002 CHARACTERISTIC-GAP_DEVICE_NAME-READ
    0x0d, 0x00, 0x02, 0xf0, 0x02, 0x00, 0x03, 0x28, 0x02, 0x03, 0x00, 0x00, 0x2a,
    // 0x0003 VALUE-GAP_DEVICE_NAME-READ-'BG-ctrl-0000'
    // READ_ANYBODY
    0x14, 0x00, 0x02, 0xf0, 0x03, 0x00, 0x00, 0x2a, 'B', 'G', '-', 'c', 't', 'r', 'l', '-', '0', '0', '0', '0',

    // 0x0004 PRIMARY_SERVICE-AB00
    0x0a, 0x00, 0x02, 0xf0, 0x04, 0x00, 0x00, 0x28, 0x00, 0xab,
    // 0x0005 CHARACTERISTIC-AB01-READ | WRITE | DYNAMIC
    0x0d, 0x00, 0x02, 0xf0, 0x05, 0x00, 0x03, 0x28, 0x0a, 0x06, 0x00, 0x01, 0xab,
    // 0x0006 VALUE-AB01-READ | WRITE | DYNAMIC-''
    // READ_ANYBODY, WRITE_ANYBODY
    0x08, 0x00, 0x0a, 0xf1, 0x06, 0x00, 0x01, 0xab,
    // 0x0007 CHARACTERISTIC-AB02-NOTIFY | DYNAMIC
    0x0d, 0x00, 0x02, 0xf0, 0x07, 0x00, 0x03, 0x28, 0x10, 0x08, 0x00, 0x02, 0xab,
    // 0x0008 VALUE-AB02-NOTIFY | DYNAMIC-''
    //
    0x08, 0x00, 0x00, 0xf1, 0x08, 0x00, 0x02, 0xab,
    // 0x0009 CLIENT_CHARACTERISTIC_CONFIGURATION
    // READ_ANYBODY, WRITE_ANYBODY
    // Changed back to 0xf1 to handle CCCD in app
    0x0a, 0x00, 0x0e, 0xf1, 0x09, 0x00, 0x02, 0x29, 0x00, 0x00,
    // 0x000a CHARACTERISTIC-AB03-NOTIFY | DYNAMIC
    0x0d, 0x00, 0x02, 0xf0, 0x0a, 0x00, 0x03, 0x28, 0x10, 0x0b, 0x00, 0x03, 0xab,
    // 0x000b VALUE-AB03-NOTIFY | DYNAMIC-''
    //
    0x08, 0x00, 0x00, 0xf1, 0x0b, 0x00, 0x03, 0xab,
    // 0x000c CLIENT_CHARACTERISTIC_CONFIGURATION
    // READ_ANYBODY, WRITE_ANYBODY
    // CRITICAL FIX: Changed from 0xf1 to 0xf0 for auto-handling
    0x0a, 0x00, 0x0e, 0xf0, 0x0c, 0x00, 0x02, 0x29, 0x00, 0x00,

    // END
    0x00, 0x00,
}; // total size 87 bytes

//
// list service handle ranges
//
#define ATT_SERVICE_GAP_SERVICE_START_HANDLE 0x0001
#define ATT_SERVICE_GAP_SERVICE_END_HANDLE 0x0003
#define ATT_SERVICE_AB00_START_HANDLE 0x0004
#define ATT_SERVICE_AB00_END_HANDLE 0x000e

//
// list mapping between characteristics and handles
//
#define ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_VALUE_HANDLE 0x0003
#define ATT_CHARACTERISTIC_AB01_01_VALUE_HANDLE 0x0006
#define ATT_CHARACTERISTIC_AB02_01_VALUE_HANDLE 0x0008
#define ATT_CHARACTERISTIC_AB02_01_CLIENT_CONFIGURATION_HANDLE 0x0009
#define ATT_CHARACTERISTIC_AB03_01_VALUE_HANDLE 0x000b
#define ATT_CHARACTERISTIC_AB03_01_CLIENT_CONFIGURATION_HANDLE 0x000c
#define ATT_CHARACTERISTIC_AB04_01_VALUE_HANDLE 0x000e


BLE_APP_CONTEXT			g_playcontrol_app_context;
GATT_SERVER_PROFILE		g_playcontrol_profile;
GAP_MODE				g_gap_mode;

// Dynamic BLE data (always enabled, not dependent on CFG_FUNC_AI)
static uint8_t *g_profile_data = NULL;
static uint8_t *g_advertisement_data = NULL;
static uint8_t g_advertisement_data_len = 0;
static bool g_ble_data_initialized = false;

#ifdef CFG_FUNC_AI
#include "ai.h"
#endif

int16_t app_att_read(uint16_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
int16_t app_att_write(uint16_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
int16_t att_read(uint16_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
int16_t att_write(uint16_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
int16_t gap_att_write(uint16_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

/**
 * @brief Initialize BLE advertisement and profile data with chip ID
 * This function is always called to set up dynamic BLE names
 */
static void init_ble_dynamic_data(void)
{
	if (g_ble_data_initialized) {
		return;  // Already initialized
	}
	
	char ble_name[20];
	uint32_t profile_data_len = 0;
	
	// Get chip unique ID for device name suffix
	uint64_t chip_id = 0;
	Chip_IDGet(&chip_id);
	uint16_t id_suffix = (uint16_t)((chip_id >> 48) & 0xFFFF);
	
	// BLE uses fixed base name "BG-ctrl"
	sprintf(ble_name, "BG-ctrl-%04X", id_suffix);
	uint8_t ble_name_len = strlen(ble_name);  // "BG-ctrl-XXXX" = 12 chars
	
	// Debug: print BLE name
	DBG("BLE Name: %s (chip_id: 0x%016llX, suffix: 0x%04X)\n", ble_name, chip_id, id_suffix);
	
	// Calculate data lengths (template uses 12 char name)
	g_advertisement_data_len = sizeof(advertisement_data_template);
	profile_data_len = sizeof(profile_data_template);
	
	// Allocate memory
	g_advertisement_data = (uint8_t *)osPortMalloc(g_advertisement_data_len);
	g_profile_data = (uint8_t *)osPortMalloc(profile_data_len);
	
	if (!g_advertisement_data || !g_profile_data) {
		DBG("BLE: Failed to allocate memory!\n");
		return;
	}
	
	// Copy template data
	memcpy(g_advertisement_data, advertisement_data_template, g_advertisement_data_len);
	memcpy(g_profile_data, profile_data_template, profile_data_len);
	
	// Update advertisement data with dynamic name
	// advertisement_data structure: [flag 3bytes][service 4bytes][name length][0x09][name 12bytes]
	// Name starts at offset 8 (after flag and service), name data at offset 10
	g_advertisement_data[7] = 1 + ble_name_len;  // Length byte = 0x09 type + name length
	memcpy(g_advertisement_data + 9, ble_name, ble_name_len);
	
	// Update profile data with dynamic name
	// profile_data: GAP_DEVICE_NAME value at offset where 0x00 0x2a appears
	// The name entry: [length][...][0x00][0x2a][name bytes]
	// In template, name starts at byte 8 of the name entry (after header)
	// Entry starts at offset 23 in template: 0x14, 0x00, 0x02, 0xf0, 0x03, 0x00, 0x00, 0x2a, 'B'...
	g_profile_data[23] = 8 + ble_name_len;  // Update length byte
	memcpy(g_profile_data + 31, ble_name, ble_name_len);  // Offset 31 = 23 + 8 = name start
	
	g_ble_data_initialized = true;
	DBG("BLE: Dynamic data initialized successfully\n");
}


#ifdef CFG_FUNC_AI
void ai_ble_set(char *name,uint8_t *bt_addr,uint8_t *ble_addr)
{
	char temp[20];
	uint32_t profile_data_len =0;
	
	// Get chip unique ID for device name suffix
	uint64_t chip_id = 0;
	Chip_IDGet(&chip_id);
	uint16_t id_suffix = (uint16_t)((chip_id >> 48) & 0xFFFF);
	
	// BLE uses fixed base name "BG-ctrl" instead of BT name
	const char* ble_base_name = "BG-ctrl";
	sprintf(temp, "%s-%04X", ble_base_name, id_suffix);
	uint8_t ble_name_len = strlen(temp);  // "BG-ctrl-XXXX" = 12 chars
	
	// Debug: print BLE name
	DBG("BLE Name: %s (chip_id: 0x%016llX, suffix: 0x%04X)\n", temp, chip_id, id_suffix);
	
	if(ble_name_len >= 12)
	{
		g_advertisement_data_len = sizeof(advertisement_data) + (ble_name_len - 12);
		profile_data_len = sizeof(profile_data) + (ble_name_len - 12);
	}
	else
	{
		g_advertisement_data_len = sizeof(advertisement_data) - (12 - ble_name_len);
		profile_data_len = sizeof(profile_data) - (12 - ble_name_len);
	}
	g_advertisement_data = osPortMalloc(g_advertisement_data_len);
	g_profile_data = osPortMalloc(profile_data_len);

	memset(g_advertisement_data,0,g_advertisement_data_len);
	memset(g_profile_data,0,profile_data_len);

	//g_advertisement_data
	uint8_t *p = (uint8_t *)advertisement_data;
	uint8_t len = 0;
	uint8_t len1 = 0;
	uint8_t offset = 0;
	uint8_t offset1 = 0;
	while(1)
	{
		len = p[0]+1;
		if(p[1] == 0x09)
		{
			g_advertisement_data[offset] = 0;
			g_advertisement_data[offset + 1] = 0x09;
		memcpy(g_advertisement_data + offset + 2, temp, ble_name_len);
		g_advertisement_data[offset] = 1 + ble_name_len;  // 0x09 type + full BLE name
			break;
		}
		else
		{
			memcpy(g_advertisement_data+offset,p,len);
			offset += len;
			p += len;
		}
		if(offset >  sizeof(advertisement_data))
		{
			printf("advertisement_data error    \n");
			while(1);
		}
	}

	p = (uint8_t *)profile_data;
	offset = 0;
	offset1 = 0;
	while(1)
	{
		len = p[0];
		if(len == 0)
		{
			break;
		}
		memcpy(g_profile_data+offset1,p,len);//取锟斤拷锟斤拷前锟斤拷锟斤拷目 copy锟斤拷g_profile_data
		if((p[6] == 0x00) && (p[7] == 0x2A))
		{
			memcpy(g_profile_data + offset1 + 8, temp, ble_name_len);
		g_profile_data[offset1] = 8 + ble_name_len;
		len1 = g_profile_data[offset1];
		}
		else
		{
			len1 = len;
		}
		//g_profile_data
		offset1 += len1;

		//profile_data  p
		offset += len;
		p += len;
	}
}
#endif


int8_t InitBlePlaycontrolProfile(void)
{
	// Always initialize dynamic BLE data with chip ID
	init_ble_dynamic_data();
	uint64_t chip_id = 0;
	Chip_IDGet(&chip_id);
	uint16_t id_suffix = (uint16_t)((chip_id >> 48) & 0xFFFF);
	memcpy(g_playcontrol_app_context.ble_device_addr, btStackConfigParams->ble_LocalDeviceAddr, 6);
	g_playcontrol_app_context.ble_device_addr[0] = 0x42;
	g_playcontrol_app_context.ble_device_addr[1] = 0x47;
	g_playcontrol_app_context.ble_device_addr[2] = 0x00;
	g_playcontrol_app_context.ble_device_addr[3] = (uint8_t)( id_suffix & 0xFF);
	g_playcontrol_app_context.ble_device_addr[4] = (uint8_t)(( id_suffix >> 8) & 0xFF);   // Use bits 8-15
	g_playcontrol_app_context.ble_device_addr[5] = (uint8_t)( id_suffix & 0xFF);
	g_playcontrol_app_context.ble_device_role = PERIPHERAL_DEVICE;

	// Always use dynamic profile data
	g_playcontrol_profile.profile_data = g_profile_data;
	g_playcontrol_profile.attr_read		= att_read;
	g_playcontrol_profile.attr_write	= att_write;

	// set gap mode
	g_gap_mode.broadcase_mode		= NON_BROADCAST_MODE;
	g_gap_mode.discoverable_mode	= GENERAL_DISCOVERABLE_MODE;
	g_gap_mode.connectable_mode		= UNDIRECTED_CONNECTABLE_MODE;
	g_gap_mode.bondable_mode		= NON_BONDABLE_MODE;
	SetGapMode(g_gap_mode);
	
	// Always use dynamic advertisement data
	SetAdvertisingData(g_advertisement_data, g_advertisement_data_len);
	return 0;
}


int8_t UninitBlePlaycontrolProfile(void)
{
	if(g_profile_data)
	{
		osPortFree(g_profile_data);
	}
	g_profile_data = NULL;
	if(g_advertisement_data)
	{
		osPortFree(g_advertisement_data);
	}
	g_advertisement_data = NULL;
	g_ble_data_initialized = false;
	return 0;
}

int16_t att_read(uint16_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size)
{
//	int att_value_len;
    if( (attribute_handle >= ATT_SERVICE_GAP_SERVICE_START_HANDLE) && (attribute_handle <= ATT_SERVICE_GAP_SERVICE_END_HANDLE))
	{
    	printf("att_read attribute_handle:%u\n",attribute_handle);
    	switch(attribute_handle)
		{
//	        case ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_CLIENT_CONFIGURATION_HANDLE:
//	            att_value_len = strlen((const char*)gap_device_name);
//	            if (buffer)
//	            {
//	                memcpy(buffer, gap_device_name, att_value_len);
//	            }
//	            return att_value_len;

	        default:
	            return 0;
		}
	}
    else if( (attribute_handle >= ATT_SERVICE_AB00_START_HANDLE) && (attribute_handle <= ATT_SERVICE_AB00_END_HANDLE))
	{
    	return app_att_read(con_handle, attribute_handle, offset, buffer, buffer_size);
	}
	else
	{
		//未知锟斤拷锟�
	}

    return 0;
}

int16_t att_write(uint16_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    if( (attribute_handle >= ATT_SERVICE_GAP_SERVICE_START_HANDLE) && (attribute_handle <= ATT_SERVICE_GAP_SERVICE_END_HANDLE))
	{
    	printf("att_write attribute_handle:%u\n",attribute_handle);
    	switch(attribute_handle)
    	{
//			case ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_CLIENT_CONFIGURATION_HANDLE:
//				if(buffer_size >= sizeof(gap_device_name))
//				{
//					buffer_size = sizeof(gap_device_name);
//				}
//				memcpy(gap_device_name,buffer+offset,buffer_size);
//				return buffer_size;

			default:
				return 0;
    	}
	}
    else if( (attribute_handle >= ATT_SERVICE_AB00_START_HANDLE) && (attribute_handle <= ATT_SERVICE_AB00_END_HANDLE))
	{
		/* With CCCD flags changed to 0xf1, we need to handle CCCD writes in app */
		if (attribute_handle == ATT_CHARACTERISTIC_AB02_01_CLIENT_CONFIGURATION_HANDLE ||
		    attribute_handle == ATT_CHARACTERISTIC_AB03_01_CLIENT_CONFIGURATION_HANDLE) {
			BT_DBG("[ATT_WRITE] CCCD handle 0x%02x write: ", attribute_handle);
			int i;
			for (i = 0; i < buffer_size; i++) {
				BT_DBG("%02x ", buffer[i]);
			}
			BT_DBG("\n");
			
			// Update CCCD status
			extern uint8_t g_BLE_CCCD_Enabled;
			if (buffer_size >= 2) {
				uint16_t cccd_value = (buffer[1] << 8) | buffer[0];
				if (cccd_value & 0x0001) {
					g_BLE_CCCD_Enabled = 1;
					BT_DBG("[CCCD] ENABLED: Notifications enabled for handle 0x%02x\n", attribute_handle);
					// Send a test notification to confirm
					extern uint16_t BLE_Send(uint8_t *data, uint16_t len);
					char test_msg[] = "[BLE_TEST] Notifications enabled!\r\n";
					BLE_Send((uint8_t *)test_msg, strlen(test_msg));
				} else {
					g_BLE_CCCD_Enabled = 0;
					BT_DBG("[CCCD] DISABLED: Notifications disabled for handle 0x%02x\n", attribute_handle);
				}
			}
			return 0;  // ATT_SUCCESS (0x00) - correct response for successful write
		}
		/* Route non-CCCD handles to app_att_write */
    	return app_att_write(con_handle, attribute_handle, transaction_mode, offset, buffer, buffer_size);
	}
	else
	{
		//未知锟斤拷锟�
	}
    return 0;
}


int16_t app_att_read(uint16_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size)
{
	BT_DBG("app_att_read for handle %02x\n", attribute_handle);
	switch(attribute_handle)
	{
		case ATT_CHARACTERISTIC_AB01_01_VALUE_HANDLE:
			BT_DBG("ATT_CHARACTERISTIC_AB01_01_VALUE_HANDLE:\n");
			if(buffer == 0)//锟斤拷锟铰达拷锟斤拷目锟斤拷锟斤拷锟捷的筹拷锟斤拷
			{

			}
			else
			{

			}
			break;

		case ATT_CHARACTERISTIC_AB02_01_VALUE_HANDLE:
			BT_DBG("ATT_CHARACTERISTIC_AB02_01_VALUE_HANDLE:\n");
			if(buffer == 0)//锟斤拷锟铰达拷锟斤拷目锟斤拷锟斤拷锟捷的筹拷锟斤拷
			{

			}
			else
			{

			}
			break;

		case ATT_CHARACTERISTIC_AB03_01_VALUE_HANDLE:
			BT_DBG("ATT_CHARACTERISTIC_AB03_01_VALUE_HANDLE:\n");
			if(buffer == 0)//锟斤拷锟铰达拷锟斤拷目锟斤拷锟斤拷锟捷的筹拷锟斤拷
			{

			}
			else
			{

			}
			break;

		default:
			return 0;
	}
	return 0;
}



int16_t app_att_write(uint16_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
	BT_DBG("app_att_write for handle %02x\n", attribute_handle);
	switch(attribute_handle)
	{
		case ATT_CHARACTERISTIC_AB01_01_VALUE_HANDLE:
			/* 发布 BLE 数据接收事件到事件总线 */
			{
				BG_EventBleRxData_t ble_rx;
				ble_rx.data = buffer;
				ble_rx.len  = buffer_size;
				BG_EVT_PUB_DATA(EVT_BLE_DATA_RECEIVED, &ble_rx, sizeof(ble_rx));
			}
			/* Shell命令行处理 */
			ShellIO_BLE_OnDataReceived(buffer, buffer_size);
			BT_DBG("ATT_CHARACTERISTIC_AB01_01_VALUE_HANDLE:\n");
			break;

		case ATT_CHARACTERISTIC_AB02_01_VALUE_HANDLE:
			BT_DBG("ATT_CHARACTERISTIC_AB02_01_VALUE_HANDLE:\n");
			break;

		case ATT_CHARACTERISTIC_AB03_01_VALUE_HANDLE:
			BT_DBG("ATT_CHARACTERISTIC_AB03_01_VALUE_HANDLE:\n");
			break;

		/* 
		 * 重要：不要在这里处理CCCD handles！
		 * ATT栈会自动处理CCCD写入，更新内部状态，使att_server_can_send()返回1
		 * 如果我们拦截CCCD写入，会阻止ATT栈更新状态
		 *
		 * ATT_CHARACTERISTIC_AB02_01_CLIENT_CONFIGURATION_HANDLE (0x0009)
		 * ATT_CHARACTERISTIC_AB03_01_CLIENT_CONFIGURATION_HANDLE (0x000c)
		 * 这些由ATT栈自动处理
		 */

		default:
			/* 对于未处理的handle（包括CCCD），返回0表示成功 */
			/* ATT栈会自己处理CCCD并更新内部状态 */
			BT_DBG("app_att_write: unhandled handle 0x%02x, letting ATT stack handle\n", attribute_handle);
			return 0;
	}
	return 0;
}


#ifdef CFG_FUNC_AI
extern int att_server_can_send(void);
void ai_ble_run_loop(void)
{
	if (att_server_can_send() == 0)
	{
		return ;
	}
	ble_send_data(ATT_CHARACTERISTIC_AB02_01_VALUE_HANDLE,ATT_CHARACTERISTIC_AB03_01_VALUE_HANDLE);
}
#endif

#endif

