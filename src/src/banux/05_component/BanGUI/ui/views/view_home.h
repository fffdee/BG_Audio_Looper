/**
 * @file    view_home.h
 * @brief   Home View - Main idle screen (New Architecture)
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 主界面视图 - 显示图标网格和状态信息
 */

#ifndef __VIEW_HOME_H__
#define __VIEW_HOME_H__

#include "../core/bg_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 配置常量
 *===========================================================================*/

#define HOME_ICON_COUNT         4
#define HOME_ICON_WIDTH         32
#define HOME_ICON_HEIGHT        32
#define HOME_ICON_SPACING       40

/*===========================================================================
 * 图标 ID
 *===========================================================================*/

typedef enum {
    HOME_ICON_SETTINGS = 0,
    HOME_ICON_AUDIO_CTRL,
    HOME_ICON_DRUM,
    HOME_ICON_LOOPER
} HomeIconID_t;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 创建 Home 视图
 * @return UI_View_t* 视图指针
 */
UI_View_t* View_Home_Create(void);

/**
 * @brief 初始化 Home 视图 (别名，兼容旧API)
 */
#define View_Home_Init() View_Home_Create()

/**
 * @brief 销毁 Home 视图
 */
void View_Home_Destroy(void);

/**
 * @brief 获取当前选中的图标
 * @return 图标索引
 */
uint8_t View_Home_GetSelectedIcon(void);

/**
 * @brief 设置图标选择回调
 * @param icon_id 图标 ID
 * @param callback 回调函数
 */
void View_Home_SetIconCallback(HomeIconID_t icon_id, void (*callback)(void));

/**
 * @brief 刷新显示
 */
void View_Home_Refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __VIEW_HOME_H__ */
