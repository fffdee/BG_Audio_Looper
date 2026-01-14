/**
 * sys_param.c - System parameter storage module implementation
 *
 * This file implements the functions declared in sys_param.h for managing
 * system parameters in internal Flash, including initialization, save, load,
 * default restore, and shell command support.
 * 
 * Flash API (SDK provided):
 *   - SpiFlashRead(addr, buf, len, timeout) - Read from flash
 *   - SpiFlashWrite(addr, buf, len, timeout) - Write to flash
 *   - SpiFlashErase(SECTOR_ERASE, sector_num, 1) - Erase sector
 *   - SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3) - Unprotect
 */

#include "sys_param.h"
#include "spi_flash.h"  /* Flash API */
#include "bg_shell.h"   /* Shell_Printf for print functions */
#include <string.h>
#include <stdio.h>

/* Debug output */
#ifdef CFG_APP_CONFIG
#include "debug.h"
#define PARAM_DBG(...)  DBG(__VA_ARGS__)
#else
#define PARAM_DBG(...)
#endif

/* Global system parameters */
SysParam_t g_sys_param;
static uint8_t g_param_modified = 0;
static uint8_t g_param_initialized = 0;

/* Flash configuration from param_def.h */
#define FLASH_ADDR      SYS_PARAM_FLASH_ADDR
#define FLASH_SECTOR    SYS_PARAM_SECTOR_NUM
#define FLASH_TIMEOUT   SYS_PARAM_FLASH_TIMEOUT

/**
 * @brief Load default graph into audio_chain
 * Loads the default 14-node 13-edge topology:
 *   ADC0(Guitar) -+
 *   ADC1(Mic)   -+-> ADC_Mixer -> Expander -> DRC -> EQ -> Reverb -+
 *                                                                     |
 *   USB_In      -+                                                   |
 *   BT_In       -+-> USB_BT_Mixer -> USB_BT_EQ ------------------+
 *                                                                  |
 *   Reverb -----+                                                 |
 *   USB_BT_EQ --+-> Final_Mixer --> DAC0_Out
 *                                 \-> USB_Out
 */
static void LoadDefaultGraphConfig(void)
{
    int i;
    
    /* Clear audio chain */
    memset(&g_sys_param.audio_chain, 0, sizeof(SysParam_AudioChain_t));
    g_sys_param.audio_chain.output_mode = 0;
    g_sys_param.audio_chain.graph_count = 1;
    g_sys_param.audio_chain.active_graph_hp = 0;
    g_sys_param.audio_chain.active_graph_spk = 0;
    
    /* Initialize node pool */
    memset(g_sys_param.audio_chain.node_pool, 0, sizeof(GraphNode_t) * MAX_GRAPH_NODES);
    
    /* Set graph name */
    strcpy(g_sys_param.audio_chain.graphs[0].name, "Default");
    g_sys_param.audio_chain.graphs[0].node_count = 14;
    g_sys_param.audio_chain.graphs[0].edge_count = 13;
    memset(g_sys_param.audio_chain.graphs[0].node_ids, 0xFF, MAX_GRAPH_NODES);
    
    /* Create 14 nodes */
    /* N0: ADC0 (Guitar) */
    g_sys_param.audio_chain.node_pool[0].node_type = NODE_TYPE_SOURCE;
    g_sys_param.audio_chain.node_pool[0].subtype = SOURCE_TYPE_GUITAR;
    g_sys_param.audio_chain.node_pool[0].enabled = 1;
    g_sys_param.audio_chain.node_pool[0].volume = 100;
    
    /* N1: ADC1 (Mic) */
    g_sys_param.audio_chain.node_pool[1].node_type = NODE_TYPE_SOURCE;
    g_sys_param.audio_chain.node_pool[1].subtype = SOURCE_TYPE_MIC;
    g_sys_param.audio_chain.node_pool[1].enabled = 1;
    g_sys_param.audio_chain.node_pool[1].volume = 100;
    
    /* N2: USB_In */
    g_sys_param.audio_chain.node_pool[2].node_type = NODE_TYPE_SOURCE;
    g_sys_param.audio_chain.node_pool[2].subtype = SOURCE_TYPE_USB;
    g_sys_param.audio_chain.node_pool[2].enabled = 1;
    g_sys_param.audio_chain.node_pool[2].volume = 100;
    
    /* N3: BT_In */
    g_sys_param.audio_chain.node_pool[3].node_type = NODE_TYPE_SOURCE;
    g_sys_param.audio_chain.node_pool[3].subtype = SOURCE_TYPE_BT;
    g_sys_param.audio_chain.node_pool[3].enabled = 1;
    g_sys_param.audio_chain.node_pool[3].volume = 100;
    
    /* N4: ADC_Mixer */
    g_sys_param.audio_chain.node_pool[4].node_type = NODE_TYPE_MIXER;
    g_sys_param.audio_chain.node_pool[4].subtype = 0;
    g_sys_param.audio_chain.node_pool[4].enabled = 1;
    g_sys_param.audio_chain.node_pool[4].volume = 100;
    
    /* N5: Expander */
    g_sys_param.audio_chain.node_pool[5].node_type = NODE_TYPE_EFFECT;
    g_sys_param.audio_chain.node_pool[5].subtype = 11; /* Expander type */
    g_sys_param.audio_chain.node_pool[5].enabled = 1;
    g_sys_param.audio_chain.node_pool[5].volume = 100;
    
    /* N6: DRC */
    g_sys_param.audio_chain.node_pool[6].node_type = NODE_TYPE_EFFECT;
    g_sys_param.audio_chain.node_pool[6].subtype = EFFECT_TYPE_COMPRESSOR;
    g_sys_param.audio_chain.node_pool[6].enabled = 1;
    g_sys_param.audio_chain.node_pool[6].volume = 100;
    
    /* N7: EQ */
    g_sys_param.audio_chain.node_pool[7].node_type = NODE_TYPE_EFFECT;
    g_sys_param.audio_chain.node_pool[7].subtype = EFFECT_TYPE_EQ;
    g_sys_param.audio_chain.node_pool[7].enabled = 1;
    g_sys_param.audio_chain.node_pool[7].volume = 100;
    
    /* N8: Reverb */
    g_sys_param.audio_chain.node_pool[8].node_type = NODE_TYPE_EFFECT;
    g_sys_param.audio_chain.node_pool[8].subtype = EFFECT_TYPE_REVERB;
    g_sys_param.audio_chain.node_pool[8].enabled = 1;
    g_sys_param.audio_chain.node_pool[8].volume = 100;
    g_sys_param.audio_chain.node_pool[8].preset = 1;
    
    /* N9: USB_BT_Mixer */
    g_sys_param.audio_chain.node_pool[9].node_type = NODE_TYPE_MIXER;
    g_sys_param.audio_chain.node_pool[9].subtype = 0;
    g_sys_param.audio_chain.node_pool[9].enabled = 1;
    g_sys_param.audio_chain.node_pool[9].volume = 100;
    
    /* N10: USB_BT_EQ */
    g_sys_param.audio_chain.node_pool[10].node_type = NODE_TYPE_EFFECT;
    g_sys_param.audio_chain.node_pool[10].subtype = EFFECT_TYPE_EQ;
    g_sys_param.audio_chain.node_pool[10].enabled = 1;
    g_sys_param.audio_chain.node_pool[10].volume = 100;
    
    /* N11: Final_Mixer */
    g_sys_param.audio_chain.node_pool[11].node_type = NODE_TYPE_MIXER;
    g_sys_param.audio_chain.node_pool[11].subtype = 0;
    g_sys_param.audio_chain.node_pool[11].enabled = 1;
    g_sys_param.audio_chain.node_pool[11].volume = 100;
    
    /* N12: DAC0_Out */
    g_sys_param.audio_chain.node_pool[12].node_type = NODE_TYPE_OUTPUT;
    g_sys_param.audio_chain.node_pool[12].subtype = OUTPUT_TYPE_HEADPHONE;
    g_sys_param.audio_chain.node_pool[12].enabled = 1;
    g_sys_param.audio_chain.node_pool[12].volume = 100;
    
    /* N13: USB_Out */
    g_sys_param.audio_chain.node_pool[13].node_type = NODE_TYPE_OUTPUT;
    g_sys_param.audio_chain.node_pool[13].subtype = 2; /* USB output type */
    g_sys_param.audio_chain.node_pool[13].enabled = 1;
    g_sys_param.audio_chain.node_pool[13].volume = 100;
    
    /* Mark nodes 0-13 as used */
    g_sys_param.audio_chain.node_used_mask = 0x3FFF; /* 0b0011111111111111 = nodes 0-13 used */
    
    /* Add nodes to graph */
    for (i = 0; i < 14; i++) {
        g_sys_param.audio_chain.graphs[0].node_ids[i] = i;
    }
    
    /* Create 13 edges */
    /* ADC0 -> ADC_Mixer */
    g_sys_param.audio_chain.graphs[0].edges[0].from_node = 0;
    g_sys_param.audio_chain.graphs[0].edges[0].to_node = 4;
    
    /* ADC1 -> ADC_Mixer */
    g_sys_param.audio_chain.graphs[0].edges[1].from_node = 1;
    g_sys_param.audio_chain.graphs[0].edges[1].to_node = 4;
    
    /* ADC_Mixer -> Expander */
    g_sys_param.audio_chain.graphs[0].edges[2].from_node = 4;
    g_sys_param.audio_chain.graphs[0].edges[2].to_node = 5;
    
    /* Expander -> DRC */
    g_sys_param.audio_chain.graphs[0].edges[3].from_node = 5;
    g_sys_param.audio_chain.graphs[0].edges[3].to_node = 6;
    
    /* DRC -> EQ */
    g_sys_param.audio_chain.graphs[0].edges[4].from_node = 6;
    g_sys_param.audio_chain.graphs[0].edges[4].to_node = 7;
    
    /* EQ -> Reverb */
    g_sys_param.audio_chain.graphs[0].edges[5].from_node = 7;
    g_sys_param.audio_chain.graphs[0].edges[5].to_node = 8;
    
    /* USB_In -> USB_BT_Mixer */
    g_sys_param.audio_chain.graphs[0].edges[6].from_node = 2;
    g_sys_param.audio_chain.graphs[0].edges[6].to_node = 9;
    
    /* BT_In -> USB_BT_Mixer */
    g_sys_param.audio_chain.graphs[0].edges[7].from_node = 3;
    g_sys_param.audio_chain.graphs[0].edges[7].to_node = 9;
    
    /* USB_BT_Mixer -> USB_BT_EQ */
    g_sys_param.audio_chain.graphs[0].edges[8].from_node = 9;
    g_sys_param.audio_chain.graphs[0].edges[8].to_node = 10;
    
    /* Reverb -> Final_Mixer */
    g_sys_param.audio_chain.graphs[0].edges[9].from_node = 8;
    g_sys_param.audio_chain.graphs[0].edges[9].to_node = 11;
    
    /* USB_BT_EQ -> Final_Mixer */
    g_sys_param.audio_chain.graphs[0].edges[10].from_node = 10;
    g_sys_param.audio_chain.graphs[0].edges[10].to_node = 11;
    
    /* Final_Mixer -> DAC0_Out */
    g_sys_param.audio_chain.graphs[0].edges[11].from_node = 11;
    g_sys_param.audio_chain.graphs[0].edges[11].to_node = 12;
    
    /* Final_Mixer -> USB_Out */
    g_sys_param.audio_chain.graphs[0].edges[12].from_node = 11;
    g_sys_param.audio_chain.graphs[0].edges[12].to_node = 13;
    
    /* Clear other graphs */
    for (i = 1; i < MAX_EFFECT_GRAPHS; i++) {
        memset(&g_sys_param.audio_chain.graphs[i], 0, sizeof(EffectGraph_t));
        memset(g_sys_param.audio_chain.graphs[i].node_ids, 0xFF, MAX_GRAPH_NODES);
    }
}

/* Standard CRC32 implementation (polynomial 0xEDB88320) */
static uint32_t calc_crc32(const void *data, size_t len) {
    const uint8_t *buf = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    size_t i;
    int j;
    for (i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (j = 0; j < 8; ++j) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

/**
 * @brief Unlock flash protection for write/erase operations
 */
static void flash_unprotect(void) {
    SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3);
}

/**
 * @brief Load parameters from flash
 * @param param Pointer to parameter structure to fill
 * @return 0 on success, -1 on error
 */
static int flash_load(SysParam_t *param) {
    int ret;
    uint32_t stored_crc, calc_crc_val;
    
    PARAM_DBG("[PARAM] Loading from flash addr 0x%08lX...\n", (unsigned long)FLASH_ADDR);
    
    /* Read entire parameter block from flash */
    ret = SpiFlashRead(FLASH_ADDR, (uint8_t*)param, sizeof(SysParam_t), FLASH_TIMEOUT);
    if (ret != FLASH_NONE_ERR) {
        PARAM_DBG("[PARAM] Flash read error: %d\n", ret);
        return -1;
    }
    
    /* Check magic number */
    if (param->magic != SYS_PARAM_MAGIC) {
        PARAM_DBG("[PARAM] Invalid magic: 0x%08lX (expected 0x%08lX)\n", 
                  (unsigned long)param->magic, (unsigned long)SYS_PARAM_MAGIC);
        return -1;
    }
    
    /* Verify CRC (calculate on data after crc32 field) */
    stored_crc = param->crc32;
    param->crc32 = 0;  /* Zero out for calculation */
    calc_crc_val = calc_crc32(param, sizeof(SysParam_t));
    param->crc32 = stored_crc;  /* Restore */
    
    if (stored_crc != calc_crc_val) {
        PARAM_DBG("[PARAM] CRC mismatch: stored=0x%08lX calc=0x%08lX\n", 
                  (unsigned long)stored_crc, (unsigned long)calc_crc_val);
        return -1;
    }
    
    PARAM_DBG("[PARAM] Load success, write_count=%lu\n", (unsigned long)param->write_count);
    return 0;
}

/**
 * @brief Save parameters to flash
 * @param param Pointer to parameter structure to save
 * @return 0 on success, -1 on error
 */
static int flash_save(SysParam_t *param) {
    int ret;
    
    PARAM_DBG("[PARAM] Saving to flash addr 0x%08lX...\n", (unsigned long)FLASH_ADDR);
    
    /* Update header fields */
    param->magic = SYS_PARAM_MAGIC;
    param->version = SYS_PARAM_VERSION;
    param->size = sizeof(SysParam_t);
    param->write_count++;
    
    /* Calculate CRC (with crc32 field zeroed) */
    param->crc32 = 0;
    param->crc32 = calc_crc32(param, sizeof(SysParam_t));
    
    /* Unlock flash */
    flash_unprotect();
    
    /* Erase sector */
    PARAM_DBG("[PARAM] Erasing sector %d...\n", FLASH_SECTOR);
    SpiFlashErase(SECTOR_ERASE, FLASH_SECTOR, 1);
    
    /* Write data */
    PARAM_DBG("[PARAM] Writing %d bytes...\n", (int)sizeof(SysParam_t));
    ret = SpiFlashWrite(FLASH_ADDR, (uint8_t*)param, sizeof(SysParam_t), FLASH_TIMEOUT);
    if (ret != FLASH_NONE_ERR) {
        PARAM_DBG("[PARAM] Flash write error: %d\n", ret);
        return -1;
    }
    
    PARAM_DBG("[PARAM] Save success, write_count=%lu, CRC=0x%08lX\n", 
              (unsigned long)param->write_count, (unsigned long)param->crc32);
    return 0;
}

SysParam_Status_t SysParam_Init(void) {
    PARAM_DBG("[PARAM] Initializing system parameters...\n");
    
    if (flash_load(&g_sys_param) != 0 || g_sys_param.magic != SYS_PARAM_MAGIC) {
        PARAM_DBG("[PARAM] Load failed or invalid, loading defaults\n");
        SysParam_LoadDefault();
        g_param_modified = 1;
        g_param_initialized = 1;
        return SYSPARAM_ERR_MAGIC;
    }
    
    /* Check version compatibility */
    if (g_sys_param.version != SYS_PARAM_VERSION) {
        PARAM_DBG("[PARAM] Version mismatch: stored=0x%04X expected=0x%04X\n",
                  g_sys_param.version, SYS_PARAM_VERSION);
        /* Could implement migration here, for now just reload defaults */
        SysParam_LoadDefault();
        g_param_modified = 1;
        g_param_initialized = 1;
        return SYSPARAM_ERR_VERSION;
    }
    
    g_param_modified = 0;
    g_param_initialized = 1;
    PARAM_DBG("[PARAM] Init complete, loaded %lu bytes\n", (unsigned long)sizeof(SysParam_t));
    return SYSPARAM_OK;
}

SysParam_Status_t SysParam_Save(void) {
    if (!g_param_initialized) {
        PARAM_DBG("[PARAM] Error: not initialized\n");
        return SYSPARAM_ERR_NOT_INIT;
    }
    
    if (flash_save(&g_sys_param) == 0) {
        g_param_modified = 0;
        PARAM_DBG("[PARAM] Parameters saved successfully\n");
        return SYSPARAM_OK;
    }
    PARAM_DBG("[PARAM] Save failed!\n");
    return SYSPARAM_ERR_FLASH;
}

SysParam_t* SysParam_Get(void) {
    return &g_sys_param;
}

SysParam_Status_t SysParam_LoadDefault(void) {
    PARAM_DBG("[PARAM] Loading default parameters...\n");
    memset(&g_sys_param, 0, sizeof(SysParam_t));
    
    /* Header */
    g_sys_param.magic = SYS_PARAM_MAGIC;
    g_sys_param.version = SYS_PARAM_VERSION;
    g_sys_param.size = sizeof(SysParam_t);
    g_sys_param.write_count = 0;
    
    /* System defaults */
    g_sys_param.system.current_boot_status = NORMAL_BOOT;
    g_sys_param.system.boot_count = 0;
    
    /* Volume defaults */
    g_sys_param.volume.guitar1_volume = 80;
    g_sys_param.volume.guitar2_volume = 80;
    g_sys_param.volume.mic1_volume = 80;
    g_sys_param.volume.mic2_volume = 80;
    g_sys_param.volume.output_volume = 80;
    
    /* Looper defaults */
    g_sys_param.looper.loop_count = 4;
    g_sys_param.looper.overdub_mode = 0;
    g_sys_param.looper.quantize = 0;
    g_sys_param.looper.click_volume = 50;
    g_sys_param.looper.tempo = 120;
    g_sys_param.looper.time_signature = 0;  /* 4/4 */
    g_sys_param.looper.fade_time = 10;      /* 100ms */
    g_sys_param.looper.max_loop_time = 60000; /* 60 seconds */
    
    /* Bluetooth defaults */
    g_sys_param.bluetooth.enabled = 1;
    g_sys_param.bluetooth.discoverable = 1;
    g_sys_param.bluetooth.auto_connect = 1;
    g_sys_param.bluetooth.a2dp_volume = 80;
    strcpy(g_sys_param.bluetooth.device_name, "BanBox");
    memset(g_sys_param.bluetooth.paired_addr, 0, 6);
    
    /* LCD defaults */
    g_sys_param.lcd.contrast = 50;
    g_sys_param.lcd.color_scheme = 0;
    g_sys_param.lcd.screen_saver = 0;
    g_sys_param.lcd.bg_color = 0x0000;  /* Black */
    
    /* Chain manager defaults */
    memset(&g_sys_param.chain_manager, 0, sizeof(BG_ParamChainManager_t));
    strcpy(g_sys_param.chain_manager.chains[0].name, "ChainA");
    strcpy(g_sys_param.chain_manager.chains[1].name, "ChainB");
    g_sys_param.chain_manager.active_chain = 0;
    
    /* Audio chain defaults - load from effect_graph_config.h */
    LoadDefaultGraphConfig();
    
    g_param_modified = 1;
    PARAM_DBG("[PARAM] Defaults loaded\n");
    return SYSPARAM_OK;
}

bool SysParam_IsModified(void) {
    return g_param_modified ? true : false;
}

uint32_t SysParam_GetWriteCount(void) {
    return g_sys_param.write_count;
}

/**
 * @brief Save a specific module's parameters
 * @param module Module name: "system", "audio", "looper", "bt", "lcd", "all"
 * @return Status code
 */
SysParam_Status_t SysParam_SaveModule(const char *module) {
    /* For simple implementation, just save entire parameter block */
    /* This could be optimized to save only specific module areas */
    (void)module;
    PARAM_DBG("[PARAM] Saving module: %s\n", module ? module : "all");
    return SysParam_Save();
}

void SysParam_Print(void) {
    Shell_Printf("=== System Parameters ===\n");
    Shell_Printf("Magic:       0x%08lX\n", (unsigned long)g_sys_param.magic);
    Shell_Printf("Version:     0x%04X\n", g_sys_param.version);
    Shell_Printf("Size:        %u bytes\n", g_sys_param.size);
    Shell_Printf("CRC32:       0x%08lX\n", (unsigned long)g_sys_param.crc32);
    Shell_Printf("WriteCount:  %lu\n", (unsigned long)g_sys_param.write_count);
    Shell_Printf("Modified:    %s\n", g_param_modified ? "Yes" : "No");
    Shell_Printf("\n--- System ---\n");
    Shell_Printf("  BootStatus: %d\n", g_sys_param.system.current_boot_status);
    Shell_Printf("  BootCount:  %d\n", g_sys_param.system.boot_count);
    Shell_Printf("\n--- Volume ---\n");
    Shell_Printf("  Guitar1:    %d\n", g_sys_param.volume.guitar1_volume);
    Shell_Printf("  Guitar2:    %d\n", g_sys_param.volume.guitar2_volume);
    Shell_Printf("  Mic1:       %d\n", g_sys_param.volume.mic1_volume);
    Shell_Printf("  Mic2:       %d\n", g_sys_param.volume.mic2_volume);
    Shell_Printf("  Output:     %d\n", g_sys_param.volume.output_volume);
    Shell_Printf("\n--- Looper ---\n");
    Shell_Printf("  LoopCount:  %d\n", g_sys_param.looper.loop_count);
    Shell_Printf("  Overdub:    %d\n", g_sys_param.looper.overdub_mode);
    Shell_Printf("  Quantize:   %d\n", g_sys_param.looper.quantize);
    Shell_Printf("  ClickVol:   %d\n", g_sys_param.looper.click_volume);
    Shell_Printf("  Tempo:      %d BPM\n", g_sys_param.looper.tempo);
    Shell_Printf("\n--- Bluetooth ---\n");
    Shell_Printf("  Enabled:    %d\n", g_sys_param.bluetooth.enabled);
    Shell_Printf("  DevName:    %s\n", g_sys_param.bluetooth.device_name);
    Shell_Printf("  A2DP Vol:   %d\n", g_sys_param.bluetooth.a2dp_volume);
    Shell_Printf("\n--- LCD ---\n");
    Shell_Printf("  Contrast:   %d\n", g_sys_param.lcd.contrast);
    Shell_Printf("  ColorScheme:%d\n", g_sys_param.lcd.color_scheme);
    Shell_Printf("  BgColor:    0x%04X\n", g_sys_param.lcd.bg_color);
    Shell_Printf("\n--- Audio Chain (Graph) ---\n");
    Shell_Printf("  OutputMode: %d\n", g_sys_param.audio_chain.output_mode);
    Shell_Printf("  GraphCount: %d\n", g_sys_param.audio_chain.graph_count);
    Shell_Printf("  HP Graph:   %d\n", g_sys_param.audio_chain.active_graph_hp);
    Shell_Printf("  SPK Graph:  %d\n", g_sys_param.audio_chain.active_graph_spk);
    Shell_Printf("  NodePool:   %d/%d used\n",
                __builtin_popcount(g_sys_param.audio_chain.node_used_mask),
                MAX_GRAPH_NODES);
}

void SysParam_PrintModule(const char *module) {
    if (!module) {
        SysParam_Print();
        return;
    }
    Shell_Printf("=== Module: %s ===\n", module);
    if (strcmp(module, "system") == 0 || strcmp(module, "sys") == 0) {
        Shell_Printf("  BootStatus: %d\n", g_sys_param.system.current_boot_status);
        Shell_Printf("  BootCount:  %d\n", g_sys_param.system.boot_count);
    }
    else if (strcmp(module, "audio") == 0 || strcmp(module, "vol") == 0) {
        Shell_Printf("  Guitar1:    %d\n", g_sys_param.volume.guitar1_volume);
        Shell_Printf("  Guitar2:    %d\n", g_sys_param.volume.guitar2_volume);
        Shell_Printf("  Mic1:       %d\n", g_sys_param.volume.mic1_volume);
        Shell_Printf("  Mic2:       %d\n", g_sys_param.volume.mic2_volume);
        Shell_Printf("  Output:     %d\n", g_sys_param.volume.output_volume);
    }
    else if (strcmp(module, "looper") == 0) {
        Shell_Printf("  LoopCount:  %d\n", g_sys_param.looper.loop_count);
        Shell_Printf("  Overdub:    %d\n", g_sys_param.looper.overdub_mode);
        Shell_Printf("  Quantize:   %d\n", g_sys_param.looper.quantize);
        Shell_Printf("  ClickVol:   %d\n", g_sys_param.looper.click_volume);
        Shell_Printf("  Tempo:      %d BPM\n", g_sys_param.looper.tempo);
        Shell_Printf("  TimeSig:    %d\n", g_sys_param.looper.time_signature);
        Shell_Printf("  FadeTime:   %d\n", g_sys_param.looper.fade_time);
        Shell_Printf("  MaxTime:    %lu ms\n", (unsigned long)g_sys_param.looper.max_loop_time);
    }
    else if (strcmp(module, "bt") == 0 || strcmp(module, "bluetooth") == 0) {
        Shell_Printf("  Enabled:    %d\n", g_sys_param.bluetooth.enabled);
        Shell_Printf("  Discover:   %d\n", g_sys_param.bluetooth.discoverable);
        Shell_Printf("  AutoConn:   %d\n", g_sys_param.bluetooth.auto_connect);
        Shell_Printf("  A2DP Vol:   %d\n", g_sys_param.bluetooth.a2dp_volume);
        Shell_Printf("  DevName:    %s\n", g_sys_param.bluetooth.device_name);
    }
    else if (strcmp(module, "lcd") == 0) {
        Shell_Printf("  Contrast:   %d\n", g_sys_param.lcd.contrast);
        Shell_Printf("  ColorScheme:%d\n", g_sys_param.lcd.color_scheme);
        Shell_Printf("  ScreenSaver:%d\n", g_sys_param.lcd.screen_saver);
        Shell_Printf("  BgColor:    0x%04X\n", g_sys_param.lcd.bg_color);
    }
    else if (strcmp(module, "chain") == 0 || strcmp(module, "audiochain") == 0 || strcmp(module, "graph") == 0) {
        Shell_Printf("  OutputMode: %d (0:Auto 1:HP 2:SPK)\n", g_sys_param.audio_chain.output_mode);
        Shell_Printf("  GraphCount: %d\n", g_sys_param.audio_chain.graph_count);
        Shell_Printf("  HP Graph:   %d\n", g_sys_param.audio_chain.active_graph_hp);
        Shell_Printf("  SPK Graph:  %d\n", g_sys_param.audio_chain.active_graph_spk);
        Shell_Printf("  NodePool:   %d/%d used\n",
                    __builtin_popcount(g_sys_param.audio_chain.node_used_mask),
                    MAX_GRAPH_NODES);
        
        /* List all graphs */
        int g, i, j;
        for (g = 0; g < g_sys_param.audio_chain.graph_count; g++) {
            EffectGraph_t *graph = &g_sys_param.audio_chain.graphs[g];
            Shell_Printf("  [Graph %d: %s]\n", g, graph->name);
            Shell_Printf("    Nodes: %d, Edges: %d\n", graph->node_count, graph->edge_count);
            
            /* List nodes */
            for (i = 0; i < graph->node_count; i++) {
                uint8_t nid = graph->node_ids[i];
                GraphNode_t *node = &g_sys_param.audio_chain.node_pool[nid];
                const char *type_str[] = {"SRC", "FX", "MIX", "OUT"};
                Shell_Printf("      N%d: %s subtype=%d vol=%d %s\n", 
                            nid, type_str[node->node_type], node->subtype, 
                            node->volume, node->enabled ? "ON" : "OFF");
            }
            
            /* List edges */
            if (graph->edge_count > 0) {
                Shell_Printf("    Edges:\n");
                for (j = 0; j < graph->edge_count; j++) {
                    Shell_Printf("      N%d -> N%d\n", 
                                graph->edges[j].from_node, graph->edges[j].to_node);
                }
            }
        }
    }
    else {
        Shell_Printf("Unknown module: %s\n", module);
        Shell_Printf("Available: system, audio, looper, bt, lcd, chain\n");
    }
}

void SysParam_MarkModified(void) {
    g_param_modified = 1;
}

void SysParam_RegisterShellCommands(void) {
    /* Shell commands are registered via REGISTER_MODULE macro */
    PARAM_DBG("[PARAM] Shell commands ready\n");
}

int SysParam_ShellCmd(int argc, char *argv[]) {
    /* This is called by shell framework */
    (void)argc; (void)argv;
    return 0;
}

