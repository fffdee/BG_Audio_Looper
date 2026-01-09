/**
 * @file    bg_page_compat.c
 * @brief   Legacy BG_Page API Compatibility Implementation
 * @author  BG Card Team
 * @date    2025-01-08
 */

#include "bg_page_compat.h"
#include <string.h>



static BG_Page s_compat_page;
static UI_Page_t s_converted_pages[UI_PAGE_MAX_COUNT];


static void compat_page_update(void)
{
    if (s_compat_page.Data.table && 
        s_compat_page.Data.running_id < s_compat_page.Data.max_id_count) {
        BG_Page_Table* entry = &s_compat_page.Data.table[s_compat_page.Data.running_id];
        if (entry->current_operation) {
            entry->current_operation();
        }
    }
}

/*===========================================================================
 * 鏃�API 瀹炵幇
 *===========================================================================*/

static void compat_loop(BG_Page* page)
{
    if (!page || !page->Data.table) return;
    
    /* 璋冪敤褰撳墠椤甸潰鐨勬搷浣滃嚱鏁�*/
    BG_Page_Table* entry = &page->Data.table[page->Data.running_id];
    if (entry->current_operation) {
        entry->current_operation();
    }
    
    /* 娓呴櫎鐘舵� (濡傛灉娌℃湁璁剧疆 exit_flag) */
    if (page->Data.exit_flag == 0) {
        page->State_clear(page);
    } else {
        page->Data.exit_flag = 0;
    }
}

static void compat_set_page(BG_Page* page, uint8_t id)
{
    if (!page || !page->Data.table) return;
    if (id >= page->Data.max_id_count) return;
    
    page->Data.table[page->Data.running_id].setup = 1;
    page->Data.running_id = id;
    
    /* 鍚屾鍒版柊 API */
    UI_PageMgr.GotoPage(id, NULL);
}

static void compat_last(BG_Page* page)
{
    if (!page || !page->Data.table) return;
    
    BG_Page_Table* entry = &page->Data.table[page->Data.running_id];
    
    if (entry->last == page->Data.running_id) {
        page->Data.last_pressed = 1;
    } else {
        entry->setup = 1;
    }
    page->Data.running_id = entry->last;
    
    /* 鍚屾鍒版柊 API */
    UI_PageMgr.NavUp();
}

static void compat_next(BG_Page* page)
{
    if (!page || !page->Data.table) return;
    
    BG_Page_Table* entry = &page->Data.table[page->Data.running_id];
    
    if (entry->next == page->Data.running_id) {
        page->Data.next_pressed = 1;
    } else {
        entry->setup = 1;
    }
    page->Data.running_id = entry->next;
    
    /* 鍚屾鍒版柊 API */
    UI_PageMgr.NavDown();
}

static void compat_enter(BG_Page* page)
{
    if (!page || !page->Data.table) return;
    
    BG_Page_Table* entry = &page->Data.table[page->Data.running_id];
    
    if (entry->enter == page->Data.running_id) {
        page->Data.enter_pressed = 1;
    } else {
        entry->setup = 1;
    }
    page->Data.running_id = entry->enter;
    
    /* 鍚屾鍒版柊 API */
    UI_PageMgr.NavEnter();
}

static void compat_exit(BG_Page* page)
{
    if (!page || !page->Data.table) return;
    
    BG_Page_Table* entry = &page->Data.table[page->Data.running_id];
    
    if (entry->exit == page->Data.running_id) {
        page->Data.exit_pressed = 1;
    } else {
        entry->setup = 1;
        /* 鍏堟墽琛岀洰鏍囬〉闈㈢殑鎿嶄綔 */
        BG_Page_Table* target = &page->Data.table[entry->exit];
        if (target->current_operation) {
            target->current_operation();
        }
        page->Data.exit_flag = 1;
    }
    page->Data.running_id = entry->exit;
    

    UI_PageMgr.NavBack();
}

static void compat_state_clear(BG_Page* page)
{
    if (!page) return;
    
    if (page->Data.table) {
        page->Data.table[page->Data.running_id].setup = 0;
    }
    page->Data.enter_pressed = 0;
    page->Data.exit_pressed = 0;
    page->Data.last_pressed = 0;
    page->Data.next_pressed = 0;
}

/*===========================================================================
 * 鍏叡 API
 *===========================================================================*/

BG_Page BG_Page_Init(BG_Page_Table* table, uint8_t size)
{
    /* 鍒濆鍖栨柊鐨勯〉闈㈢鐞嗗櫒 */
    UI_PageMgr.Init();
    uint8_t i;

    for (i = 0; i < size && i < UI_PAGE_MAX_COUNT; i++) {
        memset(&s_converted_pages[i], 0, sizeof(UI_Page_t));
        
        s_converted_pages[i].name = table[i].name;
        s_converted_pages[i].id = table[i].ID;
        s_converted_pages[i].nav_up = table[i].last;
        s_converted_pages[i].nav_down = table[i].next;
        s_converted_pages[i].nav_enter = table[i].enter;
        s_converted_pages[i].nav_back = table[i].exit;
        s_converted_pages[i].on_update = NULL;  /* 鏃�API 涓嶆敮鎸佸崟鐙殑 update */
        s_converted_pages[i].needs_redraw = (table[i].setup != 0);
        
        UI_PageMgr.Register(&s_converted_pages[i]);
    }
    
    /* 鍒濆鍖栧吋瀹瑰璞�*/
    memset(&s_compat_page, 0, sizeof(s_compat_page));
    
    s_compat_page.Loop = compat_loop;
    s_compat_page.SetPage = compat_set_page;
    s_compat_page.Last = compat_last;
    s_compat_page.Next = compat_next;
    s_compat_page.Enter = compat_enter;
    s_compat_page.Exit = compat_exit;
    s_compat_page.State_clear = compat_state_clear;
    
    s_compat_page.Data.running_id = 0;
    s_compat_page.Data.max_id_count = size;
    s_compat_page.Data.table = table;
    s_compat_page.Data.last_pressed = 0;
    s_compat_page.Data.next_pressed = 0;
    s_compat_page.Data.enter_pressed = 0;
    s_compat_page.Data.exit_pressed = 0;
    s_compat_page.Data.exit_flag = 0;
    
    return s_compat_page;
}
