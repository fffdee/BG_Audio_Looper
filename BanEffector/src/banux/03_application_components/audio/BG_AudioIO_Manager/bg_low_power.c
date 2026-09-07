/**
 * bg_low_power.c - 系统低功耗管理模块实现
 *
 * 状态机：
 *   NORMAL  →（所有活动指标 5 分钟静默）→  LOW_POWER
 *   LOW_POWER→（任意活动指标触发）         →  NORMAL
 *
 * 低功耗动作：
 *   进入：AudioDAC_DigitalMute(DAC0/1, TRUE, TRUE)
 *         跳过 Effect Graph 处理（由上层 Audio_loop 判断 IsLowPower）
 *   退出：清空 ADC 历史数据，AudioDAC_DigitalMute(DAC0/1, FALSE, FALSE)
 */

#include <stdbool.h>
#include "bg_low_power.h"
#include "audio_adc.h"
#include "dac.h"
#include "debug.h"

/* FreeRTOS tick — 提供 ms 级时间戳 */
#include "FreeRTOS.h"
#include "task.h"
#define GET_TICK_MS()  ((uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS))

/* ====================== 私有状态 ====================== */
static uint32_t g_last_activity_tick = 0; /* 上一次活跃时刻 (ms) */
static uint8_t  g_lp_active          = 0; /* 当前是否处于低功耗模式 */
static uint8_t  g_frame_activity     = 0; /* 本帧累积活动掩码，每 Tick 清零 */
static uint8_t  g_forced_lp          = 0; /* 强制低功耗标志（1=手动进入，不自动退出） */
static uint8_t  g_lp_enabled         = 1; /* 自动低功耗功能启用标志（0=用户禁用，不自动超时） */
static uint32_t g_lp_timeout_ms      = LP_IDLE_TIMEOUT_MS; /* 运行时可调空闲超时（ms） */

/* ADC 旁路检测缓冲区（仅在低功耗模式下使用） */
#define LP_ADC_PEEK_SIZE  64
static uint32_t s_peek_buf[LP_ADC_PEEK_SIZE];
static uint32_t s_last_peek_tick = 0; /* 上一次 ADC peek 的时间戳 */

/* ====================== 私有函数 ====================== */

/**
 * 进入低功耗：mute DAC 输出
 */
static void EnterLowPower(void)
{
    if (g_lp_active) {
        return;
    }
    g_lp_active = 1;
    DBG("[LowPower] ==== Entering low power mode (5 min idle) ====\n");
    AudioDAC_DigitalMute(DAC0, TRUE, TRUE);
    AudioDAC_DigitalMute(DAC1, TRUE, TRUE);
}

/**
 * 退出低功耗：
 *   1. 清空 ADC DMA FIFO（丢弃积压的陈旧样本）
 *   2. unmute DAC 输出
 */
static void ExitLowPower(void)
{
    uint32_t drain_buf[64];
    uint16_t avail;
    uint8_t iter;

    if (!g_lp_active) {
        return;
    }
    g_lp_active = 0;
    DBG("[LowPower] ==== Exiting low power mode ====\n");

    /* 排空 ADC0 积压数据（最多 16 × 64 = 1024 samples） */
    for (iter = 0; iter < 16; iter++) {
        avail = AudioADC_DataLenGet(ADC0_MODULE);
        if (avail == 0) break;
        if (avail > 64) avail = 64;
        AudioADC_DataGet(ADC0_MODULE, drain_buf, avail);
    }

    /* 排空 ADC1 积压数据 */
    for (iter = 0; iter < 16; iter++) {
        avail = AudioADC_DataLenGet(ADC1_MODULE);
        if (avail == 0) break;
        if (avail > 64) avail = 64;
        AudioADC_DataGet(ADC1_MODULE, drain_buf, avail);
    }

    /* unmute DAC */
    AudioDAC_DigitalMute(DAC0, FALSE, FALSE);
    AudioDAC_DigitalMute(DAC1, FALSE, FALSE);
}

/* ====================== 公开 API ====================== */

void LowPower_Init(void)
{
    g_last_activity_tick = GET_TICK_MS();
    g_lp_active          = 0;
    g_frame_activity     = 0;
    s_last_peek_tick     = 0;
    DBG("[LowPower] Initialized (idle timeout = %lu ms)\n",
        (unsigned long)LP_IDLE_TIMEOUT_MS);
}

void LowPower_FeedActivity(uint8_t mask)
{
    g_frame_activity |= mask;
}

void LowPower_CheckADCSignal(const uint32_t *buf, uint16_t len)
{
    uint16_t i;
    int32_t l, r;

    if (!buf || len == 0) {
        return;
    }
    /* 步长 4，约检查 25% 的样本，兼顾性能与灵敏度 */
    for (i = 0; i < len; i += 4) {
        l = (int16_t)(buf[i] & 0xFFFFU);
        r = (int16_t)((buf[i] >> 16) & 0xFFFFU);
        if (l < 0) l = -l;
        if (r < 0) r = -r;
        if (l > LP_ADC_THRESHOLD || r > LP_ADC_THRESHOLD) {
            g_frame_activity |= LP_ACT_ADC_SIGNAL;
            return;
        }
    }
}

void LowPower_Tick(void)
{
    uint32_t now       = GET_TICK_MS();
    uint8_t  any_active;
    uint16_t avail;

    /* ----- 低功耗模式下：旁路 ADC 检测（退出条件之一） ----- */
    if (g_lp_active) {
        uint32_t elapsed;
        /* 控制检测节奏：每 LP_ADC_PEEK_INTERVAL_MS 检查一次 */
        elapsed = (now >= s_last_peek_tick) ?
                  (now - s_last_peek_tick) :
                  (0xFFFFFFFFUL - s_last_peek_tick + now + 1UL);

        if (elapsed >= LP_ADC_PEEK_INTERVAL_MS) {
            s_last_peek_tick = now;

            avail = AudioADC_DataLenGet(ADC0_MODULE);
            if (avail > LP_ADC_PEEK_SIZE) avail = LP_ADC_PEEK_SIZE;
            if (avail > 0) {
                AudioADC_DataGet(ADC0_MODULE, s_peek_buf, avail);
                LowPower_CheckADCSignal(s_peek_buf, avail);
            }

        }
    }

    /* ----- 消费本帧活动掩码 ----- */
    any_active       = (g_frame_activity != 0) ? 1U : 0U;
    g_frame_activity = 0;

    /* 强制低功耗模式：忽略活动掩码，保持低功耗直到 ForceClear */
    if (g_forced_lp) {
        if (!g_lp_active) {
            EnterLowPower();
        }
        return;
    }

    if (any_active) {
        /* 有活动 → 刷新计时器；如果在低功耗则退出 */
        g_last_activity_tick = now;
        if (g_lp_active) {
            ExitLowPower();
        }
        return;
    }

    /* ----- 检查超时 → 进入低功耗 ----- */
    if (!g_lp_active) {
        uint32_t elapsed;
        /* 禁用自动低功耗时跳过超时判断 */
        if (!g_lp_enabled) {
            return;
        }
        elapsed = (now >= g_last_activity_tick) ?
                  (now - g_last_activity_tick) :
                  (0xFFFFFFFFUL - g_last_activity_tick + now + 1UL);

        if (elapsed >= g_lp_timeout_ms) {
            EnterLowPower();
        }
    }
}

uint8_t LowPower_IsLowPower(void)
{
    return g_lp_active;
}

/* ====================== 强制进入/退出低功耗 ====================== */

void LowPower_ForceEnter(void)
{
    g_forced_lp = 1;
    EnterLowPower();
    DBG("[LowPower] Force enter (DATA_TRANSFER)\n");
}

void LowPower_ForceClear(void)
{
    g_forced_lp = 0;
    /* 刷新活动时间戳，立即退出低功耗 */
    g_last_activity_tick = GET_TICK_MS();
    g_frame_activity = 0xFF; /* 标记所有活动，确保下一次 Tick 正常退出 */
    ExitLowPower();
    DBG("[LowPower] Force cleared, resuming normal\n");
}

void LowPower_SetEnabled(uint8_t enabled)
{
    g_lp_enabled = enabled ? 1 : 0;
    if (!g_lp_enabled && g_lp_active && !g_forced_lp) {
        /* 用户禁用自动低功耗时，如果当前在自动低功耗状态，立即退出 */
        g_last_activity_tick = GET_TICK_MS();
        g_frame_activity = 0xFF;
        ExitLowPower();
    }
    DBG("[LowPower] Auto-LP %s\n", g_lp_enabled ? "ENABLED" : "DISABLED");
}

uint8_t LowPower_GetEnabled(void)
{
    return g_lp_enabled;
}

void LowPower_SetTimeoutMin(uint8_t minutes)
{
    if (minutes < 1) minutes = 1;
    if (minutes > 60) minutes = 60;
    g_lp_timeout_ms = (uint32_t)minutes * 60UL * 1000UL;
    DBG("[LowPower] Idle timeout set to %d min (%lu ms)\n",
        (int)minutes, (unsigned long)g_lp_timeout_ms);
}

uint8_t LowPower_GetTimeoutMin(void)
{
    return (uint8_t)(g_lp_timeout_ms / 60000UL);
}
