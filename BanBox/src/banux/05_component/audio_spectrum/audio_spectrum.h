/**
 * @file    audio_spectrum.h
 * @brief   Audio spectrum analyzer using FFT
 * @author  BG Card Team
 * @date    2026-01-02
 */

#ifndef __AUDIO_SPECTRUM_H__
#define __AUDIO_SPECTRUM_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Configuration
 *===========================================================================*/
#define SPECTRUM_FFT_SIZE           512     /* FFT size (must be power of 2) */
#define SPECTRUM_BANDS              16      /* Number of frequency bands to display */
#define SPECTRUM_BAR_WIDTH          12      /* Width of each spectrum bar */
#define SPECTRUM_BAR_SPACING        2       /* Spacing between bars */
#define SPECTRUM_MAX_HEIGHT         50      /* Maximum bar height in pixels */
#define SPECTRUM_SMOOTH_FACTOR      0.7f    /* Smoothing factor (0.0-1.0) */

/*===========================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Spectrum display parameters
 */
typedef struct {
    uint16_t x;             /* Start X position */
    uint16_t y;             /* Start Y position */
    uint16_t width;         /* Total width */
    uint16_t height;        /* Total height */
    uint16_t bar_color;     /* Bar color (RGB565) */
    uint16_t bg_color;      /* Background color */
    bool mirror_mode;       /* Mirror mode (draw upside down too) */
} SpectrumDisplay_t;

/**
 * @brief Spectrum analyzer context
 */
typedef struct {
    int32_t fft_buffer[SPECTRUM_FFT_SIZE];      /* FFT input/output buffer */
    float magnitude[SPECTRUM_BANDS];            /* Magnitude for each band */
    float magnitude_smooth[SPECTRUM_BANDS];     /* Smoothed magnitude */
    uint16_t bar_heights[SPECTRUM_BANDS];       /* Current bar heights */
    uint16_t peak_heights[SPECTRUM_BANDS];      /* Peak hold heights */
    uint8_t peak_timers[SPECTRUM_BANDS];        /* Peak hold timers */
    SpectrumDisplay_t display;                  /* Display configuration */
    bool enabled;                                /* Spectrum enabled flag */
} AudioSpectrum_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize audio spectrum analyzer
 * @param spectrum Pointer to spectrum context
 * @param display Display configuration
 */
void AudioSpectrum_Init(AudioSpectrum_t* spectrum, const SpectrumDisplay_t* display);

/**
 * @brief Process audio data and update spectrum
 * @param spectrum Pointer to spectrum context
 * @param audio_data Audio PCM data (stereo, 16-bit)
 * @param length Number of samples (per channel)
 */
void AudioSpectrum_Process(AudioSpectrum_t* spectrum, int16_t* audio_data, uint16_t length);

/**
 * @brief Draw spectrum on LCD
 * @param spectrum Pointer to spectrum context
 */
void AudioSpectrum_Draw(AudioSpectrum_t* spectrum);

/**
 * @brief Enable/disable spectrum
 * @param spectrum Pointer to spectrum context
 * @param enabled Enable flag
 */
void AudioSpectrum_SetEnabled(AudioSpectrum_t* spectrum, bool enabled);

/**
 * @brief Clear spectrum display
 * @param spectrum Pointer to spectrum context
 */
void AudioSpectrum_Clear(AudioSpectrum_t* spectrum);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_SPECTRUM_H__ */
