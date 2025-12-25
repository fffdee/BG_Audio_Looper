/**
 * @file    ui_system.c
 * @brief   UI system main module
 * @author  BG Card Team
 * @date    2025-12-18
 */

#include "ui_system.h"
#include "bg_lcd.h"
#include "gui_tool.h"
#include <string.h>
#include "dac.h"

#include "picture.h"
/*===========================================================================
 * System private variables
 *===========================================================================*/

static UI_SystemConfig_t sys_config;
static UI_SystemState_t sys_state;
static UI_SystemState_t prev_state;
static UI_Menu_t* main_menu;
static bool system_ready;
static uint32_t idle_timer;
static uint8_t selected_icon = 0;  // Selected icon index (0-3)

/* Popup window structure */
static struct {
    bool active;
    const char* title;
    const char* message;
    uint16_t duration;
    uint32_t timer;
} popup;

/*===========================================================================
 * Private function declarations
 *===========================================================================*/

/**
 * @brief Display a cropped 64x48 icon from 80x80 source (centered)
 * @param x X coordinate on screen
 * @param y Y coordinate on screen
 * @param src 80x80 image data (RGB565, 12800 bytes)
 */
static void show_icon_cropped(uint16_t x, uint16_t y, const unsigned char* src)
{

    const uint16_t crop_x = (80 - 64) / 2;
    const uint16_t crop_y = (80 - 48) / 2;  /* 16 */
    const uint16_t src_w = 80;
    uint16_t row, col;
    /* 闁劘顢戦弰鍓с仛鐟佷礁澹�崠鍝勭厵 */
    for (row = 0; row < 48; row++) {
        for (col = 0; col < 64; col++) {
            /* 鐠侊紕鐣诲┃鎰禈閸嶅繋鑵戦惃鍕秴缂冿拷*/
            uint32_t src_idx = ((crop_y + row) * src_w + (crop_x + col)) * 2;
            uint16_t color = src[src_idx]| (src[src_idx + 1] << 8) ;
            BG_lcd.DrawPoint(x + col, y + row, color);
        }
    }
}

/**
 * @brief  Display main icon (based on custom DrawPoint implementation)
 * @param  x      Start X coordinate (LCD coordinate system, horizontal)
 * @param  y      Start Y coordinate (LCD coordinate system, vertical)
 * @param  w      Icon width (pixels, horizontal)
 * @param  h      Icon height (pixels, vertical)
 * @param  src    Icon image data pointer, 16-bit RGB565 format (low byte first, matches Image2lcd config)
 * @retval None
 */
static void show_main_icon(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const unsigned char* src)
{
    uint16_t img_row, img_col;  // 鍥炬爣鑷韩鐨勮銆佸垪绱㈠紩锛堜笌鍥炬爣灏哄瀵瑰簲锛岄潪灞忓箷鍧愭爣锛�
    uint8_t picL, picH;         // 鍥惧儚鍍忕礌鏁版嵁鐨勪綆瀛楄妭銆侀珮瀛楄妭
    uint16_t color;             // 缁勫悎鍚庣殑16浣峈GB565棰滆壊鍊硷紙閫傞厤DrawPoint鍑芥暟鐨勯鑹插弬鏁帮級

    /* 鍙屽眰寰幆锛氬厛琛屽悗鍒楋紙璐村悎浜虹溂瑙嗚椤哄簭鍜孡CD鏄剧ず閫昏緫锛岄伩鍏嶅浘鍍忔媺鎵�閿欎綅锛�
     * img_row锛氶亶鍘嗗浘鏍囪嚜韬殑姣忎竴琛岋紙瀵瑰簲灞忓箷Y杞存柟鍚戯紝鍏県琛岋級
     * img_col锛氶亶鍘嗗浘鏍囧綋鍓嶈鐨勬瘡涓�垪锛堝搴斿睆骞昘杞存柟鍚戯紝鍏眞鍒楋級
     */
    for (img_row = 0; img_row < h; img_row++)
    {
        for (img_col = 0; img_col < w; img_col++)
        {
            // 1. 璁＄畻褰撳墠鍍忕礌鍦ㄥ浘鍍忔暟鎹腑鐨勫亸绉婚噺锛堟瘡涓儚绱�瀛楄妭锛岃浼樺厛瀛樺偍锛�
            uint32_t data_offset = (uint32_t)img_row * w * 2 + (uint32_t)img_col * 2;

            // 2. 鎻愬彇RGB565鏍煎紡鐨勯珮浣庡瓧鑺傦紙浣庝綅鍦ㄥ墠锛屼笌浣犵殑鍥惧儚鏁版嵁鏍煎紡鍖归厤锛�
            picL = src[data_offset];      // 浣庡瓧鑺傦紙鍏堝瓨鍌紝瀵瑰簲棰滆壊鐨勪綆8浣嶏級
            picH = src[data_offset + 1];  // 楂樺瓧鑺傦紙鍚庡瓨鍌紝瀵瑰簲棰滆壊鐨勯珮8浣嶏級

            // 3. 缁勫悎涓�6浣嶉鑹插�锛堥�閰岲rawPoint鍑芥暟鐨勯鑹插弬鏁扮被鍨嬶級
            color = (uint16_t)(picH << 8) | picL;

            // 4. 璋冪敤浣犵殑DrawPoint鐢荤偣鍑芥暟锛堝叧閿細淇鍧愭爣鏄犲皠鍏崇郴锛岄伩鍏嶉敊浣嶏級
            // 灞忓箷X鍧愭爣 = 璧峰X + 鍥炬爣褰撳墠鍒楋紙妯悜鍋忕Щ锛�
            // 灞忓箷Y鍧愭爣 = 璧峰Y + 鍥炬爣褰撳墠琛岋紙绾靛悜鍋忕Щ锛�
            BG_lcd.DrawPoint(x + img_col, y + img_row, color);
        }
    }
}

/**
 * @brief Draw idle screen (default main interface, 4 icons, selection box, etc.)
 * Uses 2x2 grid, cell size 0x64, icon 64x48
 */
static void draw_idle_screen(void)
{
    // Clear screen
    BG_lcd.Box(0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT, UI_COLOR_BLACK);
    // Draw status bar
    UI_StatusBar_Draw();
    BG_lcd.Box(0,UI_STATUSBAR_HEIGHT,UI_SCREEN_WIDTH,50,0xFFFF);
    BG_lcd.Box(2,UI_STATUSBAR_HEIGHT+2,UI_SCREEN_WIDTH-4,46,0x0000);
    draw_default_logo();

//    // 绱у噾甯冨眬鍙傛暟
//    const uint16_t cell_w = 76;   // 姣忎釜鍗曞厓鏍煎搴�152/2锛屽乏鍙冲悇鐣�鍍忕礌杈硅窛
//    const uint16_t cell_h = 58;   // 姣忎釜鍗曞厓鏍奸珮搴�(128-12)/2=58
//    const uint16_t icon_w = 64;
//    const uint16_t icon_h = 48;
//    const uint16_t offset_x = (cell_w - icon_w) / 2;  // 6
//    const uint16_t offset_y = (cell_h - icon_h) / 2;  // 5
//    const uint16_t base_x = 4; // 宸﹀彸杈硅窛
//    const uint16_t base_y = UI_STATUSBAR_HEIGHT;

//    const uint16_t icon_x[4] = {base_x + offset_x, base_x + cell_w + offset_x, base_x + offset_x, base_x + cell_w + offset_x};
//    const uint16_t icon_y[4] = {base_y + offset_y, base_y + offset_y, base_y + cell_h + offset_y, base_y + cell_h + offset_y};
//
//    const unsigned char* icons[4] = {
//        gImage_setting,
//        gImage_acc_chart,
//        gImage_about,
//        gImage_vacal_setting
//    };
//    uint8_t i, t;
//    for (i = 0; i < 4; i++) {
//        show_icon_cropped(icon_x[i], icon_y[i], icons[i]);
//    }


        const unsigned char* icons[4] = {
            gImage_setting,
            gImage_hardware,
            gImage_music,
            gImage_looper
        };
        const unsigned char* title[4] = {
           "SysSet",
           "A-CTRL",
            "Drum",
           "Looper"
        };

    uint8_t i;
    uint8_t string_count[4] = {6,6,4,6};
    uint8_t string_x;
    for (i = 0; i < 4; i++) {

         show_main_icon(4+i*40,UI_SCREEN_HEIGHT-52,32,32, icons[i]);

         string_x = (40-6*string_count[i])/2;

         BGUI_tool.ShowString(string_x +i*40,UI_SCREEN_HEIGHT-16,title[i],0xFFFF);
    }

//    // 缁樺埗閫変腑妗�
//
//    uint16_t sel_col = selected_icon % 2;
//    uint16_t sel_row = selected_icon / 2;
//    uint16_t sel_x = base_x + sel_col * cell_w;
//    uint16_t sel_y = base_y + sel_row * cell_h;
//
//    for (t = 0; t < 2; t++) {
//        BG_lcd.DrawLine(sel_x + t, sel_y + t, sel_x + cell_w - 1 - t, sel_y + t, UI_COLOR_WHITE);
//        BG_lcd.DrawLine(sel_x + t, sel_y + cell_h - 1 - t, sel_x + cell_w - 1 - t, sel_y + cell_h - 1 - t, UI_COLOR_WHITE);
//        BG_lcd.DrawLine(sel_x + t, sel_y + t, sel_x + t, sel_y + cell_h - 1 - t, UI_COLOR_WHITE);
//        BG_lcd.DrawLine(sel_x + cell_w - 1 - t, sel_y + t, sel_x + cell_w - 1 - t, sel_y + cell_h - 1 - t, UI_COLOR_WHITE);
//    }
}

/**
 * @brief Draw popup window
 */
static void draw_popup(void)
{
    uint16_t box_w = 140;
    uint16_t box_h = 60;
    uint16_t box_x = (UI_SCREEN_WIDTH - box_w) / 2;
    uint16_t box_y = (UI_SCREEN_HEIGHT - box_h) / 2;
    
    /* Draw popup background */
    BG_lcd.Box(box_x, box_y, box_w, box_h, UI_COLOR_DARK_GRAY);
    /* Draw popup border */
    BG_lcd.DrawLine(box_x, box_y, box_x + box_w - 1, box_y, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x, box_y + box_h - 1, box_x + box_w - 1, box_y + box_h - 1, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x, box_y, box_x, box_y + box_h - 1, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x + box_w - 1, box_y, box_x + box_w - 1, box_y + box_h - 1, UI_COLOR_WHITE);
    
    /* Draw popup title */
    if (popup.title) {
        uint16_t title_x = box_x + (box_w - strlen(popup.title) * 8) / 2;
        const char* p = popup.title;
        while (*p) {
            BG_lcd.ShowChar(title_x, box_y + 8, *p, UI_COLOR_CYAN);
            title_x += 8;
            p++;
        }
    }
    
    /* Draw popup message */
    if (popup.message) {
        uint16_t msg_x = box_x + (box_w - strlen(popup.message) * 8) / 2;
        const char* p = popup.message;
        while (*p) {
            BG_lcd.ShowChar(msg_x, box_y + 28, *p, UI_COLOR_WHITE);
            msg_x += 8;
            p++;
        }
    }
    
    /* Draw popup hint */
    const char* hint = "[OK] Close";
    uint16_t hint_x = box_x + (box_w - strlen(hint) * 8) / 2;
    const char* p = hint;
    while (*p) {
        BG_lcd.ShowChar(hint_x, box_y + box_h - 14, *p, UI_COLOR_GRAY);
        hint_x += 8;
        p++;
    }
}

/**
 * @brief Change system state
 */
static void change_state(UI_SystemState_t new_state)
{
    if (sys_state == new_state) return;
    
    prev_state = sys_state;
    sys_state = new_state;
    idle_timer = 0;
    
    /* Handle state transition */
    switch (new_state) {
        case UI_STATE_IDLE:
            UI_Menu_SetVisible(false);
            UI_StatusBar_Draw();
            draw_idle_screen();
            AudioDAC_FadeEnable(DAC0);
            AudioDAC_Run(DAC0);
            break;
            
        case UI_STATE_MENU:
            UI_Menu_SetVisible(true);
            UI_StatusBar_Draw();
            UI_Menu_Draw();
            break;
            
        case UI_STATE_POPUP:
            draw_popup();
            break;
            
        default:
            break;
    }
}

/*===========================================================================
 * API Implementation
 *===========================================================================*/

void UI_System_Init(const UI_SystemConfig_t* config)
{
    if (config) {
        memcpy(&sys_config, config, sizeof(UI_SystemConfig_t));
    } else {
        /* Default configuration */
        sys_config.skip_boot = false;
        sys_config.auto_statusbar = true;
        sys_config.idle_timeout = 0;  /* Idle timeout disabled */
    }
    /* Initialize submodules */
    UI_Button_Init();
    UI_StatusBar_Init();
    UI_Menu_Init();
    UI_BootScreen_Init(NULL);
    sys_state = UI_STATE_BOOT;
    prev_state = UI_STATE_BOOT;
    main_menu = NULL;
    system_ready = false;
    idle_timer = 0;
    memset(&popup, 0, sizeof(popup));
}

void UI_System_Start(void)
{
    if (sys_config.skip_boot) {
        system_ready = true;
        change_state(UI_STATE_IDLE);
    } else {
        sys_state = UI_STATE_BOOT;
        UI_BootScreen_Start();
    }
}

void UI_System_Update(uint16_t delta_ms)
{
    /* Button scan */
    UI_Button_Scan(delta_ms);
    /* Handle button events */
    UI_ButtonEventData_t event;
    while (UI_Button_GetEvent(&event)) {
        UI_System_HandleEvent(&event);
    }
    /* State machine */
    switch (sys_state) {
        case UI_STATE_BOOT:
            if (!UI_BootScreen_Update(delta_ms)) {
                /* Boot screen finished */
                system_ready = true;
                change_state(UI_STATE_IDLE);
            }
            break;
        case UI_STATE_IDLE:
            /* Update status bar */
            UI_StatusBar_Update();
            /* Idle timeout check */
            if (sys_config.idle_timeout > 0) {
                idle_timer += delta_ms;
                if (idle_timer >= sys_config.idle_timeout * 1000) {
                    /* TODO: handle idle timeout - e.g., enter sleep or screensaver */
                    idle_timer = 0;
                }
            }
            break;
        case UI_STATE_MENU:
            UI_StatusBar_Update();
            UI_Menu_Update();
            break;
        case UI_STATE_POPUP:
            if (popup.duration > 0) {
                popup.timer += delta_ms;
                if (popup.timer >= popup.duration) {
                    UI_System_ClosePopup();
                }
            }
            break;
        default:
            break;
    }
}

void UI_System_HandleEvent(UI_ButtonEventData_t* event)
{
    if (!event) return;
    /* Handle boot screen skip by button */
    if (sys_state == UI_STATE_BOOT) {
        if (event->event == UI_BTN_EVENT_CLICKED) {
            UI_BootScreen_Skip();
            system_ready = true;
            change_state(UI_STATE_IDLE);
        }
        return;
    }
    /* Handle popup close by button */
    if (sys_state == UI_STATE_POPUP) {
        if (event->event == UI_BTN_EVENT_CLICKED) {
            UI_System_ClosePopup();
        }
        return;
    }
    /* Only handle click/long/repeat events */
    if (event->event != UI_BTN_EVENT_CLICKED && 
        event->event != UI_BTN_EVENT_LONG_PRESS &&
        event->event != UI_BTN_EVENT_REPEAT) {
        return;
    }
    idle_timer = 0;  /* Reset idle timer */
    switch (sys_state) {
        case UI_STATE_IDLE:
            /* Handle icon selection and enter */
            if (event->event == UI_BTN_EVENT_CLICKED) {
                switch (event->id) {
                    case UI_BTN_UP:
                        if (selected_icon >= 2) {
                            selected_icon -= 2;  // Move up
                            draw_idle_screen();
                        }
                        break;
                    case UI_BTN_DOWN:
                        if (selected_icon <= 1) {
                            selected_icon += 2;  // Move down
                            draw_idle_screen();
                        }
                        break;
                    case UI_BTN_ENTER:
                        /* Handle icon enter action */
                        switch (selected_icon) {
                            case 0:  // Settings
                                UI_System_ShowMenu();
                                break;
                            case 1:  // Music
                                UI_System_ShowPopup("Music", "Coming Soon", 2000);
                                break;
                            case 2:  // About
                                UI_System_ShowPopup("About", "BG Card v1.0", 3000);
                                break;
                            case 3:  // Game
                                UI_System_ShowPopup("Game", "Coming Soon", 2000);
                                break;
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        case UI_STATE_MENU:
            /* Handle menu navigation */
            switch (event->id) {
                case UI_BTN_UP:
                    UI_Menu_Up();
                    break;
                case UI_BTN_DOWN:
                    UI_Menu_Down();
                    break;
                case UI_BTN_ENTER:
                    UI_Menu_Enter();
                    break;
                case UI_BTN_BACK:
                    /* If at root menu and not editing, hide menu */
                    if (UI_Menu_GetCurrent() == main_menu && !UI_Menu_IsEditing()) {
                        UI_System_HideMenu();
                    } else {
                        UI_Menu_Back();
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void UI_System_SetState(UI_SystemState_t state)
{
    change_state(state);
}

UI_SystemState_t UI_System_GetState(void)
{
    return sys_state;
}

void UI_System_ShowMenu(void)
{
    if (main_menu) {
        UI_Menu_SetRoot(main_menu);
    }
    change_state(UI_STATE_MENU);
}

void UI_System_HideMenu(void)
{
    change_state(UI_STATE_IDLE);
}

void UI_System_ShowPopup(const char* title, const char* message, uint16_t duration_ms)
{
    popup.active = true;
    popup.title = title;
    popup.message = message;
    popup.duration = duration_ms;
    popup.timer = 0;
    
    change_state(UI_STATE_POPUP);
}

void UI_System_ClosePopup(void)
{
    popup.active = false;
    change_state(prev_state);
    UI_System_Refresh();
}

void UI_System_Refresh(void)
{
    if (sys_config.auto_statusbar) {
        UI_StatusBar_Draw();
    }
    
    switch (sys_state) {
        case UI_STATE_IDLE:
            draw_idle_screen();
            break;
        case UI_STATE_MENU:
            UI_Menu_Draw();
            break;
        default:
            break;
    }
}

void UI_System_SetMainMenu(UI_Menu_t* menu)
{
    main_menu = menu;
}

UI_Menu_t* UI_System_GetMainMenu(void)
{
    return main_menu;
}

bool UI_System_IsReady(void)
{
    return system_ready;
}
