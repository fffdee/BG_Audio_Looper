/**
 * @file    ble_app_sync.h
 * @brief   BLE 应用层参数同步（从 02_device_drivers 层分离）
 * @author  BanGO
 *
 * 架构定位: 05_component/ble_app/
 *   本模块负责 BLE 连接后的应用层参数同步逻辑，
 *   依赖 05_component 层的 sys_param / effect_graph / audio_looper 等。
 *   纯协议编解码留在 02_device_drivers/bluetooth/ble_protocol.c 中。
 */

#ifndef __BLE_APP_SYNC_H__
#define __BLE_APP_SYNC_H__

#include <stdint.h>

/**
 * @brief 初始化 BLE 应用层模块
 * @note  注册 BLE 事件订阅、设置协议数据处理器和回调。
 *        必须在 BleProto_Init() 之前调用，确保回调已就绪。
 */
void BleApp_Init(void);

/**
 * @brief BLE 断开时的应用层处理
 * @note  停止 Looper/节拍器、清除同步状态。
 *        由 BLE 事件订阅自动触发，也可手动调用。
 */
void BleApp_OnDisconnected(void);

/**
 * @brief 触发全量参数同步到 App
 * @note  在独立 FreeRTOS 任务中执行，不阻塞主循环。
 */
void BleApp_StartSync(void);

/**
 * @brief 处理来自 App 的 BLE 数据命令
 * @param cmd     命令字节
 * @param payload 负载数据
 * @param len     负载长度
 */
void BleApp_HandleDataCmd(uint8_t cmd, const uint8_t *payload, uint8_t len);

#endif /* __BLE_APP_SYNC_H__ */
