#include "ble_process.h"
#include "debug.h"


#define SUCCESS 0
#define ERR     0xff


uint8_t hardware_callback(uint8_t cmd, uint8_t *data, uint8_t data_len);
uint8_t effect_callback(uint8_t cmd, uint8_t *data, uint8_t data_len);


ctrl_info_t hardware_info[HARDWARE_CMD_MAX] = {
		{DAC_VOLUME,"DAC VOLUME",2},
		{MIC_VOLUME,"MIC VOLUME",2},
		{LINE_IN_VOLUME,"LINE IN VOLUME",2},
		{PLAY_SELECT,"PLAY SELECT",2},
		{DEBUG_SETTING,"DEBUG SETTING",2},
};

ctrl_info_t effect_info[EFFECT_CMD_MAX] = {

		{REVERB_STD, "standard reverb",4},
		{REVERB_PLATE,"plate reverb", 4},
		{DRC,"DRC", 4},


};

static CTRL_Handle evt_table_g [BLE_EVENT_MAX]={

		{HARDWARE_EVENT,hardware_callback, hardware_info},

		{EFFECT_EVENT,effect_callback,effect_info},

		//{LOOPER_EVENT,looper_callback},

};

ble_info_t ble_info_g = {

		.dac_volume = 0,
		.mic_volume = 0,
		.line_in_volume = 0,
		.debug_is_en = 1,
		.play_id = 0,
};



void get_std_ble_packet(Ble_Pcaket *packet)
{
	packet->header_high = HEADER_HIGH;
	packet->header_low = HEADER_LOW;
	packet->tail_word = TAIL_WORD;
}


uint8_t  hardware_callback(uint8_t cmd, uint8_t *data, uint8_t data_len)
{
	uint8_t len = data[DATA_LEN];
	switch(cmd){

		case DAC_VOLUME:

			if( ble_info_g.debug_is_en)
			BT_DBG("[DAC_VOLUME]Set parameters\n");

			return SUCCESS;

		case MIC_VOLUME:
			if( ble_info_g.debug_is_en)
			BT_DBG("[MIC_VOLUME]Set parameters\n");
			return SUCCESS;

		case LINE_IN_VOLUME:
			if( ble_info_g.debug_is_en)
			BT_DBG("[LINE_IN_VOLUME]Set parameters\n");
			return SUCCESS;

		case PLAY_SELECT:
			if( ble_info_g.debug_is_en)
			BT_DBG("[PLAY_SELECT]Set parameters\n");
			return SUCCESS;

		case DEBUG_SETTING:
			if( ble_info_g.debug_is_en)
			BT_DBG("[DEBUG_SETTING]Set parameters\n");
			return SUCCESS;

		default:
			if( ble_info_g.debug_is_en)
			BT_DBG("[BT_PRO]No corresponding command\n");
			return ERR;

	}
	return SUCCESS;
}


uint8_t effect_callback(uint8_t cmd, uint8_t *data, uint8_t data_len)
{
	uint8_t len = data[DATA_LEN];
	switch(cmd){

		case REVERB_STD:
			if( ble_info_g.debug_is_en)
			BT_DBG("[REVERB_STD]Set parameters\n");
			return SUCCESS;
		case REVERB_PLATE:

			if( ble_info_g.debug_is_en)
			BT_DBG("[REVERB_PLATE]Set parameters\n");
			return SUCCESS;

		default:
			if( ble_info_g.debug_is_en)
			BT_DBG("[BT_PRO]No corresponding command\n");
			return ERR;


	}
	return SUCCESS;
}




int8_t prase_ble_packet(uint8_t* data, uint8_t data_len){

	if(data_len<MIN_DATA_LEN){
		if( ble_info_g.debug_is_en)
		BT_DBG("[BT_PRO]Data too short\n");
		return ERR;
	}
	uint8_t i;

	if(data[HEADER_H]==HEADER_HIGH && data[HEADER_L]==HEADER_LOW && data[data_len-1]==TAIL_WORD){

			for(i = 0; i<BLE_EVENT_MAX; i++)
			{
				if(data[CTRL_TYPE]==evt_table_g[i].ctrl_id){
					return evt_table_g[i].callback(data[CTRL_WORD],data,data_len);
				}
			}

	}else{

		if( ble_info_g.debug_is_en)
		BT_DBG("[BT_PRO]Incorrect header format\n");
		return ERR;
	}

}


void set_ble_packet(Ble_Pcaket *packet,uint8_t ctrl_type,uint8_t data_len,uint8_t* data){

}
