/**
 * @file    ui_bootscreen.h
 * @brief   开机画面模块
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * 功能:
 *   - 显示Logo
 *   - 显示产品名称/版本
 *   - 显示进度条
 *   - 淡入淡出效果
 */

#ifndef __UI_BOOTSCREEN_H__
#define __UI_BOOTSCREEN_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 配置
 *===========================================================================*/

/* 开机画面阶段 */
typedef enum {
    UI_BOOT_STAGE_INIT = 0,     /* 初始化 */
    UI_BOOT_STAGE_LOGO,         /* 显示Logo */
    UI_BOOT_STAGE_INFO,         /* 显示信息 */
    UI_BOOT_STAGE_PROGRESS,     /* 加载进度 */
    UI_BOOT_STAGE_FADEOUT,      /* 淡出 */
    UI_BOOT_STAGE_DONE,         /* 完成 */
} UI_BootStage_t;

/* 开机画面配置 */
typedef struct {
    const uint8_t* logo_data;       /* Logo图像数据 (RGB565) */
    uint16_t logo_width;            /* Logo宽度 */
    uint16_t logo_height;           /* Logo高度 */
    const char* product_name;       /* 产品名称 */
    const char* version;            /* 版本号 */
    const char* copyright;          /* 版权信息 */
    uint16_t display_time;          /* 显示时间(ms) */
    bool show_progress;             /* 是否显示进度条 */
} UI_BootConfig_t;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化开机画面
 * @param config 配置参数，NULL使用默认配置
 */
void UI_BootScreen_Init(const UI_BootConfig_t* config);

/**
 * @brief 开始显示开机画面
 */
void UI_BootScreen_Start(void);

/**
 * @brief 更新开机画面 (在主循环中调用)
 * @param delta_ms 时间间隔(ms)
 * @return true继续显示，false显示完成
 */
bool UI_BootScreen_Update(uint16_t delta_ms);

/**
 * @brief 设置加载进度
 * @param progress 进度 0-100
 * @param message 进度消息 (可选)
 */
void UI_BootScreen_SetProgress(uint8_t progress, const char* message);

/**
 * @brief 跳过开机画面
 */
void UI_BootScreen_Skip(void);

/**
 * @brief 检查开机画面是否完成
 * @return true已完成
 */
bool UI_BootScreen_IsDone(void);

/**
 * @brief 获取当前阶段
 * @return 当前阶段
 */
UI_BootStage_t UI_BootScreen_GetStage(void);

/**
 * @brief 设置Logo数据
 * @param data 图像数据 (RGB565)
 * @param width 宽度
 * @param height 高度
 */
void UI_BootScreen_SetLogo(const uint8_t* data, uint16_t width, uint16_t height);

/**
 * @brief 设置产品信息
 * @param name 产品名称
 * @param version 版本号
 */
void UI_BootScreen_SetProductInfo(const char* name, const char* version);

#ifdef __cplusplus
}
#endif

#endif /* __UI_BOOTSCREEN_H__ */
