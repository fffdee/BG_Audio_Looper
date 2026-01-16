/**
 * shell_cmd_param.h - Parameter Shell Command Module
 * 
 * Shell commands for system parameter management:
 *   param -l        : Load parameters from flash
 *   param -s        : Save all parameters to flash
 *   param -d        : Reset to default parameters
 *   param -p [mod]  : Print parameters (all or specific module)
 *   param -i        : Show parameter info
 * 
 * Usage:
 *   param -p         # Print all parameters
 *   param -p audio   # Print audio module parameters
 *   param -s         # Save all parameters to flash
 *   param -d         # Reset to defaults (need -s to persist)
 */

#ifndef __SHELL_CMD_PARAM_H__
#define __SHELL_CMD_PARAM_H__

#include "bg_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize parameter shell commands
 *        Called during shell initialization
 */
void ShellCmd_Param_Init(void);

/**
 * @brief Get the param shell module
 * @return Pointer to ShellModule_t for registration
 */
const ShellModule_t* ShellCmd_Param_GetModule(void);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_PARAM_H__ */
