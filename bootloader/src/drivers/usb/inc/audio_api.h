#ifndef __AUDIO_API_H__
#define __AUDIO_API_H__

#include "type.h"

/**
 * Initialise DAC + ADC hardware for USB audio playback.
 * Must be called AFTER DMA_ChannelAllocTableSet() so that DMA channels 2/3
 * (DAC0/DAC1) are already visible to the allocator.
 *
 * @param SampleRate  e.g. 44100 or 48000
 */
void audio_init(uint32_t SampleRate);

/**
 * Per-loop audio pump: drains the USB speaker ring-buffer into DAC,
 * and feeds ADC data back to the USB mic ring-buffer.
 * Call every main-loop iteration ONLY when upgrade is NOT active.
 */
void audio_process(void);

#endif /* __AUDIO_API_H__ */
