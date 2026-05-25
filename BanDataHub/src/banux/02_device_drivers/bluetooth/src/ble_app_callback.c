////////////////////////////////////////////////
//
//
#include "debug.h"

#include "ble_api.h"
#include "ble_app_func.h"

#include "shell_io_ble.h" // 引入notify测试接口
#include "bg_event.h"     // 事件发布-订阅系统

/* ⚠️ 生产环境必须设置为0，避免测试任务干扰正常通信 */
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
			BG_EVT_PUB(EVT_BLE_CONNECTED);
			BT_DBG("[BLE] Connection established, waiting for CCCD subscription...\n");
#if (AUTO_START_NOTIFY_TEST)
			BLE_StartNotifyTest(); // 自动启动notify测试
			BT_DBG("[BLE] Auto-start notify test enabled\n");
#endif
			break;
			
		case BLE_STACK_DISCONNECTED:
			BT_DBG("BLE_STACK_DISCONNECTED\n");
			BleConnectFlag = 0;
			BG_EVT_PUB(EVT_BLE_DISCONNECTED);
			/* 清除同步任务状态，防止重连后弹窗永不消失 */
			{
				extern void BleProto_OnDisconnected(void);
				BleProto_OnDisconnected();
			}
#if (AUTO_START_NOTIFY_TEST)
			BLE_StopNotifyTest(); // 断开时停止notify测试
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

