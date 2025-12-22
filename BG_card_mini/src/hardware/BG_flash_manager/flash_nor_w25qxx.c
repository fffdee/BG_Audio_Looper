/**
 * flash_nor_w25qxx.c - W25Qxx series NOR Flash driver implementation
 *
 * Uses hardware SPI (SPIM) + DMA to drive external Flash
 * Refer to bg_flash_manager.c implementation in BG_card_RTOS project
 */

#include "flash_nor_w25qxx.h"
#include "spim.h"
#include "spim_interface.h"
#include "dma.h"
#include "gpio.h"
#include "debug.h"
#include "rtos_api.h"
#include <string.h>
#include <stdlib.h>

/*===========================================================================
 * Internal Macro Definitions
 *===========================================================================*/

#define W25QXX_DEBUG    1

#if W25QXX_DEBUG
    #define W25QXX_LOG(fmt, ...)  DBG("[W25Qxx] " fmt, ##__VA_ARGS__)
#else
    #define W25QXX_LOG(...)
#endif

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

static FlashStatus_t W25Qxx_Init(FlashDevice_t *dev);
static FlashStatus_t W25Qxx_DeInit(FlashDevice_t *dev);
static FlashStatus_t W25Qxx_Read(FlashDevice_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);
static FlashStatus_t W25Qxx_Write(FlashDevice_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len);
static FlashStatus_t W25Qxx_EraseSector(FlashDevice_t *dev, uint32_t addr);
static FlashStatus_t W25Qxx_EraseBlock(FlashDevice_t *dev, uint32_t addr);
static FlashStatus_t W25Qxx_EraseChip(FlashDevice_t *dev);
static FlashStatus_t W25Qxx_GetStatus(FlashDevice_t *dev, uint8_t *status);
static FlashStatus_t W25Qxx_WaitReady(FlashDevice_t *dev, uint32_t timeout_ms);
static FlashStatus_t W25Qxx_ReadID(FlashDevice_t *dev);
static FlashStatus_t W25Qxx_GetInfo(FlashDevice_t *dev, FlashDevInfo_t *info);

/*===========================================================================
 * Driver Operation Table
 *===========================================================================*/

static const FlashOps_t g_w25qxx_ops = {
    .init         = W25Qxx_Init,
    .deinit       = W25Qxx_DeInit,
    .read         = W25Qxx_Read,
    .write        = W25Qxx_Write,
    .erase_sector = W25Qxx_EraseSector,
    .erase_block  = W25Qxx_EraseBlock,
    .erase_chip   = W25Qxx_EraseChip,
    .get_status   = W25Qxx_GetStatus,
    .wait_ready   = W25Qxx_WaitReady,
    .read_id      = W25Qxx_ReadID,
    .get_info     = W25Qxx_GetInfo
};

const FlashOps_t* W25Qxx_GetOps(void)
{
    return &g_w25qxx_ops;
}

/*===========================================================================
 * Low-level SPI Operations (Using SPIM DMA)
 *===========================================================================*/

/**
 * @brief SPI DMA send single byte
 */
static void spi_write_byte(uint8_t data)
{
    SPIM_DMA_Send_Start(&data, 1);
    while (!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));
}

/**
 * @brief SPI DMA receive single byte
 */
static uint8_t spi_read_byte(void)
{
    uint8_t data;
    SPIM_DMA_Recv_Start(&data, 1);
    while (!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));
    return data;
}

/**
 * @brief SPI DMA send multiple bytes
 */
static void spi_write(uint8_t *data, uint16_t size)
{
    SPIM_DMA_Send_Start(data, size);
    while (!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_TX));
}

/**
 * @brief SPI DMA receive multiple bytes
 */
static void spi_read(uint8_t *data, uint16_t size)
{
    SPIM_DMA_Recv_Start(data, size);
    while (!SPIM_DMA_HalfDone(PERIPHERAL_ID_SPIM_RX));
}

/**
 * @brief Send command
 */
static void w25qxx_send_cmd(FlashDevice_t *dev, uint8_t cmd)
{
    dev->cs.select();
    spi_write_byte(cmd);
    dev->cs.deselect();
}

/**
 * @brief Send command and read data
 */
static void w25qxx_cmd_read(FlashDevice_t *dev, uint8_t cmd, uint8_t *buf, uint32_t len)
{
    uint32_t i;
    
    dev->cs.select();
    spi_write_byte(cmd);
    for (i = 0; i < len; i++) {
        buf[i] = spi_read_byte();
    }
    dev->cs.deselect();
}

/**
 * @brief Send command + address
 */
static void w25qxx_cmd_addr(FlashDevice_t *dev, uint8_t cmd, uint32_t addr)
{
    spi_write_byte(cmd);
    spi_write_byte((addr >> 16) & 0xFF);
    spi_write_byte((addr >> 8) & 0xFF);
    spi_write_byte(addr & 0xFF);
}

/**
 * @brief Write enable
 */
static void w25qxx_write_enable(FlashDevice_t *dev)
{
    w25qxx_send_cmd(dev, W25QXX_CMD_WRITE_ENABLE);
}

/*===========================================================================
 * Driver Implementation
 *===========================================================================*/

static FlashStatus_t W25Qxx_Init(FlashDevice_t *dev)
{
    uint8_t id[3];
    uint32_t total_size;
    
    if (!dev) {
        return FLASH_ERR_PARAM;
    }
    
    /* Initialize CS pin */
    if (dev->cs.init) {
        dev->cs.init();
    }
    if (dev->cs.deselect) {
        dev->cs.deselect();
    }
    
    /* Note: SPIM has been configured during system initialization, only CS pin needs to be initialized here */
    
    /* Read JEDEC ID */
    w25qxx_cmd_read(dev, W25QXX_CMD_READ_JEDEC_ID, id, 3);
    
    dev->info.mfg_id   = id[0];
    dev->info.mem_type = id[1];
    dev->info.dev_id   = id[2];
    
    /* Verify manufacturer ID */
    if (dev->info.mfg_id != W25QXX_MFG_WINBOND) {
        W25QXX_LOG("Unknown manufacturer: 0x%02X\n", dev->info.mfg_id);
        /* Might be a compatible chip, continue trying */
    }
    
    /* Determine capacity based on device ID */
    switch (dev->info.dev_id) {
        case W25QXX_DEV_Q32:  total_size = 4 * 1024 * 1024;  break;
        case W25QXX_DEV_Q64:  total_size = 8 * 1024 * 1024;  break;
        case W25QXX_DEV_Q128: total_size = 16 * 1024 * 1024; break;
        case W25QXX_DEV_Q256: total_size = 32 * 1024 * 1024; break;
        default:
            W25QXX_LOG("Unknown device ID: 0x%02X, assuming 8MB\n", dev->info.dev_id);
            total_size = 8 * 1024 * 1024;
            break;
    }
    
    dev->info.page_size   = W25QXX_PAGE_SIZE;
    dev->info.sector_size = W25QXX_SECTOR_SIZE;
    dev->info.block_size  = W25QXX_BLOCK_SIZE_64K;
    dev->info.total_size  = total_size;
    
    dev->initialized = true;
    
    W25QXX_LOG("Init OK: %s - MfgID=0x%02X, MemType=0x%02X, DevID=0x%02X, Size=%dMB\n",
               dev->name, id[0], id[1], id[2], total_size / (1024*1024));
    
    return FLASH_OK;
}

static FlashStatus_t W25Qxx_DeInit(FlashDevice_t *dev)
{
    if (!dev) {
        return FLASH_ERR_PARAM;
    }
    
    dev->initialized = false;
    W25QXX_LOG("DeInit: %s\n", dev->name);
    
    return FLASH_OK;
}

static FlashStatus_t W25Qxx_Read(FlashDevice_t *dev, uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (!dev || !buf) {
        return FLASH_ERR_PARAM;
    }
    
    if (!dev->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    
    if (addr + len > dev->info.total_size) {
        return FLASH_ERR_PARAM;
    }
    
    /* Wait for ready */
    FlashStatus_t ret = W25Qxx_WaitReady(dev, 100);
    if (ret != FLASH_OK) {
        return ret;
    }
    
    /* Use fast read command */
    dev->cs.select();
    spi_write_byte(W25QXX_CMD_FAST_READ);
    spi_write_byte((addr >> 16) & 0xFF);
    spi_write_byte((addr >> 8) & 0xFF);
    spi_write_byte(addr & 0xFF);
    spi_write_byte(0xFF);  /* Dummy byte */
    
    /* Use DMA for bulk read */
    spi_read(buf, (uint16_t)len);
    
    dev->cs.deselect();
    
    return FLASH_OK;
}

static FlashStatus_t W25Qxx_Write(FlashDevice_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t page_offset;
    uint32_t page_remain;
    uint32_t write_len;
    const uint8_t *p = buf;
    FlashStatus_t ret;
    
    if (!dev || !buf) {
        return FLASH_ERR_PARAM;
    }
    
    if (!dev->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    
    if (addr + len > dev->info.total_size) {
        return FLASH_ERR_PARAM;
    }
    
    while (len > 0) {
        /* Calculate current page offset and remaining space */
        page_offset = addr & (W25QXX_PAGE_SIZE - 1);
        page_remain = W25QXX_PAGE_SIZE - page_offset;
        write_len = (len < page_remain) ? len : page_remain;
        
        /* Wait for ready */
        ret = W25Qxx_WaitReady(dev, 100);
        if (ret != FLASH_OK) {
            return ret;
        }
        
        /* Write enable */
        w25qxx_write_enable(dev);
        
        /* Page programming */
        dev->cs.select();
        w25qxx_cmd_addr(dev, W25QXX_CMD_PAGE_PROGRAM, addr);
        
        /* Use DMA for bulk write */
        spi_write((uint8_t*)p, (uint16_t)write_len);
        
        dev->cs.deselect();
        
        /* Wait for write completion */
        ret = W25Qxx_WaitReady(dev, W25QXX_TIMEOUT_WRITE_PAGE);
        if (ret != FLASH_OK) {
            return FLASH_ERR_WRITE;
        }
        
        addr += write_len;
        p    += write_len;
        len  -= write_len;
    }
    
    return FLASH_OK;
}

static FlashStatus_t W25Qxx_EraseSector(FlashDevice_t *dev, uint32_t addr)
{
    FlashStatus_t ret;
    
    if (!dev) {
        return FLASH_ERR_PARAM;
    }
    
    if (!dev->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    
    /* Align address to sector boundary */
    addr &= ~(W25QXX_SECTOR_SIZE - 1);
    
    if (addr >= dev->info.total_size) {
        return FLASH_ERR_PARAM;
    }
    
    /* Wait for ready */
    ret = W25Qxx_WaitReady(dev, 100);
    if (ret != FLASH_OK) {
        return ret;
    }
    
    /* Write enable */
    w25qxx_write_enable(dev);
    
    /* Sector erase */
    dev->cs.select();
    w25qxx_cmd_addr(dev, W25QXX_CMD_SECTOR_ERASE, addr);
    dev->cs.deselect();
    
    /* Wait for erase completion */
    ret = W25Qxx_WaitReady(dev, W25QXX_TIMEOUT_ERASE_SECTOR);
    if (ret != FLASH_OK) {
        return FLASH_ERR_ERASE;
    }
    
    return FLASH_OK;
}

static FlashStatus_t W25Qxx_EraseBlock(FlashDevice_t *dev, uint32_t addr)
{
    FlashStatus_t ret;
    
    if (!dev) {
        return FLASH_ERR_PARAM;
    }
    
    if (!dev->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    
    /* Align address to block boundary */
    addr &= ~(W25QXX_BLOCK_SIZE_64K - 1);
    
    if (addr >= dev->info.total_size) {
        return FLASH_ERR_PARAM;
    }
    
    /* Wait for ready */
    ret = W25Qxx_WaitReady(dev, 100);
    if (ret != FLASH_OK) {
        return ret;
    }
    
    /* Write enable */
    w25qxx_write_enable(dev);
    
    /* Block erase */
    dev->cs.select();
    w25qxx_cmd_addr(dev, W25QXX_CMD_BLOCK_ERASE_64K, addr);
    dev->cs.deselect();
    
    /* Wait for erase completion */
    ret = W25Qxx_WaitReady(dev, W25QXX_TIMEOUT_ERASE_BLOCK);
    if (ret != FLASH_OK) {
        return FLASH_ERR_ERASE;
    }
    
    return FLASH_OK;
}

static FlashStatus_t W25Qxx_EraseChip(FlashDevice_t *dev)
{
    FlashStatus_t ret;
    
    if (!dev) {
        return FLASH_ERR_PARAM;
    }
    
    if (!dev->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    
    /* Wait for ready */
    ret = W25Qxx_WaitReady(dev, 100);
    if (ret != FLASH_OK) {
        return ret;
    }
    
    /* Write enable */
    w25qxx_write_enable(dev);
    
    /* Chip erase */
    w25qxx_send_cmd(dev, W25QXX_CMD_CHIP_ERASE);
    
    W25QXX_LOG("Chip erase started (may take up to 100 seconds)...\n");
    
    /* Wait for erase completion */
    ret = W25Qxx_WaitReady(dev, W25QXX_TIMEOUT_ERASE_CHIP);
    if (ret != FLASH_OK) {
        return FLASH_ERR_ERASE;
    }
    
    W25QXX_LOG("Chip erase completed\n");
    return FLASH_OK;
}

static FlashStatus_t W25Qxx_GetStatus(FlashDevice_t *dev, uint8_t *status)
{
    uint8_t sr;
    
    if (!dev || !status) {
        return FLASH_ERR_PARAM;
    }
    
    w25qxx_cmd_read(dev, W25QXX_CMD_READ_STATUS_REG1, &sr, 1);
    *status = sr;
    
    return FLASH_OK;
}

static FlashStatus_t W25Qxx_WaitReady(FlashDevice_t *dev, uint32_t timeout_ms)
{
    uint8_t sr;
    uint32_t start_tick = xTaskGetTickCount();
    
    if (!dev) {
        return FLASH_ERR_PARAM;
    }
    
    do {
        w25qxx_cmd_read(dev, W25QXX_CMD_READ_STATUS_REG1, &sr, 1);
        
        if (!(sr & W25QXX_SR1_BUSY)) {
            return FLASH_OK;
        }
        
        vTaskDelay(1);  /* Yield CPU */
        
    } while ((xTaskGetTickCount() - start_tick) < (timeout_ms / portTICK_PERIOD_MS));
    
    W25QXX_LOG("Wait ready timeout!\n");
    return FLASH_ERR_TIMEOUT;
}

static FlashStatus_t W25Qxx_ReadID(FlashDevice_t *dev)
{
    uint8_t id[3];
    
    if (!dev) {
        return FLASH_ERR_PARAM;
    }
    
    /* Read JEDEC ID */
    w25qxx_cmd_read(dev, W25QXX_CMD_READ_JEDEC_ID, id, 3);
    
    dev->info.mfg_id   = id[0];
    dev->info.mem_type = id[1];
    dev->info.dev_id   = id[2];
    
    W25QXX_LOG("ReadID: MfgID=0x%02X, MemType=0x%02X, DevID=0x%02X\n",
               id[0], id[1], id[2]);
    
    return FLASH_OK;
}

static FlashStatus_t W25Qxx_GetInfo(FlashDevice_t *dev, FlashDevInfo_t *info)
{
    if (!dev || !info) {
        return FLASH_ERR_PARAM;
    }
    
    if (!dev->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    
    /* Copy device info */
    memcpy(info, &dev->info, sizeof(FlashDevInfo_t));
    
    return FLASH_OK;
}

/*===========================================================================
 * Device Creation/Destruction
 *===========================================================================*/

FlashDevice_t* W25Qxx_Create(const char *name,
                             void (*cs_select)(void),
                             void (*cs_deselect)(void),
                             void (*cs_init)(void))
{
    FlashDevice_t *dev;
    
    if (!name || !cs_select || !cs_deselect) {
        return NULL;
    }
    
    dev = (FlashDevice_t*)pvPortMalloc(sizeof(FlashDevice_t));
    if (!dev) {
        return NULL;
    }
    
    memset(dev, 0, sizeof(FlashDevice_t));
    
    /* Set name */
    strncpy(dev->name, name, FLASH_DEV_NAME_MAX - 1);
    
    /* Set type */
    dev->type = FLASH_TYPE_NOR;
    
    /* Set operation table */
    dev->ops = &g_w25qxx_ops;
    
    /* Set CS control */
    dev->cs.select   = cs_select;
    dev->cs.deselect = cs_deselect;
    dev->cs.init     = cs_init;
    
    W25QXX_LOG("Created device: %s\n", name);
    
    return dev;
}

void W25Qxx_Destroy(FlashDevice_t *dev)
{
    if (!dev) return;
    
    if (dev->initialized && dev->ops && dev->ops->deinit) {
        dev->ops->deinit(dev);
    }
    
    W25QXX_LOG("Destroyed device: %s\n", dev->name);
    vPortFree(dev);
}
