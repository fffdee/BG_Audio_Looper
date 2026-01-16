/**
 *******************************************************************************
 * @file    audio_adc.h
 * @brief	ģ��Ʀ�����A/Dת������ASDM����������ӿ�
 *
 * @author  Sam
 * @version V1.0.0
 *
 * $Created: 2017-04-26 13:27:11$
 *
 * &copy; Shanghai Mountain View Silicon Technology Co.,Ltd. All rights reserved.
 *******************************************************************************
 */

/**
 * @addtogroup AUDIO_ADC
 * @{
 * @defgroup audio_adc audio_adc.h
 * @{
 */

#ifndef __AUDIO_ADC_H__
#define __AUDIO_ADC_H__

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

////////////////////////////////////////////
//     Gain Volume Table for PGA(dB)      //
////////////////////////////////////////////
//	Vol // Gain of Mic  // Gain of LineIn  // Gain of fm  //
//  00	//     21.14	  //      13.25      //      44.31  //
//  01	//     19.76	  //      12.26      //      42.99  //
//  02	//     18.29	  //      11.37      //      41.84  //
//  03	//     17.04	  //      10.31      //      40.82  //
//  04	//     15.94	  //      9.365      //      39.73  //
//  05	//     14.67	  //      8.313      //      38.77  //
//  06	//     13.56	  //      7.198      //      37.69  //
//  07	//     12.12	  //      6.21       //      36.73  //
//  08	//     10.89    //      5.323      //      35.59  //
//  09	//     9.48	    //      4.519      //      34.57  //
//  10	//     7.98	    //      3.436      //      33.66  //
//  11	//     6.48	    //      2.4737     //      32.84  //
//  12	//     5.19	    //      1.6074     //      31.72  //
//  13	//     4.07	    //      0.5723     //      30.72  //
//  14	//     2.78	    //      -0.352     //      29.69  //
//  15	//     1.52	    //      -1.32      //      28.76  //
//  16	//     0.42     //      -2.249     //      27.92  //
//  17	//     -0.86    //      -3.248     //      26.77  //
//  18	//     -1.98    //      -4.143     //      25.75  //
//  19	//     -3.19    //      -5.082     //      24.83  //
//  20	//     -4.46    //      -6.045     //      23.71  //
//  21	//     -5.57    //      -7.014     //      22.71  //
//  22	//     -6.85    //      -7.885     //      21.58  //
//  23	//     -8.1     //      -8.76      //      20.56  //
//  24	//     -9.3     //      -9.71      //      19.44  //
//  25	//     -10.56   //      -10.69     //      18.43  //
//  26	//     -11.82   //      -11.58     //      17.5   //
//  27	//     -13.08   //      -12.49     //      16.64  //
//  28	//     -14.42   //      -13.52     //      15.64  //
//  29	//     -15.7    //      -14.43     //      14.65  //
//  30	//     -17      //      -15.34     //      13.62  //
//  31	//     -18.29   //      -16.3      //      12.66  //
//  32	//                                         11.75  //
//  33	//                                         10.617 //
//  34	//                                         9.55   //
//  35	//                                         8.536  //
//  36	//                                         7.567  //
//  37	//                                         6.632  //
//  38	//                                         5.577  //
//  39	//                                         4.55   //
//  40	//                                         3.545  //
//  41	//                                         2.553  //
//  42	//                                         1.5678 //
//  43	//                                         0.5834 //
//  44	//                                         -0.407 //
//  45	//                                         -1.41  //
//  46	//                                         -2.432 //
//  47	//                                         -3.481 //
//  48	//                                         -4.409 //
//  49	//                                         -5.371 //
//  50	//                                         -6.375 //
//  51	//                                         -7.43  //
//  52	//                                         -8.55  //
//  53	//                                         -9.44  //
//  54	//                                         -10.39 //
//  55	//                                         -11.4  //
//  56	//                                         -12.36 //
//  57	//                                         -13.41 //
//  58	//                                         -14.47 //
//  59	//                                         -15.39 //
//  60	//                                         -16.4  //
//  61	//                                         -17.5  //
//  62	//                                         -18.51 //
//  63	//                                         -19.17 //
////////////////////////////////////////////

/**
 * ADC ģ��
 */
typedef enum _ADC_MODULE
{
    ADC0_MODULE,
    ADC1_MODULE

} ADC_MODULE;

typedef enum _ADC_CHANNEL
{
    CHANNEL_LEFT,
    CHANNEL_RIGHT

} ADC_CHANNEL;

typedef enum _ADC_DMIC_DOWN_SAMPLLE_RATE
{
    DOWN_SR_64,
	DOWN_SR_128

} ADC_DMIC_DSR;

typedef enum AUDIO_ADC_INPUT
{
  LINEIN_NONE,				//none�����ڹرյ�ǰPGA�µ�channelѡ��
  LINEIN1_LEFT,
  LINEIN1_RIGHT,

  LINEIN2_LEFT,
  LINEIN2_RIGHT,

  LINEIN3_LEFT_OR_MIC1,
  LINEIN3_RIGHT_OR_MIC2,

  LINEIN4_LEFT,//GPIO B1, INPUT_FMIN1_LEFT
  LINEIN4_RIGHT,//GPIO B0, INPUT_FMIN1_RIGHT

  LINEIN5_LEFT,//GPIO B3, INPUT_FMIN2_LEFT
  LINEIN5_RIGHT,//GPIO B2, INPUT_FMIN2_RIGHT
} AUDIO_ADC_INPUT;

/**
 * AGC ģʽʹ��ѡ��
 */
typedef enum _AGC_CHANNEL
{
    AGC_DISABLE	        = 0x00,       /**<��ֹAGC���� */
    AGC_RIGHT_ONLY      = 0x01,       /**<����ͨ��ʹ��AGC����*/
    AGC_LEFT_ONLY       = 0x02,       /**<����ͨ��ʹ��AGC����*/
    AGC_STEREO_OPEN     = 0x03        /**<����˫ͨ��ʹ��AGC����*/

} AGC_CHANNEL;

/**
 * @brief  ADC ģ��ʹ�ܣ��ܿ��أ�
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_Enable(ADC_MODULE ADCModule);

/**
 * @brief  ����ADC ģ��ʹ�ܣ��ܿ��أ�
 * @param  ��
 * @return ��
 * @note ����3·����ADCʱ����֤����ADC��������λ��ͬ��
 * @note ���øú����ĵط���Ҫע��ر��ж�,��ֹ�����ڲ�ִ��ʱ���жϴ��
 */
void AudioADC_AllModuleEnable(void);

/**
 * @brief  ADC ģ��ر�
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_Disable(ADC_MODULE ADCModule);

/**
 * @brief  ADC ģ������ͨ��ʹ��ѡ��
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  IsLeftEn     TRUE,��ͨ����ʹ; FALSE,��ͨ���ر�
 * @param  IsRightEn    TRUE,��ͨ����ʹ; FALSE,��ͨ���ر�
 * @return ��
 */
void AudioADC_LREnable(ADC_MODULE ADCModule, bool IsLeftEn, bool IsRightEn);

/**
 * @brief  ADC ģ���Ƿ񽻻�����ͨ������
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  IsSwap       TRUE,����ͨ����������; FALSE,����ͨ����������
 * @return ��
 */
void AudioADC_ChannelSwap(ADC_MODULE ADCModule, bool IsSwap);

/**
 * @brief  ADC ģ���ͨ�˲�����ֹƵ�ʲ�������
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  Coefficient  �˲���ϵ����12bitλ����Ĭ��ֵ0xFFE��
 *   @arg  Coefficient = 0xFFE  48k����������20Hz��˥��-1.5db��
 *   @arg  Coefficient = 0xFFC  48k����������40Hz��˥��-1.5db��
 *   @arg  Coefficient = 0xFFD  32k����������40Hz��˥��-1.5db��
 * @return ��
 * @Note �ú���������AudioADC_Enable()��������֮��
 */
void AudioADC_HighPassFilterConfig(ADC_MODULE ADCModule, uint16_t Coefficient);

/**
 * @brief  ADC ģ���ͨ�˲��Ƿ�ʹ�ܣ�ȥ��ֱ��ƫ����
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  IsHpfEn      TRUE,��ʹ��ͨ�˲�����FALSE,�رո�ͨ�˲���
 * @return ��
 */
void AudioADC_HighPassFilterSet(ADC_MODULE ADCModule, bool IsHpfEn);

/**
 * @brief  ADC ģ������Ĵ������ڴ��е���ֵ
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_Clear(ADC_MODULE ADCModule);

/**
 * @brief  ADC ģ�����������
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  SampleRate   ADC������ֵ��9�ֲ�����
 * @return ��
 */
void AudioADC_SampleRateSet(ADC_MODULE ADCModule, uint32_t SampleRate);

/**
 * @brief  ��ȡADC ģ�����������ֵ
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return ��ǰ����������ֵ
 */
uint32_t AudioADC_SampleRateGet(ADC_MODULE ADCModule);

/**
 * @brief  ADC ģ�鵭�뵭��ʱ������
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  FadeTime     ���뵭��ʱ��, ��λ:Ms
 * @return ��
 * @Note   ����ʱ��Ϊ10Ms��ʱ�䲻������Ϊ0�������رյ��뵭����������ú���AudioADC_FadeDisable();
 */
void AudioADC_FadeTimeSet(ADC_MODULE ADCModule, uint8_t FadeTime);

/**
 * @brief  ADC ģ�鵭�뵭������ʹ��
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_FadeEnable(ADC_MODULE ADCModule);

/**
 * @brief  ADC ģ�鵭�뵭�����ܽ���
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_FadeDisable(ADC_MODULE ADCModule);

/**
 * @brief  ADC ģ�����־������ƣ�����ͨ���ֱ��������
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  LeftMuteEn   TRUE,��ͨ������ʹ��; FALSE,��ͨ��ȡ������
 * @param  RightMuteEn  TRUE,��ͨ������ʹ��; FALSE,��ͨ��ȡ������
 * @return ��
 * @Note   �ú����ڲ�������ʱ������Ӳ���Ĵ���֮�������˳��������Ҫ�ȴ�����������ɣ���Ҫ�ں����ⲿ����ʱ
 */
void AudioADC_DigitalMute(ADC_MODULE ADCModule, bool LeftMuteEn, bool RightMuteEn);

/**
 * @brief  ADC ģ�������������ƣ�����ͨ���ֱ��������
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  LeftMuteEn   TRUE,��ͨ������ʹ��; FALSE,��ͨ��ȡ������
 * @param  RightMuteEn  TRUE,��ͨ������ʹ��; FALSE,��ͨ��ȡ������
 * @return ��
 * @Note   �ú����ڲ�����ʱ������Ǿ�������������ʱ�ȴ��������͵�0֮���˳��ú�����
 */
void AudioADC_SoftMute(ADC_MODULE ADCModule, bool LeftMuteEn, bool RightMuteEn);

/**
 * @brief  ADC ģ����������
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  LeftVol      ����������ֵ��0x00:����, 0x001:-72dB, 0xFFF:0dB
 * @param  RightVol     ����������ֵ��0x00:����, 0x001:-72dB, 0xFFF:0dB
 * @return ��
 */
void AudioADC_VolSet(ADC_MODULE ADCModule, uint16_t LeftVol, uint16_t RightVol);


/**
 * @brief  ADC ģ���������ã����������������ã�
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  ChannelSel   ��������ѡ��0x00: �ޣ�0x1����������0x2��������
 * @param  Vol     		����ֵ��0x00:����, 0x001:-72dB, 0xFFF:0dB��
 * @note   ��ChannelSelΪ3ʱ��ͬʱѡ����������������ʱVol���ö�������������Ч��������������ֵһ��
 * @return ��
 */
void AudioADC_VolSetChannel(ADC_MODULE ADCModule, ADC_CHANNEL ChannelSel, uint16_t Vol);

/**
 * @brief  ADC ģ��������ȡ
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  *LeftVol      ����������ֵ��0x00:����, 0x001:-72dB, 0xFFF:0dB
 * @param  *RightVol     ����������ֵ��0x00:����, 0x001:-72dB, 0xFFF:0dB
 * @return ��
 */
void AudioADC_VolGet(ADC_MODULE ADCModule, uint16_t* LeftVol, uint16_t* RightVol);

/**
 * @brief  ADC ģ��ģ�ⲿ���ϵ��ʼ��
 * @param  ��
 * @return ��
 * @note ���DACģ�鹤�����ϵ�����DAC�ϵ�������������pop��
 */
void AudioADC_AnaInit(void);

/**
 * @brief  ADC ģ��ģ�ⲿ��ȥ��ʼ��
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_AnaDeInit(ADC_MODULE ADCModule);

/**
 * @brief  ADC PAGͨ��ѡ��
 * @param  ADCModule	0,ADC0ģ��; 1,ADC1ģ��
 * @param  ChannelSel   0,Left;	1:Right
 * @param  InputSel 	PGA����ͨ·ѡ�񡣾����AUDIO_ADC_INPUTö��ֵ����
 * @note   ����3ѡ��ʱ��Ҫ�Ͳ���1�Ͳ���2��Ӧ����Ҫ��datasheet����
 * @return ��
 */
void AudioADC_PGASel(ADC_MODULE ADCModule, ADC_CHANNEL ChannelSel, AUDIO_ADC_INPUT InputSel);

/**
 * @brief  ADC PAG��������
 * @param  ADCModule	0,ADC0ģ��; 1,ADC1ģ��
 * @param  ChannelSel   0,Left;	1:Right
 * @param  InputSel 	InputSel PGA����ͨ·ѡ�񡣾����AUDIO_ADC_INPUTö��ֵ����
 * @param  Gain 		PGA�������á����÷�Χ��0-63����
 * @param  GainBoostSel 0:0dB; 1:9dB; 2:18dB; 3:27dB���ò���ֻ��ѡ��inputΪmicʱ�����á�
 * @note   ����3ѡ��ʱ��Ҫ�Ͳ���1�Ͳ���2��Ӧ����Ҫ��datasheet���ա�
 * @note   ����4����ֵ��Ӧ��ʵ��dB����ο����ļ�ͷ�����������ͬinputͬ����ֵ��Ӧ�Ĳ���ֵ��ͬ��
 * @note   ����5ֻ����inputΪMIC��ʱ�����Ч
 * @return ��
 */
void AudioADC_PGAGainSet(ADC_MODULE ADCModule, ADC_CHANNEL ChannelSel, AUDIO_ADC_INPUT InputSel, uint16_t Gain, uint8_t GainBoostSel);

/**
 * @brief  ADC ģ��Mic Bias1��ѹʹ��
 * @param  IsMicBiasEn  MIC Bias1ʹ��
 * @return ��
 */
void AudioADC_MicBias1Enable(bool IsMicBiasEn);

/**
 * @brief  ADCģ��PGAģ��ģʽѡ��
 * @param  LeftMode:  	��ͨ��ģʽѡ��   0��PGAģ��������    1��PGAģ�鵥������      Ĭ��Ϊ��������
 * @param  RightMode:  	��ͨ��ģʽѡ��   0�� PGAģ��������    1��PGAģ�鵥������     Ĭ��Ϊ��������
 * @note ���ģʽֻ��ADC0ģ��֧�֡�
 * @return ��
 */
void AudioADC_PGAMode(uint8_t LeftMode, uint8_t RightMode);

/**
 * @brief  PGAģ���������ѹ�������
 * @param  LeftGainSel:  	��ͨ������ѡ��.0:0dB;1:6dB;2:10dB;3:15dB
 * @param  RightGainSel: 	��ͨ������ѡ��.0:0dB;1:6dB;2:10dB;3:15dB
 * @note ���ģʽֻ��ADC0ģ��֧��
 * @return ��
 */
void AudioADC_PGADiffGainSel(uint8_t leftdifGainSel, uint8_t rightdifGainSel);

/**
 * @brief  AGCģ��PGAģ�������ʹ��
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  IsLeftEn:  	��ͨ��ʹ��
 * @param  IsRightEn: 	��ͨ��ʹ��
 * @return ��
 */
void AudioADC_PGAZeroCrossEnable(ADC_MODULE ADCModule, bool IsLeftEn, bool IsRightEn);

/**
 * @brief  ASDMģ��ʹ��DMIC����
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_DMICEnable(ADC_MODULE ADCModule);

/**
 * @brief  ASDMģ�����DMIC����
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_DMICDisable(ADC_MODULE ADCModule);

/**
 * @brief  DMIC�������ʱ���ѡ��
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  DownSampleRate    DOWN_SR_64,64��������; DOWN_SR_128,128��������
 * @return ��
 */
void AudioADC_DMICDownSampleSel(ADC_MODULE ADCModule, ADC_DMIC_DSR DownSampleRate);

/**
 * @brief  ADC ģ��AGCģ��ͨ��ѡ��
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  IsLeftEn  	��ͨ��ʹ��
 * @param  IsRightEn	��ͨ��ʹ��
 * @return ��
 */
void AudioADC_AGCChannelSel(ADC_MODULE ADCModule, bool IsLeftEn, bool IsRightEn);

/**
 * @brief  ADC ģ����������AGC����
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  GainOffset 	����ƫ�����á���8 ~ 15��-->��-4dB ~ -0.5dB��;��0 ~ 7��-->��0dB ~ 3.5dB��.
 * @return ��
 */
void AudioADC_AGCGainOffset(ADC_MODULE ADCModule, uint8_t GainOffset);

/**
 * @brief  ADC ģ��AGCģ���������ˮƽ
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  MaxLevel     AGCģ���������ˮƽ.��0 ~ 31��-->��-3 ~ -34dB��
 * @return ��
 */
void AudioADC_AGCMaxLevel(ADC_MODULE ADCModule, uint8_t MaxLevel);

/**
 * @brief  AGCģ������Ŀ��ˮƽ
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  TargetLevel	AGCģ������Ŀ��ˮƽ.��0 ~ 31��-->��-3 ~ -34dB��
 * @return ��
 */
void AudioADC_AGCTargetLevel(ADC_MODULE ADCModule, uint8_t TargetLevel);

/**
 * @brief  AGCģģ��ɵ��ڵ��������
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  MaxGain		AGCģ���������.��0 ~ 63��-->�� 39.64 ~ -20.3dB��,step:-0.95dB.
 * @return ��
 */
void AudioADC_AGCMaxGain(ADC_MODULE ADCModule, uint8_t MaxGain);

/**
 * @brief  AGCģģ��ɵ��ڵ���С����
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  MinGain		AGCģ����С����.��0 ~ 63��-->�� 39.64 ~ -20.3dB��,step:-0.95dB.
 * @return ��
 */
void AudioADC_AGCMinGain(ADC_MODULE ADCModule, uint8_t MinGain);

/**
 * @brief  AGCģ��֡ʱ��
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  FrameTime	֡ʱ�����á� ��λ��ms�� ��Χ�� 1 ~ 4096��
 * @return ��
 */
void AudioADC_AGCFrameTime(ADC_MODULE ADCModule, uint16_t FrameTime);

/**
 * @brief  AGCģ�鱣��ʱ�䣬��ʼAGC�㷨����Ӧ����֮ǰ�ı���ʱ�䡣
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  HoldTime		AGC��ʼ�㷨֮ǰ�ı���ʱ�䡣��λ��ms����Χ��0*FrameTime ~ 31*FrameTime��
 * @note   HoldTime����ΪFrameTimer���������������ڲ�Ҳ���������롣
 * @return ��
 */
void AudioADC_AGCHoldTime(ADC_MODULE ADCModule, uint32_t HoldTime);

/**
 * @brief  AGCģ�鵱�����ź�̫��ʱ��AGC����˥���Ĳ���ʱ�����á�
 * @param  ADCModule    	0,ADC0ģ��; 1,ADC1ģ��
 * @param  AttackStepTime 	AGC������ǿ�Ĳ���ʱ�䣬��λΪms,��ΧΪ1 ~ 4096 ms
 * @return ��
 */
void AudioADC_AGCAttackStepTime(ADC_MODULE ADCModule, uint16_t AttackStepTime);

/**
 * @brief  AGCģ�鵱�����ź�̫Сʱ��AGC������ǿ�Ĳ���ʱ�����á�
 * @param  ADCModule    	0,ADC0ģ��; 1,ADC1ģ��
 * @param  DecayStepTime 	AGC������ǿ�Ĳ���ʱ�䣬��λΪms,��ΧΪ1 ~ 4096 ms
 * @return ��
 */
void AudioADC_AGCDecayStepTime(ADC_MODULE ADCModule, uint16_t DecayStepTime);

/**
 * @brief  AGCģ��AGC������ֵ����
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  NoiseThreshold�� ������ֵ����,��Χ����0 ~ 31����Ӧֵ����-90dB ~ -28dB����step��2dB
 * 						      Ĭ��Ϊ 01111,��-60 dB
 * @return ��
 */
void AudioADC_AGCNoiseThreshold(ADC_MODULE ADCModule, uint8_t NoiseThreshold);

/**
 * @brief  AGCģ��AGCģ������ģʽѡ��,
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  NoiseMode��	0: ADC������ݵ�ƽ��������ֵ�жϣ�ȷ���������Ƿ�Ϊ������
 * 						1: ADC�������ݵ�ƽ��������ֵ�жϣ�ȷ���������Ƿ�Ϊ������
 * @return ��
 */
void AudioADC_AGCNoiseMode(ADC_MODULE ADCModule, uint8_t NoiseMode);

/**
 * @brief  AGCģ��AGCģ������Gate����ʹ��
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  NoiseGateEnable�� 0:��ֹ����Gate����; 1:ʹ������Gate����
 * @return ��
 */
void AudioADC_AGCNoiseGateEnable(ADC_MODULE ADCModule, bool NoiseGateEnable);

/**
 * @brief  AGCģ��AGCģ������Gateģʽѡ��
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  NoiseGateMode 	0:  ����鵽�����ź�ʱ��PCM����mute/unmute
 *							1:  PCM��������mute/unmute
 * @return ��
 */
void AudioADC_AGCNoiseGateMode(ADC_MODULE ADCModule, uint8_t NoiseGateMode);

/**
 * @brief  AGCģ��AGCģ����������ʱ�����á�
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  NoiseHoldTime	��������NoiseHoldTime����������㷨��ʼִ�С���λ��ms��
 * @return ��
 */
void AudioADC_AGCNoiseHoldTime(ADC_MODULE ADCModule, uint8_t NoiseHoldTime);

/**
 * @brief  AGCģ���ȡAGC����
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return AGC����
 * @note   ���ص�ֵ������AGC��ʵ������ֵ��AGCʵ������ֵ��Ĵ���ֵ
 * 		        ֮���ת����AGCʵ������ֵ��Ĵ���ֵ��Ӧ��
 */
uint8_t AudioADC_AGCGainGet(ADC_MODULE ADCModule);

/**
 * @brief  AGCģ���ȡAGCģ�龲����Ϣ
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return AGCģ�龲����־
 */
uint8_t AudioADC_AGCMuteGet(ADC_MODULE ADCModule);

/**
 * @brief  AGCģ���ȡAGCģ����±�־λ
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @return AGCģ����±�־λ
 */
uint8_t AudioADC_AGCUpdateFlagGet(ADC_MODULE ADCModule);

/**
 * @brief  AGCģ�����AGCģ����±�־λ
 * @param  ADCModule 	0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_AGCUpdateFlagClear(ADC_MODULE ADCModule);

/**
 * @brief  VCOMʹ��ΪMic����ƫ�õ�Դ
 * @param  Config 0: VCOM,DACֱ�ƶ���; 1:MicBias; 2:PowerDown VCOM
 * @return ��
 * @note   ѡ��VCOM��Ϊmic�����Դ������ʹ��DAC���������������
 * @note   ���DAC��������������DAC�ϵ磬����DAC�����ϵ�pop��
 */
void AudioADC_VcomConfig(uint8_t Config);


/**
 * @brief  Dynamic-Element-Matching enable signal
 * @param  ADCModule    0,ADC0ģ��; 1,ADC1ģ��
 * @param  IsLeftEn  	��ͨ��ʹ��
 * @param  IsRightEn	��ͨ��ʹ��
 * @return ��
 */
void AudioADC_DynamicElementMatch(ADC_MODULE ADCModule, bool IsLeftEn, bool IsRightEn);


/**
 * @brief  ��������
 * @param  ADCModule 	0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_FuncReset(ADC_MODULE ADCModule);

/**
 * @brief  �Ĵ�������
 * @param  ��
 * @return ��
 */
void AudioADC_RegReset(void);

/**
 * @brief  ��ѯADC���������Ƿ��������
 * @param  ADCModule 	0,ADC0ģ��; 1,ADC1ģ��
 * @return TREU���������
 */
bool AudioADC_IsOverflow(ADC_MODULE ADCModule);

/**
 * @brief ���ADC���������Ƿ����������־
 * @param  ADCModule 	0,ADC0ģ��; 1,ADC1ģ��
 * @return ��
 */
void AudioADC_OverflowClear(ADC_MODULE ADCModule);

void AudioADC_PowerDown(void);

#ifdef  __cplusplus
}
#endif//__cplusplus

#endif //__AUDIO_ADC_H__

/**
 * @}
 * @}
 */
