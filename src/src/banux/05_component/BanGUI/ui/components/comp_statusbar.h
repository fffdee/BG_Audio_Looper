/**
 * @file    comp_statusbar.h
 * @brief   Status Bar Component (New Architecture)
 * @author  BG Card Team
 * @date    2025-01-09
 * 
 * 状态栏组件 - 显示蓝牙状态、音量、电池等信息
 * 兼容旧 UI_StatusBar_* API
 */

#ifndef __COMP_STATUSBAR_H__
#define __COMP_STATUSBAR_H__

#include <stdint.h>
#include <stdbool.h>
#include "ui_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 硬件检测引脚配置 (BG Card Mini 实际硬件)
 *===========================================================================*/

/* ADC 输入检测引脚 */
#define UI_DET_MIC_PORT         GPIO_A_IN
#define UI_DET_MIC_PIN          GPIO_INDEX30    /* MIC 检测 (下拉，高电平有效) */
#define UI_DET_GUITAR_PORT      GPIO_A_IN
#define UI_DET_GUITAR_PIN       GPIO_INDEX29    /* 吉他检测 (上拉，低电平有效) */

/* DAC 输出检测引脚 */
#define UI_DET_HP_PORT          GPIO_B_IN
#define UI_DET_HP_PIN           GPIO_INDEX4     /* 耳机检测 (上拉，低电平有效) */

/* 音量旋钮 ADC 引脚 */
#define UI_VOLUME_ADC_PORT      GPIO_A_ANA_EN
#define UI_VOLUME_ADC_PIN       GPIO_INDEX28    /* 主音量旋钮 ADC */
#define UI_VOLUME_ADC_CHANNEL   ADC_CHANNEL_GPIOA28

/* 电池 ADC 引脚 */
#define UI_BATTERY_ADC_PORT     GPIO_A_ANA_EN
#define UI_BATTERY_ADC_PIN      GPIO_INDEX31    /* 电池电压 ADC */
#define UI_BATTERY_ADC_CHANNEL  ADC_CHANNEL_GPIOA31

/* 检测水平 (0=插入时有效低电平，1=插入时有效高电平) */
#define UI_DET_ACTIVE_LOW       0
#define UI_DET_ACTIVE_HIGH      1
#define UI_DET_MIC_ACTIVE       UI_DET_ACTIVE_HIGH  /* MIC: 下拉，插入时有效高电平 */
#define UI_DET_GUITAR_ACTIVE    UI_DET_ACTIVE_LOW   /* 吉他: 上拉，插入时有效低电平 */
#define UI_DET_HP_ACTIVE        UI_DET_ACTIVE_LOW   /* 耳机: 上拉，插入时有效低电平 */

/*===========================================================================
 * 状态定义
 *===========================================================================*/

/* 蓝牙状态 */
typedef enum {
    UI_BT_OFF = 0,          /* 蓝牙关闭 */
    UI_BT_DISCONNECTED,     /* 蓝牙开启但未连接 */
    UI_BT_CONNECTING,       /* 正在连接 */
    UI_BT_CONNECTED,        /* 已连接 */
    UI_BT_PLAYING,          /* 蓝牙音乐播放中 */
} UI_BTStatus_t;

/* ADC 输入源状态 (位标志，可以同时多个) */
typedef enum {
    UI_ADC_NONE     = 0x00, /* 无输入 */
    UI_ADC_MIC      = 0x01, /* MIC 已插入 */
    UI_ADC_GUITAR   = 0x02, /* 吉他已插入 */
    UI_ADC_USB      = 0x04, /* USB 音频输入 */
    UI_ADC_BT       = 0x08, /* 蓝牙音频输入 */
} UI_ADCSource_t;

/* DAC 输出目标状态 (位标志) */
typedef enum {
    UI_DAC_NONE     = 0x00, /* 无输出 */
    UI_DAC_HP       = 0x01, /* 耳机已插入 */
    UI_DAC_SPKR     = 0x02, /* 扬声器输出 */
    UI_DAC_LINEOUT  = 0x04, /* 线路输出 */
    UI_DAC_USB      = 0x08, /* USB 音频输出 */
} UI_DACOutput_t;

/* 状态栏数据结构 */
typedef struct {
    UI_BTStatus_t bt_status;        /* 蓝牙状态 */
    uint8_t adc_source;             /* ADC 输入源 (UI_ADCSource_t 位与组合) */
    uint8_t dac_output;             /* DAC 输出目标 (UI_DACOutput_t 位与组合) */
    uint8_t volume;                 /* 音量 0-100 */
    uint8_t battery;                /* 电池电量 0-100 */
    bool muted;                     /* 静音状态 */
    bool usb_connected;             /* USB 连接状态 */
    bool charging;                  /* 充电状态 */
    uint8_t battery_level;          /* 电池电量百分比 0-100 */
    uint8_t battery_grid;           /* 电池电量格数 0-4 */
} UI_StatusBarData_t;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化状态栏
 */
void UI_StatusBar_Init(void);

/**
 * @brief 绘制状态栏 (完全重绘)
 */
void UI_StatusBar_Draw(void);

/**
 * @brief 更新状态栏 (仅更新变化部分)
 */
void UI_StatusBar_Update(void);

/**
 * @brief 设置蓝牙状态
 * @param status 蓝牙状态
 */
void UI_StatusBar_SetBTStatus(UI_BTStatus_t status);

/**
 * @brief 扫描硬件检测引脚 (定期调用)
 */
void UI_StatusBar_ScanDetect(void);

/**
 * @brief 设置 ADC 输入状态
 * @param source ADC 源标志 (UI_ADCSource_t 位组合)
 */
void UI_StatusBar_SetADCSource(uint8_t source);

/**
 * @brief 设置 DAC 输出状态
 * @param output DAC 输出标志 (UI_DACOutput_t 位组合)
 */
void UI_StatusBar_SetDACOutput(uint8_t output);

/**
 * @brief 设置音量
 * @param volume 音量 0-100
 */
void UI_StatusBar_SetVolume(uint8_t volume);

/**
 * @brief 设置静音状态
 * @param muted true 表示静音
 */
void UI_StatusBar_SetMuted(bool muted);

/**
 * @brief 设置 USB 连接状态
 * @param connected true 表示已连接
 */
void UI_StatusBar_SetUSBConnected(bool connected);

/**
 * @brief 获取状态栏数据
 * @return 状态栏数据指针
 */
UI_StatusBarData_t* UI_StatusBar_GetData(void);

/**
 * @brief 获取状态栏高度
 * @return 高度（以像素为单位）
 */
uint16_t UI_StatusBar_GetHeight(void);

/**
 * @brief 显示/隐藏状态栏
 * @param visible true 表示显示
 */
void UI_StatusBar_SetVisible(bool visible);

/**
 * @brief 检查状态栏是否可见
 * @return true 表示可见
 */
bool UI_StatusBar_IsVisible(void);

/**
 * @brief 绘制默认logo
 */
void draw_default_logo(void);

#ifdef __cplusplus
}
#endif

#endif /* __COMP_STATUSBAR_H__ */
