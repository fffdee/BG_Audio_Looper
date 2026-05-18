#ifndef SRC_HARDWARE_BATTERY_BATTERY_DRV_H_
#define SRC_HARDWARE_BATTERY_BATTERY_DRV_H_

#include <stdint.h>

/************************ Ӳ���������ã��ɸ���ʵ���޸ģ� ************************/
/**
 * @brief ADC������̣�ͨ��12λADCΪ4095��
 */
#define ADC_MAX         4095

/**
 * @brief ADC�ο���ѹ����λ��V����STM32Ĭ��3.3V��ESP32������Ϊ3.3V/1.1V��
 */
#define ADC_REF_VOLT    3.3f

/**
 * @brief ��ѹ��ϵ����1:1��ѹ �� ��ص�ѹ=ADC������ѹ��2����2:1��ѹ��Ϊ3��
 *        ���㹫ʽ����ѹ��ϵ�� = (��������+��������)/��������
 */
#define VOLT_DIV_RATIO  2.0f

/**
 * @brief ﮵�������ѹ����λ��V��������Ԫ﮵���ֵ4.2V���������3.65V��
 */
#define FULL_VOLT       4.2f

/**
 * @brief ﮵�طŵ��ֹ��ѹ����λ��V��������Ԫ����3.0V��������ţ�
 */
#define EMPTY_VOLT      3.0f

/**
 * @brief �����˲����泤�ȣ�ֵԽ���˲�Ч��Խ�ã���ӦԽ��������3~8��
 */
#define FILTER_BUF_LEN  5

/************************ ���Ⱪ¶�ĺ���API ************************/
/**
 * @brief ��ȡ���ʣ������ٷֱȣ�SOC��
 * @return �����ٷֱȣ�0~100����0��ʾ���ţ�100��ʾ����
 * @note �����ڲ��Զ����ADC��ȡ���˲�����ѹת�����������㣬������ֱ�ӵ��ü���
 */
uint8_t battery_get_soc(void);

/**
 * @brief ����ѡ����ȡ���ʵʱ��ѹ�����ڵ��ԣ�
 * @return ���ʵ�ʵ�ѹ����λ��V��
 */
float battery_get_volt(void);

/**
 * @brief Get battery voltage in millivolts (integer only, no FPU required)
 * @return Battery voltage in millivolts
 */
uint16_t battery_get_volt_mv(void);

#endif /* SRC_HARDWARE_BATTERY_BATTERY_DRV_H_ */
