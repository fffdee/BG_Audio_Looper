/*****************************************************
 mmmmmm                           mmmm     mmmm
 ##""""##                       ##""""#   ##""##
 ##    ##   m#####m  ##m####m  ##        ##    ##
 #######    " mmm##  ##"   ##  ##  mmmm  ##    ##
 ##    ##  m##"""##  ##    ##  ##  ""##  ##    ##
 ##mmmm##  ##mmm###  ##    ##   ##mmm##   ##mm##
 """""""    """" ""  ""    ""     """"     """"
******************************************************
  funstion: Model for Encoder
  author  : BanGO
******************************************************/

#include "bg_encoder.h"
#include "gpio.h"
#include "debug.h"


typedef struct{

	uint8_t last_A;
	uint8_t A;
	uint8_t B;
	uint8_t mode;
	uint16_t min_value;
	uint16_t max_value;
	uint16_t encoder_position;

}Task_run;

Task_run task_run = {

	.mode = OVER_MODE, //LIMIT_MODE,
	.encoder_position = 50,
	.min_value = 0,
	.max_value = 100,
};

void encoder_button_init(void);
uint8_t encoder_buttun_state(void);
uint8_t enter_button_state(void);
B_ERRCODE set_encoder_value(uint16_t value);
B_ERRCODE set_encoder_range(uint16_t min_value, uint16_t max_value,uint8_t mode);
uint16_t get_encoder_value(void);
uint16_t* get_encoder_range(void);
uint8_t get_encoder_mode(void);

BG_Encoder BG_encoder = {



	.button_init = encoder_button_init,					//硬件初始化

	.enter = enter_button_state,						//获取旋钮按键的状态值

	.encoder = encoder_buttun_state,					//获取旋转的值

	.set_range = set_encoder_range,						//设置旋钮的范围

	.set_value = set_encoder_value,						//设置旋钮的初始值

	.get_range = get_encoder_range,						//获取当前旋钮的范围值

	.get_value = get_encoder_value,						//获取旋钮的当前值

	.get_mode = get_encoder_mode,						//获取当前模式

};


void encoder_button_init(void)
{

		//enter 按键初始化，上拉输入。
		GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX0);
		GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX0);
		GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX0);
		GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX0);

		//脉冲A输入 按键初始化，上拉输入
		GPIO_RegOneBitSet(GPIO_B_IE, GPIO_INDEX4);
		GPIO_RegOneBitClear(GPIO_B_OE, GPIO_INDEX4);
		GPIO_RegOneBitSet(GPIO_B_PU, GPIO_INDEX4);
		GPIO_RegOneBitClear(GPIO_B_PD, GPIO_INDEX4);

		//脉冲B输入 按键初始化，上拉输入
		GPIO_RegOneBitSet(GPIO_B_IE, GPIO_INDEX5);
		GPIO_RegOneBitClear(GPIO_B_OE, GPIO_INDEX5);
		GPIO_RegOneBitSet(GPIO_B_PU, GPIO_INDEX5);
		GPIO_RegOneBitClear(GPIO_B_PD, GPIO_INDEX5);


}


B_ERRCODE set_encoder_range(uint16_t min_value, uint16_t max_value,uint8_t mode)
{

	if(min_value<ENCODER_MIN||max_value>ENCODER_MAX){

		#ifdef BUTTON_DBG
			DBG("min or max over range\n");
		#endif
		return SET_RANGE_OVER_RANGE;
	}
	if(mode>1){

		#ifdef BUTTON_DBG
		DBG("set mode invalid\n");
		#endif
		return SET_MODE_ERR;
	}
	if(min_value>max_value){

		#ifdef BUTTON_DBG
		DBG("set min or max invalid\n");
		#endif
		return MIN_OR_MAX_INVALID;
	}

	task_run.max_value = max_value;
	task_run.min_value = min_value;
	task_run.mode = mode;


	return SUCCESS;
}

B_ERRCODE set_encoder_value(uint16_t value)
{
	if(value<ENCODER_MIN||value>ENCODER_MAX){

		#ifdef BUTTON_DBG
		DBG("set min or max invalid\n");
		#endif

		return SET_VALUE_OVER_RANGE;
	}


	task_run.encoder_position = value;


	return SUCCESS;
}

uint8_t get_encoder_mode(void)
{
	return task_run.mode;
}

uint16_t get_encoder_value(void)
{
	return task_run.encoder_position;
}

uint16_t* get_encoder_range(void)
{

	static uint16_t encoder_range[2];

	encoder_range[0] = task_run.min_value;
	encoder_range[1] = task_run.max_value;

	return encoder_range;
}

//
uint8_t encoder_buttun_state(void)
{
	task_run.A = GPIO_RegOneBitGet(GPIO_B_IN, GPIO_INDEX4); 		//读取编码器脉冲的值
	task_run.B = GPIO_RegOneBitGet(GPIO_B_IN, GPIO_INDEX5);

	  // 如果A的状态改变了，那么检测B的状态以确定旋转方向
	    if (task_run.A != task_run.last_A) {
	        if (task_run.B != task_run.A) {
	            // 顺时针旋转
	        	if(task_run.mode==OVER_MODE||
	        	  (task_run.mode==LIMIT_MODE&&
	        	   task_run.encoder_position<task_run.max_value))
	        	   task_run.encoder_position++;
	        } else {
	            // 逆时针旋转
	        	if(task_run.mode==OVER_MODE||
	        	  (task_run.mode==LIMIT_MODE&&
	        	   task_run.encoder_position>task_run.min_value)){
	        		task_run.encoder_position--;
	        	}
	        }
	        task_run.last_A = task_run.A; // 更新状态
	    }

	    if(task_run.mode==OVER_MODE){

			if(task_run.encoder_position>task_run.max_value){
					task_run.encoder_position = task_run.min_value+1;
					task_run.encoder_position-=1;
			}
			else if (task_run.encoder_position<task_run.min_value)
					task_run.encoder_position = task_run.max_value;
	    }

		#ifdef BUTTON_DBG
			DBG("Ecoder Get %d\n",task_run.encoder_position);
		#endif

		return task_run.encoder_position;

}

uint8_t enter_button_state(void)
{

	uint8_t newVal = GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX0);

	#ifdef BUTTON_DBG
		if(newVal == 0)
		{
			DBG("Enter Get \n");
		}

	#endif

	return newVal;

}
