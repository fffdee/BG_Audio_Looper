/**
 *****************************************************************************
 * @file     drv_usb_cdc.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    USB CDC driver framework adaptation layer
 *****************************************************************************
 * @attention
 *
 * Adapt USB CDC device driver to driver framework, providing unified access interface
 * 
 * Registered file system path:
 *   /driver/usb/cdc/
 *   ├── name           (read-only) - Device name "USB_CDC"
 *   ├── status         (read-only) - Connection status "connected"/"disconnected"
 *   ├── baudrate       (read-write) - Baud rate
 *   ├── databits       (read-write) - Data bits (5/6/7/8)
 *   ├── stopbits       (read-write) - Stop bits (0/1/2)
 *   ├── parity         (read-write) - Parity (none/odd/even/mark/space)
 *   ├── rx_count       (read-only) - Receive buffer data amount
 *   ├── tx_count       (read-only) - Transmit buffer data amount
 *   └── flush          (write-only) - Clear buffer (write "rx"/"tx"/"all")
 * 
 * Shell command examples:
 *   cat /driver/usb/cdc/status        # Check connection status
 *   cat /driver/usb/cdc/baudrate      # Check baud rate
 *   echo 9600 > /driver/usb/cdc/baudrate  # Set baud rate
 *   cat /driver/usb/cdc/rx_count      # Check receive data amount
 *   echo all > /driver/usb/cdc/flush  # Clear all buffers
 * 
 * Device operations:
 *   read()  - Read data from USB CDC
 *   write() - Write data to USB CDC
 *   ioctl() - Control commands:
 *             0x01: Check connection status
 *             0x02: Flush receive buffer
 *             0x03: Flush transmit buffer
 *             0x04: Get available data amount
 *****************************************************************************
 */

#ifndef DRV_USB_CDC_H
#define DRV_USB_CDC_H

#include <stdint.h>

/**
 * @brief Register USB CDC driver to driver framework
 * @return 0 on success, negative value on failure
 */
int UsbCdc_DrvRegister(void);

#endif /* DRV_USB_CDC_H */
