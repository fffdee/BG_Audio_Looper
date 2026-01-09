/**
 * @file    ui_menu_def.c
 * @brief   Default menu definition example
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * This file defines the system's default menu structure.
 * Users can modify menu items as needed.
 */

#include "ui_menu.h"
#include "ui_statusbar.h"
#include "ui_system.h"

#include <stddef.h>

/*===========================================================================
 * Menu item variables (bound to menu items)
 *===========================================================================*/

/* Audio settings */
static int32_t audio_volume = 50;
static int32_t audio_bass = 0;
static int32_t audio_treble = 0;
static bool audio_mute = false;

/* System settings */
static int32_t lcd_brightness = 80;
static uint8_t language_idx = 0;
static bool bt_enabled = true;

/* Option lists */
static const char* language_options[] = { "Chinese", "English" };
static const char* eq_options[] = { "Flat", "Rock", "Pop", "Jazz", "Classic" };
static uint8_t eq_idx = 0;

/*===========================================================================
 * Menu item callback functions
 *===========================================================================*/

static void on_volume_change(UI_MenuItem_t* item)
{
    (void)item;
    /* In actual application, call audio driver to set volume here */
    /* AudioDrv_SetVolume(audio_volume); */
}

static void on_mute_change(UI_MenuItem_t* item)
{
    (void)item;
    /* AudioDrv_SetMute(audio_mute); */
}

static void on_brightness_change(UI_MenuItem_t* item)
{
    (void)item;
    /* LCD_SetBrightness(lcd_brightness); */
}

static void on_language_change(UI_MenuItem_t* item)
{
    (void)item;
    /* Switch language */
}

static void on_bt_toggle(UI_MenuItem_t* item)
{
    (void)item;
    /* BT_SetEnable(bt_enabled); */
    UI_StatusBar_SetBTStatus(bt_enabled ? UI_BT_DISCONNECTED : UI_BT_OFF);
}

static void on_about(UI_MenuItem_t* item)
{
    (void)item;
    UI_System_ShowPopup("About", "BG Card Mini v1.0", 3000);
}

static void on_factory_reset(UI_MenuItem_t* item)
{
    (void)item;
    UI_System_ShowPopup("Reset", "Factory Reset?", 0);
}

/* Welcome page callback functions */
static void on_welcome_settings(UI_MenuItem_t* item)
{
    (void)item;
    /* Enter settings submenu - show main menu */
    UI_System_ShowMenu();
}

static void on_welcome_music(UI_MenuItem_t* item)
{
    (void)item;
    UI_System_ShowPopup("Music", "Coming Soon", 2000);
}

static void on_welcome_about(UI_MenuItem_t* item)
{
    (void)item;
    UI_System_ShowPopup("About", "BG Card Mini v1.0", 3000);
}

static void on_welcome_game(UI_MenuItem_t* item)
{
    (void)item;
    UI_System_ShowPopup("Game", "Coming Soon", 2000);
}

/*===========================================================================
 * Submenu definitions
 *===========================================================================*/

/* === Audio settings submenu === */
static UI_MenuItem_t audio_menu_items[] = {
    UI_MENU_VALUE("Volume", &audio_volume, 0, 100, 5, "%", on_volume_change),
    UI_MENU_TOGGLE("Mute", &audio_mute, on_mute_change),
    UI_MENU_VALUE("Bass", &audio_bass, -12, 12, 1, "dB", NULL),
    UI_MENU_VALUE("Treble", &audio_treble, -12, 12, 1, "dB", NULL),
    UI_MENU_SELECT("EQ Mode", &eq_idx, eq_options, 5, NULL),
    UI_MENU_BACK_ITEM("< Back"),
};

static UI_Menu_t audio_menu = {
    .title = "Audio Settings",
    .items = audio_menu_items,
    .item_count = sizeof(audio_menu_items) / sizeof(audio_menu_items[0]),
    .selected = 0,
    .scroll_offset = 0,
    .parent = NULL
};

/* === Display settings submenu === */
static UI_MenuItem_t display_menu_items[] = {
    UI_MENU_VALUE("Brightness", &lcd_brightness, 10, 100, 10, "%", on_brightness_change),
    UI_MENU_SELECT("Language", &language_idx, language_options, 2, on_language_change),
    UI_MENU_BACK_ITEM("< Back"),
};

static UI_Menu_t display_menu = {
    .title = "Display Settings",
    .items = display_menu_items,
    .item_count = sizeof(display_menu_items) / sizeof(display_menu_items[0]),
    .selected = 0,
    .scroll_offset = 0,
    .parent = NULL
};

/* === Bluetooth settings submenu === */
static UI_MenuItem_t bluetooth_menu_items[] = {
    UI_MENU_TOGGLE("Bluetooth", &bt_enabled, on_bt_toggle),
    UI_MENU_ACTION("Scan Devices", NULL),
    UI_MENU_ACTION("Pair New", NULL),
    UI_MENU_ACTION("Disconnect", NULL),
    UI_MENU_BACK_ITEM("< Back"),
};

static UI_Menu_t bluetooth_menu = {
    .title = "Bluetooth",
    .items = bluetooth_menu_items,
    .item_count = sizeof(bluetooth_menu_items) / sizeof(bluetooth_menu_items[0]),
    .selected = 0,
    .scroll_offset = 0,
    .parent = NULL
};

/* === System info submenu === */
static UI_MenuItem_t system_info_items[] = {
    UI_MENU_ACTION("About", on_about),
    UI_MENU_ACTION("Factory Reset", on_factory_reset),
    UI_MENU_BACK_ITEM("< Back"),
};

static UI_Menu_t system_info_menu = {
    .title = "System Info",
    .items = system_info_items,
    .item_count = sizeof(system_info_items) / sizeof(system_info_items[0]),
    .selected = 0,
    .scroll_offset = 0,
    .parent = NULL
};

/*===========================================================================
 * Welcome menu definition (shown after boot)
 *===========================================================================*/

static UI_MenuItem_t welcome_menu_items[] = {
    {
        .name = "Settings",
        .type = UI_MENU_ITEM_ACTION,
        .data.action.callback = on_welcome_settings,
        .enabled = true,
        .visible = true,
        .user_data = 0
    },
    {
        .name = "Music",
        .type = UI_MENU_ITEM_ACTION,
        .data.action.callback = on_welcome_music,
        .enabled = true,
        .visible = true,
        .user_data = 0
    },
    {
        .name = "About",
        .type = UI_MENU_ITEM_ACTION,
        .data.action.callback = on_welcome_about,
        .enabled = true,
        .visible = true,
        .user_data = 0
    },
    {
        .name = "Game",
        .type = UI_MENU_ITEM_ACTION,
        .data.action.callback = on_welcome_game,
        .enabled = true,
        .visible = true,
        .user_data = 0
    },
};

static UI_Menu_t welcome_menu = {
    .title = "WELCOME",
    .items = welcome_menu_items,
    .item_count = sizeof(welcome_menu_items) / sizeof(welcome_menu_items[0]),
    .selected = 0,
    .scroll_offset = 0,
    .parent = NULL
};

/*===========================================================================
 * Main menu definition
 *===========================================================================*/

static UI_MenuItem_t main_menu_items[] = {
    UI_MENU_SUBMENU("Audio", &audio_menu),
    UI_MENU_SUBMENU("Display", &display_menu),
    UI_MENU_SUBMENU("Bluetooth", &bluetooth_menu),
    UI_MENU_SUBMENU("System", &system_info_menu),
};

static UI_Menu_t main_menu = {
    .title = "Main Menu",
    .items = main_menu_items,
    .item_count = sizeof(main_menu_items) / sizeof(main_menu_items[0]),
    .selected = 0,
    .scroll_offset = 0,
    .parent = NULL
};

/*===========================================================================
 * API
 *===========================================================================*/

/**
 * @brief Get welcome menu
 */
UI_Menu_t* UI_GetWelcomeMenu(void)
{
    return &welcome_menu;
}

/**
 * @brief Get default main menu
 */
UI_Menu_t* UI_GetDefaultMainMenu(void)
{
    return &main_menu;
}

/**
 * @brief Initialize menu system (set parent-child relationships)
 */
void UI_Menu_InitDefault(void)
{
    /* Set parent for submenus */
    audio_menu.parent = &main_menu;
    display_menu.parent = &main_menu;
    bluetooth_menu.parent = &main_menu;
    system_info_menu.parent = &main_menu;
    /* Set main menu */
    UI_System_SetMainMenu(&main_menu);
}

/* Get volume value (for external use) */
int32_t UI_Menu_GetVolume(void)
{
    return audio_volume;
}

/* Set volume value (for external use) */
void UI_Menu_SetVolume(int32_t vol)
{
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    audio_volume = vol;
}

/* Get mute status */
bool UI_Menu_GetMute(void)
{
    return audio_mute;
}

/* Set mute status */
void UI_Menu_SetMute(bool mute)
{
    audio_mute = mute;
}
