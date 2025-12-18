/**
 * @file    ui_bootscreen.c
 * @brief   开机画面模块实现
 * @author  BG Card Team
 * @date    2025-12-18
 */

#include "ui_bootscreen.h"
#include "ui_config.h"
#include "bg_lcd.h"
#include <string.h>

/*===========================================================================
 * 默认Logo (简单的BG Card Logo - 32x32)
 *===========================================================================*/

/* 简单的"BG"文字图标 */
static const uint8_t default_logo[] = {
    /* B */
    0x7E, 0x42, 0x42, 0x7C, 0x42, 0x42, 0x7E, 0x00,
    /* G */
    0x3C, 0x42, 0x40, 0x4E, 0x42, 0x42, 0x3C, 0x00,
};

/*===========================================================================
 * 私有变量
 *===========================================================================*/

static UI_BootConfig_t boot_config;
static UI_BootStage_t boot_stage;
static uint32_t stage_timer;
static uint8_t current_progress;
static const char* progress_message;
static bool initialized;

/* 阶段时间配置 */
#define STAGE_LOGO_TIME     800     /* Logo显示时间 */
#define STAGE_INFO_TIME     500     /* 信息显示时间 */
#define STAGE_FADEOUT_TIME  300     /* 淡出时间 */

/*===========================================================================
 * 私有函数
 *===========================================================================*/

/**
 * @brief 绘制居中文字
 */
static void draw_centered_text(uint16_t y, const char* text, uint16_t color)
{
    if (!text) return;
    
    int len = strlen(text);
    uint16_t x = (UI_SCREEN_WIDTH - len * 8) / 2;
    
    while (*text) {
        BG_lcd.ShowChar(x, y, *text, color);
        x += 8;
        text++;
    }
}

/**
 * @brief 绘制进度条
 */
static void draw_progress_bar(uint8_t progress)
{
    uint16_t bar_width = UI_SCREEN_WIDTH - 40;
    uint16_t bar_x = 20;
    uint16_t bar_y = UI_SCREEN_HEIGHT - 20;
    uint16_t bar_height = 6;
    
    /* 绘制背景 */
    BG_lcd.Box(bar_x, bar_y, bar_width, bar_height, UI_COLOR_DARK_GRAY);
    
    /* 绘制进度 */
    uint16_t fill_width = (bar_width * progress) / 100;
    if (fill_width > 0) {
        BG_lcd.Box(bar_x, bar_y, fill_width, bar_height, UI_BOOT_PROGRESS_COLOR);
    }
    
    /* 绘制进度消息 */
    if (progress_message) {
        draw_centered_text(bar_y - 12, progress_message, UI_COLOR_GRAY);
    }
}

/**
 * @brief 绘制默认Logo
 */
static void draw_default_logo(void)
{
    uint16_t logo_x = (UI_SCREEN_WIDTH - 64) / 2;
    uint16_t logo_y = 20;
    uint8_t i, j, k;
    
    /* 绘制大号 "BG" 文字 */
    /* B - 放大4倍 */
    for (i = 0; i < 8; i++) {
        uint8_t row = default_logo[i];
        for (j = 0; j < 8; j++) {
            if (row & (0x80 >> j)) {
                /* 4x4像素块 */
                for (k = 0; k < 4; k++) {
                    BG_lcd.DrawLine(logo_x + j * 4, logo_y + i * 4 + k,
                                   logo_x + j * 4 + 3, logo_y + i * 4 + k,
                                   UI_COLOR_CYAN);
                }
            }
        }
    }
    
    /* G */
    for (i = 0; i < 8; i++) {
        uint8_t row = default_logo[8 + i];
        for (j = 0; j < 8; j++) {
            if (row & (0x80 >> j)) {
                for (k = 0; k < 4; k++) {
                    BG_lcd.DrawLine(logo_x + 32 + j * 4, logo_y + i * 4 + k,
                                   logo_x + 32 + j * 4 + 3, logo_y + i * 4 + k,
                                   UI_COLOR_GREEN);
                }
            }
        }
    }
}

/**
 * @brief 绘制Logo阶段
 */
static void draw_stage_logo(void)
{
    /* 清屏 */
    BG_lcd.Clear(UI_BOOT_BG_COLOR);
    
    /* 绘制Logo */
    if (boot_config.logo_data) {
        uint16_t logo_x = (UI_SCREEN_WIDTH - boot_config.logo_width) / 2;
        uint16_t logo_y = (UI_SCREEN_HEIGHT - boot_config.logo_height) / 2 - 20;
        BG_lcd.ShowImage(logo_x, logo_y, boot_config.logo_width, 
                         boot_config.logo_height, boot_config.logo_data);
    } else {
        draw_default_logo();
    }
}

/**
 * @brief 绘制信息阶段
 */
static void draw_stage_info(void)
{
    uint16_t y = 60;
    
    /* 产品名称 */
    if (boot_config.product_name) {
        draw_centered_text(y, boot_config.product_name, UI_COLOR_WHITE);
        y += 12;
    }
    
    /* 版本号 */
    if (boot_config.version) {
        draw_centered_text(y, boot_config.version, UI_COLOR_GRAY);
        y += 12;
    }
    
    /* 版权信息 */
    if (boot_config.copyright) {
        draw_centered_text(UI_SCREEN_HEIGHT - 10, boot_config.copyright, UI_COLOR_DARK_GRAY);
    }
}

/**
 * @brief 绘制进度阶段
 */
static void draw_stage_progress(void)
{
    if (boot_config.show_progress) {
        draw_progress_bar(current_progress);
    }
}

/*===========================================================================
 * API 实现
 *===========================================================================*/

void UI_BootScreen_Init(const UI_BootConfig_t* config)
{
    if (config) {
        memcpy(&boot_config, config, sizeof(UI_BootConfig_t));
    } else {
        /* 默认配置 */
        memset(&boot_config, 0, sizeof(UI_BootConfig_t));
        boot_config.logo_data = NULL;  /* 使用默认Logo */
        boot_config.logo_width = 64;
        boot_config.logo_height = 32;
        boot_config.product_name = "BG Card Mini";
        boot_config.version = "v1.0.0";
        boot_config.copyright = "(C) 2025 BG Team";
        boot_config.display_time = UI_BOOT_DURATION;
        boot_config.show_progress = true;
    }
    
    boot_stage = UI_BOOT_STAGE_INIT;
    stage_timer = 0;
    current_progress = 0;
    progress_message = NULL;
    initialized = true;
}

void UI_BootScreen_Start(void)
{
    if (!initialized) {
        UI_BootScreen_Init(NULL);
    }
    
    boot_stage = UI_BOOT_STAGE_LOGO;
    stage_timer = 0;
    current_progress = 0;
    
    /* 绘制Logo阶段 */
    draw_stage_logo();
}

bool UI_BootScreen_Update(uint16_t delta_ms)
{
    if (boot_stage == UI_BOOT_STAGE_DONE) {
        return false;
    }
    
    stage_timer += delta_ms;
    
    switch (boot_stage) {
        case UI_BOOT_STAGE_LOGO:
            if (stage_timer >= STAGE_LOGO_TIME) {
                boot_stage = UI_BOOT_STAGE_INFO;
                stage_timer = 0;
                draw_stage_info();
            }
            break;
            
        case UI_BOOT_STAGE_INFO:
            if (stage_timer >= STAGE_INFO_TIME) {
                boot_stage = UI_BOOT_STAGE_PROGRESS;
                stage_timer = 0;
            }
            break;
            
        case UI_BOOT_STAGE_PROGRESS:
            draw_stage_progress();
            
            /* 自动增加进度 (如果没有外部设置) */
            if (current_progress < 100) {
                current_progress += 2;
                if (current_progress > 100) current_progress = 100;
            }
            
            /* 进度完成后进入淡出 */
            if (current_progress >= 100 && stage_timer >= boot_config.display_time) {
                boot_stage = UI_BOOT_STAGE_FADEOUT;
                stage_timer = 0;
            }
            break;
            
        case UI_BOOT_STAGE_FADEOUT:
            /* 简单的淡出效果 - 逐渐变暗 */
            if (stage_timer >= STAGE_FADEOUT_TIME) {
                boot_stage = UI_BOOT_STAGE_DONE;
                BG_lcd.Clear(UI_COLOR_BLACK);
            }
            break;
            
        default:
            break;
    }
    
    return (boot_stage != UI_BOOT_STAGE_DONE);
}

void UI_BootScreen_SetProgress(uint8_t progress, const char* message)
{
    if (progress > 100) progress = 100;
    current_progress = progress;
    progress_message = message;
}

void UI_BootScreen_Skip(void)
{
    boot_stage = UI_BOOT_STAGE_DONE;
    BG_lcd.Clear(UI_COLOR_BLACK);
}

bool UI_BootScreen_IsDone(void)
{
    return (boot_stage == UI_BOOT_STAGE_DONE);
}

UI_BootStage_t UI_BootScreen_GetStage(void)
{
    return boot_stage;
}

void UI_BootScreen_SetLogo(const uint8_t* data, uint16_t width, uint16_t height)
{
    boot_config.logo_data = data;
    boot_config.logo_width = width;
    boot_config.logo_height = height;
}

void UI_BootScreen_SetProductInfo(const char* name, const char* version)
{
    boot_config.product_name = name;
    boot_config.version = version;
}
