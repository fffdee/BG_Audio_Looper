/**
 * @file    view_home.c
 * @brief   Home View - Main idle screen implementation (New Architecture)
 * @author  BG Card Team
 * @date    2025-01-08
 */
#include "picture.h"
#include "view_home.h"
#include "../components/comp_statusbar.h"
#include "bg_lcd.h"
#include "gui_tool.h"
#include <string.h>

/*===========================================================================
 * 绉佹湁鍙橀噺
 *===========================================================================*/

static UI_View_t s_home_view;
static void (*s_icon_callbacks[HOME_ICON_COUNT])(void) = {NULL};

/* 鍥炬爣璧勬簮 */
static const unsigned char* s_icons[HOME_ICON_COUNT] = {
    gImage_setting,
    gImage_hardware,
    gImage_music,
    gImage_looper
};

static const char* s_icon_titles[HOME_ICON_COUNT] = {
    "SysSet",
    "A-CTRL",
    "Drum",
    "Looper"
};

static const uint8_t s_title_lengths[HOME_ICON_COUNT] = {6, 6, 4, 6};

/* 按钮到图标的映射 (四个按键直接映射到四个应用) */
static const HomeIconID_t s_button_mapping[4] = {
    HOME_ICON_SETTINGS,     /* UI_BTN_UP -> Settings */
    HOME_ICON_AUDIO_CTRL,   /* UI_BTN_DOWN -> Audio Control */
    HOME_ICON_DRUM,         /* UI_BTN_ENTER -> Drum */
    HOME_ICON_LOOPER        /* UI_BTN_BACK -> Looper */
};

/*===========================================================================
 * 绉佹湁鍑芥暟
 *===========================================================================*/

/* Default Logo bitmap (8x16, "BG" text) */
static const uint8_t default_logo[] = {
    /* B */
    0x7E, 0x42, 0x42, 0x7C, 0x42, 0x42, 0x7E, 0x00,
    /* G */
    0x3C, 0x42, 0x40, 0x4E, 0x42, 0x42, 0x3C, 0x00,
};

/**
 * @brief Draw default BG logo
 */
void draw_default_logo(void)
{
    uint16_t logo_x = (UI_SCREEN_WIDTH - 64) / 2;
    uint16_t logo_y = 20;
    uint8_t i, j, k;
    
    /* Draw large "BG" text */
    /* B - scale up 4x */
    for (i = 0; i < 8; i++) {
        uint8_t row = default_logo[i];
        for (j = 0; j < 8; j++) {
            if (row & (0x80 >> j)) {
                /* 4x4 pixel block */
                for (k = 0; k < 4; k++) {
                    BG_lcd.DrawLine(logo_x + j * 4, logo_y + i * 4 + k,
                                   logo_x + j * 4 + 3, logo_y + i * 4 + k,
                                   0x07FF);  /* UI_COLOR_CYAN */
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
                                   0x07E0);  /* UI_COLOR_GREEN */
                }
            }
        }
    }
}

/**
 * @brief 显示主图标 (与老版本一致)
 */
static void show_main_icon(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const unsigned char* icon)
{
    uint16_t row, col;
    uint32_t idx;
    uint16_t color;

    /* 跳过头8字节 (图片头信息) */
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            idx = 8 + (row * w + col) * 2;  /* 8字节头 + 像素数据 */
            color = icon[idx] | (icon[idx + 1] << 8);
            BG_lcd.DrawPoint(x + col, y + row, color);
        }
    }
//	BGUI_tool.ShowImage(x,y,w,h,icon);

}

static void draw_banner(void)
{
    /* 绘制BanBox图片 (与老版本一致) */
    uint16_t y = UI_StatusBar_GetHeight();
    
    /* 显示BanBox图片: 160x59像素，位置(0, 12) */
    show_main_icon(0, y, 160, 59, gImage_BanBox);
}

static void draw_icon(uint8_t index, bool selected)
{
    if (index >= HOME_ICON_COUNT) return;
    
    uint16_t x = 4 + index * HOME_ICON_SPACING;
    uint16_t y = UI_SCREEN_HEIGHT - 52;
    
    const unsigned char* icon = s_icons[index];
    
    /* 绘制图标图像 (32x32) */
    uint16_t row, col;
    for (row = 0; row < HOME_ICON_HEIGHT; row++) {
        for (col = 0; col < HOME_ICON_WIDTH; col++) {
            uint32_t idx = (row * HOME_ICON_WIDTH + col) * 2;
            uint16_t color = icon[idx] | (icon[idx + 1] << 8);
            BG_lcd.DrawPoint(x + col, y + row, color);
        }
    }
   // BGUI_tool.ShowImage
    /* 不再绘制选择框 - 四个按钮直接映射到四个应用 */
    
    /* 绘制标题文本 */
    uint8_t title_len = s_title_lengths[index];
    uint8_t string_x = (HOME_ICON_SPACING - 6 * title_len) / 2;
    BGUI_tool.ShowString(string_x + index * HOME_ICON_SPACING, 
                         UI_SCREEN_HEIGHT - 16, 
                         (uint8_t*)s_icon_titles[index], 
                         UI_WHITE);
}

static void draw_all_icons(void)
{
    uint8_t i;
    for (i = 0; i < HOME_ICON_COUNT; i++) {
        draw_icon(i, false);  /* 不再使用选择状态 */
    }
}

/*===========================================================================
 * View 鍥炶皟
 *===========================================================================*/

static void home_on_enter(void)
{
    s_home_view.visible = true;
    s_home_view.dirty = true;
}

static void home_on_exit(void)
{
    s_home_view.visible = false;
}

static void home_on_update(uint16_t delta_ms)
{
    (void)delta_ms;
    /* Home 瑙嗗浘鐩墠涓嶉渶瑕佸畾鏃舵洿鏂伴�杈�*/
}

static void home_on_draw(void)
{
    /* 使用UI系统的背景色清屏 */
    extern const BG_UI_t BG_UI;
    BG_lcd.Clear(BG_UI.GetBackgroundColor());
    
    /* 绘制 Banner (BanBox图片) */
    draw_banner();
    
    /* 绘制状态栏 (使用comp_statusbar的完整版本) */
    UI_StatusBar_Draw();
    
    /* 绘制图标 */
    draw_all_icons();
}

static bool home_on_button(UI_BtnEventData_t* event)
{
    if (event->event != UI_BTN_EVT_CLICK) {
        return false;
    }
    
    /* 四个按钮直接映射到四个应用 */
    HomeIconID_t icon_id;
    
    switch (event->id) {
        case UI_BTN_UP:
            icon_id = HOME_ICON_SETTINGS;
            break;
            
        case UI_BTN_DOWN:
            icon_id = HOME_ICON_AUDIO_CTRL;
            break;
            
        case UI_BTN_ENTER:
            icon_id = HOME_ICON_DRUM;
            break;
            
        case UI_BTN_BACK:
            icon_id = HOME_ICON_LOOPER;
            break;
            
        default:
            return false;
    }
    
    /* 执行对应图标的回调 */
    if (s_icon_callbacks[icon_id]) {
        s_icon_callbacks[icon_id]();
    } else {
        /* 默认行为: 进入菜单 (Settings) 或显示提示 */
        if (icon_id == HOME_ICON_SETTINGS) {
            BG_UI.SetState(UI_STATE_MENU);
        } else {
            BG_UI.ShowPopup("Info", "Not Implemented", 1500);
        }
    }
    
    return true;
}

/*===========================================================================
 * 鍏叡 API
 *===========================================================================*/

UI_View_t* View_Home_Create(void)
{
    memset(&s_home_view, 0, sizeof(s_home_view));
    
    s_home_view.name = "Home";
    s_home_view.on_enter = home_on_enter;
    s_home_view.on_exit = home_on_exit;
    s_home_view.on_update = home_on_update;
    s_home_view.on_draw = home_on_draw;
    s_home_view.on_button = home_on_button;
    s_home_view.visible = false;
    s_home_view.dirty = true;
    
    /* 娉ㄥ唽鍒�IDLE 鐘舵� */
    BG_UI.RegisterView(UI_STATE_IDLE, &s_home_view);
    
    return &s_home_view;
}

void View_Home_Destroy(void)
{
    BG_UI.UnregisterView(&s_home_view);
}

void View_Home_SetIconCallback(HomeIconID_t icon_id, void (*callback)(void))
{
    if (icon_id < HOME_ICON_COUNT) {
        s_icon_callbacks[icon_id] = callback;
    }
}

void View_Home_Refresh(void)
{
    s_home_view.dirty = true;
}
