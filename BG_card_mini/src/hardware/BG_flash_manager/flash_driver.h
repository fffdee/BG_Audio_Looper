/**
 * flash_driver.h - Flash low-level driver abstraction layer
 * 
 * This file defines the abstract interface for Flash drivers, supporting:
 * - Multiple Flash chips of the same model controlled by different CS pins
 * - NOR Flash (W25Q64, etc.) and NAND Flash (W25N02, etc.)
 * - Unified driver interface for easy management at upper layers
 */

#ifndef __FLASH_DRIVER_H__
#define __FLASH_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

/*===========================================================================
 * Constant Definitions
 *===========================================================================*/

/* Flash type definitions */
typedef enum {
    FLASH_TYPE_NOR = 0,     /* NOR Flash (W25Qxx series) */
    FLASH_TYPE_NAND,        /* NAND Flash (W25Nxx series) */
    FLASH_TYPE_MAX
} FlashType_t;

/* Flash model definitions */
typedef enum {
    FLASH_MODEL_UNKNOWN = 0,
    /* NOR Flash */
    FLASH_MODEL_W25Q32,     /* 4MB */
    FLASH_MODEL_W25Q64,     /* 8MB */
    FLASH_MODEL_W25Q128,    /* 16MB */
    /* NAND Flash */
    FLASH_MODEL_W25N01,     /* 128MB */
    FLASH_MODEL_W25N02,     /* 256MB */
    FLASH_MODEL_MAX
} FlashModel_t;

/* Operation status codes */
typedef enum {
    FLASH_OK = 0,
    FLASH_ERROR_BUSY,
    FLASH_ERROR_TIMEOUT,
    FLASH_ERROR_BAD_BLOCK,
    FLASH_ERROR_ECC,
    FLASH_ERROR_PROGRAM_FAIL,
    FLASH_ERROR_ERASE_FAIL,
    FLASH_ERROR_PARAM,
    FLASH_ERROR_NOT_INIT
} FlashStatus_t;

/* Flash information structure */
typedef struct {
    FlashType_t type;           /* Flash type */
    FlashModel_t model;         /* Flash model */
    uint8_t manufacturer_id;    /* Manufacturer ID */
    uint8_t memory_type;        /* Memory type */
    uint8_t device_id;          /* Device ID */
    uint32_t total_size;        /* Total size (bytes) */
    uint32_t page_size;         /* Page size (bytes) */
    uint32_t sector_size;       /* Sector size (bytes) */
    uint32_t block_size;        /* Block size (bytes) */
    uint32_t block_count;       /* Block count */
} FlashInfo_t;

/* CS pin control function type */
typedef void (*FlashCsFunc_t)(bool enable);

/* Flash device configuration */
typedef struct {
    FlashCsFunc_t cs_enable;    /* CS enable function */
    FlashCsFunc_t cs_disable;   /* CS disable function (optional, use enable(false) if NULL) */
    uint32_t gpio_port;         /* GPIO port (for initialization) */
    uint32_t gpio_pin;          /* GPIO pin (for initialization) */
} FlashCsConfig_t;

/*===========================================================================
 * Flash Driver Structure
 *===========================================================================*/

/* Forward declaration */
typedef struct FlashDriver FlashDriver_t;

/* Flash driver operation interface */
struct FlashDriver {
    /* Device identification */
    uint8_t id;                 /* Device ID (0-based) */
    FlashType_t type;           /* Flash type */
    bool initialized;           /* Initialized flag */
    
    /* Device information */
    FlashInfo_t info;           /* Flash information */
    
    /* CS control */
    FlashCsConfig_t cs_config;  /* CS configuration */
    
    /* Private data */
    void *priv;                 /* Driver private data */
    
    /* Basic operations */
    FlashStatus_t (*init)(FlashDriver_t *drv);
    FlashStatus_t (*deinit)(FlashDriver_t *drv);
    FlashStatus_t (*read_id)(FlashDriver_t *drv, uint8_t *mfg, uint8_t *type, uint8_t *dev);
    
    /* Read/Write operations */
    FlashStatus_t (*read)(FlashDriver_t *drv, uint32_t addr, uint8_t *buf, uint32_t len);
    FlashStatus_t (*write)(FlashDriver_t *drv, uint32_t addr, const uint8_t *buf, uint32_t len);
    
    /* Erase operations */
    FlashStatus_t (*erase_sector)(FlashDriver_t *drv, uint32_t addr);
    FlashStatus_t (*erase_block)(FlashDriver_t *drv, uint32_t addr);
    FlashStatus_t (*erase_chip)(FlashDriver_t *drv);
    
    /* Status operations */
    FlashStatus_t (*get_status)(FlashDriver_t *drv, uint8_t *status);
    FlashStatus_t (*wait_ready)(FlashDriver_t *drv, uint32_t timeout_ms);
    
    /* Power management */
    FlashStatus_t (*power_down)(FlashDriver_t *drv);
    FlashStatus_t (*power_up)(FlashDriver_t *drv);
};

/*===========================================================================
 * Predefined CS Pin Configurations
 *===========================================================================*/

/* NOR Flash #0 (W25Q64) - GPIOA21 */
#define FLASH_NOR0_CS_INIT()    do { \
    GPIO_RegOneBitClear(GPIO_A_IE, GPIOA21); \
    GPIO_RegOneBitSet(GPIO_A_OE, GPIOA21); \
    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA21); \
} while(0)
#define FLASH_NOR0_CS_ENABLE()  GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA21)
#define FLASH_NOR0_CS_DISABLE() GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA21)

/* NOR Flash #1 (W25Q64) - GPIOA23 (Example, modify according to actual hardware) */
#define FLASH_NOR1_CS_INIT()    do { \
    GPIO_RegOneBitClear(GPIO_A_IE, GPIOA23); \
    GPIO_RegOneBitSet(GPIO_A_OE, GPIOA23); \
    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA23); \
} while(0)
#define FLASH_NOR1_CS_ENABLE()  GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA23)
#define FLASH_NOR1_CS_DISABLE() GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA23)

/* NAND Flash #0 (W25N02) - GPIOA22 */
#define FLASH_NAND0_CS_INIT()   do { \
    GPIO_RegOneBitClear(GPIO_A_IE, GPIOA22); \
    GPIO_RegOneBitSet(GPIO_A_OE, GPIOA22); \
    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA22); \
} while(0)
#define FLASH_NAND0_CS_ENABLE()  GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA22)
#define FLASH_NAND0_CS_DISABLE() GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA22)

/* WP pin control */
#define FLASH_WP_INIT()    do { \
    GPIO_RegOneBitClear(GPIO_A_IE, GPIOA17); \
    GPIO_RegOneBitSet(GPIO_A_OE, GPIOA17); \
    GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA17); \
} while(0)
#define FLASH_WP_ENABLE()   GPIO_RegOneBitClear(GPIO_A_OUT, GPIOA17)
#define FLASH_WP_DISABLE()  GPIO_RegOneBitSet(GPIO_A_OUT, GPIOA17)

/*===========================================================================
 * NOR Flash Command Definitions
 *===========================================================================*/
#define NOR_CMD_WRITE_ENABLE        0x06
#define NOR_CMD_WRITE_DISABLE       0x04
#define NOR_CMD_READ_STATUS         0x05
#define NOR_CMD_WRITE_STATUS        0x01
#define NOR_CMD_READ_DATA           0x03
#define NOR_CMD_FAST_READ           0x0B
#define NOR_CMD_PAGE_PROGRAM        0x02
#define NOR_CMD_SECTOR_ERASE_4K     0x20
#define NOR_CMD_BLOCK_ERASE_32K     0x52
#define NOR_CMD_BLOCK_ERASE_64K     0xD8
#define NOR_CMD_CHIP_ERASE          0xC7
#define NOR_CMD_POWER_DOWN          0xB9
#define NOR_CMD_RELEASE_PD          0xAB
#define NOR_CMD_READ_JEDEC_ID       0x9F

/* NOR Flash status bits */
#define NOR_STATUS_BUSY             0x01
#define NOR_STATUS_WEL              0x02

/* NOR Flash parameters */
#define NOR_PAGE_SIZE               256
#define NOR_SECTOR_SIZE_4K          4096
#define NOR_BLOCK_SIZE_32K          32768
#define NOR_BLOCK_SIZE_64K          65536

/*===========================================================================
 * NAND Flash Command Definitions (W25N02)
 *===========================================================================*/
#define NAND_CMD_RESET              0xFF
#define NAND_CMD_READ_JEDEC_ID      0x9F
#define NAND_CMD_READ_ID            0x90
#define NAND_CMD_GET_FEATURE        0x0F
#define NAND_CMD_SET_FEATURE        0x1F
#define NAND_CMD_WRITE_ENABLE       0x06
#define NAND_CMD_WRITE_DISABLE      0x04
#define NAND_CMD_PAGE_DATA_READ     0x13
#define NAND_CMD_READ_DATA          0x03
#define NAND_CMD_PROGRAM_LOAD       0x02
#define NAND_CMD_PROGRAM_EXECUTE    0x10
#define NAND_CMD_BLOCK_ERASE        0xD8

/* NAND Flash register addresses */
#define NAND_REG_PROTECTION         0xA0
#define NAND_REG_CONFIGURATION      0xB0
#define NAND_REG_STATUS             0xC0

/* NAND Flash status bits */
#define NAND_STATUS_BUSY            0x01
#define NAND_STATUS_WEL             0x02
#define NAND_STATUS_EFAIL           0x04
#define NAND_STATUS_PFAIL           0x08
#define NAND_STATUS_ECC1            0x20
#define NAND_STATUS_ECC2            0x40

/* NAND Flash parameters (W25N02) */
#define NAND_PAGE_SIZE              2048
#define NAND_PAGE_SPARE_SIZE        64
#define NAND_PAGES_PER_BLOCK        64
#define NAND_BLOCK_SIZE             (NAND_PAGE_SIZE * NAND_PAGES_PER_BLOCK)
#define NAND_W25N02_BLOCK_COUNT     1024
#define NAND_W25N02_TOTAL_SIZE      (256 * 1024 * 1024)  /* 256MB */

/*===========================================================================
 * Driver Creation Functions
 *===========================================================================*/

/**
 * Create NOR Flash driver instance
 * @param id Device ID
 * @param cs_enable CS enable function
 * @param cs_disable CS disable function
 * @return Driver instance pointer, NULL on failure
 */
FlashDriver_t* FlashDriver_CreateNOR(uint8_t id, FlashCsFunc_t cs_enable, FlashCsFunc_t cs_disable);

/**
 * Create NAND Flash driver instance
 * @param id Device ID
 * @param cs_enable CS enable function
 * @param cs_disable CS disable function
 * @return Driver instance pointer, NULL on failure
 */
FlashDriver_t* FlashDriver_CreateNAND(uint8_t id, FlashCsFunc_t cs_enable, FlashCsFunc_t cs_disable);

/**
 * Destroy Flash driver instance
 * @param drv Driver instance
 */
void FlashDriver_Destroy(FlashDriver_t *drv);

/*===========================================================================
 * Low-level SPI Communication Functions (Internal Use)
 *===========================================================================*/

void flash_spi_write_byte(uint8_t data);
uint8_t flash_spi_read_byte(void);
void flash_spi_write(const uint8_t *data, uint16_t len);
void flash_spi_read(uint8_t *data, uint16_t len);

#endif /* __FLASH_DRIVER_H__ */
