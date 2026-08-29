/**
 * @file bg_audio_legacy.c
 * @brief Legacy audio loop (non-Effect-Graph fallback path).
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bg_audio_io_internal.h"
#include "product_def.h"
#include "debug.h"

#include "gpio.h"
#include "audio_adc.h"
#include "dac_interface.h"
#include "audio_effect.h"
#include "ctrlvars.h"
#include "usb_audio_api.h"
#include "audio_decoder_api.h"
#include "bt_manager.h"
#include "audio_looper.h"
#include "reverb.h"
#include "pitch_shift.h"

#if !USE_EFFECT_GRAPH_MODE

static void ReadAudioData(uint16_t len, uint16_t *pRealLen)
{
	uint16_t RealLen = 0;
	uint16_t i;

	RealLen = AudioADC_DataGet(ADC1_MODULE, BG_AudioManager.Audio_data.mic_buf_in, len);
	if (len > RealLen)
		len = RealLen;

	RealLen = AudioADC_DataGet(ADC0_MODULE, BG_AudioManager.Audio_data.guitar_buf_in, len);
	if (len > RealLen)
		len = RealLen;

	// 混合吉他和麦克风信号
	for (i = 0; i < len; i++)
	{
		BG_AudioManager.Audio_data.OutPut_buf[i] =
			BG_AudioManager.Audio_data.guitar_buf_in[i] +
			BG_AudioManager.Audio_data.mic_buf_in[i];
	}

	*pRealLen = len;
}

/**
 * 应用音频效果
 */
static void ApplyAudioEffects(uint16_t len)
{
	static uint32_t temp_buf1[512];
	static uint32_t temp_buf2[512];
	
	// 效果链处理顺序：
	// 输入(OutPut_buf) -> 扩展器 -> DRC -> EQ -> 混响 -> 输出(guitar_buf_out)

	// 1. 扩展器（Expander）- 处理动态范围的扩展部分
	if (gCtrlVars.mic_expander_unit.enable)
	{
		AudioEffectExpanderApply(&gCtrlVars.mic_expander_unit,
		                         (int16_t *)BG_AudioManager.Audio_data.OutPut_buf,
		                         (int16_t *)temp_buf1,
		                         len);
	}
	else
	{
		memcpy(temp_buf1, BG_AudioManager.Audio_data.OutPut_buf, len * sizeof(uint32_t));
	}

	// 2. 动态范围压缩（DRC）- 压缩音频动态范围
	#if CFG_AUDIO_EFFECT_MIC_DRC_EN
	if (gCtrlVars.mic_drc_unit.enable)
	{
		AudioEffectDRCApply(&gCtrlVars.mic_drc_unit,
		                    (int16_t *)temp_buf1,
		                    (int16_t *)temp_buf2,
		                    len);
	}
	else
	#endif
	{
		memcpy(temp_buf2, temp_buf1, len * sizeof(uint32_t));
	}
	

	if (gCtrlVars.mic_out_eq_unit.enable)
	{
		AudioEffectEQApply(&gCtrlVars.mic_out_eq_unit,
		                   (int16_t *)temp_buf2,
		                   (int16_t *)temp_buf1,
		                   len,
		                   2);
	}
	else
	{
		memcpy(temp_buf1, temp_buf2, len * sizeof(uint32_t));
	}
	
	// 4. 啸叫抑制（Howling Detector）
	#if CFG_AUDIO_EFFECT_MIC_HOWLING_DECTOR_EN
	if (gCtrlVars.howling_dector_unit.enable)
	{
		AudioEffectHowlingSuppressorApply(&gCtrlVars.howling_dector_unit,
		                                  (int16_t *)temp_buf1,
		                                  (int16_t *)temp_buf2,
		                                  len);
	}
	else
	#endif
	{
		memcpy(temp_buf2, temp_buf1, len * sizeof(uint32_t));
	}
	
	// 5. 混响（Reverb）- 最后添加空间感
	if (gCtrlVars.reverb_unit.enable)
	{
		AudioEffectReverbApply(&gCtrlVars.reverb_unit,
		                       (int16_t *)temp_buf2,
		                       (int16_t *)BG_AudioManager.Audio_data.guitar_buf_out,
		                       len);
	}
	else
	{
		memcpy(BG_AudioManager.Audio_data.guitar_buf_out, temp_buf2, len * sizeof(uint32_t));
	}
}

void handle_usb_record(uint16_t len)
{
	if(usb_mic_enable)
	{
		 UsbAudioMicDataSet(BG_AudioManager.Audio_data.OutPut_buf,len);
	}

}
/**
 * 构建最终输出（考虑USB音频）
 */
static void BuildFinalOutput(uint16_t len, uint32_t *bt_audio_buffer)
{
	uint16_t i;
	uint16_t usb_data_len;
	if (usb_speaker_enable)
	{
		usb_data_len = UsbAudioSpeakerDataLenGet();
		if (usb_data_len >= len)
		{
			/* USB数据充足，正常读取 */
			UsbAudioSpeakerDataGet(BG_AudioManager.Audio_data.USB_dac_buf, len);
		}
		else if (usb_data_len > 0)
		{
			/* USB数据不足，读取可用数据，剩余填零 */
			UsbAudioSpeakerDataGet(BG_AudioManager.Audio_data.USB_dac_buf, usb_data_len);
			for (i = usb_data_len; i < len; i++)
			{
				BG_AudioManager.Audio_data.USB_dac_buf[i] = 0;
			}
		}
		else
		{
			/* USB无数据，全部填零避免噪声 */
			for (i = 0; i < len; i++)
			{
				BG_AudioManager.Audio_data.USB_dac_buf[i] = 0;
			}
		}
		for (i = 0; i < len; i++)
		{
			BG_AudioManager.Audio_data.OutPut_buf[i] =
				BG_AudioManager.Audio_data.guitar_buf_out[i] +
				BG_AudioManager.Audio_data.USB_dac_buf[i] +
				bt_audio_buffer[i];
		}
	}
	else
	{
		for (i = 0; i < len; i++)
		{
			BG_AudioManager.Audio_data.OutPut_buf[i] =
				BG_AudioManager.Audio_data.guitar_buf_out[i] +
				BG_AudioManager.Audio_data.mic_buf_out[i] +
				bt_audio_buffer[i];
		}
	}
}

/**
 * 输出音频数据到DAC
 */
static void OutputAudioData(uint16_t len)
{
#if BG_PITCH_SHIFT_TEST_EN
	PitchShift_ProcessPackedStereo(PitchShift_GetDefault(),
	                               BG_AudioManager.Audio_data.OutPut_buf,
	                               (int32_t)len);
#endif
	AudioDAC_DataSet(DAC0, BG_AudioManager.Audio_data.OutPut_buf, len);
}

/**
 * 音频主循环（有蓝牙音频数据时）
 */
void AudioLoopWithBT(uint32_t *bt_audio_buffer)
{
	static uint32_t last_bt_sample_rate = 0;  /* 上次蓝牙采样率，用于检测变化 */
	static uint16_t s_gpio_div_bt = 0;        /* GPIO 检测降频计数器 */
	uint16_t RealLen = 0;
	uint16_t n = 0;
	uint16_t i;

	if (RT_SUCCESS == audio_decoder_can_continue())
	{
		if (mv_msize(&SBC_MemHandle) <= SBC_DECODER_FIFO_MIN)
			return;

		if (audio_decoder_decode() == RT_SUCCESS)
		{
			n = audio_decoder->song_info->pcm_data_length;

			/* 检查采样率是否变化，如果变化则同步 DAC/ADC */
			if (last_bt_sample_rate != audio_decoder->song_info->sampling_rate)
			{
				last_bt_sample_rate = audio_decoder->song_info->sampling_rate;
				BG_AudioManager.Audio_data.SampleRate = last_bt_sample_rate;
				DBG("[BT] Sample rate: %ld Hz, syncing DAC/ADC...\n", (long)last_bt_sample_rate);

				/* 同步 DAC 采样率 */
				AudioDAC_SampleRateChange(DAC0, last_bt_sample_rate);
				AudioDAC_SampleRateChange(DAC1, last_bt_sample_rate);

				/* 同步 ADC 采样率 */
				AudioADC_SampleRateSet(ADC0_MODULE, last_bt_sample_rate);
				AudioADC_SampleRateSet(ADC1_MODULE, last_bt_sample_rate);
			}
			


			/* 转换PCM数据到bt_audio_buffer */
			for (i = 0; i < n; i++)
				bt_audio_buffer[i] = (uint32_t)audio_decoder->song_info->pcm_addr[i];

			/* 等待DAC有足够空间 */
			while (AudioDAC0DataSpaceLenGet() < n);
			
			/* 等待ADC有足够数据（确保吉他/麦克风数据准备好） */
			while (AudioADC_DataLenGet(ADC0_MODULE) < n || AudioADC_DataLenGet(ADC1_MODULE) < n);

			ReadAudioData(n, &RealLen);
			ApplyAudioEffects(RealLen);
			if (++s_gpio_div_bt >= 50)
			{
				s_gpio_div_bt = 0;
				//ocessGuitarOutput();
				ProcessMicOutput();
				ProcessSpeakerSwitch();
			}
			BuildFinalOutput(RealLen, bt_audio_buffer);
			handle_usb_record(RealLen);
			OutputAudioData(RealLen);
		}
	}
}

/**
 * 音频主循环（无蓝牙音频，最小缓冲处理）
 */
void AudioLoopMinimal(uint32_t *bt_audio_buffer)
{
	uint16_t RealLen = 0;
	uint16_t i;
	const uint16_t MIN_SAMPLE = 48;
	/* GPIO 检测降频：插拔事件是慢速 DC 事件，每 50 帧 (~50ms) 检测一次即可，
	 * 避免每帧 GPIO 写操作产生 ~1kHz 方波，耦合到 ADC 输入造成高频底噪 */
	static uint16_t s_gpio_div_minimal = 0;

	while(AudioADC_DataLenGet(ADC0_MODULE) >= MIN_SAMPLE)
	{
		ReadAudioData(MIN_SAMPLE, &RealLen);
		ApplyAudioEffects(RealLen);
		if (++s_gpio_div_minimal >= 50)
		{
			s_gpio_div_minimal = 0;
			ProcessGuitarOutput();
			ProcessMicOutput();
			ProcessSpeakerSwitch();
		}

		/* Looper录制处理 - 根据各段配置的录制源分别采集:
		 * ALL_MIX → guitar_buf_out (混音后信号)
		 * MIC_L/R → mic_buf_in (ADC1 原始)
		 * LINEIN_L/R → guitar_buf_in (ADC0 原始) */
		g_looper_src_mic    = BG_AudioManager.Audio_data.mic_buf_in;
		g_looper_src_linein = BG_AudioManager.Audio_data.guitar_buf_in;
		if (AudioLooper.IsRecording())
		{
			AudioLooper.ProcessRecording32(BG_AudioManager.Audio_data.guitar_buf_out,
			                               looper_flash_buffer, RealLen);
		}

		/* 清零播放缓冲区 */
		for (i = 0; i < RealLen; i++)
		{
			looper_playback_buffer[i] = 0;
		}
		
		/* Looper播放处理 */
		if (AudioLooper.IsPlaying())
		{
			AudioLooper.ProcessPlayback32(looper_playback_buffer, looper_flash_buffer, RealLen);
		}
		
		/* 混合节拍器音频（独立于播放，节拍器可单独使用） */
		if (AudioLooper.MetronomeIsEnabled())
		{
			metronome_process_audio(looper_playback_buffer, RealLen);
		}

		if (usb_speaker_enable)
		{
			uint16_t usb_data_len = UsbAudioSpeakerDataLenGet();
			if (usb_data_len >= RealLen)
			{
				/* USB数据充足，正常读取 */
				UsbAudioSpeakerDataGet(BG_AudioManager.Audio_data.USB_dac_buf, RealLen);
			}
//			else if (usb_data_len > 0)
//			{
//				/* USB数据不足，读取可用数据，剩余填零 */
//				UsbAudioSpeakerDataGet(BG_AudioManager.Audio_data.USB_dac_buf, usb_data_len);
//				for (i = usb_data_len; i < RealLen; i++)
//				{
//					BG_AudioManager.Audio_data.USB_dac_buf[i] = 0;
//				}
//			}
			else
			{
				/* USB无数据，全部填零避免噪声 */
				for (i = 0; i < RealLen; i++)
				{
					BG_AudioManager.Audio_data.USB_dac_buf[i] = 0;
				}
			}
			for (i = 0; i < RealLen; i++)
			{
				/* 逐声道饱和加法：guitar_out + USB + looper */
				int32_t acc_l = (int16_t)(BG_AudioManager.Audio_data.guitar_buf_out[i] & 0xFFFF)
				              + (int16_t)(BG_AudioManager.Audio_data.USB_dac_buf[i]    & 0xFFFF)
				              + (int16_t)(looper_playback_buffer[i]                    & 0xFFFF);
				int32_t acc_r = (int16_t)((BG_AudioManager.Audio_data.guitar_buf_out[i] >> 16) & 0xFFFF)
				              + (int16_t)((BG_AudioManager.Audio_data.USB_dac_buf[i]    >> 16) & 0xFFFF)
				              + (int16_t)((looper_playback_buffer[i]                    >> 16) & 0xFFFF);
				if (acc_l >  32767) acc_l =  32767;
				if (acc_l < -32768) acc_l = -32768;
				if (acc_r >  32767) acc_r =  32767;
				if (acc_r < -32768) acc_r = -32768;
				BG_AudioManager.Audio_data.OutPut_buf[i] =
					((uint32_t)(uint16_t)(int16_t)acc_r << 16) | ((uint16_t)(int16_t)acc_l & 0xFFFF);
			}
		}
		else
		{
			for (i = 0; i < RealLen; i++)
			{
				/* 逐声道饱和加法：guitar_out + mic + looper */
				int32_t acc_l = (int16_t)(BG_AudioManager.Audio_data.guitar_buf_out[i] & 0xFFFF)
				              + (int16_t)(BG_AudioManager.Audio_data.mic_buf_in[i]    & 0xFFFF)
				              + (int16_t)(looper_playback_buffer[i]                  & 0xFFFF);
				int32_t acc_r = (int16_t)((BG_AudioManager.Audio_data.guitar_buf_out[i] >> 16) & 0xFFFF)
				              + (int16_t)((BG_AudioManager.Audio_data.mic_buf_in[i]    >> 16) & 0xFFFF)
				              + (int16_t)((looper_playback_buffer[i]                  >> 16) & 0xFFFF);
				if (acc_l >  32767) acc_l =  32767;
				if (acc_l < -32768) acc_l = -32768;
				if (acc_r >  32767) acc_r =  32767;
				if (acc_r < -32768) acc_r = -32768;
				BG_AudioManager.Audio_data.OutPut_buf[i] =
					((uint32_t)(uint16_t)(int16_t)acc_r << 16) | ((uint16_t)(int16_t)acc_l & 0xFFFF);
			}
		}
		handle_usb_record(RealLen);
		OutputAudioData(RealLen);

#if LOOPER_IO_BUFFER_ENABLE
		/* 音频已输出到DAC，现在安全执行Flash IO（刷写缓冲 + 填读缓存）
		 * 即使Flash写入耗时较长(~0.8ms)也不影响当帧音频输出 */
		looper_flush_io();
#endif
	}
}

#endif /* !USE_EFFECT_GRAPH_MODE */
