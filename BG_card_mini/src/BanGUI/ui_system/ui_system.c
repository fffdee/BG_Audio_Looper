/**
 * @file    ui_system.c
 * @brief   UI绯荤粺涓绘ā鍧楀疄鐜�
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
 * 绉佹湁鍙橀噺
 *===========================================================================*/

static UI_SystemConfig_t sys_config;
static UI_SystemState_t sys_state;
static UI_SystemState_t prev_state;
static UI_Menu_t* main_menu;
static bool system_ready;
static uint32_t idle_timer;
static uint8_t selected_icon = 0;  // 褰撳墠閫変腑鐨勫浘鏍�(0-3)

/* 寮瑰嚭妗嗙姸鎬�*/
static struct {
    bool active;
    const char* title;
    const char* message;
    uint16_t duration;
    uint32_t timer;
} popup;

/*===========================================================================
 * 绉佹湁鍑芥暟
 *===========================================================================*/

/**
 * @brief 鏄剧ず80x80鍥炬爣鐨勪腑闂�4x48閮ㄥ垎
 * @param x 鏄剧ず浣嶇疆X
 * @param y 鏄剧ず浣嶇疆Y
 * @param src 80x80鍥炬爣鏁版嵁 (RGB565, 12800瀛楄妭)
 */
static void show_icon_cropped(uint16_t x, uint16_t y, const unsigned char* src)
{
    /* 80x80鍥炬爣涓鍓�4x48鐨勫亸绉�*/
    const uint16_t crop_x = (80 - 64) / 2;  /* 8 */
    const uint16_t crop_y = (80 - 48) / 2;  /* 16 */
    const uint16_t src_w = 80;
    uint16_t row, col;
    /* 閫愯鏄剧ず瑁佸壀鍖哄煙 */
    for (row = 0; row < 48; row++) {
        for (col = 0; col < 64; col++) {
            /* 璁＄畻婧愬浘鍍忎腑鐨勪綅缃�*/
            uint32_t src_idx = ((crop_y + row) * src_w + (crop_x + col)) * 2;
            uint16_t color = src[src_idx]| (src[src_idx + 1] << 8) ;
            BG_lcd.DrawPoint(x + col, y + row, color);
        }
    }
}

/**
 * @brief  显示主图标（基于自定义DrawPoint画点函数实现）
 * @param  x      图标显示起始X坐标（LCD屏幕坐标系，横向）
 * @param  y      图标显示起始Y坐标（LCD屏幕坐标系，纵向）
 * @param  w      图标宽度（像素，横向像素数）
 * @param  h      图标高度（像素，纵向像素数）
 * @param  src    图标图像数据指针（16位RGB565格式，低位在前，与image2lcd配置匹配）
 * @retval 无
 */
static void show_main_icon(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const unsigned char* src)
{
    uint16_t img_row, img_col;  // 图标自身的行、列索引（与图标尺寸对应，非屏幕坐标）
    uint8_t picL, picH;         // 图像像素数据的低字节、高字节
    uint16_t color;             // 组合后的16位RGB565颜色值（适配DrawPoint函数的颜色参数）

    /* 双层循环：先行后列（贴合人眼视觉顺序和LCD显示逻辑，避免图像拉扯/错位）
     * img_row：遍历图标自身的每一行（对应屏幕Y轴方向，共h行）
     * img_col：遍历图标当前行的每一列（对应屏幕X轴方向，共w列）
     */
    for (img_row = 0; img_row < h; img_row++)
    {
        for (img_col = 0; img_col < w; img_col++)
        {
            // 1. 计算当前像素在图像数据中的偏移量（每个像素2字节，行优先存储）
            uint32_t data_offset = (uint32_t)img_row * w * 2 + (uint32_t)img_col * 2;

            // 2. 提取RGB565格式的高低字节（低位在前，与你的图像数据格式匹配）
            picL = src[data_offset];      // 低字节（先存储，对应颜色的低8位）
            picH = src[data_offset + 1];  // 高字节（后存储，对应颜色的高8位）

            // 3. 组合为16位颜色值（适配DrawPoint函数的颜色参数类型）
            color = (uint16_t)(picH << 8) | picL;

            // 4. 调用你的DrawPoint画点函数（关键：修正坐标映射关系，避免错位）
            // 屏幕X坐标 = 起始X + 图标当前列（横向偏移）
            // 屏幕Y坐标 = 起始Y + 图标当前行（纵向偏移）
            BG_lcd.DrawPoint(x + img_col, y + img_row, color);
        }
    }
}

/**
 * @brief 缁樺埗绌洪棽鐣岄潰 (涓荤晫闈� - 4涓浘鏍囬摵婊″睆骞�鏃犵姸鎬佹爮)
 * 甯冨眬: 2x2, 姣忎釜鍥炬爣鍖哄煙80x64, 鍥炬爣64x48灞呬腑
 */
static void draw_idle_screen(void)
{
    // 先清屏
    BG_lcd.Box(0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT, UI_COLOR_BLACK);

    // 绘制状态栏
    UI_StatusBar_Draw();
    BG_lcd.Box(0,UI_STATUSBAR_HEIGHT,UI_SCREEN_WIDTH,50,0xFFFF);
    BG_lcd.Box(2,UI_STATUSBAR_HEIGHT+2,UI_SCREEN_WIDTH-4,46,0x0000);
    draw_default_logo();

//    // 紧凑布局参数
//    const uint16_t cell_w = 76;   // 每个单元格宽度 152/2，左右各留4像素边距
//    const uint16_t cell_h = 58;   // 每个单元格高度 (128-12)/2=58
//    const uint16_t icon_w = 64;
//    const uint16_t icon_h = 48;
//    const uint16_t offset_x = (cell_w - icon_w) / 2;  // 6
//    const uint16_t offset_y = (cell_h - icon_h) / 2;  // 5
//    const uint16_t base_x = 4; // 左右边距
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
    uint8_t i;
    for (i = 0; i < 4; i++) {
         show_main_icon(4+i*40,UI_SCREEN_HEIGHT-52,32,32, icons[i]);
    }

//    // 绘制选中框
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
 * @brief 缁樺埗寮瑰嚭妗�
 */
static void draw_popup(void)
{
    uint16_t box_w = 140;
    uint16_t box_h = 60;
    uint16_t box_x = (UI_SCREEN_WIDTH - box_w) / 2;
    uint16_t box_y = (UI_SCREEN_HEIGHT - box_h) / 2;
    
    /* 缁樺埗鑳屾櫙 */
    BG_lcd.Box(box_x, box_y, box_w, box_h, UI_COLOR_DARK_GRAY);
    
    /* 缁樺埗杈规 */
    BG_lcd.DrawLine(box_x, box_y, box_x + box_w - 1, box_y, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x, box_y + box_h - 1, box_x + box_w - 1, box_y + box_h - 1, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x, box_y, box_x, box_y + box_h - 1, UI_COLOR_WHITE);
    BG_lcd.DrawLine(box_x + box_w - 1, box_y, box_x + box_w - 1, box_y + box_h - 1, UI_COLOR_WHITE);
    
    /* 缁樺埗鏍囬 */
    if (popup.title) {
        uint16_t title_x = box_x + (box_w - strlen(popup.title) * 8) / 2;
        const char* p = popup.title;
        while (*p) {
            BG_lcd.ShowChar(title_x, box_y + 8, *p, UI_COLOR_CYAN);
            title_x += 8;
            p++;
        }
    }
    
    /* 缁樺埗娑堟伅 */
    if (popup.message) {
        uint16_t msg_x = box_x + (box_w - strlen(popup.message) * 8) / 2;
        const char* p = popup.message;
        while (*p) {
            BG_lcd.ShowChar(msg_x, box_y + 28, *p, UI_COLOR_WHITE);
            msg_x += 8;
            p++;
        }
    }
    
    /* 缁樺埗鎻愮ず */
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
 * @brief 鍒囨崲鐘舵�
 */
static void change_state(UI_SystemState_t new_state)
{
    if (sys_state == new_state) return;
    
    prev_state = sys_state;
    sys_state = new_state;
    idle_timer = 0;
    
    /* 鐘舵�杩涘叆澶勭悊 */
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
 * API 瀹炵幇
 *===========================================================================*/

void UI_System_Init(const UI_SystemConfig_t* config)
{
    if (config) {
        memcpy(&sys_config, config, sizeof(UI_SystemConfig_t));
    } else {
        /* 榛樿閰嶇疆 */
        sys_config.skip_boot = false;
        sys_config.auto_statusbar = true;
        sys_config.idle_timeout = 0;  /* 绂佺敤绌洪棽瓒呮椂 */
    }
    
    /* 鍒濆鍖栧悇瀛愭ā鍧�*/
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
    /* 鎸夐敭鎵弿 */
    UI_Button_Scan(delta_ms);
    
    /* 澶勭悊鎸夐敭浜嬩欢 */
    UI_ButtonEventData_t event;
    while (UI_Button_GetEvent(&event)) {
        UI_System_HandleEvent(&event);
    }
    
    /* 鐘舵�鏇存柊 */
    switch (sys_state) {
        case UI_STATE_BOOT:
            if (!UI_BootScreen_Update(delta_ms)) {
                /* 寮�満鐢婚潰瀹屾垚 */
                system_ready = true;
                change_state(UI_STATE_IDLE);
            }
            break;
            
        case UI_STATE_IDLE:
            /* 鐘舵�鏍忔洿鏂�*/
            UI_StatusBar_Update();
            
            /* 绌洪棽瓒呮椂澶勭悊 */
            if (sys_config.idle_timeout > 0) {
                idle_timer += delta_ms;
                if (idle_timer >= sys_config.idle_timeout * 1000) {
                    /* 瓒呮椂澶勭悊 - 鍙互杩涘叆鐪佺數妯″紡绛�*/
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
    
    /* 寮�満鐢婚潰鏃舵寜浠绘剰閿烦杩�*/
    if (sys_state == UI_STATE_BOOT) {
        if (event->event == UI_BTN_EVENT_CLICKED) {
            UI_BootScreen_Skip();
            system_ready = true;
            change_state(UI_STATE_IDLE);
        }
        return;
    }
    
    /* 寮瑰嚭妗嗗鐞�*/
    if (sys_state == UI_STATE_POPUP) {
        if (event->event == UI_BTN_EVENT_CLICKED) {
            UI_System_ClosePopup();
        }
        return;
    }
    
    /* 鍙鐞嗗崟鍑诲拰闀挎寜浜嬩欢 */
    if (event->event != UI_BTN_EVENT_CLICKED && 
        event->event != UI_BTN_EVENT_LONG_PRESS &&
        event->event != UI_BTN_EVENT_REPEAT) {
        return;
    }
    
    idle_timer = 0;  /* 閲嶇疆绌洪棽璁℃椂 */
    
    switch (sys_state) {
        case UI_STATE_IDLE:
            /* 绌洪棽鐣岄潰锛氬鐞嗗浘鏍囧鑸�*/
            if (event->event == UI_BTN_EVENT_CLICKED) {
                switch (event->id) {
                    case UI_BTN_UP:
                        if (selected_icon >= 2) {
                            selected_icon -= 2;  // 鍚戜笂绉诲姩
                            draw_idle_screen();
                        }
                        break;
                    case UI_BTN_DOWN:
                        if (selected_icon <= 1) {
                            selected_icon += 2;  // 鍚戜笅绉诲姩
                            draw_idle_screen();
                        }
                        break;
                    case UI_BTN_ENTER:
                        /* 鏍规嵁閫変腑鐨勫浘鏍囨墽琛屾搷浣�*/
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
            /* 鑿滃崟鐣岄潰瀵艰埅 */
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
                    /* 濡傛灉鍦ㄦ牴鑿滃崟锛岃繑鍥炵┖闂茬晫闈�*/
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
