/**
 * shell_cmd_battery_calib.c - Battery Calibration Shell Command Implementation
 *
 * Provides USB CDC shell interface to battery discharge curve calibration:
 *   batt calib -s / --start   : Begin recording discharge profile
 *   batt calib -t / --stop    : Halt calibration (keep data)
 *   batt calib -q / --query   : Show current voltage, duration, step
 *   batt calib -c / --clear   : Erase saved curve, revert to defaults
 *   batt calib -i / --info    : Show calibration data summary
 */

#include "shell_cmd_battery_calib.h"
#include "battery_calib.h"
#include "battery_drv.h"
#include "bg_shell.h"
#include <stdio.h>

/*============================================================================
 * Private Command Handlers
 *===========================================================================*/

/**
 * @brief Start calibration from full charge
 */
static int calib_start(int argc, char *argv[])
{
    (void)argc; (void)argv;

    Shell_Print("Starting battery discharge curve calibration...\r\n");
    Shell_Print("Note: Device must be at ~4.2V (full charge) at start\r\n");
    Shell_Print("      Will record time per 0.1V band until hardware shuts down\r\n");
    Shell_Print("      Low-power mode is DISABLED while calibrating\r\n");
    Shell_Print("\r\nStarting...\r\n");

    BattCalib_Start();
    Shell_Print("Calibration started. Use 'batt calib -q' to monitor progress\r\n");
    return 0;
}

/**
 * @brief Stop calibration (keep saved data)
 */
static int calib_stop(int argc, char *argv[])
{
    (void)argc; (void)argv;

    Shell_Print("Stopping calibration...\r\n");
    
    if (!BattCalib_IsRunning()) {
        Shell_Print("Calibration is not running\r\n");
        return 0;
    }

    BattCalib_Stop();
    Shell_Print("Calibration stopped. Previously recorded steps are preserved\r\n");
    Shell_Print("Use 'batt calib -s' to resume from current voltage\r\n");
    return 0;
}

/**
 * @brief Query calibration status and progress
 */
static int calib_query(int argc, char *argv[])
{
    (void)argc; (void)argv;

    uint16_t mv = battery_get_volt_mv();
    uint8_t is_running = BattCalib_IsRunning();
    uint8_t soc = BattCalib_GetSOC();

    Shell_Printf("=== Battery Calibration Status ===\r\n");
    Shell_Printf("Running:        %s\r\n", is_running ? "YES" : "NO");
    Shell_Printf("Voltage:        %u mV\r\n", (unsigned)mv);
    Shell_Printf("SOC (calibrated): %u %%\r\n", soc);
    
    if (is_running) {
        Shell_Print("\r[Monitoring active, will record times for each 0.1V step]\r\n");
    } else {
        Shell_Print("\r[Not recording — use 'batt calib -s' to start]\r\n");
    }

    return 0;
}

/**
 * @brief Clear all calibration data and revert to defaults
 */
static int calib_clear(int argc, char *argv[])
{
    (void)argc; (void)argv;

    Shell_Print("Clearing saved calibration data...\r\n");
    Shell_Print("WARNING: This will revert to the built-in default curve\r\n");

    BattCalib_ClearData();

    Shell_Print("Calibration data cleared and reverted to defaults\r\n");
    Shell_Print("Use 'batt calib -s' to start a new calibration\r\n");
    return 0;
}

/**
 * @brief Show calibration data summary
 */
static int calib_info(int argc, char *argv[])
{
    (void)argc; (void)argv;

    Shell_Print("=== Battery Calibration Info ===\r\n");
    Shell_Printf("Flash Address:  0x%08lX\r\n", (unsigned long)BATT_CALIB_FLASH_ADDR);
    Shell_Printf("Flash Sector:   %lu\r\n", (unsigned long)BATT_CALIB_FLASH_SECTOR);
    Shell_Printf("Voltage Range:  %u.%u V -> 2.4 V (0.%u V per step)\r\n",
                (unsigned)(BATT_CALIB_V_TOP_MV / 1000u),
                (unsigned)((BATT_CALIB_V_TOP_MV % 1000u) / 100u),
                (unsigned)(BATT_CALIB_V_STEP_MV / 100u));
    Shell_Printf("Max Steps:      %u\r\n", BATT_CALIB_MAX_STEPS);
    Shell_Print("\r\nUsage:\r\n");
    Shell_Print("  batt calib -s   Start calibration (device must be at ~4.2V)\r\n");
    Shell_Print("  batt calib -t   Stop calibration (keep data)\r\n");
    Shell_Print("  batt calib -q   Query current status\r\n");
    Shell_Print("  batt calib -c   Clear saved data, revert to defaults\r\n");
    Shell_Print("  batt calib -i   Show this info\r\n");
    return 0;
}

/*============================================================================
 * Module Definition
 *===========================================================================*/

static const ShellOpt_t calib_opts[] = {
    OPT("s", "start", NULL, "Start discharge curve calibration", calib_start),
    OPT("t", "stop",  NULL, "Stop calibration (preserve recorded data)", calib_stop),
    OPT("q", "query", NULL, "Query calibration status and current SOC", calib_query),
    OPT("c", "clear", NULL, "Clear saved data, revert to defaults", calib_clear),
    OPT("i", "info",  NULL, "Show calibration system info", calib_info),
    OPT_END()
};

DEFINE_MODULE(calib, "Battery calibration", MOD_CAT_SYSTEM, calib_opts);

#define CALIB_MODULE_VAR  _mod_calib

/*============================================================================
 * Public Functions
 *===========================================================================*/

void ShellCmd_BattCalib_Init(void)
{
    Shell_RegisterModule(&CALIB_MODULE_VAR);
}

const ShellModule_t* ShellCmd_BattCalib_GetModule(void)
{
    return &CALIB_MODULE_VAR;
}
