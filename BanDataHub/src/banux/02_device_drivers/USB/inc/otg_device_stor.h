/**
 *****************************************************************************
 * @file     otg_device_stor.h
 * @author   Owen
 * @version  V1.0.0
 * @date     24-June-2015
 * @brief    stor device interface
 *****************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT 2013 MVSilicon </center></h2>
 */

#ifndef __OTG_DEVICE_STOR_H__
#define	__OTG_DEVICE_STOR_H__

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus
	
#include "type.h"

void OTG_DeviceStorInit(void);
<<<<<<< Updated upstream
void OTG_DeviceStorProcess(void);
=======
<<<<<<< HEAD
void OTG_DeviceStorProcess(void);
=======
<<<<<<< HEAD
bool OTG_DeviceStorProcess(void);
void OTG_DeviceStorSetLocalAccess(bool busy);
bool OTG_DeviceStorIsLocalAccess(void);
=======
void OTG_DeviceStorProcess(void);
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes

#ifdef  __cplusplus
}
#endif//__cplusplus

#endif
