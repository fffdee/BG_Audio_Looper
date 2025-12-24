/**
 * @file    ui_button.c
 * @brief   4按键输入处理模块实现
 * @author  BG Card Team
 * @date    2025-12-18
 */

#include "ui_button.h"
#include "ui_config.h"
#include "gpio.h"
#include <string.h>

/*===========================================================================
 * GPIO 映射
 *===========================================================================*/

/* 按键GPIO配置 */
typedef struct {
    uint32_t port_ie;       /* 输入使能寄存器 */
    uint32_t port_in;       /* 输入寄存器 */
    uint32_t port_pu;       /* 上拉寄存器 */
    uint32_t pin;           /* 引脚位 */
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
 * 私有变量
 *===========================================================================*/

static UI_ButtonInfo_t button_info[UI_BTN_COUNT];
static UI_ButtonCallback_t event_callback = NULL;

/* 事件队列 */
#define EVENT_QUEUE_SIZE    8
static UI_ButtonEventData_t event_queue[EVENT_QUEUE_SIZE];
static uint8_t event_head = 0;
static uint8_t event_tail = 0;
static uint8_t event_count = 0;

/*===========================================================================
 * 私有函数
 *===========================================================================*/

/**
 * @brief 读取按键GPIO状态
 */
static uint8_t read_button_gpio(UI_ButtonID_t id)
{
    if (id >= UI_BTN_COUNT) return 1;
    return GPIO_RegOneBitGet(button_gpio[id].port_in, button_gpio[id].pin) ? 1 : 0;
}

/**
 * @brief 推送事件到队列
 */
static void push_event(UI_ButtonID_t id, UI_ButtonEvent_t event, uint16_t duration)
{
    UI_ButtonEventData_t evt;
    evt.id = id;
    evt.event = event;
    evt.press_duration = duration;
    
    /* 回调通知 */
    if (event_callback) {
        event_callback(&evt);
    }
    
    /* 入队 */
    if (event_count < EVENT_QUEUE_SIZE) {
        event_queue[event_tail] = evt;
        event_tail = (event_tail + 1) % EVENT_QUEUE_SIZE;
        event_count++;
    }
}

/*===========================================================================
 * API 实现
 *===========================================================================*/

void UI_Button_Init(void)
{
    uint8_t i;
    
    /* 初始化按键GPIO - 输入模式，内部上拉 */
    for (i = 0; i < UI_BTN_COUNT; i++) {
        /* 使能输入 */
        GPIO_RegOneBitSet(button_gpio[i].port_ie, button_gpio[i].pin);
        /* 禁止输出 */
        if (button_gpio[i].port_ie == GPIO_A_IE) {
            GPIO_RegOneBitClear(GPIO_A_OE, button_gpio[i].pin);
        } else {
            GPIO_RegOneBitClear(GPIO_B_OE, button_gpio[i].pin);
        }
        /* 使能上拉 */
        GPIO_RegOneBitSet(button_gpio[i].port_pu, button_gpio[i].pin);
        /* 禁止下拉 */
        if (button_gpio[i].port_ie == GPIO_A_IE) {
            GPIO_RegOneBitClear(GPIO_A_PD, button_gpio[i].pin);
        } else {
            GPIO_RegOneBitClear(GPIO_B_PD, button_gpio[i].pin);
        }
    }
    
    /* 初始化按键状态 */
    memset(button_info, 0, sizeof(button_info));
    for (i = 0; i < UI_BTN_COUNT; i++) {
        button_info[i].raw_state = 1;  /* 默认高电平(未按下) */
        button_info[i].state = UI_BTN_STATE_IDLE;
    }
    
    /* 清空事件队列 */
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
        
        /* 去抖处理 */
        if (current_raw != btn->raw_state) {
            btn->debounce_cnt++;
            if (btn->debounce_cnt >= (UI_BTN_DEBOUNCE_MS / delta_ms)) {
                btn->debounce_cnt = 0;
                btn->raw_state = current_raw;
                
                if (current_raw == 0) {
                    /* 按键按下 (低电平有效) */
                    btn->state = UI_BTN_STATE_PRESSED;
                    btn->press_time = 0;
                    btn->repeat_time = 0;
                    btn->long_press_fired = false;
                    push_event((UI_ButtonID_t)i, UI_BTN_EVENT_PRESSED, 0);
                } else {
                    /* 按键释放 */
                    if (btn->state == UI_BTN_STATE_PRESSED) {
                        /* 短按释放 = 单击 */
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
        
        /* 长按和连按处理 */
        if (btn->state == UI_BTN_STATE_PRESSED || btn->state == UI_BTN_STATE_LONG_PRESSED) {
            btn->press_time += delta_ms;
            
            /* 长按检测 */
            if (!btn->long_press_fired && btn->press_time >= UI_BTN_LONG_PRESS_MS) {
                btn->long_press_fired = true;
                btn->state = UI_BTN_STATE_LONG_PRESSED;
                btn->repeat_time = 0;
                push_event((UI_ButtonID_t)i, UI_BTN_EVENT_LONG_PRESS, btn->press_time);
            }
            
            /* 连按处理 */
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
