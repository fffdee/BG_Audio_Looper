/**
 *****************************************************************************
 * @file     shell_io_ble.h
 * @author   BG Card Team
 * @version  V2.0.0
 * @date     20-June-2026
 * @brief    BLE Shell IO 接口声明（02层纯接口，不依赖高层模块）
 *****************************************************************************
 * @attention
 *
 * 此头文件仅声明02层BLE模块需要调用的接口函数。
 * 不包含 bg_shell.h 等高层头文件，避免02→04层跨层依赖。
 *
 * 完整的 ShellIO_t 适配器接口定义在 04_shell_commands/shell_io_ble.h 中。
 *
 *****************************************************************************
 */

#ifndef __SHELL_IO_BLE_H__
#define __SHELL_IO_BLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  BLE数据接收回调（从BLE协议栈调用）
 * @param  data: 接收到的数据
 * @param  len: 数据长度
 * @note   在BLE SPP数据到达时调用此函数
 */
void ShellIO_BLE_OnDataReceived(uint8_t *data, uint16_t len);

/**
 * @brief  启动BLE Notify测试（连接时调用）
 */
void BLE_StartNotifyTest(void);

/**
 * @brief  停止BLE Notify测试（断开时调用）
 */
void BLE_StopNotifyTest(void);

/* CCCD状态标志：App写入CCCD后设置为1，App清除CCCD后设置为0 */
extern uint8_t g_BLE_CCCD_Enabled;

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_IO_BLE_H__ */
