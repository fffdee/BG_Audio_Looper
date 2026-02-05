/**
 *****************************************************************************
 * @file     shell_cmd_ui.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     10-January-2026
 * @brief    Shell commands for UI system control
 *****************************************************************************
 */

#include "shell_cmd_ui.h"
#include "bg_ui.h"
#include "comp_statusbar.h"
#include <string.h>
#include <stdlib.h>

/*============================================================================
 * UI State Control
 *===========================================================================*/

/**
 * @brief  Set UI state: ui -s <boot|idle|menu|looper|settings>
 */
static int ui_set_state(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Print("Usage: ui -s <boot|idle|menu|looper|settings>\r\n");
        return -1;
    }
    
    UI_State_t state;
    
    if (strcmp(argv[0], "boot") == 0) {
        state = UI_STATE_BOOT;
    } else if (strcmp(argv[0], "idle") == 0) {
        state = UI_STATE_IDLE;
    } else if (strcmp(argv[0], "menu") == 0) {
        state = UI_STATE_MENU;
    } else if (strcmp(argv[0], "looper") == 0) {
        state = UI_STATE_LOOPER;
    } else if (strcmp(argv[0], "settings") == 0) {
        state = UI_STATE_SETTINGS;
    } else {
        Shell_Printf("Unknown state: %s\r\n", argv[0]);
        return -1;
    }
    
    extern const BG_UI_t BG_UI;
    BG_UI.SetState(state);
    
    Shell_Printf("UI state set to: %s\r\n", BG_UI.GetStateName(state));
    return 0;
}

/**
 * @brief  Get current UI state: ui -g
 */
static int ui_get_state(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    extern const BG_UI_t BG_UI;
    UI_State_t state = BG_UI.GetState();
    UI_State_t prev = BG_UI.GetPrevState();
    
    Shell_Printf("Current state: %s\r\n", BG_UI.GetStateName(state));
    Shell_Printf("Previous state: %s\r\n", BG_UI.GetStateName(prev));
    Shell_Printf("Ready: %s\r\n", BG_UI.IsReady() ? "Yes" : "No");
    
    return 0;
}

/**
 * @brief  Show popup: ui -p [title] <message> [duration_ms]
 */
static int ui_show_popup(int argc, char *argv[])
{
    const char *title = "Info";
    const char *message = "Test Message";
    uint16_t duration = 2000;
    
    if (argc >= 1) {
        message = argv[0];
    }
    if (argc >= 2) {
        title = argv[0];
        message = argv[1];
    }
    if (argc >= 3) {
        duration = (uint16_t)atoi(argv[2]);
    }
    
    extern const BG_UI_t BG_UI;
    

    BG_UI.ShowPopup(title, message, duration);
    
    /* 弹窗显示后不立即输出，避免CDC通信中断 */
    return 0;
}

/**
 * @brief  Close popup: ui -c
 */
static int ui_close_popup(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    extern const BG_UI_t BG_UI;
    BG_UI.ClosePopup();
    
    //Shell_Print("Popup closed\r\n");
    return 0;
}

/**
 * @brief  Refresh UI: ui -r
 */
static int ui_refresh(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    extern const BG_UI_t BG_UI;
    BG_UI.Invalidate();
    
    Shell_Print("UI refreshed\r\n");
    return 0;
}

/**
 * @brief  List UI button states: ui -k
 */
static int ui_button_state(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    extern const BG_UI_t BG_UI;
    uint8_t i;
    const char* btn_names[] = {"UP", "DOWN", "ENTER", "BACK"};
    
    Shell_Print("Button States:\r\n");
    for (i = 0; i < 4; i++) {
        bool pressed = BG_UI.IsButtonPressed((UI_BtnID_t)i);
        Shell_Printf("  %s: %s\r\n", btn_names[i], pressed ? "PRESSED" : "Released");
    }
    
    return 0;
}

/*============================================================================
 * Status Bar Control
 *===========================================================================*/

/**
 * @brief  Set battery level: ui -b <0-100>
 */
static int ui_set_battery(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Print("Usage: ui -b <0-100>\r\n");
        return -1;
    }
    
    int level = atoi(argv[0]);
    if (level < 0 || level > 100) {
        Shell_Print("Battery level must be 0-100\r\n");
        return -1;
    }
    
    extern const BG_UI_t BG_UI;
    BG_UI.StatusBar_SetBattery((uint8_t)level);
    
    Shell_Printf("Battery level set to: %d%%\r\n", level);
    return 0;
}

/**
 * @brief  Set Bluetooth status: ui -t <0-4>
 */
static int ui_set_bluetooth(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Print("Usage: ui -t <0-4>\r\n");
        Shell_Print("  0: Disconnected\r\n");
        Shell_Print("  1: Advertising\r\n");
        Shell_Print("  2: Connecting\r\n");
        Shell_Print("  3: Connected\r\n");
        Shell_Print("  4: Playing\r\n");
        return 0;
    }
    
    int status = atoi(argv[0]);
    if (status < 0 || status > 4) {
        Shell_Print("Bluetooth status must be 0-4\r\n");
        return -1;
    }
    
    extern const BG_UI_t BG_UI;
    BG_UI.StatusBar_SetBT((uint8_t)status);
    
    Shell_Printf("Bluetooth status set to: %d\r\n", status);
    return 0;
}

/**
 * @brief  Set volume: ui -v <0-100>
 */
static int ui_set_volume(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Print("Usage: ui -v <0-100>\r\n");
        return -1;
    }
    
    int volume = atoi(argv[0]);
    if (volume < 0 || volume > 100) {
        Shell_Print("Volume must be 0-100\r\n");
        return -1;
    }
    
    extern const BG_UI_t BG_UI;
    BG_UI.StatusBar_SetVolume((uint8_t)volume);
    
    Shell_Printf("Volume set to: %d\r\n", volume);
    return 0;
}

/**
 * @brief  Update status bar: ui -u
 */
static int ui_update_statusbar(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    extern const BG_UI_t BG_UI;
    BG_UI.StatusBar_Update();
    
    Shell_Print("Status bar updated\r\n");
    return 0;
}

/**
 * @brief  Enable debug mode: ui -d <on|off>
 */
static int ui_debug(int argc, char *argv[])
{
    if (argc < 1) {
        Shell_Print("Usage: ui -d <on|off>\r\n");
        return 0;
    }
    
    bool enable = (strcmp(argv[0], "on") == 0 || strcmp(argv[0], "1") == 0);
    
    extern const BG_UI_t BG_UI;
    BG_UI.SetDebug(enable);
    
    Shell_Printf("UI debug: %s\r\n", enable ? "ON" : "OFF");
    return 0;
}

/**
 * @brief Query UI state in JSON format: ui -q
 */
static int ui_query(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    extern const BG_UI_t BG_UI;
    UI_State_t state = BG_UI.GetState();
    UI_State_t prev = BG_UI.GetPrevState();
    
    Shell_Printf("{\"status\":\"ok\",\"ui\":{");
    Shell_Printf("\"current_state\":%d,", (int)state);
    Shell_Printf("\"previous_state\":%d,", (int)prev);
    Shell_Printf("\"ready\":%s", BG_UI.IsReady() ? "true" : "false");
    Shell_Printf("}}\n");
    
    return 0;
}

/*============================================================================
 * Module Definition
 *===========================================================================*/

static const ShellOpt_t ui_options[] = {
    OPT("s", "state",    "<state>",           "Set UI state (boot/idle/menu/looper/settings)", ui_set_state),
    OPT("g", "get",      NULL,                "Get current UI state",                          ui_get_state),
    OPT("p", "popup",    "[title] <msg> [ms]","Show popup message",                           ui_show_popup),
    OPT("c", "close",    NULL,                "Close popup",                                   ui_close_popup),
    OPT("r", "refresh",  NULL,                "Refresh UI",                                    ui_refresh),
    OPT("k", "keys",     NULL,                "Show button states",                            ui_button_state),
    OPT("b", "battery",  "<0-100>",           "Set battery level",                             ui_set_battery),
    OPT("t", "bt",       "<0-4>",             "Set Bluetooth status",                          ui_set_bluetooth),
    OPT("v", "volume",   "<0-100>",           "Set volume",                                    ui_set_volume),
    OPT("u", "update",   NULL,                "Update status bar",                             ui_update_statusbar),
    OPT("q", "query",    NULL,                "Query UI state (JSON)",                         ui_query),
    OPT("d", "debug",    "<on|off>",          "Enable/disable debug mode",                     ui_debug),
    OPT_END()
};

DEFINE_MODULE(ui, "UI system control", MOD_CAT_SYSTEM, ui_options);

/*============================================================================
 * Public API
 *===========================================================================*/

void UICmd_Register(void)
{
    REGISTER_MODULE(ui);
}
