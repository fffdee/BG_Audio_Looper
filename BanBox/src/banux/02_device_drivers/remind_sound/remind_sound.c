/**
 ******************************************************************************
 * @file    remind_sound.c
 * @brief   开机/事件提示音播放模块（基于 const 数组的 MP3 解码输出）
 *
 * 架构说明：
 *   - s_remind_table[] 是集中注册表，每条目包含 id/name/data/size
 *   - RemindSound_PlayByIndex(id) / PlayByName(name) 通过表查找后调用底层
 *   - RemindSound_Play() 是底层实现，含完整 DBG 日志
 *   - g_remind_sound_active 标志让 Effect Graph 暂停 DAC 写入，避免冲突
 *
 * 新增提示音步骤：
 *   1. 用 mp3_to_c_array.py 生成 .h / .c
 *   2. 在下方 #include 区域引入新 .h
 *   3. 在 s_remind_table[] 末尾追加一条 { id, "name", data, size }
 ******************************************************************************
 */

#include <stdbool.h>
#include <string.h>
#include "typedefine.h"
#include "audio_decoder_api.h"
#include "mvstdio.h"
#include "dac_interface.h"
#include "debug.h"
#include "remind_sound.h"
#include "rtos_api.h"
#include "adc.h"
#include "dac.h"
#include "gpio.h"

/* ---- 音频数据头文件（mp3_to_c_array.py 生成）---- */
#include "g_remind_on.h"
#include "g_remind_off.h"

/* ---- 解码器缓冲区大小 ---- */
#define REMIND_DECODER_BUF_SIZE  (19 * 1024)

/* ---- 连续解码失败计数上限 ---- */
#define REMIND_ERROR_MAX  3

/* ---- 解码器缓冲区（动态分配，仅在 Play 期间占用堆）---- */
/* 必须在 Audio_Init 之前（堆满 74KB 时）调用，或释放较大效果器后调用 */
static uint8_t *s_decoder_buf = NULL;

/* ---- Effect Graph DAC 互斥标志 ---- */
volatile uint8_t g_remind_sound_active = 0;

/* ============================================================
 * 提示音调用表 —— 在此处集中注册所有提示音
 * 新增提示音：追加一行 { id, "name", 数组名, 数组名_size }
 * ============================================================ */
/* vol_pct: 0-100，播放时的音量相对于电位器满量程的百分比
 * 100 = 跟随电位器；50 = 电位器音量的一半（抵消录音电平偏高） */
static const RemindSoundItem_t s_remind_table[] = {
    { 0, "on",  g_remind_on,  (uint32_t)0, 50  /* on:  源文件电平偏高，衰减至50% */ },
    { 1, "off", g_remind_off, (uint32_t)0, 100 /* off: 电平合适，满量程跟随电位器 */ },
};
/* size 字段在编译期常量不可用时通过 getter 取得，见下方 */
static const uint32_t s_remind_sizes[] = {
    /* 与 s_remind_table 顺序严格对应 */
    0, /* [0] on  — 下方 init 时填入 g_remind_on_size  */
    0, /* [1] off — 下方 init 时填入 g_remind_off_size */
};

#define REMIND_TABLE_COUNT  ((int)(sizeof(s_remind_table) / sizeof(s_remind_table[0])))

/* 运行期 size 缓存（因为 g_remind_on_size 是 extern const，链接后才有值）*/
static uint32_t s_sizes_cache[REMIND_TABLE_COUNT];
static uint8_t  s_table_inited = 0;

static void remind_table_init(void)
{
    if (s_table_inited) return;
    s_sizes_cache[0] = g_remind_on_size;
    s_sizes_cache[1] = g_remind_off_size;
    (void)s_remind_sizes; /* suppress unused warning */
    s_table_inited = 1;
}

/* ---- 当前播放的 const 数组指针及偏移 ---- */
static const uint8_t *s_mp3_data;
static uint32_t       s_mp3_size;
static uint32_t       s_mp3_offset;

/* ---- MemHandle（传给 audio_decoder_initialize 的 IO 句柄）---- */
static MemHandle s_mem_handle;


/* ------------------------------------------------------------
 * mv_mread 回调：从 const 数组读取 MP3 数据喂给解码器
 * ------------------------------------------------------------ */
static uint32_t RemindFillCallback(void *buffer, uint32_t length)
{
    uint32_t remain = s_mp3_size - s_mp3_offset;
    uint32_t read   = (length > remain) ? remain : length;

    if (read == 0)
        return 0;

    memcpy(buffer, s_mp3_data + s_mp3_offset, read);
    s_mp3_offset += read;
    return read;
}


/* ============================================================
 * 公开 API：表查询
 * ============================================================ */

int RemindSound_GetCount(void)
{
    return REMIND_TABLE_COUNT;
}

void RemindSound_ListAll(void)
{
    int i;
    remind_table_init();
    DBG("[Remind] ---- Remind Sound Table (%d items) ----\n", REMIND_TABLE_COUNT);
    for (i = 0; i < REMIND_TABLE_COUNT; i++) {
        DBG("[Remind]   [%d] %-12s  %lu bytes  vol=%d%%\n",
            s_remind_table[i].id,
            s_remind_table[i].name,
            (unsigned long)s_sizes_cache[i],
            (int)s_remind_table[i].vol_pct);
    }
    DBG("[Remind] -----------------------------------------\n");
}

int RemindSound_PlayByIndex(uint8_t id)
{
    int i;
    remind_table_init();
    for (i = 0; i < REMIND_TABLE_COUNT; i++) {
        if (s_remind_table[i].id == id) {
            DBG("[Remind] PlayByIndex(%d) -> \"%s\" (%lu bytes, vol=%d%%)\n",
                id, s_remind_table[i].name, (unsigned long)s_sizes_cache[i],
                (int)s_remind_table[i].vol_pct);
            RemindSound_Play(s_remind_table[i].data, s_sizes_cache[i],
                             s_remind_table[i].vol_pct);
            return 0;
        }
    }
    DBG("[Remind] PlayByIndex(%d): NOT FOUND (table has %d items)\n",
        id, REMIND_TABLE_COUNT);
    return -1;
}

int RemindSound_PlayByName(const char *name)
{
    int i;
    if (name == NULL) return -1;
    remind_table_init();
    for (i = 0; i < REMIND_TABLE_COUNT; i++) {
        if (strcmp(s_remind_table[i].name, name) == 0) {
            DBG("[Remind] PlayByName(\"%s\") -> id=%d (%lu bytes, vol=%d%%)\n",
                name, s_remind_table[i].id, (unsigned long)s_sizes_cache[i],
                (int)s_remind_table[i].vol_pct);
            RemindSound_Play(s_remind_table[i].data, s_sizes_cache[i],
                             s_remind_table[i].vol_pct);
            return 0;
        }
    }
    DBG("[Remind] PlayByName(\"%s\"): NOT FOUND\n", name);
    return -1;
}


/* ------------------------------------------------------------
 * 读取音量电位器原始值并换算为 DAC 音量寄存器值
 * ADC_CHANNEL_GPIOA28 返回 0~4095，与 SetVolume() 保持一致乘以4
 * ------------------------------------------------------------ */
static uint16_t remind_read_pot_dac_vol(void)
{
    uint16_t adc_val;
    GPIO_RegOneBitClear(GPIO_A_ANA_EN, GPIO_INDEX28);
    GPIO_RegOneBitSet(GPIO_A_ANA_EN, GPIO_INDEX28);
    adc_val = (uint16_t)ADC_SingleModeDataGet(ADC_CHANNEL_GPIOA28);
    return (uint16_t)(adc_val * 4);  /* 0~16380, DAC 寄存器最大值 0x3FFF=16383 */
}

/* ============================================================
 * 底层播放函数（阻塞，直到播完）
 * vol_pct: 0~100，相对于当前电位器音量的百分比
 * ============================================================ */
void RemindSound_Play(const uint8_t *mp3_data, uint32_t mp3_size, uint8_t vol_pct)
{
    int      error_cnt   = 0;
    uint32_t sample_rate = 0;
    uint16_t pot_vol     = 0;
    uint16_t play_vol    = 0;
    int32_t  decoder_type = MP3_DECODER;  /* 默认 MP3，后续根据 RIFF 头拦截修改 */

    if (mp3_data == NULL || mp3_size == 0) {
        DBG("[Remind] Play: invalid param (data=%p size=%lu)\n",
            (void*)mp3_data, (unsigned long)mp3_size);
        return;
    }

    /* 读取电位器当前音量，按 vol_pct 缩放后设置 DAC */
    pot_vol  = remind_read_pot_dac_vol();
    play_vol = (uint16_t)((uint32_t)pot_vol * vol_pct / 100);
    if (play_vol == 0) play_vol = 1;   /* 至少输出极小音量，避免静音 */
    AudioDAC_VolSet(DAC0, play_vol, play_vol);
    DBG("[Remind] Play: pot_vol=%u, vol_pct=%d%%, play_vol=%u\n",
        (unsigned)pot_vol, (int)vol_pct, (unsigned)play_vol);

    /* 暂停 Effect Graph 的 DAC 写入，独占 DAC FIFO */
    g_remind_sound_active = 1;

    /* 动态分配解码器缓冲区 */
    s_decoder_buf = (uint8_t *)osPortMalloc(REMIND_DECODER_BUF_SIZE);
    if (s_decoder_buf == NULL) {
        DBG("[Remind] Play: osPortMalloc(%d) FAILED, heap too small\n",
            REMIND_DECODER_BUF_SIZE);
        g_remind_sound_active = 0;
        return;
    }

    DBG("[Remind] Play: start, size=%lu, buf=%p\n",
        (unsigned long)mp3_size, (void*)s_decoder_buf);

    /* ---- 初始化播放状态 ---- */
    s_mp3_data   = mp3_data;
    s_mp3_size   = mp3_size;
    s_mp3_offset = 0;

    mv_mopen(&s_mem_handle, NULL, mp3_size, RemindFillCallback);

    /* ---- 自动检测音频格式（RIFF 头 = WAV，否则按 MP3 处理）---- */
    if (mp3_size >= 4 &&
        mp3_data[0] == 'R' && mp3_data[1] == 'I' &&
        mp3_data[2] == 'F' && mp3_data[3] == 'F') {
        decoder_type = WAV_DECODER;
        DBG("[Remind] Play: format=WAV\n");
    } else {
        DBG("[Remind] Play: format=MP3\n");
    }

    /* ---- 初始化解码器 ---- */
    if (audio_decoder_initialize(s_decoder_buf, &s_mem_handle,
                                 IO_TYPE_MEMORY, decoder_type) != RT_SUCCESS)
    {
        DBG("[Remind] Play: audio_decoder_initialize FAILED\n");
        osPortFree(s_decoder_buf);
        s_decoder_buf = NULL;
        g_remind_sound_active = 0;
        return;
    }

    /* 从 MP3 Header 读到的采样率，调整 DAC */
    sample_rate = (uint32_t)audio_decoder->song_info->sampling_rate;
    DBG("[Remind] Play: ch=%d, rate=%lu Hz, bitrate=%lu bps\n",
        (int)audio_decoder->song_info->num_channels,
        (unsigned long)sample_rate,
        (unsigned long)audio_decoder->song_info->bitrate);
    AudioDAC_SampleRateChange(ALL, sample_rate);

    /* ---- 解码循环，直到数据耗尽或连续 3 次出错 ---- */
    while (audio_decoder_can_continue() == RT_YES)
    {
        if (audio_decoder_decode() == RT_SUCCESS)
        {
            /* VBR：采样率可能逐帧变化 */
            if (sample_rate != (uint32_t)audio_decoder->song_info->sampling_rate)
            {
                sample_rate = (uint32_t)audio_decoder->song_info->sampling_rate;
                DBG("[Remind] Play: sample rate changed -> %lu Hz\n",
                    (unsigned long)sample_rate);
                AudioDAC_SampleRateChange(ALL, sample_rate);
            }

            error_cnt = 0;

            /* 实时跟踪电位器音量：每解码一帧重新读 ADC 并按 vol_pct 缩放更新 DAC */
            pot_vol  = remind_read_pot_dac_vol();
            play_vol = (uint16_t)((uint32_t)pot_vol * vol_pct / 100);
            if (play_vol == 0) play_vol = 1;
            AudioDAC_VolSet(DAC0, play_vol, play_vol);

            /* 等待 DAC FIFO 有足够空间 */
            while (AudioDAC0DataSpaceLenGet() <
                   audio_decoder->song_info->pcm_data_length)
                ;

            if (audio_decoder->song_info->num_channels == 1)
            {
                uint16_t  i;
                uint16_t *src = (uint16_t *)audio_decoder->song_info->pcm_addr;
                for (i = 0; i < audio_decoder->song_info->pcm_data_length; i++)
                {
                    uint16_t stereo[2];
                    stereo[0] = *src;
                    stereo[1] = *src;
                    AudioDAC0DataSet(stereo, 1);
                    src++;
                }
            }
            else
            {
                AudioDAC0DataSet(audio_decoder->song_info->pcm_addr,
                                 audio_decoder->song_info->pcm_data_length);
            }
        }
        else
        {
            if (++error_cnt >= REMIND_ERROR_MAX) {
                DBG("[Remind] Play: %d consecutive decode errors, abort\n",
                    REMIND_ERROR_MAX);
                break;
            }
        }
    }

    /* ---- 释放解码器缓冲区，恢复 Effect Graph DAC 写入权 ---- */
    osPortFree(s_decoder_buf);
    s_decoder_buf = NULL;
    g_remind_sound_active = 0;
    /* 恢复电位器音量（重新读取，避免旋钮在播放期间被调动）*/
    pot_vol = remind_read_pot_dac_vol();
    AudioDAC_VolSet(DAC0, pot_vol, pot_vol);
    AudioDAC_SampleRateChange(ALL, 44100);
    DBG("[Remind] Play: done, restored pot_vol=%u\n", (unsigned)pot_vol);
}
