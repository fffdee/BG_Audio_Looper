/**
 * sys_param.h - System Parameter Configuration and Storage Management
 *
 * Features:
 *   - Use SPI Flash to store system parameters
 *   - Background data reading and writing
 *   - Automatic parameter loading during boot
 *   - Parameter validation and default setting
 *
 * Usage:
 *   1. Initialize system parameters: SysParam_Init()
 *   2. Get a parameter: SysParam_Get()->audio.volume
 *   3. Set a parameter: SysParam_Get()->audio.volume = 80;
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

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Version and Magic Number
 *===========================================================================*/

#define SYS_PARAM_VERSION       0x0102      /* Version number, increment for changes */
#define SYS_PARAM_MAGIC         0x50415241  /* "PARA" Magic number for validation */

/* Flash Memory Configuration
 * BP1048 Flash has 16MB, with parameters stored in 4KB sectors
 * Total sectors = 16MB / 4KB = 4096
 */
#define SYS_PARAM_SECTOR_NUM    255         /* Use last 255 sectors (after 1st sector) */
#define SYS_PARAM_FLASH_ADDR    (SYS_PARAM_SECTOR_NUM * 4096)  /* 0xFF000 */
#define SYS_PARAM_SECTOR_SIZE   4096        /* Sector size 4KB */
#define SYS_PARAM_FLASH_TIMEOUT 100         /* Flash operation timeout (ms) */

/*===========================================================================
 * Parameter Definitions
 *===========================================================================*/

typedef enum{

	NORMAL_BOOT = 0,
	CHARGE_BOOT,
	BT_REBOOT,
	BOOT_STATUS_MAX,

}SYS_BOOT_STATUS;
/* System parameters */
typedef struct {
	uint8_t boot_mode;
} SysParam_System_t;

/* Audio parameters */
typedef struct {
    uint8_t  master_volume;     /* Master volume 0-100 */
    uint8_t  mic_volume;        /* Microphone volume 0-100 */
    uint8_t  effect_type;       /* Effect type */
    uint8_t  eq_mode;           /* EQ mode 0:Flat 1:Rock 2:Pop 3:Classical 4:Jazz */
    int8_t   eq_bass;           /* Bass adjustment -12 ~ +12 dB */
    int8_t   eq_mid;            /* Mid adjustment -12 ~ +12 dB */
    int8_t   eq_treble;         /* Treble adjustment -12 ~ +12 dB */
    uint8_t  mic_echo;          /* Microphone echo 0-100 */
    uint8_t  mic_reverb;        /* Microphone reverb 0-100 */
    uint8_t  noise_gate;        /* Noise gate 0-100 */
    uint8_t  reserved[2];       /* Reserved for future use */
} SysParam_Audio_t;

/* Looper parameters */
typedef struct {
    uint8_t  loop_count;        /* Loop count 1-4 */
    uint8_t  overdub_mode;      /* Overdub mode 0:Off 1:On */
    uint8_t  quantize;          /* Quantize 0:Off 1:On */
    uint8_t  click_volume;      /* Click track volume 0-100 */
    uint16_t tempo;             /* Default BPM 40-240 */
    uint8_t  time_signature;    /* Time signature 0:4/4 1:3/4 2:6/8 */
    uint8_t  fade_time;         /* Fade time in 10ms units */
    uint32_t max_loop_time;     /* Max loop time in ms */
} SysParam_Looper_t;

/* Bluetooth parameters */
typedef struct {
    uint8_t  enabled;           /* Bluetooth enabled */
    uint8_t  discoverable;      /* Discoverable mode */
    uint8_t  auto_connect;      /* Auto connect to last device */
    uint8_t  a2dp_volume;       /* A2DP volume 0-100 */
    char     device_name[16];   /* Device name */
    uint8_t  paired_addr[6];    /* Paired device address */
    uint8_t  reserved[2];       /* Reserved for future use */
} SysParam_Bluetooth_t;

/* Encoder parameters */
typedef struct {
    uint8_t  sensitivity;       /* Sensitivity 1-10 */
    uint8_t  acceleration;      /* Acceleration 0:Off 1-5:On */
    uint8_t  direction;         /* Direction 0:Normal 1:Reverse */
    uint8_t  click_action;      /* Click action */
    uint8_t  long_press_time;   /* Long press time in 100ms units */
    uint8_t  reserved[3];       /* Reserved for future use */
} SysParam_Encoder_t;

/* LCD display parameters */
typedef struct {
    uint8_t  contrast;          /* Contrast 0-100 */
    uint8_t  color_scheme;      /* Color scheme 0:Default 1:Scheme1 2:Scheme2 */
    uint8_t  font_size;         /* Font size 0:Large 1:Medium 2:Small */
    uint8_t  screen_saver;      /* Screen saver timeout (0 = Off) */
    uint16_t bg_color;          /* Background color RGB565 */
    uint16_t fg_color;          /* Foreground color RGB565 */
} SysParam_LCD_t;



/* User custom parameter area (for widget use) */
typedef struct {
    uint8_t  data[32];          /* User data */
} SysParam_User_t;

/*===========================================================================
 * Main parameter structure
 *===========================================================================*/

typedef struct {
    /* Header information */
     uint32_t magic;             /* Magic number for validation */
//    uint16_t version;           /* Parameter version */
//    uint16_t size;              /* Structure size */
//    uint32_t crc32;             /* CRC check */
     uint32_t write_count;       /* Write count */
//
    /* Module parameters */
    SysParam_System_t    system;      /* System parameters */
//    SysParam_Audio_t     audio;       /* Audio parameters */
//    SysParam_Looper_t    looper;      /* Looper parameters */
//    SysParam_Bluetooth_t bluetooth;   /* Bluetooth parameters */
//    SysParam_Encoder_t   encoder;     /* Encoder parameters */
//    SysParam_LCD_t       lcd;         /* LCD parameters */
//    SysParam_hardware_t  hardware;
//    SysParam_User_t      user;        /* User custom parameters */
    
    
} SysParam_t;

/*===========================================================================
 * Status code definitions
 *===========================================================================*/

typedef enum {
    SYSPARAM_OK = 0,            /* Success */
    SYSPARAM_ERR_FLASH,         /* Flash operation error */
    SYSPARAM_ERR_CRC,           /* CRC check failed */
    SYSPARAM_ERR_VERSION,       /* Invalid version */
    SYSPARAM_ERR_MAGIC,         /* Invalid magic number */
    SYSPARAM_ERR_NOT_INIT,      /* Not initialized */
    SYSPARAM_ERR_PARAM,         /* Invalid parameter */
} SysParam_Status_t;

/*===========================================================================
 * API Function Declarations
 *===========================================================================*/

/**
 * @brief Initialize system parameters
 *        Reads parameters from Flash into RAM
 *        If read fails, default parameters are used
 * @return SYSPARAM_OK on success
 */
SysParam_Status_t SysParam_Init(void);

/**
 * @brief Save parameters to Flash
 * @return SYSPARAM_OK on success
 */
SysParam_Status_t SysParam_Save(void);

/**
 * @brief Get pointer to parameter structure
 *        Use this to access or modify parameters
 * @return Pointer to parameter structure
 */
SysParam_t* SysParam_Get(void);

/**
 * @brief Load default parameters
 *        Resets parameters to factory defaults and saves to Flash
 * @return SYSPARAM_OK on success
 */
SysParam_Status_t SysParam_LoadDefault(void);

/**
 * @brief Check if parameters have been modified
 *        Compares current parameters with saved parameters
 * @return true if modified, false if not
 */
bool SysParam_IsModified(void);

/**
 * @brief Get the write count
 *        Indicates how many times parameters have been written to Flash
 * @return Write count
 */
uint32_t SysParam_GetWriteCount(void);

/**
 * @brief Print parameters to debug console
 *        Displays all parameters in a human-readable format
 */
void SysParam_Print(void);

/**
 * @brief Print specific module parameters
 * @param module Module name: "system", "audio", "looper", "bt", "encoder", "lcd"
 */
void SysParam_PrintModule(const char *module);

/*===========================================================================
 * Shell Command Interface
 *===========================================================================*/

/**
 * @brief Register Shell commands for parameter management
 */
void SysParam_RegisterShellCommands(void);

/**
 * @brief Shell command handler
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success
 * 
 * Shell command format:
 *   param -s              Save current parameters
 *   param -d              Load default parameters
 *   param -p              Print current parameters
 *   param -p <module>     Print specific module parameters
 *   param -i              Print parameter information, including write count
 * 
 * Module-specific commands:
 *   audio -s              Save audio parameters
 *   audio vol <0-100>     Set master volume
 *   audio mic <0-100>     Set microphone volume
 *   looper -s             Save looper parameters
 *   looper tempo <40-240> Set BPM
 */
int SysParam_ShellCmd(int argc, char *argv[]);

/*===========================================================================
 * Inline Access Macros
 *===========================================================================*/

/* Quick access to parameter structures */
#define SYSPARAM_SYSTEM()       (&SysParam_Get()->system)
#define SYSPARAM_AUDIO()        (&SysParam_Get()->audio)
#define SYSPARAM_LOOPER()       (&SysParam_Get()->looper)
#define SYSPARAM_BLUETOOTH()    (&SysParam_Get()->bluetooth)
#define SYSPARAM_ENCODER()      (&SysParam_Get()->encoder)
#define SYSPARAM_LCD()          (&SysParam_Get()->lcd)
#define SYSPARAM_USER()         (&SysParam_Get()->user)

/* Example usage:
 * Read: uint8_t vol = SYSPARAM_AUDIO()->master_volume;
 * Write: SYSPARAM_AUDIO()->master_volume = 80; SysParam_Save();
 */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_PARAM_H__ */
