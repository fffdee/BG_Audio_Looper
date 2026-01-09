/**
 * @file    audio_spectrum_simple.c
 * @brief   Simple audio spectrum analyzer implementation
 * @author  BG Card Team
 * @date    2025-12-18
 */

#include "audio_spectrum_simple.h"
#include "bg_lcd.h"
#include "framebuffer.h"
#include <string.h>
#include <math.h>

#define USE_FRAME_BUFFER
// External FFT function from SDK
extern void rfft_api(int32_t *data, int N, int ifft);

/*===========================================================================
 * Color Definitions
 *===========================================================================*/

// RGB565 gradient colors (green -> yellow -> red)
static const uint16_t spectrum_colors[] = {
    0x07E0, // Green (low)
    0x87E0, // Light green
    0xFFE0, // Yellow (mid)
    0xFDA0, // Orange
    0xFB20, // Orange-red
    0xF900, // Dark red
    0xF800  // Red (high)
};

/*===========================================================================
 * Private Functions (currently unused but kept for future reference)
 *===========================================================================*/

#if 0  /* Unused functions - disabled to avoid compiler warnings */
/**
 * @brief Map FFT bin to frequency band (8 bands)
 * Frequency range: ~172Hz to 24kHz at 44.1kHz sample rate
 */
static uint8_t get_band_index(uint16_t bin)
{
    // Logarithmic band mapping for 8 bands
    // Band 0: 172-344 Hz
    // Band 1: 344-688 Hz
    // Band 2: 688-1376 Hz
    // Band 3: 1376-2752 Hz
    // Band 4: 2752-5500 Hz
    // Band 5: 5500-11000 Hz
    // Band 6: 11000-16500 Hz
    // Band 7: 16500-24000 Hz
    
    if (bin < 2) return 0;
    if (bin < 4) return 1;
    if (bin < 8) return 2;
    if (bin < 16) return 3;
    if (bin < 32) return 4;
    if (bin < 64) return 5;
    if (bin < 96) return 6;
    return 7;
}

/**
 * @brief Get gradient color based on intensity (0-255)
 */
static uint16_t get_gradient_color(uint8_t intensity)
{
    if (intensity < 36) return spectrum_colors[0];      // Green
    if (intensity < 72) return spectrum_colors[1];      // Light green
    if (intensity < 108) return spectrum_colors[2];     // Yellow
    if (intensity < 144) return spectrum_colors[3];     // Orange
    if (intensity < 180) return spectrum_colors[4];     // Orange-red
    if (intensity < 216) return spectrum_colors[5];     // Dark red
    return spectrum_colors[6];                          // Red
}
#endif  /* Unused functions */

/*===========================================================================
 * Public Functions
 *===========================================================================*/

void AudioSpectrum_Simple_Init(AudioSpectrum_Simple_t* spectrum,
                               uint16_t x, uint16_t y,
                               uint16_t width, uint16_t height)
{
    if (spectrum == NULL) return;
    
    memset(spectrum, 0, sizeof(AudioSpectrum_Simple_t));
    spectrum->enabled = true;
    spectrum->x = x;
    spectrum->y = y;
    spectrum->width = width;
    spectrum->height = height;
}

void AudioSpectrum_Simple_Process(AudioSpectrum_Simple_t* spectrum,
                                  const int16_t* audio_left,
                                  const int16_t* audio_right,
                                  uint16_t sample_count)
{
    uint16_t i;
    uint8_t band;
    uint16_t samples_to_copy;
    uint32_t band_magnitude[SPECTRUM_BANDS];
    uint16_t band_count[SPECTRUM_BANDS];
    int32_t real, imag;
    uint32_t mag;
    uint32_t avg_mag;
    uint16_t new_height;
    uint32_t max_input;
    
    if (spectrum == NULL || !spectrum->enabled) return;
    if (audio_left == NULL) return;
    
    /* Initialize arrays */
    for (i = 0; i < SPECTRUM_BANDS; i++) {
        band_magnitude[i] = 0;
        band_count[i] = 0;
    }
    
    /* Convert stereo to mono and fill FFT buffer */
    samples_to_copy = (sample_count < SPECTRUM_FFT_SIZE) ? 
                      sample_count : SPECTRUM_FFT_SIZE;
    
    max_input = 0;
    for (i = 0; i < samples_to_copy; i++) {
        int32_t sample = (int32_t)audio_left[i];
        if (audio_right != NULL) {
            sample = (sample + (int32_t)audio_right[i]) / 2;
        }
        spectrum->fft_buffer[i] = sample;
        
        /* Track max for debug */
        if (sample < 0) sample = -sample;
        if ((uint32_t)sample > max_input) max_input = (uint32_t)sample;
    }
    
    /* Zero-pad if needed */
    for (i = samples_to_copy; i < SPECTRUM_FFT_SIZE; i++) {
        spectrum->fft_buffer[i] = 0;
    }
    
    /* Debug: check if we have audio data */
    if (max_input < 100) {
        /* Audio too quiet or no audio, decay bars quickly */
        for (i = 0; i < SPECTRUM_BANDS; i++) {
            if (spectrum->bar_heights[i] > 1) {
                spectrum->bar_heights[i] -= 2;  /* Faster decay */
            } else if (spectrum->bar_heights[i] > 0) {
                spectrum->bar_heights[i]--;
            }
        }
        return;
    }
    
    /* Perform FFT - this is the heavy operation */
    rfft_api(spectrum->fft_buffer, SPECTRUM_FFT_SIZE, 0);
    
    /* Process FFT bins - optimized for speed */
    /* 64 point FFT gives 32 usable bins, process only relevant ones */
    for (i = 1; i < SPECTRUM_FFT_SIZE / 2; i++) {
        real = spectrum->fft_buffer[i];
        imag = spectrum->fft_buffer[SPECTRUM_FFT_SIZE - i];
        
        /* Fast magnitude calculation using integer math */
        /* Avoid multiplication overflow by shifting first */
        real = real >> 4;
        imag = imag >> 4;
        mag = (uint32_t)(real * real + imag * imag);  /* No additional shift needed */
        
        /* Map to band using simpler formula for 32 bins */
        if (i < 2) band = 0;
        else if (i < 3) band = 1;
        else if (i < 5) band = 2;
        else if (i < 8) band = 3;
        else if (i < 12) band = 4;
        else if (i < 18) band = 5;
        else if (i < 25) band = 6;
        else band = 7;
        
        if (band < SPECTRUM_BANDS) {
            band_magnitude[band] += mag;
            band_count[band]++;
        }
    }
    
    /* Calculate bar heights - optimized for speed */
    for (i = 0; i < SPECTRUM_BANDS; i++) {
        if (band_count[i] > 0) {
            /* Fast average using bit shift instead of division where possible */
            if (band_count[i] == 1) {
                avg_mag = band_magnitude[i];
            } else if (band_count[i] == 2) {
                avg_mag = band_magnitude[i] >> 1;  /* Divide by 2 */
            } else if (band_count[i] <= 4) {
                avg_mag = band_magnitude[i] >> 2;  /* Divide by 4 */
            } else {
                avg_mag = band_magnitude[i] / band_count[i];  /* General case */
            }
            
            /* Very aggressive scaling: shift by 14 */
            new_height = (uint16_t)(avg_mag >> 14);
            
            /* Apply soft limit at 75% of max height */
            if (new_height > (SPECTRUM_MAX_HEIGHT * 3 / 4)) {
                uint16_t excess = new_height - (SPECTRUM_MAX_HEIGHT * 3 / 4);
                new_height = (SPECTRUM_MAX_HEIGHT * 3 / 4) + (excess >> 2);  /* Use shift for /4 */
            }
            
            if (new_height > SPECTRUM_MAX_HEIGHT) {
                new_height = SPECTRUM_MAX_HEIGHT;
            }
            
            /* Fast smoothing using bit operations: (old*3 + new*7) / 10 ≈ (old*3 + new*8) >> 3 */
            spectrum->bar_heights[i] = ((spectrum->bar_heights[i] * 3) + (new_height << 3)) >> 4;
        } else {
            /* Decay if no data for this band */
            if (spectrum->bar_heights[i] > 0) {
                spectrum->bar_heights[i]--;
            }
        }
        
        /* Update peak hold */
        if (spectrum->bar_heights[i] >= spectrum->peak_heights[i]) {
            spectrum->peak_heights[i] = spectrum->bar_heights[i];
            spectrum->peak_decay[i] = 15;
        } else if (spectrum->peak_decay[i] > 0) {
            spectrum->peak_decay[i]--;
        } else {
            if (spectrum->peak_heights[i] > 0) {
                spectrum->peak_heights[i]--;
            }
        }
    }
}

void AudioSpectrum_Simple_Draw(AudioSpectrum_Simple_t* spectrum)
{
    uint8_t i, j;
    uint16_t bar_x, bar_height, bar_y;
    uint16_t peak_y;
    uint16_t color;
    uint16_t y;
    uint16_t* fb;
    uint16_t fb_width;
    
    if (spectrum == NULL || !spectrum->enabled) return;
    
#ifdef USE_FRAME_BUFFER
    /* Use frame buffer for fast batch rendering */
    fb = FrameBuffer_GetBuffer();
    if (fb == NULL) {
        /* Fallback to direct LCD if framebuffer not available */
        goto use_lcd_fallback;
    }
    fb_width = FB_WIDTH;
    
    /* Clear entire spectrum area in frame buffer (one operation) */
    for (y = 0; y < spectrum->height; y++) {
        uint16_t screen_y = spectrum->y + y;
        if (screen_y >= FB_HEIGHT) continue;
        uint16_t* row_ptr = fb + screen_y * fb_width + spectrum->x;
        for (j = 0; j < spectrum->width && (spectrum->x + j) < fb_width; j++) {
            row_ptr[j] = 0x0000;  /* Black background */
        }
    }
    
    /* Draw all 8 bars directly to frame buffer */
    for (i = 0; i < SPECTRUM_BANDS; i++) {
        uint16_t bar_x_offset = i * (SPECTRUM_BAR_WIDTH + SPECTRUM_BAR_SPACING) + 3;
        bar_x = spectrum->x + bar_x_offset;
        bar_height = spectrum->bar_heights[i];
        
        if (bar_height == 0) continue;
        if (bar_x >= fb_width) continue;
        
        bar_y = spectrum->y + spectrum->height - bar_height;
        
        /* Draw bar pixels directly to frame buffer with gradient colors */
        for (y = 0; y < bar_height; y++) {
            uint16_t pixel_y = bar_y + y;
            if (pixel_y >= FB_HEIGHT) break;
            uint16_t height_from_bottom = y;
            
            /* Determine color based on height (gradient from green to red) */
            if (height_from_bottom >= 34) {
                color = 0xF800;  /* Red */
            } else if (height_from_bottom >= 28) {
                color = 0xF900;  /* Dark red */
            } else if (height_from_bottom >= 22) {
                color = 0xFB20;  /* Orange-red */
            } else if (height_from_bottom >= 16) {
                color = 0xFDA0;  /* Orange */
            } else if (height_from_bottom >= 10) {
                color = 0xFFE0;  /* Yellow */
            } else if (height_from_bottom >= 5) {
                color = 0x87E0;  /* Light green */
            } else {
                color = 0x07E0;  /* Green */
            }
            
            /* Draw horizontal line of bar width directly to frame buffer */
            uint16_t* pixel_ptr = fb + pixel_y * fb_width + bar_x;
            for (j = 0; j < SPECTRUM_BAR_WIDTH && (bar_x + j) < fb_width; j++) {
                pixel_ptr[j] = color;
            }
        }
        
        /* Draw peak indicator (2 pixels tall bright white line) */
        if (spectrum->peak_heights[i] > 1) {
            peak_y = spectrum->y + spectrum->height - spectrum->peak_heights[i];
            if (peak_y < FB_HEIGHT) {
                uint16_t* peak_ptr = fb + peak_y * fb_width + bar_x;
                for (j = 0; j < SPECTRUM_BAR_WIDTH && (bar_x + j) < fb_width; j++) {
                    peak_ptr[j] = 0xFFFF;  /* White */
                    if (peak_y + 1 < FB_HEIGHT && peak_y + 1 < spectrum->y + spectrum->height) {
                        peak_ptr[fb_width + j] = 0xFFFF;  /* Second row */
                    }
                }
            }
        }
    }
    
    /* Mark spectrum area as dirty for flush */
    FrameBuffer_MarkDirty(spectrum->x, spectrum->y, spectrum->width, spectrum->height);
    return;
    
use_lcd_fallback:
    /* If framebuffer failed, use direct LCD drawing */
#endif
    
    /* Fallback: original box-by-box drawing (slower) */
    for (i = 0; i < SPECTRUM_BANDS; i++) {
        bar_x = spectrum->x + i * (SPECTRUM_BAR_WIDTH + SPECTRUM_BAR_SPACING) + 3;
        bar_height = spectrum->bar_heights[i];
        uint16_t current_y;
        uint16_t segment_height;
        
        /* Clear this bar's column */
        BG_lcd.Box(bar_x, spectrum->y, SPECTRUM_BAR_WIDTH, spectrum->height, 0x0000);
        
        if (bar_height == 0) continue;
        
        bar_y = spectrum->y + spectrum->height - bar_height;
        current_y = bar_y;
        
        /* Draw color segments */
        if (bar_height > 34) {
            segment_height = bar_height - 34;
            BG_lcd.Box(bar_x, current_y, SPECTRUM_BAR_WIDTH, segment_height, 0xF800);
            current_y += segment_height;
        }
        if (bar_height > 28) {
            segment_height = (bar_height > 34) ? 6 : (bar_height - 28);
            BG_lcd.Box(bar_x, current_y, SPECTRUM_BAR_WIDTH, segment_height, 0xF900);
            current_y += segment_height;
        }
        if (bar_height > 22) {
            segment_height = (bar_height > 28) ? 6 : (bar_height - 22);
            BG_lcd.Box(bar_x, current_y, SPECTRUM_BAR_WIDTH, segment_height, 0xFB20);
            current_y += segment_height;
        }
        if (bar_height > 16) {
            segment_height = (bar_height > 22) ? 6 : (bar_height - 16);
            BG_lcd.Box(bar_x, current_y, SPECTRUM_BAR_WIDTH, segment_height, 0xFDA0);
            current_y += segment_height;
        }
        if (bar_height > 10) {
            segment_height = (bar_height > 16) ? 6 : (bar_height - 10);
            BG_lcd.Box(bar_x, current_y, SPECTRUM_BAR_WIDTH, segment_height, 0xFFE0);
            current_y += segment_height;
        }
        if (bar_height > 5) {
            segment_height = (bar_height > 10) ? 5 : (bar_height - 5);
            BG_lcd.Box(bar_x, current_y, SPECTRUM_BAR_WIDTH, segment_height, 0x87E0);
            current_y += segment_height;
        }
        if (bar_height > 0) {
            segment_height = (bar_height > 5) ? 5 : bar_height;
            BG_lcd.Box(bar_x, current_y, SPECTRUM_BAR_WIDTH, segment_height, 0x07E0);
        }
        
        /* Draw peak indicator */
        if (spectrum->peak_heights[i] > 1) {
            peak_y = spectrum->y + spectrum->height - spectrum->peak_heights[i];
            BG_lcd.Box(bar_x, peak_y, SPECTRUM_BAR_WIDTH, 2, 0xFFFF);
        }
    }
}

void AudioSpectrum_Simple_SetEnabled(AudioSpectrum_Simple_t* spectrum, bool enabled)
{
    if (spectrum == NULL) return;
    spectrum->enabled = enabled;
    
    if (!enabled) {
        AudioSpectrum_Simple_Clear(spectrum);
    }
}

void AudioSpectrum_Simple_Clear(AudioSpectrum_Simple_t* spectrum)
{
    if (spectrum == NULL) return;
    
    memset(spectrum->bar_heights, 0, sizeof(spectrum->bar_heights));
    memset(spectrum->peak_heights, 0, sizeof(spectrum->peak_heights));
    memset(spectrum->peak_decay, 0, sizeof(spectrum->peak_decay));
    memset(spectrum->fft_buffer, 0, sizeof(spectrum->fft_buffer));
}
