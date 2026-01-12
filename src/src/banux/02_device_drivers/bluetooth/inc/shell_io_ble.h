/**
 *****************************************************************************
 * @file     shell_io_ble.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     16-December-2025
 * @brief    Shell BLE SPP IO适配器
 *****************************************************************************
 * @attention
 *
 * 将BLE SPP接口适配为Shell IO接口
 * 
 *****************************************************************************
 */

#ifndef __SHELL_IO_BLE_H__
#define __SHELL_IO_BLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bg_shell.h"

/**
 * @brief  获取BLE IO接口
 * @return ShellIO_t指针
 */
const ShellIO_t* ShellIO_BLE_Get(void);

/**
 * @brief  初始化BLE Shell（快捷函数）
 * @note   等价于 Shell_Init() + Shell_SetIO(ShellIO_BLE_Get()) + Shell_RegisterAllModules()
 */
void ShellIO_BLE_Init(void);

/**
 * @brief  BLE数据接收回调（从BLE协议栈调用）
 * @param  data: 接收到的数据
 * @param  len: 数据长度
 * @note   在BLE SPP数据到达时调用此函数
 */
void ShellIO_BLE_OnDataReceived(uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_IO_BLE_H__ */
