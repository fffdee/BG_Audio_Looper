/**
 * @file  bg_audio_io_internal.h
 * @brief Internal shared state/APIs for BG_AudioIO_Manager modules.
 */
#ifndef __BG_AUDIO_IO_INTERNAL_H__
#define __BG_AUDIO_IO_INTERNAL_H__

#include <stdint.h>
#include <stdbool.h>
#include "bg_audio_io_manager.h"
#include "effect_graph.h"
#include "audio_decoder_api.h"
#include "resampler.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USE_EFFECT_GRAPH_MODE
#define USE_EFFECT_GRAPH_MODE  1
#endif

#define DAC_FIFO_SAMPLES        1024U
#define BT_SBC_PACKET_SIZE      595U
#define BT_SBC_DECODER_INPUT_LEN (8U * 1024U)
#define BT_SBC_LEVEL_HIGH       (BT_SBC_DECODER_INPUT_LEN - BT_SBC_PACKET_SIZE * 4U)
#define BT_SBC_LEVEL_LOW        (BT_SBC_PACKET_SIZE * 6U)
#define BT_SBC_LEVEL_START      (BT_SBC_LEVEL_HIGH - BT_SBC_PACKET_SIZE * 3U)
#define SBC_DECODER_FIFO_MIN    (119U * 2U)
#define BT_DECODED_BUFFER_SIZE  128U

extern BG_Audio_Io_Manager BG_AudioManager;

extern uint32_t AudioADC1Buf[1024];
extern uint32_t AudioADC2Buf[1024];
extern uint32_t DAC0_FIFO[DAC_FIFO_SAMPLES];
extern uint32_t DAC1_FIFO[DAC_FIFO_SAMPLES];
extern uint8_t  looper_flash_buffer[512];
extern uint32_t looper_playback_buffer[256];

extern uint32_t usb_speaker_enable;
extern uint32_t usb_mic_enable;

extern uint8_t  a2dp_sbcBuf[BT_SBC_DECODER_INPUT_LEN];
extern uint8_t  decoder_buf[1024 * 4];
extern uint8_t  DecoderInitialized;
extern MemHandle SBC_MemHandle;
extern ResamplerContext bt_resmaper;

extern uint32_t bt_decoded_buffer[BT_DECODED_BUFFER_SIZE];
extern uint16_t bt_decoded_len;
extern bool     bt_has_decoded_data;
extern uint32_t bt_current_sample_rate;
extern uint32_t system_default_sample_rate;

extern uint16_t s_bt_gain_q8;
extern uint16_t s_usb_gain_q8;
extern uint16_t s_usb_out_gain_q8;
extern uint8_t  s_usb_out_mute;
extern bool     s_usb_connected;

void BG_audio_Init(uint16_t SampleRate);
void Audio_loop(void);

void A2dp_DecoderInit(void);
void SaveDataToSbcBuffer(uint8_t *data, uint16_t dataLen);

void SetVolume(void);
void ProcessGuitarOutput(void);
void ProcessMicOutput(void);
void ProcessSpeakerSwitch(void);
void USB_HotplugCheck(void);

void AudioLoopWithGraph(void);

#if !USE_EFFECT_GRAPH_MODE
void AudioLoopWithBT(uint32_t *bt_audio_buffer);
void AudioLoopMinimal(uint32_t *bt_audio_buffer);
#endif

uint16_t ADC0_GetAvailableData(EffectNode_t *node);
uint16_t ADC1_GetAvailableData(EffectNode_t *node);
uint16_t USB_GetAvailableData(EffectNode_t *node);
uint16_t BT_GetAvailableData(EffectNode_t *node);
uint16_t ADC0_ReadGuitarData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
uint16_t ADC1_ReadMicData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
uint16_t USB_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
uint16_t BT_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
void DAC0_WriteSpeakerData(EffectNode_t *node, uint32_t *in_buf, uint16_t len);
void USB_WriteAudioData(EffectNode_t *node, uint32_t *in_buf, uint16_t len);

uint16_t Metronome_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
uint16_t Metronome_GetAvailCallback(EffectNode_t *node);
uint16_t Remind_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
uint16_t Remind_GetAvailCallback(EffectNode_t *node);
uint16_t LooperPlay_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
uint16_t LooperPlay_GetAvailCallback(EffectNode_t *node);
void LooperRecord_SinkCallback(EffectNode_t *node, uint32_t *in_buf, uint16_t len);

void ADC_Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Expander_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void DRC_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void EQ_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Passthrough_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Reverb_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BG_AUDIO_IO_INTERNAL_H__ */
