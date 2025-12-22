/**
 * flash_nor_w25qxx.h - W25Qxx series NOR Flash driver
 *
 * Supported models: W25Q32, W25Q64, W25Q128, W25Q256, etc.
 */

#ifndef __FLASH_NOR_W25QXX_H__
#define __FLASH_NOR_W25QXX_H__

#include "flash_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * W25Qxx Command Set
 *===========================================================================*/

/* Basic commands */
#define W25QXX_CMD_WRITE_ENABLE      0x06
#define W25QXX_CMD_WRITE_DISABLE     0x04
#define W25QXX_CMD_READ_STATUS_REG1  0x05
#define W25QXX_CMD_READ_STATUS_REG2  0x35
#define W25QXX_CMD_WRITE_STATUS_REG  0x01
#define W25QXX_CMD_READ_DATA         0x03
#define W25QXX_CMD_FAST_READ         0x0B
#define W25QXX_CMD_PAGE_PROGRAM      0x02
#define W25QXX_CMD_SECTOR_ERASE      0x20    /* 4KB */
#define W25QXX_CMD_BLOCK_ERASE_32K   0x52    /* 32KB */
#define W25QXX_CMD_BLOCK_ERASE_64K   0xD8    /* 64KB */
#define W25QXX_CMD_CHIP_ERASE        0xC7
#define W25QXX_CMD_POWER_DOWN        0xB9
#define W25QXX_CMD_RELEASE_PD        0xAB
#define W25QXX_CMD_READ_JEDEC_ID     0x9F
#define W25QXX_CMD_READ_UNIQUE_ID    0x4B

/* Status register bits */
#define W25QXX_SR1_BUSY              0x01
#define W25QXX_SR1_WEL               0x02
#define W25QXX_SR1_BP0               0x04
#define W25QXX_SR1_BP1               0x08
#define W25QXX_SR1_BP2               0x10
#define W25QXX_SR1_TB                0x20
#define W25QXX_SR1_SEC               0x40
#define W25QXX_SR1_SRP               0x80

/* Manufacturer ID */
#define W25QXX_MFG_WINBOND           0xEF

/* Device types */
#define W25QXX_DEV_Q32               0x16    /* W25Q32: 4MB */
#define W25QXX_DEV_Q64               0x17    /* W25Q64: 8MB */
#define W25QXX_DEV_Q128              0x18    /* W25Q128: 16MB */
#define W25QXX_DEV_Q256              0x19    /* W25Q256: 32MB */

/* Specification parameters */
#define W25QXX_PAGE_SIZE             256
#define W25QXX_SECTOR_SIZE           4096
#define W25QXX_BLOCK_SIZE_32K        (32 * 1024)
#define W25QXX_BLOCK_SIZE_64K        (64 * 1024)

/* Timeout settings (ms) */
#define W25QXX_TIMEOUT_WRITE_PAGE    5
#define W25QXX_TIMEOUT_ERASE_SECTOR  100
#define W25QXX_TIMEOUT_ERASE_BLOCK   400
#define W25QXX_TIMEOUT_ERASE_CHIP    100000

/*===========================================================================
 * W25Qxx Driver Interface
 *===========================================================================*/

/**
 * @brief Create W25Qxx device instance
 * @param name      Device name
 * @param cs_select CS select function
 * @param cs_deselect CS deselect function
 * @param cs_init   CS initialization function (optional)
 * @return Device pointer, NULL on failure
 */
FlashDevice_t* W25Qxx_Create(const char *name,
                             void (*cs_select)(void),
                             void (*cs_deselect)(void),
                             void (*cs_init)(void));

/**
 * @brief Destroy W25Qxx device instance
 * @param dev Device pointer
 */
void W25Qxx_Destroy(FlashDevice_t *dev);

/**
 * @brief Get W25Qxx driver operation table
 * @return Operation table pointer
 */
const FlashOps_t* W25Qxx_GetOps(void);

/*===========================================================================
 * IOCTL Commands
 *===========================================================================*/

#define W25QXX_IOCTL_POWER_DOWN      0x01
#define W25QXX_IOCTL_RELEASE_PD      0x02
#define W25QXX_IOCTL_GET_UNIQUE_ID   0x03
#define W25QXX_IOCTL_WRITE_PROTECT   0x04
#define W25QXX_IOCTL_WRITE_UNPROTECT 0x05

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_NOR_W25QXX_H__ */
