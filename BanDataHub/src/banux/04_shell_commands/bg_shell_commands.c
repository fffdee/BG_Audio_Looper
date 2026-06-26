/**
 *****************************************************************************
 * @file     bg_shell_commands.c
 * @author   BG Card Team
 * @version  V2.0.0
 * @date     16-December-2025
 * @brief    Shell command module implementation
 *****************************************************************************
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bg_shell.h"
#include "shell_io_cdc.h"
#include "shell_io_ble.h"
#include "shell_io_manager.h"
#include "remind_sound.h"

#include "bg_lcd.h"
#include "BG_FlashMgr.h"
#include "flash_bus.h"

#include "gpio.h"
#include "adc.h"
#include "dac.h"
#include "bt_a2dp_api.h"
#include "battery_drv.h"
#include "drv_init.h"
#include "flash_boot.h"
#include "vfs.h"
#include "drv_fs.h"
#include "drv_device.h"
#include "chip_info.h"
#include "FreeRTOS.h"
#include "task.h"

#include "audio_setting.h"



#include "sys_param.h"

#include "shell_cmd_fat.h"
#include "sd_card.h"
#include "fat32_reader.h"

#if HW_CMD_PSRAM_EN
/* PSRAM 内存管理命令 */
#include "shell_cmd_psram.h"
#endif

/*============================================================================
 * Common save function for modules
 *===========================================================================*/

static int audio_save_param(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("Saving audio parameters to flash...\r\n");
    if (SysParam_SaveModule("audio") == SYSPARAM_OK) {
        Shell_Print("Audio params saved\r\n");
        return 0;
    }
    Shell_Print("Save failed!\r\n");
    return -1;
}

static int bt_save_param(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("Saving bluetooth parameters to flash...\r\n");
    if (SysParam_SaveModule("bt") == SYSPARAM_OK) {
        Shell_Print("Bluetooth params saved\r\n");
        return 0;
    }
    Shell_Print("Save failed!\r\n");
    return -1;
}

static int lcd_save_param(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("Saving LCD parameters to flash...\r\n");
    if (SysParam_SaveModule("lcd") == SYSPARAM_OK) {
        Shell_Print("LCD params saved\r\n");
        return 0;
    }
    Shell_Print("Save failed!\r\n");
    return -1;
}

/*============================================================================
 * sys module - System information
 *===========================================================================*/

static int sys_info(int argc, char *argv[])
{
    ShellIOType_t active_io;
    ShellIOState_t state;
    const char *state_str;
    
    (void)argc; (void)argv;
    
    active_io = ShellIOManager_GetActiveIO();
    state = ShellIOManager_GetState();
    
    switch (state)
    {
        case SHELL_IO_STATE_IDLE: state_str = "IDLE"; break;
        case SHELL_IO_STATE_ACTIVE: state_str = "ACTIVE"; break;
        case SHELL_IO_STATE_LOCKED: state_str = "LOCKED"; break;
        default: state_str = "UNKNOWN"; break;
    }
    
    uint64_t chip_id = 0;
    uint16_t id_suffix = 0;
    
    Chip_IDGet(&chip_id);
    id_suffix = (uint16_t)((chip_id >> 48) & 0xFFFF);
    
    Shell_Print("\r\nSystem Information:\r\n");
    Shell_Print("  Device:    BG Card Mini\r\n");
    Shell_Print("  MCU:       BP1048\r\n");
    Shell_Print("  Clock:     288MHz\r\n");
    Shell_Print("  Flash:     4MB\r\n");
    Shell_Print("  RAM:       320KB\r\n");
    Shell_Printf("  Chip ID:   0x%016llX (Suffix: %04X)\r\n", chip_id, id_suffix);
    Shell_Printf("  Shell IO:  %s (%s)\r\n\r\n", 
                 ShellIOManager_GetIOName(active_io), state_str);
    return 0;
}

static int sys_mem(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("\r\nMemory Status:\r\n");
    Shell_Print("  Total:     128 KB\r\n");
    Shell_Print("  Used:      64 KB\r\n");
    Shell_Print("  Free:      64 KB\r\n");
    Shell_Print("  Heap:      32 KB free\r\n\r\n");
    return 0;
}

static int sys_tasks(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("\r\nTask List:\r\n");
    Shell_Print("  Name          State   Pri   Stack\r\n");
    Shell_Print("  ----------    -----   ---   -----\r\n");
    Shell_Print("  IDLE          Ready   0     128\r\n");
    Shell_Print("  MainTask      Run     3     256\r\n");
    Shell_Print("  AudioTask     Ready   5     512\r\n");
    Shell_Print("  USBTask       Ready   4     256\r\n\r\n");
    return 0;
}

static int sys_uptime(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("Uptime: 00:12:34\r\n");
    return 0;
}

static int sys_reboot(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("System rebooting...\r\n");
    Reset_McuSystem ();
    return 0;
}

/* Factory reset - restore default settings */
static int sys_factory_reset(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("\r\n========================================\r\n");
    Shell_Print("Factory Reset - Restore Default Settings\r\n");
    Shell_Print("========================================\r\n");
    Shell_Print("WARNING: All settings will be lost!\r\n\r\n");
    
    /* Load default parameters */
    Shell_Print("Loading default parameters...\r\n");
    SysParam_LoadDefault();
    
    /* Save to flash */
    Shell_Print("Saving to flash...\r\n");
    if (SysParam_Save() == SYSPARAM_OK) {
        Shell_Print("Factory reset completed successfully!\r\n");
        Shell_Print("Default graph loaded with 14 nodes and 13 edges.\r\n");
        Shell_Print("\r\nPlease reboot to apply changes.\r\n");
        Shell_Print("Use: sys -b\r\n");
    } else {
        Shell_Print("ERROR: Failed to save parameters!\r\n");
        return -1;
    }
    
    Shell_Print("========================================\r\n\r\n");
    return 0;
}

/* LCD control command */
static int sys_console(int argc, char *argv[])
{
    if (argc < 1)
    {
        Shell_Printf("LCD Console: %s\r\n", Shell_ConsoleIsEnabled() ? "ON" : "OFF");
        Shell_Print("Usage: sys -c <on|off|clear>\r\n");
        return 0;
    }
    
    if (strcmp(argv[0], "on") == 0 || strcmp(argv[0], "1") == 0)
    {
        Shell_ConsoleEnable(TRUE);
        Shell_Print("LCD Console enabled\r\n");
    }
    else if (strcmp(argv[0], "off") == 0 || strcmp(argv[0], "0") == 0)
    {
        Shell_ConsoleEnable(FALSE);
        Shell_Print("LCD Console disabled\r\n");
    }
    else if (strcmp(argv[0], "clear") == 0 || strcmp(argv[0], "clr") == 0)
    {
        Shell_ConsoleClear();
        Shell_Print("Console cleared\r\n");
    }
    else
    {
        Shell_Printf("Unknown: %s\r\n", argv[0]);
        return -1;
    }
    return 0;
}

/* DBG output to LCD console switch */
static int sys_dbglcd(int argc, char *argv[])
{
    if (argc < 1)
    {
        Shell_Printf("DBG to LCD: %s\r\n", Shell_DbgToLcdIsEnabled() ? "ON" : "OFF");
        Shell_Print("Usage: sys -d <on|off>\r\n");
        return 0;
    }
    
    if (strcmp(argv[0], "on") == 0 || strcmp(argv[0], "1") == 0)
    {
        Shell_DbgToLcdEnable(TRUE);
        Shell_Print("DBG to LCD enabled\r\n");
    }
    else if (strcmp(argv[0], "off") == 0 || strcmp(argv[0], "0") == 0)
    {
        Shell_DbgToLcdEnable(FALSE);
        Shell_Print("DBG to LCD disabled\r\n");
    }
    else
    {
        Shell_Printf("Unknown: %s\r\n", argv[0]);
        return -1;
    }
    return 0;
}

static int sys_io(int argc, char *argv[])
{
    ShellIOType_t active_io;
    ShellIOState_t state;
    
    if (argc < 1)
    {
        active_io = ShellIOManager_GetActiveIO();
        state = ShellIOManager_GetState();
        
        Shell_Printf("Active IO: %s\r\n", ShellIOManager_GetIOName(active_io));
        Shell_Printf("State: %s\r\n", 
                     state == SHELL_IO_STATE_IDLE ? "IDLE" :
                     state == SHELL_IO_STATE_ACTIVE ? "ACTIVE" : "LOCKED");
        Shell_Print("Usage: sys -o <cdc|ble|lock|unlock>\r\n");
        return 0;
    }
    
    if (strcmp(argv[0], "cdc") == 0)
    {
        if (ShellIOManager_SwitchIO(SHELL_IO_CDC))
        {
            Shell_Print("Switched to CDC\r\n");
        }
        else
        {
            Shell_Print("Failed: IO is locked\r\n");
            return -1;
        }
    }
    else if (strcmp(argv[0], "ble") == 0)
    {
        if (ShellIOManager_SwitchIO(SHELL_IO_BLE))
        {
            Shell_Print("Switched to BLE\r\n");
        }
        else
        {
            Shell_Print("Failed: IO is locked\r\n");
            return -1;
        }
    }
    else if (strcmp(argv[0], "lock") == 0)
    {
        active_io = ShellIOManager_GetActiveIO();
        if (ShellIOManager_TryLock(active_io))
        {
            Shell_Print("IO locked\r\n");
        }
        else
        {
            Shell_Print("Lock failed\r\n");
            return -1;
        }
    }
    else if (strcmp(argv[0], "unlock") == 0)
    {
        ShellIOManager_Unlock();
        Shell_Print("IO unlocked\r\n");
    }
    else
    {
        Shell_Printf("Unknown: %s\r\n", argv[0]);
        return -1;
    }
    return 0;
}

static int sys_rotate(int argc, char *argv[])
{
    static uint8_t current_rotation = 0;
    
    if (argc < 1)
    {
        /* Show current rotation angle */
        Shell_Printf("Current rotation: %d掳 (", current_rotation * 90);
        switch (current_rotation)
        {
            case 0: Shell_Print("Portrait)\r\n"); break;
            case 1: Shell_Print("Landscape)\r\n"); break;
            case 2: Shell_Print("Portrait Inverted)\r\n"); break;
            case 3: Shell_Print("Landscape Inverted)\r\n"); break;
        }
        Shell_Print("\r\nUsage:\r\n");
        Shell_Print("  sys -r 0    - 0掳 (Portrait)\r\n");
        Shell_Print("  sys -r 1    - 90掳 (Landscape)\r\n");
        Shell_Print("  sys -r 2    - 180掳 (Portrait Inverted)\r\n");
        Shell_Print("  sys -r 3    - 270掳 (Landscape Inverted)\r\n");
        return 0;
    }
    
    /* Set rotation angle */
    int rotation = atoi(argv[0]);
    if (rotation < 0 || rotation > 3)
    {
        Shell_Print("Error: Rotation must be 0-3\r\n");
        return -1;
    }
    
    current_rotation = (uint8_t)rotation;
#if HW_DRV_LCD_EN
    Lcd_SetRotation(current_rotation);
    Shell_Printf("Screen rotated to %d掳\r\n", current_rotation * 90);
    
    /* Clear screen after rotation */
    BG_lcd.Clear(BLACK);
#else
    Shell_Print("LCD not enabled\r\n");
#endif
    
    return 0;
}

static int sys_chipid(int argc, char *argv[])
{
    uint64_t chip_id = 0;
    
    (void)argc; (void)argv;
    
    Chip_IDGet(&chip_id);
    
    Shell_Printf("\r\nChip Unique ID:\r\n");
    Shell_Printf("  Full ID:   0x%016llX\r\n", chip_id);
    Shell_Printf("  Bits 0-15:  0x%04X\r\n", (uint16_t)(chip_id & 0xFFFF));
    Shell_Printf("  Bits 16-31: 0x%04X\r\n", (uint16_t)((chip_id >> 16) & 0xFFFF));
    Shell_Printf("  Bits 32-47: 0x%04X\r\n", (uint16_t)((chip_id >> 32) & 0xFFFF));
    Shell_Printf("  Bits 48-63: 0x%04X (Used in BT/BLE name)\r\n\r\n", (uint16_t)((chip_id >> 48) & 0xFFFF));
    
    return 0;
}

static const ShellOpt_t sys_opts[] = {
    OPT("i", "info",    NULL,      "Show system info",     sys_info),
    OPT("m", "mem",     NULL,      "Show memory status",   sys_mem),
    OPT("t", "tasks",   NULL,      "List running tasks",   sys_tasks),
    OPT("u", "uptime",  NULL,      "Show uptime",          sys_uptime),
    OPT("b", "reboot",  NULL,      "Reboot system",        sys_reboot),
    OPT("f", "factory", NULL,      "Factory reset (restore defaults)", sys_factory_reset),
    OPT("o", "io",      "[cmd]",   "IO control (cdc/ble/lock/unlock)", sys_io),
    OPT("c", "console", "[cmd]",   "LCD console (on/off/clear)",       sys_console),
    OPT("d", "dbglcd",  "[cmd]",   "DBG to LCD (on/off)",              sys_dbglcd),
    OPT("r", "rotate_distr",  "[0-3]",   "Rotate screen (0/1/2/3 = 0掳/90掳/180掳/270掳)", sys_rotate),
    OPT("s", "chipid",  NULL,      "Show chip unique ID",  sys_chipid),
    OPT_END()
};

DEFINE_MODULE(sys, "System information", MOD_CAT_SYSTEM, sys_opts);

/*============================================================================
 * audio module - Audio control
 *===========================================================================*/

/* 鍚変粬1闊抽噺 */
static int audio_guitar1_vol(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Printf("Guitar1 volume: %d\r\n", SYSPARAM_AUDIO()->guitar1_volume);
        return 0;
    }
    int vol = atoi(argv[0]);
    if(vol < 0) vol = 0;
    if(vol > 100) vol = 100;
    AudioSetting_SetGuitar1VolumePercent(vol);
    SYSPARAM_AUDIO()->guitar1_volume = vol;
    SysParam_MarkModified();
    Shell_Printf("Guitar1 volume: %d\r\n", vol);
    return 0;
}
/* 鍚変粬2闊抽噺 */
static int audio_guitar2_vol(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Printf("Guitar2 volume: %d\r\n",SYSPARAM_AUDIO()->guitar2_volume);
        return 0;
    }
    int vol = atoi(argv[0]);
    if(vol < 0) vol = 0;
    if(vol > 100) vol = 100;
    AudioSetting_SetGuitar2VolumePercent(vol);
    SYSPARAM_AUDIO()->guitar2_volume = vol;
    SysParam_MarkModified();
    Shell_Printf("Guitar2 volume: %d\r\n", vol);
    return 0;
}
/* 楹﹀厠椋�闊抽噺 */
static int audio_mic1_vol(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Printf("Mic1 volume: %d\r\n", SYSPARAM_AUDIO()->mic1_volume);
        return 0;
    }
    int vol = atoi(argv[0]);
    if(vol < 0) vol = 0;
    if(vol > 100) vol = 100;
    AudioSetting_SetMic1VolumePercent(vol);
    SYSPARAM_AUDIO()->mic1_volume = vol;
    SysParam_MarkModified();
    Shell_Printf("Mic1 volume: %d\r\n", vol);
    return 0;
}
/* 楹﹀厠椋�闊抽噺 */
static int audio_mic2_vol(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Printf("Mic2 volume: %d\r\n", SYSPARAM_AUDIO()->mic2_volume);
        return 0;
    }
    int vol = atoi(argv[0]);
    if(vol < 0) vol = 0;
    if(vol > 100) vol = 100;
    AudioSetting_SetMic2VolumePercent(vol);
    SYSPARAM_AUDIO()->mic2_volume = vol;
    SysParam_MarkModified();
    Shell_Printf("Mic2 volume: %d\r\n", vol);
    return 0;
}

/* 杈撳嚭闊抽噺 */
static int audio_output_vol(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Printf("Output volume: %d\r\n", SYSPARAM_AUDIO()->output_volume);
        return 0;
    }
    
    int vol = atoi(argv[0]);
    if(vol < 0) vol = 0;
    if(vol > 100) vol = 100;
    SYSPARAM_AUDIO()->output_volume = vol;
    SysParam_MarkModified();
    Shell_Printf("Output volume: %d\r\n", vol);
    return 0;
}

static int audio_mute(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Print("Usage: audio -m <0|1>\r\n");
        return -1;
    }
    int m = atoi(argv[0]);
    if(m){
        AudioDAC_Pause(DAC0);
    }else{
        AudioDAC_Run(DAC0);
    }
    Shell_Printf("Mute: %s\r\n", m ? "ON" : "OFF");
    return 0;
}





static const ShellOpt_t audio_opts[] = {
    OPT("g1", "guitar1",  "<0-100>",  "Guitar1 volume",        audio_guitar1_vol),
    OPT("g2", "guitar2",  "<0-100>",  "Guitar2 volume",        audio_guitar2_vol),
    OPT("m1", "mic1",     "<0-100>",  "Mic1 volume",           audio_mic1_vol),
    OPT("m2", "mic2",     "<0-100>",  "Mic2 volume",           audio_mic2_vol),
    OPT("o", "output",  "<0-100>",  "Output volume",        audio_output_vol),
    OPT("m", "mute",    "<0|1>",    "Mute on/off",          audio_mute),
    OPT("S", "save",    NULL,       "Save audio params",    audio_save_param),
    OPT_END()
};

DEFINE_MODULE(audio, "Audio control", MOD_CAT_PARAM, audio_opts);

/*============================================================================
 * chain module - Effect Graph Architecture (Multi-source, Graph-based)
 *===========================================================================*/

static const char* node_type_names[] = {"SOURCE", "EFFECT", "MIXER", "OUTPUT"};
static const char* source_type_names[] = {"Guitar", "Mic", "BT", "USB", "Line"};
static const char* effect_type_names[] = {
    "None", "EQ", "Comp", "Reverb", "Delay", "Chorus", 
    "Dist", "Wah", "Flanger", "Phaser", "Tremolo"
};
static const char* output_type_names[] = {"HP", "SPK", "LineOut"};

static uint8_t g_current_graph = 0;  /* Current working graph */

/* Helper: Allocate node from pool */
static int alloc_node_id(void) {
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    int i;
    for (i = 0; i < MAX_GRAPH_NODES; i++) {
        if (!(ac->node_used_mask & (1 << i))) {
            ac->node_used_mask |= (1 << i);
            return i;
        }
    }
    return -1;
}

/* Helper: Free node from pool */
static void free_node_id(uint8_t id) {
    if (id < MAX_GRAPH_NODES) {
        SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
        ac->node_used_mask &= ~(1 << id);
        memset(&ac->node_pool[id], 0, sizeof(GraphNode_t));
    }
}

/* Helper: Get current graph */
static EffectGraph_t* get_current_graph(void) {
    if (g_current_graph >= SYSPARAM_AUDIOCHAIN()->graph_count) {
        return NULL;
    }
    return &SYSPARAM_AUDIOCHAIN()->graphs[g_current_graph];
}

/* Helper: Find graph by name */
static int find_graph_by_name(const char *name) {
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    int i;
    for (i = 0; i < ac->graph_count; i++) {
        if (strcmp(ac->graphs[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* Helper: Validate graph (all sources must lead to outputs) */
static int validate_graph(EffectGraph_t *graph) {
    /* TODO: Implement graph validation - DFS from each source to find output */
    /* For now, just check basic structure */
    if (graph->node_count == 0) return 0;  /* Empty graph is valid */
    
    uint8_t has_source = 0, has_output = 0;
    GraphNode_t *nodes = SYSPARAM_AUDIOCHAIN()->node_pool;
    int i;
    
    for (i = 0; i < graph->node_count; i++) {
        uint8_t nid = graph->node_ids[i];
        if (nodes[nid].node_type == NODE_TYPE_SOURCE) has_source = 1;
        if (nodes[nid].node_type == NODE_TYPE_OUTPUT) has_output = 1;
    }
    
    if (has_source && !has_output) {
        Shell_Print("Error: Source nodes without output!\r\n");
        return -1;
    }
    
    return 0;
}

/* graph list - List all graphs */
static int chain_graph_list(int argc, char *argv[]) {
    (void)argc; (void)argv;
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    int i;
    
    Shell_Printf("=== Effect Graphs (%d/%d) ===\r\n", ac->graph_count, MAX_EFFECT_GRAPHS);
    Shell_Printf("HP uses: Graph %d\r\n", ac->active_graph_hp);
    Shell_Printf("SPK uses: Graph %d\r\n", ac->active_graph_spk);
    Shell_Printf("Current: Graph %d\r\n\r\n", g_current_graph);
    
    for (i = 0; i < ac->graph_count; i++) {
        EffectGraph_t *g = &ac->graphs[i];
        Shell_Printf("[%d] %s - %d nodes, %d edges%s\r\n",
                    i, g->name, g->node_count, g->edge_count,
                    (i == g_current_graph) ? " *" : "");
    }
    return 0;
}

/* graph create <name> - Create new graph */
static int chain_graph_create(int argc, char *argv[]) {
    if (argc < 1) {
        Shell_Print("Usage: chain graph create <name>\r\n");
        return -1;
    }
    
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    if (ac->graph_count >= MAX_EFFECT_GRAPHS) {
        Shell_Printf("Max graphs reached (%d)\r\n", MAX_EFFECT_GRAPHS);
        return -1;
    }
    
    if (find_graph_by_name(argv[0]) >= 0) {
        Shell_Print("Graph name already exists\r\n");
        return -1;
    }
    
    uint8_t idx = ac->graph_count++;
    EffectGraph_t *g = &ac->graphs[idx];
    memset(g, 0, sizeof(EffectGraph_t));
    strncpy(g->name, argv[0], GRAPH_NAME_LEN - 1);
    memset(g->node_ids, 0xFF, MAX_GRAPH_NODES);
    
    SysParam_MarkModified();
    Shell_Printf("Created graph %d: %s\r\n", idx, g->name);
    return 0;
}

/* graph delete <name> - Delete graph */
static int chain_graph_delete(int argc, char *argv[]) {
    if (argc < 1) {
        Shell_Print("Usage: chain graph delete <name>\r\n");
        return -1;
    }
    
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    
    /* Cannot delete if only one graph left */
    if (ac->graph_count <= 1) {
        Shell_Print("Cannot delete last graph\r\n");
        return -1;
    }
    
    int idx = find_graph_by_name(argv[0]);
    if (idx < 0) {
        Shell_Print("Graph not found\r\n");
        return -1;
    }
    
    EffectGraph_t *g = &ac->graphs[idx];
    int i;
    
    /* Free all nodes in this graph */
    for (i = 0; i < g->node_count; i++) {
        free_node_id(g->node_ids[i]);
    }
    
    /* Shift remaining graphs */
    for (i = idx; i < ac->graph_count - 1; i++) {
        memcpy(&ac->graphs[i], &ac->graphs[i + 1], sizeof(EffectGraph_t));
    }
    ac->graph_count--;
    
    /* Update active graph indices */
    if (ac->active_graph_hp == idx) ac->active_graph_hp = 0;
    else if (ac->active_graph_hp > idx) ac->active_graph_hp--;
    
    if (ac->active_graph_spk == idx) ac->active_graph_spk = 0;
    else if (ac->active_graph_spk > idx) ac->active_graph_spk--;
    
    if (g_current_graph == idx) g_current_graph = 0;
    else if (g_current_graph > idx) g_current_graph--;
    
    /* If only one graph left, both HP and SPK use it */
    if (ac->graph_count == 1) {
        ac->active_graph_hp = 0;
        ac->active_graph_spk = 0;
    }
    
    SysParam_MarkModified();
    Shell_Print("Graph deleted\r\n");
    return 0;
}

/* graph select <hp|spk> <name> - Select graph for HP or SPK */
static int chain_graph_select(int argc, char *argv[]) {
    if (argc < 2) {
        Shell_Print("Usage: chain graph select <hp|spk> <name>\r\n");
        return -1;
    }
    
    int idx = find_graph_by_name(argv[1]);
    if (idx < 0) {
        Shell_Print("Graph not found\r\n");
        return -1;
    }
    
    if (strcmp(argv[0], "hp") == 0) {
        SYSPARAM_AUDIOCHAIN()->active_graph_hp = idx;
        Shell_Printf("HP uses graph %d\r\n", idx);
        /* effect_graph removed */
    } else if (strcmp(argv[0], "spk") == 0) {
        SYSPARAM_AUDIOCHAIN()->active_graph_spk = idx;
        Shell_Printf("SPK uses graph %d\r\n", idx);
        /* effect_graph removed */
    } else {
        Shell_Print("Invalid output (hp or spk)\r\n");
        return -1;
    }
    
    SysParam_MarkModified();
    return 0;
}

/* graph use <name> - Switch current working graph */
static int chain_graph_use(int argc, char *argv[]) {
    if (argc < 1) {
        Shell_Printf("Current graph: %d (%s)\r\n", 
                    g_current_graph,
                    SYSPARAM_AUDIOCHAIN()->graphs[g_current_graph].name);
        return 0;
    }
    
    int idx = find_graph_by_name(argv[0]);
    if (idx < 0) {
        Shell_Print("Graph not found\r\n");
        return -1;
    }
    
    g_current_graph = idx;
    Shell_Printf("Switched to graph %d: %s\r\n", idx, argv[0]);
    return 0;
}

/* Helper: Get short name for node */
static void get_node_short_name(GraphNode_t *n, char *buf, int max_len) {
    if (n->node_type == NODE_TYPE_SOURCE) {
        snprintf(buf, max_len, "%s", source_type_names[n->subtype]);
    } else if (n->node_type == NODE_TYPE_EFFECT) {
        snprintf(buf, max_len, "%s", effect_type_names[n->subtype]);
    } else if (n->node_type == NODE_TYPE_MIXER) {
        snprintf(buf, max_len, "Mixer");
    } else if (n->node_type == NODE_TYPE_OUTPUT) {
        snprintf(buf, max_len, "%s", output_type_names[n->subtype]);
    } else {
        snprintf(buf, max_len, "N%d", 0);
    }
}

/* Helper: Find all input nodes that connect to a target node */
static int find_inputs_to_node(EffectGraph_t *g, uint8_t target, uint8_t *inputs, int max_inputs) {
    int count = 0;
    int i;
    for (i = 0; i < g->edge_count && count < max_inputs; i++) {
        if (g->edges[i].to_node == target) {
            inputs[count++] = g->edges[i].from_node;
        }
    }
    return count;
}

/* Helper: Find output node from a source */
static uint8_t find_output_from_node(EffectGraph_t *g, uint8_t source) {
    int i;
    for (i = 0; i < g->edge_count; i++) {
        if (g->edges[i].from_node == source) {
            return g->edges[i].to_node;
        }
    }
    return 0xFF;
}

/* graph show [id] - Show graph topology as ASCII art */
static int chain_graph_show(int argc, char *argv[]) {
    int idx = g_current_graph;
    int i, j;
    char name_buf[20];
    
    /* Parse graph ID */
    if (argc >= 1) {
        idx = atoi(argv[0]);
        if (idx < 0 || idx >= SYSPARAM_AUDIOCHAIN()->graph_count) {
            Shell_Print("Invalid graph ID\r\n");
            return -1;
        }
    }
    
    EffectGraph_t *g = &SYSPARAM_AUDIOCHAIN()->graphs[idx];
    GraphNode_t *nodes = SYSPARAM_AUDIOCHAIN()->node_pool;
    
    Shell_Printf("\r\n=== Graph %d: %s ===\r\n", idx, g->name);
    Shell_Printf("Nodes: %d/%d, Edges: %d/%d\r\n\r\n", g->node_count, MAX_GRAPH_NODES, g->edge_count, MAX_GRAPH_EDGES);
    
    if (g->node_count == 0) {
        Shell_Print("  (Empty graph)\r\n");
        return 0;
    }
    
    /* Print compact topology diagram - trace each source to output */
    Shell_Print("Signal Flow Paths:\r\n");
    Shell_Print("+========================================================+\r\n");
    
    /* Group nodes by type */
    uint8_t sources[8];
    int src_count = 0;
    
    for (i = 0; i < g->node_count; i++) {
        uint8_t nid = g->node_ids[i];
        GraphNode_t *n = &nodes[nid];
        if (n->node_type == NODE_TYPE_SOURCE && src_count < 8) {
            sources[src_count++] = nid;
        }
    }
    
    /* Follow each source to its destination */
    for (i = 0; i < src_count; i++) {
        uint8_t current = sources[i];
        GraphNode_t *n = &nodes[current];
        uint8_t visited[32];
        int visit_count = 0;
        
        get_node_short_name(n, name_buf, sizeof(name_buf));
        Shell_Printf("  N%-2d %-10s", current, name_buf);
        visited[visit_count++] = current;
        
        /* Follow the chain */
        int depth = 0;
        while (depth < 20) {
            uint8_t next = find_output_from_node(g, current);
            if (next == 0xFF) break;
            
            /* Check for cycles */
            int is_visited = 0;
            for (j = 0; j < visit_count; j++) {
                if (visited[j] == next) {
                    is_visited = 1;
                    break;
                }
            }
            if (is_visited) {
                Shell_Print(" --> [CYCLE]");
                break;
            }
            
            GraphNode_t *next_node = &nodes[next];
            get_node_short_name(next_node, name_buf, sizeof(name_buf));
            
            /* Check if next node has multiple inputs (convergence point) */
            uint8_t inputs[8];
            int input_count = find_inputs_to_node(g, next, inputs, 8);
            
            if (input_count > 1) {
                Shell_Printf(" --+-> N%-2d %-10s", next, name_buf);
            } else {
                Shell_Printf(" --> N%-2d %-10s", next, name_buf);
            }
            
            visited[visit_count++] = next;
            current = next;
            depth++;
        }
        Shell_Print("\r\n");
    }
    
    Shell_Print("+========================================================+\r\n\r\n");
    
    /* Show detailed node information */
    Shell_Print("Node Details:\r\n");
    Shell_Print("------------------------------------------------------------\r\n");
    for (i = 0; i < g->node_count; i++) {
        uint8_t nid = g->node_ids[i];
        GraphNode_t *n = &nodes[nid];
        
        get_node_short_name(n, name_buf, sizeof(name_buf));
        Shell_Printf("  N%-2d: %-10s [%s] Vol=%3d%%", 
                     nid, name_buf, 
                     n->enabled ? "ON " : "OFF", 
                     n->volume);
        
        if (n->node_type == NODE_TYPE_EFFECT && n->preset > 0) {
            Shell_Printf(" Preset=%d", n->preset);
        }
        
        /* Show connections */
        uint8_t inputs[8];
        int input_count = find_inputs_to_node(g, nid, inputs, 8);
        if (input_count > 0) {
            Shell_Print("  <--");
            for (j = 0; j < input_count; j++) {
                Shell_Printf(" N%d", inputs[j]);
                if (j < input_count - 1) Shell_Print(",");
            }
        }
        
        Shell_Print("\r\n");
    }
    
    Shell_Print("\r\n");
    return 0;
}

/* node add <type> <subtype> - Add node to current graph */
static int chain_node_add(int argc, char *argv[]) {
    if (argc < 2) {
        Shell_Print("Usage: chain node add <type> <subtype>\r\n");
        Shell_Print("Types: 0=SRC 1=FX 2=MIX 3=OUT\r\n");
        Shell_Print("SRC: 0=Guitar 1=Mic 2=BT 3=USB 4=Line\r\n");
        Shell_Print("FX: 1=EQ 2=Comp 3=Reverb 4=Delay ...\r\n");
        Shell_Print("OUT: 0=HP 1=SPK 2=LineOut\r\n");
        return -1;
    }
    
    EffectGraph_t *g = get_current_graph();
    if (!g) {
        Shell_Print("No active graph\r\n");
        return -1;
    }
    
    if (g->node_count >= MAX_GRAPH_NODES) {
        Shell_Print("Graph full\r\n");
        return -1;
    }
    
    int node_type = atoi(argv[0]);
    int subtype = atoi(argv[1]);
    
    if (node_type < 0 || node_type >= NODE_TYPE_MAX) {
        Shell_Print("Invalid node type\r\n");
        return -1;
    }
    
    int nid = alloc_node_id();
    if (nid < 0) {
        Shell_Print("Node pool full\r\n");
        return -1;
    }
    
    /* Initialize node */
    GraphNode_t *node = &SYSPARAM_AUDIOCHAIN()->node_pool[nid];
    memset(node, 0, sizeof(GraphNode_t));
    node->node_type = node_type;
    node->subtype = subtype;
    node->enabled = 1;
    node->volume = 80;
    
    /* Add to graph */
    g->node_ids[g->node_count++] = nid;
    SysParam_MarkModified();
    
    Shell_Printf("Added N%d: %s\r\n", nid, node_type_names[node_type]);
    return 0;
}

/* node del <id> - Delete node from current graph */
static int chain_node_del(int argc, char *argv[]) {
    if (argc < 1) {
        Shell_Print("Usage: chain node del <node_id>\r\n");
        return -1;
    }
    
    EffectGraph_t *g = get_current_graph();
    int i, j, found = -1;
    int nid = atoi(argv[0]);
    
    if (!g) return -1;
    
    /* Find node in graph */
    for (i = 0; i < g->node_count; i++) {
        if (g->node_ids[i] == nid) {
            found = i;
            break;
        }
    }
    
    if (found < 0) {
        Shell_Print("Node not in this graph\r\n");
        return -1;
    }
    
    /* Remove all edges in this graph */
    for (i = 0; i < g->edge_count; ) {
        if (g->edges[i].from_node == nid || g->edges[i].to_node == nid) {
            /* Shift remaining edges */
            for (j = i; j < g->edge_count - 1; j++) {
                g->edges[j] = g->edges[j + 1];
            }
            g->edge_count--;
        } else {
            i++;
        }
    }
    
    /* Remove from graph */
    for (i = found; i < g->node_count - 1; i++) {
        g->node_ids[i] = g->node_ids[i + 1];
    }
    g->node_count--;
    g->node_ids[g->node_count] = 0xFF;
    
    /* Free node */
    free_node_id(nid);
    SysParam_MarkModified();
    
    Shell_Printf("Deleted N%d\r\n", nid);
    return 0;
}

/* edge add <from> <to> - Add connection */
static int chain_edge_add(int argc, char *argv[]) {
    if (argc < 2) {
        Shell_Print("Usage: chain edge add <from_node> <to_node>\r\n");
        return -1;
    }
    
    EffectGraph_t *g = get_current_graph();
    if (!g) return -1;
    
    if (g->edge_count >= MAX_GRAPH_EDGES) {
        Shell_Print("Max edges reached\r\n");
        return -1;
    }
    
    int from = atoi(argv[0]);
    int to = atoi(argv[1]);
    int i, from_ok = 0, to_ok = 0;
    
    /* Check if nodes exist in graph */
    for (i = 0; i < g->node_count; i++) {
        if (g->node_ids[i] == from) from_ok = 1;
        if (g->node_ids[i] == to) to_ok = 1;
    }
    
    if (!from_ok || !to_ok) {
        Shell_Print("Node not in graph\r\n");
        return -1;
    }
    
    /* Check if edge already exists */
    for (i = 0; i < g->edge_count; i++) {
        if (g->edges[i].from_node == from && g->edges[i].to_node == to) {
            Shell_Print("Edge already exists\r\n");
            return -1;
        }
    }
    
    g->edges[g->edge_count].from_node = from;
    g->edges[g->edge_count].to_node = to;
    g->edge_count++;
    
    SysParam_MarkModified();
    Shell_Printf("Added edge N%d -> N%d\r\n", from, to);
    return 0;
}

/* edge del <from> <to> - Delete connection */
static int chain_edge_del(int argc, char *argv[]) {
    if (argc < 2) {
        Shell_Print("Usage: chain edge del <from_node> <to_node>\r\n");
        return -1;
    }
    
    EffectGraph_t *g = get_current_graph();
    if (!g) return -1;
    
    int from = atoi(argv[0]);
    int to = atoi(argv[1]);
    int i, j;
    
    for (i = 0; i < g->edge_count; i++) {
        if (g->edges[i].from_node == from && g->edges[i].to_node == to) {
            /* Shift remaining edges */
            for (j = i; j < g->edge_count - 1; j++) {
                g->edges[j] = g->edges[j + 1];
            }
            g->edge_count--;
            SysParam_MarkModified();
            Shell_Print("Edge deleted\r\n");
            return 0;
        }
    }
    
    Shell_Print("Edge not found\r\n");
    return -1;
}

/* chain mode <0-2> - Set output mode */
static int chain_mode(int argc, char *argv[]) {
    if (argc < 1) {
        const char *mode_str[] = {"Auto", "Headphone", "Speaker"};
        Shell_Printf("Output mode: %d (%s)\r\n", 
                    SYSPARAM_AUDIOCHAIN()->output_mode,
                    mode_str[SYSPARAM_AUDIOCHAIN()->output_mode]);
        return 0;
    }
    
    int mode = atoi(argv[0]);
    if (mode < 0 || mode > 2) {
        Shell_Print("Invalid mode (0:Auto 1:HP 2:SPK)\r\n");
        return -1;
    }
    
    SYSPARAM_AUDIOCHAIN()->output_mode = mode;
    SysParam_MarkModified();
    Shell_Printf("Output mode: %d\r\n", mode);
    return 0;
}

/* chain save - Save with validation */
static int chain_save(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
    /* effect_graph removed */
    
    /* Set this graph as active for both HP and SPK */
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    ac->active_graph_hp = 0;
    ac->active_graph_spk = 0;
    
    /* Validate all graphs */
    int i;
    for (i = 0; i < ac->graph_count; i++) {
        if (validate_graph(&ac->graphs[i]) < 0) {
            Shell_Printf("Graph %d validation failed\r\n", i);
            return -1;
        }
    }
    
    SysParam_Status_t status = SysParam_SaveModule("chain");
    if (status == SYSPARAM_OK) {
        Shell_Print("Chain params saved\r\n");
        return 0;
    }
    Shell_Printf("Save failed: %d\r\n", status);
    return -1;
}

static const ShellOpt_t chain_opts[] = {
    /* Graph management */
    OPT("l", "graph-list",    NULL,           "List all graphs",          chain_graph_list),
    OPT("c", "graph-create",  "<name>",       "Create graph",             chain_graph_create),
    OPT("d", "graph-delete",  "<name>",       "Delete graph",             chain_graph_delete),
    OPT("s", "graph-select",  "<hp|spk> <name>", "Select graph",          chain_graph_select),
    OPT("u", "graph-use",     "<name>",       "Switch graph",             chain_graph_use),
    OPT("w", "graph-show",    "[id]",         "Show graph topology",      chain_graph_show),
    
    /* Node management */
    OPT("n", "node-add",      "<type> <sub>", "Add node",                 chain_node_add),
    OPT("r", "node-del",      "<id>",         "Delete node",              chain_node_del),
    
    /* Edge management */
    OPT("e", "edge-add",      "<from> <to>",  "Add edge",                 chain_edge_add),
    OPT("x", "edge-del",      "<from> <to>",  "Delete edge",              chain_edge_del),
    
    /* Config */
    OPT("m", "mode",          "<0-2>",        "Output mode",              chain_mode),
    OPT("S", "save",          NULL,           "Save (validate)",          chain_save),
    OPT_END()
};

DEFINE_MODULE(chain, "Effect Graph (Multi-source DAG)", MOD_CAT_PARAM, chain_opts);

/*============================================================================
 * gpio module - GPIO control
 *===========================================================================*/

static int gpio_read(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Print("Usage: gpio -r <pin>\r\n");
        return -1;
    }
    int pin = atoi(argv[0]);
    Shell_Printf("GPIO%d = HIGH\r\n", pin);
    return 0;
}

static int gpio_write(int argc, char *argv[])
{
    if(argc < 2)
    {
        Shell_Print("Usage: gpio -w <pin> <0|1>\r\n");
        return -1;
    }
    int pin = atoi(argv[0]);
    int val = atoi(argv[1]);
    Shell_Printf("GPIO%d = %s\r\n", pin, val ? "HIGH" : "LOW");
    return 0;
}

static int gpio_toggle(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Print("Usage: gpio -t <pin>\r\n");
        return -1;
    }
    int pin = atoi(argv[0]);
    Shell_Printf("GPIO%d toggled\r\n", pin);
    return 0;
}

static const ShellOpt_t gpio_opts[] = {
    OPT("r", "read",    "<pin>",        "Read GPIO",        gpio_read),
    OPT("w", "write",   "<pin> <0|1>",  "Write GPIO",       gpio_write),
    OPT("t", "toggle",  "<pin>",        "Toggle GPIO",      gpio_toggle),
    OPT_END()
};

DEFINE_MODULE(gpio, "GPIO control", MOD_CAT_HARDWARE, gpio_opts);

/*============================================================================
 * lcd module - LCD control
 *===========================================================================*/

static int lcd_on(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("LCD ON\r\n");
    return 0;
}

static int lcd_off(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("LCD OFF\r\n");
    return 0;
}

static int lcd_bl(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Printf("Contrast: %d%%\r\n", SYSPARAM_LCD()->contrast);
        return 0;
    }
    int bl = atoi(argv[0]);
    if(bl < 0) bl = 0;
    if(bl > 100) bl = 100;
    SYSPARAM_LCD()->contrast = (uint8_t)bl;
    SysParam_MarkModified();
    Shell_Printf("Contrast: %d%%\r\n", bl);
    return 0;
}

/* 鑳屾櫙棰滆壊 */
static int lcd_bgcolor(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Printf("BG Color: 0x%04X\r\n", SYSPARAM_LCD()->bg_color);
        return 0;
    }
    
    uint16_t color = (uint16_t)strtol(argv[0], NULL, 16);
    SYSPARAM_LCD()->bg_color = color;
    SysParam_MarkModified();

    /* BG_UI removed - color saved to param only */

    Shell_Printf("BG Color: 0x%04X (Saved)\r\n", color);
    return 0;
}

static const ShellOpt_t lcd_opts[] = {
    OPT("o", "on",      NULL,       "Turn on LCD",       lcd_on),
    OPT("f", "off",     NULL,       "Turn off LCD",      lcd_off),
    OPT("b", "bl",      "<0-100>",  "Set contrast",      lcd_bl),
    OPT("c", "color",   "<0xRRGG>", "Set BG color",      lcd_bgcolor),
    OPT("S", "save",    NULL,       "Save LCD params",   lcd_save_param),
    OPT_END()
};

DEFINE_MODULE(lcd, "LCD control", MOD_CAT_HARDWARE, lcd_opts);

/*============================================================================
 * led module - LED control
 *===========================================================================*/

static int led_on(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("LED ON\r\n");
    return 0;
}

static int led_off(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("LED OFF\r\n");
    return 0;
}

static int led_blink(int argc, char *argv[])
{
    int ms = 500;
    if(argc > 0) ms = atoi(argv[0]);
    Shell_Printf("LED blinking: %dms\r\n", ms);
    return 0;
}

static const ShellOpt_t led_opts[] = {
    OPT("o", "on",      NULL,       "LED on",               led_on),
    OPT("f", "off",     NULL,       "LED off",              led_off),
    OPT("b", "blink",   "[ms]",     "Blink (default 500ms)", led_blink),
    OPT_END()
};

DEFINE_MODULE(led, "LED control", MOD_CAT_HARDWARE, led_opts);

/*============================================================================
 * dbg module - Debug commands
 *===========================================================================*/

static int dbg_echo(int argc, char *argv[])
{
	uint32_t i;
    for(i = 0; i < argc; i++)
    {
        Shell_Print(argv[i]);
        if(i < argc - 1) Shell_Print(" ");
    }
    Shell_NewLine();
    return 0;
}

static int dbg_dump(int argc, char *argv[])
{
    if(argc < 2)
    {
        Shell_Print("Usage: dbg -d <addr> <len>\r\n");
        return -1;
    }
    uint32_t i ,j;
    uint32_t addr = strtoul(argv[0], NULL, 0);
    uint32_t len = strtoul(argv[1], NULL, 0);
    if(len > 256) len = 256;
    
    uint8_t *p = (uint8_t *)addr;
    Shell_Printf("\r\nDump 0x%08X (%u bytes):\r\n", (unsigned)addr, (unsigned)len);
    
    for(i = 0; i < len; i += 16)
    {
        Shell_Printf("%08X: ", (unsigned)(addr + i));
        for(j = 0; j < 16 && (i + j) < len; j++)
            Shell_Printf("%02X ", p[i + j]);
        Shell_Print(" ");
        for(j = 0; j < 16 && (i + j) < len; j++)
        {
            char c = p[i + j];
            Shell_Printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
        }
        Shell_NewLine();
    }
    return 0;
}

static int dbg_poke(int argc, char *argv[])
{
    if(argc < 2)
    {
        Shell_Print("Usage: dbg -p <addr> <value>\r\n");
        return -1;
    }
    uint32_t addr = strtoul(argv[0], NULL, 0);
    uint32_t val = strtoul(argv[1], NULL, 0);
    *(volatile uint32_t *)addr = val;
    Shell_Printf("Write 0x%08X -> [0x%08X]\r\n", (unsigned)val, (unsigned)addr);
    return 0;
}

static int dbg_peek(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Print("Usage: dbg -k <addr>\r\n");
        return -1;
    }
    uint32_t addr = strtoul(argv[0], NULL, 0);
    uint32_t val = *(volatile uint32_t *)addr;
    Shell_Printf("[0x%08X] = 0x%08X\r\n", (unsigned)addr, (unsigned)val);
    return 0;
}

static const ShellOpt_t dbg_opts[] = {
    OPT("e", "echo",    "<text...>",        "Echo text",        dbg_echo),
    OPT("d", "dump",    "<addr> <len>",     "Memory dump",      dbg_dump),
    OPT("p", "poke",    "<addr> <val>",     "Write memory",     dbg_poke),
    OPT("k", "peek",    "<addr>",           "Read memory",      dbg_peek),
    OPT_END()
};

DEFINE_MODULE(dbg, "Debug tools", MOD_CAT_DEBUG, dbg_opts);

/*============================================================================
 * flash module - Flash storage management
 *===========================================================================*/

static int flash_info_cmd(int argc, char *argv[])
{
    (void)argc; (void)argv;
    BG_FlashMgr.PrintInfo();
    return 0;
}

static int flash_test_cmd(int argc, char *argv[])
{
    FlashDevice_t  *dev;
    FlashDevInfo_t  info;
    FlashStatus_t   ret;
    uint8_t  dev_id = 0;
    uint8_t  write_buf[256];
    uint8_t  read_buf[256];
    uint32_t test_addr;
    uint32_t cp_addr;   /* cross-page test address */
    int      i;
    int      pass_count = 0;
    int      fail_at;

    if (argc >= 1) {
        dev_id = (uint8_t)atoi(argv[0]);
    }

    Shell_Printf("Testing device %d...\r\n\r\n", dev_id);

    /* ---- 前置检查 ------------------------------------------- */
    if (!BG_FlashMgr.IsReady()) {
        Shell_Print("  ERROR: FlashMgr not ready!\r\n");
        return -1;
    }

    dev = FlashBus_GetDeviceById(dev_id);
    if (dev == NULL) {
        Shell_Printf("  ERROR: Device %d not found on bus!\r\n", dev_id);
        return -1;
    }
    if (!dev->initialized) {
        Shell_Printf("  ERROR: Device %d not initialized!\r\n", dev_id);
        return -1;
    }

    /* ---- 读取并打印设备信息 ---------------------------------- */
    if (dev->ops->get_info(dev, &info) != FLASH_OK) {
        Shell_Print("  ERROR: Cannot read device info!\r\n");
        return -1;
    }

    Shell_Printf("  Device  : %-12s  MFG=0x%02X  MemType=0x%02X  DevID=0x%02X\r\n",
                 dev->name, info.mfg_id, info.mem_type, info.dev_id);
    Shell_Printf("  Size    : %lu MB   Page=%lu B   Sector=%lu B   Block=%lu KB\r\n",
                 (unsigned long)(info.total_size >> 20),
                 (unsigned long)info.page_size,
                 (unsigned long)info.sector_size,
                 (unsigned long)(info.block_size >> 10));

    /* 测试区: 最后一个扇区, 不干扰有效数据 */
    test_addr = info.total_size - info.sector_size;
    Shell_Printf("  TestAddr: 0x%06lX  (last sector, %lu KB from start)\r\n\r\n",
                 (unsigned long)test_addr,
                 (unsigned long)(test_addr >> 10));

    /* ======================================================
     * [1/4]  Erase sector + blank verify
     * ====================================================== */
    Shell_Print("  [1/4] Erase + blank verify         ");
    ret = FlashDev_EraseSector(dev, test_addr);
    if (ret != FLASH_OK) {
        Shell_Printf("[FAIL] erase ret=%d\r\n", (int)ret);
        goto summary;
    }
    ret = FlashDev_Read(dev, test_addr, read_buf, 256);
    if (ret != FLASH_OK) {
        Shell_Printf("[FAIL] read after erase ret=%d\r\n", (int)ret);
        goto summary;
    }
    fail_at = -1;
    for (i = 0; i < 256; i++) {
        if (read_buf[i] != 0xFF) { fail_at = i; break; }
    }
    if (fail_at >= 0) {
        Shell_Printf("[FAIL] buf[%d]=0x%02X (expected 0xFF)\r\n",
                     fail_at, read_buf[fail_at]);
    } else {
        Shell_Print("[PASS]\r\n");
        pass_count++;
    }

    /* ======================================================
     * [2/4]  Single-byte write / read
     * ====================================================== */
    Shell_Print("  [2/4] Single-byte write/read       ");
    write_buf[0] = 0xA5;
    ret = FlashDev_Write(dev, test_addr, write_buf, 1);
    if (ret != FLASH_OK) {
        Shell_Printf("[FAIL] write ret=%d\r\n", (int)ret);
        goto summary;
    }
    read_buf[0] = 0x00;
    ret = FlashDev_Read(dev, test_addr, read_buf, 1);
    if (ret != FLASH_OK) {
        Shell_Printf("[FAIL] read ret=%d\r\n", (int)ret);
        goto summary;
    }
    if (read_buf[0] != 0xA5) {
        Shell_Printf("[FAIL] wrote=0xA5 read=0x%02X\r\n", read_buf[0]);
    } else {
        Shell_Print("[PASS]\r\n");
        pass_count++;
    }

    /* ======================================================
     * [3/4]  256-byte page write / verify (re-erase first)
     * ====================================================== */
    Shell_Print("  [3/4] 256-byte page write/verify   ");
    FlashDev_EraseSector(dev, test_addr);
    for (i = 0; i < 256; i++) {
        write_buf[i] = (uint8_t)i;
    }
    ret = FlashDev_Write(dev, test_addr, write_buf, 256);
    if (ret != FLASH_OK) {
        Shell_Printf("[FAIL] write ret=%d\r\n", (int)ret);
        goto summary;
    }
    memset(read_buf, 0, sizeof(read_buf));
    ret = FlashDev_Read(dev, test_addr, read_buf, 256);
    if (ret != FLASH_OK) {
        Shell_Printf("[FAIL] read ret=%d\r\n", (int)ret);
        goto summary;
    }
    fail_at = -1;
    for (i = 0; i < 256; i++) {
        if (read_buf[i] != (uint8_t)i) { fail_at = i; break; }
    }
    if (fail_at >= 0) {
        Shell_Printf("[FAIL] buf[%d] wrote=0x%02X read=0x%02X\r\n",
                     fail_at, (uint8_t)fail_at, read_buf[fail_at]);
    } else {
        Shell_Print("[PASS]\r\n");
        pass_count++;
    }

    /* ======================================================
     * [4/4]  Cross-page write / verify
     *        写 256 字节起始于 test_addr+384, 横跨本扇区内
     *        第 1 页(+256)与第 2 页(+512)的分界 (+384 = mid)
     *        此区域未被前面测试写过, 无需额外擦除
     * ====================================================== */
    Shell_Print("  [4/4] Cross-page write/verify      ");
    cp_addr = test_addr + 384;
    for (i = 0; i < 256; i++) {
        write_buf[i] = (uint8_t)(0xA0 + (i & 0x0F));
    }
    ret = FlashDev_Write(dev, cp_addr, write_buf, 256);
    if (ret != FLASH_OK) {
        Shell_Printf("[FAIL] write ret=%d\r\n", (int)ret);
        goto summary;
    }
    memset(read_buf, 0, sizeof(read_buf));
    ret = FlashDev_Read(dev, cp_addr, read_buf, 256);
    if (ret != FLASH_OK) {
        Shell_Printf("[FAIL] read ret=%d\r\n", (int)ret);
        goto summary;
    }
    fail_at = -1;
    for (i = 0; i < 256; i++) {
        if (read_buf[i] != (uint8_t)(0xA0 + (i & 0x0F))) { fail_at = i; break; }
    }
    if (fail_at >= 0) {
        Shell_Printf("[FAIL] buf[%d] wrote=0x%02X read=0x%02X\r\n",
                     fail_at,
                     (uint8_t)(0xA0 + (fail_at & 0x0F)),
                     read_buf[fail_at]);
    } else {
        Shell_Print("[PASS]\r\n");
        pass_count++;
    }

summary:
    Shell_Printf("\r\n  Result: %d/4 tests passed  [%s]\r\n",
                 pass_count,
                 (pass_count == 4) ? "ALL PASSED" : "FAILED");
    return (pass_count == 4) ? 0 : -1;
}

static int flash_read_cmd(int argc, char *argv[])
{
    uint32_t offset, len;
    uint8_t buffer[256];
    int32_t ret;
    uint32_t i;
    if (argc < 2) {
        Shell_Print("Usage: flash -r <offset> <len>\r\n");
        return -1;
    }
    
    offset = strtoul(argv[0], NULL, 0);
    len = atoi(argv[1]);
    
    if (len > 256) {
        len = 256;
    }
    
    Shell_Printf("Reading %d bytes from Looper offset 0x%X...\r\n", len, offset);
    
    ret = BG_FlashMgr.ReadLooper(offset, buffer, len);
    if (ret != BG_FLASH_OK) {
        Shell_Printf("Read failed: %d\r\n", ret);
        return -1;
    }
    
    /* Print data */
    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            Shell_Printf("\r\n%06X: ", offset + i);
        }
        Shell_Printf("%02X ", buffer[i]);
    }
    Shell_Print("\r\n\r\n");
    
    return 0;
}

static int flash_erase_cmd(int argc, char *argv[])
{
    uint32_t offset;
    int32_t ret;
    
    if (argc < 1) {
        Shell_Print("Usage: flash -e <offset>\r\n");
        return -1;
    }
    
    offset = strtoul(argv[0], NULL, 0);
    
    Shell_Printf("Erasing Looper sector at 0x%X...\r\n", offset);
    
    ret = BG_FlashMgr.EraseLooperSector(offset);
    if (ret == BG_FLASH_OK) {
        Shell_Print("Erase OK\r\n");
    } else {
        Shell_Printf("Erase failed: %d\r\n", ret);
    }
    
    return 0;
}

static int flash_format_cmd(int argc, char *argv[])
{
    uint8_t dev_id = 0;
    int32_t ret;
    
    if (argc >= 1) {
        dev_id = (uint8_t)atoi(argv[0]);
    }
    
    Shell_Printf("WARNING: Formatting device %d, all data will be lost!\r\n", dev_id);
    Shell_Print("Press Ctrl+C to cancel...\r\n");
    
    /* Simple delay */
    vTaskDelay(1000);
    
    Shell_Print("Formatting...\r\n");
    ret = BG_FlashMgr.Format(dev_id);
    
    if (ret == BG_FLASH_OK) {
        Shell_Print("Format completed\r\n");
    } else {
        Shell_Printf("Format failed: %d\r\n", ret);
    }
    
    return 0;
}

static int flash_status_cmd(int argc, char *argv[])
{
    BG_FlashMgrStatus_t status;
    int32_t ret;
    
    (void)argc; (void)argv;
    
    ret = BG_FlashMgr.GetStatus(&status);
    if (ret != BG_FLASH_OK) {
        Shell_Print("Failed to get status\r\n");
        return -1;
    }
    
    Shell_Print("\r\n=== Flash Status ===\r\n");
    
    Shell_Print("\r\nFlash #0 (System + Looper):\r\n");
    Shell_Printf("  Ready:     %s\r\n", status.flash0.ready ? "YES" : "NO");
    Shell_Printf("  Device ID: %d\r\n", status.flash0.device_id);
    Shell_Printf("  Size:      %d MB\r\n", status.flash0.total_size / (1024*1024));
    Shell_Printf("  Errors:    %d\r\n", status.flash0.error_count);
    
    Shell_Print("\r\nFlash #1 (Storage):\r\n");
    Shell_Printf("  Ready:     %s\r\n", status.flash1.ready ? "YES" : "NO");
    Shell_Printf("  Device ID: %d\r\n", status.flash1.device_id);
    Shell_Printf("  Size:      %d MB\r\n", status.flash1.total_size / (1024*1024));
    Shell_Printf("  Errors:    %d\r\n", status.flash1.error_count);
    
    Shell_Print("\r\nPartitions:\r\n");
    Shell_Print("  System:    1 MB\r\n");
    Shell_Printf("  Looper:    7 MB (%d KB free)\r\n", BG_FlashMgr.GetLooperFreeSpace() / 1024);
    Shell_Printf("  Storage:   %d MB (%d KB free)\r\n",status.flash0.total_size , BG_FlashMgr.GetStorageFreeSpace() / 1024);
    Shell_Print("\r\n");
    
    return 0;
}

static const ShellOpt_t flash_opts[] = {
    OPT("i", "info",    NULL,           "Show flash info",           flash_info_cmd),
    OPT("s", "status",  NULL,           "Show flash status",         flash_status_cmd),
    OPT("t", "test",    "[dev]",        "Test device (0 or 1)",      flash_test_cmd),
    OPT("r", "read",    "<off> <len>",  "Read from Looper",          flash_read_cmd),
    OPT("e", "erase",   "<offset>",     "Erase Looper sector",       flash_erase_cmd),
    OPT("f", "format",  "[dev]",        "Format device (0 or 1)",    flash_format_cmd),
    OPT_END()
};

DEFINE_MODULE(flash, "Flash storage management", MOD_CAT_HARDWARE, flash_opts);

/*============================================================================
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD
=======
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
 * sd module - SD Card block-level read/write test
 *===========================================================================*/

#include "sd_card.h"
#include "sd_card_driver.h"
#include "flash_bus.h"
#include "hal_sdio.h"

static int sd_require_info(HAL_SD_CardInfo_t *info)
{
    HAL_SD_Error_t ret;

    ret = HAL_SD_GetInfo(info);
    if (ret != HAL_SD_OK || info->block_count == 0) {
        Shell_Printf("ERROR: SD card not ready (err=%d)\r\n", (int)ret);
        return -1;
    }

    if (info->block_size != SD_CARD_BLOCK_SIZE) {
        Shell_Printf("ERROR: Unsupported block size %lu\r\n",
                     (unsigned long)info->block_size);
        return -1;
    }

    return 0;
}

static int sd_info(int argc, char *argv[])
{
    SD_CARD *card;
    HAL_SD_CardInfo_t info;
    HAL_SD_Error_t ret;
    (void)argc; (void)argv;

    card = SDCard_GetCardInfo();
    if (!card) {
        Shell_Print("ERROR: Cannot get SD card info\r\n");
        return -1;
    }

    Shell_Print("\r\n=== SD Card Info ===\r\n");
    Shell_Printf("  Init State: %d", card->CardInit);
    switch (card->CardInit) {
        case SD_NOINIT:      Shell_Print(" (NOT INIT)\r\n"); break;
        case SD_CONTROLER_INIT: Shell_Print(" (CTRL INIT)\r\n"); break;
        case SD_IDLE:        Shell_Print(" (IDLE)\r\n"); break;
        case SD_READY:       Shell_Print(" (READY)\r\n"); break;
        case SD_IDENT:       Shell_Print(" (IDENT)\r\n"); break;
        case SD_STANDBY:     Shell_Print(" (STANDBY)\r\n"); break;
        case SD_INITED:      Shell_Print(" (INITED)\r\n"); break;
        default:             Shell_Print(" (UNKNOWN)\r\n"); break;
    }
    Shell_Printf("  Card Type:  %d (%s)\r\n", card->CardType,
                 card->IsSDHC ? "SDHC" : "SDSC");
    Shell_Printf("  BlockNum:   %lu\r\n", (unsigned long)card->BlockNum);
    Shell_Printf("  Capacity:   %lu MB\r\n",
                 (unsigned long)(card->BlockNum / 2048));
    Shell_Printf("  MaxSpeed:   %d\r\n", card->MaxTransSpeed);

    ret = HAL_SD_GetInfo(&info);
    if (ret == HAL_SD_OK) {
        Shell_Print("\r\n=== HAL SD Info ===\r\n");
        Shell_Printf("  Type:       %s\r\n",
                     info.type == HAL_SD_CARD_TYPE_SDHC ? "SDHC" :
                     info.type == HAL_SD_CARD_TYPE_SDXC ? "SDXC" :
                     info.type == HAL_SD_CARD_TYPE_SDSC ? "SDSC" : "UNKNOWN");
        Shell_Printf("  Blocks:     %lu\r\n", (unsigned long)info.block_count);
        Shell_Printf("  Block Size: %lu\r\n", (unsigned long)info.block_size);
        Shell_Printf("  Capacity:   %lu MB\r\n",
                     (unsigned long)(info.capacity_bytes / (1024u * 1024u)));
    } else {
        Shell_Printf("\r\nHAL_SD_GetInfo failed: %d\r\n", (int)ret);
    }
    Shell_Print("\r\n");
    return 0;
}

static int sd_read(int argc, char *argv[])
{
    uint32_t block;
    uint8_t  count = 1;
    uint8_t  buf[SD_CARD_BLOCK_SIZE];
    HAL_SD_Error_t ret;
    HAL_SD_CardInfo_t info;
    uint32_t i, j;

    if (argc < 1) {
        Shell_Print("Usage: sd -r <block> [count]\r\n");
        return -1;
    }
    block = strtoul(argv[0], NULL, 0);
    if (argc >= 2) {
        int c = atoi(argv[1]);
        if (c < 1) c = 1;
        if (c > 8) c = 8;
        count = (uint8_t)c;
    }

    if (sd_require_info(&info) != 0) {
        return -1;
    }

    if (block > info.block_count || (uint32_t)count > (info.block_count - block)) {
        Shell_Printf("ERROR: Block range %lu + %u exceeds max %lu\r\n",
                     (unsigned long)block, (unsigned)count,
                     (unsigned long)info.block_count);
        return -1;
    }

    for (i = 0; i < count; i++) {
        ret = HAL_SD_ReadBlocks(block + i, buf, 1);
        if (ret != HAL_SD_OK) {
            Shell_Printf("ERROR: Read block %lu failed, err=%d\r\n",
                         (unsigned long)(block + i), (int)ret);
            return -1;
        }

        Shell_Printf("\r\nBlock %lu (0x%lX):\r\n",
                     (unsigned long)(block + i),
                     (unsigned long)((block + i) * SD_CARD_BLOCK_SIZE));
        for (j = 0; j < SD_CARD_BLOCK_SIZE; j += 16) {
            uint32_t k;
            Shell_Printf("%04lX: ", (unsigned long)j);
            for (k = 0; k < 16; k++) {
                Shell_Printf("%02X ", buf[j + k]);
            }
            Shell_Print(" ");
            for (k = 0; k < 16; k++) {
                char c = buf[j + k];
                Shell_Printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
            }
            Shell_NewLine();
        }
    }
    return 0;
}

static int sd_write(int argc, char *argv[])
{
    uint32_t block;
    uint8_t  pattern = 0xA5;
    uint8_t  buf[SD_CARD_BLOCK_SIZE];
    uint8_t  verify[SD_CARD_BLOCK_SIZE];
    HAL_SD_Error_t ret;
    HAL_SD_CardInfo_t info;
    uint32_t i;
    int fail_at;

    if (argc < 1) {
        Shell_Print("Usage: sd -w <block> [pattern]\r\n");
        Shell_Print("  Writes test pattern to block, then reads back to verify\r\n");
        Shell_Print("  WARNING: This overwrites data on the specified block!\r\n");
        return -1;
    }
    block = strtoul(argv[0], NULL, 0);
    if (argc >= 2) {
        pattern = (uint8_t)strtoul(argv[1], NULL, 0);
    }

    if (sd_require_info(&info) != 0) {
        return -1;
    }

    if (block >= info.block_count) {
        Shell_Printf("ERROR: Block %lu exceeds max %lu\r\n",
                     (unsigned long)block, (unsigned long)info.block_count);
        return -1;
    }

    /* Fill buffer with pattern */
    for (i = 0; i < SD_CARD_BLOCK_SIZE; i++) {
        buf[i] = (uint8_t)(pattern ^ (i & 0xFF));
    }

    Shell_Printf("Writing block %lu, pattern=0x%02X...\r\n",
                 (unsigned long)block, pattern);
    ret = HAL_SD_WriteBlocks(block, buf, 1);
    if (ret != HAL_SD_OK) {
        Shell_Printf("ERROR: Write failed, err=%d\r\n", (int)ret);
        return -1;
    }
    Shell_Print("Write OK, reading back...\r\n");

    ret = HAL_SD_ReadBlocks(block, verify, 1);
    if (ret != HAL_SD_OK) {
        Shell_Printf("ERROR: Read back failed, err=%d\r\n", (int)ret);
        return -1;
    }

    fail_at = -1;
    for (i = 0; i < SD_CARD_BLOCK_SIZE; i++) {
        if (verify[i] != buf[i]) {
            fail_at = (int)i;
            break;
        }
    }

    if (fail_at >= 0) {
        Shell_Printf("VERIFY FAIL at offset %d: wrote=0x%02X read=0x%02X\r\n",
                     fail_at, buf[fail_at], verify[fail_at]);
        return -1;
    }

    Shell_Print("Verify OK! Read/write test passed.\r\n");
    return 0;
}

static int sd_test(int argc, char *argv[])
{
    uint8_t  original[SD_CARD_BLOCK_SIZE];
    uint8_t  buf[SD_CARD_BLOCK_SIZE];
    uint8_t  verify[SD_CARD_BLOCK_SIZE];
    uint8_t  multi_buf[SD_CARD_BLOCK_SIZE * 4];
    HAL_SD_Error_t ret;
    HAL_SD_CardInfo_t info;
    uint32_t block;
    uint32_t i;
    int pass_count = 0;
    int fail_at;
    int restore_needed = 0;
    int result = -1;

    (void)argc; (void)argv;

    if (sd_require_info(&info) != 0) {
        return -1;
    }

    /* Use a block near the end to avoid corrupting important data */
    block = info.block_count - 1;

    Shell_Printf("\r\n=== SD Card R/W Test (block %lu) ===\r\n\r\n",
                 (unsigned long)block);

    /* [1/3] Read test */
    Shell_Print("  [1/3] Read block              ");
    ret = HAL_SD_ReadBlocks(block, original, 1);
    if (ret != HAL_SD_OK) {
        Shell_Printf("[FAIL] err=%d\r\n", (int)ret);
        goto done;
    }
    Shell_Print("[PASS]\r\n");
    pass_count++;

    /* [2/3] Write + verify */
    Shell_Print("  [2/3] Write + verify          ");
    for (i = 0; i < SD_CARD_BLOCK_SIZE; i++) {
        buf[i] = (uint8_t)(0x5A ^ (i & 0xFF));
    }
    ret = HAL_SD_WriteBlocks(block, buf, 1);
    if (ret != HAL_SD_OK) {
        Shell_Printf("[FAIL] write err=%d\r\n", (int)ret);
        goto done;
    }
    restore_needed = 1;
    ret = HAL_SD_ReadBlocks(block, verify, 1);
    if (ret != HAL_SD_OK) {
        Shell_Printf("[FAIL] read err=%d\r\n", (int)ret);
        goto done;
    }
    fail_at = -1;
    for (i = 0; i < SD_CARD_BLOCK_SIZE; i++) {
        if (verify[i] != buf[i]) { fail_at = (int)i; break; }
    }
    if (fail_at >= 0) {
        Shell_Printf("[FAIL] offset %d: wrote=0x%02X read=0x%02X\r\n",
                     fail_at, buf[fail_at], verify[fail_at]);
        goto done;
    }
    Shell_Print("[PASS]\r\n");
    pass_count++;

    /* [3/3] Multi-block read */
    Shell_Print("  [3/3] Multi-block read (4)    ");
    if (info.block_count >= 4) {
        ret = HAL_SD_ReadBlocks(block - 3, multi_buf, 4);
        if (ret != HAL_SD_OK) {
            Shell_Printf("[FAIL] err=%d\r\n", (int)ret);
            goto done;
        }
        Shell_Print("[PASS]\r\n");
        pass_count++;
    } else {
        Shell_Print("[SKIP] card too small\r\n");
    }

done:
    if (restore_needed) {
        ret = HAL_SD_WriteBlocks(block, original, 1);
        if (ret != HAL_SD_OK) {
            Shell_Printf("\r\n  WARNING: Restore block failed, err=%d\r\n", (int)ret);
        } else {
            Shell_Print("  Test block restored\r\n");
        }
    }

    if (pass_count == 3) {
        result = 0;
    }
    Shell_Printf("\r\n  Result: %d/3 tests passed  [%s]\r\n",
                 pass_count, (result == 0) ? "ALL PASSED" : "FAILED");
    return result;
}

static int sd_diag(int argc, char *argv[])
{
    uint8_t buf[SD_CARD_BLOCK_SIZE];
    uint32_t sector = 0;
    uint32_t i;
    HAL_SD_Error_t ret;

    (void)argc; (void)argv;

    ret = HAL_SD_ReadBlocks(0, buf, 1);
    if (ret != HAL_SD_OK) {
        Shell_Printf("ERROR: read LBA0 failed, err=%d\r\n", (int)ret);
        return -1;
    }

    Shell_Print("\r\n=== SD LBA0 Diagnostic ===\r\n");
    Shell_Printf("  sig[510..511]: %02X %02X\r\n", buf[510], buf[511]);
    Shell_Printf("  first bytes: ");
    for (i = 0; i < 16; i++) {
        Shell_Printf("%02X ", buf[i]);
    }
    Shell_Print("\r\n");

    if (buf[510] == 0x55 && buf[511] == 0xAA &&
        buf[0] != 0xEB && buf[0] != 0xE9) {
        Shell_Print("  MBR partition table:\r\n");
        for (i = 0; i < 4; i++) {
            uint8_t *p = &buf[0x1BE + i * 16u];
            uint32_t lba = (uint32_t)p[8] | ((uint32_t)p[9] << 8) |
                           ((uint32_t)p[10] << 16) | ((uint32_t)p[11] << 24);
            uint32_t nsec = (uint32_t)p[12] | ((uint32_t)p[13] << 8) |
                            ((uint32_t)p[14] << 16) | ((uint32_t)p[15] << 24);
            Shell_Printf("    [%lu] type=0x%02X lba=%lu sectors=%lu\r\n",
                         (unsigned long)i, p[4],
                         (unsigned long)lba, (unsigned long)nsec);
            if (sector == 0 && p[4] != 0 && lba != 0) {
                sector = lba;
            }
        }
    }

    if (sector != 0) {
        ret = HAL_SD_ReadBlocks(sector, buf, 1);
        if (ret != HAL_SD_OK) {
            Shell_Printf("ERROR: read BPB LBA%lu failed, err=%d\r\n",
                         (unsigned long)sector, (int)ret);
            return -1;
        }
        Shell_Printf("  partition first bytes @LBA%lu: ", (unsigned long)sector);
        for (i = 0; i < 64; i++) {
            Shell_Printf("%02X ", buf[i]);
            if ((i & 0x0F) == 0x0F && i != 63) {
                Shell_Print("\r\n                             ");
            }
        }
        Shell_Print("\r\n");
    }

    if (buf[0] == 0xEB || buf[0] == 0xE9) {
        uint16_t bps = (uint16_t)buf[0x0B] | ((uint16_t)buf[0x0C] << 8);
        uint8_t spc = buf[0x0D];
        uint16_t rsvd = (uint16_t)buf[0x0E] | ((uint16_t)buf[0x0F] << 8);
        uint8_t fats = buf[0x10];
        uint32_t total32 = (uint32_t)buf[0x20] | ((uint32_t)buf[0x21] << 8) |
                           ((uint32_t)buf[0x22] << 16) | ((uint32_t)buf[0x23] << 24);
        uint32_t fatsz32 = (uint32_t)buf[0x24] | ((uint32_t)buf[0x25] << 8) |
                           ((uint32_t)buf[0x26] << 16) | ((uint32_t)buf[0x27] << 24);
        char fs[9];
        for (i = 0; i < 8; i++) {
            fs[i] = (buf[0x52 + i] >= 0x20 && buf[0x52 + i] < 0x7F) ? (char)buf[0x52 + i] : '.';
        }
        fs[8] = '\0';
        Shell_Printf("  BPB LBA: %lu\r\n", (unsigned long)sector);
        Shell_Printf("  bytes/sector=%u sectors/cluster=%u reserved=%u fats=%u\r\n",
                     bps, spc, rsvd, fats);
        Shell_Printf("  total32=%lu fatsz32=%lu fs='%s'\r\n",
                     (unsigned long)total32, (unsigned long)fatsz32, fs);
    } else if (memcmp(&buf[3], "EXFAT   ", 8) == 0) {
        Shell_Printf("  BPB LBA: %lu\r\n", (unsigned long)sector);
        Shell_Print("  exFAT detected (not supported by current FAT library)\r\n");
    } else {
        Shell_Print("  No BPB jump signature found\r\n");
    }

    return 0;
}

static const ShellOpt_t sd_opts[] = {
    OPT("i", "info",  NULL,            "Show SD card info",          sd_info),
    OPT("r", "read",  "<blk> [cnt]",   "Read block(s) hex dump",    sd_read),
    OPT("w", "write", "<blk> [pat]",   "Write test pattern + verify",sd_write),
    OPT("t", "test",  NULL,            "Quick R/W test (last block)",sd_test),
    OPT("d", "diag",  NULL,            "Diagnose MBR/BPB",          sd_diag),
    OPT_END()
};

DEFINE_MODULE(sd, "SD Card block R/W test", MOD_CAT_HARDWARE, sd_opts);

/*============================================================================
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
 * Module registration
 *===========================================================================*/

 static int raw_battry_info(int argc, char *argv[])
 {
	(void)argc; (void)argv;
	uint16_t battery_mv;

      battery_mv = battery_get_volt_mv();
	Shell_Printf(" Bat Voltage = { %u mV }\r\n", (unsigned)battery_mv);
	return 0;
 }

 static int battry_val_info(int argc, char *argv[])
{
	 uint16_t  battery_soc;
	 battery_soc = battery_get_soc();
	 Shell_Printf(" Bat's SOC = { %d }\r\n", battery_soc);
	 return 0;
}

static const ShellOpt_t battery_opts[] = {
    OPT("r", "raw_bat",    NULL,      "Show raw battry adc value",     raw_battry_info),
    OPT("v", "bat_val",    NULL,      "Show battry persent val",     battry_val_info),
    OPT_END()
};
DEFINE_MODULE(battery, "battery controller", MOD_CAT_HARDWARE, battery_opts);
 static int  bt_get_staus(int argc, char *argv[])
{

     
     char *status[] ={
        "None",
        "Connecting",
        "Connected",
        "Streaming"
     };
     Shell_Printf(" Bluetooth state is{ %s } \r\n", status[GetA2dpState()]);
	 return 0;
}

/* 钃濈墮鍚嶇О璁剧疆 */
static int bt_set_name(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Printf("BT Name: %s\r\n", SYSPARAM_BLUETOOTH()->device_name);
        return 0;
    }
    
    /* 澶嶅埗鍚嶇О锛屾渶澶�5涓瓧绗︼紙淇濈暀\0锛�*/
    strncpy(SYSPARAM_BLUETOOTH()->device_name, argv[0], 15);
    SYSPARAM_BLUETOOTH()->device_name[15] = '\0';
    SysParam_MarkModified();
    Shell_Printf("BT Name set to: %s\r\n", SYSPARAM_BLUETOOTH()->device_name);
    Shell_Print("Note: Restart required to apply\r\n");
    return 0;
}

/* A2DP闊抽噺 */
static int bt_a2dp_vol(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Printf("A2DP Volume: %d\r\n", SYSPARAM_BLUETOOTH()->a2dp_volume);
        return 0;
    }
    
    int vol = atoi(argv[0]);
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    SYSPARAM_BLUETOOTH()->a2dp_volume = (uint8_t)vol;
    SysParam_MarkModified();
    Shell_Printf("A2DP Volume: %d\r\n", vol);
    return 0;
}

static const ShellOpt_t bt_opts[] = {
    OPT("s", "state",    NULL,      "Show bt status",       bt_get_staus),
    OPT("n", "name",     "[name]",  "Get/Set BT name",      bt_set_name),
    OPT("v", "a2dpvol",  "<0-100>", "A2DP volume",          bt_a2dp_vol),
    OPT("S", "save",     NULL,      "Save bt params",       bt_save_param),
    OPT_END()
};

DEFINE_MODULE(bt, "Bluetooth controller", MOD_CAT_HARDWARE, bt_opts);

/*============================================================================
 * ble module - BLE controller
 *===========================================================================*/

static int ble_get_status(int argc, char *argv[])
{
    (void)argc; (void)argv;
    /* TODO: 浠嶣LE绠＄悊鍣ㄨ幏鍙栧疄闄呯姸鎬�*/
    Shell_Print("BLE Status: Idle\r\n");
    return 0;
}

static const ShellOpt_t ble_opts[] = {
    OPT("s", "state",    NULL,      "Show BLE status",    ble_get_status),
    OPT_END()
};

DEFINE_MODULE(ble, "BLE controller", MOD_CAT_HARDWARE, ble_opts);

/*============================================================================
 * File system navigation commands (ls, pwd, cd, cat)
 *===========================================================================*/

#if FAT32_EN
static uint8_t  g_shell_sd_mode = 0;
static uint32_t g_shell_sd_cwd_cluster = 0;
static char     g_shell_sd_cwd_path[128] = "/sd";

static int shell_sd_mount(void)
{
    BG_ERR ret = FAT32_Init();
    if (ret != SUCCESS) {
        Shell_Printf("sd: FAT mount failed: %d\r\n", ret);
        return -1;
    }
    if (g_shell_sd_cwd_cluster == 0) {
        g_shell_sd_cwd_cluster = FAT32_GetRootCluster();
    }
    return 0;
}

static const char* shell_sd_strip_prefix(const char *path)
{
    if (!path) return "";
    if (strcmp(path, "/sd") == 0) return "";
    if (strncmp(path, "/sd/", 4) == 0) return path + 4;
    return path;
}

static int shell_sd_resolve_parent(const char *path, uint32_t *dir_cluster, const char **name)
{
    static char buf[128];
    char *p;
    char *slash;
    uint32_t cluster;
    FAT32_FileInfo_t info;
    BG_ERR ret;

    if (!dir_cluster || !name) return -1;
    if (shell_sd_mount() != 0) return -1;

    path = shell_sd_strip_prefix(path);
    cluster = (path && path[0] == '/') ? FAT32_GetRootCluster() : g_shell_sd_cwd_cluster;
    if (path && path[0] == '/') path++;
    if (!path) path = "";

    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    p = buf;

    while (1) {
        slash = strchr(p, '/');
        if (!slash) {
            *dir_cluster = cluster;
            *name = p;
            return 0;
        }
        *slash = '\0';
        if (p[0] != '\0') {
            ret = FAT32_FindEntryInDir(cluster, p, &info);
            if (ret != SUCCESS || !(info.attr & DIR_ATTR_DIRECTORY)) {
                return -1;
            }
            cluster = info.start_cluster;
        }
        p = slash + 1;
    }
}

static int shell_sd_list_cb(const FAT32_FileInfo_t *info, void *user)
{
    (void)user;
    Shell_Printf("%c %-24.24s %lu\r\n",
                 (info->attr & DIR_ATTR_DIRECTORY) ? 'd' : '-',
                 info->name,
                 (unsigned long)info->size);
    return 0;
}

static int shell_sd_ls(const char *path)
{
    uint32_t cluster;
    const char *name;
    FAT32_FileInfo_t info;
    BG_ERR ret;

    if (shell_sd_mount() != 0) return -1;
    if (!path || strcmp(path, "/sd") == 0 || strcmp(path, "") == 0) {
        cluster = g_shell_sd_cwd_cluster;
    } else {
        if (shell_sd_resolve_parent(path, &cluster, &name) != 0) return -1;
        if (name[0] != '\0') {
            ret = FAT32_FindEntryInDir(cluster, name, &info);
            if (ret != SUCCESS) return -1;
            if (!(info.attr & DIR_ATTR_DIRECTORY)) {
                Shell_Printf("%s\r\n", info.name);
                return 0;
            }
            cluster = info.start_cluster;
        }
    }

    Shell_Print("Attr Name                     Size\r\n");
    Shell_Print("-------------------------------------\r\n");
    ret = FAT32_ListDirByCluster(cluster, shell_sd_list_cb, NULL);
    return (ret == SUCCESS) ? 0 : -1;
}

static int shell_sd_cd(const char *path)
{
    uint32_t cluster;
    const char *name;
    FAT32_FileInfo_t info;
    BG_ERR ret;

    if (shell_sd_mount() != 0) return -1;
    if (!path || strcmp(path, "/sd") == 0 || strcmp(path, "/sd/") == 0) {
        g_shell_sd_mode = 1;
        g_shell_sd_cwd_cluster = FAT32_GetRootCluster();
        strcpy(g_shell_sd_cwd_path, "/sd");
        return 0;
    }
    if (strcmp(path, "/") == 0) {
        g_shell_sd_mode = 0;
        return 0;
    }
    if (strcmp(path, "..") == 0) {
        g_shell_sd_mode = 0;
        g_shell_sd_cwd_cluster = FAT32_GetRootCluster();
        strcpy(g_shell_sd_cwd_path, "/sd");
        return 0;
    }

    if (shell_sd_resolve_parent(path, &cluster, &name) != 0) return -1;
    ret = FAT32_FindEntryInDir(cluster, name, &info);
    if (ret != SUCCESS || !(info.attr & DIR_ATTR_DIRECTORY)) return -1;
    g_shell_sd_mode = 1;
    g_shell_sd_cwd_cluster = info.start_cluster;
    if (strncmp(path, "/sd", 3) == 0) {
        strncpy(g_shell_sd_cwd_path, path, sizeof(g_shell_sd_cwd_path) - 1);
        g_shell_sd_cwd_path[sizeof(g_shell_sd_cwd_path) - 1] = '\0';
    } else {
        if (strcmp(g_shell_sd_cwd_path, "/sd") != 0) {
            strncat(g_shell_sd_cwd_path, "/", sizeof(g_shell_sd_cwd_path) - strlen(g_shell_sd_cwd_path) - 1);
        } else {
            strncat(g_shell_sd_cwd_path, "/", sizeof(g_shell_sd_cwd_path) - strlen(g_shell_sd_cwd_path) - 1);
        }
        strncat(g_shell_sd_cwd_path, name, sizeof(g_shell_sd_cwd_path) - strlen(g_shell_sd_cwd_path) - 1);
    }
    return 0;
}

static int shell_sd_cat(const char *path)
{
    FAT32_FileHandle_t h;
    uint32_t cluster;
    const char *name;
    int32_t n;
    uint8_t buf[128];
    BG_ERR ret;

    if (shell_sd_resolve_parent(path, &cluster, &name) != 0) return -1;
    ret = FAT32_OpenFileInDir(cluster, name, &h);
    if (ret != SUCCESS) return -1;
    while ((n = FAT32_ReadFile(&h, buf, sizeof(buf))) > 0) {
        uint32_t i;
        for (i = 0; i < (uint32_t)n; i++) {
            Shell_Printf("%c", (buf[i] >= 0x20 && buf[i] < 0x7f) ? buf[i] : '.');
        }
    }
    Shell_Print("\r\n");
    FAT32_CloseFile(&h);
    return 0;
}
#endif /* FAT32_EN */

static int cmd_ls(int argc, char *argv[])
{
    FsNode_t *node;
    int i;
    char lineBuf[256];
    int lineLen = 0;
    int itemsInLine = 0;
    
#if FAT32_EN
    if ((argc > 0 && strncmp(argv[0], "/sd", 3) == 0) || (argc == 0 && g_shell_sd_mode)) {
        return shell_sd_ls((argc > 0) ? argv[0] : "");
    }
#endif

    /* 濡傛灉鏈夊弬鏁帮紝浣跨敤鍙傛暟鏌ユ壘鑺傜偣锛涘惁鍒欎娇鐢ㄥ綋鍓嶅伐浣滅洰褰�*/
    if (argc > 0) {
        node = DrvFs_FindNode(argv[0]);
        if (!node) {
            Shell_Printf("ls: cannot access '%s': No such file or directory\r\n", argv[0]);
            return -1;
        }
    } else {
        /* 鏃犲弬鏁版椂锛屽垪鍑哄綋鍓嶅伐浣滅洰褰�*/
        node = DrvFs_GetCwd();
        if (!node) {
            Shell_Print("ls: cannot get current directory\r\n");
            return -1;
        }
    }
    
    if (node->type != FS_NODE_DIR && node->type != FS_NODE_DEV) {
        Shell_Printf("ls: '%s': Not a directory\r\n", node->name);
        return -1;
    }
    
    Shell_Printf("\r\n");
    
    lineBuf[0] = '\0';
    lineLen = 0;
    itemsInLine = 0;
    
    for (i = 0; i < node->childCount; i++) {
        FsNode_t *child = node->children[i];
        char itemBuf[64];
        
        /* 鏍煎紡鍖栧崟涓」鐩�*/
        switch (child->type) {
            case FS_NODE_DIR:
                snprintf(itemBuf, sizeof(itemBuf), "\033[1;34m%s\033[0m", child->name);
                break;
            case FS_NODE_DEV:
                snprintf(itemBuf, sizeof(itemBuf), "\033[1;32m%s\033[0m", child->name);
                break;
            case FS_NODE_PARAM:
                snprintf(itemBuf, sizeof(itemBuf), "%s", child->name);
                break;
            default:
                snprintf(itemBuf, sizeof(itemBuf), "%s", child->name);
                break;
        }
        
        /* 娣诲姞鍒拌缂撳啿 */
        if (lineLen + strlen(itemBuf) + 4 < sizeof(lineBuf)) {
            strcat(lineBuf, itemBuf);
            lineLen += strlen(itemBuf);
            itemsInLine++;
            
            /* 娣诲姞鍒嗛殧绗︽垨鎹㈣ */
            if (itemsInLine >= 2) {
                /* 杈撳嚭杩欎竴琛�*/
                Shell_Printf("%s\r\n", lineBuf);
                lineBuf[0] = '\0';
                lineLen = 0;
                itemsInLine = 0;
            } else if (i < node->childCount - 1) {
                strcat(lineBuf, "    ");  /* 4涓┖鏍煎垎闅�*/
                lineLen += 4;
            }
        }
    }
    
    /* 杈撳嚭鍓╀綑椤圭洰 */
    if (itemsInLine > 0) {
        Shell_Printf("%s\r\n", lineBuf);
    }
    
    Shell_Print("\r\n");
    return 0;
}

static const ShellOpt_t ls_opts[] = {
    OPT("", "", "[path]", "List directory contents", cmd_ls),
    OPT_END()
};

DEFINE_MODULE(ls, "List directory contents", MOD_CAT_SYSTEM, ls_opts);

static int cmd_pwd(int argc, char *argv[])
{
    char path[64];
    
    (void)argc; (void)argv;

#if FAT32_EN
    if (g_shell_sd_mode) {
        Shell_Printf("%s\r\n", g_shell_sd_cwd_path);
        return 0;
    }
#endif

    if (DrvFs_GetCwdPath(path, sizeof(path)) == FS_OK) {
        Shell_Printf("%s\r\n", path);
    } else {
        Shell_Print("pwd: error getting current directory\r\n");
        return -1;
    }
    
    return 0;
}

static const ShellOpt_t pwd_opts[] = {
    OPT("", "", NULL, "Print working directory", cmd_pwd),
    OPT_END()
};

DEFINE_MODULE(pwd, "Print working directory", MOD_CAT_SYSTEM, pwd_opts);

static int cmd_cd(int argc, char *argv[])
{
#if FAT32_EN
    if (argc >= 1 && (strncmp(argv[0], "/sd", 3) == 0 || g_shell_sd_mode)) {
        if (shell_sd_cd(argv[0]) != 0) {
            Shell_Printf("cd: %s: No such directory\r\n", argv[0]);
            return -1;
        }
        return 0;
    }
#endif

    if (argc < 1) {
        /* 鏃犲弬鏁版椂杩斿洖鏍圭洰褰�*/
        if (DrvFs_Cd("/") != FS_OK) {
            Shell_Print("cd: error\r\n");
            return -1;
        }
        return 0;
    }
    
    if (DrvFs_Cd(argv[0]) != FS_OK) {
        Shell_Printf("cd: %s: No such directory\r\n", argv[0]);
        return -1;
    }
    
    return 0;
}

static const ShellOpt_t cd_opts[] = {
    OPT("", "", "[path]", "Change directory", cmd_cd),
    OPT_END()
};

DEFINE_MODULE(cd, "Change directory", MOD_CAT_SYSTEM, cd_opts);

static int cmd_cat(int argc, char *argv[])
{
    FsNode_t *node;
    char buf[128];
    int ret;
    
    if (argc < 1) {
        Shell_Print("cat: missing operand\r\n");
        Shell_Print("Usage: cat <parameter>\r\n");
        return -1;
    }

#if FAT32_EN
    if (strncmp(argv[0], "/sd", 3) == 0 || g_shell_sd_mode) {
        if (shell_sd_cat(argv[0]) != 0) {
            Shell_Printf("cat: %s: Read error\r\n", argv[0]);
            return -1;
        }
        return 0;
    }
#endif

    node = DrvFs_FindNode(argv[0]);
    if (!node) {
        Shell_Printf("cat: %s: No such file or directory\r\n", argv[0]);
        return -1;
    }
    
    if (node->type != FS_NODE_PARAM) {
        Shell_Printf("cat: %s: Not a parameter file\r\n", argv[0]);
        return -1;
    }
    
    ret = DrvFs_ReadParam(node, buf, sizeof(buf));
    if (ret < 0) {
        if (ret == -2) {
            Shell_Printf("cat: %s: Write-only parameter (use echo to set)\r\n", argv[0]);
        } else {
            Shell_Printf("cat: %s: Read error\r\n", argv[0]);
        }
        return -1;
    }
    
    Shell_Printf("%s\r\n", buf);
    return 0;
}

static const ShellOpt_t cat_opts[] = {
    OPT("", "", "<file>", "Display parameter value", cmd_cat),
    OPT_END()
};

DEFINE_MODULE(cat, "Display file contents", MOD_CAT_SYSTEM, cat_opts);

static int cmd_write_file(int argc, char *argv[])
{
#if FAT32_EN
    uint32_t cluster;
    const char *name;
    int32_t written;

    if (argc < 2) {
        Shell_Print("Usage: write /sd/<file> <text>\r\n");
        return -1;
    }

    if (strncmp(argv[0], "/sd", 3) != 0 && !g_shell_sd_mode) {
        Shell_Print("write: only /sd paths are supported\r\n");
        return -1;
    }

    if (shell_sd_resolve_parent(argv[0], &cluster, &name) != 0 || !name || !name[0]) {
        Shell_Printf("write: bad path: %s\r\n", argv[0]);
        return -1;
    }

    written = FAT32_WriteFile(cluster, name, argv[1], strlen(argv[1]));
    if (written < 0) {
        Shell_Printf("write: failed: %s\r\n", argv[0]);
        return -1;
    }
    Shell_Printf("written %ld bytes\r\n", (long)written);
    return 0;
#else
    (void)argc; (void)argv;
    Shell_Print("write: FAT32 disabled\r\n");
    return -1;
#endif
}

static const ShellOpt_t write_opts[] = {
    OPT("", "", "<file> <text>", "Write text file", cmd_write_file),
    OPT_END()
};

DEFINE_MODULE(write, "Write file contents", MOD_CAT_SYSTEM, write_opts);

/*
 * echo鍛戒护 - 鍐欏叆鍙傛暟鍊� * 鐢ㄦ硶: echo <value> > <parameter>
 * 绠�寲鐢ㄦ硶: echo <parameter> <value>
 */
static int cmd_echo(int argc, char *argv[])
{
    FsNode_t *node;
    const char *value;
    const char *path;
    int ret;
    int redirect_idx = -1;
    int i;
    
    if (argc < 2) {
        Shell_Print("echo: missing operand\r\n");
        Shell_Print("Usage: echo <parameter> <value>\r\n");
        Shell_Print("   or: echo <value> > <parameter>\r\n");
        Shell_Print("Example: echo threshold -20\r\n");
        Shell_Print("     or: echo -20 > threshold\r\n");
        return -1;
    }
    
    /* 妫�煡鏄惁鏈夐噸瀹氬悜绗﹀彿 > */
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) {
            redirect_idx = i;
            break;
        }
    }
    
    if (redirect_idx > 0 && redirect_idx < argc - 1) {
        /* 鏍煎紡: echo <value> > <parameter> */
        value = argv[0];  /* 绗竴涓弬鏁版槸鍊�*/
        path = argv[redirect_idx + 1];  /* > 鍚庨潰鏄矾寰�*/
    } else {
        /* 绠�寲鏍煎紡: echo <parameter> <value> */
        path = argv[0];   /* 绗竴涓弬鏁版槸璺緞 */
        value = argv[1];  /* 绗簩涓弬鏁版槸鍊�*/
    }
    
    /* 鏌ユ壘鑺傜偣 */
    node = DrvFs_FindNode(path);
    if (!node) {
        Shell_Printf("echo: %s: No such file or directory\r\n", path);
        return -1;
    }
    
    if (node->type != FS_NODE_PARAM) {
        Shell_Printf("echo: %s: Not a parameter file\r\n", path);
        return -1;
    }
    
    /* 鍐欏叆鍙傛暟 */
    ret = DrvFs_WriteParam(node, value);
    if (ret < 0) {
        if (ret == -2) {
            Shell_Printf("echo: %s: Read-only parameter\r\n", path);
        } else {
            Shell_Printf("echo: %s: Write error\r\n", path);
        }
        return -1;
    }
    
    Shell_Printf("OK\r\n");
    return 0;
}

static const ShellOpt_t echo_opts[] = {
    OPT("", "", "<param> <value>", "Write parameter value", cmd_echo),
    OPT_END()
};

DEFINE_MODULE(echo, "Write to file", MOD_CAT_SYSTEM, echo_opts);

/*----------------------------------------------------------------------------
 * 閫掑綊鎵撳嵃VFS鏍戠粨鏋� *----------------------------------------------------------------------------*/
static void print_vfs_tree(VfsNode_t *node, int depth, int isLast)
{
    char prefix[32] = "";
    int i;
    for (i = 0; i < depth; i++) {
        strcat(prefix, (i == depth - 1 && isLast) ? "   " : "鈹� ");
    }
    if (depth > 0) {
        Shell_Print(prefix);
        Shell_Print(isLast ? "鈹斺攢 " : "鈹溾攢 ");
    }
    Shell_Print(node->name);
    if (node->type == VFS_NODE_DIR) Shell_Print("/");
    Shell_Print("\r\n");
    if (node->type == VFS_NODE_DIR && node->childCount > 0) {
        for (i = 0; i < node->childCount; i++) {
            print_vfs_tree(node->children[i], depth + 1, i == node->childCount - 1);
        }
    }
}

static int cmd_tree(int argc, char *argv[])
{
    (void)argc; (void)argv;
    VfsNode_t *root = Vfs_GetRoot();
    if (!root) {
        Shell_Print("VFS not initialized!\r\n");
        return -1;
    }
    print_vfs_tree(root, 0, 1);
    Shell_Print("\r\n");
    return 0;
}

static const ShellOpt_t tree_opts[] = {
    OPT("", "", NULL, "Display driver tree", cmd_tree),
    OPT_END()
};

DEFINE_MODULE(tree, "Display directory tree", MOD_CAT_SYSTEM, tree_opts);

static int cmd_drivers(int argc, char *argv[])
{
    int count;
    DrvDevice_t **devices;
    int i;
    
    (void)argc; (void)argv;
    
    devices = DrvDevice_GetList(&count);
    
    Shell_Print("\r\nRegistered Drivers:\r\n");
    Shell_Print("鈹屸攢鈹�攢鈹�攢鈹�攢鈹�攢鈹�攢鈹攢鈹�攢鈹�攢鈹�攢鈹�攢鈹攢鈹�攢鈹�攢鈹�攢鈹�攢鈹�攢鈹�攼\r\n");
    Shell_Print("鈹�Name      鈹�Bus     鈹�Status     鈹俓r\n");
    Shell_Print("鈹溾攢鈹�攢鈹�攢鈹�攢鈹�攢鈹�攢鈹尖攢鈹�攢鈹�攢鈹�攢鈹�攢鈹尖攢鈹�攢鈹�攢鈹�攢鈹�攢鈹�攢鈹�敜\r\n");
    
    for (i = 0; i < count; i++) {
        const char *bus_name;
        
        switch (devices[i]->bus) {
            case DRV_BUS_SPI: bus_name = "SPI"; break;
            case DRV_BUS_I2C: bus_name = "I2C"; break;
            case DRV_BUS_USB: bus_name = "USB"; break;
            case DRV_BUS_POWER: bus_name = "POWER"; break;
            default: bus_name = "UNKNOWN"; break;
        }
        
        Shell_Printf("鈹�%-9s 鈹�%-7s 鈹�%-10s 鈹俓r\n",
                     devices[i]->name, 
                     bus_name,
                     devices[i]->isRegistered ? "OK" : "FAIL");
    }
    
    Shell_Print("鈹斺攢鈹�攢鈹�攢鈹�攢鈹�攢鈹�攢鈹粹攢鈹�攢鈹�攢鈹�攢鈹�攢鈹粹攢鈹�攢鈹�攢鈹�攢鈹�攢鈹�攢鈹�敇\r\n");
    Shell_Printf("Total: %d drivers\r\n\r\n", count);
    return 0;
}

static const ShellOpt_t drivers_opts[] = {
    OPT("", "", NULL, "List all registered drivers", cmd_drivers),
    OPT_END()
};

DEFINE_MODULE(drivers, "List device drivers", MOD_CAT_SYSTEM, drivers_opts);

static int cmd_ble_send(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Print("ble_send: missing string operand\r\n");
        Shell_Print("Usage: ble_send <string>\r\n");
        return -1;
    }

    // 检查BLE连接状态 (使用att_server_can_send作为连接指示)
    extern int att_server_can_send(void);
    if (att_server_can_send() == 0) {
        Shell_Print("BLE not connected or CCCD not enabled\r\n");
        return -1;
    }

    // 合并所有参数为字符串
    char send_str[256] = "";
    int i;
    for (i = 0; i < argc; i++) {
        if (i > 0) strcat(send_str, " ");
        strncat(send_str, argv[i], sizeof(send_str) - strlen(send_str) - 1);
    }

    // 发送到BLE
    BLE_Send((uint8_t *)send_str, strlen(send_str));

    Shell_Printf("Sent to BLE: %s\r\n", send_str);
    return 0;
}

static const ShellOpt_t ble_send_opts[] = {
    OPT("", "", "<string>", "Send string via BLE when connected", cmd_ble_send),
    OPT_END()
};

DEFINE_MODULE(ble_send, "Send string via BLE", MOD_CAT_SYSTEM, ble_send_opts);

/*============================================================================
 * upg — 进入固件升级模式 (SDK Flash Boot)
 *===========================================================================*/
#if FLASH_BOOT_EN
extern void start_up_grate(uint32_t UpdateResource);

static int cmd_upg_enter(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("Entering firmware upgrade mode (SDK Flash Boot)...\r\n");
    Shell_Print("Device will reboot into bootloader.\r\n");
    start_up_grate(AppResourceUsbDevice);
    return 0;
}

static const ShellOpt_t upg_opts[] = {
    OPT("", "", "", "Enter firmware upgrade mode (SDK Flash Boot)", cmd_upg_enter),
    OPT_END()
};

DEFINE_MODULE(upg, "Enter firmware upgrade mode", MOD_CAT_SYSTEM, upg_opts);
#endif /* FLASH_BOOT_EN */

/*============================================================================
 * Module registration
 *===========================================================================*/
    /* 鍙傛暟淇濆瓨鍛戒护 */
    extern void ShellCmd_Param_Init(void);
    /* 鐢甸姘斿簳婀濃垂昭正功能 */
    extern void ShellCmd_BattCalib_Init(void);
void Shell_RegisterAllModules(void)
{
    REGISTER_MODULE(sys);
    REGISTER_MODULE(audio);
    REGISTER_MODULE(gpio);
    REGISTER_MODULE(lcd);
    REGISTER_MODULE(led);
    REGISTER_MODULE(dbg);
    REGISTER_MODULE(flash);
<<<<<<< Updated upstream
    REGISTER_MODULE(sd);
=======
<<<<<<< HEAD
    REGISTER_MODULE(sd);
=======
<<<<<<< HEAD
=======
    REGISTER_MODULE(sd);
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
    REGISTER_MODULE(battery);
    REGISTER_MODULE(bt);
    REGISTER_MODULE(ble);
    REGISTER_MODULE(chain);
    #if VFS_EN

    /* 鏂囦欢绯荤粺瀵艰埅鍛戒护 [SHELL_BLE] Calling Shell_Process()*/
    REGISTER_MODULE(ls);
    REGISTER_MODULE(pwd);
    REGISTER_MODULE(cd);
    REGISTER_MODULE(cat);
    REGISTER_MODULE(write);
    REGISTER_MODULE(echo);    /* 鍐欏叆鍙傛暟鍊�*/
    REGISTER_MODULE(tree);

    #endif /* VFS_EN */
    REGISTER_MODULE(drivers);
    REGISTER_MODULE(ble_send);
#if FLASH_BOOT_EN
    REGISTER_MODULE(upg);
#endif


    ShellCmd_Param_Init();       /* param 鍛戒护 */

    ShellCmd_BattCalib_Init();   /* batt calib 校准 */

    /* 提示音测试命令 */
    extern void ShellCmdRemind_Register(void);

    ShellCmdRemind_Register();   /* remind 提示音测试命令 */

    /* 硬件相关命令 - 根据板子硬件能力条件编译 */
#if HW_CMD_FAT_EN
    /* FAT32 文件系统命令 */
    extern int ShellCmdFat_Register(void);
    ShellCmdFat_Register();
#endif

#if HW_CMD_PSRAM_EN
    /* PSRAM 堆内存查看命令 */
    extern int ShellCmdPsram_Register(void);
    ShellCmdPsram_Register();
#endif

    /* 低功耗控制命令 */
    {
        extern void ShellCmdLp_Register(void);
        ShellCmdLp_Register();
    }

    /* 硬件测试命令 */
#if HW_DRV_SSD1306_EN
    {
        extern void ShellCmdHwtest_Register(void);
        ShellCmdHwtest_Register();
    }
#endif
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD

    /* 录制命令 */
#if FAT32_EN
    {
        extern void ShellCmdRec_Register(void);
        ShellCmdRec_Register();
    }
#endif
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
}

/*============================================================================
 * remind 模块 - 提示音测试
 *   remind -l / --list          列出所有已注册的提示音
 *   remind -p / --play <id>     按索引播放
 *   remind -n / --on            播放开机音（等同于 -p 0）
 *   remind -f / --off           播放关机音（等同于 -p 1）
 *===========================================================================*/

static int remind_list(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Printf("Remind sound table (%d items):\r\n", RemindSound_GetCount());
    RemindSound_ListAll();
    return 0;
}

static int remind_play_by_index(int argc, char *argv[])
{
    int id;
    if (argc < 1) {
        Shell_Print("Usage: remind -p <id>\r\n");
        Shell_Printf("  Total items: %d\r\n", RemindSound_GetCount());
        return -1;
    }
    id = atoi(argv[0]);
    Shell_Printf("Playing remind sound id=%d ...\r\n", id);
    if (RemindSound_PlayByIndex((uint8_t)id) != 0) {
        Shell_Printf("Error: id=%d not found (table has %d items)\r\n",
                     id, RemindSound_GetCount());
        return -1;
    }
    Shell_Print("Done.\r\n");
    return 0;
}

static int remind_play_on(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("Playing power-on remind sound...\r\n");
    RemindSound_PlayByName("on");
    Shell_Print("Done.\r\n");
    return 0;
}

static int remind_play_off(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("Playing power-off remind sound...\r\n");
    RemindSound_PlayByName("off");
    Shell_Print("Done.\r\n");
    return 0;
}

static const ShellOpt_t remind_opts[] = {
    OPT("l", "list",  NULL,   "List all registered remind sounds", remind_list),
    OPT("p", "play",  "<id>", "Play remind sound by index",        remind_play_by_index),
    OPT("n", "on",    NULL,   "Play power-on  sound (id=0)",       remind_play_on),
    OPT("f", "off",   NULL,   "Play power-off sound (id=1)",       remind_play_off),
    OPT_END()
};

DEFINE_MODULE(remind, "Remind sound test", MOD_CAT_DEBUG, remind_opts);

void ShellCmdRemind_Register(void)
{
    REGISTER_MODULE(remind);
}
<<<<<<< Updated upstream
=======
<<<<<<< HEAD
=======
<<<<<<< HEAD

/*============================================================================
 * rec module - Audio recorder control
 *===========================================================================*/
#if FAT32_EN
#include "bg_recorder.h"

static int rec_start(int argc, char *argv[])
{
    RecMode_t mode = REC_MODE_SINGLE_CH;
    int ret;

    if (argc >= 1) {
        if (strcmp(argv[0], "dual") == 0 || strcmp(argv[0], "d") == 0) {
            mode = REC_MODE_DUAL_CH;
        } else if (strcmp(argv[0], "single") == 0 || strcmp(argv[0], "s") == 0) {
            mode = REC_MODE_SINGLE_CH;
        }
    }

    ret = BG_Rec_Start(mode);
    if (ret != 0) {
        Shell_Printf("Start failed: %d\r\n", ret);
        return -1;
    }
    Shell_Printf("Recording started (%s)\r\n",
                 mode == REC_MODE_DUAL_CH ? "DUAL: MIC+LINE" : "SINGLE: LINE L+R");
    return 0;
}

static int rec_stop(int argc, char *argv[])
{
    (void)argc; (void)argv;
    if (!BG_Rec_IsRecording()) {
        Shell_Print("Not recording\r\n");
        return -1;
    }
    BG_Rec_Stop();
    {
        const RecInfo_t *ri = BG_Rec_GetInfo();
        uint32_t sec = ri->total_samples / REC_SAMPLE_RATE;
        Shell_Printf("Stopped: %s + %s, %lu samples (%lu sec)\r\n",
                     ri->filename_raw, ri->filename_tgt,
                     (unsigned long)ri->total_samples,
                     (unsigned long)sec);
    }
    return 0;
}

static int rec_status(int argc, char *argv[])
{
    const RecInfo_t *ri = BG_Rec_GetInfo();
    (void)argc; (void)argv;

    Shell_Printf("State: %s\r\n",
                 ri->state == REC_STATE_IDLE ? "IDLE" :
                 ri->state == REC_STATE_RECORDING ? "RECORDING" :
                 ri->state == REC_STATE_STOPPING ? "STOPPING" : "ERROR");
    Shell_Printf("Mode:  %s\r\n",
                 ri->mode == REC_MODE_DUAL_CH ? "DUAL (MIC+LINE)" : "SINGLE (LINE L+R)");
    Shell_Printf("RAW:   %s\r\n", ri->filename_raw);
    Shell_Printf("TGT:   %s\r\n", ri->filename_tgt);
    Shell_Printf("Index: %lu\r\n", (unsigned long)ri->file_index);
    if (ri->state == REC_STATE_RECORDING) {
        uint32_t sec = ri->total_samples / REC_SAMPLE_RATE;
        Shell_Printf("Time:  %02lu:%02lu\r\n",
                     (unsigned long)(sec / 60), (unsigned long)(sec % 60));
    }
    Shell_Printf("SD:    %s\r\n", ri->sd_ready ? "OK" : "NOT READY");
    Shell_Printf("Errors: %lu\r\n", (unsigned long)ri->write_errors);
    return 0;
}

static int rec_init(int argc, char *argv[])
{
    BG_ERR ret;
    (void)argc; (void)argv;

    /* Force re-init FAT32 */
    FAT32_DeInit();
    ret = FAT32_Init();
    if (ret != 0) {
        Shell_Printf("FAT32 init failed: %d\r\n", ret);
        BG_Rec_SetSDReady(false);
        return -1;
    }
    BG_Rec_SetSDReady(true);
    Shell_Print("SD card mounted OK\r\n");
    return 0;
}

static const ShellOpt_t rec_opts[] = {
    OPT("s", "start", "[single|dual]", "Start recording", rec_start),
    OPT("x", "stop",  NULL,            "Stop recording",  rec_stop),
    OPT("i", "info",  NULL,            "Show recorder status", rec_status),
    OPT("f", "init",  NULL,            "Force init SD/FAT32", rec_init),
    OPT_END()
};

DEFINE_MODULE(rec, "Audio recorder", MOD_CAT_HARDWARE, rec_opts);

void ShellCmdRec_Register(void)
{
    REGISTER_MODULE(rec);
}
#endif /* FAT32_EN */
=======
>>>>>>> 69f72477ab92a7ac337c78cbc6167910bcd3c4ac
>>>>>>> 691fcd2 (refactor(ble): 解耦跨层依赖并重构BLE同步回调)
>>>>>>> Stashed changes
