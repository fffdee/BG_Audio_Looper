/**
 *****************************************************************************
 * @file     drv_w25qxx.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    W25Qxx Flash driver framework adaptation layer
 *****************************************************************************
 * @attention
 *
 * Register W25Qxx Flash driver to driver framework, providing:
 * 1. Driver registered to /driver/spi/w25qxx
 * 2. Parameter nodes: capacity/page_size/sector_size etc.
 * 3. Shell command access: cat /driver/spi/w25qxx/capacity
 *
 *****************************************************************************
 */

#include "drv_w25qxx.h"
#include "drv_device.h"
#include "drv_fs.h"
#include "flash_nor_w25qxx.h"
#include "BG_FlashMgr.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************************
 * Private data structures
 ******************************************************************************/
typedef struct {
    uint32_t capacity;      // Flash capacity (bytes)
    uint16_t page_size;     // Page size
    uint32_t sector_size;   // Sector size
    bool initialized;
    char name[32];
    uint16_t device_id;
} W25qxxPrivData_t;

static W25qxxPrivData_t g_w25qxx_priv = {
    .capacity = 0,
    .page_size = 256,
    .sector_size = 4096,
    .initialized = false,
    .name = "W25Qxx_Flash",
    .device_id = 0
};

/*******************************************************************************
 * Parameter read/write callback functions
 ******************************************************************************/

static int param_get_name(char *buf, uint16_t maxLen, void *userData)
{
    W25qxxPrivData_t *priv = (W25qxxPrivData_t *)userData;
    snprintf(buf, maxLen, "%s", priv->name);
    return strlen(buf);
}

static int param_get_capacity(char *buf, uint16_t maxLen, void *userData)
{
    W25qxxPrivData_t *priv = (W25qxxPrivData_t *)userData;
    // Display in KB
    snprintf(buf, maxLen, "%lu KB", (unsigned long)(priv->capacity / 1024));
    return strlen(buf);
}

static int param_get_page_size(char *buf, uint16_t maxLen, void *userData)
{
    W25qxxPrivData_t *priv = (W25qxxPrivData_t *)userData;
    snprintf(buf, maxLen, "%u", priv->page_size);
    return strlen(buf);
}

static int param_get_sector_size(char *buf, uint16_t maxLen, void *userData)
{
    W25qxxPrivData_t *priv = (W25qxxPrivData_t *)userData;
    snprintf(buf, maxLen, "%lu", (unsigned long)priv->sector_size);
    return strlen(buf);
}

static int param_get_status(char *buf, uint16_t maxLen, void *userData)
{
    W25qxxPrivData_t *priv = (W25qxxPrivData_t *)userData;
    snprintf(buf, maxLen, "%s", priv->initialized ? "initialized" : "uninitialized");
    return strlen(buf);
}

static int param_get_device_id(char *buf, uint16_t maxLen, void *userData)
{
    W25qxxPrivData_t *priv = (W25qxxPrivData_t *)userData;
    snprintf(buf, maxLen, "0x%04X", priv->device_id);
    return strlen(buf);
}

static int param_cmd_erase_chip(const char *value, void *userData)
{
    if (strcmp(value, "confirm") == 0) {
        // TODO: Perform chip erase
        // Flash_EraseChip();
        return 0;
    }
    return -1;
}

/*******************************************************************************
 * Parameter definition table
 ******************************************************************************/
static const FsParamDef_t w25qxx_params[] = {
    {
        .name = "name",
        .desc = "Flash driver name",
        .get = param_get_name,
        .set = NULL,
    },
    {
        .name = "capacity",
        .desc = "Flash capacity",
        .get = param_get_capacity,
        .set = NULL,
    },
    {
        .name = "page_size",
        .desc = "Page size (bytes)",
        .get = param_get_page_size,
        .set = NULL,
    },
    {
        .name = "sector_size",
        .desc = "Sector size (bytes)",
        .get = param_get_sector_size,
        .set = NULL,
    },
    {
        .name = "status",
        .desc = "Initialization status",
        .get = param_get_status,
        .set = NULL,
    },
    {
        .name = "device_id",
        .desc = "Device ID",
        .get = param_get_device_id,
        .set = NULL,
    },
    {
        .name = "erase_chip",
        .desc = "Chip erase (write 'confirm' to execute)",
        .get = NULL,
        .set = param_cmd_erase_chip,
    },
    FS_PARAM_END
};

/*******************************************************************************
 * Driver operation functions
 ******************************************************************************/

static int w25qxx_drv_init(void *priv)
{
    W25qxxPrivData_t *flash = (W25qxxPrivData_t *)priv;
    
    if (flash->initialized) {
        return 0;
    }
    
    // Call underlying Flash initialization
    // Flash_Init();  // Assume this function exists
    
    // Read device ID
    // flash->device_id = Flash_ReadID();
    flash->device_id = 0xEF40;  // W25Q64 example ID
    
    // Determine capacity based on ID
    flash->capacity = 8 * 1024 * 1024;  // 8MB for W25Q64
    
    flash->initialized = true;
    
    return 0;
}

static int w25qxx_drv_deinit(void *priv)
{
    W25qxxPrivData_t *flash = (W25qxxPrivData_t *)priv;
    flash->initialized = false;
    return 0;
}

static int w25qxx_drv_open(void *priv)
{
    return 0;
}

static int w25qxx_drv_close(void *priv)
{
    return 0;
}

static int w25qxx_drv_read(void *priv, uint8_t *buf, uint32_t len)
{
    // Implement Flash read
    // return Flash_Read(0, buf, len);
    return len;
}

static int w25qxx_drv_write(void *priv, const uint8_t *buf, uint32_t len)
{
    // Implement Flash write
    // return Flash_Write(0, buf, len);
    return len;
}

static int w25qxx_drv_ioctl(void *priv, uint32_t cmd, void *arg)
{
    W25qxxPrivData_t *flash = (W25qxxPrivData_t *)priv;
    
    switch (cmd) {
        case 0x01:  // Erase sector
        {
            uint32_t addr = *(uint32_t *)arg;
            // Flash_EraseSector(addr);
            break;
        }
        case 0x02:  // Erase block
        {
            uint32_t addr = *(uint32_t *)arg;
            // Flash_EraseBlock(addr);
            break;
        }
        case 0x03:  // Chip erase
            // Flash_EraseChip();
            break;
        default:
            return -1;
    }
    
    return 0;
}

/*******************************************************************************
 * Driver definition
 ******************************************************************************/
/* Note: Cannot use const because isRegistered/fsNode fields need to be modified at runtime */
static DrvDevice_t w25qxx_driver = {
    .name = "w25qxx",
    .bus = DRV_BUS_SPI,
    .init = w25qxx_drv_init,
    .deinit = w25qxx_drv_deinit,
    .open = w25qxx_drv_open,
    .close = w25qxx_drv_close,
    .read = w25qxx_drv_read,
    .write = w25qxx_drv_write,
    .ioctl = w25qxx_drv_ioctl,
    .params = w25qxx_params,
    .privData = &g_w25qxx_priv,
};

/*******************************************************************************
 * Driver registration function
 ******************************************************************************/
int W25qxx_DrvRegister(void)
{
    return DrvDevice_Register(&w25qxx_driver);
}
