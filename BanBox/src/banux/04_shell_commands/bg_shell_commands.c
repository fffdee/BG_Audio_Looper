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
#include "audio_looper.h"
#include "shell_io_cdc.h"
#include "shell_io_ble.h"
#include "shell_io_manager.h"

#include "bg_lcd.h"
#include "BG_FlashMgr.h"
#include "flash_bus.h"

#include "gpio.h"
#include "adc.h"
#include "dac.h"
#include "bt_a2dp_api.h"
#include "battery_drv.h"
#include "drv_init.h"  /* 椹卞姩妗嗘灦鍒濆鍖�*/
#include "vfs.h"       /* 铏氭嫙鏂囦欢绯荤粺API */
#include "drv_fs.h"    /* 椹卞姩鏂囦欢绯荤粺API */
#include "drv_device.h" /* 椹卞姩璁惧绠＄悊 */
#include "chip_info.h"  /* 鑺墖ID璇诲彇 */
#include "FreeRTOS.h"
#include "task.h"

#include "bangui.h"
#include "audio_setting.h"

/* 鏁堟灉鍣ㄥ懡浠ゆā鍧�*/
#include "shell_cmd_effect.h"
#include "shell_cmd_graph.h"

/* UI鎺у埗鍛戒护妯″潡 */
#include "shell_cmd_ui.h"

/* 鍙傛暟淇濆瓨妯″潡 */
#include "sys_param.h"

/* Chain Graph Apply 妯″潡 */
#include "chain_graph_apply.h"

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

static int looper_save_param(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Shell_Print("Saving looper parameters to flash...\r\n");
    if (SysParam_SaveModule("looper") == SYSPARAM_OK) {
        Shell_Print("Looper params saved\r\n");
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
    Lcd_SetRotation(current_rotation);
    Shell_Printf("Screen rotated to %d掳\r\n", current_rotation * 90);
    
    /* Clear screen after rotation */
    BG_lcd.Clear(BLACK);
    
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
        
        // Apply the selected graph to running EffectGraph
        if (ChainGraph_ApplyToEffectGraph(idx) == 0) {
            Shell_Print("Graph applied successfully\r\n");
        } else {
            Shell_Print("Failed to apply graph\r\n");
        }
        
    } else if (strcmp(argv[0], "spk") == 0) {
        SYSPARAM_AUDIOCHAIN()->active_graph_spk = idx;
        Shell_Printf("SPK uses graph %d\r\n", idx);
        
        // Apply the selected graph to running EffectGraph
        if (ChainGraph_ApplyToEffectGraph(idx) == 0) {
            Shell_Print("Graph applied successfully\r\n");
        } else {
            Shell_Print("Failed to apply graph\r\n");
        }
        
    } else {
        Shell_Print("Invalid output (hp or spk)\r\n");
        return -1;
    }
    
    SysParam_MarkModified();
    return 0;
}

/* graph apply <name> - Apply graph to running EffectGraph */
static int chain_graph_apply(int argc, char *argv[]) {
    if (argc < 1) {
        Shell_Print("Usage: chain graph apply <name>\r\n");
        return -1;
    }
    
    int idx = find_graph_by_name(argv[0]);
    if (idx < 0) {
        Shell_Print("Graph not found\r\n");
        return -1;
    }
    
    // Apply the graph to running EffectGraph
    if (ChainGraph_ApplyToEffectGraph(idx) == 0) {
        Shell_Printf("Graph '%s' applied successfully\r\n", argv[0]);
    } else {
        Shell_Print("Failed to apply graph\r\n");
        return -1;
    }
    
    return 0;
}

/* graph save <name> - Save current EffectGraph to chain graph */
static int chain_graph_save(int argc, char *argv[]) {
    if (argc < 1) {
        Shell_Print("Usage: chain graph save <name>\r\n");
        return -1;
    }
    
    if (ChainGraph_SaveByName(argv[0]) == 0) {
        Shell_Printf("Saved current EffectGraph to chain graph '%s'\r\n", argv[0]);
        return 0;
    } else {
        Shell_Print("Failed to save graph\r\n");
        return -1;
    }
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
    
    /* First, save current EffectGraph state to default chain graph (index 0) */
    if (ChainGraph_SaveFromEffectGraph(0) != 0) {
        Shell_Print("Failed to save current EffectGraph state\r\n");
        return -1;
    }
    
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
    OPT("a", "graph-apply",   "<name>",       "Apply graph to EffectGraph", chain_graph_apply),
    OPT("v", "graph-save",    "<name>",       "Save EffectGraph to chain", chain_graph_save),
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
    

    BG_UI.SetBackgroundColor(color);
    
    Shell_Printf("BG Color: 0x%04X (Applied)\r\n", color);
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
 * looper module - Audio Looper control and test
 *===========================================================================*/

static int looper_init_cmd(int argc, char *argv[])
{
    int flash_type = 0;
    (void)argc;
    
    if (argc > 0) {
        flash_type = atoi(argv[0]);
    }
    
    if (flash_type == 0) {
        AudioLooper.InitWithFlashType(FLASH_TYPE_NOR);
        Shell_Print("Looper initialized with NOR Flash\r\n");
    } else {
        AudioLooper.InitWithFlashType(FLASH_TYPE_NAND);
        Shell_Print("Looper initialized with NAND Flash\r\n");
    }
    return 0;
}

static int looper_status_cmd(int argc, char *argv[])
{
    LoopStatus_t status;
    uint8_t i;
    const char *state_str;
    const char *seg_state_str;
    SegmentState_t seg_state;
    
    (void)argc; (void)argv;
    
    status = AudioLooper.GetStatus();
    
    switch (status.current_state) {
        case LOOP_STATE_IDLE: state_str = "IDLE"; break;
        case LOOP_STATE_RECORDING: state_str = "RECORDING"; break;
        case LOOP_STATE_PLAYING: state_str = "PLAYING"; break;
        case LOOP_STATE_RECORDING_AND_PLAYING: state_str = "REC+PLAY"; break;
        default: state_str = "UNKNOWN"; break;
    }
    
    Shell_Print("\r\n=== Looper Status ===\r\n");
    Shell_Printf("State:      %s\r\n", state_str);
    Shell_Printf("Recording:  %s\r\n", status.is_recording ? "YES" : "NO");
    Shell_Printf("Playing:    %s\r\n", status.is_playing ? "YES" : "NO");
    Shell_Printf("Flash:      %s\r\n", status.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
    Shell_Printf("Segments:   %d active\r\n", status.active_segments);
    Shell_Printf("Current:    Segment %d\r\n", status.current_segment);
    Shell_Printf("Recorded:   %lu bytes\r\n", (unsigned long)status.total_recorded_bytes);
    
    Shell_Print("\r\n--- Segment Details ---\r\n");
    for (i = 0; i < MAX_SEGMENTS; i++) {
        seg_state = loop_get_segment_state(i);
        switch (seg_state) {
            case SEGMENT_INACTIVE: seg_state_str = "INACTIVE"; break;
            case SEGMENT_RECORDING: seg_state_str = "RECORDING"; break;
            case SEGMENT_PLAYING: seg_state_str = "PLAYING"; break;
            case SEGMENT_STOPPED: seg_state_str = "STOPPED"; break;
            default: seg_state_str = "UNKNOWN"; break;
        }
        Shell_Printf("  Seg[%d]: %s\r\n", i, seg_state_str);
    }
    Shell_Print("\r\n");
    return 0;
}

static int looper_record_cmd(int argc, char *argv[])
{
    int seg = 0;
    
    if (argc > 0) {
        seg = atoi(argv[0]);
        if (seg < 0 || seg >= MAX_SEGMENTS) {
            Shell_Print("Error: segment must be 0-3\r\n");
            return -1;
        }
    }
    
    loop_set_segment_recording((uint8_t)seg);
    Shell_Printf("Segment %d: RECORDING\r\n", seg);
    return 0;
}

static int looper_play_cmd(int argc, char *argv[])
{
    int seg = 0;
    
    if (argc > 0) {
        seg = atoi(argv[0]);
        if (seg < 0 || seg >= MAX_SEGMENTS) {
            Shell_Print("Error: segment must be 0-3\r\n");
            return -1;
        }
    }
    
    loop_set_segment_playing((uint8_t)seg);
    Shell_Printf("Segment %d: PLAYING\r\n", seg);
    return 0;
}

static int looper_stop_cmd(int argc, char *argv[])
{
    int seg = 0;
    
    if (argc > 0) {
        seg = atoi(argv[0]);
        if (seg < 0 || seg >= MAX_SEGMENTS) {
            Shell_Print("Error: segment must be 0-3\r\n");
            return -1;
        }
    }
    
    loop_set_segment_stopped((uint8_t)seg);
    Shell_Printf("Segment %d: STOPPED\r\n", seg);
    return 0;
}

static int looper_btn_cmd(int argc, char *argv[])
{
    int seg;
    
    if (argc < 1) {
        Shell_Print("Usage: looper -b <0-3>\r\n");
        Shell_Print("  Simulates button press for segment\r\n");
        return -1;
    }
    
    seg = atoi(argv[0]);
    if (seg < 0 || seg >= MAX_SEGMENTS) {
        Shell_Print("Error: segment must be 0-3\r\n");
        return -1;
    }
    
    AudioLooper.SegmentButtonPress((uint8_t)seg);
    Shell_Printf("Button %d pressed\r\n", seg);
    return 0;
}

static int looper_clear_cmd(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    loop_clear_all_segments();
    Shell_Print("All segments cleared\r\n");
    return 0;
}

static int looper_reset_cmd(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    AudioLooper.Reset();
    Shell_Print("Looper reset\r\n");
    return 0;
}

static int looper_mode_cmd(int argc, char *argv[])
{
    LoopMode_t mode;
    
    if (argc < 1) {
        mode = AudioLooper.GetMode();
        Shell_Printf("Mode: %s\r\n", mode == LOOP_MODE_SONG ? "SONG" : "FREE");
        return 0;
    }
    
    if (strcmp(argv[0], "song") == 0) {
        AudioLooper.SetMode(LOOP_MODE_SONG);
        SYSPARAM_LOOPER()->loop_count = LOOP_MODE_SONG;
        SysParam_MarkModified();
        Shell_Print("Mode: SONG\r\n");
    } else if (strcmp(argv[0], "free") == 0) {
        AudioLooper.SetMode(LOOP_MODE_FREE);
        SYSPARAM_LOOPER()->loop_count = LOOP_MODE_FREE;
        SysParam_MarkModified();
        Shell_Print("Mode: FREE\r\n");
    } else {
        Shell_Print("Usage: looper -m <song|free>\r\n");
        return -1;
    }
    return 0;
}

static int looper_metro_cmd(int argc, char *argv[])
{
    int val;
    
    if (argc < 1) {
        Shell_Print("\r\n=== Metronome Status ===\r\n");
        Shell_Printf("Enabled:  %s\r\n", AudioLooper.MetronomeIsEnabled() ? "YES" : "NO");
        Shell_Printf("BPM:      %d\r\n", AudioLooper.MetronomeGetBPM());
        Shell_Printf("Beats:    %d per measure\r\n", AudioLooper.MetronomeGetBeatsPerMeasure());
        Shell_Print("\r\n");
        return 0;
    }
    
    if (strcmp(argv[0], "on") == 0) {
        metronome_enable();
        Shell_Print("Metronome: ON\r\n");
    } else if (strcmp(argv[0], "off") == 0) {
        metronome_disable();
        Shell_Print("Metronome: OFF\r\n");
    } else if (strcmp(argv[0], "toggle") == 0) {
        AudioLooper.MetronomeToggle();
        Shell_Printf("Metronome: %s\r\n", AudioLooper.MetronomeIsEnabled() ? "ON" : "OFF");
    } else if (strcmp(argv[0], "bpm") == 0 && argc > 1) {
        val = atoi(argv[1]);
        if (val >= METRONOME_MIN_BPM && val <= METRONOME_MAX_BPM) {
            AudioLooper.MetronomeSetBPM((uint16_t)val);
            SYSPARAM_LOOPER()->tempo = (uint16_t)val;
            SysParam_MarkModified();
            Shell_Printf("BPM: %d\r\n", val);
        } else {
            Shell_Printf("Error: BPM must be %d-%d\r\n", METRONOME_MIN_BPM, METRONOME_MAX_BPM);
            return -1;
        }
    } else if (strcmp(argv[0], "beats") == 0 && argc > 1) {
        val = atoi(argv[1]);
        if (val >= METRONOME_MIN_BEATS_PER_MEASURE && val <= METRONOME_MAX_BEATS_PER_MEASURE) {
            AudioLooper.MetronomeSetBeatsPerMeasure((uint8_t)val);
            SYSPARAM_LOOPER()->time_signature = (uint8_t)val;
            SysParam_MarkModified();
            Shell_Printf("Beats: %d per measure\r\n", val);
        } else {
            Shell_Printf("Error: Beats must be %d-%d\r\n", 
                        METRONOME_MIN_BEATS_PER_MEASURE, METRONOME_MAX_BEATS_PER_MEASURE);
            return -1;
        }
    } else {
        Shell_Print("Usage: looper -M <on|off|toggle|bpm N|beats N>\r\n");
        return -1;
    }
    return 0;
}

static const ShellOpt_t looper_opts[] = {
    OPT("i", "init",    "[0|1]",        "Init (0=NOR, 1=NAND)",     looper_init_cmd),
    OPT("s", "status",  NULL,           "Show looper status",       looper_status_cmd),
    OPT("r", "record",  "[seg]",        "Start recording segment",  looper_record_cmd),
    OPT("p", "play",    "[seg]",        "Play segment",             looper_play_cmd),
    OPT("t", "stop",    "[seg]",        "Stop segment",             looper_stop_cmd),
    OPT("b", "btn",     "<0-3>",        "Simulate button press",    looper_btn_cmd),
    OPT("c", "clear",   NULL,           "Clear all segments",       looper_clear_cmd),
    OPT("R", "reset",   NULL,           "Reset looper",             looper_reset_cmd),
    OPT("m", "mode",    "[song|free]",  "Get/Set loop mode",        looper_mode_cmd),
    OPT("M", "metro",   "<cmd> [val]",  "Metronome control",        looper_metro_cmd),
    OPT("S", "save",    NULL,           "Save looper params",       looper_save_param),
    OPT_END()
};

DEFINE_MODULE(looper, "Audio Looper control", MOD_CAT_HARDWARE, looper_opts);

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
    int32_t ret;
    uint8_t dev_id = 0;
    
    if (argc >= 1) {
        dev_id = (uint8_t)atoi(argv[0]);
    }
    
    Shell_Printf("Testing device %d...\r\n", dev_id);

    //FlashNewDriver_Test();
    // ret = BG_FlashMgr.TestDevice(dev_id);
    
    // if (ret == BG_FLASH_OK) {
    //     Shell_Print("Test PASSED\r\n");
    // } else {
    //     Shell_Printf("Test FAILED: %d\r\n", ret);
    // }
    return 0;
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
 * Module registration
 *===========================================================================*/

 static int raw_battry_info(int argc, char *argv[])
 {
	(void)argc; (void)argv;
	float battery_volt;

      battery_volt = battery_get_volt();
	Shell_Printf(" Bat's ADC Value = { %f }\r\n", battery_volt);
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

static int cmd_ls(int argc, char *argv[])
{
    FsNode_t *node;
    int i;
    char lineBuf[256];
    int lineLen = 0;
    int itemsInLine = 0;
    
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

/*============================================================================
 * Module registration
 *===========================================================================*/
    /* 鍙傛暟淇濆瓨鍛戒护 */
    extern void ShellCmd_Param_Init(void);
void Shell_RegisterAllModules(void)
{
    REGISTER_MODULE(sys);
    REGISTER_MODULE(audio);
    REGISTER_MODULE(gpio);
    REGISTER_MODULE(lcd);
    REGISTER_MODULE(led);
    REGISTER_MODULE(dbg);
    REGISTER_MODULE(looper);
    REGISTER_MODULE(flash);
    REGISTER_MODULE(battery);
    REGISTER_MODULE(bt);
    REGISTER_MODULE(ble);
    REGISTER_MODULE(chain);
    #ifdef VFS_EN

    /* 鏂囦欢绯荤粺瀵艰埅鍛戒护 */
    REGISTER_MODULE(ls);
    REGISTER_MODULE(pwd);
    REGISTER_MODULE(cd);
    REGISTER_MODULE(cat);
    REGISTER_MODULE(echo);    /* 鍐欏叆鍙傛暟鍊�*/
    REGISTER_MODULE(tree);

    #endif /* VFS_EN */
    REGISTER_MODULE(drivers);
    /* 鏁堟灉鍥惧拰鏁堟灉鍣ㄥ懡浠�*/
    ShellCmdEffect_Register();   /* effect 鍛戒护 */
    ShellCmdGraph_Register();    /* graph 鍜�fx 鍛戒护 */
    
    /* UI鎺у埗鍛戒护 */
    UICmd_Register();            /* ui 鍛戒护 */
    

    ShellCmd_Param_Init();       /* param 鍛戒护 */

    /* 鍒濆鍖栭┍鍔ㄦ鏋�(鍖呭惈鎵�湁椹卞姩娉ㄥ唽) */
    /* 娉ㄦ剰锛氬凡鍦╩ain()涓垵濮嬪寲锛岃繖閲屼笉鍐嶉噸澶嶈皟鐢�*/
    /* DrvFramework_FullInit(); */
}
