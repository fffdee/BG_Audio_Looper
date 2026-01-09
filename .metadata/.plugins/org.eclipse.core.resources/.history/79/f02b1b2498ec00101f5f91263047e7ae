/**
 * @file    audio_spectrum_simple.h
 * @brief   Simple audio spectrum analyzer for idle screen
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * Lightweight spectrum analyzer optimized for performance
 * - Smaller FFT (128 points instead of 512)
 * - Reduced bands (8 instead of 16)
 * - Optimized for idle screen white box display
 */

#ifndef AUDIO_SPECTRUM_SIMPLE_H
#define AUDIO_SPECTRUM_SIMPLE_H

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * Configuration
 *===========================================================================*/

#define SPECTRUM_FFT_SIZE       64     // Very small FFT for minimal performance impact
#define SPECTRUM_BANDS          8      // 8 frequency bands
#define SPECTRUM_MAX_HEIGHT     40     // Max bar height (46px box - 6px margin)
#define SPECTRUM_BAR_WIDTH      18     // Width of each bar (160 total / 8 = 20, -2 spacing)
#define SPECTRUM_BAR_SPACING    2      // Spacing between bars

/*===========================================================================
 * Data Structures
 *===========================================================================*/

typedef struct {
    int32_t fft_buffer[SPECTRUM_FFT_SIZE];  // FFT input/output buffer
    uint16_t bar_heights[SPECTRUM_BANDS];   // Current bar heights
    uint16_t peak_heights[SPECTRUM_BANDS];  // Peak hold heights
    uint8_t peak_decay[SPECTRUM_BANDS];     // Peak hold decay counters
    bool enabled;                            // Enable/disable flag
    uint16_t x;                              // Drawing X position
    uint16_t y;                              // Drawing Y position
    uint16_t width;                          // Drawing area width
    uint16_t height;                         // Drawing area height
} AudioSpectrum_Simple_t;

/*===========================================================================
 * Public Functions
 *===========================================================================*/

/**
 * @brief Initialize spectrum analyzer
 * @param spectrum Pointer to spectrum structure
 * @param x X position for drawing
 * @param y Y position for drawing
 * @param width Width of drawing area
 * @param height Height of drawing area
 */
void AudioSpectrum_Simple_Init(AudioSpectrum_Simple_t* spectrum,
                               uint16_t x, uint16_t y,
                               uint16_t width, uint16_t height);

/**
 * @brief Process audio samples and update spectrum
 * @param spectrum Pointer to spectrum structure
 * @param audio_left Left channel audio samples
 * @param audio_right Right channel audio samples
 * @param sample_count Number of samples
 * @note This function should be called from a background task, not audio interrupt!
 */
void AudioSpectrum_Simple_Process(AudioSpectrum_Simple_t* spectrum,
                                  const int16_t* audio_left,
                                  const int16_t* audio_right,
                                  uint16_t sample_count);

/**
 * @brief Draw spectrum on screen
 * @param spectrum Pointer to spectrum structure
 */
void AudioSpectrum_Simple_Draw(AudioSpectrum_Simple_t* spectrum);

/**
 * @brief Enable/disable spectrum display
 * @param spectrum Pointer to spectrum structure
 * @param enabled True to enable, false to disable
 */
void AudioSpectrum_Simple_SetEnabled(AudioSpectrum_Simple_t* spectrum, bool enabled);

/**
 * @brief Clear spectrum data
 * @param spectrum Pointer to spectrum structure
 */
void AudioSpectrum_Simple_Clear(AudioSpectrum_Simple_t* spectrum);

#endif // AUDIO_SPECTRUM_SIMPLE_H
