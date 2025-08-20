
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include "type.h"

typedef uint16_t (*AudioCoreDataGetFunc)(void* Buf, uint16_t Samples);
typedef uint16_t (*AudioCoreDataLenGetFunc)(void);
typedef uint16_t (*AudioCoreDataSetFunc)(void* Buf, uint16_t Samples);
typedef uint16_t (*AudioCoreDataSpaceLenSetFunc)(void);

typedef void (*AudioCoreProcessFunc)(void*);//音效处理主函数

#define AUDIO_CORE_SOURCE_MAX_MUN	4

enum
{
	AUDIO_DAC0_SINK_NUM,		//主音频输出在audiocore Sink中的通道，必须配置，audiocore借用此通道buf处理数据
	#ifdef CFG_FUNC_RECORDER_EN
	AUDIO_RECORDER_SINK_NUM,	//录音专用通道		 不叠加提示音音源。
	#endif
	#if (defined(CFG_APP_BT_MODE_EN) && (BT_HFP_SUPPORT == ENABLE))
	AUDIO_HF_SCO_SINK_NUM,	    //蓝牙sco发送数据通道
	#endif
	#ifdef CFG_RES_AUDIO_DACX_EN
	AUDIO_DACX_SINK_NUM,		//dacx通道
	#endif
	#ifdef CFG_RES_AUDIO_I2SOUT_EN
	AUDIO_I2SOUT_SINK_NUM,      //i2s_out通道
	#endif
	#ifdef CFG_APP_USB_AUDIO_MODE_EN
	USB_AUDIO_SINK_NUM,         //usb_out通道
	#endif
	AUDIO_CORE_SINK_MAX_NUM,

};

typedef struct _AudioCoreSource
{
	uint8_t						Index;
	uint8_t						PcmFormat;	//数据格式,sign or stereo
	bool						Enable; 	//
	bool						IsSreamData;//是否流数据，用于决定该通路数据是否要满一帧
	AudioCoreDataGetFunc		FuncDataGet;//****流数据入口函数
	AudioCoreDataLenGetFunc		FuncDataGetLen;
	int16_t						*PcmInBuf;	//缓存数据buf
	int16_t						PreGain;
	int16_t						LeftVol;	//音量
	int16_t						RightVol;	//音量
	int16_t						LeftCurVol;	//当前音量
	int16_t						RightCurVol;//当前音量
	bool						LeftMuteFlag;//静音标志
	bool						RightMuteFlag;//静音标志
}AudioCoreSource;


typedef struct _AudioCoreSink
{
	uint8_t							Index;
	uint8_t							PcmFormat;	//数据格式，1:单声道输出; 2:立体声输出
	bool							Enable;
	uint8_t							SreamDataState;//0:流数据;1：检测sinkbuf是否足够；2：
	AudioCoreDataSetFunc			FuncDataSet;//****推流入口 内buf->外缓冲搬移函数
	AudioCoreDataSpaceLenSetFunc	FuncDataSpaceLenGet;
	int16_t							*PcmOutBuf;
	int16_t							LeftVol;	//音量
	int16_t							RightVol;	//音量
	int16_t							LeftCurVol;	//当前音量
	int16_t							RightCurVol;//当前音量
	bool							LeftMuteFlag;//静音标志
	bool							RightMuteFlag;//静音标志

}AudioCoreSink;

typedef struct _AudioCoreContext
{
	AudioCoreSource AudioSource[AUDIO_CORE_SOURCE_MAX_MUN];
	AudioCoreProcessFunc AudioEffectProcess;			//****流处理入口
	AudioCoreSink   AudioSink[AUDIO_CORE_SINK_MAX_NUM];

}AudioCoreContext;
#define	CFG_FUNC_MIC_KARAOKE_EN
#define CFG_FUNC_AUDIO_EFFECT_EN //总音效使能开关

#ifdef CFG_FUNC_AUDIO_EFFECT_EN

    //#define CFG_FUNC_ECHO_DENOISE          //消除快速调节delay时的杂音，
 	//#define CFG_FUNC_MUSIC_EQ_MODE_EN     //Music EQ模式功能配置               
	#ifdef CFG_FUNC_MUSIC_EQ_MODE_EN	    
 		#define CFG_FUNC_EQMODE_FADIN_FADOUT_EN    //EQ模式切换时fade in/fade out处理功能选择,调节EQ模式中有POP声时，建议打开 		
    #endif
	#define CFG_FUNC_MUSIC_TREB_BASS_EN    		//Music高低音调节功能配置
    //#define CFG_FUNC_SILENCE_AUTO_POWER_OFF_EN     //无信号自动关机功能，
    #ifdef CFG_FUNC_SILENCE_AUTO_POWER_OFF_EN      
		#define  SILENCE_THRESHOLD                 120        //设置信号检测门限，小于这个值认为无信号
		#define  SILENCE_POWER_OFF_DELAY_TIME      60*1000     //无信号关机延时时间，单位：ms
    #endif

	#if CFG_RES_MIC_SELECT
	#define	CFG_FUNC_MIC_KARAOKE_EN      //MIC karaoke功能选择
	#endif

	#ifdef CFG_FUNC_MIC_KARAOKE_EN
		//#define CFG_FUNC_GUITAR_EN        //吉他音效功能选择
		//#define CFG_FUNC_DETECT_MIC_EN 	//mic插拔检测功能
        //#define CFG_FUNC_MIC_TREB_BASS_EN   //mic高低音调节功能配置
		//闪避功能选择设置
		//注:若需要完善移频开启后的啾啾干扰声问题，需要开启此功能(利用MIC信号检测接口处理)
		#define  CFG_FUNC_SHUNNING_EN                                
			#define SHNNIN_VALID_DATA                          	 500  ////MIC音量阈值
			#define SHNNIN_STEP                                  256  /////单次调节的步数
			#define SHNNIN_THRESHOLD                             SHNNIN_STEP*2  ////threshold
			#define SHNNIN_VOL_RECOVER_TIME                      50////伴奏音量恢复时长：100*20ms = 2s
			#define SHNNIN_UP_DLY                                3/////音量上升时间
			#define SHNNIN_DOWN_DLY                              1/////音量下降时间
	#endif	

	//**音频SDK版本号,不要修改**/
//	#define  CFG_EFFECT_MAJOR_VERSION						(CFG_SDK_MAJOR_VERSION)
//	#define  CFG_EFFECT_MINOR_VERSION						(CFG_SDK_MINOR_VERSION)
//	#define  CFG_EFFECT_USER_VERSION						(CFG_SDK_PATCH_VERSION)

	#define  CFG_EFFECT_MAJOR_VERSION						1
	#define  CFG_EFFECT_MINOR_VERSION						2
	#define  CFG_EFFECT_USER_VERSION						3
	/**在线调音硬件接口有USB HID接口，或者UART接口*/
	/**建议使用USB HID接口，收发buf共512Byte*/
	/**UART接口占用2路DMA，收发Buf共2k Byte*/
	/**建议使用USB HID作为调音接口，DMA资源紧张*/
	/**如果使用UART作为调音接口请设置DMA接口并保证DMA资源充足*/
	#define  CFG_COMMUNICATION_BY_USB		// usb or uart 					
	//#define  CFG_COMMUNICATION_BY_UART		// usb or uart 				
	
	#define	 CFG_UART_COMMUNICATION_TX_PIN					GPIOA10
	#define  CFG_UART_COMMUNICATION_TX_PIN_MUX_SEL			(3)
	#define  CFG_UART_COMMUNICATION_RX_PIN					GPIOA9
	#define  CFG_UART_COMMUNICATION_RX_PIN_MUX_SEL			(1)
	
	#define  CFG_COMMUNICATION_CRYPTO						(0)////调音通讯加密=1 调音通讯不加密=0
	#define  CFG_COMMUNICATION_PASSWORD                     0x11223344//////四字节的长度密码
#endif


#endif
