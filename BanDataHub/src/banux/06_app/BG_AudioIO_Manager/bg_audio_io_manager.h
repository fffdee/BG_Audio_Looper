#ifndef _BG_AUDIO_IO_MANAGER_H__
#define _BG_AUDIO_IO_MANAGER_H__

#include <stdint.h>
#include "product_def.h"

#define AUDIO_BUF_MAX 128
typedef struct {

    uint8_t MicEnable;
    uint8_t mic_count;
    uint8_t det_state;
    uint8_t guitar_count;
    uint8_t LineOutEnable;
    uint8_t LineInEnable;
    uint16_t SampleRate;
    uint32_t mic_buf_in[AUDIO_BUF_MAX];
    uint32_t mic_buf_out[AUDIO_BUF_MAX];
    uint32_t guitar_buf_in[AUDIO_BUF_MAX];
    uint32_t guitar_buf_out[AUDIO_BUF_MAX];
    uint32_t USB_adc_buf[AUDIO_BUF_MAX];
    uint32_t USB_dac_buf[AUDIO_BUF_MAX];
    uint32_t OutPut_buf[AUDIO_BUF_MAX];

} Audio_Data;

typedef struct bg_audio_io_manager
{
    Audio_Data Audio_data;
    void (*Audio_Init)(uint16_t);
    void (*Audio_Loop)(void);
} BG_Audio_Io_Manager;

typedef enum {
    NONE = 0,
    MIC_DET_IN,
    MIC_DET_OUT,
    GUITAR_DET_IN,
    GUITAR_DET_OUT,
    EARPHONE_DET,
    SPEAKER_DET
} DET_STATE;

extern BG_Audio_Io_Manager BG_AudioManager;

#if EFFECT_GRAPHICS_EN
void BG_AudioIO_SetupEffectGraphCallbacks(void);
#endif
void BG_AudioIO_PrepareForShutdown(void);

#endif
