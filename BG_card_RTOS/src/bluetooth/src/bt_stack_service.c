/**
 **************************************************************************************
 * @file    bt_stack_service.c
 * @brief   
 *
 * @author  KK
 * @version V1.0.0
 *
 * $Created: 2018-2-9 13:06:47$
 *
 * @Copyright (C) 2016, Shanghai Mountain View Silicon Co.,Ltd. All rights reserved.
 **************************************************************************************
 */


#include <string.h>
#include "type.h"
//#include "app_config.h"
#include "gpio.h" //for BOARD
#include "debug.h"
#include "uarts.h"
#include "uarts_interface.h"
#include "dma.h"
#include "timeout.h"
#include "irqn.h"
#include "ble_api.h"
#include "ble_app_func.h"

#include "clk.h"
#include "reset.h"
#include "bb_api.h"
#include "bt_app_func.h"
#include "bt_app_interface.h"
#include "bt_avrcp_api.h"
#include "bt_manager.h"
#include "bt_pbap_api.h"
#include "bt_platform_interface.h"
#include "bt_stack_api.h"
#include "app_message.h"
#include "bt_config.h"

#include "bt_stack_service.h"
#ifdef CFG_FUNC_AI
#include "ai.h"
#endif

//#ifdef CFG_APP_BT_MODE_EN

#ifdef CFG_BT_BACKGROUND_RUN_EN
uint8_t gBtHostStackMemHeap[BT_STACK_MEM_SIZE];
#endif

//BR/EDR STACK SERVICE
#define BT_STACK_SERVICE_STACK_SIZE		768
#define BT_STACK_SERVICE_PRIO			3
#define BT_STACK_NUM_MESSAGE_QUEUE		10

//USER SERVICE //��������Э��ջcallback���û���Ҫ�����msg
#define BT_USER_SERVICE_STACK_SIZE		256
#define BT_USER_SERVICE_PRIO			3
#define BT_USER_NUM_MESSAGE_QUEUE		10

typedef struct _BtStackServiceContext
{
//	xTaskHandle			taskHandle;
//	MessageHandle		msgHandle;
	TaskState			serviceState;

	uint8_t				serviceWaitResume;	//1:�������ں�̨����ʱ,����ͨ��,�˳�����ģʽ,����kill����Э��ջ

	uint8_t				bbErrorMode;
	uint8_t				bbErrorType;
}BtStackServiceContext;

static BtStackServiceContext	*btStackServiceCt = NULL;

BT_CONFIGURATION_PARAMS		*btStackConfigParams = NULL;

//extern uint8_t bt_powerup_flag;




unsigned char BT_Stack_Service_Context[sizeof(BtStackServiceContext)];
unsigned char BT_Stack_Config_Params[sizeof(BT_CONFIGURATION_PARAMS)];



#define ATT_CHARACTERISTIC_AB02_01_VALUE_HANDLE 0x0008
uint8_t bleNotifyBuf[128]={0};

 void BtStackServiceEntrance(void)
{

	uint8_t len;
	uint8_t c;

	BT_DBG("BtStackServiceEntrance.\n");

	//BR/EDR init
	if(!BtStackInit())
	{
		BT_DBG("error init bt device\n");
		//���ֳ�ʼ���쳣ʱ,����Э��ջ�������
		while(1)
		{

		}
	}
	else
	{
		BT_DBG("bt device init success!\n");
	}

	//BLE init
#if (BLE_SUPPORT == ENABLE)
	{
		//��ʼ��LE�������
		for( len = 0; len < sizeof( bleNotifyBuf ); len++ )
			bleNotifyBuf[ len ]= len;

		InitBlePlaycontrolProfile();
		
		if(!InitBleStack(&g_playcontrol_app_context, &g_playcontrol_profile))
		{
			BT_DBG("error ble stack init\n");
		}
	}
#endif

//	//����������Э��ջ��ת��A2DP�����Ϻ���ٵ��ý��벥��
//	while(1)
//	{
//
//		rw_main();
//
//		BTStackRun();
//
////��������ж�������Ƶ��ݣ����У�����벢����
//		A2dp_Decode();
//
//		//ʹ�ô����������ݣ����ʹ�õ��ǵ��Կڣ���ȷ�����ںźͰ弶����һ��
//		if(UART0_RecvByte(&c))
//		{
//			switch( c ){
//			case 'S':
//				//�������LE�����Ϻ��ٷ������
//				att_server_notify( ( uint8_t )ATT_CHARACTERISTIC_AB02_01_VALUE_HANDLE, bleNotifyBuf, sizeof( bleNotifyBuf ) );
//				break;
//			}
//		}
//
//	}
}

/**
 * @brief	Start bluetooth stack service initial.
 * @param	NONE
 * @return	
 */


static void BtStackServiceInit(void)
{
	BT_DBG("bluetooth stack service init.\n");

	btStackServiceCt = (BtStackServiceContext*)BT_Stack_Service_Context;//(BtStackServiceContext*)osPortMalloc(sizeof(BtStackServiceContext));

	memset(btStackServiceCt, 0, sizeof(BtStackServiceContext));
	
	btStackConfigParams = (BT_CONFIGURATION_PARAMS*)BT_Stack_Config_Params;//(BT_CONFIGURATION_PARAMS*)osPortMalloc(sizeof(BT_CONFIGURATION_PARAMS));

	memset(btStackConfigParams, 0, sizeof(BT_CONFIGURATION_PARAMS));


}

/**
 * @brief	Start bluetooth stack service.
 * @param	NONE
 * @return	
 */
 
void BtStackServiceStart(void)
{

	BtBbParams bbParams;

	
	memset((uint8_t*)BB_EM_MAP_ADDR, 0, BB_EM_SIZE);//clear em erea
	
	ClearBtManagerReg();
	

	SetBtStackState(BT_STACK_STATE_INITAILIZING);
	
	BtStackServiceInit();


	//load bt stack all params
	LoadBtConfigurationParams();
		
	//BB init
	ConfigBtBbParams(&bbParams);
	Bt_init((void*)&bbParams);

	//host memory init
	SetBtPlatformInterface(&pfiOS, &pfiBtDdb);
		

	BtStackServiceEntrance();


}


void BtStackServiceRun(void)
{
	rw_main();

	BTStackRun();
}




