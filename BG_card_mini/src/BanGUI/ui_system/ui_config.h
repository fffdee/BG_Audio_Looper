/**
 * @file    ui_config.h
 * @brief   UI绯荤粺閰嶇疆鏂囦欢
 * @author  BG Card Team
 * @date    2025-12-18
 */

#ifndef __UI_CONFIG_H__
#define __UI_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 灞忓箷閰嶇疆
 *===========================================================================*/
#define UI_SCREEN_WIDTH         160
#define UI_SCREEN_HEIGHT        128

/*===========================================================================
 * 鐘舵�鏍忛厤缃�
 *===========================================================================*/
#define UI_STATUSBAR_HEIGHT     12      /* 鐘舵�鏍忛珮搴�*/
#define UI_STATUSBAR_Y          0       /* 鐘舵�鏍廦浣嶇疆 */
#define UI_STATUSBAR_BG_COLOR   0x2104  /* 娣辩伆鑹茶儗鏅�*/
#define UI_STATUSBAR_FG_COLOR   0xFFFF  /* 鐧借壊鍓嶆櫙 */

/* 鍥炬爣浣嶇疆瀹氫箟 (浠庡乏鍒板彸锛屾瘡涓浘鏍囬棿闅�2鍍忕礌) */
#define UI_ICON_BT_X            2       /* 钃濈墮鍥炬爣X */
#define UI_ICON_MIC_X           16      /* 楹﹀厠椋庡浘鏍嘪 */
#define UI_ICON_GUITAR_X        30      /* 鍚変粬鍥炬爣X */
#define UI_ICON_HP_X            44      /* 鑰虫満/鎵０鍣ㄥ浘鏍嘪 */
#define UI_ICON_USB_X           58      /* USB鍥炬爣X */
#define UI_ICON_VOLUME_X        130     /* 闊抽噺鍥炬爣X */
#define UI_ICON_Y               2       /* 鍥炬爣Y浣嶇疆 */
#define UI_ICON_SIZE            8       /* 鍥炬爣澶у皬 */

/*===========================================================================
 * 鑿滃崟閰嶇疆
 *===========================================================================*/
#define UI_MENU_START_Y         (UI_STATUSBAR_HEIGHT + 2)  /* 鑿滃崟璧峰Y */
#define UI_MENU_HEIGHT          (UI_SCREEN_HEIGHT - UI_STATUSBAR_HEIGHT - 2)
#define UI_MENU_ITEM_HEIGHT     20      /* 鑿滃崟椤归珮搴�*/
#define UI_MENU_MAX_ITEMS       16      /* 鏈�ぇ鑿滃崟椤规暟 */
#define UI_MENU_MAX_DEPTH       4       /* 鏈�ぇ鑿滃崟娣卞害 */
#define UI_MENU_VISIBLE_ITEMS   5       /* 鍙鑿滃崟椤规暟 */

#define UI_MENU_BG_COLOR        0x0000  /* 榛戣壊鑳屾櫙 */
#define UI_MENU_FG_COLOR        0xFFFF  /* 鐧借壊鏂囧瓧 */
#define UI_MENU_SEL_BG_COLOR    0x001F  /* 钃濊壊閫変腑鑳屾櫙 */
#define UI_MENU_SEL_FG_COLOR    0xFFFF  /* 鐧借壊閫変腑鏂囧瓧 */
#define UI_MENU_ICON_COLOR      0x07E0  /* 缁胯壊鍥炬爣 */

/*===========================================================================
 * 寮�満鐢婚潰閰嶇疆
 *===========================================================================*/
#define UI_BOOT_DURATION        100    /* 寮�満鐢婚潰鎸佺画鏃堕棿(ms) */
/* 闃舵鏃堕棿閰嶇疆 */
#define STAGE_LOGO_TIME     50     /* Logo鏄剧ず鏃堕棿 */
#define STAGE_INFO_TIME     50     /* 淇℃伅鏄剧ず鏃堕棿 */
#define STAGE_FADEOUT_TIME  10     /* 娣″嚭鏃堕棿 */

#define UI_BOOT_LOGO_WIDTH      64      /* Logo瀹藉害 */
#define UI_BOOT_LOGO_HEIGHT     64      /* Logo楂樺害 */
#define UI_BOOT_BG_COLOR        0x0000  /* 榛戣壊鑳屾櫙 */
#define UI_BOOT_TEXT_COLOR      0xFFFF  /* 鐧借壊鏂囧瓧 */
#define UI_BOOT_PROGRESS_COLOR  0x07E0  /* 缁胯壊杩涘害鏉�*/

/*===========================================================================
 * 鎸夐敭閰嶇疆
 *===========================================================================*/
#define UI_BTN_DEBOUNCE_MS      20      /* 鍘绘姈鏃堕棿(ms) */
#define UI_BTN_LONG_PRESS_MS    800     /* 闀挎寜鏃堕棿(ms) */
#define UI_BTN_REPEAT_MS        200     /* 杩炴寜闂撮殧(ms) */

/*===========================================================================
 * 棰滆壊瀹氫箟
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
