/**
 * @file    ui_system.h
 * @brief   UI system main module - integrates menu, status bar, boot screen
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * Usage:
 *   1. UI_System_Init()       - Initialize
 *   2. UI_System_Start()      - Start (show boot screen)
 *   3. UI_System_Update()     - Call in main loop
 *   4. UI_System_HandleEvent()- Handle button events
 */

#ifndef __UI_SYSTEM_H__
#define __UI_SYSTEM_H__

#include <stdint.h>
#include <stdbool.h>
#include "ui_config.h"
#include "ui_button.h"
#include "ui_statusbar.h"
#include "ui_menu.h"
#include "ui_bootscreen.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * UI system states
 *===========================================================================*/

typedef enum {
    UI_STATE_BOOT = 0,          /* Boot screen */
    UI_STATE_IDLE,              /* Idle/Main interface */
    UI_STATE_MENU,              /* Menu interface */
    UI_STATE_PLAYER,            /* Player interface */
    UI_STATE_SETTINGS,          /* Settings interface */
    UI_STATE_POPUP,             /* Popup window */
} UI_SystemState_t;

/* UI system configuration */
typedef struct {
    bool skip_boot;             /* Skip boot screen */
    bool auto_statusbar;        /* Auto show status bar */
    uint16_t idle_timeout;      /* Idle timeout (seconds, 0=disable) */
} UI_SystemConfig_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize UI system
 * @param config Configuration parameter, NULL for default config
 */
void UI_System_Init(const UI_SystemConfig_t* config);

/**
 * @brief Start UI system (show boot screen)
 */
void UI_System_Start(void);

/**
 * @brief UI system update (call in main loop)
 * @param delta_ms Time interval (ms)
 */
void UI_System_Update(uint16_t delta_ms);

/**
 * @brief Handle button event
 * @param event Button event
 */
void UI_System_HandleEvent(UI_ButtonEventData_t* event);

/**
 * @brief Set system state
 * @param state Target state
 */
void UI_System_SetState(UI_SystemState_t state);

/**
 * @brief Get current state
 * @return Current state
 */
UI_SystemState_t UI_System_GetState(void);

/**
 * @brief Show main menu
 */
void UI_System_ShowMenu(void);

/**
 * @brief Hide menu, return to main interface
 */
void UI_System_HideMenu(void);

/**
 * @brief Show popup message
 * @param title Title
 * @param message Message content
 * @param duration_ms Display time (ms), 0=until button close
 */
void UI_System_ShowPopup(const char* title, const char* message, uint16_t duration_ms);

/**
 * @brief Close popup window
 */
void UI_System_ClosePopup(void);

/**
 * @brief Refresh the entire interface
 */
void UI_System_Refresh(void);

/**
 * @brief Set main menu
 * @param menu Pointer to main menu
 */
void UI_System_SetMainMenu(UI_Menu_t* menu);

/**
 * @brief Get main menu
 * @return Pointer to main menu
 */
UI_Menu_t* UI_System_GetMainMenu(void);

/**
 * @brief Check if boot is complete
 * @return true if boot is complete
 */
bool UI_System_IsReady(void);

/*===========================================================================
 * Default menu declaration (defined externally)
 *===========================================================================*/

/* Default menu structure defined in ui_menu_def.c */
extern UI_Menu_t* UI_GetDefaultMainMenu(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_SYSTEM_H__ */
