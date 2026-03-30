/**
 * @file  app_upgrade.h
 * @brief User-firmware OTA engine.
 *
 * Architecture
 * 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
 * The bootloader is minimal (no BT stack).  All BLE OTA logic lives here.
 *
 * Upgrade flow:
 *   1. BLE peer sends firmware packets in the standard protocol format
 *      [SOF:1][CMD:1][SEQ:2][LEN:2][DATA:N][CRC16:2].
 *   2. App calls App_OTA_OnData() for each raw BLE notification received.
 *   3. The OTA engine parses packets and:
 *        SYNC  鈫?ACK with protocol version
 *        ERASE 鈫?erases backup partition in flash
 *        START 鈫?validates firmware size
 *        DATA  鈫?writes chunks to backup partition
 *        FINISH鈫?verifies magic, writes partition flags (active鈫攂ackup swap),
 *                then reboots 鈥?bootloader will boot the new partition.
 *   4. App must call App_OTA_Process() periodically (every main-loop tick)
 *      to handle deferred flash operations.
 *
 * Partition semantics (equal-peer A/B):
 *   - active_part=0 鈫?A is running, B is backup
 *   - active_part=1 鈫?B is running, A is backup
 *   - After successful OTA: active_part flips, backup becomes new active.
 *   - Bootloader increments boot_fail_cnt before each jump; user app resets
 *     it by calling App_ConfirmBootSuccess() on clean startup.
 */
#ifndef __APP_UPGRADE_H__
#define __APP_UPGRADE_H__

#include <stdint.h>

/* 鈹€鈹€ OTA packet protocol (same framing as bootloader USB CDC) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
#define OTA_SOF          0xAAU
#define OTA_VERSION      0x03U

#define OTA_CMD_SYNC     0x01U
#define OTA_CMD_START    0x02U
#define OTA_CMD_DATA     0x03U
#define OTA_CMD_FINISH   0x04U
#define OTA_CMD_ERASE    0x06U
#define OTA_CMD_QUERY    0x07U

#define OTA_RSP_ACK      0xA1U
#define OTA_RSP_NACK     0xA2U

#define OTA_ERR_CRC      0x01U
#define OTA_ERR_FLASH    0x02U
#define OTA_ERR_SIZE     0x03U
#define OTA_ERR_STATE    0x04U
#define OTA_ERR_PARAM    0x05U

/* 鈹€鈹€ Public API 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */

/**
 * @brief  Initialise the OTA engine.  Call once at startup.
 * @param  send_fn  Callback to transmit ACK/NACK bytes back to the BLE peer.
 *                  Signature: void send_fn(const uint8_t *data, uint16_t len)
 */
void App_OTA_Init(void (*send_fn)(const uint8_t *data, uint16_t len));

/**
 * @brief  Feed raw BLE bytes into the OTA packet parser.
 *         Call from the BLE GATT write/notify handler whenever data arrives.
 */
void App_OTA_OnData(const uint8_t *data, uint16_t len);

/**
 * @brief  Drive deferred OTA work (flash erase / reboot after FINISH).
 *         Call once per main task iteration.
 */
void App_OTA_Process(void);

/**
 * @brief  Confirm that this boot was successful.
 *         Resets boot_fail_cnt to 0 in the partition flags.
 *         Call once early in startup, after basic HW init is complete.
 */
void App_ConfirmBootSuccess(void);

#endif /* __APP_UPGRADE_H__ */
