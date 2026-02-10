/*
 * audio_setting.h
 *
 *  Created on: Dec 27, 2025
 *      Author: Hasee
 */
#ifndef __AUDIO_SETTING_H_
#define __AUDIO_SETTING_H_

// Microphone 1 (ADC0 left)
void AudioSetting_SetMic1Volume(uint16_t vol);
uint16_t AudioSetting_GetMic1Volume(void);
void AudioSetting_SetMic1VolumePercent(uint8_t percent);
uint8_t AudioSetting_GetMic1VolumePercent(void);

// Microphone 2 (ADC0 right)
void AudioSetting_SetMic2Volume(uint16_t vol);
uint16_t AudioSetting_GetMic2Volume(void);
void AudioSetting_SetMic2VolumePercent(uint8_t percent);
uint8_t AudioSetting_GetMic2VolumePercent(void);

// Guitar 1 (ADC1 left)
void AudioSetting_SetGuitar1Volume(uint16_t vol);
uint16_t AudioSetting_GetGuitar1Volume(void);
void AudioSetting_SetGuitar1VolumePercent(uint8_t percent);
uint8_t AudioSetting_GetGuitar1VolumePercent(void);

// Guitar 2 (ADC1 right)
void AudioSetting_SetGuitar2Volume(uint16_t vol);
uint16_t AudioSetting_GetGuitar2Volume(void);
void AudioSetting_SetGuitar2VolumePercent(uint8_t percent);
uint8_t AudioSetting_GetGuitar2VolumePercent(void);

#endif

