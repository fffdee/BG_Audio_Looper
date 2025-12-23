/**
 *****************************************************************************
 * @file     device_stor_audio_request.c
 * @author   owen
 * @version  V1.0.0
 * @date     7-September-2015
 * @brief    device audio and mass-storage module driver interface
 *****************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT 2013 MVSilicon </center></h2>
 */

#include <string.h>
#include "type.h"
#include "debug.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "otg_device_descriptor.h"
#include "otg_device_audio.h"
#include "otg_device_cdc.h"
#define CFG_APP_USB_AUDIO_MODE_EN
#ifdef CFG_APP_CONFIG
#include "app_config.h"
#include "mode_switch_api.h"
#include "main_task.h"
#if FLASH_BOOT_EN
void start_up_grate(uint32_t UpdateResource);
#endif
#endif

//------------------------------------//
void HIDUsb_Tx(uint8_t *buf,uint16_t len);
void HIDUsb_Rx(uint8_t *buf,uint16_t len);


uint8_t hid_tx_buf[256];
void IsAndroid(void);

//------------------------------------//


const uint8_t  DeviceQualifier[10] = {10,6,0x10,0x01,0,0,0,64,1,0};
extern void OnDeviceAudioRcvIsoPacket(void);
extern void OnDeviceAudioSendIsoPacket(void);

extern void OTG_DeviceAudioRequest(void);
void hid_recive_data(void);
void hid_send_data(void);
const uint8_t DeviceString_LangID[] = {0x04, 0x03, 0x09, 0x04};

uint8_t Setup[8];
uint8_t Request[256];

uint8_t *ConfigDescriptor;
uint8_t *InterfaceNum;
const char *gDeviceProductString ="BG Card audio";		//max length: 32bytes
const char *gDeviceString_Manu ="BanGO";		//max length: 32bytes
const char *gDeviceString_SerialNumber ="20250405";//max length: 32bytes
uint8_t *gDeviceString_Index;

extern UsbAudio UsbAudioSpeaker;
extern UsbAudio UsbAudioMic;

void OTG_DeviceModeSel(uint8_t Mode,uint16_t UsbVid,uint16_t UsbPid)
{
	
	DeviceDescriptor[8] = UsbVid;
	DeviceDescriptor[9] = UsbVid>>8;
	DeviceDescriptor[10] = UsbPid;
	DeviceDescriptor[11] = UsbPid>>8;
	ConfigDescriptor = (uint8_t *)ConfigDescriptorTab[Mode];
	InterfaceNum = (uint8_t *)InterFaceNumTab[Mode];

 	gDeviceProductString		    = "BG Card audio"; 	//max length: 32bytesv bkd add
 	gDeviceString_Manu 		        = "BanGO";			    	//max length: 32bytes
	gDeviceString_SerialNumber      = "20250405";						//max length: 32bytes
}



/**
 * @brief  锟斤拷锟酵匡拷锟狡达拷锟斤拷锟斤拷锟斤拷锟接︼拷锟斤拷锟斤拷锟� * @param  Resp 应锟斤拷锟斤拷锟斤拷
 * @param  n 应锟斤拷锟斤拷锟捷筹拷锟饺ｏ拷1 or 2
 * @return NONE
 */
void OTG_DeviceSendResp(uint16_t Resp, uint8_t n)
{
	Resp = CpuToLe16(Resp);
	OTG_DeviceControlSend((uint8_t*)&Resp, n,3);
}

/**
 * @brief  锟斤拷锟斤拷锟斤拷取锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
 * @param  NONE
 * @return NONE
 */
void OTG_DeviceGetDescriptor(void)
{
	uint8_t 	StringBuf[32 * 2 + 2];
	uint8_t*	UsbSendPtr = 0;
	uint16_t	Len = 0;
	switch(Setup[3])
	{
		case USB_DT_DEVICE:
		//DBG("USB_DT_DEVICE\n");
			UsbSendPtr = (uint8_t*)DeviceDescriptor;
			Len = sizeof(DeviceDescriptor);
			break;

		case USB_DT_CONFIG:
		//DBG("USB_DT_CONFIG\n");
			UsbSendPtr = (uint8_t*)ConfigDescriptor;
            Len = UsbSendPtr[3];
            Len = Len<<8;
            Len = Len + UsbSendPtr[2];
			break;

		case USB_DT_STRING:
		//DBG("USB_DT_STRING\n");
			if(Setup[2] == 0)			//lang ids
			{
				UsbSendPtr = (uint8_t*)DeviceString_LangID;
				Len = UsbSendPtr[0];
				break;
			}
			else if(Setup[2] == 1)		//manu
			{
				UsbSendPtr = (uint8_t*)gDeviceString_Manu;
			}
			else if(Setup[2] == 2)		//product
			{
				UsbSendPtr = (uint8_t*)gDeviceProductString;
			}
			else if(Setup[2] == 4)		//debug effect
			{
				UsbSendPtr = gDeviceString_Index;
			}			
			else 	//serial number
			{
				UsbSendPtr = (uint8_t*)gDeviceString_SerialNumber;
			}

			for(Len = 0; Len < 32; Len++)
			{
				if(UsbSendPtr[Len] == '\0')
				{
					break;
				}
				StringBuf[2 + Len * 2 + 0] = UsbSendPtr[Len];
				StringBuf[2 + Len * 2 + 1] = 0x00;
			}

			Len = Len * 2 + 2;
			StringBuf[0] = Len;
			StringBuf[1] = 0x03;
			UsbSendPtr = StringBuf;
			break;

		case USB_DT_INTERFACE:
			//PC默锟较诧拷锟结发锟酵革拷锟斤拷锟斤拷
		//	DBG("USB_DT_INTERFACE\n");
			break;

		case USB_DT_ENDPOINT:
			//PC默锟较诧拷锟结发锟酵革拷锟斤拷锟斤拷
		//DBG("USB_DT_ENDPOINT\n");
			break;
			
		case USB_DT_DEVICE_QUALIFIER:
			UsbSendPtr = (uint8_t*)DeviceQualifier;
			Len = 10;
			break;

		case USB_HID_REPORT:
			if(Setup[4] == InterfaceNum[HID_DATA_INTERFACE_NUM])
			{
				//DBG("HID_DATA_INTERFACE_NUM REPORTR\n");
#if HID_DATA_FUN_EN
				UsbSendPtr = (uint8_t*)&HidDataReportDescriptor[0];
				Len = sizeof(HidDataReportDescriptor);
#endif
			}
			else if(Setup[4] == InterfaceNum[HID_CTL_INTERFACE_NUM])
			{
				//DBG("HID_CTL_INTERFACE_NUM REPORTR\n");
				UsbSendPtr = (uint8_t*)&AudioCtrlReportDescriptor[0];
				Len = sizeof(AudioCtrlReportDescriptor);
			}
			else
			{
				//DBG("NOT FOUND INTERFACE %d\n",Setup[4]);
			}
			break;

		default:
		DBG("UsbDeviceSendStall:100\n");
			OTG_DeviceStallSend(DEVICE_CONTROL_EP);
			return;
	}

	if(Len > (Setup[7] * 256 + Setup[6]))
	{

		Len = Setup[7] * 256 + Setup[6];
	}
	OTG_DeviceControlSend((uint8_t*)UsbSendPtr, Len,3);
}

void OTG_DeviceAudioInit();
//extern uint32_t SpeakerRun;
void OTG_DeviceStandardRequest()
{
	uint8_t Resp[8];
	//DBG("\nSetup[1] = %d\n\n", Setup[1]);
	switch(Setup[1])
	{
		case USB_REQ_GET_STATUS:
			//OTG_DBG("GetStatus\n");
			//锟斤拷锟节伙拷取USB锟借备锟接口端碉拷锟阶刺�			Resp[0] = 0x00;
			Resp[1] = 0x00;
			OTG_DeviceControlSend((uint8_t*)&Resp,2,10);
			break;

		case USB_REQ_CLEAR_FEATURE:
			//OTG_DBG("ClearFeature\n");
			//锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷呓锟街筓SB锟借备00,锟接匡拷01,锟剿碉拷02,锟斤拷某些锟斤拷锟皆ｏ拷锟斤拷锟斤拷锟捷达拷锟斤拷
			break;

		case USB_REQ_SET_FEATURE:
		//	OTG_DBG("SetFeature\n");
			//锟斤拷锟斤拷锟斤拷锟矫伙拷锟斤拷使USB锟借备00,锟接匡拷01,锟剿碉拷02,锟斤拷某些锟斤拷锟皆ｏ拷锟斤拷锟斤拷锟捷达拷锟斤拷
			OTG_DeviceStallSend(Setup[4]);
			break;

		case USB_REQ_SET_ADDRESS:
		//	OTG_DBG("SetAddress\n");
			OTG_DeviceAddressSet(Setup[2] & 0x7F);
			break;

		case USB_REQ_GET_DESCRIPTOR:
			//OTG_DBG("GetDescriptor\n");
			OTG_DeviceGetDescriptor();
			break;
		
		case USB_REQ_SET_DESCRIPTOR:
			//OTG_DBG("GetDescriptor111\n");
			//DeviceGetDescriptor();
			break;

		case USB_REQ_GET_CONFIGURATION:
		//	OTG_DBG("GetConfiguration\n");
			Resp[0] = 0x01;
			//OtgDeviceControlSend(Resp, 1,3);
			OTG_DeviceControlSend((uint8_t*)&Resp, 1,3);
			break;

		case USB_REQ_SET_CONFIGURATION:
#ifdef CFG_COMMUNICATION_BY_USB
#ifndef CFG_BT_KARAOKE_APP
			if((GetSystemMode() == AppModeUsbDevicePlay)
#ifndef BT_TWS_SUPPORT
			|| (GetSystemMode() == AppModeUsbPhone)
#endif
			)
#endif
#endif
			{
			//	DBG("Audio_ISO sam test\n");
				OTG_DeviceEndpointReset(DEVICE_INT_IN_EP,TYPE_INT_IN);
				OTG_DeviceEndpointReset(DEVICE_BULK_IN_EP,TYPE_BULK_IN);
				OTG_DeviceEndpointReset(DEVICE_BULK_OUT_EP,TYPE_BULK_OUT);
				OTG_DeviceEndpointReset(DEVICE_ISO_IN_EP,TYPE_ISO_IN);
				OTG_DeviceEndpointReset(DEVICE_ISO_OUT_EP,TYPE_ISO_OUT);

				// CDC端点复用Bulk端点，无需单独reset
				// OTG_DeviceEndpointReset(DEVICE_CDC_CMD_EP,TYPE_INT_IN);
				// OTG_DeviceEndpointReset(DEVICE_CDC_DATA_IN_EP,TYPE_BULK_IN);
				// OTG_DeviceEndpointReset(DEVICE_CDC_DATA_OUT_EP,TYPE_BULK_OUT);
#ifdef CFG_APP_USB_AUDIO_MODE_EN
				OTG_EndpointInterruptEnable(DEVICE_ISO_OUT_EP,OnDeviceAudioRcvIsoPacket);
				OTG_EndpointInterruptEnable(DEVICE_ISO_IN_EP,OnDeviceAudioSendIsoPacket);
#endif			
				OTG_DeviceISOSend(DEVICE_ISO_IN_EP,0,0);
#ifdef CFG_APP_USB_AUDIO_MODE_EN
				OTG_DeviceAudioInit();
				UsbAudioMic.InitOk = 1;
				UsbAudioSpeaker.InitOk = 1;
#endif

				OTG_DeviceCDC_Init();
				DBG("CDC Device Initialized\n");
			}
			break;

		case USB_REQ_GET_INTERFACE:
			OTG_DeviceControlSend((uint8_t*)&Resp, 1,3);
			break;

		case USB_REQ_SET_INTERFACE:
		#ifdef CFG_APP_USB_AUDIO_MODE_EN
			//DBG("Setup[4] %d",Setup[4]);
			if(Setup[4] == InterfaceNum[AUDIO_SRM_IN_INTERFACE_NUM])
			{
				//DBG("mic %d",Setup[2]);
				UsbAudioMic.AltSet = Setup[2];
			}
			else if(Setup[4] == InterfaceNum[AUDIO_SRM_OUT_INTERFACE_NUM])
			{
				UsbAudioSpeaker.AltSet = Setup[2];
				//DBG("speaker %d",Setup[2]);
			}
		#endif
			break;

		case USB_REQ_SYNCH_FRAME:
			//OTG_DBG("SYNC FRAME\n");
			break;

		default:
		//	OTG_DBG("UsbDeviceSendStall 006\n");
			OTG_DeviceStallSend(DEVICE_CONTROL_EP);
			break;
	}
}

uint32_t pc_upgrade = 0;

//锟借备锟斤拷锟斤拷锟斤拷
void OTG_DeviceClassRequest()
{
	if((Setup[0] == 0x22) && (Setup[1] == 0x01))
	{
#ifdef CFG_APP_USB_AUDIO_MODE_EN
		OTG_DeviceAudioRequest();
#endif
		return;
	}
	if(Setup[4] == InterfaceNum[MSC_INTERFACE_NUM])
	{
	//	OTG_DBG("MSC_INTERFACE_NUM\n");
		OTG_DeviceSendResp(0x0000, 1);
	}
	
#ifdef CFG_APP_USB_AUDIO_MODE_EN
	else if(Setup[4] ==  InterfaceNum[AUDIO_ATL_INTERFACE_NUM])
	{
		DBG("AUDIO_ATL_INTERFACE_NUM\n");
		OTG_DeviceAudioRequest();
	}
#endif

	else if(Setup[4] == InterfaceNum[AUDIO_SRM_OUT_INTERFACE_NUM])
	{
	//	OTG_DBG("AUDIO_SRM_OUT_INTERFACE_NUM\n");
	}
	else if(Setup[4] == InterfaceNum[AUDIO_SRM_IN_INTERFACE_NUM])
	{
	//	OTG_DBG("AUDIO_SRM_IN_INTERFACE_NUM\n");
	}
	else if(Setup[4] == InterfaceNum[HID_CTL_INTERFACE_NUM])
	{
		//OTG_DBG("HID_CTL_INTERFACE_NUM\n");
		if(Setup[1] == 0x01)//get report
		{
			if(Setup[3] == 0x01)//get report
			{
				Setup[0] = 0;
				OTG_DeviceControlSend((uint8_t*)Setup, 1,3);
			}
		}
	}
	else if(Setup[4] == InterfaceNum[CDC_CTL_INTERFACE_NUM] || Setup[4] == InterfaceNum[CDC_DATA_INTERFACE_NUM])
	{
		// CDC鎺ュ彛璇锋眰澶勭悊
		DBG("CDC_INTERFACE Request\n");
		OTG_DeviceCDC_Request();
	}
	else if(Setup[4] == InterfaceNum[HID_DATA_INTERFACE_NUM])
	{
		//uint32_t len=0;
		//OTG_DBG("HID_DATA_INTERFACE_NUM\n");//hid_send_data();
		if((Setup[3] == 0x02)&&(Setup[0] == 0x21))//out
		{
			hid_recive_data();
		}
		else if((Setup[3] == 0x01)&&(Setup[0] == 0xA1))//int
		{
			hid_send_data();
		}
#ifdef CFG_APP_CONFIG
		else if((Setup[3] == 0x03)&&(Setup[0] == 0xA1))//GetReport (Feature Report)
		{
			DBG("pc_upgrade start 1\n");
			if(pc_upgrade)
			{
				DBG("pc_upgrade start 2\n");
				Setup[0] = 0x55;
				OTG_DeviceControlSend(Setup,Setup[7]*256+Setup[6],1);
				#if FLASH_BOOT_EN
				start_up_grate(AppResourceUsbDevice);
				#endif
			}
			else
			{
				Setup[0] = 0;
				OTG_DeviceControlSend(Setup,Setup[7]*256+Setup[6],1);
			}
		}
		else if((Setup[3] == 0x03)&&(Setup[0] == 0x21))//SetReport (Feature Report)
		{
			pc_upgrade = 0;
			DBG("pc_upgrade start 0\n");
			if(Request[0] == 0x55)//锟斤拷CODE锟斤拷锟斤拷
			{
				//uint8_t *p = (uint8_t *)(0x10000 + 0xB8);
				//p = Request+1;// bkd // 2019.5.7
				if(memcmp((uint8_t*)(0x10000 + 0xB8),Request + 1,4) != 0)//锟斤拷锟斤拷要锟斤拷锟斤拷锟斤拷
				{
					pc_upgrade = 1;
				}
			}
			else if(Request[0] == 0xAA)//锟斤拷code锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
			{
				pc_upgrade = 1;
			}
		}
#endif
		else
		{
           // OTG_DBG("Others Cmd\n");
		}		
	}
	
#ifdef CFG_APP_USB_AUDIO_MODE_EN
	else
	{
		OTG_DeviceAudioRequest();
	}
#endif

}


//锟斤拷锟斤拷锟皆讹拷锟斤拷锟斤拷锟捷达拷锟斤拷锟斤拷
void OTG_DeviceManufacturerRequest()
{
	//锟斤拷锟斤拷锟皆讹拷锟斤拷锟斤拷锟捷达拷锟斤拷锟斤拷
}


//未知锟斤拷锟斤拷
void OTG_DeviceOtherRequest()
{
	//OTG_DBG("UsbDeviceSendStall\n");
	OTG_DeviceStallSend(DEVICE_CONTROL_EP);
}

//__attribute__((weak))// bkd // 2019.5.7
//void start_up_grate(uint32_t UpdateResource)
//{
//}

/**
 * @brief  锟斤拷锟斤拷PC锟斤拷锟斤拷锟侥匡拷锟斤拷锟斤拷锟斤拷
 * @param  NONE
 * @return NONE
 */
void OTG_DeviceRequestProcess(void)
{
	//DBG("is run");
	uint8_t BusEvent = OTG_DeviceBusEventGet();
	uint32_t DataLeng;
	uint8_t ReqType;

	if(BusEvent & 0x04)
	{
		OTG_DeviceAddressSet(0);
#ifdef CFG_APP_USB_AUDIO_MODE_EN
		UsbAudioMic.InitOk = 0;
		UsbAudioSpeaker.InitOk = 0;
#endif
		// CDC鍘诲垵濮嬪寲
		OTG_DeviceCDC_DeInit();
	}
	if(OTG_DeviceSetupReceive(Setup, 8, &DataLeng) != DEVICE_NONE_ERR)
	{
//		for(i=0;i<8;i++)
//		DBG("Setup %d is锟斤拷%d\n",i,Setup[i]);
		return;
	}
	//IsAndroid();
	//锟叫断凤拷锟斤拷
	//锟斤拷锟斤拷锟給ut 锟斤拷要锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟�然锟斤拷锟斤拷锟斤拷锟斤拷
	//锟斤拷锟斤拷锟絠n锟斤拷锟斤拷要锟斤拷准锟斤拷锟斤拷锟捷ｏ拷然锟斤拷锟斤拷锟斤拷锟斤拷
	if((Setup[0]&0x80) == 0)//out
	{
		//if(!((Setup[3] == 0x02)&&(Setup[0] == 0x21)&&(Setup[1] == 0x09)))//audio effect out
		{
			uint32_t temp=0;
			temp = Setup[7]*256 + Setup[6];
			if(temp)
			{
				int i;
				for(i=0;i<temp/64;i++)
				{
					OTG_DeviceControlReceive(Request+i*64,64,&DataLeng,10);
				}
				if(temp%64)
				{
					OTG_DeviceControlReceive(Request+i*64,temp%64,&DataLeng,10);
				}
			}
		}
	}

	ReqType = (Setup[0]&0x60)>>5;
	//DBG("ReqType:%d\n",ReqType);
	switch(ReqType)
	{
		case 0:
			//锟斤拷准锟斤拷锟斤拷
			//DBG("is run");
			OTG_DeviceStandardRequest();
			break;

		case 1:
			//锟斤拷锟斤拷锟斤拷
			OTG_DeviceClassRequest();
			break;

		case 2:
			//锟斤拷锟斤拷锟斤拷锟斤拷
			OTG_DeviceManufacturerRequest();
			break;

		case 3:
			//锟斤拷锟斤拷锟斤拷锟斤拷
			OTG_DeviceOtherRequest();
			break;			
	}
}

//*************************************************//
//*************************************************//
//*************************************************//



void hid_recive_data(void)
{
#ifdef CFG_COMMUNICATION_BY_USB
	HIDUsb_Rx(Request,256);
#endif
}


void hid_send_data(void)
{
	OTG_DeviceControlSend(hid_tx_buf,256,6);
}

void IsAndroid(void)
{
	/////锟叫讹拷锟角凤拷Android锟街伙拷 "A1 01 00 01 03 00 01 00"
	if( (Setup[0]==0xA1) && (Setup[1]==0x01) )//
	{
		if( (Setup[2]==0x00) && (Setup[3]==0x01) )
		{
			if( (Setup[4]==0x03) && (Setup[5]==0x00) )
			{
				if( (Setup[6]==0x01) && (Setup[7]==0x00) )
				{
					//gCtrlVars.usb_android = 1;
				}
			}
		}
	}
}
