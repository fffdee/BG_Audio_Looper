#ifndef __PAGE_MANAGER_H__
#define __PAGE_MANAGER_H__

#include "bg_page.h"

#define MAX_PAGE 5
#define SETUP    1
#define UNSETUP  0
typedef enum{

    WELCOME_PAGE=0,       // 欢迎页面 - 开机后首页
    HOME_PAGE,
    LIST_PAGE,
    LIST_PAGE_IN,
    BG_MENU_SLIDER_PAGE,  // 新增的BG_MenuSlider演示页面
	NONE_OPR,

}BG_Page_ID;

extern uint8_t data[9];
extern BG_Page_Table table[MAX_PAGE];
extern const unsigned char gImage_qq[3200];
extern BG_Page BG_page;

#endif
