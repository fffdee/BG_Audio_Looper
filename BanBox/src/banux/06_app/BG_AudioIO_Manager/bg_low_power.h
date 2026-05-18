/**
 * bg_low_power.h - 系统低功耗管理模块
 *
 * 功能：
 *   持续监测 5 类活动指标；当所有指标连续 5 分钟均为静默，
 *   自动 mute DAC 并跳过音频 DSP 处理（低功耗状态）。
 *   任意指标重新激活时立刻退出低功耗模式，恢复正常处理。
 *
 * 活动指标：
 *   LP_ACT_USB_AUDIO  — USB 音频（Speaker / Mic）正在使用
 *   LP_ACT_BT_AUDIO   — 蓝牙 A2DP 正在流式播放
 *   LP_ACT_ADC_SIGNAL — ADC 输入信号电平超过噪声门限
 *   LP_ACT_CDC_COMM   — CDC 串口有数据到达
 *   LP_ACT_BLE_COMM   — BLE Shell 有数据到达
 */

#ifndef _BG_LOW_POWER_H__
#define _BG_LOW_POWER_H__

#include <stdint.h>

/* ====================== 活动掩码 ====================== */
#define LP_ACT_USB_AUDIO    0x01U
#define LP_ACT_BT_AUDIO     0x02U
#define LP_ACT_ADC_SIGNAL   0x04U
#define LP_ACT_CDC_COMM     0x08U
#define LP_ACT_BLE_COMM     0x10U
#define LP_ACT_BATT_CALIB   0x20U  /* Battery calibration in progress */

/* ====================== 超时阈值 ====================== */
/* 5 分钟不活跃后进入低功耗，单位 ms */
#define LP_IDLE_TIMEOUT_MS  (1UL * 60UL * 1000UL)

/* ADC 信号检测门限（int16_t 绝对值，0-32767）
 * 200 ≈ -44 dBFS，足以区分底噪与真实信号 */
#define LP_ADC_THRESHOLD    200

/* ADC 旁路检测间隔（ms）：低功耗中每隔 20ms 采样一次 ADC */
#define LP_ADC_PEEK_INTERVAL_MS  20

/* ====================== API 声明 ====================== */

/**
 * @brief 初始化低功耗管理器，在 BG_audio_Init() 末尾调用一次
 */
void LowPower_Init(void);

/**
 * @brief 喂入活动掩码，任意位置置 1 均会刷新不活跃计时器
 * @param mask  LP_ACT_xxx 的组合
 */
void LowPower_FeedActivity(uint8_t mask);

/**
 * @brief 对 ADC 帧数据做峰值检测，信号超门限则自动喂入 LP_ACT_ADC_SIGNAL
 * @param buf  32-bit 打包样本（高16位=R，低16位=L）
 * @param len  样本数
 */
void LowPower_CheckADCSignal(const uint32_t *buf, uint16_t len);

/**
 * @brief 低功耗 Tick，在 Audio_loop() 每次迭代时调用
 *        负责判断是否进入/退出低功耗状态
 */
void LowPower_Tick(void);

/**
 * @brief 查询当前是否处于低功耗模式
 * @return 1 = 低功耗，0 = 正常
 */
uint8_t LowPower_IsLowPower(void);

/**
 * @brief 强制立即进入低功耗模式（DAC 静音，跳过 DSP）
 *        用于数据传输态等需要立即静音的场景。
 *        调用后 g_forced_lp = 1，阻止正常活动检测恢复音频。
 */
void LowPower_ForceEnter(void);

/**
 * @brief 清除强制低功耗，恢复正常活动检测逻辑
 *        调用后刷新活动时间戳，立即退出低功耗状态（如果当前在低功耗中）。
 */
void LowPower_ForceClear(void);

/**
 * @brief 设置低功耗自动检测功能的启用/禁用状态
 *        禁用时，系统不会因空闲超时进入低功耗模式（强制 ForceEnter 仍有效）。
 * @param enabled  1=启用自动低功耗（默认），0=禁用自动低功耗
 */
void LowPower_SetEnabled(uint8_t enabled);

/**
 * @brief 查询当前低功耗自动检测功能是否启用
 * @return 1=已启用，0=已禁用
 */
uint8_t LowPower_GetEnabled(void);

/**
 * @brief 设置空闲超时时间（分钟）
 * @param minutes  超时分钟数，范围 1-60
 */
void LowPower_SetTimeoutMin(uint8_t minutes);

/**
 * @brief 获取当前空闲超时时间（分钟）
 * @return 超时分钟数（1-60）
 */
uint8_t LowPower_GetTimeoutMin(void);

#endif /* _BG_LOW_POWER_H__ */
