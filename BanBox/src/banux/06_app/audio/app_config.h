
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
#define HAS_BOOTLOADER				0

typedef uint16_t (*AudioCoreDataGetFunc)(void* Buf, uint16_t Samples);
typedef uint16_t (*AudioCoreDataLenGetFunc)(void);
typedef uint16_t (*AudioCoreDataSetFunc)(void* Buf, uint16_t Samples);
typedef uint16_t (*AudioCoreDataSpaceLenSetFunc)(void);

typedef void (*AudioCoreProcessFunc)(void*);//��Ч����������

#define AUDIO_CORE_SOURCE_MAX_MUN	4

enum
{
	AUDIO_DAC0_SINK_NUM,		//����Ƶ�����audiocore Sink�е�ͨ�����������ã�audiocore���ô�ͨ��buf��������
	#ifdef CFG_FUNC_RECORDER_EN
	AUDIO_RECORDER_SINK_NUM,	//¼��ר��ͨ��		 ��������ʾ����Դ��
	#endif
	#if (defined(CFG_APP_BT_MODE_EN) && (BT_HFP_SUPPORT == ENABLE))
	AUDIO_HF_SCO_SINK_NUM,	    //����sco��������ͨ��
	#endif
	#ifdef CFG_RES_AUDIO_DACX_EN
	AUDIO_DACX_SINK_NUM,		//dacxͨ��
	#endif
	#ifdef CFG_RES_AUDIO_I2SOUT_EN
	AUDIO_I2SOUT_SINK_NUM,      //i2s_outͨ��
	#endif
	#ifdef CFG_APP_USB_AUDIO_MODE_EN
	USB_AUDIO_SINK_NUM,         //usb_outͨ��
	#endif
	AUDIO_CORE_SINK_MAX_NUM,

};

typedef struct _AudioCoreSource
{
	uint8_t						Index;
	uint8_t						PcmFormat;	//���ݸ�ʽ,sign or stereo
	bool						Enable; 	//
	bool						IsSreamData;//�Ƿ������ݣ����ھ�����ͨ·�����Ƿ�Ҫ��һ֡
	AudioCoreDataGetFunc		FuncDataGet;//****��������ں���
	AudioCoreDataLenGetFunc		FuncDataGetLen;
	int16_t						*PcmInBuf;	//��������buf
	int16_t						PreGain;
	int16_t						LeftVol;	//����
	int16_t						RightVol;	//����
	int16_t						LeftCurVol;	//��ǰ����
	int16_t						RightCurVol;//��ǰ����
	bool						LeftMuteFlag;//������־
	bool						RightMuteFlag;//������־
}AudioCoreSource;


typedef struct _AudioCoreSink
{
	uint8_t							Index;
	uint8_t							PcmFormat;	//���ݸ�ʽ��1:���������; 2:���������
	bool							Enable;
	uint8_t							SreamDataState;//0:������;1�����sinkbuf�Ƿ��㹻��2��
	AudioCoreDataSetFunc			FuncDataSet;//****������� ��buf->�⻺����ƺ���
	AudioCoreDataSpaceLenSetFunc	FuncDataSpaceLenGet;
	int16_t							*PcmOutBuf;
	int16_t							LeftVol;	//����
	int16_t							RightVol;	//����
	int16_t							LeftCurVol;	//��ǰ����
	int16_t							RightCurVol;//��ǰ����
	bool							LeftMuteFlag;//������־
	bool							RightMuteFlag;//������־

}AudioCoreSink;

typedef struct _AudioCoreContext
{
	AudioCoreSource AudioSource[AUDIO_CORE_SOURCE_MAX_MUN];
	AudioCoreProcessFunc AudioEffectProcess;			//****���������
	AudioCoreSink   AudioSink[AUDIO_CORE_SINK_MAX_NUM];

}AudioCoreContext;
#define	CFG_FUNC_MIC_KARAOKE_EN
#define CFG_FUNC_AUDIO_EFFECT_EN //����Чʹ�ܿ���

#ifdef CFG_FUNC_AUDIO_EFFECT_EN

    //#define CFG_FUNC_ECHO_DENOISE          //�������ٵ���delayʱ��������
 	//#define CFG_FUNC_MUSIC_EQ_MODE_EN     //Music EQģʽ��������               
	#ifdef CFG_FUNC_MUSIC_EQ_MODE_EN	    
 		#define CFG_FUNC_EQMODE_FADIN_FADOUT_EN    //EQģʽ�л�ʱfade in/fade out��������ѡ��,����EQģʽ����POP��ʱ������� 		
    #endif
	#define CFG_FUNC_MUSIC_TREB_BASS_EN    		//Music�ߵ������ڹ�������
    //#define CFG_FUNC_SILENCE_AUTO_POWER_OFF_EN     //���ź��Զ��ػ����ܣ�
    #ifdef CFG_FUNC_SILENCE_AUTO_POWER_OFF_EN      
		#define  SILENCE_THRESHOLD                 120        //�����źż�����ޣ�С�����ֵ��Ϊ���ź�
		#define  SILENCE_POWER_OFF_DELAY_TIME      60*1000     //���źŹػ���ʱʱ�䣬��λ��ms
    #endif

	#if CFG_RES_MIC_SELECT
	#define	CFG_FUNC_MIC_KARAOKE_EN      //MIC karaoke����ѡ��
	#endif

	#ifdef CFG_FUNC_MIC_KARAOKE_EN
		//#define CFG_FUNC_GUITAR_EN        //������Ч����ѡ��
		//#define CFG_FUNC_DETECT_MIC_EN 	//mic��μ�⹦��
        //#define CFG_FUNC_MIC_TREB_BASS_EN   //mic�ߵ������ڹ�������
		//���ܹ���ѡ������
		//ע:����Ҫ������Ƶ��������౸��������⣬��Ҫ�����˹���(����MIC�źż��ӿڴ���)
		#define  CFG_FUNC_SHUNNING_EN                                
			#define SHNNIN_VALID_DATA                          	 500  ////MIC������ֵ
			#define SHNNIN_STEP                                  256  /////���ε��ڵĲ���
			#define SHNNIN_THRESHOLD                             SHNNIN_STEP*2  ////threshold
			#define SHNNIN_VOL_RECOVER_TIME                      50////���������ָ�ʱ����100*20ms = 2s
			#define SHNNIN_UP_DLY                                3/////��������ʱ��
			#define SHNNIN_DOWN_DLY                              1/////�����½�ʱ��
	#endif	

	//**��ƵSDK�汾��,��Ҫ�޸�**/
//	#define  CFG_EFFECT_MAJOR_VERSION						(CFG_SDK_MAJOR_VERSION)
//	#define  CFG_EFFECT_MINOR_VERSION						(CFG_SDK_MINOR_VERSION)
//	#define  CFG_EFFECT_USER_VERSION						(CFG_SDK_PATCH_VERSION)

	#define  CFG_EFFECT_MAJOR_VERSION						1
	#define  CFG_EFFECT_MINOR_VERSION						2
	#define  CFG_EFFECT_USER_VERSION						3
	/**���ߵ���Ӳ���ӿ���USB HID�ӿڣ�����UART�ӿ�*/
	/**����ʹ��USB HID�ӿڣ��շ�buf��512Byte*/
	/**UART�ӿ�ռ��2·DMA���շ�Buf��2k Byte*/
	/**����ʹ��USB HID��Ϊ�����ӿڣ�DMA��Դ����*/
	/**���ʹ��UART��Ϊ�����ӿ�������DMA�ӿڲ���֤DMA��Դ����*/
	#define  CFG_COMMUNICATION_BY_USB		// usb or uart 					
	//#define  CFG_COMMUNICATION_BY_UART		// usb or uart 				
	
	#define	 CFG_UART_COMMUNICATION_TX_PIN					GPIOA10
	#define  CFG_UART_COMMUNICATION_TX_PIN_MUX_SEL			(3)
	#define  CFG_UART_COMMUNICATION_RX_PIN					GPIOA9
	#define  CFG_UART_COMMUNICATION_RX_PIN_MUX_SEL			(1)
	
	#define  CFG_COMMUNICATION_CRYPTO						(0)////����ͨѶ����=1 ����ͨѶ������=0
	#define  CFG_COMMUNICATION_PASSWORD                     0x11223344//////���ֽڵĳ�������
#endif


#endif
