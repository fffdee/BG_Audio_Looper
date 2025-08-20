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



	.button_init = encoder_button_init,					//Ӳ����ʼ��

	.enter = enter_button_state,						//��ȡ��ť������״ֵ̬

	.encoder = encoder_buttun_state,					//��ȡ��ת��ֵ

	.set_range = set_encoder_range,						//������ť�ķ�Χ

	.set_value = set_encoder_value,						//������ť�ĳ�ʼֵ

	.get_range = get_encoder_range,						//��ȡ��ǰ��ť�ķ�Χֵ

	.get_value = get_encoder_value,						//��ȡ��ť�ĵ�ǰֵ

	.get_mode = get_encoder_mode,						//��ȡ��ǰģʽ

};


void encoder_button_init(void)
{

		//enter ������ʼ�����������롣
		GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX0);
		GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX0);
		GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX0);
		GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX0);

		//����A���� ������ʼ������������
		GPIO_RegOneBitSet(GPIO_B_IE, GPIO_INDEX4);
		GPIO_RegOneBitClear(GPIO_B_OE, GPIO_INDEX4);
		GPIO_RegOneBitSet(GPIO_B_PU, GPIO_INDEX4);
		GPIO_RegOneBitClear(GPIO_B_PD, GPIO_INDEX4);

		//����B���� ������ʼ������������
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
	task_run.A = GPIO_RegOneBitGet(GPIO_B_IN, GPIO_INDEX4); 		//��ȡ�����������ֵ
	task_run.B = GPIO_RegOneBitGet(GPIO_B_IN, GPIO_INDEX5);

	  // ���A��״̬�ı��ˣ���ô���B��״̬��ȷ����ת����
	    if (task_run.A != task_run.last_A) {
	        if (task_run.B != task_run.A) {
	            // ˳ʱ����ת
	        	if(task_run.mode==OVER_MODE||
	        	  (task_run.mode==LIMIT_MODE&&
	        	   task_run.encoder_position<task_run.max_value))
	        	   task_run.encoder_position++;
	        } else {
	            // ��ʱ����ת
	        	if(task_run.mode==OVER_MODE||
	        	  (task_run.mode==LIMIT_MODE&&
	        	   task_run.encoder_position>task_run.min_value)){
	        		task_run.encoder_position--;
	        	}
	        }
	        task_run.last_A = task_run.A; // ����״̬
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
