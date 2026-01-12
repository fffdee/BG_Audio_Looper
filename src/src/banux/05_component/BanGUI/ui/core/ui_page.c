/**
 * @file    ui_page.c
 * @brief   UI Page Management System Implementation
 * @author  BG Card Team
 * @date    2025-01-08
 */

#include "ui_page.h"
#include "bg_ui.h"
#include <string.h>

/*===========================================================================
 * 私有变量
 *===========================================================================*/

/* 页面注册表 */
static UI_Page_t* s_pages[UI_PAGE_MAX_COUNT] = {NULL};
static uint8_t s_page_count = 0;

/* 导航堆栈 */
static UI_PageID_t s_nav_stack[UI_PAGE_STACK_DEPTH];
static uint8_t s_stack_top = 0;

/* 当前页面 */
static UI_PageID_t s_current_id = UI_PAGE_INVALID;
static bool s_needs_redraw = true;

/*===========================================================================
 * 私有函数
 *===========================================================================*/

static UI_Page_t* find_page(UI_PageID_t id)
{
    uint8_t i;
    for (i = 0; i < s_page_count; i++) {
        if (s_pages[i] && s_pages[i]->id == id) {
            return s_pages[i];
        }
    }
    return NULL;
}

static void push_stack(UI_PageID_t id)
{
    if (s_stack_top < UI_PAGE_STACK_DEPTH) {
        s_nav_stack[s_stack_top++] = id;
    }
}

static UI_PageID_t pop_stack(void)
{
    if (s_stack_top > 0) {
        return s_nav_stack[--s_stack_top];
    }
    return UI_PAGE_INVALID;
}

static void switch_to_page(UI_PageID_t new_id, void* param, bool push_current)
{
    UI_Page_t* old_page = find_page(s_current_id);
    UI_Page_t* new_page = find_page(new_id);
    
    if (!new_page) return;
    
    /* 退出旧页面 */
    if (old_page && old_page->on_exit) {
        old_page->on_exit();
    }
    
    /* 压入堆栈 */
    if (push_current && s_current_id != UI_PAGE_INVALID) {
        push_stack(s_current_id);
    }
    
    /* 切换到新页面 */
    s_current_id = new_id;
    s_needs_redraw = true;
    
    /* 初始化新页面 */
    if (new_page->on_init) {
        new_page->on_init(param);
    }
    
    new_page->needs_redraw = true;
}

/*===========================================================================
 * API 实现
 *===========================================================================*/

static void page_init(void)
{
    memset(s_pages, 0, sizeof(s_pages));
    memset(s_nav_stack, UI_PAGE_INVALID, sizeof(s_nav_stack));
    s_page_count = 0;
    s_stack_top = 0;
    s_current_id = UI_PAGE_INVALID;
    s_needs_redraw = true;
}

static void page_deinit(void)
{
    /* 退出当前页面 */
    UI_Page_t* current = find_page(s_current_id);
    if (current && current->on_exit) {
        current->on_exit();
    }
    
    page_init();
}

static bool page_register(UI_Page_t* page)
{
    if (!page || s_page_count >= UI_PAGE_MAX_COUNT) {
        return false;
    }
    
    /* 检查 ID 是否已存在 */
    if (find_page(page->id)) {
        return false;
    }
    
    s_pages[s_page_count++] = page;
    
    /* 如果是第一个页面，设为当前页面 */
    if (s_current_id == UI_PAGE_INVALID) {
        s_current_id = page->id;
        if (page->on_init) {
            page->on_init(NULL);
        }
    }
    
    return true;
}

static bool page_unregister(UI_PageID_t id)
{
    uint8_t i, j;
    for (i = 0; i < s_page_count; i++) {
        if (s_pages[i] && s_pages[i]->id == id) {
            /* 如果是当前页面，需要先退出 */
            if (s_current_id == id) {
                if (s_pages[i]->on_exit) {
                    s_pages[i]->on_exit();
                }
                s_current_id = UI_PAGE_INVALID;
            }
            
            /* 移除 */
            for (j = i; j < s_page_count - 1; j++) {
                s_pages[j] = s_pages[j + 1];
            }
            s_page_count--;
            s_pages[s_page_count] = NULL;
            return true;
        }
    }
    return false;
}

static UI_Page_t* page_get(UI_PageID_t id)
{
    return find_page(id);
}

static void page_goto(UI_PageID_t id, void* param)
{
    if (id == s_current_id) return;
    switch_to_page(id, param, true);
}

static void page_back(void)
{
    UI_PageID_t prev_id = pop_stack();
    if (prev_id != UI_PAGE_INVALID) {
        switch_to_page(prev_id, NULL, false);
    } else {
        /* 没有历史，检查当前页面的 nav_back */
        UI_Page_t* current = find_page(s_current_id);
        if (current && current->nav_back != s_current_id) {
            switch_to_page(current->nav_back, NULL, false);
        }
    }
}

static void page_home(void)
{
    /* 清空堆栈，回到第一个页面 */
    s_stack_top = 0;
    if (s_page_count > 0 && s_pages[0]) {
        switch_to_page(s_pages[0]->id, NULL, false);
    }
}

static void page_handle_key(uint8_t key_id, uint8_t event)
{
    UI_Page_t* current = find_page(s_current_id);
    if (!current) return;
    
    /* 先让页面处理 */
    if (current->on_key) {
        if (current->on_key(key_id, event)) {
            return;  /* 事件被消费 */
        }
    }
    
    /* 默认导航处理 (只处理点击事件) */
    if (event != 1) return;  /* 1 = CLICK */
    
    switch (key_id) {
        case 0:  /* UP */
            UI_PageMgr.NavUp();
            break;
        case 1:  /* DOWN */
            UI_PageMgr.NavDown();
            break;
        case 2:  /* ENTER */
            UI_PageMgr.NavEnter();
            break;
        case 3:  /* BACK */
            UI_PageMgr.NavBack();
            break;
    }
}

static void page_nav_up(void)
{
    UI_Page_t* current = find_page(s_current_id);
    if (current && current->nav_up != s_current_id) {
        switch_to_page(current->nav_up, NULL, false);
    }
}

static void page_nav_down(void)
{
    UI_Page_t* current = find_page(s_current_id);
    if (current && current->nav_down != s_current_id) {
        switch_to_page(current->nav_down, NULL, false);
    }
}

static void page_nav_enter(void)
{
    UI_Page_t* current = find_page(s_current_id);
    if (current && current->nav_enter != s_current_id) {
        switch_to_page(current->nav_enter, NULL, true);
    }
}

static void page_nav_back(void)
{
    page_back();
}

static void page_update(void)
{
    UI_Page_t* current = find_page(s_current_id);
    if (current && current->on_update) {
        current->on_update();
    }
}

static void page_draw(void)
{
    UI_Page_t* current = find_page(s_current_id);
    if (!current) return;
    
    if (s_needs_redraw || current->needs_redraw) {
        if (current->on_draw) {
            current->on_draw();
        }
        s_needs_redraw = false;
        current->needs_redraw = false;
    }
}

static void page_invalidate(void)
{
    s_needs_redraw = true;
    UI_Page_t* current = find_page(s_current_id);
    if (current) {
        current->needs_redraw = true;
    }
}

static UI_PageID_t page_get_current_id(void)
{
    return s_current_id;
}

static UI_Page_t* page_get_current(void)
{
    return find_page(s_current_id);
}

static const char* page_get_current_name(void)
{
    UI_Page_t* current = find_page(s_current_id);
    return current ? current->name : "None";
}

static uint8_t page_get_stack_depth(void)
{
    return s_stack_top;
}

/*===========================================================================
 * UI_PageMgr 对象实例
 *===========================================================================*/

const UI_PageMgr_t UI_PageMgr = {
    /* 初始化和管理 */
    .Init = page_init,
    .Deinit = page_deinit,
    
    /* 页面注册 */
    .Register = page_register,
    .Unregister = page_unregister,
    .GetPage = page_get,
    
    /* 导航 */
    .GotoPage = page_goto,
    .Back = page_back,
    .Home = page_home,
    
    /* 按键处理 */
    .HandleKey = page_handle_key,
    .NavUp = page_nav_up,
    .NavDown = page_nav_down,
    .NavEnter = page_nav_enter,
    .NavBack = page_nav_back,
    
    /* 更新和绘制 */
    .Update = page_update,
    .Draw = page_draw,
    .Invalidate = page_invalidate,
    
    /* 状态查询 */
    .GetCurrentID = page_get_current_id,
    .GetCurrent = page_get_current,
    .GetCurrentName = page_get_current_name,
    .GetStackDepth = page_get_stack_depth,
};
