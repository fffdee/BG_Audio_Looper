#include <stdint.h>
#include <stdbool.h>
#include "audio_setting.h"
#include "audio_adc.h"
#include "debug.h"
// dB表，索引0~31，单位dB，Mic为21.14~-18.29，LineIn为13.25~-16.3
static const float mic_db_table[32] = {
    21.14, 19.76, 18.29, 17.04, 15.94, 14.67, 13.56, 12.12,
    10.89, 9.48, 7.98, 6.48, 5.19, 4.07, 2.78, 1.52,
    0.42, -0.86, -1.98, -3.19, -4.46, -5.57, -6.85, -8.1,
    -9.3, -10.56, -11.82, -13.08, -14.42, -15.7, -17.0, -18.29
};

// 硬件音量范围定义（与AudioADC_VolSetChannel文档一致）
#define VOL_MIN        0x001  // -72dB
#define VOL_MAX        0xFFF  // 0dB
#define DB_HW_MAX      0.0f   // 硬件最大音量对应dB
#define DB_HW_MIN      -72.0f // 硬件最小音量对应dB

/**
 * @brief  百分比转dB（0-100% → 对应mic_db_table中的dB值）
 * @param  percent 音量百分比(0-100)
 * @return 对应的dB值
 */
static float percent_to_db(uint8_t percent) {
    if (percent > 100) percent = 100;
    // 0%对应表中最小dB(-18.29), 100%对应表中最大dB(21.14)
    int idx = (percent * 31 + 50) / 100; // 四舍五入计算索引
    idx = (idx < 0) ? 0 : (idx > 31) ? 31 : idx;
    return mic_db_table[idx];
}

/**
 * @brief  dB转百分比（输入dB → 匹配mic_db_table后转0-100%）
 * @param  db 输入的dB值
 * @return 对应的音量百分比
 */
static uint8_t db_to_percent(float db) {
    int idx = 0;
    float min_diff = 100.0f;
    uint8_t i;
    // 找到表中最接近的dB值对应的索引
    for (i = 0; i < 32; i++) {
        float diff = (db > mic_db_table[i]) ? (db - mic_db_table[i]) : (mic_db_table[i] - db);
        if (diff < min_diff) {
            min_diff = diff;
            idx = i;
        }
    }
    // 索引转百分比（统一四舍五入规则）
    return (uint8_t)((idx * 100 + 50) / 31);
}

/**
 * @brief  dB值转硬件音量值（核心映射：dB → 0x001~0xFFF）
 * @param  db 输入的dB值
 * @return 对应的硬件音量值(0x001~0xFFF)
 */
static uint16_t db_to_vol(float db) {
    // 1. 先将输入dB限制在硬件支持的范围内
    float clamped_db = db;
    if (clamped_db > DB_HW_MAX) clamped_db = DB_HW_MAX;
    if (clamped_db < DB_HW_MIN) clamped_db = DB_HW_MIN;

    // 2. 线性映射：dB值 → 硬件音量值
    // 公式：vol = VOL_MIN + (db - DB_HW_MIN) * (VOL_MAX - VOL_MIN) / (DB_HW_MAX - DB_HW_MIN)
    float vol_float = VOL_MIN + (clamped_db - DB_HW_MIN) * (VOL_MAX - VOL_MIN) / (DB_HW_MAX - DB_HW_MIN);

    // 3. 转整数并做边界保护
    uint16_t vol = (uint16_t)(vol_float + 0.5f); // 四舍五入
    if (vol > VOL_MAX) vol = VOL_MAX;
    if (vol < VOL_MIN) vol = VOL_MIN;

    return vol;
}

/**
 * @brief  硬件音量值转dB值（核心反向映射：0x001~0xFFF → dB）
 * @param  vol 硬件音量值
 * @return 对应的dB值
 */
static float vol_to_db(uint16_t vol) {
    // 1. 边界保护
    if (vol > VOL_MAX) vol = VOL_MAX;
    if (vol < VOL_MIN) vol = VOL_MIN;

    // 2. 反向映射：vol → dB
    // 公式：db = DB_HW_MIN + (vol - VOL_MIN) * (DB_HW_MAX - DB_HW_MIN) / (VOL_MAX - VOL_MIN)
    float db = DB_HW_MIN + (vol - VOL_MIN) * (DB_HW_MAX - DB_HW_MIN) / (VOL_MAX - VOL_MIN);

    return db;
}

// -------------------------- 基础音量设置/获取（直接操作硬件值） --------------------------
/**
 * @brief  设置麦克风1音量（ADC0左）
 * @param  vol 硬件音量值(0~0xFFF)
 */
void AudioSetting_SetMic1Volume(uint16_t vol) {
    if (vol > VOL_MAX) vol = VOL_MAX;
    // 静音特殊处理：0直接设为0，否则按硬件规则设为≥0x001
    uint16_t actual_vol = (vol == 0) ? 0 : (vol < VOL_MIN) ? VOL_MIN : vol;
    //DBG("vol is %d\n", actual_vol);
    AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_LEFT, actual_vol);
}

/**
 * @brief  获取麦克风1音量（ADC0左）
 * @return 硬件音量值(0~0xFFF)
 */
uint16_t AudioSetting_GetMic1Volume(void) {
    uint16_t leftVol = 0, rightVol = 0;
    AudioADC_VolGet(ADC0_MODULE, &leftVol, &rightVol);
    return leftVol;
}

/**
 * @brief  设置麦克风2音量（ADC0右）
 * @param  vol 硬件音量值(0~0xFFF)
 */
void AudioSetting_SetMic2Volume(uint16_t vol) {
    if (vol > VOL_MAX) vol = VOL_MAX;
    uint16_t actual_vol = (vol == 0) ? 0 : (vol < VOL_MIN) ? VOL_MIN : vol;
    AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_RIGHT, actual_vol);
}

/**
 * @brief  设置吉他1音量（ADC1左）
 * @param  vol 硬件音量值(0~0xFFF)
 */
void AudioSetting_SetGuitar1Volume(uint16_t vol) {
    if (vol > VOL_MAX) vol = VOL_MAX;
    uint16_t actual_vol = (vol == 0) ? 0 : (vol < VOL_MIN) ? VOL_MIN : vol;

    AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_LEFT, actual_vol);
}

/**
 * @brief  设置吉他2音量（ADC1右）
 * @param  vol 硬件音量值(0~0xFFF)
 */
void AudioSetting_SetGuitar2Volume(uint16_t vol) {
    if (vol > VOL_MAX) vol = VOL_MAX;
    uint16_t actual_vol = (vol == 0) ? 0 : (vol < VOL_MIN) ? VOL_MIN : vol;
    //DBG("vol is %d \n",vol);
    AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_RIGHT, actual_vol);
}

void AudioSetting_SetMic1VolumePercent(uint8_t percent) {
    if (percent > 100) percent = 100;

    // 核心公式：线性映射
    // 0% -> 0
    // 100% -> 4095
    uint16_t vol = (uint16_t)(((uint32_t)percent * VOL_MAX) / 100);

    AudioADC_VolSetChannel(ADC1_MODULE, CHANNEL_LEFT, vol);
}

uint8_t AudioSetting_GetMic1VolumePercent(void) {
	 uint16_t leftVol = 0, rightVol = 0;
    AudioADC_VolGet(ADC1_MODULE, &leftVol, &rightVol);
    return (uint8_t)(((uint32_t)leftVol * 100) / VOL_MAX);
}

void AudioSetting_SetMic2VolumePercent(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint16_t vol = (uint16_t)(((uint32_t)percent * VOL_MAX) / 100);

    AudioADC_VolSetChannel(ADC1_MODULE, CHANNEL_RIGHT, vol);
}

uint8_t AudioSetting_GetMic2VolumePercent(void) {
    uint16_t leftVol = 0, rightVol = 0;
    AudioADC_VolGet(ADC1_MODULE, &leftVol, &rightVol);
    return (uint8_t)(((uint32_t)rightVol * 100) / VOL_MAX);
}

void AudioSetting_SetGuitar1VolumePercent(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint16_t vol = (uint16_t)(((uint32_t)percent * VOL_MAX) / 100);

    AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_LEFT, vol);
}

uint8_t AudioSetting_GetGuitar1VolumePercent(void) {
    uint16_t leftVol = 0, rightVol = 0;
    AudioADC_VolGet(ADC0_MODULE, &leftVol, &rightVol);
    return (uint8_t)(((uint32_t)leftVol * 100) / VOL_MAX);
}

void AudioSetting_SetGuitar2VolumePercent(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint16_t vol = (uint16_t)(((uint32_t)percent * VOL_MAX) / 100);
    //DBG("vol is %d \n",vol);
    AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_RIGHT, vol);
}

uint8_t AudioSetting_GetGuitar2VolumePercent(void) {
    uint16_t leftVol = 0, rightVol = 0;
    AudioADC_VolGet(ADC0_MODULE, &leftVol, &rightVol);
    return (uint8_t)(((uint32_t)rightVol * 100) / VOL_MAX);
}
