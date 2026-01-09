/**
 * @file    ui_button.h
 * @brief   4-button input processing module
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * Button mapping:
 *   BTN_UP     - Up/Increase
 *   BTN_DOWN   - Down/Decrease
 *   BTN_ENTER  - Confirm/Enter
 *   BTN_BACK   - Back/Cancel
 */

#ifndef __UI_BUTTON_H__
#define __UI_BUTTON_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Button definitions
 *===========================================================================*/

/* Button ID */
typedef enum {
    UI_BTN_UP = 0,      /* Up - GPIO_A0 */
    UI_BTN_DOWN,        /* Down - GPIO_B5 */
    UI_BTN_ENTER,       /* Confirm - GPIO_A15 */
    UI_BTN_BACK,        /* Back - GPIO_A16 */
    UI_BTN_COUNT        /* Total button count */
} UI_ButtonID_t;

/* Button event type */
typedef enum {
    UI_BTN_EVENT_NONE = 0,      /* No event */
    UI_BTN_EVENT_PRESSED,       /* Pressed */
    UI_BTN_EVENT_RELEASED,      /* Released */
    UI_BTN_EVENT_CLICKED,       /* Clicked (press and release) */
    UI_BTN_EVENT_LONG_PRESS,    /* Long press */
    UI_BTN_EVENT_REPEAT,        /* Repeat (triggered after long press) */
} UI_ButtonEvent_t;

/* Button state */
typedef enum {
    UI_BTN_STATE_IDLE = 0,      /* Idle */
    UI_BTN_STATE_PRESSED,       /* Pressing */
    UI_BTN_STATE_LONG_PRESSED,  /* Long pressing */
} UI_ButtonState_t;

/* Single button info */
typedef struct {
    UI_ButtonState_t state;     /* Current state */
    uint8_t raw_state;          /* Raw GPIO state */
    uint8_t debounce_cnt;       /* Debounce counter */
    uint16_t press_time;        /* Press duration (ms) */
    uint16_t repeat_time;       /* Repeat timer (ms) */
    bool long_press_fired;      /* Long press event triggered */
} UI_ButtonInfo_t;

/* Button event structure */
typedef struct {
    UI_ButtonID_t id;           /* Button ID */
    UI_ButtonEvent_t event;     /* Event type */
    uint16_t press_duration;    /* Press duration */
} UI_ButtonEventData_t;

/* Button event callback function */
typedef void (*UI_ButtonCallback_t)(UI_ButtonEventData_t* event);

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize button module
 */
void UI_Button_Init(void);

/**
 * @brief Button scan processing (call in timer or main loop)
 * @param delta_ms Time since last call (ms)
 */
void UI_Button_Scan(uint16_t delta_ms);

/**
 * @brief Get button event (non-blocking)
 * @param event Event output
 * @return true if event, false if no event
 */
bool UI_Button_GetEvent(UI_ButtonEventData_t* event);

/**
 * @brief Check if there are pending events
 * @return true if event exists
 */
bool UI_Button_HasEvent(void);

/**
 * @brief Clear all pending events
 */
void UI_Button_ClearEvents(void);

/**
 * @brief Check if button is pressed
 * @param id Button ID
 * @return true if pressed
 */
bool UI_Button_IsPressed(UI_ButtonID_t id);

/**
 * @brief Set button event callback
 * @param callback Callback function
 */
void UI_Button_SetCallback(UI_ButtonCallback_t callback);

/**
 * @brief Get button name
 * @param id Button ID
 * @return Button name string
 */
const char* UI_Button_GetName(UI_ButtonID_t id);

/*===========================================================================
 * Convenience macros
 *===========================================================================*/

/* Quick check for click event */
#define UI_BTN_IS_CLICK(e, btn)     ((e)->id == (btn) && (e)->event == UI_BTN_EVENT_CLICKED)
#define UI_BTN_IS_LONG(e, btn)      ((e)->id == (btn) && (e)->event == UI_BTN_EVENT_LONG_PRESS)
#define UI_BTN_IS_REPEAT(e, btn)    ((e)->id == (btn) && (e)->event == UI_BTN_EVENT_REPEAT)

#ifdef __cplusplus
}
#endif

#endif /* __UI_BUTTON_H__ */
