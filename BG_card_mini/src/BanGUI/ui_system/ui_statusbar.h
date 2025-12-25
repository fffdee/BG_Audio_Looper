/**
 * @file    ui_statusbar.h
 * @brief   Top status bar module
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * Features:
 *   - Bluetooth connection status icon
 *   - ADC input detection (MIC/LineIn/Guitar)
 *   - DAC output detection (Headphone/Speaker)
 *   - Volume indicator
 */

#ifndef __UI_STATUSBAR_H__
#define __UI_STATUSBAR_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Hardware detection pin configuration (BG Card Mini actual hardware)
 *===========================================================================*/

/* ADC input detection pins */
#define UI_DET_MIC_PORT         GPIO_A_IN
#define UI_DET_MIC_PIN          GPIO_INDEX30    /* MIC detection (pull-down, high level valid) */
#define UI_DET_GUITAR_PORT      GPIO_A_IN
#define UI_DET_GUITAR_PIN       GPIO_INDEX29    /* Guitar detection (pull-up, low level valid) */

/* DAC output detection pins */
#define UI_DET_HP_PORT          GPIO_B_IN
#define UI_DET_HP_PIN           GPIO_INDEX4     /* Headphone detection (pull-up, low level valid) */

/* Volume knob ADC pin */
#define UI_VOLUME_ADC_PORT      GPIO_A_ANA_EN
#define UI_VOLUME_ADC_PIN       GPIO_INDEX28    /* Main volume knob ADC */
#define UI_VOLUME_ADC_CHANNEL   ADC_CHANNEL_GPIOA28

/* Battery ADC pin */
#define UI_BATTERY_ADC_PORT     GPIO_A_ANA_EN
#define UI_BATTERY_ADC_PIN      GPIO_INDEX31    /* Battery voltage ADC */
#define UI_BATTERY_ADC_CHANNEL  ADC_CHANNEL_GPIOA31

/* Detection level (0=low valid when inserted, 1=high valid when inserted) */
#define UI_DET_ACTIVE_LOW       0
#define UI_DET_ACTIVE_HIGH      1
#define UI_DET_MIC_ACTIVE       UI_DET_ACTIVE_HIGH  /* MIC: pull-down, high valid when inserted */
#define UI_DET_GUITAR_ACTIVE    UI_DET_ACTIVE_LOW   /* Guitar: pull-up, low valid when inserted */
#define UI_DET_HP_ACTIVE        UI_DET_ACTIVE_LOW   /* Headphone: pull-up, low valid when inserted */

/*===========================================================================
 * Status definitions
 *===========================================================================*/

/* Bluetooth status */
typedef enum {
    UI_BT_OFF = 0,          /* Bluetooth off */
    UI_BT_DISCONNECTED,     /* Bluetooth on but not connected */
    UI_BT_CONNECTING,       /* Connecting */
    UI_BT_CONNECTED,        /* Connected */
    UI_BT_PLAYING,          /* Bluetooth music playing */
} UI_BTStatus_t;

/* ADC input source status (bit flag, can be multiple at once) */
typedef enum {
    UI_ADC_NONE     = 0x00, /* No input */
    UI_ADC_MIC      = 0x01, /* MIC inserted */
    UI_ADC_GUITAR   = 0x02, /* Guitar inserted */
    UI_ADC_USB      = 0x04, /* USB audio input */
    UI_ADC_BT       = 0x08, /* Bluetooth audio input */
} UI_ADCSource_t;

/* DAC output target status (bit flag) */
typedef enum {
    UI_DAC_NONE     = 0x00, /* No output */
    UI_DAC_HP       = 0x01, /* Headphone inserted */
    UI_DAC_SPKR     = 0x02, /* Speaker output */
    UI_DAC_LINEOUT  = 0x04, /* LineOut output */
    UI_DAC_USB      = 0x08, /* USB audio output */
} UI_DACOutput_t;

/* Status bar data structure */
typedef struct {
    UI_BTStatus_t bt_status;        /* Bluetooth status */
    uint8_t adc_source;             /* ADC input source (UI_ADCSource_t bitwise combination) */
    uint8_t dac_output;             /* DAC output target (UI_DACOutput_t bitwise combination) */
    uint8_t volume;                 /* Volume 0-100 */
    uint8_t battery;                /* Battery level 0-100 */
    bool muted;                     /* Muted */
    bool usb_connected;             /* USB connected */
    bool charging;                  /* Charging */
    uint8_t battery_level;  // 0-100 percent
    uint8_t battery_grid;   // 0-4 grid count
} UI_StatusBarData_t;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化状态栏
 */
void UI_StatusBar_Init(void);

/**
 * @brief 绘制状态栏 (完整重绘)
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
 * @brief 扫描硬件检测引脚 (周期调用)
 */
void UI_StatusBar_ScanDetect(void);

/**
 * @brief 设置ADC输入状态
 * @param source ADC源标志 (UI_ADCSource_t位组合)
 */
void UI_StatusBar_SetADCSource(uint8_t source);

/**
 * @brief 设置DAC输出状态
 * @param output DAC输出标志 (UI_DACOutput_t位组合)
 */
void UI_StatusBar_SetDACOutput(uint8_t output);

/**
 * @brief 设置音量
 * @param volume 音量 0-100
 */
void UI_StatusBar_SetVolume(uint8_t volume);

/**
 * @brief 设置静音状态
 * @param muted true静音
 */
void UI_StatusBar_SetMuted(bool muted);

/**
 * @brief 设置USB连接状态
 * @param connected true已连接
 */
void UI_StatusBar_SetUSBConnected(bool connected);

/**
 * @brief 获取状态栏数据
 * @return 状态栏数据指针
 */
UI_StatusBarData_t* UI_StatusBar_GetData(void);

/**
 * @brief 获取状态栏高度
 * @return 高度像素
 */
uint16_t UI_StatusBar_GetHeight(void);

/**
 * @brief 显示/隐藏状态栏
 * @param visible true显示
 */
void UI_StatusBar_SetVisible(bool visible);

/**
 * @brief 状态栏是否可见
 * @return true可见
 */
bool UI_StatusBar_IsVisible(void);

void draw_default_logo(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_STATUSBAR_H__ */
