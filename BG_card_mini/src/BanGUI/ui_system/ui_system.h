/**
 * @file    ui_system.h
 * @brief   UI系统主模块 - 集成菜单、状态栏、开机画面
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * 使用方法:
 *   1. UI_System_Init()       - 初始化
 *   2. UI_System_Start()      - 启动(显示开机画面)
 *   3. UI_System_Update()     - 主循环中调用
 *   4. UI_System_HandleEvent()- 处理按键事件
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
 * UI 系统状态
 *===========================================================================*/

typedef enum {
    UI_STATE_BOOT = 0,          /* 开机画面 */
    UI_STATE_IDLE,              /* 空闲/主界面 */
    UI_STATE_MENU,              /* 菜单界面 */
    UI_STATE_PLAYER,            /* 播放器界面 */
    UI_STATE_SETTINGS,          /* 设置界面 */
    UI_STATE_POPUP,             /* 弹出框 */
} UI_SystemState_t;

/* UI系统配置 */
typedef struct {
    bool skip_boot;             /* 跳过开机画面 */
    bool auto_statusbar;        /* 自动显示状态栏 */
    uint16_t idle_timeout;      /* 空闲超时(秒,0=禁用) */
} UI_SystemConfig_t;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化UI系统
 * @param config 配置参数，NULL使用默认配置
 */
void UI_System_Init(const UI_SystemConfig_t* config);

/**
 * @brief 启动UI系统 (显示开机画面)
 */
void UI_System_Start(void);

/**
 * @brief UI系统更新 (在主循环中调用)
 * @param delta_ms 时间间隔(ms)
 */
void UI_System_Update(uint16_t delta_ms);

/**
 * @brief 处理按键事件
 * @param event 按键事件
 */
void UI_System_HandleEvent(UI_ButtonEventData_t* event);

/**
 * @brief 设置系统状态
 * @param state 目标状态
 */
void UI_System_SetState(UI_SystemState_t state);

/**
 * @brief 获取当前状态
 * @return 当前状态
 */
UI_SystemState_t UI_System_GetState(void);

/**
 * @brief 显示主菜单
 */
void UI_System_ShowMenu(void);

/**
 * @brief 隐藏菜单，返回主界面
 */
void UI_System_HideMenu(void);

/**
 * @brief 显示弹出消息
 * @param title 标题
 * @param message 消息内容
 * @param duration_ms 显示时间(ms), 0=直到按键关闭
 */
void UI_System_ShowPopup(const char* title, const char* message, uint16_t duration_ms);

/**
 * @brief 关闭弹出框
 */
void UI_System_ClosePopup(void);

/**
 * @brief 刷新整个界面
 */
void UI_System_Refresh(void);

/**
 * @brief 设置主菜单
 * @param menu 主菜单指针
 */
void UI_System_SetMainMenu(UI_Menu_t* menu);

/**
 * @brief 获取主菜单
 * @return 主菜单指针
 */
UI_Menu_t* UI_System_GetMainMenu(void);

/**
 * @brief 检查是否开机完成
 * @return true开机完成
 */
bool UI_System_IsReady(void);

/*===========================================================================
 * 默认菜单声明 (外部定义)
 *===========================================================================*/

/* 在ui_menu_def.c中定义默认菜单结构 */
extern UI_Menu_t* UI_GetDefaultMainMenu(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_SYSTEM_H__ */
