/**
 ******************************************************************************
 * @file    power_on_music.c
 * @brief   开机音乐播放模块 — 实现
 *
 * 架构说明:
 *   - 直接 PCM 播放：开机音乐为已解码的 int16_t 立体声 PCM 数据，
 *     无需经过 audio_decoder，直接写入 DAC FIFO 播放
 *   - 播放期间通过 g_remind_sound_active 标志独占 DAC，Effect Graph 静默
 *   - 注册 Shell 命令 pwr_music 用于命令行测试
 *
 * Shell 命令用法:
 *   pwr_music -p / --play    播放开机音乐
 *   pwr_music -i / --info    显示音乐数据信息
 ******************************************************************************
 */

#include "power_on_music.h"
#include "product_def.h"        /* POWER_ON_MUSIC_EN, HW_VOLUME_ADC_EN, etc. */

#if POWER_ON_MUSIC_EN

#include "power_on.h"           /* const int16_t g_pwr_music_pcm[352800] */
#include "remind_sound.h"       /* g_remind_sound_active */
#include "bg_shell.h"
#include "dac_interface.h"      /* AudioDAC0DataSet, AudioDAC0DataSpaceLenGet,
                                   AudioDAC_SampleRateChange, AudioDAC_VolSet */
#include "dac.h"                /* DAC0, ALL, DAC_MODULE */
#include "adc.h"                /* ADC_SingleModeDataGet */
#include "gpio.h"               /* GPIO_RegOneBitClear/Set */
#include <stdlib.h>

/* ---- 开机音乐 PCM 参数（由 power_on.h 注释标定）---- */
#define PWR_MUSIC_SAMPLE_RATE    44100U
#define PWR_MUSIC_CHANNELS       2U
#define PWR_MUSIC_BIT_DEPTH      16U
#define PWR_MUSIC_STEREO_FRAMES  (sizeof(g_pwr_music_pcm) / sizeof(g_pwr_music_pcm[0]) / PWR_MUSIC_CHANNELS)

/* ================================================================
 * 辅助函数：读取音量电位器
 * ================================================================ */
static uint16_t pwr_music_read_pot_vol(void)
{
#if HW_VOLUME_ADC_EN
    uint16_t adc_val;
    GPIO_RegOneBitClear(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
    GPIO_RegOneBitSet(HW_VOLUME_ADC_GPIO_PORT, HW_VOLUME_ADC_GPIO_PIN);
    adc_val = (uint16_t)ADC_SingleModeDataGet(HW_VOLUME_ADC_CHANNEL);
    return (uint16_t)(adc_val * 4);
#else
    /* 无音量旋钮：返回最大音量 16383 (0x3FFF) */
    return 16383U;
#endif
}

/* ================================================================
 * 核心实现：直接 PCM 播放（阻塞）
 * ================================================================ */
void PowerOnMusic_Play(void)
{
    uint16_t pot_vol;
    uint16_t play_vol;
    uint32_t offset = 0;           /* 已写入的立体声帧数 */
    uint32_t fifo_timeout;

    /* 读取电位器并设置播放音量 */
    pot_vol  = pwr_music_read_pot_vol();
    play_vol = pot_vol;
    if (play_vol == 0) play_vol = 1;
    AudioDAC_VolSet(DAC0, play_vol, play_vol);

    /* 设置 DAC 为开机音乐的采样率 */
    AudioDAC_SampleRateChange(ALL, PWR_MUSIC_SAMPLE_RATE);

    /* 独占 DAC，暂停 Effect Graph 写入 */
    g_remind_sound_active = 1;

    /* ---- 逐块向 DAC FIFO 写 PCM 数据 ---- */
    while (offset < PWR_MUSIC_STEREO_FRAMES)
    {
        uint32_t space;
        uint32_t chunk;

        /* 等待 FIFO 有空间，带超时防 WDT 复位 */
        fifo_timeout = 2000000U;
        while ((space = (uint32_t)AudioDAC0DataSpaceLenGet()) == 0)
        {
            if (--fifo_timeout == 0) {
                goto pwr_music_done;
            }
        }

        chunk = (space < (PWR_MUSIC_STEREO_FRAMES - offset))
                    ? space : (PWR_MUSIC_STEREO_FRAMES - offset);

        /* g_pwr_music_pcm[] 为 int16_t 交错立体声 (L,R,L,R,...)，
         * offset*2 为 int16_t 索引，AudioDAC0DataSet 按立体声帧写入 */
        AudioDAC0DataSet((void *)&g_pwr_music_pcm[offset * 2], (uint16_t)chunk);
        offset += chunk;
    }

pwr_music_done:
    /* 恢复 Effect Graph DAC 写入权 */
    g_remind_sound_active = 0;

    /* 恢复电位器音量 & 采样率 */
    pot_vol = pwr_music_read_pot_vol();
    AudioDAC_VolSet(DAC0, pot_vol, pot_vol);
    AudioDAC_SampleRateChange(ALL, 44100);
}

/* ================================================================
 * Shell 命令处理函数
 * ================================================================ */

/**
 * @brief  pwr_music -p / --play  —  播放开机音乐
 */
static int pwr_music_play(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Printf("Playing power-on music (%u bytes, %u frames)...\r\n",
                 (unsigned int)sizeof(g_pwr_music_pcm),
                 (unsigned int)PWR_MUSIC_STEREO_FRAMES);
    PowerOnMusic_Play();
    Shell_Print("Done.\r\n");
    return 0;
}

/**
 * @brief  pwr_music -i / --info  —  显示音乐数据信息
 */
static int pwr_music_info(int argc, char *argv[])
{
    uint32_t pcm_bytes  = (uint32_t)sizeof(g_pwr_music_pcm);
    uint32_t frames     = (uint32_t)PWR_MUSIC_STEREO_FRAMES;
    uint32_t duration_ms = (uint32_t)((uint64_t)frames * 1000 / PWR_MUSIC_SAMPLE_RATE);

    Shell_Printf("---- Power-On Music Info ----\r\n");
    Shell_Printf("  Format     : PCM int16_t (direct DAC)\r\n");
    Shell_Printf("  Channels   : %u\r\n",   (unsigned int)PWR_MUSIC_CHANNELS);
    Shell_Printf("  Sample Rate: %u Hz\r\n", (unsigned int)PWR_MUSIC_SAMPLE_RATE);
    Shell_Printf("  Bit Depth  : %u bits\r\n",(unsigned int)PWR_MUSIC_BIT_DEPTH);
    Shell_Printf("  PCM Bytes  : %u\r\n",   (unsigned int)pcm_bytes);
    Shell_Printf("  Frames     : %u\r\n",   (unsigned int)frames);
    Shell_Printf("  Duration   : %u ms\r\n", (unsigned int)duration_ms);
    Shell_Printf("-----------------------------\r\n");
    return 0;
}

/* ================================================================
 * Shell 模块注册
 * ================================================================ */

static const ShellOpt_t pwr_music_opts[] = {
    OPT("p", "play", NULL,   "Play power-on music",          pwr_music_play),
    OPT("i", "info", NULL,   "Show power-on music info",     pwr_music_info),
    OPT_END()
};

DEFINE_MODULE(pwr_music, "Power-on music test", MOD_CAT_DEBUG, pwr_music_opts);

void ShellCmdPowerOnMusic_Register(void)
{
    REGISTER_MODULE(pwr_music);
}

#else /* !POWER_ON_MUSIC_EN — 模块禁用，提供空桩函数以节省 ~814KB Flash */

void PowerOnMusic_Play(void) {}

void ShellCmdPowerOnMusic_Register(void) {}

#endif /* POWER_ON_MUSIC_EN */
