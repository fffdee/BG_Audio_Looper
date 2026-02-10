/**
 *****************************************************************************
 * @file     drv_framework.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    Driver framework unified header file
 *****************************************************************************
 * @attention
 *
 * Include this file to use complete driver registration framework functionality:
 * - Device file system
 * - Driver registration management
 * - Shell file system commands
 *
 *****************************************************************************
 */

#ifndef __DRV_FRAMEWORK_H__
#define __DRV_FRAMEWORK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Core modules */
#include "drv_fs.h"
#include "drv_device.h"
#include "drv_init.h"

/* Shell command integration */
/* Note: ShellFs_RegisterCommands() needs to be implemented in shell_fs_commands.c */

/*******************************************************************************
 * Framework initialization
 ******************************************************************************/

/**
 * @brief  Initialize driver framework
 * @note   Should be called after Shell initialization
 *
 * Usage (pure Linux style commands):
 *   pwd             Print current path
 *   cd <path>       Change directory
 *   cd ..           Go to parent directory
 *   cd /            Go to root directory
 *   ls              List current directory (brief)
 *   ls -l           List directory in detail
 *   tree            Show directory tree
 *   cat <param>     Read parameter
 *   echo <p> <v>    Write parameter
 *   drivers         List all registered drivers
 *
 * Example:
 *   cd driver/spi/st7735    -> Change to st7735 device directory
 *   ls -l                   -> List all parameters in current directory
 *   cat width               -> Read width parameter value
 *   echo width 128          -> Set width to 128
 */
static inline int DrvFramework_Init_Old(void)
{
    /* Use new initialization method */
    return DrvFramework_FullInit();
}

#ifdef __cplusplus
}
#endif

#endif /* __DRV_FRAMEWORK_H__ */
