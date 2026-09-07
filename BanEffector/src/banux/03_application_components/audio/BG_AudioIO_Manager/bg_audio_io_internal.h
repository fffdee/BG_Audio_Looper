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
/* SBC: AudioDecoderContext + inbuf(2560) + SBCContext(3856) + BufferContext + SongInfo ≈ 7KB+ */
#define BT_DECODER_BUF_SIZE     (8U * 1024U)

extern BG_Audio_Io_Manager BG_AudioManager;

extern uint32_t AudioADC1Buf[1024];
extern uint32_t AudioADC2Buf[1024];
extern uint32_t DAC0_FIFO[DAC_FIFO_SAMPLES];
extern uint32_t DAC1_FIFO[DAC_FIFO_SAMPLES];

extern uint32_t usb_speaker_enable;
extern uint32_t usb_mic_enable;

extern uint32_t system_default_sample_rate;

extern uint16_t s_bt_gain_q8;
extern uint16_t s_usb_gain_q8;
extern uint16_t s_usb_out_gain_q8;
extern uint8_t  s_usb_out_mute;
extern bool     s_usb_connected;

void BG_audio_Init(uint16_t SampleRate);
void Audio_loop(void);

/* USB 音频始终允许（Looper 已移除，无模式互斥） */
uint8_t BG_AudioUSBAllowed(void);

void SetVolume(void);
void ProcessGuitarOutput(void);
void ProcessMicOutput(void);
void ProcessSpeakerSwitch(void);
void USB_HotplugCheck(void);

void AudioLoopWithGraph(void);

uint16_t ADC0_GetAvailableData(EffectNode_t *node);
uint16_t ADC1_GetAvailableData(EffectNode_t *node);
uint16_t USB_GetAvailableData(EffectNode_t *node);
uint16_t ADC0_ReadGuitarData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
uint16_t ADC1_ReadMicData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
uint16_t USB_ReadAudioData(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);
void DAC0_WriteSpeakerData(EffectNode_t *node, uint32_t *in_buf, uint16_t len);
void USB_WriteAudioData(EffectNode_t *node, uint32_t *in_buf, uint16_t len);

void ADC_Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Mixer_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Expander_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void DRC_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void EQ_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Passthrough_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Reverb_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Delay_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Chorus_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Distortion_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);
void Sustain_Process(EffectNode_t *node, uint32_t **in_bufs, uint8_t in_count, uint32_t *out_buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BG_AUDIO_IO_INTERNAL_H__ */
