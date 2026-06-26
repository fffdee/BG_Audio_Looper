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
#include "effect_graph.h"  /* Effect graph API for node queries */
#include "shell_io_ble.h"  /* BLE sync response buffering */
#include "bg_audio_io_manager.h"  /* BG_AudioIO_SetUsbOutVolume */
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
    Shell_Print("\r\nModules: system, audio, looper, bt, lcd\r\n");
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
 * @brief Set volume parameters
 * Usage: param set bt_max <0-100>
 *        param set usb_max <0-100>
 *        param set usb_out <0-100>
 *        param set usb_mute <0|1>
 */
static int param_set(int argc, char *argv[])
{
    if (argc < 2) {
        Shell_Print("Usage: param set <key> <value>\r\n");
        Shell_Print("  bt_max   <0-100>  BT music max volume mapped by wheel\r\n");
        Shell_Print("  usb_max  <0-100>  USB music max volume mapped by wheel\r\n");
        Shell_Print("  usb_out  <0-100>  USB output (device->PC) volume\r\n");
        Shell_Print("  usb_mute <0|1>    USB output mute\r\n");
        return -1;
    }
    
    const char *key = argv[0];
    int value = atoi(argv[1]);
    SysParam_t *p = SysParam_Get();
    
    if (strcmp(key, "bt_max") == 0) {
        if (value < 0 || value > 100) {
            Shell_Print("Error: value must be 0-100\r\n");
            return -1;
        }
        p->volume.bt_max_volume = (uint8_t)value;
        Shell_Printf("BT max volume set to %d%%\r\n", value);
    }
    else if (strcmp(key, "usb_max") == 0) {
        if (value < 0 || value > 100) {
            Shell_Print("Error: value must be 0-100\r\n");
            return -1;
        }
        p->volume.usb_max_volume = (uint8_t)value;
        Shell_Printf("USB max volume set to %d%%\r\n", value);
    }
    else if (strcmp(key, "usb_out") == 0) {
        if (value < 0 || value > 100) {
            Shell_Print("Error: value must be 0-100\r\n");
            return -1;
        }
        p->volume.usb_out_volume = (uint8_t)value;
        BG_AudioIO_SetUsbOutVolume(p->volume.usb_out_volume, p->volume.usb_out_mute);
        Shell_Printf("USB output volume set to %d%%\r\n", value);
    }
    else if (strcmp(key, "usb_mute") == 0) {
        if (value < 0 || value > 1) {
            Shell_Print("Error: value must be 0 or 1\r\n");
            return -1;
        }
        p->volume.usb_out_mute = (uint8_t)value;
        BG_AudioIO_SetUsbOutVolume(p->volume.usb_out_volume, p->volume.usb_out_mute);
        Shell_Printf("USB output mute set to %s\r\n", value ? "ON" : "OFF");
    }
    else {
        Shell_Printf("Error: unknown key '%s'\r\n", key);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Query parameters in binary format for APP (compact BLE transmission)
 * Binary protocol: [0xAA][0x55][type][length][data...]
 * Types: 0x01=volume, 0x02=system, 0x03=looper, 0x04=metronome, 0x05=lcd,
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
        // Volume parameters: mic1, mic2, guitar1, guitar2, output,
        //                    bt_max, usb_max, usb_out, usb_out_mute = 9 bytes
        buf[idx++] = 0xAA; // header
        buf[idx++] = 0x55;
        buf[idx++] = 0x01; // type: volume
        buf[idx++] = 9;    // length
        buf[idx++] = (uint8_t)g_sys_param.volume.mic1_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.mic2_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.guitar1_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.guitar2_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.output_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.bt_max_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.usb_max_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.usb_out_volume;
        buf[idx++] = (uint8_t)g_sys_param.volume.usb_out_mute;
        
        if (g_is_sync_command) {
            BLE_BufferSyncResponse((const char *)buf, idx);
        } else {
            Shell_WriteRaw(buf, idx);
        }
        return 0;
    }
    else if (strcmp(target, "looper") == 0) {
        // Looper parameters: loop_count(1), overdub_mode(1), quantize(1), click_volume(1), 
        // tempo(1), time_signature(1), fade_time(2), max_loop_time(4) = 12 bytes
        buf[idx++] = 0xAA; // header
        buf[idx++] = 0x55;
        buf[idx++] = 0x03; // type: looper
        buf[idx++] = 12;   // length
        buf[idx++] = (uint8_t)g_sys_param.looper.loop_count;
        buf[idx++] = (uint8_t)g_sys_param.looper.overdub_mode;
        buf[idx++] = (uint8_t)g_sys_param.looper.quantize;
        buf[idx++] = (uint8_t)g_sys_param.looper.click_volume;
        buf[idx++] = (uint8_t)g_sys_param.looper.tempo;
        buf[idx++] = (uint8_t)g_sys_param.looper.time_signature;
        buf[idx++] = (uint8_t)(g_sys_param.looper.fade_time & 0xFF);
        buf[idx++] = (uint8_t)((g_sys_param.looper.fade_time >> 8) & 0xFF);
        buf[idx++] = (uint8_t)(g_sys_param.looper.max_loop_time & 0xFF);
        buf[idx++] = (uint8_t)((g_sys_param.looper.max_loop_time >> 8) & 0xFF);
        buf[idx++] = (uint8_t)((g_sys_param.looper.max_loop_time >> 16) & 0xFF);
        buf[idx++] = (uint8_t)((g_sys_param.looper.max_loop_time >> 24) & 0xFF);
        
        if (g_is_sync_command) {
            BLE_BufferSyncResponse((const char *)buf, idx);
        } else {
            Shell_WriteRaw(buf, idx);
        }
        return 0;
    }
    else if (strcmp(target, "metronome") == 0) {
        // Metronome parameters: enabled(1), bpm(1), beats(1), volume(1) = 4 bytes
        buf[idx++] = 0xAA; // header
        buf[idx++] = 0x55;
        buf[idx++] = 0x04; // type: metronome
        buf[idx++] = 4;    // length
        buf[idx++] = (uint8_t)(g_sys_param.looper.click_volume > 0 ? 1 : 0);
        buf[idx++] = (uint8_t)g_sys_param.looper.tempo;
        buf[idx++] = (uint8_t)(g_sys_param.looper.time_signature + 4);
        buf[idx++] = (uint8_t)g_sys_param.looper.click_volume;
        
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
        // Query effect node parameters by ID in binary format for app
        if (argc >= 2) {
            int node_id = atoi(argv[1]);
            EffectNode_t* node = EffectGraph_FindNodeById((uint8_t)node_id);
            
            if (node != NULL) {
                idx = 0;
                buf[idx++] = 0xAA; // header
                buf[idx++] = 0x55;
                
                // Type-specific binary data
                switch (node->type) {
                    case EFFECT_NODE_TYPE_EFFECT_DRC:
                        // DRC: threshold(2), ratio(2), attack(2), release(2) = 8 bytes
                        buf[idx++] = 0x10; // type: effect_drc
                        buf[idx++] = 8;    // length
                        buf[idx++] = (uint8_t)(node->params.drc.threshold & 0xFF);
                        buf[idx++] = (uint8_t)((node->params.drc.threshold >> 8) & 0xFF);
                        buf[idx++] = (uint8_t)(node->params.drc.ratio & 0xFF);
                        buf[idx++] = (uint8_t)((node->params.drc.ratio >> 8) & 0xFF);
                        buf[idx++] = (uint8_t)(node->params.drc.attack & 0xFF);
                        buf[idx++] = (uint8_t)((node->params.drc.attack >> 8) & 0xFF);
                        buf[idx++] = (uint8_t)(node->params.drc.release & 0xFF);
                        buf[idx++] = (uint8_t)((node->params.drc.release >> 8) & 0xFF);
                        break;
                        
                    case EFFECT_NODE_TYPE_EFFECT_REVERB:
                        // Reverb: room_size(1), damping(1), wet_dry(1) = 3 bytes
                        buf[idx++] = 0x11; // type: effect_reverb
                        buf[idx++] = 3;    // length
                        buf[idx++] = (uint8_t)node->params.reverb.room_size;
                        buf[idx++] = (uint8_t)node->params.reverb.damping;
                        buf[idx++] = (uint8_t)node->params.reverb.wet_dry;
                        break;
                        
                    case EFFECT_NODE_TYPE_EFFECT_EQ:
                        // EQ: band_count(1), pregain(2), bands(5*band_count bytes) = variable
                        // Each band: gain(2), f0(4), Q(2), type(1), enabled(1) = 10 bytes per band
                        {
                            int band_count = node->params.eq.band_count;
                            int data_len = 3 + (band_count * 10); // band_count(1) + pregain(2) + bands
                            
                            if (idx + data_len + 4 > sizeof(buf)) { // Check buffer overflow
                                Shell_Printf("{\"error\":\"EQ data too large for buffer\"}");
                                return -1;
                            }
                            
                            buf[idx++] = 0x12; // type: effect_eq
                            buf[idx++] = (uint8_t)data_len; // length
                            buf[idx++] = (uint8_t)band_count;
                            buf[idx++] = (uint8_t)(node->params.eq.pregain & 0xFF);
                            buf[idx++] = (uint8_t)((node->params.eq.pregain >> 8) & 0xFF);
                            
                            // Add band data (limit to 10 bands max)
                            {
                                int i;
                                for (i = 0; i < band_count && i < 10; i++) {
                                    buf[idx++] = (uint8_t)(node->params.eq.band_gains[i] & 0xFF);
                                    buf[idx++] = (uint8_t)((node->params.eq.band_gains[i] >> 8) & 0xFF);
                                    buf[idx++] = (uint8_t)(node->params.eq.band_f0[i] & 0xFF);
                                    buf[idx++] = (uint8_t)((node->params.eq.band_f0[i] >> 8) & 0xFF);
                                    buf[idx++] = (uint8_t)((node->params.eq.band_f0[i] >> 16) & 0xFF);
                                    buf[idx++] = (uint8_t)((node->params.eq.band_f0[i] >> 24) & 0xFF);
                                    buf[idx++] = (uint8_t)(node->params.eq.band_Q[i] & 0xFF);
                                    buf[idx++] = (uint8_t)((node->params.eq.band_Q[i] >> 8) & 0xFF);
                                    buf[idx++] = (uint8_t)node->params.eq.band_types[i];
                                    buf[idx++] = (uint8_t)node->params.eq.band_enables[i];
                                }
                            }
                            break; // 补充EQ分支的break
                        } // 补充EQ分支的闭合大括号
                        
                    case EFFECT_NODE_TYPE_EFFECT_DELAY:
                        // Delay: delay_ms(2), feedback(1), wet_dry(1) = 4 bytes
                        buf[idx++] = 0x13; // type: effect_delay
                        buf[idx++] = 4;    // length
                        buf[idx++] = (uint8_t)(node->params.delay.delay_ms & 0xFF);
                        buf[idx++] = (uint8_t)((node->params.delay.delay_ms >> 8) & 0xFF);
                        buf[idx++] = (uint8_t)node->params.delay.feedback;
                        buf[idx++] = (uint8_t)node->params.delay.wet_dry;
                        break;
                        
                    case EFFECT_NODE_TYPE_EFFECT_GAIN:
                        // Gain: gain_db(2) = 2 bytes
                        buf[idx++] = 0x14; // type: effect_gain
                        buf[idx++] = 2;    // length
                        buf[idx++] = (uint8_t)(node->params.gain.gain_db & 0xFF);
                        buf[idx++] = (uint8_t)((node->params.gain.gain_db >> 8) & 0xFF);
                        break;
                        
                    default:
                        Shell_Printf("{\"error\":\"Unsupported effect type %d\"}", node->type);
                        return -1;
                }
                
                if (g_is_sync_command) {
                    BLE_BufferSyncResponse((const char *)buf, idx);
                } else {
                    Shell_WriteRaw(buf, idx);
                }
            } else {
                Shell_Printf("{\"error\":\"Effect node with ID %d not found\"}", node_id);
                return -1; // 补充返回值，避免语法警告
            }
        } else {
            // List all effect nodes in JSON format (keep JSON for listing)
            Shell_Printf("{\"status\":\"ok\",\"effects\":[");
            
            // For demonstration, return some example nodes
            EffectGraphRuntime_t* graph = EffectGraph_GetInstance();
            
            if (graph != NULL) {
                Shell_Printf("{\"id\":5,\"name\":\"eq_guitar_r\",\"type\":%d,\"enabled\":true}", EFFECT_NODE_TYPE_EFFECT_EQ);
                Shell_Printf(",{\"id\":10,\"name\":\"drc\",\"type\":%d,\"enabled\":true}", EFFECT_NODE_TYPE_EFFECT_DRC);
                Shell_Printf(",{\"id\":12,\"name\":\"reverb\",\"type\":%d,\"enabled\":true}", EFFECT_NODE_TYPE_EFFECT_REVERB);
            }
            
            Shell_Printf("]}");
        }
        return 0; // 补充effect分支的返回值，修复else前缺少}的核心问题
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
    OPT("q", "query",   "<target>", "Query params in binary format (system/volume/looper/bluetooth/lcd/effect/metronome)", param_query),
    OPT("e", "erase",   NULL,       "Erase param sector (danger!)", param_erase),
    OPT("t", "test",    NULL,       "Test flash save/load",         param_test),
    OPT("",  "set",     "<key> <val>", "Set volume param (bt_max/usb_max/usb_out/usb_mute)", param_set),
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
