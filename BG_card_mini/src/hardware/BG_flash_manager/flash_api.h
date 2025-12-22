/**
 * flash_api.h - Unified API for Flash subsystem
 *
 * Usage:
 *   #include "flash_api.h"
 *
 * Initialization:
 *   FlashDevices_Init();
 *
 * Partition operations:
 *   FlashPartition_LooperRead(offset, buf, len);
 *   FlashPartition_LooperWrite(offset, buf, len);
 *   FlashPartition_LooperEraseSector(offset);
 *
 * Low-level device operations:
 *   FlashDevice_t *dev = FlashBus_GetDeviceById(0);
 *   FlashDev_Read(dev, addr, buf, len);
 */

#ifndef __FLASH_API_H__
#define __FLASH_API_H__

#include "flash_bus.h"
#include "flash_nor_w25qxx.h"
#include "flash_devices.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Quick Reference
 *===========================================================================*/

/*
 * Architecture layers:
 *
 *   +-------------------+
 *   | Application Layer  |  FlashPartition_xxx() - partition read/write
 *   +-------------------+
 *   | Device Layer       |  flash_devices.c - device registration
 *   +-------------------+
 *   | Bus Layer          |  flash_bus.c - device management
 *   +-------------------+
 *   | Driver Layer       |  flash_nor_w25qxx.c - hardware driver
 *   +-------------------+
 *   | Hardware           |  SPI Flash (W25Q64 x 2)
 *   +-------------------+
 *
 * Partition layout:
 *
 *   Flash #0 (CS=GPIOA21):
 *   +----------------------------+
 *   | System Partition (1MB)     | 0x000000 - 0x0FFFFF
 *   +----------------------------+
 *   | Looper Partition (7MB)     | 0x100000 - 0x7FFFFF
 *   +----------------------------+
 *
 *   Flash #1 (CS=GPIOA23):
 *   +----------------------------+
 *   | Storage Partition (8MB)    | 0x000000 - 0x7FFFFF
 *   +----------------------------+
 */

/*===========================================================================
 * Common API Overview
 *===========================================================================*/

/*
 * Initialization (call once in main.c)
 * --------------------------------
 * FlashDevices_Init()      - Initialize all Flash devices
 * FlashDevices_DeInit()    - Deinitialize
 *
 * Partition operations (recommended)
 * --------------------------------
 * FlashPartition_SystemRead/Write/EraseSector()    - System partition
 * FlashPartition_LooperRead/Write/EraseSector/EraseBlock() - Looper partition
 * FlashPartition_StorageRead/Write/EraseSector/EraseBlock() - Storage partition
 *
 * Device operations (low-level)
 * --------------------------------
 * FlashBus_GetDeviceById(id)   - Get device
 * FlashDev_Read/Write()        - Read/write data
 * FlashDev_EraseSector/Block() - Erase
 *
 * Debug
 * --------------------------------
 * FlashBus_PrintInfo()     - Print all device info
 * FlashBus_TestDevice(id)  - Test specified device
 *
 * Shell commands
 * --------------------------------
 * flash list              - List all devices
 * flash info <id>         - Show device details
 * flash init <id>         - Initialize device
 * flash test <id>         - Test read/write
 * flash read <id> <addr>  - Read data
 * flash erase <id> <addr> - Erase sector
 * flash eraseall <id>     - Erase entire chip
 */

/*===========================================================================
 * Usage Examples
 *===========================================================================*/

/*
 * Example 1: Simple partition read/write
 *
 *   uint8_t buf[256];
 *
 *   // Erase first sector of Looper partition
 *   FlashPartition_LooperEraseSector(0);
 *
 *   // Write data
 *   memset(buf, 0xAA, 256);
 *   FlashPartition_LooperWrite(0, buf, 256);
 *
 *   // Read and verify
 *   FlashPartition_LooperRead(0, buf, 256);
 *
 * Example 2: Low-level device operation
 *
 *   FlashDevice_t *dev = FlashBus_GetDeviceById(0);
 *   if (dev && dev->initialized) {
 *       FlashDev_Read(dev, 0x100000, buf, 256);
 *   }
 *
 * Example 3: Iterate all devices
 *
 *   void print_dev(FlashDevice_t *dev, void *arg) {
 *       DBG("Device: %s\n", dev->name);
 *   }
 *   FlashBus_ForEach(print_dev, NULL);
 */

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_API_H__ */
