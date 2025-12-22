/**
 * flash_test.c - New Flash driver framework test code
 * Tests flash_bus + flash_devices + flash_nor_w25qxx framework
 */

#include "flash_test.h"
#include "BG_FlashMgr.h"
#include "flash_bus.h"
#include "flash_devices.h"
#include "flash_nor_w25qxx.h"
#include "debug.h"
#include <string.h>

/* Test buffer area */
static uint8_t test_write_buffer[512];
static uint8_t test_read_buffer[512];

/**
 * @brief Test a single Flash device
 * @param dev Device pointer
 * @param device_name Device name (for printing)
 * @return true if test passed, false if test failed
 */
static bool test_single_flash_device(FlashDevice_t *dev, const char* device_name)
{
    FlashDevInfo_t info;
    uint32_t test_address = 0x1000; /* Use the second 4K sector */
    uint16_t i;
    bool result = true;

    DBG("\n========== Testing %s ==========\n", device_name);

    /* 1. Get device info */
    if (dev->ops->get_info(dev, &info) != FLASH_OK) {
        DBG("[FAIL] Failed to get device info\n");
        return false;
    }

    DBG("Device Info:\n");
    DBG("  Manufacturer: 0x%02X\n", info.mfg_id);
    DBG("  Memory Type:  0x%02X\n", info.mem_type);
    DBG("  Device ID:    0x%02X\n", info.dev_id);
    DBG("  Total Size:   %lu bytes\n", (unsigned long)info.total_size);
    DBG("  Page Size:    %lu bytes\n", (unsigned long)info.page_size);
    DBG("  Sector Size:  %lu bytes\n", (unsigned long)info.sector_size);
    DBG("  Block Size:   %lu bytes\n", (unsigned long)info.block_size);

    /* 2. Prepare test data */
    for (i = 0; i < 512; i++) {
        test_write_buffer[i] = (uint8_t)(0xA0 + (i & 0x0F));
    }

    /* 3. Single byte test */
    DBG("\n--- Single Byte Test ---\n");
    test_write_buffer[0] = 0xAA;

    if (FlashDev_EraseSector(dev, test_address) != FLASH_OK) {
        DBG("[FAIL] Sector erase failed\n");
        return false;
    }

    if (FlashDev_Write(dev, test_address, test_write_buffer, 1) != FLASH_OK) {
        DBG("[FAIL] Single byte write failed\n");
        return false;
    }

    if (FlashDev_Read(dev, test_address, test_read_buffer, 1) != FLASH_OK) {
        DBG("[FAIL] Single byte read failed\n");
        return false;
    }

    if (test_write_buffer[0] == test_read_buffer[0]) {
        DBG("[OK] Single byte: wrote 0x%02X, read 0x%02X\n", 
            test_write_buffer[0], test_read_buffer[0]);
    } else {
        DBG("[FAIL] Single byte: wrote 0x%02X, read 0x%02X\n", 
            test_write_buffer[0], test_read_buffer[0]);
        result = false;
    }

    /* 4. 256-byte page write test */
    DBG("\n--- 256 Byte Page Test ---\n");
    
    if (FlashDev_EraseSector(dev, test_address) != FLASH_OK) {
        DBG("[FAIL] Sector erase failed\n");
        return false;
    }

    if (FlashDev_Write(dev, test_address, test_write_buffer, 256) != FLASH_OK) {
        DBG("[FAIL] 256 byte write failed\n");
        return false;
    }

    memset(test_read_buffer, 0, 256);
    if (FlashDev_Read(dev, test_address, test_read_buffer, 256) != FLASH_OK) {
        DBG("[FAIL] 256 byte read failed\n");
        return false;
    }

    /* Verify data */
    for (i = 0; i < 256; i++) {
        if (test_write_buffer[i] != test_read_buffer[i]) {
            DBG("[FAIL] 256 byte verify failed at offset %d: wrote 0x%02X, read 0x%02X\n",
                i, test_write_buffer[i], test_read_buffer[i]);
            result = false;
            break;
        }
    }
    if (i == 256) {
        DBG("[OK] 256 byte write/read verified\n");
    }

    /* 5. 512-byte cross-page test */
    DBG("\n--- 512 Byte Cross-Page Test ---\n");

    if (FlashDev_EraseSector(dev, test_address) != FLASH_OK) {
        DBG("[FAIL] Sector erase failed\n");
        return false;
    }

    if (FlashDev_Write(dev, test_address, test_write_buffer, 512) != FLASH_OK) {
        DBG("[FAIL] 512 byte write failed\n");
        return false;
    }

    memset(test_read_buffer, 0, 512);
    if (FlashDev_Read(dev, test_address, test_read_buffer, 512) != FLASH_OK) {
        DBG("[FAIL] 512 byte read failed\n");
        return false;
    }

    /* Verify data */
    for (i = 0; i < 512; i++) {
        if (test_write_buffer[i] != test_read_buffer[i]) {
            DBG("[FAIL] 512 byte verify failed at offset %d: wrote 0x%02X, read 0x%02X\n",
                i, test_write_buffer[i], test_read_buffer[i]);
            result = false;
            break;
        }
    }
    if (i == 512) {
        DBG("[OK] 512 byte cross-page write/read verified\n");
    }

    return result;
}

/**
 * @brief Complete test for the new Flash driver architecture
 */
void FlashNewDriver_Test(void)
{
    bool nor1_result = false;
    bool nor2_result = false;
    FlashDevice_t *dev;

    DBG("\n");
    DBG("**************************************************\n");
    DBG("*     New Flash Driver Architecture Test        *\n");
    DBG("**************************************************\n");

    /* 1. Initialize Flash devices */
    DBG("\nInitializing Flash devices...\n");
    if (FlashDevices_Init() != FLASH_OK) {
        DBG("[FAIL] Flash devices initialization failed\n");
        return;
    }
    DBG("[OK] Flash devices initialized\n");

    /* 2. List all devices */
    DBG("\nListing all Flash devices:\n");
    uint8_t device_count = FlashBus_GetDeviceCount();
    DBG("Found %d Flash device(s)\n", device_count);

    if (device_count == 0) {
        DBG("[FAIL] No Flash devices found!\n");
        return;
    }

    /* 3. Test NOR1 (CS = A21, Device 0) */
    dev = FlashBus_GetDeviceById(FLASH_DEV_ID_SYSTEM);
    if (dev != NULL) {
        nor1_result = test_single_flash_device(dev, "NOR1 (CS=A21)");
    } else {
        DBG("[FAIL] Cannot get NOR1 device\n");
    }

    /* 4. Test NOR2 (CS = A22, Device 1) */
    dev = FlashBus_GetDeviceById(FLASH_DEV_ID_STORAGE);
    if (dev != NULL) {
        nor2_result = test_single_flash_device(dev, "NOR2 (CS=A22)");
    } else {
        DBG("[FAIL] Cannot get NOR2 device\n");
    }

    /* 5. Show final result */
    DBG("\n");
    DBG("========================================\n");
    DBG("         Test Summary                  \n");
    DBG("========================================\n");
    DBG("  NOR1 (CS=A21): %s\n", nor1_result ? "PASS" : "FAIL");
    DBG("  NOR2 (CS=A22): %s\n", nor2_result ? "PASS" : "FAIL");
    DBG("========================================\n");
    
    if (nor1_result && nor2_result) {
        DBG("  Overall Result: ALL TESTS PASSED\n");
    } else {
        DBG("  Overall Result: SOME TESTS FAILED\n");
    }
    DBG("========================================\n");
}

/**
 * @brief Quick function test (for debugging)
 */
void FlashNewDriver_QuickTest(void)
{
    FlashDevice_t *dev;
    uint8_t test_data = 0xAA;
    uint8_t read_data = 0;
    uint32_t addr = 0x1000;

    DBG("\n=== Flash New Driver Quick Test ===\n");

    /* Initialization */
    if (FlashDevices_Init() != FLASH_OK) {
        DBG("Init failed\n");
        return;
    }

    /* Test device 0 */
    dev = FlashBus_GetDeviceById(FLASH_DEV_ID_SYSTEM);
    if (dev != NULL) {
        DBG("Testing Device 0 (NOR1)...\n");
        FlashDev_EraseSector(dev, addr);
        FlashDev_Write(dev, addr, &test_data, 1);
        FlashDev_Read(dev, addr, &read_data, 1);
        DBG("Wrote: 0x%02X, Read: 0x%02X %s\n", 
            test_data, read_data, (test_data == read_data) ? "[OK]" : "[FAIL]");
    }

    /* Test device 1 */
    dev = FlashBus_GetDeviceById(FLASH_DEV_ID_STORAGE);
    if (dev != NULL) {
        test_data = 0x55;
        DBG("Testing Device 1 (NOR2)...\n");
        FlashDev_EraseSector(dev, addr);
        FlashDev_Write(dev, addr, &test_data, 1);
        FlashDev_Read(dev, addr, &read_data, 1);
        DBG("Wrote: 0x%02X, Read: 0x%02X %s\n", 
            test_data, read_data, (test_data == read_data) ? "[OK]" : "[FAIL]");
    }
}
