/**
 * BG_FlashMgr.h - BanGUI Flash Manager (Application Layer Interface)
 *
 * Features:
 *   - Manage multiple Flash chips (NOR/NAND)
 *   - Provide partition-level read/write interfaces
 *   - Automatically handle erase operations
 *   - Thread-safe protection
 *   - Simple and easy-to-use API
 *
 * Usage:
 *   BG_FlashMgr.Init();
 *   BG_FlashMgr.WriteLooper(offset, data, size);
 *   BG_FlashMgr.ReadLooper(offset, buffer, size);
 */

#ifndef __BG_FLASH_MGR_H__
#define __BG_FLASH_MGR_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Status Definitions
 *===========================================================================*/

#define BG_FLASH_OK                 0
#define BG_FLASH_ERROR             -1
#define BG_FLASH_ERROR_PARAM       -2
#define BG_FLASH_ERROR_NOT_INIT    -3
#define BG_FLASH_ERROR_ERASE       -4
#define BG_FLASH_ERROR_WRITE       -5
#define BG_FLASH_ERROR_READ        -6
#define BG_FLASH_ERROR_VERIFY      -7
#define BG_FLASH_ERROR_TIMEOUT     -8
#define BG_FLASH_ERROR_BUSY        -9
#define BG_FLASH_ERROR_NO_SPACE    -10

/*===========================================================================
 * Partition Definitions
 *===========================================================================*/

/* Flash #0 (8MB NOR Flash) - GPIOA21 */
#define BG_FLASH_PARTITION_SYSTEM_SIZE      (1 * 1024 * 1024)    /* 1MB */
#define BG_FLASH_PARTITION_LOOPER_SIZE      (7 * 1024 * 1024)    /* 7MB */

/* Flash #1 (8MB NOR Flash) - GPIOA23 */
#define BG_FLASH_PARTITION_STORAGE_SIZE     (8 * 1024 * 1024)    /* 8MB */

/* Sector/Block Size */
#define BG_FLASH_SECTOR_SIZE                4096                  /* 4KB */
#define BG_FLASH_BLOCK_SIZE                 (64 * 1024)           /* 64KB */
#define BG_FLASH_PAGE_SIZE                  256

/*===========================================================================
 * Device Status
 *===========================================================================*/

typedef struct {
    bool initialized;           /* Initialization flag */
    bool ready;                 /* Device ready */
    uint8_t device_id;          /* Device ID */
    uint32_t total_size;        /* Total capacity (bytes) */
    uint32_t used_size;         /* Used space */
    uint32_t error_count;       /* Error count */
} BG_FlashDeviceStatus_t;

typedef struct {
    BG_FlashDeviceStatus_t flash0;   /* System Flash status */
    BG_FlashDeviceStatus_t flash1;   /* Storage Flash status */
    bool mutex_initialized;          /* Mutex initialization flag */
} BG_FlashMgrStatus_t;

/*===========================================================================
 * BG_FlashMgr Interface Structure
 *===========================================================================*/

typedef struct {
    /* Initialization and De-initialization */
    int32_t (*Init)(void);
    void (*DeInit)(void);
    
    /* System Partition Operations (Flash #0 first 1MB) */
    int32_t (*ReadSystem)(uint32_t offset, uint8_t *buffer, uint32_t size);
    int32_t (*WriteSystem)(uint32_t offset, const uint8_t *data, uint32_t size);
    int32_t (*EraseSystemSector)(uint32_t offset);
    
    /* Looper Partition Operations (Flash #0 last 7MB) */
    int32_t (*ReadLooper)(uint32_t offset, uint8_t *buffer, uint32_t size);
    int32_t (*WriteLooper)(uint32_t offset, const uint8_t *data, uint32_t size);
    int32_t (*EraseLooperSector)(uint32_t offset);
    int32_t (*EraseLooperBlock)(uint32_t offset);
    int32_t (*EraseLooperAll)(void);
    
    /* Storage Partition Operations (Flash #1 entire 8MB) */
    int32_t (*ReadStorage)(uint32_t offset, uint8_t *buffer, uint32_t size);
    int32_t (*WriteStorage)(uint32_t offset, const uint8_t *data, uint32_t size);
    int32_t (*EraseStorageSector)(uint32_t offset);
    int32_t (*EraseStorageBlock)(uint32_t offset);
    int32_t (*EraseStorageAll)(void);
    
    /* Status Query */
    int32_t (*GetStatus)(BG_FlashMgrStatus_t *status);
    bool (*IsReady)(void);
    uint32_t (*GetLooperFreeSpace)(void);
    uint32_t (*GetStorageFreeSpace)(void);
    
    /* Testing and Debugging */
    int32_t (*TestDevice)(uint8_t device_id);
    void (*PrintInfo)(void);
    int32_t (*Format)(uint8_t device_id);  /* Format device */
    
} BG_FlashMgr_t;

/*===========================================================================
 * Global Instance
 *===========================================================================*/

extern BG_FlashMgr_t BG_FlashMgr;

/*===========================================================================
 * Convenience Macro Definitions (Optional)
 *===========================================================================*/

#define BG_FLASH_INIT()                 BG_FlashMgr.Init()
#define BG_FLASH_DEINIT()               BG_FlashMgr.DeInit()

#define BG_FLASH_READ_LOOPER(o,b,s)     BG_FlashMgr.ReadLooper(o,b,s)
#define BG_FLASH_WRITE_LOOPER(o,d,s)    BG_FlashMgr.WriteLooper(o,d,s)
#define BG_FLASH_ERASE_LOOPER(o)        BG_FlashMgr.EraseLooperSector(o)

#define BG_FLASH_READ_STORAGE(o,b,s)    BG_FlashMgr.ReadStorage(o,b,s)
#define BG_FLASH_WRITE_STORAGE(o,d,s)   BG_FlashMgr.WriteStorage(o,d,s)
#define BG_FLASH_ERASE_STORAGE(o)       BG_FlashMgr.EraseStorageSector(o)

#define BG_FLASH_IS_READY()             BG_FlashMgr.IsReady()
#define BG_FLASH_PRINT_INFO()           BG_FlashMgr.PrintInfo()

/*===========================================================================
 * Usage Examples
 *===========================================================================*/

#if 0
/* Example 1: Initialization and Basic Read/Write */
void example_basic_usage(void)
{
    uint8_t buffer[256];
    
    // Initialization
    if (BG_FlashMgr.Init() != BG_FLASH_OK) {
        DBG("Flash init failed!\n");
        return;
    }
    
    // Erase the first sector of the Looper partition
    BG_FlashMgr.EraseLooperSector(0);
    
    // Write data
    memset(buffer, 0xAA, 256);
    BG_FlashMgr.WriteLooper(0, buffer, 256);
    
    // Read data
    memset(buffer, 0, 256);
    BG_FlashMgr.ReadLooper(0, buffer, 256);
    
    // Verify
    {
        int i;
        for (i = 0; i < 256; i++) {
            if (buffer[i] != 0xAA) {
                DBG("Verify failed at %d\n", i);
            }
        }
    }
}

/* Example 2: Using Convenience Macros */
void example_macro_usage(void)
{
    uint8_t data[128] = {0};
    
    BG_FLASH_INIT();
    
    if (BG_FLASH_IS_READY()) {
        BG_FLASH_WRITE_LOOPER(0x1000, data, 128);
        BG_FLASH_READ_LOOPER(0x1000, data, 128);
    }
    
    BG_FLASH_PRINT_INFO();
}

/* Example 3: Status Query */
void example_status_query(void)
{
    BG_FlashMgrStatus_t status;
    
    if (BG_FlashMgr.GetStatus(&status) == BG_FLASH_OK) {
        DBG("Flash0: %s, Size=%dMB, Errors=%d\n",
            status.flash0.ready ? "Ready" : "Not Ready",
            status.flash0.total_size / (1024*1024),
            status.flash0.error_count);
            
        DBG("Looper Free: %d KB\n", 
            BG_FlashMgr.GetLooperFreeSpace() / 1024);
    }
}

/* Example 4: Large Data Write (Automatically Handle Page Alignment) */
void example_large_write(void)
{
    uint8_t *large_data = malloc(64 * 1024);
    
    // Erase a block (64KB)
    BG_FlashMgr.EraseLooperBlock(0);
    
    // Write 64KB of data (automatically handle page alignment)
    BG_FlashMgr.WriteLooper(0, large_data, 64 * 1024);
    
    free(large_data);
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BG_FLASH_MGR_H__ */
