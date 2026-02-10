/**
 *****************************************************************************
 * @file     drv_device.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    Driver device registration framework - Linux driver model
 *****************************************************************************
 * @attention
 *
 * This module implements unified registration management for hardware devices:
 * 1. Driver operation layer: Define standard driver interfaces (init/open/close/read/write/ioctl)
 * 2. Device registration: Register drivers to device file system
 * 3. Parameter auto-registration: Automatically create parameter nodes based on parameter definitions
 * 4. Bus type classification: SPI/I2C/I2S/SDIO
 *
 * Usage example:
 *   // 1. Define device parameters
 *   static const FsParamDef_t st7735_params[] = {
 *       FS_PARAM_DEF("name",   "Driver name", get_name, NULL),
 *       FS_PARAM_DEF("width",  "LCD width",  get_width, set_width),
 *       FS_PARAM_DEF("height", "LCD height",  get_height, set_height),
 *       FS_PARAM_END()
 *   };
 *
 *   // 2. Define driver structure
 *   static const DrvDevice_t st7735_drv = {
 *       .name = "st7735",
 *       .bus = DRV_BUS_SPI,
 *       .init = st7735_drv_init,
 *       .params = st7735_params,
 *   };
 *
 *   // 3. Register driver
 *   DrvDevice_Register(&st7735_drv);
 *
 *****************************************************************************
 */

#ifndef __DRV_DEVICE_H__
#define __DRV_DEVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"
#include "drv_fs.h"

/*******************************************************************************
 * Bus type definitions
 ******************************************************************************/
typedef enum {
    DRV_BUS_SPI = 0,        /* SPI bus */
    DRV_BUS_I2C,            /* I2C bus */
    DRV_BUS_I2S,            /* I2S bus */
    DRV_BUS_SDIO,           /* SDIO bus */
    DRV_BUS_GPIO,           /* GPIO direct control */
    DRV_BUS_UART,           /* UART bus */
    DRV_BUS_POWER,          /* Power management bus */
    DRV_BUS_USB,            /* USB bus */
    DRV_BUS_MAX
} DrvBusType_t;

/*******************************************************************************
 * Driver operation interface type definitions
 ******************************************************************************/
/**
 * @brief  Driver initialization
 * @param  priv: Device private data
 * @return 0 success, others failure
 */
typedef int (*DrvInit_t)(void *priv);

/**
 * @brief  Driver deinitialization
 * @param  priv: Device private data
 * @return 0 success
 */
typedef int (*DrvDeinit_t)(void *priv);

/**
 * @brief  Open device
 * @param  priv: Device private data
 * @return 0 success
 */
typedef int (*DrvOpen_t)(void *priv);

/**
 * @brief  Close device
 * @param  priv: Device private data
 * @return 0 success
 */
typedef int (*DrvClose_t)(void *priv);

/**
 * @brief  Read device data
 * @param  priv: Device private data
 * @param  buf: Data buffer
 * @param  len: Length
 * @return Actual read length, -1 error
 */
typedef int (*DrvRead_t)(void *priv, uint8_t *buf, uint32_t len);

/**
 * @brief  Write device data
 * @param  priv: Device private data
 * @param  buf: Data buffer
 * @param  len: Length
 * @return Actual write length, -1 error
 */
typedef int (*DrvWrite_t)(void *priv, const uint8_t *buf, uint32_t len);

/**
 * @brief  Device control
 * @param  priv: Device private data
 * @param  cmd: Control command
 * @param  arg: Parameter
 * @return 0 success, others failure
 */
typedef int (*DrvIoctl_t)(void *priv, uint32_t cmd, void *arg);

/*******************************************************************************
 * Driver device structure
 ******************************************************************************/
typedef struct DrvDevice {
    /* Basic information */
    const char         *name;           /* Device name */
    const char         *desc;           /* Device description */
    DrvBusType_t        bus;            /* Bus type */
    
    /* Driver operation interfaces */
    DrvInit_t           init;           /* Initialization function */
    DrvDeinit_t         deinit;         /* Deinitialization function */
    DrvOpen_t           open;           /* Open device */
    DrvClose_t          close;          /* Close device */
    DrvRead_t           read;           /* Read data */
    DrvWrite_t          write;          /* Write data */
    DrvIoctl_t          ioctl;          /* Device control */
    
    /* Parameter definition list */
    const FsParamDef_t *params;         /* Parameter array, NULL terminated */
    
    /* Private data */
    void               *privData;       /* Device private data */
    
    /* Runtime status (managed by framework) */
    FsNode_t           *fsNode;         /* File system node */
    bool                isRegistered;   /* Whether registered */
    bool                isOpened;       /* Whether opened */
} DrvDevice_t;

/*******************************************************************************
 * Driver registration information (internal use)
 ******************************************************************************/
#define DRV_DEVICE_MAX      8          /* Maximum registered device count */

/*******************************************************************************
 * Public API
 ******************************************************************************/

/**
 * @brief  Initialize device management framework
 * @return 0 success
 * @note   Will automatically call DrvFs_Init()
 */
int DrvDevice_Init(void);

/**
 * @brief  Register driver device
 * @param  dev: Driver device structure pointer
 * @return 0 success, others failure
 * @note   Will automatically create device node and parameter nodes in corresponding bus directory
 */
int DrvDevice_Register(DrvDevice_t *dev);

/**
 * @brief  Unregister driver device
 * @param  dev: Driver device structure pointer
 * @return 0 success
 */
int DrvDevice_Unregister(DrvDevice_t *dev);

/**
 * @brief  Find device by name
 * @param  name: Device name
 * @return Device pointer, NULL if not found
 */
DrvDevice_t* DrvDevice_Find(const char *name);

/**
 * @brief  Find device by path
 * @param  path: Device path, e.g. "/driver/spi/st7735"
 * @return Device pointer, NULL if not found
 */
DrvDevice_t* DrvDevice_FindByPath(const char *path);

/**
 * @brief  Get directory node corresponding to bus type
 * @param  bus: Bus type
 * @return Directory node pointer
 */
FsNode_t* DrvDevice_GetBusDir(DrvBusType_t bus);

/**
 * @brief  Get bus type name
 * @param  bus: Bus type
 * @return Name string
 */
const char* DrvDevice_GetBusName(DrvBusType_t bus);

/**
 * @brief  List all registered devices
 * @param  callback: Callback function
 * @param  userData: User data
 */
typedef void (*DrvDeviceListCallback_t)(DrvDevice_t *dev, void *userData);
void DrvDevice_List(DrvDeviceListCallback_t callback, void *userData);

/**
 * @brief  Get registered device count
 * @return Device count
 */
int DrvDevice_GetCount(void);
/**
 * @brief  Get device list
 * @param  count: Output device count
 * @return Device pointer array
 */
DrvDevice_t** DrvDevice_GetList(int *count);
/*******************************************************************************
 * Convenience definitions
 ******************************************************************************/

/* Define device driver */
#define DRV_DEVICE_DEF(n, d, b, i) \
    { \
        .name = n, \
        .desc = d, \
        .bus = b, \
        .init = i, \
        .deinit = NULL, \
        .open = NULL, \
        .close = NULL, \
        .read = NULL, \
        .write = NULL, \
        .ioctl = NULL, \
        .params = NULL, \
        .privData = NULL, \
        .fsNode = NULL, \
        .isRegistered = FALSE, \
        .isOpened = FALSE \
    }

/* Simplified parameter definitions */
#define DRV_PARAM_RO(n, d, g)       FS_PARAM_DEF(n, d, g, NULL)
#define DRV_PARAM_RW(n, d, g, s)    FS_PARAM_DEF(n, d, g, s)

#ifdef __cplusplus
}
#endif

#endif /* __DRV_DEVICE_H__ */
