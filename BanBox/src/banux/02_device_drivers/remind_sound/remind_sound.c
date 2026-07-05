/**
 ******************************************************************************
 * @file    remind_sound.c
 * @brief   提示音播放模块（非阻塞，通过 Effect Graph 音频系统输出）
 *
 * 架构说明：
 *   - RemindSound_Start() 初始化解码器并设置播放状态，立即返回
 *   - RemindSound_GenerateAudio() 由 Effect Graph 的 REMIND 源节点在
 *     每个 Audio_loop() 周期调用，每次解码一小帧 PCM 数据
 *   - 解码后的 PCM 经过 USB_BT_MIXER → USB_BT_EQ → FINAL_MIXER → DAC0
 *     输出，与 USB/BT/节拍器音源混音，不再独占 DAC
 *   - 播放完成后自动释放资源，无需手动清理
 *   - 不再阻塞，不再需要 g_remind_sound_active 互斥标志
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
#include "debug.h"
#include "remind_sound.h"
#include "product_def.h"
#include "rtos_api.h"  /* osPortMalloc / osPortFree */

/* ---- 音频数据头文件（mp3_to_c_array.py 生成）---- */
#include "g_remind_power_on.h"

/* ---- 解码器缓冲区大小 ---- */
#define REMIND_DECODER_BUF_SIZE  (19 * 1024)

/* ---- 淡入参数 ---- */
#define REMIND_FADE_IN_MS        50    /* 淡入时长 50ms，消除爆破音 */
#define REMIND_FADE_IN_FRAMES    ((uint32_t)((uint64_t)REMIND_FADE_IN_MS * 44100 / 1000U))
/* 44100Hz * 50ms ≈ 2205 帧 */

/* ---- 解码重试参数 ---- */
#define REMIND_DECODE_RETRIES    10    /* 单次 GenerateAudio 内最大解码重试次数 */
#define REMIND_ERROR_MAX         30    /* 连续解码失败上限（跨多个 GenerateAudio 周期） */

/* ============================================================
 * 提示音调用表 —— 在此处集中注册所有提示音
 * 新增提示音：追加一行 { id, "name", 数组名, 数组名_size, vol_pct }
 * ============================================================ */
static const RemindSoundItem_t s_remind_table[] = {
    { 0, "on",  g_remind_power_on, (uint32_t)0, 30  /* on:  vol_pct=30% */ },
};

/* size 字段在编译期常量不可用时通过 getter 取得，见下方 */
static const uint32_t s_remind_sizes[] = {
    0, /* [0] on — 下方 init 时填入 g_remind_power_on_size */
};

#define REMIND_TABLE_COUNT  ((int)(sizeof(s_remind_table) / sizeof(s_remind_table[0])))

/* 运行期 size 缓存（因为 g_remind_power_on_size 是 extern const，链接后才有值）*/
static uint32_t s_sizes_cache[REMIND_TABLE_COUNT];
static uint8_t  s_table_inited = 0;

static void remind_table_init(void)
{
    if (s_table_inited) return;
    s_sizes_cache[0] = g_remind_power_on_size;
    (void)s_remind_sizes; /* suppress unused warning */
    s_table_inited = 1;
}

/* ============================================================
 * 播放状态机
 * ============================================================ */
typedef enum {
    REMIND_STATE_IDLE = 0,      /* 空闲，未播放 */
    REMIND_STATE_PLAYING,       /* 正在解码播放 */
    REMIND_STATE_DONE           /* 播放结束，待清理 */
} RemindState_t;

static RemindState_t s_state = REMIND_STATE_IDLE;

/* 当前播放的数据源 */
static const uint8_t *s_audio_data;
static uint32_t       s_audio_size;
static uint32_t       s_audio_offset;
static uint8_t        s_vol_pct;

/* 解码器缓冲区（动态分配，播放时从堆分配，结束释放）
 * RAM优化: 改为动态分配，空闲时释放19KB给EQ等效果器使用 */
static uint8_t *s_decoder_buf = NULL;
static uint8_t s_buf_in_use = 0;  /* 缓冲区是否被占用 */

/* MemHandle（传给 audio_decoder_initialize 的 IO 句柄） */
static MemHandle s_mem_handle;

/* 解码后 PCM 缓冲区（用于 GenerateAudio 时暂存解码结果） */
static int16_t  s_pcm_buf[640 * 2];  /* 640 帧 × 2 声道，约 2560 字节 */
static uint16_t s_pcm_frames;        /* s_pcm_buf 中有效帧数 */
static uint16_t s_pcm_read_pos;      /* 当前读到哪一帧 */
static int32_t  s_decoder_type = MP3_DECODER;

/* 连续解码失败计数 */
static int s_error_cnt = 0;

/* 淡入计数器：播放开始后从 0 递增，达到 REMIND_FADE_IN_FRAMES 后不再淡入 */
static uint32_t s_fade_pos = 0;

/* ------------------------------------------------------------
 * mv_mread 回调：从 const 数组读取音频数据喂给解码器
 * ------------------------------------------------------------ */
static uint32_t RemindFillCallback(void *buffer, uint32_t length)
{
    uint32_t remain = s_audio_size - s_audio_offset;
    uint32_t read   = (length > remain) ? remain : length;

    if (read == 0)
        return 0;

    memcpy(buffer, s_audio_data + s_audio_offset, read);
    s_audio_offset += read;
    return read;
}

/* ------------------------------------------------------------
 * 内部：解码一帧 PCM 数据到 s_pcm_buf
 * 返回：解码的帧数，0=结束/出错
 * ------------------------------------------------------------ */
static uint16_t decode_one_frame(void)
{
    if (audio_decoder_can_continue() != RT_YES) {
        return 0;
    }

    if (audio_decoder_decode() != RT_SUCCESS) {
        int32_t err = audio_decoder_get_error_code();
        s_error_cnt++;
        DBG("[Remind] decode error #%d, err=%ld\n", s_error_cnt, (long)err);
        if (s_error_cnt >= REMIND_ERROR_MAX) {
            DBG("[Remind] %d consecutive decode errors, stopping\n", REMIND_ERROR_MAX);
            return 0;
        }
        return 0;  /* 返回 0 让调用方继续尝试 */
    }

    s_error_cnt = 0;
    return (uint16_t)audio_decoder->song_info->pcm_data_length;
}

/* ------------------------------------------------------------
 * 内部：清理播放资源
 * ------------------------------------------------------------ */
static void remind_cleanup(void)
{
    if (s_decoder_buf != NULL) {
        osPortFree(s_decoder_buf);
        s_decoder_buf = NULL;
    }
    s_buf_in_use = 0;
    s_state = REMIND_STATE_IDLE;
    s_pcm_frames = 0;
    s_pcm_read_pos = 0;
    s_error_cnt = 0;
    s_fade_pos = 0;
    DBG("[Remind] Playback finished, resources released\n");
}

/* ============================================================
 * 公开 API
 * ============================================================ */

void RemindSound_Init(void)
{
    remind_table_init();
    s_state = REMIND_STATE_IDLE;
    s_buf_in_use = 0;
}

int RemindSound_Start(const char *name)
{
    int i;
    if (name == NULL) return -1;
    remind_table_init();

    /* 正在播放，不中断 */
    if (s_state == REMIND_STATE_PLAYING) return -1;

    for (i = 0; i < REMIND_TABLE_COUNT; i++) {
        if (strcmp(s_remind_table[i].name, name) == 0) {
            return RemindSound_StartById(s_remind_table[i].id);
        }
    }
    DBG("[Remind] Start(\"%s\"): NOT FOUND\n", name);
    return -1;
}

int RemindSound_StartById(uint8_t id)
{
    int i;
    remind_table_init();

    if (s_state == REMIND_STATE_PLAYING) return -1;

    for (i = 0; i < REMIND_TABLE_COUNT; i++) {
        if (s_remind_table[i].id == id) {
            int32_t dec_type = MP3_DECODER;
            const uint8_t *data = s_remind_table[i].data;
            uint32_t size = s_sizes_cache[i];

            if (data == NULL || size == 0) return -1;

            DBG("[Remind] StartById(%d) -> \"%s\" (%lu bytes, vol=%d%%)\n",
                id, s_remind_table[i].name, (unsigned long)size,
                (int)s_remind_table[i].vol_pct);

            /* 设置数据源 */
            s_audio_data    = data;
            s_audio_size    = size;
            s_audio_offset  = 0;
            s_vol_pct       = s_remind_table[i].vol_pct;
            s_pcm_frames    = 0;
            s_pcm_read_pos  = 0;
            s_error_cnt     = 0;
            s_fade_pos      = 0;

            /* 动态分配解码器缓冲区 */
            if (s_buf_in_use || s_decoder_buf != NULL) {
                DBG("[Remind] decoder buffer busy\n");
                return -1;
            }
            s_decoder_buf = (uint8_t *)osPortMalloc(REMIND_DECODER_BUF_SIZE);
            if (s_decoder_buf == NULL) {
                DBG("[Remind] decoder buffer malloc FAILED (%d bytes)\n", REMIND_DECODER_BUF_SIZE);
                return -1;
            }
            s_buf_in_use = 1;
            memset(s_decoder_buf, 0, REMIND_DECODER_BUF_SIZE);

            /* 初始化 MemHandle */
            mv_mopen(&s_mem_handle, NULL, size, RemindFillCallback);

            /* 检测格式 */
            if (size >= 4 &&
                data[0] == 'R' && data[1] == 'I' &&
                data[2] == 'F' && data[3] == 'F') {
                dec_type = WAV_DECODER;
                DBG("[Remind] format=WAV\n");
            } else {
                DBG("[Remind] format=MP3\n");
            }
            s_decoder_type = dec_type;

            /* 初始化解码器 */
            if (audio_decoder_initialize(s_decoder_buf, &s_mem_handle,
                                         IO_TYPE_MEMORY, dec_type) != RT_SUCCESS)
            {
                DBG("[Remind] audio_decoder_initialize FAILED\n");
                s_buf_in_use = 0;
                return -1;
            }

            DBG("[Remind] ch=%d, rate=%lu Hz, bitrate=%lu bps\n",
                (int)audio_decoder->song_info->num_channels,
                (unsigned long)audio_decoder->song_info->sampling_rate,
                (unsigned long)audio_decoder->song_info->bitrate);

            s_state = REMIND_STATE_PLAYING;
            return 0;
        }
    }
    DBG("[Remind] StartById(%d): NOT FOUND\n", id);
    return -1;
}

void RemindSound_Stop(void)
{
    if (s_state != REMIND_STATE_IDLE) {
        remind_cleanup();
    }
}

int RemindSound_IsPlaying(void)
{
    return (s_state == REMIND_STATE_PLAYING) ? 1 : 0;
}

uint16_t RemindSound_GetAvailableData(void)
{
    if (s_state != REMIND_STATE_PLAYING) {
        return 0;
    }
    /* 提示音始终可以提供数据（解码或返回静音） */
    return 48;
}

uint16_t RemindSound_GenerateAudio(uint32_t *out_buf, uint16_t max_len)
{
    uint16_t i;
    uint16_t generated = 0;
    int retry;

    if (max_len > 640) {
        max_len = 640;
    }

    /* 空闲状态：输出静音 */
    if (s_state != REMIND_STATE_PLAYING) {
        for (i = 0; i < max_len; i++) {
            out_buf[i] = 0;
        }
        return max_len;
    }

    /* 音量缩放因子（Q15 定点） */
    int16_t vol_q15 = (int16_t)((uint32_t)s_vol_pct * 32767 / 100);

    /* 逐样本填充输出缓冲区 */
    generated = 0;
    while (generated < max_len)
    {
        /* 如果 PCM 缓冲区已读完，解码下一帧 */
        if (s_pcm_read_pos >= s_pcm_frames) {
            uint16_t decoded_frames = 0;

            /* 检查是否还有数据可解码 */
            if (audio_decoder_can_continue() != RT_YES) {
                /* 播放结束，剩余填静音 */
                for (i = generated; i < max_len; i++) {
                    out_buf[i] = 0;
                }
                remind_cleanup();
                return max_len;
            }

            /* 带重试的解码：WAV 首帧可能返回 pcm_data_length=0，
             * 需要再次调用 decode 才能拿到实际 PCM 数据 */
            for (retry = 0; retry < REMIND_DECODE_RETRIES; retry++) {
                decoded_frames = decode_one_frame();
                if (decoded_frames > 0) {
                    break;  /* 拿到数据，退出重试 */
                }
                if (s_error_cnt >= REMIND_ERROR_MAX) {
                    /* 连续出错太多，停止 */
                    for (i = generated; i < max_len; i++) {
                        out_buf[i] = 0;
                    }
                    remind_cleanup();
                    return max_len;
                }
                /* decoded_frames==0 但未超错误上限，继续重试 */
            }

            if (decoded_frames == 0) {
                /* 重试耗尽仍无数据，输出静音帧让调用方下周期继续轮询 */
                for (i = generated; i < max_len; i++) {
                    out_buf[i] = 0;
                }
                return max_len;
            }

            /* 从解码器拷贝 PCM 到内部缓冲区 */
            {
                uint16_t ch = audio_decoder->song_info->num_channels;
                int16_t *src = (int16_t *)audio_decoder->song_info->pcm_addr;

                s_pcm_frames = decoded_frames;
                s_pcm_read_pos = 0;

                /* 将解码器输出转为立体声 int16 格式存入 s_pcm_buf
                 * 格式: s_pcm_buf[2*n] = L, s_pcm_buf[2*n+1] = R */
                if (ch == 1) {
                    /* 单声道复制到双声道 */
                    for (i = 0; i < decoded_frames && i < 640; i++) {
                        int16_t sample = src[i];
                        s_pcm_buf[i * 2]     = (int16_t)((int32_t)sample * vol_q15 >> 15);
                        s_pcm_buf[i * 2 + 1] = s_pcm_buf[i * 2];
                    }
                } else {
                    /* 立体声 */
                    for (i = 0; i < decoded_frames && i < 640; i++) {
                        int16_t l = src[i * 2];
                        int16_t r = src[i * 2 + 1];
                        s_pcm_buf[i * 2]     = (int16_t)((int32_t)l * vol_q15 >> 15);
                        s_pcm_buf[i * 2 + 1] = (int16_t)((int32_t)r * vol_q15 >> 15);
                    }
                }
            }
        }

        /* 从 PCM 缓冲区填充输出 */
        if (s_pcm_read_pos < s_pcm_frames) {
            int16_t l = s_pcm_buf[s_pcm_read_pos * 2];
            int16_t r = s_pcm_buf[s_pcm_read_pos * 2 + 1];

            /* 淡入处理：播放开始后前 REMIND_FADE_IN_FRAMES 帧渐增音量 */
            if (s_fade_pos < REMIND_FADE_IN_FRAMES) {
                int32_t fade_q15 = (int32_t)(s_fade_pos * 32767 / REMIND_FADE_IN_FRAMES);
                l = (int16_t)((int32_t)l * fade_q15 >> 15);
                r = (int16_t)((int32_t)r * fade_q15 >> 15);
                s_fade_pos++;
            }

            /* Pack to uint32_t: [R:16 | L:16] — 与 Effect Graph 格式一致 */
            out_buf[generated] = ((uint32_t)(uint16_t)r << 16) | ((uint16_t)l & 0xFFFF);
            s_pcm_read_pos++;
            generated++;
        }
    }

    return generated;
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

int RemindSound_GetCount(void)
{
    return REMIND_TABLE_COUNT;
}
