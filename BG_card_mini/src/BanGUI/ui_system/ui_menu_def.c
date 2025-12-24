/**
 * @file    ui_menu_def.c
 * @brief   榛樿鑿滃崟瀹氫箟绀轰緥
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * 鏈枃浠跺畾涔変簡绯荤粺榛樿鑿滃崟缁撴瀯
 * 鐢ㄦ埛鍙互鏍规嵁闇�淇敼鑿滃崟椤�
 */

#include "ui_menu.h"
#include "ui_statusbar.h"
#include "ui_system.h"

#include <stddef.h>

/*===========================================================================
 * 鑿滃崟椤瑰彉閲�(缁戝畾鍒拌彍鍗曢」)
 *===========================================================================*/

/* 闊抽璁剧疆 */
static int32_t audio_volume = 50;
static int32_t audio_bass = 0;
static int32_t audio_treble = 0;
static bool audio_mute = false;

/* 绯荤粺璁剧疆 */
static int32_t lcd_brightness = 80;
static uint8_t language_idx = 0;
static bool bt_enabled = true;

/* 閫夐」鍒楄〃 */
static const char* language_options[] = { "涓枃", "English" };
static const char* eq_options[] = { "Flat", "Rock", "Pop", "Jazz", "Classic" };
static uint8_t eq_idx = 0;

/*===========================================================================
 * 鑿滃崟鍥炶皟鍑芥暟
 *===========================================================================*/

static void on_volume_change(UI_MenuItem_t* item)
{
    (void)item;
    /* 瀹為檯搴旂敤涓湪杩欓噷璋冪敤闊抽椹卞姩璁剧疆闊抽噺 */
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
    /* 鍒囨崲璇█ */
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

/* 娆㈣繋椤甸潰鍥炶皟鍑芥暟 */
static void on_welcome_settings(UI_MenuItem_t* item)
{
    (void)item;
    /* 杩涘叆璁剧疆瀛愯彍鍗�- 鏄剧ず涓昏彍鍗�*/
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
 * 瀛愯彍鍗曞畾涔�
 *===========================================================================*/

/* === 闊抽璁剧疆瀛愯彍鍗�=== */
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

/* === 鏄剧ず璁剧疆瀛愯彍鍗�=== */
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

/* === 钃濈墮璁剧疆瀛愯彍鍗�=== */
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

/* === 绯荤粺淇℃伅瀛愯彍鍗�=== */
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
 * 娆㈣繋鑿滃崟瀹氫箟 (寮�満鍚庢樉绀�
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
 * 涓昏彍鍗曞畾涔�
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
 * @brief 鑾峰彇娆㈣繋鑿滃崟
 */
UI_Menu_t* UI_GetWelcomeMenu(void)
{
    return &welcome_menu;
}

/**
 * @brief 鑾峰彇榛樿涓昏彍鍗�
 */
UI_Menu_t* UI_GetDefaultMainMenu(void)
{
    return &main_menu;
}

/**
 * @brief 鍒濆鍖栬彍鍗曠郴缁�(璁剧疆鐖跺瓙鍏崇郴)
 */
void UI_Menu_InitDefault(void)
{
    /* 璁剧疆瀛愯彍鍗曠殑鐖惰彍鍗�*/
    audio_menu.parent = &main_menu;
    display_menu.parent = &main_menu;
    bluetooth_menu.parent = &main_menu;
    system_info_menu.parent = &main_menu;
    
    /* 璁剧疆涓昏彍鍗�*/
    UI_System_SetMainMenu(&main_menu);
}

/* 鑾峰彇闊抽噺鍊�(渚涘閮ㄨ鍙� */
int32_t UI_Menu_GetVolume(void)
{
    return audio_volume;
}

/* 璁剧疆闊抽噺鍊�(渚涘閮ㄨ皟鐢� */
void UI_Menu_SetVolume(int32_t vol)
{
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    audio_volume = vol;
}

/* 鑾峰彇闈欓煶鐘舵� */
bool UI_Menu_GetMute(void)
{
    return audio_mute;
}

/* 璁剧疆闈欓煶鐘舵� */
void UI_Menu_SetMute(bool mute)
{
    audio_mute = mute;
}
