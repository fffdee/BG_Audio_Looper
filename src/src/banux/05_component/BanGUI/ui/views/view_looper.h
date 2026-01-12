/**
 * @file    view_looper.h
 * @brief   Looper View - Audio looper control interface
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * Looper 视图 - 音频循环器控制界面
 */

#ifndef __VIEW_LOOPER_H__
#define __VIEW_LOOPER_H__

#include "../core/bg_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 配置常量
 *===========================================================================*/

#define LOOPER_MAX_SEGMENTS     4

/*===========================================================================
 * 段状态枚举
 *===========================================================================*/

typedef enum {
    LOOPER_SEG_INACTIVE = 0,    /* 未激活 */
    LOOPER_SEG_RECORDING,       /* 录制中 */
    LOOPER_SEG_PLAYING,         /* 播放中 */
    LOOPER_SEG_STOPPED,         /* 已停止 */
} LooperSegState_t;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 创建并初始化 Looper 视图
 * @return UI_View_t* 视图指针
 */
UI_View_t* View_Looper_Create(void);

/**
 * @brief 初始化 Looper 视图 (别名，兼容旧API)
 */
#define View_Looper_Init() View_Looper_Create()

/**
 * @brief 销毁 Looper 视图
 */
void View_Looper_Destroy(void);

/**
 * @brief 设置段状态 (从 AudioLooper 模块调用)
 * @param seg_index 段索引 (0-3)
 * @param state 段状态
 */
void View_Looper_SetSegmentState(uint8_t seg_index, LooperSegState_t state);

/**
 * @brief 设置段进度 (0-100%)
 * @param seg_index 段索引
 * @param progress 进度百分比
 */
void View_Looper_SetSegmentProgress(uint8_t seg_index, uint8_t progress);

/**
 * @brief 刷新显示
 */
void View_Looper_Refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __VIEW_LOOPER_H__ */
