/**
 * @file    bg_page_compat.h
 * @brief   Legacy BG_Page API Compatibility Layer
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 兼容层 - 提供旧版 BG_Page API 的兼容支持
 * 
 * 用法:
 *   将原来的 #include "bg_page.h" 替换为 #include "bg_page_compat.h"
 *   代码无需其他修改即可编译
 * 
 * 注意:
 *   建议逐步迁移到新的 UI_PageMgr API
 */

#ifndef __BG_PAGE_COMPAT_H__
#define __BG_PAGE_COMPAT_H__

#include "ui_page.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 旧版常量兼容定义
 *===========================================================================*/

#define NONE_OPR        0xFF    /* 无操作标志 (兼容旧 page_manager.h) */
#define SETUP           1
#define UNSETUP         0

/*===========================================================================
 * 旧版结构体兼容定义
 *===========================================================================*/

/**
 * @brief 旧版页面表结构 (兼容)
 */
typedef struct {
    const char* name;
    unsigned char ID;
    unsigned char last;
    unsigned char next;
    unsigned char enter;
    unsigned char exit;
    unsigned char setup;
    void (*current_operation)(void);
} BG_Page_Table;

/**
 * @brief 旧版页面数据结构 (兼容)
 */
typedef struct {
    uint8_t running_id;
    uint8_t max_id_count;
    uint8_t last_pressed;
    uint8_t next_pressed;
    uint8_t enter_pressed;
    uint8_t exit_pressed;
    uint8_t exit_flag;
    BG_Page_Table* table;
} BG_Page_Data;

/**
 * @brief 旧版页面对象结构 (兼容)
 */
typedef struct BG_Page {
    BG_Page_Data Data;
    void (*Loop)(struct BG_Page*);
    void (*SetPage)(struct BG_Page*, uint8_t);
    void (*Last)(struct BG_Page*);
    void (*Next)(struct BG_Page*);
    void (*Enter)(struct BG_Page*);
    void (*Exit)(struct BG_Page*);
    void (*State_clear)(struct BG_Page*);
} BG_Page;

/*===========================================================================
 * 兼容 API
 *===========================================================================*/

/**
 * @brief 初始化页面系统 (兼容旧 API)
 * @param table 页面表
 * @param size 页面数量
 * @return BG_Page 对象
 */
BG_Page BG_Page_Init(BG_Page_Table* table, uint8_t size);

/*===========================================================================
 * 迁移指南
 *===========================================================================*/

/*
 * 旧 API -> 新 API 对照:
 * 
 * BG_Page_Init(table, size)     ->  UI_PageMgr.Init() + UI_PageMgr.Register()
 * BG_page.Loop(&BG_page)        ->  UI_PageMgr.Update() + UI_PageMgr.Draw()
 * BG_page.Last(&BG_page)        ->  UI_PageMgr.NavUp()
 * BG_page.Next(&BG_page)        ->  UI_PageMgr.NavDown()
 * BG_page.Enter(&BG_page)       ->  UI_PageMgr.NavEnter()
 * BG_page.Exit(&BG_page)        ->  UI_PageMgr.NavBack()
 * BG_page.SetPage(&BG_page, id) ->  UI_PageMgr.GotoPage(id, NULL)
 * BG_page.Data.running_id       ->  UI_PageMgr.GetCurrentID()
 * 
 * 新 API 优势:
 *   - 支持导航堆栈 (自动记住返回路径)
 *   - 页面生命周期回调 (on_init, on_exit, on_update, on_draw, on_key)
 *   - 与 BG_UI 状态机集成
 *   - 支持页面参数传递
 */

#ifdef __cplusplus
}
#endif

#endif /* __BG_PAGE_COMPAT_H__ */
