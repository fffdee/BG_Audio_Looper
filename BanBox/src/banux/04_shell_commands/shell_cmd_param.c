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
 * @brief Query parameters in JSON format for APP
 */

 static int param_query(int argc, char *argv[])
{
    extern SysParam_t g_sys_param;
    const char *target = (argc >= 1) ? argv[0] : "all";
    
    if (strcmp(target, "all") == 0 || strcmp(target, "system") == 0) {

        Shell_Printf("{\"status\":\"ok\",\"system\":{" 
                    "\"boot_count\":%d," 
                    "\"current_boot_status\":%d" 
                    "}}", 
                    g_sys_param.system.boot_count,
                    g_sys_param.system.current_boot_status);
        Shell_Printf("\n");

    }
    else if (strcmp(target, "volume") == 0) {
        Shell_Printf("{\"status\":\"ok\",\"volume\":{" 
                    "\"mic1\":%d," 
                    "\"mic2\":%d," 
                    "\"guitar1\":%d," 
                    "\"guitar2\":%d," 
                    "\"output\":%d" 
                    "}}",
                    g_sys_param.volume.mic1_volume,
                    g_sys_param.volume.mic2_volume,
                    g_sys_param.volume.guitar1_volume,
                    g_sys_param.volume.guitar2_volume,
                    g_sys_param.volume.output_volume);
        Shell_Printf("\n");
    }
    else if (strcmp(target, "looper") == 0) {
        Shell_Printf("{\"status\":\"ok\",\"looper\":{" 
                    "\"loop_count\":%d," 
                    "\"overdub_mode\":%d," 
                    "\"quantize\":%d," 
                    "\"click_volume\":%d," 
                    "\"tempo\":%d," 
                    "\"time_signature\":%d," 
                    "\"fade_time\":%d," 
                    "\"max_loop_time\":%lu" 
                    "}}",
                    g_sys_param.looper.loop_count,
                    g_sys_param.looper.overdub_mode,
                    g_sys_param.looper.quantize,
                    g_sys_param.looper.click_volume,
                    g_sys_param.looper.tempo,
                    g_sys_param.looper.time_signature,
                    g_sys_param.looper.fade_time,
                    (unsigned long)g_sys_param.looper.max_loop_time);
        Shell_Printf("\n");
    }
    else if (strcmp(target, "effect") == 0 || strcmp(target, "effects") == 0) {
        // Query effect node parameters by ID in JSON format for app
        if (argc >= 2) {
            int node_id = atoi(argv[1]);
            extern EffectNode_t* EffectGraph_FindNodeById(uint8_t id);
            EffectNode_t* node = EffectGraph_FindNodeById((uint8_t)node_id);
            
            if (node != NULL) {
                // JSON response for app
                Shell_Printf("{\"status\":\"ok\",\"effect\":{");
                Shell_Printf("\"id\":%d,", node->id);
                Shell_Printf("\"name\":\"%s\",", node->name);
                Shell_Printf("\"type\":%d,", node->type);
                Shell_Printf("\"enabled\":%s,", node->enabled ? "true" : "false");
                Shell_Printf("\"bypass\":%s,", node->bypass ? "true" : "false");
                Shell_Printf("\"params\":{");
                
                // Type-specific parameters
                switch (node->type) {
                    case EFFECT_NODE_TYPE_EFFECT_EQ:
                        Shell_Printf("\"eq\":{");
                        Shell_Printf("\"band_count\":%d,", node->params.eq.band_count);
                        Shell_Printf("\"pregain\":%d,", node->params.eq.pregain);
                        Shell_Printf("\"bands\":[");
                        {
                            int i;
                            for (i = 0; i < node->params.eq.band_count && i < 10; i++) {
                                if (i > 0) Shell_Printf(",");
                                Shell_Printf("{\"gain\":%d,\"f0\":%lu,\"Q\":%lu,\"type\":%d,\"enabled\":%s}",
                                           node->params.eq.band_gains[i],
                                           (unsigned long)node->params.eq.band_f0[i],
                                           (unsigned long)node->params.eq.band_Q[i],
                                           node->params.eq.band_types[i],
                                           node->params.eq.band_enables[i] ? "true" : "false");
                            }
                        }
                        Shell_Printf("]}");
                        break;
                        
                    case EFFECT_NODE_TYPE_EFFECT_DRC:
                        Shell_Printf("\"drc\":{");
                        Shell_Printf("\"threshold\":%d,", node->params.drc.threshold);
                        Shell_Printf("\"ratio\":%d,", node->params.drc.ratio);
                        Shell_Printf("\"attack\":%d,", node->params.drc.attack);
                        Shell_Printf("\"release\":%d", node->params.drc.release);
                        Shell_Printf("}");
                        break;
                        
                    case EFFECT_NODE_TYPE_EFFECT_REVERB:
                        Shell_Printf("\"reverb\":{");
                        Shell_Printf("\"room_size\":%d,", node->params.reverb.room_size);
                        Shell_Printf("\"damping\":%d,", node->params.reverb.damping);
                        Shell_Printf("\"wet_dry\":%d", node->params.reverb.wet_dry);
                        Shell_Printf("}");
                        break;
                        
                    case EFFECT_NODE_TYPE_EFFECT_DELAY:
                        Shell_Printf("\"delay\":{");
                        Shell_Printf("\"delay_ms\":%d,", node->params.delay.delay_ms);
                        Shell_Printf("\"feedback\":%d,", node->params.delay.feedback);
                        Shell_Printf("\"wet_dry\":%d", node->params.delay.wet_dry);
                        Shell_Printf("}");
                        break;
                        
                    case EFFECT_NODE_TYPE_EFFECT_GAIN:
                        Shell_Printf("\"gain\":{");
                        Shell_Printf("\"gain_db\":%d", node->params.gain.gain_db);
                        Shell_Printf("}");
                        break;
                        
                    default:
                        // No additional parameters
                        break;
                }
                
                Shell_Printf("}}}");
                Shell_Printf("\n");
            } else {
                Shell_Printf("{\"error\":\"Effect node with ID %d not found\"}", node_id);
                Shell_Printf("\n");
            }
        } else {
            // List all effect nodes in JSON format
            Shell_Printf("{\"status\":\"ok\",\"effects\":[");
            
            // For demonstration, return some example nodes
            // In a real implementation, you would iterate through all nodes in the graph
            extern EffectGraphRuntime_t* EffectGraph_GetInstance(void);
            EffectGraphRuntime_t* graph = EffectGraph_GetInstance();
            
            if (graph != NULL) {
                // Simplified implementation - in practice you would enumerate all nodes
                Shell_Printf("{\"id\":5,\"name\":\"eq_guitar_r\",\"type\":%d,\"enabled\":true}", EFFECT_NODE_TYPE_EFFECT_EQ);
                Shell_Printf(",{\"id\":10,\"name\":\"drc\",\"type\":%d,\"enabled\":true}", EFFECT_NODE_TYPE_EFFECT_DRC);
                Shell_Printf(",{\"id\":12,\"name\":\"reverb\",\"type\":%d,\"enabled\":true}", EFFECT_NODE_TYPE_EFFECT_REVERB);
            }
            
            Shell_Printf("]}");
            Shell_Printf("\n");
        }
    }
    else if (strcmp(target, "metronome") == 0) {
        Shell_Printf("{\"status\":\"ok\",\"metronome\":{");
        Shell_Printf("\"enabled\":%s,", g_sys_param.looper.click_volume > 0 ? "true" : "false");
        Shell_Printf("\"bpm\":%d,", g_sys_param.looper.tempo);
        Shell_Printf("\"beats\":%d,", g_sys_param.looper.time_signature + 4);
        Shell_Printf("\"volume\":%d", g_sys_param.looper.click_volume);
        Shell_Printf("}}");
        Shell_Printf("\n");
    }
    else if (strcmp(target, "lcd") == 0) {
        Shell_Printf("{\"status\":\"ok\",\"lcd\":{" 
                    "\"contrast\":%d," 
                    "\"color_scheme\":%d," 
                    "\"screen_saver\":%d," 
                    "\"bg_color\":%d" 
                    "}}",
                    g_sys_param.lcd.contrast,
                    g_sys_param.lcd.color_scheme,
                    g_sys_param.lcd.screen_saver,
                    g_sys_param.lcd.bg_color);
        Shell_Printf("\n");
    }
    else {
        Shell_Printf("{\"error\":\"Unknown target: %s\"}", target);
        Shell_Printf("\n");
        Shell_Printf("{\"hint\":\"Available: all, system, volume, looper, bluetooth, lcd, effect, metronome\"}");
        Shell_Printf("\n");
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
    OPT("q", "query",   "<target>", "Query params in JSON (system/volume/looper/bluetooth/lcd/effect/metronome)", param_query),
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
