/**
 * @file bg_audio_io_core.c
 * @brief Manager instance and shared audio buffers.
 */
#include "bg_audio_io_internal.h"

uint32_t AudioADC1Buf[1024] = {0};
uint32_t AudioADC2Buf[1024] = {0};
uint32_t DAC0_FIFO[DAC_FIFO_SAMPLES];
uint32_t DAC1_FIFO[DAC_FIFO_SAMPLES];

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

uint32_t system_default_sample_rate = 44100;

uint16_t s_bt_gain_q8 = 256;
uint16_t s_usb_gain_q8 = 256;
uint16_t s_usb_out_gain_q8 = 256;
uint8_t  s_usb_out_mute = 0;

bool s_usb_connected = false;
