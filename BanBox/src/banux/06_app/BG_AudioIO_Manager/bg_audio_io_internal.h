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
/* SBC: AudioDecoderContext + inbuf(2560) + SBCContext(3856) + BufferContext + SongInfo ≈ 7KB+ */
#define BT_DECODER_BUF_SIZE     (8U * 1024U)

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
extern uint8_t  decoder_buf[BT_DECODER_BUF_SIZE];
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

/* ==================== 工作模式互斥（Loop 模式 / 自由模式） ====================
 *
 * Loop 模式：Looper 正在录音或播放。此时禁止 BT Audio / USB Audio 参与音频运行。
 * 原因：Looper 工作时主循环被压到 48 采样小帧(1ms)，而 BT(44.1k 大帧 + SBC 解码)
 *      与 USB(等时传输) 会与之争抢 CPU / 总线带宽，导致 Loop 播放卡顿。
 * 自由模式：Looper 空闲，BT / USB / ADC 均可用。
 *
 * 进入 Loop 模式：立即（Looper 一启动就关 BT/USB，保证 Loop 流畅）
 * 退出 Loop 模式：延迟确认（避免抖动导致 BT/USB 反复开关，比卡顿更糟）
 * ============================================================================ */
typedef enum {
	AUDIO_WORK_MODE_FREE = 0,   /* 自由模式：BT/USB/ADC 均可用 */
	AUDIO_WORK_MODE_LOOP = 1,   /* Loop 模式：Looper 工作中，禁用 BT/USB 音频 */
} AudioWorkMode_t;

/* Looper 是否正在工作（录音或播放中）：1=工作中 */
uint8_t BG_AudioLooperIsActive(void);
/* 更新工作模式状态机（主循环每轮调用一次） */
void BG_AudioWorkModeUpdate(void);
/* 获取当前生效的工作模式 */
AudioWorkMode_t BG_AudioGetWorkMode(void);
/* 模式互斥：1=允许 BT 音频, 0=禁止（Loop 模式） */
uint8_t BG_AudioBTAllowed(void);
/* 模式互斥：1=允许 USB 音频, 0=禁止（Loop 模式） */
uint8_t BG_AudioUSBAllowed(void);

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
