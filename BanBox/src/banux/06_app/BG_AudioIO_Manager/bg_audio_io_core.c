/**
 * @file bg_audio_io_core.c
 * @brief Manager instance and shared audio/BT buffers.
 */
#include "bg_audio_io_internal.h"

uint32_t AudioADC1Buf[1024] = {0};
uint32_t AudioADC2Buf[1024] = {0};
uint32_t DAC0_FIFO[DAC_FIFO_SAMPLES];
uint32_t DAC1_FIFO[DAC_FIFO_SAMPLES];
uint8_t  looper_flash_buffer[512];
uint32_t looper_playback_buffer[256];

extern uint32_t usb_speaker_enable;
extern uint32_t usb_mic_enable;

BG_Audio_Io_Manager BG_AudioManager __attribute__((section(".data"))) = {
	.Audio_Init = BG_audio_Init,
	.Audio_Loop = Audio_loop,
	.Audio_data = {
		.guitar_count = 0,
		.mic_count = 0,
		.det_state = NONE,
	},
};

uint8_t a2dp_sbcBuf[BT_SBC_DECODER_INPUT_LEN];
uint8_t decoder_buf[1024 * 4] = {0};
uint8_t DecoderInitialized = 0;

MemHandle SBC_MemHandle;
ResamplerContext bt_resmaper;

uint32_t bt_decoded_buffer[BT_DECODED_BUFFER_SIZE];
uint16_t bt_decoded_len = 0;
bool     bt_has_decoded_data = false;
uint32_t bt_current_sample_rate = 0;
uint32_t system_default_sample_rate = 44100;

uint16_t s_bt_gain_q8 = 256;
uint16_t s_usb_gain_q8 = 256;
uint16_t s_usb_out_gain_q8 = 256;
uint8_t  s_usb_out_mute = 0;

bool s_usb_connected = false;
