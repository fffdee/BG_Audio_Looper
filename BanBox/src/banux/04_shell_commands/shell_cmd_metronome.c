/**
 *****************************************************************************
 * @file     shell_cmd_metronome.c
 * @author   BG Audio Team
 * @version  V1.0.0
 * @date     03-February-2026
 * @brief    Metronome Shell Command Implementation
 *****************************************************************************
 */

#include "shell_cmd_metronome.h"
#include "metronome.h"
#include "bg_shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "debug.h"

/*******************************************************************************
 * Internal Helper Functions
 ******************************************************************************/

/**
 * @brief Show metronome status
 */
static void show_metronome_status(void)
{
    Shell_Printf("\n========== Metronome Status ==========\n");
    Shell_Printf("Enabled:       %s\n", MetronomeModule.IsEnabled() ? "ON" : "OFF");
    Shell_Printf("BPM:           %d\n", MetronomeModule.GetBPM());
    Shell_Printf("Beats/Bar:     %d\n", MetronomeModule.GetBeatsPerMeasure());
    Shell_Printf("Volume:        %d%%\n", (int)(MetronomeModule.GetVolume() * 100));
    Shell_Printf("Downbeat Freq: %d Hz\n", MetronomeModule.GetDownbeatFreq());
    Shell_Printf("Regular Freq:  %d Hz\n", MetronomeModule.GetRegularBeatFreq());
    Shell_Printf("Beat Duration: %d ms\n", MetronomeModule.GetBeatDuration());
    Shell_Printf("=======================================\n\n");
}

/**
 * @brief Show help
 */
static void show_help(void)
{
    Shell_Printf("\n========== Metronome Commands ==========\n");
    Shell_Printf("metro              - Show status\n");
    Shell_Printf("metro on           - Enable metronome\n");
    Shell_Printf("metro off          - Disable metronome\n");
    Shell_Printf("metro toggle       - Toggle metronome\n");
    Shell_Printf("metro bpm <60-200> - Set BPM (default: 120)\n");
    Shell_Printf("metro beats <2-8>  - Set beats per measure (default: 4)\n");
    Shell_Printf("metro vol <0-100>  - Set volume as percentage (default: 80)\n");
    Shell_Printf("metro freq <down> <reg> - Set frequencies in Hz\n");
    Shell_Printf("metro dur <ms>     - Set beat duration (default: 100ms)\n");
    Shell_Printf("metro help         - Show this help\n");
    Shell_Printf("========================================\n\n");
}

/*******************************************************************************
 * Metro Command Handlers
 ******************************************************************************/

/**
 * @brief Main metro command router - This is the default handler
 */
static int cmd_metro_main(int argc, char *argv[])
{
    /* If no subcommand provided, show status */
    if (argc < 1) {
        show_metronome_status();
        return 0;
    }

    const char *subcmd = argv[0];

    /* Route to appropriate subcommand */
    if (strcmp(subcmd, "on") == 0) {
        MetronomeModule.Enable();
        Shell_Printf("Metronome enabled\n");
        return 0;
    }
    else if (strcmp(subcmd, "off") == 0) {
        MetronomeModule.Disable();
        Shell_Printf("Metronome disabled\n");
        return 0;
    }
    else if (strcmp(subcmd, "toggle") == 0) {
        MetronomeModule.Toggle();
        Shell_Printf("Metronome toggled - now %s\n", MetronomeModule.IsEnabled() ? "ON" : "OFF");
        return 0;
    }
    else if (strcmp(subcmd, "bpm") == 0) {
        if (argc < 2) {
            Shell_Printf("Usage: metro bpm <60-200>\n");
            return -1;
        }
        int bpm = atoi(argv[1]);
        if (bpm < 60 || bpm > 200) {
            Shell_Printf("ERROR: BPM must be 60-200\n");
            return -1;
        }
        MetronomeModule.SetBPM(bpm);
        Shell_Printf("BPM set to %d\n", bpm);
        return 0;
    }
    else if (strcmp(subcmd, "beats") == 0) {
        if (argc < 2) {
            Shell_Printf("Usage: metro beats <2-8>\n");
            return -1;
        }
        int beats = atoi(argv[1]);
        if (beats < 2 || beats > 8) {
            Shell_Printf("ERROR: Beats must be 2-8\n");
            return -1;
        }
        MetronomeModule.SetBeatsPerMeasure(beats);
        Shell_Printf("Beats per measure set to %d\n", beats);
        return 0;
    }
    else if (strcmp(subcmd, "vol") == 0) {
        if (argc < 2) {
            Shell_Printf("Usage: metro vol <0-100>\n");
            return -1;
        }
        int vol = atoi(argv[1]);
        if (vol < 0 || vol > 100) {
            Shell_Printf("ERROR: Volume must be 0-100\n");
            return -1;
        }
        MetronomeModule.SetVolume((float)vol / 100.0f);
        Shell_Printf("Volume set to %d%%\n", vol);
        return 0;
    }
    else if (strcmp(subcmd, "freq") == 0) {
        if (argc < 3) {
            Shell_Printf("Usage: metro freq <downbeat_hz> <regular_hz>\n");
            return -1;
        }
        int freq_down = atoi(argv[1]);
        int freq_reg = atoi(argv[2]);
        if (freq_down < 100 || freq_down > 2000 || freq_reg < 100 || freq_reg > 2000) {
            Shell_Printf("ERROR: Frequencies must be 100-2000 Hz\n");
            return -1;
        }
        MetronomeModule.SetDownbeatFreq(freq_down);
        MetronomeModule.SetRegularBeatFreq(freq_reg);
        Shell_Printf("Frequencies set to downbeat=%d Hz, regular=%d Hz\n", freq_down, freq_reg);
        return 0;
    }
    else if (strcmp(subcmd, "dur") == 0) {
        if (argc < 2) {
            Shell_Printf("Usage: metro dur <ms>\n");
            return -1;
        }
        int dur = atoi(argv[1]);
        if (dur < 10 || dur > 500) {
            Shell_Printf("ERROR: Duration must be 10-500 ms\n");
            return -1;
        }
        MetronomeModule.SetBeatDuration(dur);
        Shell_Printf("Beat duration set to %d ms\n", dur);
        return 0;
    }
    else if (strcmp(subcmd, "help") == 0) {
        show_help();
        return 0;
    }
    else if (strcmp(subcmd, "status") == 0) {
        show_metronome_status();
        return 0;
    }
    else {
        Shell_Printf("Unknown metronome command: %s\n", subcmd);
        Shell_Printf("Use 'metro help' for available commands\n");
        return -1;
    }
}

/*******************************************************************************
 * Shell Module Registration (using BanBox's ShellModule_t system)
 ******************************************************************************/

/**
 * @brief Metronome command options
 */
static const ShellOpt_t g_MetroOpts[] = {
    { "", NULL, "[subcmd] [args]", "Metronome control", cmd_metro_main },
    OPT_END()
};

/**
 * @brief Metronome module definition
 */
static const ShellModule_t g_MetroModule = {
    "metro",
    "Metronome Control (BPM, beats, volume, frequencies)",
    MOD_CAT_AUDIO,
    g_MetroOpts,
    1  /* optCount - 1 for the main handler, OPT_END doesn't count */
};

/**
 * @brief Register metronome command module
 */
void ShellCmdMetronome_Register(void)
{
    Shell_RegisterModule(&g_MetroModule);
}
