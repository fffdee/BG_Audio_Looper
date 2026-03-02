/**
 * @file    shell_cmd_drum.c
 * @brief   鼓机 Shell 命令模块
 * @author  BanGO
 * @date    2026-06-01
 *
 * 提供独立于合成器的鼓机命令:
 *   drum -p [loop]|stop|status  播放/停止/查询
 *   drum -b <bpm>               设置BPM
 *   drum -v [0-100]             音量
 *   drum -e <step> <track> <0|1> 编辑单步
 *   drum -s [slot]              保存Pattern到Flash
 *   drum -l [slot]              从Flash加载Pattern
 *   drum -P <preset>            加载预设 (0=Rock,1=Pop,2=Funk,3=Latin)
 *   drum -c                     清空Pattern
 *   drum -d                     显示/打印当前Pattern
 *   drum -D <size>              下载鼓机音源
 *   drum -q                     查询状态 (JSON格式, 供App使用)
 *
 * 编译条件: BANGTSYNTH_EN
 */

#include "bg_config.h"

#ifdef BANGTSYNTH_EN

#include "shell_cmd_drum.h"
#include "bg_shell.h"
#include "drum_machine.h"
#include <string.h>
#include <stdlib.h>

/* ============================================
 * 命令: drum -p  播放控制
 * ============================================ */
static int cmd_drum_play(int argc, char *argv[])
{
    if (argc >= 1) {
        if (strcmp(argv[0], "stop") == 0) {
            DrumMachine_Stop();
            Shell_Printf("Drum machine stopped\n");
            return 0;
        }
        if (strcmp(argv[0], "status") == 0) {
            Shell_Printf("Playing: %s, Step: %u/%u, BPM: %u\n",
                DrumMachine_IsPlaying() ? "Yes" : "No",
                DrumMachine_GetCurrentStep(),
                DM_STEPS_PER_PATTERN,
                DrumMachine_GetBPM());
            return 0;
        }
    }

    /* 默认循环播放, 参数1表示循环 */
    {
        uint8_t loop = 1;
        if (argc >= 1) {
            loop = (uint8_t)strtoul(argv[0], NULL, 0);
        }
        if (DrumMachine_Play(loop) < 0) {
            Shell_Printf("Error: DrumMachine not initialized\n");
            return -1;
        }
        Shell_Printf("Drum playing (loop=%u, BPM=%u)\n", loop, DrumMachine_GetBPM());
    }
    return 0;
}

/* ============================================
 * 命令: drum -b <bpm>  设置BPM
 * ============================================ */
static int cmd_drum_bpm(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Printf("BPM: %u\n", DrumMachine_GetBPM());
        return 0;
    }

    {
        uint16_t bpm = (uint16_t)strtoul(argv[0], NULL, 0);
        DrumMachine_SetBPM(bpm);
        Shell_Printf("BPM set to %u\n", DrumMachine_GetBPM());
    }
    return 0;
}

/* ============================================
 * 命令: drum -v [0-100]  音量
 * ============================================ */
static int cmd_drum_volume(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Printf("Volume: %u%%\n", DrumMachine_GetVolume());
        return 0;
    }

    {
        uint8_t vol = (uint8_t)strtoul(argv[0], NULL, 0);
        DrumMachine_SetVolume(vol);
        Shell_Printf("Volume set to %u%%\n", DrumMachine_GetVolume());
    }
    return 0;
}

/* ============================================
 * 命令: drum -e <step> <track> <0|1>  编辑单步
 * ============================================ */
static int cmd_drum_edit(int argc, char *argv[])
{
    uint8_t step, track, on;

    if (argc < 3) {
        Shell_Printf("Usage: drum -e <step 0-15> <track 0-7> <0|1>\n");
        Shell_Printf("  Tracks: 0=Kick 1=Snare 2=HHC 3=HHO 4=TomL 5=TomM 6=TomH 7=Crash\n");
        return -1;
    }

    step  = (uint8_t)strtoul(argv[0], NULL, 0);
    track = (uint8_t)strtoul(argv[1], NULL, 0);
    on    = (uint8_t)strtoul(argv[2], NULL, 0);

    if (DrumMachine_SetStep(step, track, on) < 0) {
        Shell_Printf("Error: invalid step(%u) or track(%u)\n", step, track);
        return -1;
    }

    {
        const DrumTrackInfo_t *info = DrumMachine_GetTrackInfo(track);
        Shell_Printf("Step %u, %s = %s\n", step,
            info ? info->name : "?",
            on ? "ON" : "OFF");
    }
    return 0;
}

/* ============================================
 * 命令: drum -s [slot]  保存Pattern到Flash
 * ============================================ */
static int cmd_drum_save(int argc, char *argv[])
{
    uint8_t slot = 0;

    if (argc >= 1) {
        slot = (uint8_t)strtoul(argv[0], NULL, 0);
    }

    if (DrumMachine_SavePattern(slot) < 0) {
        Shell_Printf("Error: save to slot %u failed\n", slot);
        return -1;
    }

    Shell_Printf("Pattern saved to slot %u\n", slot);
    return 0;
}

/* ============================================
 * 命令: drum -l [slot]  从Flash加载Pattern
 * ============================================ */
static int cmd_drum_load(int argc, char *argv[])
{
    uint8_t slot = 0;

    if (argc >= 1) {
        slot = (uint8_t)strtoul(argv[0], NULL, 0);
    }

    if (DrumMachine_LoadPattern(slot) < 0) {
        Shell_Printf("Error: load from slot %u failed (empty or corrupt)\n", slot);
        return -1;
    }

    Shell_Printf("Pattern loaded from slot %u\n", slot);
    return 0;
}

/* ============================================
 * 命令: drum -P <preset>  加载预设
 * ============================================ */
static int cmd_drum_preset(int argc, char *argv[])
{
    uint8_t preset;
    static const char *names[] = {"Rock", "Pop", "Funk", "Latin"};

    if (argc < 1) {
        Shell_Printf("Usage: drum -P <preset>\n");
        Shell_Printf("  0=Rock  1=Pop  2=Funk  3=Latin\n");
        return -1;
    }

    preset = (uint8_t)strtoul(argv[0], NULL, 0);
    if (DrumMachine_LoadPreset(preset) < 0) {
        Shell_Printf("Error: invalid preset %u (max %u)\n", preset, DM_PRESET_COUNT - 1);
        return -1;
    }

    Shell_Printf("Preset '%s' loaded\n", preset < DM_PRESET_COUNT ? names[preset] : "?");
    return 0;
}

/* ============================================
 * 命令: drum -c  清空Pattern
 * ============================================ */
static int cmd_drum_clear(int argc, char *argv[])
{
    (void)argc; (void)argv;
    DrumMachine_ClearPattern();
    Shell_Printf("Pattern cleared\n");
    return 0;
}

/* ============================================
 * 命令: drum -d  显示当前Pattern
 * ============================================ */
static int cmd_drum_display(int argc, char *argv[])
{
    const DrumPattern_t *pat;
    uint8_t track, step;

    (void)argc; (void)argv;
    pat = DrumMachine_GetPattern();

    Shell_Printf("=== Drum Pattern: %s ===\n", pat->name);
    Shell_Printf("BPM: %u  Swing: %u%%\n", pat->bpm, pat->swing);
    Shell_Printf("       ");
    for (step = 0; step < DM_STEPS_PER_PATTERN; step++) {
        Shell_Printf("%X", step);
    }
    Shell_Printf("\n");

    for (track = 0; track < DM_TRACKS; track++) {
        const DrumTrackInfo_t *info = DrumMachine_GetTrackInfo(track);
        if (!info) continue;
        Shell_Printf("  %s  ", info->short_name);
        for (step = 0; step < DM_STEPS_PER_PATTERN; step++) {
            Shell_Printf("%c", (pat->steps[step] & (1 << track)) ? 'X' : '.');
        }
        Shell_Printf("  v%u\n", pat->velocity[track]);
    }

    Shell_Printf("Playing: %s", DrumMachine_IsPlaying() ? "Yes" : "No");
    if (DrumMachine_IsPlaying()) {
        Shell_Printf("  Step: %u", DrumMachine_GetCurrentStep());
    }
    Shell_Printf("\n");

    return 0;
}

/* ============================================
 * 命令: drum -D <size>  下载鼓机音源
 * ============================================ */
static int cmd_drum_download(int argc, char *argv[])
{
    uint32_t file_size;

    if (argc < 1) {
        Shell_Printf("Usage: drum -D <size_bytes>\n");
        return -1;
    }

    file_size = (uint32_t)strtoul(argv[0], NULL, 0);
    if (file_size == 0) {
        Shell_Printf("Error: invalid size\n");
        return -1;
    }

    Shell_Printf("=== Drum Soundbank Download ===\n");
    Shell_Printf("  File size: %u bytes\n", file_size);

    if (DrumMachine_DownloadSoundbank(file_size) < 0) {
        Shell_Printf("Error: download failed\n");
        return -1;
    }

    Shell_Printf("Download complete\n");
    return 0;
}

/* ============================================
 * 命令: drum -q  查询状态 (JSON, 供App解析)
 * ============================================ */
static int cmd_drum_query(int argc, char *argv[])
{
    const DrumPattern_t *pat;
    uint8_t step;

    (void)argc; (void)argv;
    pat = DrumMachine_GetPattern();

    Shell_Printf("{\"drum\":{");
    Shell_Printf("\"playing\":%u,", DrumMachine_IsPlaying());
    Shell_Printf("\"step\":%u,", DrumMachine_GetCurrentStep());
    Shell_Printf("\"bpm\":%u,", pat->bpm);
    Shell_Printf("\"vol\":%u,", DrumMachine_GetVolume());
    Shell_Printf("\"prog\":%u,", DrumMachine_GetProgram());
    Shell_Printf("\"name\":\"%s\",", pat->name);
    Shell_Printf("\"swing\":%u,", pat->swing);
    Shell_Printf("\"pattern\":[");
    for (step = 0; step < DM_STEPS_PER_PATTERN; step++) {
        Shell_Printf("%u", pat->steps[step]);
        if (step < DM_STEPS_PER_PATTERN - 1) Shell_Printf(",");
    }
    Shell_Printf("],\"velocity\":[");
    {
        uint8_t t;
        for (t = 0; t < DM_TRACKS; t++) {
            Shell_Printf("%u", pat->velocity[t]);
            if (t < DM_TRACKS - 1) Shell_Printf(",");
        }
    }
    Shell_Printf("]}}\n");

    return 0;
}

/* ============================================
 * 选项定义
 * ============================================ */
static const ShellOpt_t drum_options[] = {
    OPT("p", "play",     "[loop]|stop|status", "Play/stop/status",             cmd_drum_play),
    OPT("b", "bpm",      "[40-240]",           "Set/query BPM",                cmd_drum_bpm),
    OPT("v", "volume",   "[0-100]",            "Set/query volume",             cmd_drum_volume),
    OPT("e", "edit",     "<step> <trk> <0|1>", "Edit step (step=0-15 trk=0-7)", cmd_drum_edit),
    OPT("s", "save",     "[slot]",             "Save pattern to Flash",        cmd_drum_save),
    OPT("l", "load",     "[slot]",             "Load pattern from Flash",      cmd_drum_load),
    OPT("P", "preset",   "<0-3>",              "Load preset (Rock/Pop/Funk/Latin)", cmd_drum_preset),
    OPT("c", "clear",    NULL,                 "Clear all steps",              cmd_drum_clear),
    OPT("d", "display",  NULL,                 "Display pattern grid",         cmd_drum_display),
    OPT("D", "download", "<size>",             "Download drum soundbank",      cmd_drum_download),
    OPT("q", "query",    NULL,                 "Query status (JSON for App)",  cmd_drum_query),
    OPT_END()
};

/* ============================================
 * 模块注册
 * ============================================ */
int ShellCmdDrum_Register(void)
{
    static const ShellModule_t drum_module = {
        "drum",
        "Drum machine (play/edit/preset/save/download)",
        MOD_CAT_AUDIO,
        drum_options,
        11
    };
    return Shell_RegisterModule(&drum_module) ? 0 : -1;
}

#else  /* !BANGTSYNTH_EN */

#include "shell_cmd_drum.h"

int ShellCmdDrum_Register(void)
{
    return 0;
}

#endif /* BANGTSYNTH_EN */
