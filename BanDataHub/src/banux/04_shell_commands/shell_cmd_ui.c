/**
 *****************************************************************************
 * @file     shell_cmd_ui.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     10-January-2026
 * @brief    Shell commands for UI system control (stub - UI component not available)
 *****************************************************************************
 */

#include "shell_cmd_ui.h"
#include <string.h>
#include <stdlib.h>

/*============================================================================
 * Stub Command Handlers
 *===========================================================================*/

static int ui_set_state(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_get_state(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_show_popup(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_close_popup(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_refresh(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_button_state(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_set_battery(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_set_bluetooth(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_set_volume(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_update_statusbar(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_debug(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
}

static int ui_query(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Shell_Print("UI component not available\r\n");
    return -1;
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
