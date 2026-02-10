#include <stdint.h>
#include <stdbool.h>
#include "audio_setting.h"
#include "audio_adc.h"
#include "debug.h"
#include "sys_param.h"
// dB table, index 0~31, unit dB, Mic is 21.14~-18.29, LineIn is 13.25~-16.3
static const float mic_db_table[32] = {
    21.14, 19.76, 18.29, 17.04, 15.94, 14.67, 13.56, 12.12,
    10.89, 9.48, 7.98, 6.48, 5.19, 4.07, 2.78, 1.52,
    0.42, -0.86, -1.98, -3.19, -4.46, -5.57, -6.85, -8.1,
    -9.3, -10.56, -11.82, -13.08, -14.42, -15.7, -17.0, -18.29
};

// Hardware volume range definition (consistent with AudioADC_VolSetChannel documentation)
#define VOL_MIN        0x001  // -72dB
#define VOL_MAX        0xFFF  // 0dB
#define DB_HW_MAX      0.0f   // Hardware maximum volume corresponds to dB
#define DB_HW_MIN      -72.0f // Hardware minimum volume corresponds to dB

/**
 * @brief  Convert percentage to dB (0-100% → corresponding dB value in mic_db_table)
 * @param  percent volume percentage (0-100)
 * @return corresponding dB value
 */
static float percent_to_db(uint8_t percent) {
    if (percent > 100) percent = 100;
    // 0% corresponds to minimum dB in table (-18.29), 100% corresponds to maximum dB in table (21.14)
    int idx = (percent * 31 + 50) / 100; // Round to nearest integer to calculate index
    idx = (idx < 0) ? 0 : (idx > 31) ? 31 : idx;
    return mic_db_table[idx];
}

/**
 * @brief  Convert dB to percentage (input dB → match mic_db_table then convert to 0-100%)
 * @param  db input dB value
 * @return corresponding volume percentage
 */
static uint8_t db_to_percent(float db) {
    int idx = 0;
    float min_diff = 100.0f;
    uint8_t i;
    // Find the index corresponding to the closest dB value in the table
    for (i = 0; i < 32; i++) {
        float diff = (db > mic_db_table[i]) ? (db - mic_db_table[i]) : (mic_db_table[i] - db);
        if (diff < min_diff) {
            min_diff = diff;
            idx = i;
        }
    }
    // Convert index to percentage (unified rounding rule)
    return (uint8_t)((idx * 100 + 50) / 31);
}

/**
 * @brief  Convert dB value to hardware volume value (core mapping: dB → 0x001~0xFFF)
 * @param  db input dB value
 * @return corresponding hardware volume value (0x001~0xFFF)
 */
static uint16_t db_to_vol(float db) {
    // 1. First limit the input dB to the hardware supported range
    float clamped_db = db;
    if (clamped_db > DB_HW_MAX) clamped_db = DB_HW_MAX;
    if (clamped_db < DB_HW_MIN) clamped_db = DB_HW_MIN;

    // 2. Linear mapping: dB value → hardware volume value
    // Formula: vol = VOL_MIN + (db - DB_HW_MIN) * (VOL_MAX - VOL_MIN) / (DB_HW_MAX - DB_HW_MIN)
    float vol_float = VOL_MIN + (clamped_db - DB_HW_MIN) * (VOL_MAX - VOL_MIN) / (DB_HW_MAX - DB_HW_MIN);

    // 3. Convert to integer and do boundary protection
    uint16_t vol = (uint16_t)(vol_float + 0.5f); // Round to nearest
    if (vol > VOL_MAX) vol = VOL_MAX;
    if (vol < VOL_MIN) vol = VOL_MIN;

    return vol;
}

/**
 * @brief  Convert hardware volume value to dB value (core reverse mapping: 0x001~0xFFF → dB)
 * @param  vol hardware volume value
 * @return corresponding dB value
 */
static float vol_to_db(uint16_t vol) {
    // 1. Boundary protection
    if (vol > VOL_MAX) vol = VOL_MAX;
    if (vol < VOL_MIN) vol = VOL_MIN;

    // 2. Reverse mapping: vol → dB
    // Formula: db = DB_HW_MIN + (vol - VOL_MIN) * (DB_HW_MAX - DB_HW_MIN) / (VOL_MAX - VOL_MIN)
    float db = DB_HW_MIN + (vol - VOL_MIN) * (DB_HW_MAX - DB_HW_MIN) / (VOL_MAX - VOL_MIN);

    return db;
}

// -------------------------- Basic volume setting/getting (directly operate hardware values) --------------------------
/**
 * @brief  Set microphone 1 volume (ADC0 left)
 * @param  vol hardware volume value (0~0xFFF)
 */
void AudioSetting_SetMic1Volume(uint16_t vol) {
    if (vol > VOL_MAX) vol = VOL_MAX;
    // Mute special handling: 0 is directly set to 0, otherwise set to ≥0x001 according to hardware rules
    uint16_t actual_vol = (vol == 0) ? 0 : (vol < VOL_MIN) ? VOL_MIN : vol;
    //DBG("vol is %d\n", actual_vol);
    AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_LEFT, actual_vol);
}

/**
 * @brief  Get microphone 1 volume (ADC0 left)
 * @return hardware volume value (0~0xFFF)
 */
uint16_t AudioSetting_GetMic1Volume(void) {
    uint16_t leftVol = 0, rightVol = 0;
    AudioADC_VolGet(ADC0_MODULE, &leftVol, &rightVol);
    return leftVol;
}

/**
 * @brief  Set microphone 2 volume (ADC0 right)
 * @param  vol hardware volume value (0~0xFFF)
 */
void AudioSetting_SetMic2Volume(uint16_t vol) {
    if (vol > VOL_MAX) vol = VOL_MAX;
    uint16_t actual_vol = (vol == 0) ? 0 : (vol < VOL_MIN) ? VOL_MIN : vol;
    AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_RIGHT, actual_vol);
}

/**
 * @brief  Set guitar 1 volume (ADC1 left)
 * @param  vol hardware volume value (0~0xFFF)
 */
void AudioSetting_SetGuitar1Volume(uint16_t vol) {
    if (vol > VOL_MAX) vol = VOL_MAX;
    uint16_t actual_vol = (vol == 0) ? 0 : (vol < VOL_MIN) ? VOL_MIN : vol;

    AudioADC_VolSetChannel(ADC0_MODULE, CHANNEL_LEFT, actual_vol);
}

/**
 * @brief  Set guitar 2 volume (ADC1 right)
 * @param  vol hardware volume value (0~0xFFF)
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


    return (uint8_t) SYSPARAM_AUDIO()->guitar2_volume;
}
