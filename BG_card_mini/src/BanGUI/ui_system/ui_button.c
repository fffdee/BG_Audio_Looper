/**
 * @file    ui_button.c
 * @brief   4-button input processing module implementation
 * @author  BG Card Team
 * @date    2025-12-18
 */

#include "ui_button.h"
#include "ui_config.h"
#include "gpio.h"
#include <string.h>

/*===========================================================================
 * GPIO mapping
 *===========================================================================*/

/* Button GPIO configuration */
typedef struct {
    uint32_t port_ie;       /* Input enable register */
    uint32_t port_in;       /* Input register */
    uint32_t port_pu;       /* Pull-up register */
    uint32_t pin;           /* Pin index */
} UI_ButtonGPIO_t;

static const UI_ButtonGPIO_t button_gpio[UI_BTN_COUNT] = {
    { GPIO_A_IE, GPIO_A_IN, GPIO_A_PU, GPIO_INDEX0  },  /* BTN_UP    - A0  */
    { GPIO_B_IE, GPIO_B_IN, GPIO_B_PU, GPIO_INDEX5  },  /* BTN_DOWN  - B5  */
    { GPIO_A_IE, GPIO_A_IN, GPIO_A_PU, GPIO_INDEX15 },  /* BTN_ENTER - A15 */
    { GPIO_A_IE, GPIO_A_IN, GPIO_A_PU, GPIO_INDEX16 },  /* BTN_BACK  - A16 */
};

static const char* button_names[UI_BTN_COUNT] = {
    "UP", "DOWN", "ENTER", "BACK"
};

/*===========================================================================
 * Private variables
 *===========================================================================*/

static UI_ButtonInfo_t button_info[UI_BTN_COUNT];
static UI_ButtonCallback_t event_callback = NULL;

/* Event queue */
#define EVENT_QUEUE_SIZE    8
static UI_ButtonEventData_t event_queue[EVENT_QUEUE_SIZE];
static uint8_t event_head = 0;
static uint8_t event_tail = 0;
static uint8_t event_count = 0;

/*===========================================================================
 * Private functions
 *===========================================================================*/

/**
 * @brief Read button GPIO state
 */
static uint8_t read_button_gpio(UI_ButtonID_t id)
{
    if (id >= UI_BTN_COUNT) return 1;
    return GPIO_RegOneBitGet(button_gpio[id].port_in, button_gpio[id].pin) ? 1 : 0;
}

/**
 * @brief Push event to queue
 */
static void push_event(UI_ButtonID_t id, UI_ButtonEvent_t event, uint16_t duration)
{
    UI_ButtonEventData_t evt;
    evt.id = id;
    evt.event = event;
    evt.press_duration = duration;
    /* Callback notification */
    if (event_callback) {
        event_callback(&evt);
    }
    /* Enqueue */
    if (event_count < EVENT_QUEUE_SIZE) {
        event_queue[event_tail] = evt;
        event_tail = (event_tail + 1) % EVENT_QUEUE_SIZE;
        event_count++;
    }
}

/*===========================================================================
 * API Implementation
 *===========================================================================*/

void UI_Button_Init(void)
{
    uint8_t i;
    /* Initialize button GPIO - input mode, internal pull-up */
    for (i = 0; i < UI_BTN_COUNT; i++) {
        /* Enable input */
        GPIO_RegOneBitSet(button_gpio[i].port_ie, button_gpio[i].pin);
        /* Disable output */
        if (button_gpio[i].port_ie == GPIO_A_IE) {
            GPIO_RegOneBitClear(GPIO_A_OE, button_gpio[i].pin);
        } else {
            GPIO_RegOneBitClear(GPIO_B_OE, button_gpio[i].pin);
        }
        /* Enable pull-up */
        GPIO_RegOneBitSet(button_gpio[i].port_pu, button_gpio[i].pin);
        /* Disable pull-down */
        if (button_gpio[i].port_ie == GPIO_A_IE) {
            GPIO_RegOneBitClear(GPIO_A_PD, button_gpio[i].pin);
        } else {
            GPIO_RegOneBitClear(GPIO_B_PD, button_gpio[i].pin);
        }
    }
    
    /* Initialize button states */
    memset(button_info, 0, sizeof(button_info));
    for (i = 0; i < UI_BTN_COUNT; i++) {
        button_info[i].raw_state = 1;  /* Default high level (not pressed) */
        button_info[i].state = UI_BTN_STATE_IDLE;
    }
    
    /* Clear event queue */
    event_head = 0;
    event_tail = 0;
    event_count = 0;
    event_callback = NULL;
}

void UI_Button_Scan(uint16_t delta_ms)
{
    uint8_t i;
    uint8_t current_raw;
    
    for (i = 0; i < UI_BTN_COUNT; i++) {
        UI_ButtonInfo_t* btn = &button_info[i];
        current_raw = read_button_gpio((UI_ButtonID_t)i);
        
        /* Debounce processing */
        if (current_raw != btn->raw_state) {
            btn->debounce_cnt++;
            if (btn->debounce_cnt >= (UI_BTN_DEBOUNCE_MS / delta_ms)) {
                btn->debounce_cnt = 0;
                btn->raw_state = current_raw;
                
                if (current_raw == 0) {
                    /* Button pressed (active low) */
                    btn->state = UI_BTN_STATE_PRESSED;
                    btn->press_time = 0;
                    btn->repeat_time = 0;
                    btn->long_press_fired = false;
                    push_event((UI_ButtonID_t)i, UI_BTN_EVENT_PRESSED, 0);
                } else {
                    /* Button released */
                    if (btn->state == UI_BTN_STATE_PRESSED) {
                        /* Short press release = click */
                        push_event((UI_ButtonID_t)i, UI_BTN_EVENT_CLICKED, btn->press_time);
                    }
                    push_event((UI_ButtonID_t)i, UI_BTN_EVENT_RELEASED, btn->press_time);
                    btn->state = UI_BTN_STATE_IDLE;
                    btn->press_time = 0;
                }
            }
        } else {
            btn->debounce_cnt = 0;
        }
        
        /* Long press and repeat press processing */
        if (btn->state == UI_BTN_STATE_PRESSED || btn->state == UI_BTN_STATE_LONG_PRESSED) {
            btn->press_time += delta_ms;
            
            /* Long press detection */
            if (!btn->long_press_fired && btn->press_time >= UI_BTN_LONG_PRESS_MS) {
                btn->long_press_fired = true;
                btn->state = UI_BTN_STATE_LONG_PRESSED;
                btn->repeat_time = 0;
                push_event((UI_ButtonID_t)i, UI_BTN_EVENT_LONG_PRESS, btn->press_time);
            }
            
            /* Repeat press processing */
            if (btn->state == UI_BTN_STATE_LONG_PRESSED) {
                btn->repeat_time += delta_ms;
                if (btn->repeat_time >= UI_BTN_REPEAT_MS) {
                    btn->repeat_time = 0;
                    push_event((UI_ButtonID_t)i, UI_BTN_EVENT_REPEAT, btn->press_time);
                }
            }
        }
    }
}

bool UI_Button_GetEvent(UI_ButtonEventData_t* event)
{
    if (event_count == 0 || event == NULL) {
        return false;
    }
    
    *event = event_queue[event_head];
    event_head = (event_head + 1) % EVENT_QUEUE_SIZE;
    event_count--;
    
    return true;
}

bool UI_Button_HasEvent(void)
{
    return (event_count > 0);
}

void UI_Button_ClearEvents(void)
{
    event_head = 0;
    event_tail = 0;
    event_count = 0;
}

bool UI_Button_IsPressed(UI_ButtonID_t id)
{
    if (id >= UI_BTN_COUNT) return false;
    return (button_info[id].state != UI_BTN_STATE_IDLE);
}

void UI_Button_SetCallback(UI_ButtonCallback_t callback)
{
    event_callback = callback;
}

const char* UI_Button_GetName(UI_ButtonID_t id)
{
    if (id >= UI_BTN_COUNT) return "UNKNOWN";
    return button_names[id];
}
