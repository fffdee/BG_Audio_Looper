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
#include "bg_shell.h"
#include "audio_looper.h"
#include "shell_io_cdc.h"
#include "shell_io_ble.h"
#include "shell_io_manager.h"
#include "page_manager.h"
#include "bg_lcd.h"
#include "BG_FlashMgr.h"
#include "flash_bus.h"

#include "gpio.h"
#include "adc.h"
#include "dac.h"
#include "bt_a2dp_api.h"
#include "battery_drv.h"
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
    
    Shell_Print("\r\nSystem Information:\r\n");
    Shell_Print("  Device:    BG Card Mini\r\n");
    Shell_Print("  MCU:       BP1048\r\n");
    Shell_Print("  Clock:     288MHz\r\n");
    Shell_Print("  Flash:     4MB\r\n");
    Shell_Print("  RAM:       128KB\r\n");
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
        Shell_Printf("Current rotation: %d° (", current_rotation * 90);
        switch (current_rotation)
        {
            case 0: Shell_Print("Portrait)\r\n"); break;
            case 1: Shell_Print("Landscape)\r\n"); break;
            case 2: Shell_Print("Portrait Inverted)\r\n"); break;
            case 3: Shell_Print("Landscape Inverted)\r\n"); break;
        }
        Shell_Print("\r\nUsage:\r\n");
        Shell_Print("  sys -r 0    - 0° (Portrait)\r\n");
        Shell_Print("  sys -r 1    - 90° (Landscape)\r\n");
        Shell_Print("  sys -r 2    - 180° (Portrait Inverted)\r\n");
        Shell_Print("  sys -r 3    - 270° (Landscape Inverted)\r\n");
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
    Shell_Printf("Screen rotated to %d°\r\n", current_rotation * 90);
    
    /* Clear screen after rotation */
    BG_lcd.Clear(BLACK);
    
    return 0;
}

static const ShellOpt_t sys_opts[] = {
    OPT("i", "info",    NULL,      "Show system info",     sys_info),
    OPT("m", "mem",     NULL,      "Show memory status",   sys_mem),
    OPT("t", "tasks",   NULL,      "List running tasks",   sys_tasks),
    OPT("u", "uptime",  NULL,      "Show uptime",          sys_uptime),
    OPT("b", "reboot",  NULL,      "Reboot system",        sys_reboot),
    OPT("o", "io",      "[cmd]",   "IO control (cdc/ble/lock/unlock)", sys_io),
    OPT("c", "console", "[cmd]",   "LCD console (on/off/clear)",       sys_console),
    OPT("d", "dbglcd",  "[cmd]",   "DBG to LCD (on/off)",              sys_dbglcd),
    OPT("r", "rotate_distr",  "[0-3]",   "Rotate screen (0/1/2/3 = 0°/90°/180°/270°)", sys_rotate),
    OPT_END()
};

DEFINE_MODULE(sys, "System information", MOD_CAT_SYSTEM, sys_opts);

/*============================================================================
 * audio module - Audio control
 *===========================================================================*/

static int audio_vol(int argc, char *argv[])
{
    if(argc < 1)
    {
        Shell_Print("Current volume: 75\r\n");
        return 0;
    }
    
    int vol = atoi(argv[0]);
    if(vol < 0) vol = 0;
    if(vol > 100) vol = 100;
    Shell_Printf("Volume: %d\r\n", vol);
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
    OPT("v", "vol",     "<0-100>",  "Set/get volume",       audio_vol),
    OPT("m", "mute",    "<0|1>",    "Mute on/off",          audio_mute),
    OPT_END()
};

DEFINE_MODULE(audio, "Audio control", MOD_CAT_PARAM, audio_opts);

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
        Shell_Print("Backlight: 80%\r\n");
        return 0;
    }
    int bl = atoi(argv[0]);
    if(bl < 0) bl = 0;
    if(bl > 100) bl = 100;
    Shell_Printf("Backlight: %d%%\r\n", bl);
    return 0;
}

static const ShellOpt_t lcd_opts[] = {
    OPT("o", "on",      NULL,       "Turn on LCD",      lcd_on),
    OPT("f", "off",     NULL,       "Turn off LCD",     lcd_off),
    OPT("b", "bl",      "<0-100>",  "Set backlight",    lcd_bl),
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
        Shell_Print("Mode: SONG\r\n");
    } else if (strcmp(argv[0], "free") == 0) {
        AudioLooper.SetMode(LOOP_MODE_FREE);
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
            Shell_Printf("BPM: %d\r\n", val);
        } else {
            Shell_Printf("Error: BPM must be %d-%d\r\n", METRONOME_MIN_BPM, METRONOME_MAX_BPM);
            return -1;
        }
    } else if (strcmp(argv[0], "beats") == 0 && argc > 1) {
        val = atoi(argv[1]);
        if (val >= METRONOME_MIN_BEATS_PER_MEASURE && val <= METRONOME_MAX_BEATS_PER_MEASURE) {
            AudioLooper.MetronomeSetBeatsPerMeasure((uint8_t)val);
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
    FlashNewDriver_Test();
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
static const ShellOpt_t bt_opts[] = {
    OPT("s", "state",    NULL,      "Show bt status",    bt_get_staus),
   
    OPT_END()
};

DEFINE_MODULE(bt, "Bluetooth controller", MOD_CAT_HARDWARE, bt_opts);

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


}
