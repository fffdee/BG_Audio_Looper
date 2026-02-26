#ifndef SRC_HARDWARE_BATTERY_BATTERY_DRV_H_
#define SRC_HARDWARE_BATTERY_BATTERY_DRV_H_

#include <stdint.h>

/************************ 硬件参数配置（可根据实际修改） ************************/
/**
 * @brief ADC最大量程（通常12位ADC为4095）
 */
#define ADC_MAX         4095

/**
 * @brief ADC参考电压（单位：V，如STM32默认3.3V，ESP32可配置为3.3V/1.1V）
 */
#define ADC_REF_VOLT    3.3f

/**
 * @brief 分压比系数（1:1分压 → 电池电压=ADC采样电压×2；若2:1分压则为3）
 *        计算公式：分压比系数 = (上拉电阻+下拉电阻)/下拉电阻
 */
#define VOLT_DIV_RATIO  2.0f

/**
 * @brief 锂电池满电电压（单位：V，单体三元锂典型值4.2V，磷酸铁锂3.65V）
 */
#define FULL_VOLT       4.2f

/**
 * @brief 锂电池放电截止电压（单位：V，单体三元锂最低3.0V，避免过放）
 */
#define EMPTY_VOLT      3.0f

/**
 * @brief 滑动滤波缓存长度（值越大滤波效果越好，响应越慢，建议3~8）
 */
#define FILTER_BUF_LEN  5

/************************ 对外暴露的核心API ************************/
/**
 * @brief 获取电池剩余电量百分比（SOC）
 * @return 电量百分比（0~100），0表示过放，100表示满电
 * @note 驱动内部自动完成ADC读取、滤波、电压转换、电量计算，主函数直接调用即可
 */
uint8_t battery_get_soc(void);

/**
 * @brief （可选）获取电池实时电压（便于调试）
 * @return 电池实际电压（单位：V）
 */
float battery_get_volt(void);

#endif /* SRC_HARDWARE_BATTERY_BATTERY_DRV_H_ */
