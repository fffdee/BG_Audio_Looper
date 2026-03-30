////////////////////////////////////////////////
//
//
#include "debug.h"

#include "ble_api.h"
#include "ble_app_func.h"




/* 鈿狅笍 鐢熶骇鐜蹇呴』璁剧疆涓�锛岄伩鍏嶆祴璇曚换鍔″共鎵版甯搁�淇�*/
#define AUTO_START_NOTIFY_TEST 0

#if (AUTO_START_NOTIFY_TEST != 0)
#warning "AUTO_START_NOTIFY_TEST is enabled! This should be disabled in production."
#endif

#if (BLE_SUPPORT == ENABLE)

uint8_t BleConnectFlag=0;
void BLEStackCallBackFunc(uint8_t event)
{
	switch(event)
	{
		case BLE_STACK_INIT_OK:
			BT_DBG("BLE_STACK_INIT_OK\n");
			BleConnectFlag = 0;
			break;

		case BLE_STACK_CONNECTED:
			BT_DBG("BLE_STACK_CONNECTED\n");
			BleConnectFlag = 1;
			BT_DBG("[BLE] Connection established, waiting for CCCD subscription...\n");

			break;
			
		case BLE_STACK_DISCONNECTED:
			BT_DBG("BLE_STACK_DISCONNECTED\n");
			BleConnectFlag = 0;
		
#if (AUTO_START_NOTIFY_TEST)
			BLE_StopNotifyTest(); // 鏂紑鏃跺仠姝otify娴嬭瘯
#endif
			break;

		case GATT_SERVER_INDICATION_TIMEOUT:
			BT_DBG("GATT_SERVER_INDICATION_TIMEOUT\n");
			break;

		case GATT_SERVER_INDICATION_COMPLETE:
			BT_DBG("GATT_SERVER_INDICATION_COMPLETE\n");
			break;

		default:
			break;

	}
}

#endif

