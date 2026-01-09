/**
 * @file    ui_bootscreen.h
 * @brief   Boot screen module
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * Features:
 *   - Display Logo
 *   - Display product name/version
 *   - Display progress bar
 *   - Fade in/out effect
 */

#ifndef __UI_BOOTSCREEN_H__
#define __UI_BOOTSCREEN_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Configuration
 *===========================================================================*/

/* Boot screen stages */
typedef enum {
    UI_BOOT_STAGE_INIT = 0,     /* Initialization */
    UI_BOOT_STAGE_LOGO,         /* Display Logo */
    UI_BOOT_STAGE_INFO,         /* Display info */
    UI_BOOT_STAGE_PROGRESS,     /* Loading progress */
    UI_BOOT_STAGE_FADEOUT,      /* Fade out */
    UI_BOOT_STAGE_DONE,         /* Done */
} UI_BootStage_t;

/* Boot screen configuration */
typedef struct {
    const uint8_t* logo_data;       /* Logo image data (RGB565) */
    uint16_t logo_width;            /* Logo width */
    uint16_t logo_height;           /* Logo height */
    const char* product_name;       /* Product name */
    const char* version;            /* Version number */
    const char* copyright;          /* Copyright info */
    uint16_t display_time;          /* Display time (ms) */
    bool show_progress;             /* Whether to show progress bar */
} UI_BootConfig_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize boot screen
 * @param config Configuration parameter, NULL for default config
 */
void UI_BootScreen_Init(const UI_BootConfig_t* config);

/**
 * @brief Start displaying boot screen
 */
void UI_BootScreen_Start(void);

/**
 * @brief Update boot screen (called in main loop)
 * @param delta_ms Time interval (ms)
 * @return true to continue displaying, false if display is complete
 */
bool UI_BootScreen_Update(uint16_t delta_ms);

/**
 * @brief Set loading progress
 * @param progress Progress 0-100
 * @param message Progress message (optional)
 */
void UI_BootScreen_SetProgress(uint8_t progress, const char* message);

/**
 * @brief Skip boot screen
 */
void UI_BootScreen_Skip(void);

/**
 * @brief Check if boot screen is complete
 * @return true if complete
 */
bool UI_BootScreen_IsDone(void);

/**
 * @brief Get current stage
 * @return Current stage
 */
UI_BootStage_t UI_BootScreen_GetStage(void);

/**
 * @brief Set Logo data
 * @param data Image data (RGB565)
 * @param width Width
 * @param height Height
 */
void UI_BootScreen_SetLogo(const uint8_t* data, uint16_t width, uint16_t height);

/**
 * @brief Set product information
 * @param name Product name
 * @param version Version number
 */
void UI_BootScreen_SetProductInfo(const char* name, const char* version);

#ifdef __cplusplus
}
#endif

#endif /* __UI_BOOTSCREEN_H__ */
