/**
 * @file bg_audio_detection.c
 * @brief Plug detection GPIO helpers (guitar/mic/speaker).
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_internal.h"
#include "product_def.h"
#include "debug.h"
#include "bg_audio_detection.h"

#include "gpio.h"
#include "bg_event.h"
#include "adc.h"

#include "FreeRTOS.h"
#include "task.h"

void ProcessGuitarOutput()
{
#ifdef BANBOX_II
	/* BANBOX_II: A29 = NAND Flash CS, 不能读取吉他检测信号 */
	(void)0;
#else
	if (!GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX29))
	{

		BG_AudioManager.Audio_data.det_state  = GUITAR_DET_OUT;
		GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA17);

	}
	else
	{
		GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA17);
		BG_AudioManager.Audio_data.det_state  = GUITAR_DET_IN;


	}
#endif /* !BANBOX_II */
}

/**
 * 处理麦克风信号输出
 */
void ProcessMicOutput()
{
	if (GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX30))
	{
		GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA1);
		BG_AudioManager.Audio_data.det_state = MIC_DET_OUT;

	}
	else
	{
		GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA1);
		BG_AudioManager.Audio_data.det_state  = MIC_DET_IN;


	}
}

/**
 * 处理扬声器/耳机切换
 */
void ProcessSpeakerSwitch(void)
{
	if (GPIO_RegOneBitGet(GPIO_B_IN, GPIO_INDEX4))
	{
#ifndef BANBOX_II
		GPIO_RegOneBitClear(GPIO_B_OUT, GPIOB6); /* BANBOX_II: B6 = PSRAM CS */
#endif
		BG_AudioManager.Audio_data.det_state  = SPEAKER_DET;
	}
	else
	{
#ifndef BANBOX_II
		GPIO_RegOneBitSet(GPIO_B_OUT, GPIOB6); /* BANBOX_II: B6 = PSRAM CS */
#endif
		BG_AudioManager.Audio_data.det_state  = EARPHONE_DET;
	}
}

/* ==================== LINE IN 插入检测（左右声道独立） ==================== */

/**
 * Line1（Line In 左声道 / Guitar1）插入检测：A29 上拉，高 = 已插入。
 * 检测宏关闭或 BANBOX_II（A29 复用 NAND Flash CS）时不限制，恒视为已插入。
 */
uint8_t BG_AudioDetection_Line1IsPlugged(void)
{
#if LINE1_INPUT_DETECT_EN && !defined(BANBOX_II)
	return (GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX29) != 0) ? 1U : 0U;
#else
	return 1U;
#endif
}

/* Line2（Line In 右声道 / Guitar2）插入检测状态：
 * 由 Line2Poll 每 1 秒采样 POWERKEY ADC 独立判定，与 Line1(A29) 无关 */
static uint8_t line2_plugged = 1U;   /* 默认视为已插入 */
static uint8_t line2_det_cnt = 0U;   /* 当前状态连续出现次数（去抖） */
static uint8_t line2_det_cur = 0xFF; /* 去抖窗口内当前状态（0xFF = 尚未采样） */

/**
 * Line2（Line In 右声道 / Guitar2）插入检测：独立采样 POWERKEY ADC 通道，
 * 判定：ADC 值 > 4000 → 已插入（与 BanDataHub 原版一致）。
 */
uint8_t BG_AudioDetection_Line2IsPlugged(void)
{
#if LINE2_INPUT_DETECT_EN
	return line2_plugged;
#else
	return 1U;
#endif
}

/**
 * Line2 轮询（每 1 秒调用一次）：独立采样 POWERKEY ADC 通道，
 * 连续 3 次(3s)同一状态才确认切换，避免瞬时抖动误判。
 * 状态变化时发布 EVT_AUDIO_GUITAR_IN 事件（port_id=2）。
 */
void BG_AudioDetection_Line2Poll(void)
{
#if LINE2_INPUT_DETECT_EN
	uint16_t adc_val = (uint16_t)ADC_SingleModeDataGet(ADC_CHANNEL_POWERKEY);
	uint8_t now = (adc_val > 4000) ? 1U : 0U;

	/* 去抖：连续 3 次(3s)同一状态才切换 */
	if (now == line2_det_cur) {
		if (line2_det_cnt < 3U) {
			line2_det_cnt++;
		}
	} else {
		line2_det_cur = now;
		line2_det_cnt = 1U;
	}
	if (line2_det_cnt >= 3U && line2_plugged != line2_det_cur) {
		line2_plugged = line2_det_cur;
		BG_EventAudioDetData_t det = { .port_id = 2, .connected = line2_plugged };
		BG_EVT_PUB_DATA(EVT_AUDIO_GUITAR_IN, &det, sizeof(det));
	}

	// /* 调试: 打印 POWERKEY ADC 原始值到 USB CDC */
	// 	{
	// 		char cdc_buf[32];
	// 		int n = snprintf(cdc_buf, sizeof(cdc_buf), "PWRKEY ADC: %u\r\n", (unsigned)adc_val);
	// 		if (n > 0)
	// 			OTG_DeviceCDC_Send((uint8_t *)cdc_buf, (uint16_t)n);
	// 	}
#else
	line2_plugged = 1;
#endif
}

/* ==================== MIC 插入检测（A30 下拉，低电平=已插入） ==================== */

/* MIC 数据放行稳定时间：检测到插入后延迟多久才放行数据（消除插入瞬态 pop） */
#define MIC_INSERT_STABLE_MS  1000U

/* MIC 数据放行状态机：由 ADC1_ReadMicData 每帧调用推进 */
static uint8_t  mic_stable = 0U;       /* 1 = 已插入且稳定期已过 */
static uint32_t mic_insert_tick = 0U;  /* 首次检测到插入的系统 tick（ms） */

/**
 * MIC 插入状态：A30 下拉，低电平 = 已插入。
 * 检测宏关闭时恒视为已插入。
 */
uint8_t BG_AudioDetection_MicIsPlugged(void)
{
#if MIC_INPUT_DETECT_EN
	return (GPIO_RegOneBitGet(GPIO_A_IN, GPIO_INDEX30) == 0) ? 1U : 0U;
#else
	return 1U;
#endif
}

/**
 * MIC 数据就绪：检测到插入后延迟 MIC_INSERT_STABLE_MS 毫秒才返回 1。
 * 未插入或稳定期内返回 0。需被周期调用以推进内部状态机。
 */
uint8_t BG_AudioDetection_MicReady(void)
{
#if MIC_INPUT_DETECT_EN
	uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

	if (!BG_AudioDetection_MicIsPlugged()) {
		/* 未插入：复位状态，下次插入重新计时 */
		mic_stable = 0U;
		mic_insert_tick = 0U;
		return 0U;
	}

	if (mic_stable) {
		return 1U;
	}

	/* 已插入但尚未稳定：记录首次插入时刻，等待稳定期结束 */
	if (mic_insert_tick == 0U) {
		mic_insert_tick = now_ms;
		return 0U;
	}

	if ((now_ms - mic_insert_tick) >= MIC_INSERT_STABLE_MS) {
		mic_stable = 1U;
		return 1U;
	}
	return 0U;
#else
	return 1U;
#endif
}
