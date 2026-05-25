/**
 * shell_cmd_param.c - Parameter Shell Command Module Implementation
 * 
 * Provides shell commands for system parameter management including:
 *   - Load/Save parameters from/to internal flash
 *   - Reset to default values
 *   - Print and display parameter values
 *   - Module-specific parameter save support
 */

#include "shell_cmd_param.h"
#include "sys_param.h"
#include "bg_shell.h"
#include "spi_flash.h"  /* Flash API for erase operation */
#include "shell_io_ble.h"  /* BLE sync response buffering */
#include <string.h>
#include <stdlib.h>

/*============================================================================
 * Private Command Handlers
 *===========================================================================*/

/**
 * @brief Load parameters from flash
 */
static int param_load(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    Shell_Print("Loading parameters from flash...\r\n");
    SysParam_Status_t status = SysParam_Init();
    
    switch (status) {
        case SYSPARAM_OK:
            Shell_Print("Parameters loaded successfully\r\n");
            break;
        case SYSPARAM_ERR_MAGIC:
            Shell_Print("Flash empty or corrupted, defaults loaded\r\n");
            break;
        case SYSPARAM_ERR_CRC:
            Shell_Print("CRC error, defaults loaded\r\n");
            break;
        case SYSPARAM_ERR_VERSION:
            Shell_Print("Version mismatch, defaults loaded\r\n");
            break;
        default:
            Shell_Printf("Load error: %d\r\n", status);
            return -1;
    }
    return 0;
}

/**
 * @brief Save parameters to flash
 */
static int param_save(int argc, char *argv[])
{
    const char *module = NULL;
    
    if (argc >= 1) {
        module = argv[0];
        Shell_Printf("Saving module [%s] to flash...\r\n", module);
    } else {
        Shell_Print("Saving all parameters to flash...\r\n");
    }
    
    SysParam_Status_t status;
    if (module) {
        status = SysParam_SaveModule(module);
    } else {
        status = SysParam_Save();
    }
    
    if (status == SYSPARAM_OK) {
        Shell_Printf("Parameters saved, write count: %lu\r\n", 
                    (unsigned long)SysParam_GetWriteCount());
        return 0;
    }
    
    Shell_Printf("Save failed: %d\r\n", status);
    return -1;
}

/**
 * @brief Reset to default parameters
 */
static int param_default(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    Shell_Print("Resetting to default parameters...\r\n");
    SysParam_LoadDefault();
    Shell_Print("Defaults loaded (use -s to save to flash)\r\n");
    return 0;
}

/**
 * @brief Print parameters
 */
static int param_print(int argc, char *argv[])
{
    if (argc >= 1) {
        SysParam_PrintModule(argv[0]);
    } else {
        SysParam_Print();
    }
    return 0;
}

/**
 * @brief Show parameter info
 */
static int param_info(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    Shell_Print("=== Parameter System Info ===\r\n");
    Shell_Printf("Structure size: %u bytes\r\n", (unsigned int)sizeof(SysParam_t));
    Shell_Printf("Flash sector:   %d\r\n", SYS_PARAM_SECTOR_NUM);
    Shell_Printf("Flash address:  0x%08lX\r\n", (unsigned long)SYS_PARAM_FLASH_ADDR);
    Shell_Printf("Write count:    %lu\r\n", (unsigned long)SysParam_GetWriteCount());
    Shell_Printf("Modified:       %s\r\n", SysParam_IsModified() ? "Yes" : "No");
    Shell_Print("\r\nModules: system, audio, bt, lcd\r\n");
    return 0;
}

/**
 * @brief Erase parameter flash sector (dangerous!)
 */
static int param_erase(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    Shell_Print("WARNING: This will erase all saved parameters!\r\n");
    Shell_Print("Erasing parameter sector...\r\n");
    
    /* Unlock and erase */
    SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3);
    SpiFlashErase(SECTOR_ERASE, SYS_PARAM_SECTOR_NUM, 1);
    
    Shell_Print("Parameter sector erased\r\n");
    Shell_Print("Reloading defaults...\r\n");
    SysParam_LoadDefault();
    return 0;
}

/**
 * @brief Test flash read/write
 */
static int param_test(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    Shell_Print("=== Flash Parameter Test ===\r\n");
    
    /* Save current parameters */
    Shell_Print("1. Saving current parameters...\r\n");
    if (SysParam_Save() != SYSPARAM_OK) {
        Shell_Print("   FAILED!\r\n");
        return -1;
    }
    Shell_Print("   OK\r\n");
    
    /* Reload and verify */
    Shell_Print("2. Reloading from flash...\r\n");
    
    if (SysParam_Init() != SYSPARAM_OK) {
        Shell_Print("   Load failed (may be first run)\r\n");
    } else {
        Shell_Print("   OK\r\n");
    }
    
    /* Verify magic */
    Shell_Print("3. Verifying data...\r\n");
    SysParam_t *loaded = SysParam_Get();
    if (loaded->magic == SYS_PARAM_MAGIC) {
        Shell_Printf("   Magic: OK (0x%08lX)\r\n", (unsigned long)loaded->magic);
    } else {
        Shell_Printf("   Magic: FAIL (0x%08lX)\r\n", (unsigned long)loaded->magic);
    }
    
    Shell_Printf("   WriteCount: %lu\r\n", (unsigned long)loaded->write_count);
    Shell_Print("=== Test Complete ===\r\n");
    
    return 0;
}

/**
 * @brief Query parameters in binary format for APP (compact BLE transmission)
 * Binary protocol: [0xAA][0x55][type][length][data...]
 * Types: 0x01=volume, 0x02=system, 0x05=lcd,
 *        0x10=effect_drc, 0x11=effect_reverb, 0x12=effect_eq
 */

static int param_query(int argc, char *argv[])
{
    extern SysParam_t g_sys_param;
    extern uint8_t g_is_sync_command;  /* From shell_io_ble.c */
    const char *target = (argc >= 1) ? argv[0] : "all";
    uint8_t buf[200]; // Max 200 bytes to stay under BLE 250 limit
    int idx = 0;
    
    if (strcmp(target, "all") == 0 || strcmp(target, "system") == 0) {
        // System parameters: boot_count (2), current_boot_status (2) = 4 bytes
        buf[idx++] = 0xAA; // header
        buf[idx++] = 0x55;
        buf[idx++] = 0x02; // type: system
        buf[idx++] = 4;    // length
        buf[idx++] = (uint8_t)(g_sys_param.system.boot_count & 0xFF);
        buf[idx++] = (uint8_t)((g_sys_param.system.boot_count >> 8) & 0xFF);
        buf[idx++] = (uint8_t)(g_sys_param.system.current_boot_status & 0xFF);
        buf[idx++] = (uint8_t)((g_sys_param.system.current_boot_status >> 8) & 0xFF);
        
        if (g_is_sync_command) {
            BLE_BufferSyncResponse((const char *)buf, idx);
        } else {
            Shell_WriteRaw(buf, idx);
        }
        return 0;
    }
    else if (strcmp(target, "volume") == 0) {
        // Volume parameters: mic1, mic2, guitar1, guitar2, output = 5 bytes
        buf[idx++] = 0xAA; // header
        buf[idx++] = 0x55;
        buf[idx++] = 0x01; // type: volume
        buf[idx++] = 5;    // length
        buf[idx++] = (uint8_t)g_sys_param.volume.mic1_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.mic2_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.guitar1_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.guitar2_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.output_volume;
        
        if (g_is_sync_command) {
            BLE_BufferSyncResponse((const char *)buf, idx);
        } else {
            Shell_WriteRaw(buf, idx);
        }
        return 0;
    }
    else if (strcmp(target, "lcd") == 0) {
        // LCD parameters: contrast(1), color_scheme(1), screen_saver(1), bg_color(1) = 4 bytes
        buf[idx++] = 0xAA; // header
        buf[idx++] = 0x55;
        buf[idx++] = 0x05; // type: lcd
        buf[idx++] = 4;    // length
        buf[idx++] = (uint8_t)g_sys_param.lcd.contrast;
        buf[idx++] = (uint8_t)g_sys_param.lcd.color_scheme;
        buf[idx++] = (uint8_t)g_sys_param.lcd.screen_saver;
        buf[idx++] = (uint8_t)g_sys_param.lcd.bg_color;
        
        if (g_is_sync_command) {
            BLE_BufferSyncResponse((const char *)buf, idx);
        } else {
            Shell_WriteRaw(buf, idx);
        }
        return 0;
    }
    else if (strcmp(target, "effect") == 0 || strcmp(target, "effects") == 0) {
        Shell_Print("{\"error\":\"effect_graph removed\"}");
        return 0;
    }
    else {
        Shell_Printf("{\"error\":\"Unknown target: %s\"}", target);
        return -1;
    }
    
    return 0;
}
/*============================================================================
 * Module Definition
 *===========================================================================*/

static const ShellOpt_t param_opts[] = {
    OPT("l", "load",    NULL,       "Load params from flash",       param_load),
    OPT("s", "save",    "[module]", "Save params to flash",         param_save),
    OPT("d", "default", NULL,       "Reset to default params",      param_default),
    OPT("p", "print",   "[module]", "Print params (sys/audio/looper/bt/lcd)", param_print),
    OPT("i", "info",    NULL,       "Show param system info",       param_info),
    OPT("q", "query",   "<target>", "Query params in binary format (system/volume/lcd/effect)", param_query),
    OPT("e", "erase",   NULL,       "Erase param sector (danger!)", param_erase),
    OPT("t", "test",    NULL,       "Test flash save/load",         param_test),
    OPT_END()
};

DEFINE_MODULE(param, "Parameter management", MOD_CAT_SYSTEM, param_opts);

/* Module name macro for external access */
#define PARAM_MODULE_VAR  _mod_param

/*============================================================================
 * Public Functions
 *===========================================================================*/

void ShellCmd_Param_Init(void)
{
    /* Register with shell system */
    Shell_RegisterModule(&PARAM_MODULE_VAR);
}

const ShellModule_t* ShellCmd_Param_GetModule(void)
{
    return &PARAM_MODULE_VAR;
}
