/**
 * flash_manager_example.h - Flash Manager Usage Example
 * 
 * Demonstrates how to use the refactored Flash driver and management layer
 */

#ifndef __FLASH_MANAGER_EXAMPLE_H__
#define __FLASH_MANAGER_EXAMPLE_H__

#include "flash_manager.h"

/*===========================================================================
 * Usage Examples
 *===========================================================================*/

/**
 * Example 1: Initialize the Flash Manager
 */
static inline void example_init(void)
{
    /* Initialize the Flash Manager */
    FlashStatus_t ret = FlashManager_Init();
    if (ret != FLASH_OK) {
        DBG("Flash Manager init failed!\n");
        return;
    }
    
    /* Check if system settings are valid, initialize if invalid */
    if (!FlashManager_IsSettingsValid()) {
        DBG("Settings invalid, initializing...\n");
        FlashManager_InitSettings();
    }
    
    /* Print Flash information */
    FlashManager_PrintInfo();
}

/**
 * Example 2: System settings read/write
 */

/* Define offset addresses for settings items */
#define SETTING_VOLUME          0x0000  /* Volume setting, 4 bytes */
#define SETTING_EQ_MODE         0x0004  /* EQ mode, 4 bytes */
#define SETTING_BLUETOOTH_NAME  0x0100  /* Bluetooth name, 32 bytes */
#define SETTING_USER_DATA       0x0200  /* User data area */

static inline void example_settings(void)
{
    /* Read volume setting */
    uint32_t volume;
    FlashManager_ReadSettings(SETTING_VOLUME, (uint8_t*)&volume, sizeof(volume));
    DBG("Current volume: %d\n", volume);
    
    /* Write new volume setting (Note: Flash must be erased before writing) */
    /* For frequently changed settings, it is recommended to use RAM cache and write periodically */
    volume = 80;
    
    /* Erase the sector where the setting is located */
    FlashManager_EraseSector(PARTITION_TYPE_SYSTEM, 0);
    
    /* Re-write magic value and settings */
    uint32_t magic = SETTINGS_MAGIC_VALUE;
    FlashManager_Write(PARTITION_TYPE_SYSTEM, SETTINGS_MAGIC_ADDR, (uint8_t*)&magic, sizeof(magic));
    FlashManager_WriteSettings(SETTING_VOLUME, (uint8_t*)&volume, sizeof(volume));
}

/**
 * Example 3: Looper partition read/write
 */
static inline void example_looper(void)
{
    uint8_t audio_data[512];
    uint32_t offset = 0;
    
    /* Get Looper partition size */
    uint32_t looper_size = FlashManager_LooperGetSize();
    DBG("Looper partition size: %d KB\n", looper_size / 1024);
    
    /* Erase Looper sector (must erase before writing) */
    FlashManager_LooperEraseSector(offset);
    
    /* Write audio data */
    FlashManager_LooperWrite(offset, audio_data, sizeof(audio_data));
    
    /* Read audio data */
    FlashManager_LooperRead(offset, audio_data, sizeof(audio_data));
}

/**
 * Example 4: Storage partition read/write (second Flash chip)
 */
static inline void example_storage(void)
{
    uint8_t data[256];
    uint32_t offset = 0;
    int i;
    
    /* Get storage partition size */
    uint32_t storage_size = FlashManager_StorageGetSize();
    DBG("Storage partition size: %d KB\n", storage_size / 1024);
    
    /* Erase storage sector */
    FlashManager_StorageEraseSector(offset);
    
    /* Write data */
    for (i = 0; i < 256; i++) {
        data[i] = i;
    }
    FlashManager_StorageWrite(offset, data, sizeof(data));
    
    /* Read data */
    FlashManager_StorageRead(offset, data, sizeof(data));
}

/**
 * Example 5: Direct access to the underlying driver
 */
static inline void example_direct_access(void)
{
    /* Get Flash #0 driver */
    FlashDriver_t *flash0 = FlashManager_GetFlash(FLASH_DEV_0);
    if (flash0) {
        DBG("Flash #0 size: %d MB\n", flash0->info.total_size / (1024*1024));
        
        /* Directly call driver functions */
        uint8_t mfg, type, dev;
        flash0->read_id(flash0, &mfg, &type, &dev);
        DBG("Flash #0 ID: %02X %02X %02X\n", mfg, type, dev);
    }
    
    /* Get Flash #1 driver */
    FlashDriver_t *flash1 = FlashManager_GetFlash(FLASH_DEV_1);
    if (flash1) {
        DBG("Flash #1 size: %d MB\n", flash1->info.total_size / (1024*1024));
    }
}

/**
 * Example 6: Test Flash read/write
 */
static inline void example_test(void)
{
    /* Test Flash #0 */
    DBG("Testing Flash #0...\n");
    FlashManager_Test(FLASH_DEV_0);
    
    /* Test Flash #1 */
    DBG("Testing Flash #1...\n");
    FlashManager_Test(FLASH_DEV_1);
}

/*===========================================================================
 * Partition Layout Description
 *===========================================================================
 * 
 * Flash #0 (W25Q64 - 8MB) @ CS = GPIOA21
 * ┌─────────────────────────────────────────┐
 * │         System Settings Partition (1MB) │  0x000000 - 0x0FFFFF
 * │  - Magic Value (4B)                     │  0x000000
 * │  - Version (4B)                         │  0x000004
 * │  - Settings Data                        │  0x000100 - 0x07FFFF
 * │  - Backup Area                          │  0x080000 - 0x0FFFFF
 * ├─────────────────────────────────────────┤
 * │         Looper Partition (7MB)          │  0x100000 - 0x7FFFFF
 * │  - Audio Recording Data                 │
 * │  - Loop Playback Data                   │
 * └─────────────────────────────────────────┘
 * 
 * Flash #1 (W25Q64 - 8MB) @ CS = GPIOA23 (modify according to actual hardware)
 * ┌─────────────────────────────────────────┐
 * │         Storage Partition (8MB)         │  0x000000 - 0x7FFFFF
 * │  - General Data Storage                 │
 * │  - Preset/Tone Data                     │
 * │  - Other User Data                      │
 * └─────────────────────────────────────────┘
 * 
 *===========================================================================*/

#endif /* __FLASH_MANAGER_EXAMPLE_H__ */
