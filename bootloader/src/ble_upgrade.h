#ifndef BLE_UPGRADE_H
#define BLE_UPGRADE_H
/**
 * @file  ble_upgrade.h
 * @brief BLE transport adapter for the firmware upgrade engine.
 *
 * Wires the BLE GATT service (AB00) onto the generic UpgradeChannel_t
 * interface defined in upgrade.h.
 *
 *   AB01 (handle 0x0006)  Write from Android App -> BLE_Upgrade_OnDataReceived()
 *   AB02 (handle 0x0008)  Notify to Android App  <- called from upgrade engine TX
 *
 * Call sequence (from UpdataTask):
 *   1. BLE_Upgrade_Init()           — once, after BtStackServiceStart()
 *   2. BLE_Upgrade_Process()        — every loop iteration
 *
 * The GATT callback (ble_app_func.c) must call:
 *   BLE_Upgrade_OnDataReceived(data, len)  when AB01 is written.
 */

#include <stdint.h>

/**
 * @brief Initialise the BLE upgrade transport layer.
 *        Must be called once after the BT stack is started.
 */
void BLE_Upgrade_Init(void);

/**
 * @brief Drive the BLE upgrade state machine.
 *        Call every iteration of the main upgrade task loop.
 */
void BLE_Upgrade_Process(void);

/**
 * @brief Feed received bytes from the GATT AB01 characteristic write
 *        into the BLE upgrade engine.
 *        Called from app_att_write() in ble_app_func.c.
 *
 * @param data  Pointer to the received data buffer.
 * @param len   Number of bytes received.
 */
void BLE_Upgrade_OnDataReceived(const uint8_t *data, uint16_t len);

/**
 * @brief Transmit bytes to the Android App via GATT AB02 Notify.
 *        Called internally by the upgrade engine via the channel TX hook.
 *
 * @param data  Pointer to data to transmit.
 * @param len   Number of bytes to transmit.
 */
void BLE_Upgrade_Send(const uint8_t *data, uint16_t len);

#endif /* BLE_UPGRADE_H */
