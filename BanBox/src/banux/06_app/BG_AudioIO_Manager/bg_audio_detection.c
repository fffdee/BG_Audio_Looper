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

#include "gpio.h"

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
