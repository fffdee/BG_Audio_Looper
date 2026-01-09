/**
 * @file    view_boot.h
 * @brief   Boot Splash Screen View (开机界面)
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 开机界面视图：
 *   - 显示 Logo + 进度条
 *   - 使用定时器/状态机驱动动画，不使用硬延迟
 *   - 动画完成后自动切换到主界面 (UI_STATE_IDLE)
 */

#ifndef __VIEW_BOOT_H__
#define __VIEW_BOOT_H__

#include "bg_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 公共接口
 *===========================================================================*/

/**
 * @brief 获取 Boot 视图
 * @return Boot 视图对象指针
 */
UI_View_t* View_Boot_Get(void);

/**
 * @brief 初始化 Boot 视图
 */
void View_Boot_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __VIEW_BOOT_H__ */
