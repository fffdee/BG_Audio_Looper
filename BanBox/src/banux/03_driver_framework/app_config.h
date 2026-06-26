
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include "type.h"

//******************************************************************************
//                         FLASH BOOT升级功能配置
//******************************************************************************
// 使能Flash Boot升级功能（USB/SD卡/U盘升级）
// 1: 使能Flash Boot升级功能
// 0: 禁用Flash Boot升级功能
#define FLASH_BOOT_EN				1

//******************************************************************************
//                         Bootloader配置
//******************************************************************************
// 是否使用Bootloader启动
// 1: APP由Bootloader跳转启动，跳过Chip_Init等bootloader已完成的初始化
// 0: APP独立运行（直接从0x0启动），需要完整初始化
#define HAS_BOOTLOADER				1

typedef uint16_t (*AudioCoreDataGetFunc)(void* Buf, uint16_t Samples);
typedef uint16_t (*AudioCoreDataLenGetFunc)(void);
typedef uint16_t (*AudioCoreDataSetFunc)(void* Buf, uint16_t Samples);
typedef uint16_t (*AudioCoreDataSpaceLenSetFunc)(void);

typedef void (*AudioCoreProcessFunc)(void);

#define AUDIO_CORE_SOURCE_MAX_MUN	4

enum
{
	AUDIO_DAC0_SINK_NUM,
	#ifdef CFG_FUNC_RECORDER_EN
	AUDIO_RECORDER_SINK_NUM,
	#endif
	#if (defined(CFG_APP_BT_MODE_EN) && (BT_HFP_SUPPORT == ENABLE))
	AUDIO_HF_SCO_SINK_NUM,
	#endif
	#ifdef CFG_RES_AUDIO_DACX_EN
	AUDIO_DACX_SINK_NUM,
	#endif
	#ifdef CFG_RES_AUDIO_I2SOUT_EN
	AUDIO_I2SOUT_SINK_NUM,
	#endif
	#ifdef CFG_APP_USB_AUDIO_MODE_EN
	USB_AUDIO_SINK_NUM,
	#endif
	AUDIO_CORE_SINK_MAX_NUM,

};

typedef struct _AudioCoreSource
{
	uint8_t						Index;
	uint8_t						PcmFormat;
	bool						Enable;
	bool						IsSreamData;
	AudioCoreDataGetFunc		FuncDataGet;
	AudioCoreDataLenGetFunc		FuncDataGetLen;
	int16_t						*PcmInBuf;
	int16_t						PreGain;
	int16_t						LeftVol;
	int16_t						RightVol;
	int16_t						LeftCurVol;
	int16_t						RightCurVol;
	bool						LeftMuteFlag;
	bool						RightMuteFlag;
}AudioCoreSource;


typedef struct _AudioCoreSink
{
	uint8_t							Index;
	uint8_t							PcmFormat;
	bool							Enable;
	uint8_t							SreamDataState;
	AudioCoreDataSetFunc			FuncDataSet;
	AudioCoreDataSpaceLenSetFunc	FuncDataSpaceLenGet;
	int16_t							*PcmOutBuf;
	int16_t							LeftVol;
	int16_t							RightVol;
	int16_t							LeftCurVol;
	int16_t							RightCurVol;
	bool							LeftMuteFlag;
	bool							RightMuteFlag;

}AudioCoreSink;

typedef struct _AudioCoreContext
{
	AudioCoreSource AudioSource[AUDIO_CORE_SOURCE_MAX_MUN];
	AudioCoreProcessFunc AudioEffectProcess;
	AudioCoreSink   AudioSink[AUDIO_CORE_SINK_MAX_NUM];

}AudioCoreContext;
#define	CFG_FUNC_MIC_KARAOKE_EN
#define CFG_FUNC_AUDIO_EFFECT_EN

#ifdef CFG_FUNC_AUDIO_EFFECT_EN

    //#define CFG_FUNC_ECHO_DENOISE
 	//#define CFG_FUNC_MUSIC_EQ_MODE_EN
	#ifdef CFG_FUNC_MUSIC_EQ_MODE_EN
 		#define CFG_FUNC_EQMODE_FADIN_FADOUT_EN
    #endif
	#define CFG_FUNC_MUSIC_TREB_BASS_EN
    //#define CFG_FUNC_SILENCE_AUTO_POWER_OFF_EN
    #ifdef CFG_FUNC_SILENCE_AUTO_POWER_OFF_EN
		#define  SILENCE_THRESHOLD                 120
		#define  SILENCE_POWER_OFF_DELAY_TIME      60*1000
    #endif

	#if CFG_RES_MIC_SELECT
	#define	CFG_FUNC_MIC_KARAOKE_EN
	#endif

	#ifdef CFG_FUNC_MIC_KARAOKE_EN
		//#define CFG_FUNC_GUITAR_EN
		//#define CFG_FUNC_DETECT_MIC_EN
        //#define CFG_FUNC_MIC_TREB_BASS_EN
		#define  CFG_FUNC_SHUNNING_EN
			#define SHNNIN_VALID_DATA                          	 500
			#define SHNNIN_STEP                                  256
			#define SHNNIN_THRESHOLD                             SHNNIN_STEP*2
			#define SHNNIN_VOL_RECOVER_TIME                      50
			#define SHNNIN_UP_DLY                                3
			#define SHNNIN_DOWN_DLY                              1
	#endif

	#define  CFG_EFFECT_MAJOR_VERSION						1
	#define  CFG_EFFECT_MINOR_VERSION						2
	#define  CFG_EFFECT_USER_VERSION						3
	#define  CFG_COMMUNICATION_BY_USB
	//#define  CFG_COMMUNICATION_BY_UART

	#define	 CFG_UART_COMMUNICATION_TX_PIN					GPIOA10
	#define  CFG_UART_COMMUNICATION_TX_PIN_MUX_SEL			(3)
	#define  CFG_UART_COMMUNICATION_RX_PIN					GPIOA9
	#define  CFG_UART_COMMUNICATION_RX_PIN_MUX_SEL			(1)

	#define  CFG_COMMUNICATION_CRYPTO						(0)
	#define  CFG_COMMUNICATION_PASSWORD                     0x11223344
#endif


#endif
