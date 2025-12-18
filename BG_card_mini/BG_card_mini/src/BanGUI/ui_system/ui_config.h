/**
 * @file    ui_config.h
 * @brief   UI系统配置文件
 * @author  BG Card Team
 * @date    2025-12-18
 */

#ifndef __UI_CONFIG_H__
#define __UI_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 屏幕配置
 *===========================================================================*/
#define UI_SCREEN_WIDTH         160
#define UI_SCREEN_HEIGHT        128

/*===========================================================================
 * 状态栏配置
 *===========================================================================*/
#define UI_STATUSBAR_HEIGHT     12      /* 状态栏高度 */
#define UI_STATUSBAR_Y          0       /* 状态栏Y位置 */
#define UI_STATUSBAR_BG_COLOR   0x2104  /* 深灰色背景 */
#define UI_STATUSBAR_FG_COLOR   0xFFFF  /* 白色前景 */

/* 图标位置定义 (从左到右，每个图标间隔12像素) */
#define UI_ICON_BT_X            2       /* 蓝牙图标X */
#define UI_ICON_MIC_X           16      /* 麦克风图标X */
#define UI_ICON_GUITAR_X        30      /* 吉他图标X */
#define UI_ICON_HP_X            44      /* 耳机/扬声器图标X */
#define UI_ICON_USB_X           58      /* USB图标X */
#define UI_ICON_VOLUME_X        130     /* 音量图标X */
#define UI_ICON_Y               2       /* 图标Y位置 */
#define UI_ICON_SIZE            8       /* 图标大小 */

/*===========================================================================
 * 菜单配置
 *===========================================================================*/
#define UI_MENU_START_Y         (UI_STATUSBAR_HEIGHT + 2)  /* 菜单起始Y */
#define UI_MENU_HEIGHT          (UI_SCREEN_HEIGHT - UI_STATUSBAR_HEIGHT - 2)
#define UI_MENU_ITEM_HEIGHT     20      /* 菜单项高度 */
#define UI_MENU_MAX_ITEMS       16      /* 最大菜单项数 */
#define UI_MENU_MAX_DEPTH       4       /* 最大菜单深度 */
#define UI_MENU_VISIBLE_ITEMS   5       /* 可见菜单项数 */

#define UI_MENU_BG_COLOR        0x0000  /* 黑色背景 */
#define UI_MENU_FG_COLOR        0xFFFF  /* 白色文字 */
#define UI_MENU_SEL_BG_COLOR    0x001F  /* 蓝色选中背景 */
#define UI_MENU_SEL_FG_COLOR    0xFFFF  /* 白色选中文字 */
#define UI_MENU_ICON_COLOR      0x07E0  /* 绿色图标 */

/*===========================================================================
 * 开机画面配置
 *===========================================================================*/
#define UI_BOOT_DURATION        2000    /* 开机画面持续时间(ms) */
#define UI_BOOT_LOGO_WIDTH      64      /* Logo宽度 */
#define UI_BOOT_LOGO_HEIGHT     64      /* Logo高度 */
#define UI_BOOT_BG_COLOR        0x0000  /* 黑色背景 */
#define UI_BOOT_TEXT_COLOR      0xFFFF  /* 白色文字 */
#define UI_BOOT_PROGRESS_COLOR  0x07E0  /* 绿色进度条 */

/*===========================================================================
 * 按键配置
 *===========================================================================*/
#define UI_BTN_DEBOUNCE_MS      20      /* 去抖时间(ms) */
#define UI_BTN_LONG_PRESS_MS    800     /* 长按时间(ms) */
#define UI_BTN_REPEAT_MS        200     /* 连按间隔(ms) */

/*===========================================================================
 * 颜色定义
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
