#ifndef _BLE_PROCESS_H__
#define _BLE_PROCESS_H__

#include "stdint.h"
#define HEADER_HIGH 0xAB
#define HEADER_LOW  0xBA
#define TAIL_WORD   0xCD

#define SUCCESS 0
#define ERR     0xff

#define MIN_DATA_LEN 6


typedef enum{

	HARDWARE_EVENT= 0x00,
	//LOOPER_EVENT,
	EFFECT_EVENT,
	BLE_EVENT_MAX,


}CTRL_EVENT;

typedef enum{

	DAC_VOLUME=0,
	MIC_VOLUME,
	LINE_IN_VOLUME,
	PLAY_SELECT,
	DEBUG_SETTING,
	HARDWARE_CMD_MAX,

}HARDWARE_CMD;



typedef enum{

	REVERB_STD,
	REVERB_PLATE,
	DRC,
	EFFECT_CMD_MAX,
}EFFECT_CMD;



typedef enum{

	HEADER_H= 0x00,
	HEADER_L,
	CTRL_TYPE,
	CTRL_WORD,
	DATA_LEN,

}DATA_CMD_ID;

typedef struct{
	uint8_t header_high;
	uint8_t header_low;
	uint8_t ctrl_type;
	uint8_t ctrl_word;
	uint8_t data_len;
	uint8_t *data;
	uint8_t tail_word;
}Ble_Pcaket;

typedef struct{

	uint8_t cmd;
	const char *name;
	const uint8_t arg_len;

}ctrl_info_t;
typedef struct{
	uint8_t ctrl_id;
	uint8_t (*callback)(uint8_t word, uint8_t *data,uint8_t len);
	ctrl_info_t *ctrl_info;
}CTRL_Handle;



typedef struct{

	uint8_t debug_is_en;
	uint8_t play_id;
	uint16_t dac_volume;
	uint16_t mic_volume;
	uint16_t line_in_volume;


}ble_info_t;




int8_t prase_ble_packet(uint8_t* data, uint8_t data_len);


#endif
