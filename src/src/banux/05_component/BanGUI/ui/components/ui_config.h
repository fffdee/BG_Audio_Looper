/**
 * @file    ui_config.h
 * @brief   UI configuration definitions
 * @author  BG Card Team
 * @date    2025-01-09
 */

#ifndef __UI_CONFIG_H__
#define __UI_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Screen configuration
 *===========================================================================*/
#define UI_SCREEN_WIDTH         160
#define UI_SCREEN_HEIGHT        128

/*===========================================================================
 * Status bar configuration
 *===========================================================================*/
#define UI_STATUSBAR_HEIGHT     12      /* Status bar height */
#define UI_STATUSBAR_Y          0       /* Status bar Y position */
#define UI_STATUSBAR_BG_COLOR   0x0000  /* Status bar background color */
#define UI_STATUSBAR_FG_COLOR   0xFFFF  /* Status bar foreground color */

/* Icon position configuration (Bluetooth, MIC, Guitar, etc.) */
#define UI_ICON_BT_X            2       /* Bluetooth icon X */
#define UI_ICON_MIC_X           16      /* MIC icon X */
#define UI_ICON_GUITAR_X        30      /* Guitar icon X */
#define UI_ICON_HP_X            44      /* Headphone/Speaker icon X */
#define UI_ICON_USB_X           58      /* USB icon X */
#define UI_ICON_VOLUME_X        130     /* Volume icon X */
#define UI_ICON_Y               2       /* Icon Y position */
#define UI_ICON_SIZE            8       /* Icon size */

/*===========================================================================
 * Menu configuration
 *===========================================================================*/
#define UI_MENU_START_Y         (UI_STATUSBAR_HEIGHT + 2)  /* Menu start Y */
#define UI_MENU_HEIGHT          (UI_SCREEN_HEIGHT - UI_STATUSBAR_HEIGHT - 2)
#define UI_MENU_ITEM_HEIGHT     20      /* Menu item height */
#define UI_MENU_MAX_ITEMS       16      /* Max menu item count */
#define UI_MENU_MAX_DEPTH       4       /* Max menu depth */
#define UI_MENU_VISIBLE_ITEMS   5       /* Max visible menu items */

#define UI_MENU_BG_COLOR        0x0000  /* Menu background color */
#define UI_MENU_FG_COLOR        0xFFFF  /* Menu foreground color */
#define UI_MENU_SEL_BG_COLOR    0x001F  /* Selected item background color */
#define UI_MENU_SEL_FG_COLOR    0xFFFF  /* Selected item foreground color */
#define UI_MENU_ICON_COLOR      0x07E0  /* Menu icon color */

/*===========================================================================
 * Boot screen configuration
 *===========================================================================*/
#define UI_BOOT_DURATION        100    /* Boot screen duration (ms) */
#define STAGE_LOGO_TIME         50     /* Logo stage duration */
#define STAGE_INFO_TIME         50     /* Info stage duration */
#define STAGE_FADEOUT_TIME      10     /* Fadeout stage duration */

#define UI_BOOT_LOGO_WIDTH      64      /* Logo width */
#define UI_BOOT_LOGO_HEIGHT     64      /* Logo height */
#define UI_BOOT_BG_COLOR        0x0000  /* Boot background color */
#define UI_BOOT_TEXT_COLOR      0xFFFF  /* Boot text color */
#define UI_BOOT_PROGRESS_COLOR  0x07E0  /* Boot progress bar color */

/*===========================================================================
 * Button configuration
 *===========================================================================*/
#define UI_BTN_DEBOUNCE_MS      20      /* Button debounce time (ms) */
#define UI_BTN_LONG_PRESS_MS    800     /* Long press time (ms) */
#define UI_BTN_REPEAT_MS        200     /* Repeat trigger time (ms) */

/*===========================================================================
 * Color definitions
 *===========================================================================*/
#define UI_COLOR_BLACK          0x0000
#define UI_COLOR_WHITE          0xFFFF
#define UI_COLOR_RED            0xF800
#define UI_COLOR_GREEN          0x07E0
#define UI_COLOR_BLUE           0x001F
#define UI_COLOR_YELLOW         0xFFE0
#define UI_COLOR_CYAN           0x07FF
#define UI_COLOR_MAGENTA        0xF81F
#define UI_COLOR_ORANGE         0xFD20
#define UI_COLOR_GRAY           0x8410
#define UI_COLOR_DARK_GRAY      0x2104
#define UI_COLOR_LIGHT_GRAY     0xC618

#ifdef __cplusplus
}
#endif

#endif /* __UI_CONFIG_H__ */
