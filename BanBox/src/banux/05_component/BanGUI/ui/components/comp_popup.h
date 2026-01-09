/**
 * @file    comp_popup.h
 * @brief   Popup Component (New Architecture)
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 弹窗组件 - 显示模态消息框
 */

#ifndef __COMP_POPUP_H__
#define __COMP_POPUP_H__

#include "../core/bg_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 弹窗类型
 *===========================================================================*/

typedef enum {
    POPUP_TYPE_INFO = 0,        /* 信息 */
    POPUP_TYPE_WARNING,         /* 警告 */
    POPUP_TYPE_ERROR,           /* 错误 */
    POPUP_TYPE_SUCCESS,         /* 成功 */
} PopupType_t;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化弹窗组件
 */
void Comp_Popup_Init(void);

/**
 * @brief 显示弹窗
 * @param type 弹窗类型
 * @param title 标题
 * @param message 消息内容
 * @param duration_ms 显示时长 (0 = 手动关闭)
 */
void Comp_Popup_Show(PopupType_t type, const char* title, 
                     const char* message, uint16_t duration_ms);

/**
 * @brief 关闭弹窗
 */
void Comp_Popup_Close(void);

/**
 * @brief 弹窗是否激活
 * @return true 如果弹窗正在显示
 */
bool Comp_Popup_IsActive(void);

/**
 * @brief 更新弹窗 (处理计时)
 * @param delta_ms 时间间隔
 */
void Comp_Popup_Update(uint16_t delta_ms);

/**
 * @brief 绘制弹窗
 */
void Comp_Popup_Draw(void);

/**
 * @brief 处理按钮事件
 * @param event 按钮事件
 * @return true 如果事件被消费
 */
bool Comp_Popup_HandleButton(UI_BtnEventData_t* event);

#ifdef __cplusplus
}
#endif

#endif /* __COMP_POPUP_H__ */
