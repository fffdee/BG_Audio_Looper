/**
 * @file    drum_machine.c
 * @brief   独立鼓机模块 - 节拍编辑、非阻塞播放、Flash存储、BLE音源下载
 * @author  BanGO
 * @date    2026-06-01
 *
 * 实现鼓机模块的所有功能：
 * - Pattern管理（编辑、预设、加载）
 * - Tick驱动非阻塞播�?
 * - Flash存储（Pattern、配置）
 * - BLE音源下载
 */

#include "bg_config.h"

#if BANGTSYNTH_EN

#include "drum_machine.h"
#include "soundbank_manager.h"
#include "bg_log.h"
#include "bg_osal.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>

/*============================================================================

 * 调试宏控�?
 *===========================================================================*/

#define DM_DEBUG_EN  0

#if DM_DEBUG_EN
#define DM_DBG(...)  DBG(__VA_ARGS__)
#else
#define DM_DBG(...)  do {} while(0)
#endif

/*============================================================================
 * 内部状�?
 *===========================================================================*/

static uint8_t g_dm_initialized = 0;

/* 当前活跃Pattern */
static DrumPattern_t g_current_pattern = {
    .magic = DM_PATTERN_MAGIC,
    .name = "Default",
    .bpm = 120,
    .swing = 0,
};

/* 配置 */
static DrumConfig_t g_config = {
    .magic = DM_CONFIG_MAGIC,
    .active_slot = 0,
    .volume = 80,
    .program = 0,
};

/* 播放状�?*/
typedef struct {
    uint8_t  playing;
    uint8_t  loop;
    uint8_t  current_step;
    uint8_t  prev_step;
    uint32_t  step_duration_ticks;
    uint32_t   next_step_tick;
} DrumPlayState_t;

static DrumPlayState_t g_play_state = {0};

/* 轨道信息�?*/
static const DrumTrackInfo_t g_track_info[DM_TRACKS] = {
    {"Kick",    "KCK", DM_NOTE_KICK,      DM_BIT_KICK},
    {"Snare",   "SNR", DM_NOTE_SNARE,     DM_BIT_SNARE},
    {"HiHat C", "HHC", DM_NOTE_HIHAT_C,   DM_BIT_HIHAT_C},
    {"HiHat O", "HHO", DM_NOTE_HIHAT_O,   DM_BIT_HIHAT_O},
    {"Tom L",   "TML", DM_NOTE_TOM_L,     DM_BIT_TOM_L},
    {"Tom M",   "TMM", DM_NOTE_TOM_M,     DM_BIT_TOM_M},
    {"Tom H",   "TMH", DM_NOTE_TOM_H,     DM_BIT_TOM_H},
    {"Crash",   "CRH", DM_NOTE_CRASH,     DM_BIT_CRASH},
};

/* 预设Pattern数据 */
static const DrumPattern_t g_preset_patterns[DM_PRESET_COUNT] = {
    /* Rock Pattern */
    {
        .magic = DM_PATTERN_MAGIC,
        .name = "Rock",
        .bpm = 120,
        .steps = {
            0xC5, 0x04, 0xC5, 0x04,
            0xC5, 0x04, 0xC5, 0x04,
            0xC5, 0x04, 0xC5, 0x04,
            0xC5, 0x04, 0xC5, 0x04,
        },
        .velocity = {100, 100, 100, 100, 100, 100, 100, 100},
        .swing = 0,
    },
    /* Pop Pattern */
    {
        .magic = DM_PATTERN_MAGIC,
        .name = "Pop",
        .bpm = 120,
        .steps = {
            0x85, 0x00, 0x85, 0x00,
            0x85, 0x00, 0x85, 0x00,
            0x85, 0x00, 0x85, 0x00,
            0x85, 0x00, 0x85, 0x00,
        },
        .velocity = {100, 100, 100, 100, 100, 100, 100, 100},
        .swing = 0,
    },
    /* Funk Pattern */
    {
        .magic = DM_PATTERN_MAGIC,
        .name = "Funk",
        .bpm = 110,
        .steps = {
            0xC1, 0x04, 0xC1, 0x00,
            0xC1, 0x04, 0xC1, 0x00,
            0xC1, 0x04, 0xC1, 0x00,
            0xC1, 0x04, 0xC1, 0x00,
        },
        .velocity = {100, 100, 100, 100, 100, 100, 100, 100},
        .swing = 15,
    },
    /* Latin Pattern */
    {
        .magic = DM_PATTERN_MAGIC,
        .name = "Latin",
        .bpm = 100,
        .steps = {
            0xC5, 0x08, 0x45, 0x08,
            0xC5, 0x08, 0x45, 0x08,
            0xC5, 0x08, 0x45, 0x08,
            0xC5, 0x08, 0x45, 0x08,
        },
        .velocity = {100, 100, 100, 100, 100, 100, 100, 100},
        .swing = 0,
    },
};

/*============================================================================
 * 初始�?/ 反初始化
 *===========================================================================*/

int DrumMachine_Init(void)
{
    if (g_dm_initialized) {
        DM_DBG("[DrumMachine] Already initialized\n");
        return 0;
    }

    DM_DBG("[DrumMachine] Initializing...\n");

    /* 初始化默认�?*/
    memset(&g_current_pattern, 0, sizeof(g_current_pattern));
    g_current_pattern.magic = DM_PATTERN_MAGIC;
    strcpy(g_current_pattern.name, "Default");
    g_current_pattern.bpm = 120;

    memset(&g_config, 0, sizeof(g_config));
    g_config.magic = DM_CONFIG_MAGIC;
    g_config.volume = 80;
    g_config.program = 0;

    memset(&g_play_state, 0, sizeof(g_play_state));

    g_dm_initialized = 1;
    DM_DBG("[DrumMachine] Initialized OK\n");
    return 0;
}

void DrumMachine_DeInit(void)
{
    if (!g_dm_initialized) {
        return;
    }

    DrumMachine_Stop();
    g_dm_initialized = 0;
    DM_DBG("[DrumMachine] Deinitialized\n");
}

/*============================================================================
 * Tick 驱动播放
 *===========================================================================*/

void DrumMachine_Tick(void)
{
    uint32_t now;
    uint8_t step;
    uint8_t track;
    uint8_t mask;

    if (!g_dm_initialized || !g_play_state.playing) {
        return;
    }

    now = bg_get_tick_ms();

    /* 还没到下一步时�?*/
    if ((int32_t)(now - g_play_state.next_step_tick) < 0) {
        return;
    }

    step = g_play_state.current_step;

    /* 清理上一步的音符 (NoteOff) */
    if (g_play_state.prev_step < DM_STEPS_PER_PATTERN) {
        mask = g_current_pattern.steps[g_play_state.prev_step];
        for (track = 0; track < DM_TRACKS; track++) {
            if (mask & (1 << track)) {
                soundbank_manager.NoteOff(g_track_info[track].note, g_config.program);
            }
        }
    }

    /* 触发当前步的音符 (NoteOn) */
    mask = g_current_pattern.steps[step];
    for (track = 0; track < DM_TRACKS; track++) {
        if (mask & (1 << track)) {
            soundbank_manager.NoteOn(
                g_track_info[track].note,
                g_current_pattern.velocity[track],
                g_config.program
            );
        }
    }

    g_play_state.prev_step = step;

    /* 步进 */
    g_play_state.current_step++;
    if (g_play_state.current_step >= DM_STEPS_PER_PATTERN) {
        g_play_state.current_step = 0;
        if (!g_play_state.loop) {
            g_play_state.playing = 0;
            DM_DBG("[DrumMachine] Playback complete\n");
            return;
        }
    }

    /* 计算下一步触发时�?*/
    g_play_state.next_step_tick = now + g_play_state.step_duration_ticks;
}

/*============================================================================
 * 播放控制
 *===========================================================================*/

int DrumMachine_Play(uint8_t loop)
{
    uint32_t beat_ms;

    if (!g_dm_initialized) {
        DM_DBG("[DrumMachine] Not initialized\n");
        return -1;
    }

    if (g_play_state.playing) {
        DrumMachine_Stop();
    }

    /* 计算每步时长 (16分音�? 所�?beat_ms / 4) */
    beat_ms = 60000 / g_current_pattern.bpm;

    g_play_state.step_duration_ticks = beat_ms / 4;  /* bg_get_tick_ms 返回 ms */
    g_play_state.current_step = 0;
    g_play_state.prev_step = 0xFF;
    g_play_state.loop = loop;
    g_play_state.next_step_tick = bg_get_tick_ms();
    g_play_state.playing = 1;

    DM_DBG("[DrumMachine] Play: BPM=%u loop=%u step_ticks=%u\n",
        g_current_pattern.bpm, loop, g_play_state.step_duration_ticks);
    return 0;
}

void DrumMachine_Stop(void)
{
    if (!g_play_state.playing) {
        return;
    }

    g_play_state.playing = 0;
    DM_DBG("[DrumMachine] Stop\n");
}

uint8_t DrumMachine_IsPlaying(void)
{
    return g_play_state.playing;
}

uint8_t DrumMachine_GetCurrentStep(void)
{
    return g_play_state.playing ? g_play_state.current_step : 0xFF;
}

/*============================================================================
 * BPM / 音量 / 音色
 *===========================================================================*/

void DrumMachine_SetBPM(uint16_t bpm)
{
    if (bpm < 40) bpm = 40;
    if (bpm > 240) bpm = 240;
    g_current_pattern.bpm = bpm;
    DM_DBG("[DrumMachine] BPM set to %u\n", bpm);
}

uint16_t DrumMachine_GetBPM(void)
{
    return g_current_pattern.bpm;
}

void DrumMachine_SetVolume(uint8_t vol)
{
    if (vol > 100) vol = 100;
    g_config.volume = vol;
    DM_DBG("[DrumMachine] Volume set to %u%%\n", vol);
}

uint8_t DrumMachine_GetVolume(void)
{
    return g_config.volume;
}

void DrumMachine_SetProgram(uint8_t program)
{
    g_config.program = program;
    DM_DBG("[DrumMachine] Program set to %u\n", program);
}

uint8_t DrumMachine_GetProgram(void)
{
    return g_config.program;
}

/*============================================================================
 * Pattern 编辑
 *===========================================================================*/

int DrumMachine_SetStep(uint8_t step, uint8_t track, uint8_t on)
{
    if (step >= DM_STEPS_PER_PATTERN || track >= DM_TRACKS) {
        return -1;
    }

    if (on) {
        g_current_pattern.steps[step] |= (1 << track);
    } else {
        g_current_pattern.steps[step] &= ~(1 << track);
    }

    return 0;
}

int DrumMachine_GetStep(uint8_t step, uint8_t track)
{
    if (step >= DM_STEPS_PER_PATTERN || track >= DM_TRACKS) {
        return -1;
    }

    return (g_current_pattern.steps[step] >> track) & 1;
}

int DrumMachine_SetStepMask(uint8_t step, uint8_t mask)
{
    if (step >= DM_STEPS_PER_PATTERN) {
        return -1;
    }

    g_current_pattern.steps[step] = mask;
    return 0;
}

uint8_t DrumMachine_GetStepMask(uint8_t step)
{
    if (step >= DM_STEPS_PER_PATTERN) {
        return 0xFF;
    }

    return g_current_pattern.steps[step];
}

void DrumMachine_ClearPattern(void)
{
    memset(g_current_pattern.steps, 0, sizeof(g_current_pattern.steps));
    DM_DBG("[DrumMachine] Pattern cleared\n");
}

int DrumMachine_LoadPreset(uint8_t preset)
{
    if (preset >= DM_PRESET_COUNT) {
        return -1;
    }

    memcpy(&g_current_pattern, &g_preset_patterns[preset], sizeof(DrumPattern_t));
    DM_DBG("[DrumMachine] Preset %u loaded\n", preset);
    return 0;
}

const DrumTrackInfo_t* DrumMachine_GetTrackInfo(uint8_t track)
{
    if (track >= DM_TRACKS) {
        return NULL;
    }

    return &g_track_info[track];
}

const DrumPattern_t* DrumMachine_GetPattern(void)
{
    return &g_current_pattern;
}

#endif /* BANGTSYNTH_EN */
