/**
 * @file shell_cmd_wav_ble.c
 * @brief Shell command - WAV BLE export test
 *
 * Commands:
 *   wav_ble export <mask> [ch]  - Start BLE WAV export
 *   wav_ble status               - Show export status
 *   wav_ble cancel               - Cancel ongoing export
 */

#include "bg_shell.h"
#include "looper_wav_ble_export.h"
#include "audio_looper.h"
#include <string.h>
#include <stdlib.h>

/* ============================================
 * Command handler
 * ============================================ */

static int cmd_wav_ble_handler(int argc, char *argv[])
{
    if (argc < 2) {
        Shell_Print("WAV BLE export test\r\n");
        Shell_Print("Usage:\r\n");
        Shell_Print("  wav_ble export <mask> [ch]  Start export (mask=0x01-0x0F, ch=1|2)\r\n");
        Shell_Print("  wav_ble status              Show export state\r\n");
        Shell_Print("  wav_ble cancel              Cancel export\r\n");
        return 0;
    }

    /* export sub-command */
    if (strcmp(argv[1], "export") == 0) {
        uint8_t payload[3];
        uint8_t mask;
        uint8_t channels = 2;

        if (argc < 3) {
            Shell_Printf("Usage: wav_ble export <mask> [channels]\r\n");
            Shell_Printf("  mask: 1=seg0, 2=seg1, 4=seg2, 8=seg3, 15=all\r\n");
            Shell_Printf("  channels: 1=mono, 2=stereo (default: 2)\r\n");
            return -1;
        }

        mask = (uint8_t)strtol(argv[2], NULL, 0);
        if (mask == 0 || mask > 0x0F) {
            Shell_Printf("Error: mask must be 1-15 (0x01-0x0F)\r\n");
            return -1;
        }

        if (argc >= 4) {
            channels = (uint8_t)atoi(argv[3]);
        }
        if (channels != 1 && channels != 2) {
            Shell_Printf("Error: channels must be 1 or 2\r\n");
            return -1;
        }

        if (LooperWavBle_IsBusy()) {
            Shell_Printf("Error: export already in progress\r\n");
            return -1;
        }

        Shell_Printf("Starting BLE WAV export: mask=0x%02X, ch=%d\r\n", mask, channels);

        /* Build export request payload and invoke handler directly */
        payload[0] = WAV_BLE_SUBCMD_EXPORT_REQ;
        payload[1] = mask;
        payload[2] = channels;
        LooperWavBle_HandleCommand(payload, 3);

        Shell_Printf("Export started (use 'wav_ble status' to monitor)\r\n");
        return 0;
    }

    /* status sub-command */
    if (strcmp(argv[1], "status") == 0) {
        if (LooperWavBle_IsBusy()) {
            Shell_Printf("Export: IN PROGRESS\r\n");
        } else {
            Shell_Printf("Export: IDLE\r\n");
        }
        return 0;
    }

    /* cancel sub-command */
    if (strcmp(argv[1], "cancel") == 0) {
        uint8_t payload[1];

        if (!LooperWavBle_IsBusy()) {
            Shell_Printf("No export in progress\r\n");
            return 0;
        }

        payload[0] = WAV_BLE_SUBCMD_CANCEL;
        LooperWavBle_HandleCommand(payload, 1);
        Shell_Printf("Export cancelled\r\n");
        return 0;
    }

    Shell_Printf("Unknown subcommand: %s\r\n", argv[1]);
    return -1;
}

/* ============================================
 * Shell Module Definition
 * ============================================ */

static const ShellOpt_t wav_ble_options[] = {
    OPT("", "<subcommand>", "BLE WAV export",
        "BLE WAV export test commands\n"
        "    wav_ble export <mask> [ch] - Start BLE export\n"
        "    wav_ble status             - Show state\n"
        "    wav_ble cancel             - Cancel export",
        cmd_wav_ble_handler),
    OPT_END()
};

DEFINE_MODULE(wav_ble, "Audio Looper WAV BLE export", MOD_CAT_DEBUG, wav_ble_options);

/* ============================================
 * Public Interface
 * ============================================ */

void ShellCmdWavBle_Register(void)
{
    Shell_RegisterModule(&_mod_wav_ble);
}
