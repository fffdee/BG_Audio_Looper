#ifndef __PAGE_MANAGER_H__
#define __PAGE_MANAGER_H__

#include "bg_page.h"

#define MAX_PAGE 3
#define SETUP    1
#define UNSETUP  0
typedef enum{

    HOME_PAGE = 0,
    LIST_PAGE,
    LIST_PAGE_IN,

}BG_Page_ID;
extern uint8_t data[9];
extern BG_Page_Table table[MAX_PAGE];
extern const unsigned char gImage_qq[3200];
extern BG_Page BG_page;

#endif
