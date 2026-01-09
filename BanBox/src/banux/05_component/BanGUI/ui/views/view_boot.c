/**
 * @file    view_boot.c
 * @brief   Boot Splash Screen View Implementation
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 开机界面实现：
 *   - 动画状态机驱动进度条
 *   - 不使用 vTaskDelay 等硬延迟
 *   - 动画完成后自动切换到 UI_STATE_IDLE
 */

#include "view_boot.h"
#include "bg_lcd.h"
#include "gui_tool.h"
#include <string.h>

/*===========================================================================
 * 宏定义
 *===========================================================================*/

/* 进度条布局参数 */
#define PROGRESS_X      20
#define PROGRESS_Y      105
#define PROGRESS_WIDTH  120
#define PROGRESS_HEIGHT 6

/* 动画参数 */
#define PROGRESS_STEP   5       /* 每次更新增加5% */
#define UPDATE_INTERVAL 30      /* 更新间隔: 30ms */
#define SPLASH_DURATION 300     /* Logo 显示后停留时间: 300ms */

/* 版本信息 */
#define VERSION_STRING  "v1.0.0"

/*===========================================================================
 * 内部状态
 *===========================================================================*/

/**
 * @brief Boot 视图状态机
 */
typedef enum {
    BOOT_STATE_INIT = 0,        /* 初始化，显示 Logo */
    BOOT_STATE_PROGRESS,        /* 进度条动画中 */
    BOOT_STATE_EXIT             /* 准备退出 */
} BootState_t;

/**
 * @brief Boot 视图内部数据
 */
typedef struct {
    BootState_t state;          /* 当前状态 */
    uint8_t progress;           /* 进度条进度 (0-100) */
    uint32_t timer;             /* 计时器累积时间 (ms) */
    bool first_draw;            /* 首次绘制标志 */
} BootViewData_t;

static BootViewData_t s_boot_data = {
    .state = BOOT_STATE_INIT,
    .progress = 0,
    .timer = 0,
    .first_draw = true
};

/*===========================================================================
 * 内部函数声明
 *===========================================================================*/

static void boot_draw_logo(void);
static void boot_draw_progress_bar(uint8_t progress);

/*===========================================================================
 * View 生命周期回调
 *===========================================================================*/

/**
 * @brief 进入 Boot 视图
 */
static void boot_on_enter(void) {
    /* 重置状态 */
    s_boot_data.state = BOOT_STATE_INIT;
    s_boot_data.progress = 0;
    s_boot_data.timer = 0;
    s_boot_data.first_draw = true;
    
    /* 清屏 */
    BG_lcd.Clear(0x0000);  /* 黑色背景 */
}

/**
 * @brief 退出 Boot 视图
 */
static void boot_on_exit(void) {
    /* 清屏，准备切换到主界面 */
    BG_lcd.Clear(0x0000);
}

/**
 * @brief 更新 Boot 视图（状态机驱动）
 * @param delta_ms 距离上次更新的时间（毫秒）
 */
static void boot_on_update(uint16_t delta_ms) {
    s_boot_data.timer += delta_ms;
    
    switch (s_boot_data.state) {
        case BOOT_STATE_INIT:
            /* 初始化状态：显示 Logo */
            boot_draw_logo();
            s_boot_data.state = BOOT_STATE_PROGRESS;
            s_boot_data.timer = 0;  /* 重置计时器，准备进度条动画 */
            break;
            
        case BOOT_STATE_PROGRESS:
            /* 进度条动画状态 */
            if (s_boot_data.timer >= UPDATE_INTERVAL) {
                s_boot_data.timer = 0;  /* 重置计时器 */
                
                /* 更新进度 */
                s_boot_data.progress += PROGRESS_STEP;
                if (s_boot_data.progress > 100) {
                    s_boot_data.progress = 100;
                }
                
                /* 绘制进度条 */
                boot_draw_progress_bar(s_boot_data.progress);
                
                /* 进度完成，等待一段时间后切换 */
                if (s_boot_data.progress >= 100) {
                    s_boot_data.state = BOOT_STATE_EXIT;
                    s_boot_data.timer = 0;
                }
            }
            break;
            
        case BOOT_STATE_EXIT:
            /* 等待停留时间后退出 */
            if (s_boot_data.timer >= SPLASH_DURATION) {
                /* 自动切换到主界面 */
                extern const BG_UI_t BG_UI;
                BG_UI.SetState(UI_STATE_IDLE);
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief 绘制 Boot 视图
 */
static void boot_on_draw(void) {
    /* 在 on_update 中已按需绘制，这里可以为空 */
    /* 如果需要强制刷新，可以根据状态重绘 */
}

/**
 * @brief 处理按钮事件
 * @param event 按钮事件
 * @return true: 事件已处理, false: 事件未处理
 */
static bool boot_on_button(UI_BtnEventData_t* event) {
    /* Boot 界面不处理按钮事件 */
    /* 如果用户希望跳过开机动画，可以在这里处理 */
    
    /* 例如：按任意键跳过 */
    if (event->event == UI_BTN_EVT_CLICK) {
        /* 直接切换到主界面 */
        extern const BG_UI_t BG_UI;
        BG_UI.SetState(UI_STATE_IDLE);
        return true;
    }
    
    return false;
}

/*===========================================================================
 * 内部绘图函数
 *===========================================================================*/

/**
 * @brief 绘制 Logo 和初始文字
 */
static void boot_draw_logo(void) {
    /* 清屏 - 黑色背景 */
    BG_lcd.Clear(0x0000);
    
    /* 绘制大字体 Logo "BanBox" - 使用 Cyan 颜色 */
    /* 手动绘制大字体 B */
    uint16_t logo_x = 20;
    uint16_t logo_y = 30;
    
    /* 使用简单的方式：显示放大的文本 */
    /* B */
    BGUI_tool.ShowString(logo_x, logo_y, (uint8_t*)"B", 0x07FF);
    BGUI_tool.ShowString(logo_x+1, logo_y, (uint8_t*)"B", 0x07FF);
    BGUI_tool.ShowString(logo_x, logo_y+1, (uint8_t*)"B", 0x07FF);
    BGUI_tool.ShowString(logo_x+1, logo_y+1, (uint8_t*)"B", 0x07FF);
    
    /* a */
    BGUI_tool.ShowString(logo_x+16, logo_y, (uint8_t*)"a", 0x07FF);
    BGUI_tool.ShowString(logo_x+17, logo_y, (uint8_t*)"a", 0x07FF);
    BGUI_tool.ShowString(logo_x+16, logo_y+1, (uint8_t*)"a", 0x07FF);
    BGUI_tool.ShowString(logo_x+17, logo_y+1, (uint8_t*)"a", 0x07FF);
    
    /* n */
    BGUI_tool.ShowString(logo_x+32, logo_y, (uint8_t*)"n", 0x07FF);
    BGUI_tool.ShowString(logo_x+33, logo_y, (uint8_t*)"n", 0x07FF);
    BGUI_tool.ShowString(logo_x+32, logo_y+1, (uint8_t*)"n", 0x07FF);
    BGUI_tool.ShowString(logo_x+33, logo_y+1, (uint8_t*)"n", 0x07FF);
    
    /* B */
    BGUI_tool.ShowString(logo_x+48, logo_y, (uint8_t*)"B", 0x07E0);
    BGUI_tool.ShowString(logo_x+49, logo_y, (uint8_t*)"B", 0x07E0);
    BGUI_tool.ShowString(logo_x+48, logo_y+1, (uint8_t*)"B", 0x07E0);
    BGUI_tool.ShowString(logo_x+49, logo_y+1, (uint8_t*)"B", 0x07E0);
    
    /* o */
    BGUI_tool.ShowString(logo_x+64, logo_y, (uint8_t*)"o", 0x07E0);
    BGUI_tool.ShowString(logo_x+65, logo_y, (uint8_t*)"o", 0x07E0);
    BGUI_tool.ShowString(logo_x+64, logo_y+1, (uint8_t*)"o", 0x07E0);
    BGUI_tool.ShowString(logo_x+65, logo_y+1, (uint8_t*)"o", 0x07E0);
    
    /* x */
    BGUI_tool.ShowString(logo_x+80, logo_y, (uint8_t*)"x", 0x07E0);
    BGUI_tool.ShowString(logo_x+81, logo_y, (uint8_t*)"x", 0x07E0);
    BGUI_tool.ShowString(logo_x+80, logo_y+1, (uint8_t*)"x", 0x07E0);
    BGUI_tool.ShowString(logo_x+81, logo_y+1, (uint8_t*)"x", 0x07E0);
    
    /* 显示产品名称 "Audio Looper" */
    BGUI_tool.ShowString(20, 55, (uint8_t*)"Audio Looper", 0xFFFF);
    
    /* 显示版本号 */
    BGUI_tool.ShowString(55, 70, (uint8_t*)VERSION_STRING, 0x07E0);
    
    /* 版权信息 */
    BGUI_tool.ShowString(20, 85, (uint8_t*)"(c) BG Card Team", 0x8410);
    
    /* 绘制进度条外框 */
    BG_lcd.Box(PROGRESS_X, PROGRESS_Y, PROGRESS_WIDTH, PROGRESS_HEIGHT, 0xFFFF);
}

/**
 * @brief 绘制进度条
 * @param progress 进度 (0-100)
 */
static void boot_draw_progress_bar(uint8_t progress) {
    uint16_t fill_width;
    
    if (progress > 100) {
        progress = 100;
    }
    
    /* 计算填充宽度 */
    fill_width = (PROGRESS_WIDTH - 4) * progress / 100;
    
    /* 绘制进度条填充 */
    BG_lcd.Box(PROGRESS_X + 2, PROGRESS_Y + 2, 
               fill_width, PROGRESS_HEIGHT - 4, 0x07E0);
}

/*===========================================================================
 * View 对象
 *===========================================================================*/

static UI_View_t s_view_boot = {
    .name = "Boot",
    .on_enter = boot_on_enter,
    .on_exit = boot_on_exit,
    .on_update = boot_on_update,
    .on_draw = boot_on_draw,
    .on_button = boot_on_button,
    .visible = true,
    .dirty = false
};

/*===========================================================================
 * 公共接口
 *===========================================================================*/

/**
 * @brief 获取 Boot 视图
 */
UI_View_t* View_Boot_Get(void) {
    return &s_view_boot;
}

/**
 * @brief 初始化 Boot 视图
 */
void View_Boot_Init(void) {
    /* 注册视图到 UI 系统 */
    extern const BG_UI_t BG_UI;
    BG_UI.RegisterView(UI_STATE_BOOT, &s_view_boot);
}
