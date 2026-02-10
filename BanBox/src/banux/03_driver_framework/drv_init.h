/**
 *****************************************************************************
 * @file     drv_init.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    Driver framework initialization header file
 *****************************************************************************
 */

#ifndef __DRV_INIT_H__
#define __DRV_INIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

/*******************************************************************************
 * Function declarations
 ******************************************************************************/

/**
 * @brief  Initialize driver framework core
 * @retval 0-success, <0-failure
 */
int DrvFramework_Init(void);

/**
 * @brief  Register all hardware drivers
 * @retval 0-success, <0-failure
 */
int DrvFramework_RegisterAll(void);

/**
 * @brief  Driver framework full initialization (framework + drivers)
 * @retval 0-success, <0-failure
 * 
 * @note   Call this function in main() to complete all driver registration
 * 
 * @example
 *   int main(void) {
 *       // Hardware initialization...
 *       
 *       // Driver framework initialization
 *       DrvFramework_FullInit();
 *       
 *       // Shell initialization
 *       Shell_Init();
 *       
 *       while(1) {
 *           // Main loop
 *       }
 *   }
 */
int DrvFramework_FullInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_INIT_H__ */
