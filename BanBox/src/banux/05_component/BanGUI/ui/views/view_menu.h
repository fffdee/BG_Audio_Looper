/**
 * @file    view_menu.h
 * @brief   Menu View - Navigation menu (New Architecture)
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 鑿滃崟瑙嗗浘 - 鏄剧ず瀵艰埅鑿滃崟
 */

#ifndef __VIEW_MENU_H__
#define __VIEW_MENU_H__

#include "../core/bg_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 鍓嶅悜澹版槑
 *===========================================================================*/

struct UI_Menu;
typedef struct UI_Menu UI_Menu_t;

/*===========================================================================
 * API 鍑芥暟
 *===========================================================================*/

/**
 * @brief 鍒涘缓 Menu 瑙嗗浘
 */
UI_View_t* View_Menu_Create(void);

/**
 * @brief 鍒濆鍖�Menu 瑙嗗浘
 * @return UI_View_t* 瑙嗗浘鎸囬拡
 */
UI_View_t* View_Menu_Init(void);

/**
 * @brief 閿�瘉 Menu 瑙嗗浘
 */
void View_Menu_Destroy(void);

/**
 * @brief 璁剧疆褰撳墠鑿滃崟
 * @param menu 鑿滃崟鎸囬拡
 */
void View_Menu_SetMenu(UI_Menu_t* menu);

/**
 * @brief 鑾峰彇褰撳墠鑿滃崟
 * @return UI_Menu_t* 褰撳墠鑿滃崟鎸囬拡
 */
UI_Menu_t* View_Menu_GetMenu(void);

/**
 * @brief 鍒锋柊鏄剧ず
 */
void View_Menu_Refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __VIEW_MENU_H__ */
