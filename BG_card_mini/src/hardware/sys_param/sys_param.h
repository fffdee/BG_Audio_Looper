/**
 * sys_param.h - System Parameter Configuration Storage Module
 *
 * Features:
 *   - Use SPI Flash to store system parameters
 *   - Read and write parameters with CRC check
 *   - Support default parameters loading
 *   - Parameter module management
 *
 * Usage Methods:
 *   1. Initialize system parameters: SysParam_Init()
 *   2. Get parameter: SysParam_Get()->audio.volume
 *   3. Set parameter: SysParam_Get()->audio.volume = 80;
 *   4. Save parameters: SysParam_Save() or shell command "param -s"
 * 
 * Flash API (SDK Provided):
 *   - SpiFlashRead(addr, buf, len, timeout)
 *   - SpiFlashWrite(addr, buf, len, timeout)
 *   - SpiFlashErase(SECTOR_ERASE, sector_num, wait)
 *   - SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3)
 */

#ifndef __SYS_PARAM_H__
#define __SYS_PARAM_H__

#include <stdint.h>
#include <stdbool.h>
#include "param_def.h"
#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Parameter Definitions
 *===========================================================================*/

typedef enum{

	NORMAL_BOOT = 0,
	CHARGE_BOOT,
	BT_REBOOT,
	BOOT_STATUS_MAX,

}SYS_BOOT_STATUS;


/* System parameters structure */
typedef struct __attribute__((packed)) {
	uint8_t current_boot_status;
    uint8_t  boot_count;
} SysParam_System_t;

/* Audio parameters structure */
typedef struct __attribute__((packed)) {
    uint8_t  guitar_volume;     /* Guitar volume 0-100 */
    uint8_t  mic_volume;        /* Mic volume 0-100 */
    uint8_t  output_volume;
} SysParam_Volume_t;

/* Looper parameters structure */
typedef struct __attribute__((packed)) {
    uint8_t  loop_count;        /* Loop count 1-4 */
    uint8_t  overdub_mode;      /* Overdub mode 0:Off 1:On */
    uint8_t  quantize;          /* Quantize 0:Off 1:On */
    uint8_t  click_volume;      /* Click track volume 0-100 */
    uint16_t tempo;             /* Default BPM 40-240 */
    uint8_t  time_signature;    /* Time signature 0:4/4 1:3/4 2:6/8 */
    uint8_t  fade_time;         /* Fade in/out time (10ms units) */
    uint32_t max_loop_time;     /* Max loop time (ms) */
} SysParam_Looper_t;

/* Bluetooth parameters structure */
typedef struct __attribute__((packed)) {
    uint8_t  enabled;           /* Bluetooth enabled */
    uint8_t  discoverable;      /* Discoverable mode */
    uint8_t  auto_connect;      /* Auto connect to last device */
    uint8_t  a2dp_volume;       /* A2DP volume 0-100 */
    char     device_name[16];   /* Device name */
    uint8_t  paired_addr[6];    /* Paired device address */
} SysParam_Bluetooth_t;


/* LCD parameters structure */
typedef struct __attribute__((packed)) {

    uint8_t  contrast;          /* Contrast 0-100 */
    uint8_t  color_scheme;      /* Color scheme 0:Default 1:Inverted 2:Grayscale */
    uint8_t  screen_saver;      /* Screen saver timeout (0 = Off) */
    uint16_t bg_color;          /* Background color RGB565 */

} SysParam_LCD_t;


/* User data structure (for custom parameters) */
typedef struct __attribute__((packed)) {
    uint8_t  data[32];          /* User data */
} SysParam_User_t;

#define BG_PARAM_CHAIN_MAX      2   // 支持两条参数链
#define BG_PARAM_CHAIN_NAME_LEN 16  // 链名称长度
#define MAX_NODES 8

typedef struct {

    char name[BG_PARAM_CHAIN_NAME_LEN]; // 效果节点名称
    uint8_t enabled;
    uint16_t id;
    
}BG_EffectNode_t;
typedef struct {
    char name[BG_PARAM_CHAIN_NAME_LEN]; // 链名称，如 "ChainA"、"ChainB"
    uint8_t enabled;                    // 是否启用
    BG_EffectNode_t nodes[MAX_NODES];   // 节点ID数组，最多支持8个节点
    uint8_t node_count;
} BG_ParamChain_t;

typedef struct {
    BG_ParamChain_t chains[BG_PARAM_CHAIN_MAX]; // 两条链
    uint8_t active_chain;                       // 当前激活链索引（0或1）
} BG_ParamChainManager_t;

typedef struct __attribute__((packed)) {
    
    uint8_t  contrast;          /* Contrast 0-100 */
    uint8_t  color_scheme;      /* Color scheme 0:Default 1:Inverted 2:Grayscale */
    uint8_t  screen_saver;      /* Screen saver timeout (0 = Off) */
    uint16_t bg_color;          /* Background color RGB565 */

} SysParam_Reverb_t;

typedef struct __attribute__((packed)) {
    
    uint8_t  contrast;          /* Contrast 0-100 */
    uint8_t  color_scheme;      /* Color scheme 0:Default 1:Inverted 2:Grayscale */
    uint8_t  screen_saver;      /* Screen saver timeout (0 = Off) */
    uint16_t bg_color;          /* Background color RGB565 */

} SysParam_DRC_t;


/*===========================================================================
 * Full parameter structure
 *===========================================================================*/

typedef struct __attribute__((packed)) {
    /* Header */
    uint32_t magic;             /* Magic number for validation */
    uint16_t version;           /* Parameter structure version */
    uint16_t size;              /* Size of the parameter structure */
    uint32_t crc32;             /* CRC32 checksum */
    uint32_t write_count;       /* Write access count */
    /* Parameter modules */
    SysParam_System_t    system;      /* System parameters */
    SysParam_Volume_t    volume;       /* Audio parameters */
    BG_ParamChainManager_t chain_manager;/* 参数链管理器 */
    SysParam_Looper_t    looper;      /* Looper parameters */
    SysParam_Bluetooth_t bluetooth;   /* Bluetooth parameters */
    SysParam_LCD_t       lcd;         /* LCD parameters */
    SysParam_User_t      user;        /* User parameters */

} SysParam_t;

/*===========================================================================
 * Status codes
 *===========================================================================*/

typedef enum {
    SYSPARAM_OK = 0,            /* Success */
    SYSPARAM_ERR_FLASH,         /* Flash operation error */
    SYSPARAM_ERR_CRC,           /* CRC check failed */
    SYSPARAM_ERR_VERSION,       /* Version mismatch */
    SYSPARAM_ERR_MAGIC,         /* Magic number mismatch */
    SYSPARAM_ERR_NOT_INIT,      /* Not initialized */
    SYSPARAM_ERR_PARAM,         /* Parameter error */
} SysParam_Status_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize system parameters
 *        This function must be called before any other parameter operations
 *        It will read parameters from Flash and initialize the SysParam structure
 *        If the read operation fails, default parameters will be loaded
 * @return SYSPARAM_OK on success
 */
SysParam_Status_t SysParam_Init(void);

/**
 * @brief Save parameters to Flash
 * @return SYSPARAM_OK on success
 */
SysParam_Status_t SysParam_Save(void);

/**
 * @brief Get pointer to the parameter structure
 *        This function returns a pointer to the in-memory parameter structure
 *        which is updated on every read/write operation
 * @return Pointer to SysParam_t structure
 */
SysParam_t* SysParam_Get(void);

/**
 * @brief Load default parameters
 *        This function loads default parameters from Flash and updates the in-memory structure
 *        It is called automatically if the parameter read operation fails
 * @return SYSPARAM_OK on success
 */
SysParam_Status_t SysParam_LoadDefault(void);

/**
 * @brief Check if parameters have been modified
 *        This function checks if the parameter values in Flash are different from the default values
 * @return true if modified, false if not
 */
bool SysParam_IsModified(void);

/**
 * @brief Get the write access count
 * @return Write access count
 */
uint32_t SysParam_GetWriteCount(void);

/**
 * @brief Print the current parameter values
 */
void SysParam_Print(void);

/**
 * @brief Print a specific module's parameters
 * @param module Module name: "system", "audio", "looper", "bt", "encoder", "lcd"
 */
void SysParam_PrintModule(const char *module);

/*===========================================================================
 * Quick access to module parameters
 *===========================================================================*/

#define SYSPARAM_SYSTEM()       (&SysParam_Get()->system)
#define SYSPARAM_AUDIO()        (&SysParam_Get()->audio)
#define SYSPARAM_LOOPER()       (&SysParam_Get()->looper)
#define SYSPARAM_BLUETOOTH()    (&SysParam_Get()->bluetooth)
#define SYSPARAM_ENCODER()      (&SysParam_Get()->encoder)
#define SYSPARAM_LCD()          (&SysParam_Get()->lcd)
#define SYSPARAM_USER()         (&SysParam_Get()->user)

/* Quick read/write examples */
/* Read: uint8_t vol = SYSPARAM_AUDIO()->master_volume; */
/* Write: SYSPARAM_AUDIO()->master_volume = 80; SysParam_Save(); */



extern SysParam_t g_sys_param;
#ifdef __cplusplus
}
#endif

#endif /* __SYS_PARAM_H__ */
