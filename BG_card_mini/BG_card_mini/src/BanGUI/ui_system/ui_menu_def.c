/**
 * @file    ui_menu_def.c
 * @brief   默认菜单定义示例
 * @author  BG Card Team
 * @date    2025-12-18
 * 
 * 本文件定义了系统默认菜单结构
 * 用户可以根据需要修改菜单项
 */

#include "ui_menu.h"
#include "ui_statusbar.h"
#include "ui_system.h"
#include <stddef.h>

/*===========================================================================
 * 菜单项变量 (绑定到菜单项)
 *===========================================================================*/

/* 音频设置 */
static int32_t audio_volume = 50;
static int32_t audio_bass = 0;
static int32_t audio_treble = 0;
static bool audio_mute = false;

/* 系统设置 */
static int32_t lcd_brightness = 80;
static uint8_t language_idx = 0;
static bool bt_enabled = true;

/* 选项列表 */
static const char* language_options[] = { "中文", "English" };
static const char* eq_options[] = { "Flat", "Rock", "Pop", "Jazz", "Classic" };
static uint8_t eq_idx = 0;

/*===========================================================================
 * 菜单回调函数
 *===========================================================================*/

static void on_volume_change(UI_MenuItem_t* item)
{
    (void)item;
    /* 实际应用中在这里调用音频驱动设置音量 */
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
    /* 切换语言 */
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

/*===========================================================================
 * 子菜单定义
 *===========================================================================*/

/* === 音频设置子菜单 === */
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

/* === 显示设置子菜单 === */
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

/* === 蓝牙设置子菜单 === */
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

/* === 系统信息子菜单 === */
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
 * 主菜单定义
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
 * @brief 获取默认主菜单
 */
UI_Menu_t* UI_GetDefaultMainMenu(void)
{
    return &main_menu;
}

/**
 * @brief 初始化菜单系统 (设置父子关系)
 */
void UI_Menu_InitDefault(void)
{
    /* 设置子菜单的父菜单 */
    audio_menu.parent = &main_menu;
    display_menu.parent = &main_menu;
    bluetooth_menu.parent = &main_menu;
    system_info_menu.parent = &main_menu;
    
    /* 设置主菜单 */
    UI_System_SetMainMenu(&main_menu);
}

/* 获取音量值 (供外部读取) */
int32_t UI_Menu_GetVolume(void)
{
    return audio_volume;
}

/* 设置音量值 (供外部调用) */
void UI_Menu_SetVolume(int32_t vol)
{
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    audio_volume = vol;
}

/* 获取静音状态 */
bool UI_Menu_GetMute(void)
{
    return audio_mute;
}

/* 设置静音状态 */
void UI_Menu_SetMute(bool mute)
{
    audio_mute = mute;
}
