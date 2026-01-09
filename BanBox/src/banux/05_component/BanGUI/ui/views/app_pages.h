/**
 * @file    app_pages.h
 * @brief   Application Pages Definition
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 应用页面定义 - 定义所有页面及其导航关系
 */

#ifndef __APP_PAGES_H__
#define __APP_PAGES_H__

#include "../core/ui_page.h"
#include "../core/bg_page_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 页面 ID 定义
 *===========================================================================*/

#define MAX_PAGE    4

typedef enum {
    PAGE_HOME = 0,      /* 主页 */
    PAGE_MENU,          /* 菜单页 */
    PAGE_LIST,          /* 列表页 */
    PAGE_LOOPER,        /* Looper 页 */
} AppPageID_t;

/*===========================================================================
 * 外部声明 (兼容旧 API)
 *===========================================================================*/

extern BG_Page_Table table[MAX_PAGE];
extern BG_Page BG_page;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化应用页面
 * 注册所有页面到页面管理器
 */
void App_Pages_Init(void);

/**
 * @brief 获取兼容的 BG_Page 对象
 * @return BG_Page 对象指针
 */
BG_Page* App_Pages_GetCompat(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_PAGES_H__ */
