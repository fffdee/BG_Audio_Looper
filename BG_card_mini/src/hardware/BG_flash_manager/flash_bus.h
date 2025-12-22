/**
 * flash_bus.h - Flash bus manager
 *
 * Uses bus-driver model:
 * - Drivers are registered to the bus
 * - Unified management of all Flash devices
 * - Provides Shell interface
 */

#ifndef __FLASH_BUS_H__
#define __FLASH_BUS_H__

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * Constant Definitions
 *===========================================================================*/

#define FLASH_BUS_MAX_DEVICES       8       /* Maximum number of devices on the bus */
#define FLASH_NAME_MAX_LEN          16      /* Maximum device name length */
#define FLASH_DEV_NAME_MAX          FLASH_NAME_MAX_LEN  /* Alias compatibility */

/*===========================================================================
 * Status Code Definitions
 *===========================================================================*/

typedef enum {
    FLASH_OK = 0,
    FLASH_ERR_PARAM,
    FLASH_ERR_BUSY,
    FLASH_ERR_TIMEOUT,
    FLASH_ERR_NOT_FOUND,
    FLASH_ERR_NOT_INIT,
    FLASH_ERR_BAD_BLOCK,
    FLASH_ERR_ECC,
    FLASH_ERR_PROGRAM,
    FLASH_ERR_ERASE,
    FLASH_ERR_VERIFY,
    FLASH_ERR_FULL,
    FLASH_ERR_NOMEM,
    FLASH_ERR_READ,
    FLASH_ERR_WRITE
} FlashStatus_t;

/*===========================================================================
 * Flash Type Definitions (compatible with audio_looper.h)
 *===========================================================================*/

#ifndef FLASH_TYPE_DEFINED
#define FLASH_TYPE_DEFINED
typedef enum {
    FLASH_TYPE_NOR = 0,
    FLASH_TYPE_NAND,
    FLASH_TYPE_MAX
} FlashType_t;
#endif /* FLASH_TYPE_DEFINED */

/*===========================================================================
 * Flash Device Information
 *===========================================================================*/

typedef struct {
    uint8_t  mfg_id;            /* Manufacturer ID */
    uint8_t  mem_type;          /* Memory type */
    uint8_t  dev_id;            /* Device ID */
    uint32_t total_size;        /* Total capacity (bytes) */
    uint32_t page_size;         /* Page size */
    uint32_t sector_size;       /* Sector size */
    uint32_t block_size;        /* Block size */
    uint16_t block_count;       /* Number of blocks */
} FlashDevInfo_t;

/*===========================================================================
 * Flash Driver Operation Interface
 *===========================================================================*/

/* Forward declaration */
typedef struct FlashDevice FlashDevice_t;

/* Driver operation function pointer types */
typedef struct {
    /* Initialization/De-initialization */
    FlashStatus_t (*init)(FlashDevice_t *dev);
    FlashStatus_t (*deinit)(FlashDevice_t *dev);
    
    /* Read/Write operations */
    FlashStatus_t (*read)(FlashDevice_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);
    FlashStatus_t (*write)(FlashDevice_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len);
    
    /* Erase operations */
    FlashStatus_t (*erase_sector)(FlashDevice_t *dev, uint32_t addr);
    FlashStatus_t (*erase_block)(FlashDevice_t *dev, uint32_t addr);
    FlashStatus_t (*erase_chip)(FlashDevice_t *dev);
    
    /* Status operations */
    FlashStatus_t (*get_status)(FlashDevice_t *dev, uint8_t *status);
    FlashStatus_t (*wait_ready)(FlashDevice_t *dev, uint32_t timeout_ms);
    
    /* Information retrieval */
    FlashStatus_t (*read_id)(FlashDevice_t *dev);
    FlashStatus_t (*get_info)(FlashDevice_t *dev, FlashDevInfo_t *info);
} FlashOps_t;

/*===========================================================================
 * CS Pin Control
 *===========================================================================*/

typedef struct {
    void (*init)(void);         /* Initialize CS pin */
    void (*select)(void);       /* Select device (CS low) */
    void (*deselect)(void);     /* Deselect (CS high) */
} FlashCS_t;

/*===========================================================================
 * Flash Device Structure
 *===========================================================================*/

struct FlashDevice {
    /* Device identification */
    char            name[FLASH_NAME_MAX_LEN];   /* Device name */
    uint8_t         id;                         /* Device ID (assigned by bus) */
    FlashType_t     type;                       /* Flash type */
    bool            initialized;                /* Is initialized */
    bool            registered;                 /* Is registered */
    
    /* Device information */
    FlashDevInfo_t  info;                       /* Device information */
    
    /* Hardware control */
    FlashCS_t       cs;                         /* CS pin control */
    
    /* Driver operations */
    const FlashOps_t *ops;                      /* Operation functions */
    
    /* Private data */
    void            *priv;                      /* Driver private data */
    
    /* Linked list */
    FlashDevice_t   *next;                      /* Next device */
};

/*===========================================================================
 * Flash Bus Structure
 *===========================================================================*/

typedef struct {
    bool            initialized;                /* Is bus initialized */
    uint8_t         device_count;               /* Number of registered devices */
    FlashDevice_t   *head;                      /* Device list head */
    FlashDevice_t   *devices[FLASH_BUS_MAX_DEVICES]; /* Device array (indexed by ID) */
} FlashBus_t;

/*===========================================================================
 * Bus API
 *===========================================================================*/

/**
 * Initialize Flash bus
 */
FlashStatus_t FlashBus_Init(void);

/**
 * De-initialize Flash bus
 */
void FlashBus_DeInit(void);

/**
 * Get bus instance
 */
FlashBus_t* FlashBus_GetInstance(void);

/**
 * Register device to bus
 * @param dev Device pointer
 * @return FLASH_OK on success
 */
FlashStatus_t FlashBus_Register(FlashDevice_t *dev);

/**
 * Unregister device from bus
 * @param dev Device pointer
 * @return FLASH_OK on success
 */
FlashStatus_t FlashBus_Unregister(FlashDevice_t *dev);

/**
 * Get device by ID
 * @param id Device ID
 * @return Device pointer, NULL on failure
 */
FlashDevice_t* FlashBus_GetDeviceById(uint8_t id);

/**
 * Get device by name
 * @param name Device name
 * @return Device pointer, NULL on failure
 */
FlashDevice_t* FlashBus_GetDeviceByName(const char *name);

/**
 * Get device count
 */
uint8_t FlashBus_GetDeviceCount(void);

/**
 * Iterate over all devices
 * @param callback Callback function
 * @param user_data User data
 */
void FlashBus_ForEach(void (*callback)(FlashDevice_t *dev, void *user_data), void *user_data);

/*===========================================================================
 * Device Operation Convenience API
 *===========================================================================*/

/**
 * Initialize device
 */
FlashStatus_t FlashDev_Init(FlashDevice_t *dev);

/**
 * Read data
 */
FlashStatus_t FlashDev_Read(FlashDevice_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * Write data
 */
FlashStatus_t FlashDev_Write(FlashDevice_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * Erase sector
 */
FlashStatus_t FlashDev_EraseSector(FlashDevice_t *dev, uint32_t addr);

/**
 * Erase block
 */
FlashStatus_t FlashDev_EraseBlock(FlashDevice_t *dev, uint32_t addr);

/**
 * Chip erase
 */
FlashStatus_t FlashDev_EraseChip(FlashDevice_t *dev);

/**
 * Print device information
 */
void FlashDev_PrintInfo(FlashDevice_t *dev);

/*===========================================================================
 * Shell Command Interface
 *===========================================================================*/

/**
 * Register Flash Shell commands
 */
void FlashBus_RegisterShellCommands(void);

/**
 * Flash Shell command handler
 * @param argc Argument count
 * @param argv Argument array
 * @return 0 on success, other values on failure
 */
int FlashBus_ShellCmd(int argc, char *argv[]);

/*===========================================================================
 * Debug Interface
 *===========================================================================*/

/**
 * Print bus information
 */
void FlashBus_PrintInfo(void);

/**
 * Test device
 */
FlashStatus_t FlashBus_TestDevice(uint8_t id);

#endif /* __FLASH_BUS_H__ */
