/**
 * @file  ble_upgrade.c
 * @brief BLE transport adapter for the firmware upgrade engine.
 *
 * Provides a 512-byte ring-buffer backed UpgradeChannel_t that bridges
 *   - GATT AB01 writes  (bytes in  → rx ring buffer)
 *   - GATT AB02 notifies (bytes out ← engine TX hook)
 * onto the generic Upgrade_ProcessChannel() state machine in upgrade.c.
 *
 * BLE MTU is typically 20 bytes for standard BLE 4.x; the engine chunks
 * large frames automatically because every packet is ≤268 bytes, and the
 * TX side sends 20-byte ATT notify segments.
 */

#include <string.h>
#include "ble_upgrade.h"
#include "upgrade.h"
#include "ble_api.h"   /* GattServerNotify() */
#include "debug.h"

/* -------------------------------------------------------------------------
 * Configuration
 * ------------------------------------------------------------------------- */
#define BLE_RX_BUF_SZ   512u   /* must be power-of-two */
#define BLE_NOTIFY_MTU   20u   /* max bytes per ATT Notify payload */

/* AB02 characteristic handle (same as ble_app_func.c define) */
#define BLE_NOTIFY_HANDLE  0x0008u

/* -------------------------------------------------------------------------
 * Ring buffer (interrupt-safe: single producer / single consumer)
 * ------------------------------------------------------------------------- */
static uint8_t  s_rx_buf[BLE_RX_BUF_SZ];
static volatile uint16_t s_rx_head = 0;   /* write index (ISR/callback side) */
static volatile uint16_t s_rx_tail = 0;   /* read  index (task side)         */

static void rb_put(uint8_t b)
{
    uint16_t next = (s_rx_head + 1u) & (BLE_RX_BUF_SZ - 1u);
    if (next != s_rx_tail) {   /* drop byte if full */
        s_rx_buf[s_rx_head] = b;
        s_rx_head = next;
    }
}

static int rb_available(void)
{
    return (s_rx_head != s_rx_tail) ? 1 : 0;
}

static uint16_t rb_read(uint8_t *buf, uint16_t max)
{
    uint16_t n = 0;
    while (n < max && s_rx_tail != s_rx_head) {
        buf[n++] = s_rx_buf[s_rx_tail];
        s_rx_tail = (s_rx_tail + 1u) & (BLE_RX_BUF_SZ - 1u);
    }
    return n;
}

/* -------------------------------------------------------------------------
 * UpgradeChannel_t hook implementations
 * ------------------------------------------------------------------------- */
static uint16_t ble_ch_rx_read(uint8_t *buf, uint16_t max)
{
    return rb_read(buf, max);
}

static void ble_ch_tx_write(const uint8_t *data, uint16_t len)
{
    /* BLE Notify has a limited MTU; chunk into BLE_NOTIFY_MTU bytes */
    uint16_t offset = 0;
    while (offset < len) {
        uint16_t chunk = len - offset;
        if (chunk > BLE_NOTIFY_MTU) chunk = BLE_NOTIFY_MTU;
        GattServerNotify(BLE_NOTIFY_HANDLE,
                         (uint8_t *)(data + offset), chunk);
        offset = (uint16_t)(offset + chunk);
    }
}

static int ble_ch_rx_available(void)
{
    return rb_available();
}

static const UpgradeChannel_t g_ble_channel = {
    ble_ch_rx_read,
    ble_ch_tx_write,
    ble_ch_rx_available,
    UPG_CH_BLE
};

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */
void BLE_Upgrade_Init(void)
{
    s_rx_head = 0;
    s_rx_tail = 0;
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
    DBG("[BLE_UPG] Init OK\n");
}

void BLE_Upgrade_Process(void)
{
    Upgrade_ProcessChannel(&g_ble_channel);
}

void BLE_Upgrade_OnDataReceived(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    if (!data || len == 0) return;
    for (i = 0; i < len; i++)
        rb_put(data[i]);
}

void BLE_Upgrade_Send(const uint8_t *data, uint16_t len)
{
    ble_ch_tx_write(data, len);
}
