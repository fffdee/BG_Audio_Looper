
#ifndef __BT_PBAP_API_H_
#define __BT_PBAP_API_H_

/******************************************************************************************************************************************************
 * PBAP使用说明
 * 1. PBAP_MTU_SIZE能设置每次手机端发送1包的数据长度; 设置范围(1-4096)
 * 2. 在1包数据发送完成后,后续还有数据,则会收到 event： BT_STACK_EVENT_PBAP_CONTINUE_FLAG; 默认在该事件中立即发送继续接收数据 PBAP_PullPhoneBook_Continue
 * 3. 如客户需要将接收到的数据保存到flash,可以将PBAP_PullPhoneBook_Continue调用延迟进行; 可以根据实际应用延迟发送
 * 注意事项：
 * 1. 接收到的PBAP数据,不能直接在callback中直接进行处理,如解码等操作,避免阻塞导致蓝牙运行异常;
 * 2. 建议是缓存在临时buffer中,在其他任务中进行处理;
 ******************************************************************************************************************************************************/




#define PBAP_MTU_SIZE		3*1024	//3K

/**
 * PBAP phone book : select patch
 */
enum {
	PHONE = 0,
	SIM1,
	SIM2,
};

/**
 * PBAP releate event
 */
typedef enum{
	BT_STACK_EVENT_PBAP_NONE = 0,
	BT_STACK_EVENT_PBAP_DISCONNECTING,
	BT_STACK_EVENT_PBAP_DISCONNECT,
	BT_STACK_EVENT_PBAP_CONNECT_ERROR,
	BT_STACK_EVENT_PBAP_CONNECTING,
	BT_STACK_EVENT_PBAP_CONNECTED,
	BT_STACK_EVENT_PBAP_DATA_START,//开始接收长数据包
	BT_STACK_EVENT_PBAP_DATA,//长数据包
	BT_STACK_EVENT_PBAP_DATA_END,//结束包
	BT_STACK_EVENT_PBAP_DATA_END1,
	BT_STACK_EVENT_PBAP_NOT_ACCEPTABLE,
	BT_STACK_EVENT_PBAP_NOT_FOUND,
	BT_STACK_EVENT_PBAP_DATA_SINGLE,//单包完整的数据
	BT_STACK_EVENT_PBAP_PACKET_END,//数据接收完成
	BT_STACK_EVENT_PBAP_CONTINUE_FLAG,//继续接收数据标志
}BT_PBAP_CALLBACK_EVENT;

/*
enum {
	PBAP_CONNECT_START = 0,
	PBAP_CONNECT_OK,
	PBAP_CONNECT_ERROR,
	PBAP_CLOSE_OK,
	PBAP_CLOSE_START,
	PBAP_DATA_START,//开始接收长数据包
	PBAP_DATA,//长数据包
	PBAP_DATA_END,//结束包
	PBAP_DATA_END1,
	PBAP_NOT_ACCEPTABLE,
	PBAP_NOT_FOUND,
	PBAP_DATA_SINGLE,//单包完整的数据
	PBAP_PACKET_END,//数据接收完成
};
*/
typedef struct _BT_PBAP_CALLBACK_PARAMS {
    uint32_t	length;		// data length
    uint8_t		*buffer;	// pointer to data buffer
}BT_PBAP_CALLBACK_PARAMS;


typedef void (*BTPbapCallbackFunc)(BT_PBAP_CALLBACK_EVENT event, BT_PBAP_CALLBACK_PARAMS * param);

void BtPbapCallback(BT_PBAP_CALLBACK_EVENT event, BT_PBAP_CALLBACK_PARAMS * param);

/**
 * @brief
 *  	pbap connect
 *
 * @param 
 *		addr - the remote address
 *
 * @return
 *		1 = connect success
 *		0 = fail
 *
 * @note
 *		This function must be called after BTStackRunInit and before BTStackRun
 */
bool PBAPConnect(uint8_t* addr);


/**
 * @brief
 *  	pbap disconnect
 *
 * @param 
 *		NONE
 *
 * @return
 *		1 = command send success
 *		0 = fail
 *
 * @note
 *		NONE
 */
bool PBAPDisconnect(void);

/**
 * @brief
 *  	pbap pull phone book
 *
 * @param 
 *		sel: patch(phone,sim1,sim2)
 *		buf: pointer to type info 
 *
 * @return
 *		NONE
 *
 * @note
 *		NONE
 */
void PBAP_PullPhoneBook(uint8_t Sel,uint8_t *buf);
void PBAP_PullPhoneBook_Continue(void);

int8_t PBAP_MtuSizeSet(uint16_t size);

bool PbapAppInit(BTPbapCallbackFunc callback);

//获取卡1电话簿信息 
void GetSim1CardPhoneBook(void);

//获取卡2电话簿信息 
void GetSim2CardPhoneBook(void);

//获取手机自身电话簿信息 
void GetMobilePhoneBook(void);

//获取呼入电话信息 
void GetIncomingCallBook(void);

//获取呼出电话簿信息 
void GetOutgoingCallBook(void);

//获取未接电话簿信息 
void GetMissedCallBook(void);

void GetCombinedCallBook(void);

#endif /* __BT_PBAP_API_H_ */ 

