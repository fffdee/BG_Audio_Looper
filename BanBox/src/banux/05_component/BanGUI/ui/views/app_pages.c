/**
 * @file    app_pages.c
 * @brief   Application Pages Implementation
 * @author  BG Card Team
 * @date    2025-01-08
 */

#include "app_pages.h"
#include "view_home.h"
#include "view_menu.h"
#include "view_looper.h"
#include "bg_lcd.h"
#include "gui_tool.h"
#include <string.h>

/*===========================================================================
 * 页面操作函数声明
 *===========================================================================*/

static void page_home_operation(void);
static void page_menu_operation(void);
static void page_list_operation(void);
static void page_looper_operation(void);

/*===========================================================================
 * 全局变量 (兼容旧 API)
 *===========================================================================*/

BG_Page BG_page;

/**
 * @brief 页面表定义 (兼容旧 API)
 * 
 * 格式: {名称, ID, 上, 下, 确认, 返回, 初始化标志, 操作函数}
 */
BG_Page_Table table[MAX_PAGE] = {
    /* 主页: 上下切换到菜单，确认进入菜单，返回留在主页 */
    {"Home",   PAGE_HOME,   PAGE_MENU,   PAGE_MENU,   PAGE_MENU,   PAGE_HOME,   1, page_home_operation},
    /* 菜单页: 上下切换回主页，确认进入列表，返回到主页 */
    {"Menu",   PAGE_MENU,   PAGE_HOME,   PAGE_HOME,   PAGE_LIST,   PAGE_HOME,   1, page_menu_operation},
    /* 列表页: 上下自身，确认进入 Looper，返回到菜单 */
    {"List",   PAGE_LIST,   PAGE_LIST,   PAGE_LIST,   PAGE_LOOPER, PAGE_MENU,   1, page_list_operation},
    /* Looper 页: 上下自身，确认自身，返回到列表 */
    {"Looper", PAGE_LOOPER, PAGE_LOOPER, PAGE_LOOPER, PAGE_LOOPER, PAGE_LIST,   1, page_looper_operation},
};

/*===========================================================================
 * 页面操作函数实现
 *===========================================================================*/

static void page_home_operation(void)
{
    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1) {
        /* 首次进入或切换到此页面 */
        BG_lcd.Clear(0x0000);
        BGUI_tool.ShowString(30, 50, (uint8_t*)"BanBox Home", 0xFFFF);
        BGUI_tool.ShowString(20, 70, (uint8_t*)"Press ENTER for Menu", 0x07E0);
    }
}

static void page_menu_operation(void)
{
    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1) {
        BG_lcd.Clear(0x0000);
        BGUI_tool.ShowString(40, 50, (uint8_t*)"Main Menu", 0xFFFF);
        BGUI_tool.ShowString(10, 70, (uint8_t*)"UP/DOWN: Navigate", 0x07E0);
        BGUI_tool.ShowString(10, 86, (uint8_t*)"ENTER: Select", 0x07E0);
    }
}

static void page_list_operation(void)
{
    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1) {
        BG_lcd.Clear(0x0000);
        BGUI_tool.ShowString(40, 50, (uint8_t*)"List Page", 0xFFFF);
        BGUI_tool.ShowString(10, 70, (uint8_t*)"ENTER: Go to Looper", 0x07E0);
        BGUI_tool.ShowString(10, 86, (uint8_t*)"BACK: Return", 0x07E0);
    }
    
    /* 处理按键状态 */
    if (BG_page.Data.last_pressed == 1) {
        /* 上键处理 */
    }
    if (BG_page.Data.next_pressed == 1) {
        /* 下键处理 */
    }
    if (BG_page.Data.enter_pressed == 1) {
        /* 确认键处理 */
    }
}

static void page_looper_operation(void)
{
    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1) {
        BG_lcd.Clear(0x0000);
        BGUI_tool.ShowString(30, 30, (uint8_t*)"Audio Looper", 0x07FF);
        BGUI_tool.ShowString(10, 50, (uint8_t*)"Seg1  Seg2  Seg3  Seg4", 0xFFFF);
        BGUI_tool.ShowString(10, 100, (uint8_t*)"BACK: Return to List", 0x07E0);
    }
    
    /* Looper 特有的按键处理 */
    if (BG_page.Data.enter_pressed == 1) {
        /* 切换段状态 */
    }
}

/*===========================================================================
 * 公共 API
 *===========================================================================*/

void App_Pages_Init(void)
{
    /* 使用兼容层初始化 */
    BG_page = BG_Page_Init(table, MAX_PAGE);
}

BG_Page* App_Pages_GetCompat(void)
{
    return &BG_page;
}
