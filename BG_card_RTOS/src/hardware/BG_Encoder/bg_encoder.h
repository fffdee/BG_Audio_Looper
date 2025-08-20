#ifndef __BG_ENCOEDR_H__
#define __BG_ENCOEDR_H__

#include <stdint.h>

//#define BUTTON_DBG

#define ENCODER_MAX				10000
#define ENCODER_MIN				0

#define OVER_MODE				0      //溢出模式
#define LIMIT_MODE				1      //限制模式

typedef enum{

	SUCCESS = 0,
	SET_RANGE_OVER_RANGE,
	SET_VALUE_OVER_RANGE,
	SET_MODE_ERR,
	MIN_OR_MAX_INVALID,

}B_ERRCODE;


typedef struct {

	void (*button_init)(void);
	uint8_t (*enter)(void);
	uint8_t (*encoder)(void);
	B_ERRCODE (*set_value)(uint16_t);
	B_ERRCODE (*set_range)(uint16_t,uint16_t,uint8_t);
	uint16_t (*get_value)(void);
	uint16_t* (*get_range)(void);
	uint8_t (*get_mode)(void);

}BG_Encoder;

extern BG_Encoder BG_encoder;


#endif
