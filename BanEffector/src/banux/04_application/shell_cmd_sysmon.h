/**
 *****************************************************************************
 * @file     shell_cmd_sysmon.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     06-January-2026
 * @brief    System Monitor Shell Commands Header
 *****************************************************************************
 */

#ifndef __SHELL_CMD_SYSMON_H__
#define __SHELL_CMD_SYSMON_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register system monitor commands to shell
 * 
 * Registers the following commands:
 *   - sysmon -m : Show memory usage
 *   - sysmon -c : Show CPU usage statistics
 *   - sysmon -t : Show task information
 *   - sysmon -s : Show system information
 */
void ShellCmdSysmon_Register(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_SYSMON_H__ */
