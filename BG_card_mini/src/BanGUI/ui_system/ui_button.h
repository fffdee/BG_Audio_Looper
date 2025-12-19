/**
 * @file    ui_button.h
 * @brief   4按键输入处理模块
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * 按键映射:
 *   BTN_UP     - 向上/增加
 *   BTN_DOWN   - 向下/减少
 *   BTN_ENTER  - 确认/进入
 *   BTN_BACK   - 返回/取消
 */

#ifndef __UI_BUTTON_H__
#define __UI_BUTTON_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 按键定义
 *===========================================================================*/

/* 按键ID */
typedef enum {
    UI_BTN_UP = 0,      /* 向上 - GPIO_A0 */
    UI_BTN_DOWN,        /* 向下 - GPIO_B5 */
    UI_BTN_ENTER,       /* 确认 - GPIO_A15 */
    UI_BTN_BACK,        /* 返回 - GPIO_A16 */
    UI_BTN_COUNT        /* 按键总数 */
} UI_ButtonID_t;

/* 按键事件类型 */
typedef enum {
    UI_BTN_EVENT_NONE = 0,      /* 无事件 */
    UI_BTN_EVENT_PRESSED,       /* 按下 */
    UI_BTN_EVENT_RELEASED,      /* 释放 */
    UI_BTN_EVENT_CLICKED,       /* 单击(按下并释放) */
    UI_BTN_EVENT_LONG_PRESS,    /* 长按 */
    UI_BTN_EVENT_REPEAT,        /* 连按(长按后持续触发) */
} UI_ButtonEvent_t;

/* 按键状态 */
typedef enum {
    UI_BTN_STATE_IDLE = 0,      /* 空闲 */
    UI_BTN_STATE_PRESSED,       /* 按下中 */
    UI_BTN_STATE_LONG_PRESSED,  /* 长按中 */
} UI_ButtonState_t;

/* 单个按键信息 */
typedef struct {
    UI_ButtonState_t state;     /* 当前状态 */
    uint8_t raw_state;          /* 原始GPIO状态 */
    uint8_t debounce_cnt;       /* 去抖计数 */
    uint16_t press_time;        /* 按下持续时间(ms) */
    uint16_t repeat_time;       /* 连按计时(ms) */
    bool long_press_fired;      /* 长按事件已触发 */
} UI_ButtonInfo_t;

/* 按键事件结构 */
typedef struct {
    UI_ButtonID_t id;           /* 按键ID */
    UI_ButtonEvent_t event;     /* 事件类型 */
    uint16_t press_duration;    /* 按下持续时间 */
} UI_ButtonEventData_t;

/* 按键事件回调函数 */
typedef void (*UI_ButtonCallback_t)(UI_ButtonEventData_t* event);

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化按键模块
 */
void UI_Button_Init(void);

/**
 * @brief 按键扫描处理 (在定时器或主循环中调用)
 * @param delta_ms 距上次调用的时间间隔(ms)
 */
void UI_Button_Scan(uint16_t delta_ms);

/**
 * @brief 获取按键事件 (非阻塞)
 * @param event 事件输出
 * @return true有事件, false无事件
 */
bool UI_Button_GetEvent(UI_ButtonEventData_t* event);

/**
 * @brief 检查是否有待处理事件
 * @return true有事件
 */
bool UI_Button_HasEvent(void);

/**
 * @brief 清空所有待处理事件
 */
void UI_Button_ClearEvents(void);

/**
 * @brief 检查按键是否按下
 * @param id 按键ID
 * @return true按下中
 */
bool UI_Button_IsPressed(UI_ButtonID_t id);

/**
 * @brief 设置按键事件回调
 * @param callback 回调函数
 */
void UI_Button_SetCallback(UI_ButtonCallback_t callback);

/**
 * @brief 获取按键名称
 * @param id 按键ID
 * @return 按键名称字符串
 */
const char* UI_Button_GetName(UI_ButtonID_t id);

/*===========================================================================
 * 便捷宏
 *===========================================================================*/

/* 快速检测单击事件 */
#define UI_BTN_IS_CLICK(e, btn)     ((e)->id == (btn) && (e)->event == UI_BTN_EVENT_CLICKED)
#define UI_BTN_IS_LONG(e, btn)      ((e)->id == (btn) && (e)->event == UI_BTN_EVENT_LONG_PRESS)
#define UI_BTN_IS_REPEAT(e, btn)    ((e)->id == (btn) && (e)->event == UI_BTN_EVENT_REPEAT)

#ifdef __cplusplus
}
#endif

#endif /* __UI_BUTTON_H__ */
