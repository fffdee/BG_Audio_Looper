/**
 * flash_manager.h - Flash management layer
 * 
 * Manage partitions and access for multiple Flash chips:
 * - Flash #0 (W25Q64): First 1MB for system settings + Last 7MB for Looper
 * - Flash #1 (W25Q64): 8MB pure storage
 */

#ifndef __FLASH_MANAGER_H__
#define __FLASH_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>
#include "flash_driver.h"

/*===========================================================================
 * Partition Definitions
 *===========================================================================*/

/* Flash device ID */
#define FLASH_DEV_0             0   /* Main Flash (System+Looper) */
#define FLASH_DEV_1             1   /* Storage Flash */
#define FLASH_DEV_MAX           2

/* Flash #0 partition layout (W25Q64 = 8MB) */
#define PARTITION_SYSTEM_START      0x000000    /* System settings start address */
#define PARTITION_SYSTEM_SIZE       0x100000    /* System settings size: 1MB */
#define PARTITION_LOOPER_START      0x100000    /* Looper start address */
#define PARTITION_LOOPER_SIZE       0x700000    /* Looper size: 7MB */

/* Flash #1 partition layout (W25Q64 = 8MB) */
#define PARTITION_STORAGE_START     0x000000    /* Storage start address */
#define PARTITION_STORAGE_SIZE      0x800000    /* Storage size: 8MB */

/* System settings partition internal layout */
#define SETTINGS_MAGIC_ADDR         0x000000    /* Magic word address */
#define SETTINGS_VERSION_ADDR       0x000004    /* Version number address */
#define SETTINGS_DATA_ADDR          0x000100    /* Data start address */
#define SETTINGS_BACKUP_ADDR        0x080000    /* Backup area start address (512KB) */
#define SETTINGS_MAGIC_VALUE        0x42475346  /* "BGSF" */

/*===========================================================================
 * Partition Types
 *===========================================================================*/

typedef enum {
    PARTITION_TYPE_SYSTEM = 0,  /* System settings partition */
    PARTITION_TYPE_LOOPER,      /* Looper partition */
    PARTITION_TYPE_STORAGE,     /* General storage partition */
    PARTITION_TYPE_MAX
} PartitionType_t;

/*===========================================================================
 * Partition Information Structure
 *===========================================================================*/

typedef struct {
    PartitionType_t type;       /* Partition type */
    uint8_t flash_id;           /* Belonging Flash device ID */
    uint32_t start_addr;        /* Partition start address */
    uint32_t size;              /* Partition size */
    const char *name;           /* Partition name */
} PartitionInfo_t;

/*===========================================================================
 * Flash Manager Status
 *===========================================================================*/

typedef struct {
    bool initialized;                       /* Whether initialized */
    FlashDriver_t *flash[FLASH_DEV_MAX];    /* Flash driver instances */
    PartitionInfo_t partitions[PARTITION_TYPE_MAX]; /* Partition information */
} FlashManager_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * Initialize Flash manager
 * @return FLASH_OK success, others failure
 */
FlashStatus_t FlashManager_Init(void);

/**
 * Deinitialize Flash manager
 */
void FlashManager_DeInit(void);

/**
 * Get Flash manager instance
 * @return Manager pointer
 */
FlashManager_t* FlashManager_GetInstance(void);

/**
 * Get specified Flash device
 * @param flash_id Flash device ID
 * @return Driver pointer, NULL on failure
 */
FlashDriver_t* FlashManager_GetFlash(uint8_t flash_id);

/**
 * Get partition information
 * @param type Partition type
 * @return Partition info pointer, NULL on failure
 */
const PartitionInfo_t* FlashManager_GetPartition(PartitionType_t type);

/*===========================================================================
 * Partition Read/Write API
 *===========================================================================*/

/**
 * Read data from partition
 * @param type Partition type
 * @param offset Offset within partition
 * @param buf Data buffer
 * @param len Read length
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_Read(PartitionType_t type, uint32_t offset, uint8_t *buf, uint32_t len);

/**
 * Write data to partition
 * @param type Partition type
 * @param offset Offset within partition
 * @param buf Data buffer
 * @param len Write length
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_Write(PartitionType_t type, uint32_t offset, const uint8_t *buf, uint32_t len);

/**
 * Erase partition sector
 * @param type Partition type
 * @param offset Offset within partition (will align to sector boundary)
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_EraseSector(PartitionType_t type, uint32_t offset);

/**
 * Erase entire partition
 * @param type Partition type
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_ErasePartition(PartitionType_t type);

/*===========================================================================
 * System Settings API
 *===========================================================================*/

/**
 * Read system settings
 * @param key Setting key (offset address)
 * @param buf Data buffer
 * @param len Read length
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_ReadSettings(uint32_t key, uint8_t *buf, uint32_t len);

/**
 * Write system settings
 * @param key Setting key (offset address)
 * @param buf Data buffer
 * @param len Write length
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_WriteSettings(uint32_t key, const uint8_t *buf, uint32_t len);

/**
 * Check if system settings are valid
 * @return true valid, false invalid
 */
bool FlashManager_IsSettingsValid(void);

/**
 * Initialize system settings (first use)
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_InitSettings(void);

/*===========================================================================
 * Looper Dedicated API
 *===========================================================================*/

/**
 * Read Looper data
 * @param offset Offset within Looper partition
 * @param buf Data buffer
 * @param len Read length
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_LooperRead(uint32_t offset, uint8_t *buf, uint32_t len);

/**
 * Write Looper data
 * @param offset Offset within Looper partition
 * @param buf Data buffer
 * @param len Write length
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_LooperWrite(uint32_t offset, const uint8_t *buf, uint32_t len);

/**
 * Erase Looper sector
 * @param offset Offset within Looper partition
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_LooperEraseSector(uint32_t offset);

/**
 * Get Looper partition size
 * @return Partition size (bytes)
 */
uint32_t FlashManager_LooperGetSize(void);

/*===========================================================================
 * Storage Partition API
 *===========================================================================*/

/**
 * Read storage data
 * @param offset Offset within storage partition
 * @param buf Data buffer
 * @param len Read length
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_StorageRead(uint32_t offset, uint8_t *buf, uint32_t len);

/**
 * Write storage data
 * @param offset Offset within storage partition
 * @param buf Data buffer
 * @param len Write length
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_StorageWrite(uint32_t offset, const uint8_t *buf, uint32_t len);

/**
 * Erase storage sector
 * @param offset Offset within storage partition
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_StorageEraseSector(uint32_t offset);

/**
 * Get storage partition size
 * @return Partition size (bytes)
 */
uint32_t FlashManager_StorageGetSize(void);

/*===========================================================================
 * Debug and Test
 *===========================================================================*/

/**
 * Print Flash manager information
 */
void FlashManager_PrintInfo(void);

/**
 * Test Flash read/write
 * @param flash_id Flash device ID
 * @return FLASH_OK success
 */
FlashStatus_t FlashManager_Test(uint8_t flash_id);

#endif /* __FLASH_MANAGER_H__ */
