/**
 * shell_cmd_battery_calib.h - Battery Calibration Shell Command Module
 *
 * Shell commands for battery discharge curve calibration:
 *   batt calib -s         : Start calibration
 *   batt calib -t         : Stop calibration
 *   batt calib -q         : Query calibration status
 *   batt calib -c         : Clear saved curve (revert to defaults)
 *
 * Usage:
 *   batt calib --start    # Start from full charge (USB: 4.2V typically)
 *   batt calib --status   # Check progress
 *   batt calib --stop     # Abort (data saved so far remains)
 *   batt calib --clear    # Erase saved data
 */

#ifndef __SHELL_CMD_BATTERY_CALIB_H__
#define __SHELL_CMD_BATTERY_CALIB_H__

#include "bg_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize battery calibration shell commands
 */
void ShellCmd_BattCalib_Init(void);

/**
 * @brief Get the battery calib shell module
 */
const ShellModule_t* ShellCmd_BattCalib_GetModule(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_BATTERY_CALIB_H__ */
