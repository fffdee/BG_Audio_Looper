#ifndef __PARAM_DEF_H__
#define __PARAM_DEF_H__

// Version and Magic
#define SYS_PARAM_VERSION       0x0102      /* Parameter structure version, changes require firmware update */
#define SYS_PARAM_MAGIC         0x50415241  /* "PARA" Magic number*/

// Flash Storage Configuration
#define SYS_PARAM_SECTOR_NUM    255         /* Using 255 sectors (after bootloader reserved) */
#define SYS_PARAM_FLASH_ADDR    (SYS_PARAM_SECTOR_NUM * 4096)  /* 0xFF000 */
#define SYS_PARAM_SECTOR_SIZE   4096        /* Sector size 4KB */
#define SYS_PARAM_FLASH_TIMEOUT 100         /* Flash operation timeout (ms) */

// Header partition address and size
#define SYS_PARAM_ADDR_HEADER      (SYS_PARAM_FLASH_ADDR)
#define SYS_PARAM_HEADER_SIZE      0x1000    /* 4KB header area */

// Module Flash Address Definitions (interval 0x1000, after header)
#define SYS_PARAM_ADDR_SYSTEM      (SYS_PARAM_ADDR_HEADER + SYS_PARAM_HEADER_SIZE + 0x0000)
#define SYS_PARAM_ADDR_AUDIO       (SYS_PARAM_ADDR_HEADER + SYS_PARAM_HEADER_SIZE + 0x1000)
#define SYS_PARAM_ADDR_LOOPER      (SYS_PARAM_ADDR_HEADER + SYS_PARAM_HEADER_SIZE + 0x2000)
#define SYS_PARAM_ADDR_BLUETOOTH   (SYS_PARAM_ADDR_HEADER + SYS_PARAM_HEADER_SIZE + 0x3000)
#define SYS_PARAM_ADDR_ENCODER     (SYS_PARAM_ADDR_HEADER + SYS_PARAM_HEADER_SIZE + 0x4000)
#define SYS_PARAM_ADDR_LCD         (SYS_PARAM_ADDR_HEADER + SYS_PARAM_HEADER_SIZE + 0x5000)
#define SYS_PARAM_ADDR_USER        (SYS_PARAM_ADDR_HEADER + SYS_PARAM_HEADER_SIZE + 0x6000)

// Each module parameter area size (4KB)
#define SYS_PARAM_MODULE_SIZE      0x1000    /* 4KB per module */




#endif /* __PARAM_DEF_H__ */
