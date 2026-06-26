/**
 ******************************************************************************
 * @file    power_on_music.c
 * @brief   开机音乐播放模块 — 实现
 *
 * 架构说明:
 *   - 底层复用 RemindSound_Play() 解码器（自动识别 WAV/MP3，阻塞式播放）
 *   - 播放期间通过 g_remind_sound_active 标志独占 DAC，Effect Graph 静默
 *   - 注册 Shell 命令 pwr_music 用于命令行测试
 *
 * Shell 命令用法:
 *   pwr_music -p / --play    播放开机音乐
 *   pwr_music -i / --info    显示音乐数据信息
 ******************************************************************************
 */

#include "power_on_music.h"
#include "g_power_on_wav.h"
#include "remind_sound.h"
#include "bg_shell.h"
#include <stdlib.h>

/* ================================================================
 * 内部实现
 * ================================================================ */

void PowerOnMusic_Play(void)
{
    RemindSound_Play(g_power_on_wav, g_power_on_wav_size, 100);
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
    Shell_Printf("Playing power-on music (%u bytes)...\r\n",
                 (unsigned int)g_power_on_wav_size);
    PowerOnMusic_Play();
    Shell_Print("Done.\r\n");
    return 0;
}

/**
 * @brief  pwr_music -i / --info  —  显示音乐数据信息
 */
static int pwr_music_info(int argc, char *argv[])
{
    const uint8_t *data = g_power_on_wav;
    uint32_t size = g_power_on_wav_size;

    Shell_Printf("---- Power-On Music Info ----\r\n");
    Shell_Printf("  Data size  : %u bytes\r\n", (unsigned int)size);

    if (size >= 44 && data[0] == 'R' && data[1] == 'I' &&
                     data[2] == 'F' && data[3] == 'F') {
        /* 解析 WAV 头 */
        uint16_t audio_format   = data[20] | (data[21] << 8);
        uint16_t num_channels   = data[22] | (data[23] << 8);
        uint32_t sample_rate    = data[24] | (data[25] << 8) |
                                  (data[26] << 16) | (data[27] << 24);
        uint16_t bits_per_sample = data[34] | (data[35] << 8);
        uint32_t data_chunk_size = data[40] | (data[41] << 8) |
                                   (data[42] << 16) | (data[43] << 24);

        Shell_Printf("  Format     : WAV (PCM=%u)\r\n", audio_format);
        Shell_Printf("  Channels   : %u\r\n", num_channels);
        Shell_Printf("  Sample Rate: %u Hz\r\n", (unsigned int)sample_rate);
        Shell_Printf("  Bit Depth  : %u bits\r\n", bits_per_sample);
        Shell_Printf("  PCM Bytes  : %u\r\n", (unsigned int)data_chunk_size);
        Shell_Printf("  Duration   : %u ms\r\n",
                     (unsigned int)((uint64_t)data_chunk_size * 1000 /
                                    ((uint64_t)sample_rate * num_channels *
                                     (bits_per_sample / 8))));
    } else {
        Shell_Printf("  Format     : MP3 or unknown\r\n");
    }

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
