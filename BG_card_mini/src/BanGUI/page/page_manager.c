#include "bg_lcd.h"
#include "gui_tool.h"

#include "bg_list.h"
#include "page_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "debug.h"

#include "menu_slider.h"
#include "bg_menu_slider.h"
#include "../ui_system/icon.h"  // 引入图标声明
#include "../ui_system/ui_system.h"  // 引入ui_system
#include "../ui_system/ui_menu.h"    // 引入ui_menu

uint8_t data[9] = {33, 44, 55, 66, 77, 88, 99, 100, 111};
void welcome_page();
void home_page();
void list_page();
void list_page_in();
void bg_menu_slider_page();

BG_Page_Table table[MAX_PAGE] = {
    {"welcome page", WELCOME_PAGE, WELCOME_PAGE, WELCOME_PAGE, WELCOME_PAGE, WELCOME_PAGE, SETUP, welcome_page},
    {"home page", HOME_PAGE, WELCOME_PAGE, WELCOME_PAGE, WELCOME_PAGE, HOME_PAGE, SETUP, home_page},
    {"list page", LIST_PAGE, WELCOME_PAGE, WELCOME_PAGE, LIST_PAGE_IN, LIST_PAGE, SETUP, list_page},
    {"list page in", LIST_PAGE_IN, LIST_PAGE_IN, LIST_PAGE_IN, LIST_PAGE_IN, WELCOME_PAGE, SETUP, list_page_in},
    {"bg menu slider", BG_MENU_SLIDER_PAGE, WELCOME_PAGE, WELCOME_PAGE, WELCOME_PAGE, BG_MENU_SLIDER_PAGE, SETUP, bg_menu_slider_page},
};

#if 0  // Unused - old menu initialization// Main menu slider instance
static IconMenuSlider main_menu;

static void menu_list_callback(void) {
    BG_page.SetPage(&BG_page, LIST_PAGE);
}

static void menu_setup_callback(void) {
   BGUI_tool.ShowString(10, 100, (uint8_t*)"Setup Selected!", 0xFFFF);
}

static void menu_bt_callback(void) {
    BGUI_tool.ShowString(10, 100, (uint8_t*)"BT Selected!", 0x07E0);
}

static void menu_audio_callback(void) {
    BGUI_tool.ShowString(10, 100, (uint8_t*)"Audio Selected!", 0xF800);
}
#endif
/* Unused for now
static void menu_system_callback(void) {
    BGUI_tool.ShowString(10, 100, (uint8_t*)"System Selected!", 0x001F);
}
*/

// BG_MenuSlider 婕旂ず鎺т欢
static BG_MenuSlider demo_menu_slider;
static uint32_t last_time_ms = 0;

static void demo_music_callback(void) {
    BG_lcd.Clear(0x0000);
    BGUI_tool.ShowString(30, 60, (uint8_t*)"Music Player", 0x07E0);
    BGUI_tool.ShowString(20, 80, (uint8_t*)"Press Exit to Back", 0xFFFF);
}

static void demo_games_callback(void) {
    BG_lcd.Clear(0x0000);
    BGUI_tool.ShowString(30, 60, (uint8_t*)"Game Center", 0xF800);
    BGUI_tool.ShowString(20, 80, (uint8_t*)"Press Exit to Back", 0xFFFF);
}

static void demo_settings_callback(void) {
    BG_lcd.Clear(0x0000);
    BGUI_tool.ShowString(30, 60, (uint8_t*)"Settings", 0xFFE0);
    BGUI_tool.ShowString(20, 80, (uint8_t*)"Press Exit to Back", 0xFFFF);
}

static void demo_photo_callback(void) {
    BG_lcd.Clear(0x0000);
    BGUI_tool.ShowString(30, 60, (uint8_t*)"Photo Album", 0xF81F);
    BGUI_tool.ShowString(20, 80, (uint8_t*)"Press Exit to Back", 0xFFFF);
}

static void demo_tools_callback(void) {
    BG_lcd.Clear(0x0000);
    BGUI_tool.ShowString(30, 60, (uint8_t*)"Tools", 0x001F);
    BGUI_tool.ShowString(20, 80, (uint8_t*)"Press Exit to Back", 0xFFFF);
}

/* Unused - for old menu system
static void menu_bg_slider_callback(void) {
    BG_page.SetPage(&BG_page, BG_MENU_SLIDER_PAGE);
}
*/

// 锟斤拷始锟斤拷锟剿碉拷
#if 0  // Unused - old menu initialization
static void home_menu_init(void) {
    static uint8_t is_initialized = 0;
    if (!is_initialized) {

        IconMenuSlider_Init(&main_menu,
                          LCD_WIDTH / 2,  // 选锟斤拷锟斤拷锟斤拷锟斤拷锟绞�
						  (LCD_HEIGHT - MENU_ITEM_HEIGHT) / 2,  // 锟斤拷直锟斤拷锟斤拷
                          0x001F,  // 选锟斤拷锟斤拷呖锟缴�锟斤拷色)
                          0x7BEF); // 锟斤拷通锟斤拷呖锟缴�锟斤拷色)

        // 娣诲姞鑿滃崟椤�鍚嶇О銆佸浘鏍囥�鍥炬爣瀹介珮銆佸洖璋�
        IconMenuSlider_AddItem(&main_menu, "List", gImage_qq, 40, 40, menu_list_callback);
        IconMenuSlider_AddItem(&main_menu, "BG Slider", gImage_qq, 40, 40, menu_bg_slider_callback);  // 鏂板婕旂ず椤甸潰
        IconMenuSlider_AddItem(&main_menu, "Setup", gImage_qq, 40, 40, menu_setup_callback);
        IconMenuSlider_AddItem(&main_menu, "BT", gImage_qq, 40, 40, menu_bt_callback);
        IconMenuSlider_AddItem(&main_menu, "Audio", gImage_qq, 40, 40, menu_audio_callback);

        is_initialized = 1;
    }
}
#endif

// 声明外部函数
extern UI_Menu_t* UI_GetWelcomeMenu(void);

/**
 * @brief 欢迎页面 - 使用ui_system的菜单系统
 * 显示开机后的欢迎界面，包含4个图标菜单项
 */
void welcome_page() {
    static uint8_t ui_initialized = 0;
    static UI_Menu_t* welcome_menu = NULL;
    
    // 页面初始化
    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1) {
        // 初始化UI系统
        if (!ui_initialized) {
            UI_SystemConfig_t config = {
                .skip_boot = true,
                .auto_statusbar = false,
                .idle_timeout = 0
            };
            UI_System_Init(&config);
            
            // 获取欢迎菜单
            welcome_menu = UI_GetWelcomeMenu();
            
            // 初始化并设置为当前菜单
            UI_Menu_Init();
            UI_Menu_SetRoot(welcome_menu);
            UI_Menu_SetVisible(true);
            
            ui_initialized = 1;
        }
        
        // 清屏并绘制菜单
        BG_lcd.Clear(0x0000);
        UI_Menu_Draw();
        
        BG_page.Data.table[BG_page.Data.running_id].setup = 0;
    }
    
    // 处理按键输入
    if (BG_page.Data.last_pressed) {  // Up
        UI_Menu_Up();
        UI_Menu_Draw();
        BG_page.Data.last_pressed = 0;
    }
    
    if (BG_page.Data.next_pressed) {  // Down
        UI_Menu_Down();
        UI_Menu_Draw();
        BG_page.Data.next_pressed = 0;
    }
    
    if (BG_page.Data.enter_pressed) {  // Enter
        UI_Menu_Enter();
        UI_Menu_Draw();
        BG_page.Data.enter_pressed = 0;
    }
    
    if (BG_page.Data.exit_pressed) {  // Exit/Back
        UI_Menu_Back();
        UI_Menu_Draw();
        BG_page.Data.exit_pressed = 0;
    }
    
    // 更新菜单系统
    UI_System_Update(20); // 假设20ms刷新率
}

void home_page() {
    static uint8_t ui_drawn = 0;
    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1) {
//#ifdef USE_FRAME_BUFFER
//        BG_lcd.Clear(0x0000);
//#else
//        BG_lcd.Clear(0x0000);
//#endif
//        home_menu_init();     // 鍒濆鍖栬彍鍗�        BG_page.Data.table[BG_page.Data.running_id].setup = 0;
//        ui_drawn = 0; // 閲嶇疆UI缁樺埗鏍囪
//        need_redraw = 1;
    }

    // Draw fixed UI elements only once
    if (!ui_drawn) {
        BGUI_tool.ShowString((LCD_WIDTH - 9 * 8) / 2, 10, (uint8_t*)"MAIN MENU", 0xFFFF);
        BGUI_tool.ShowString(5, LCD_HEIGHT - 15, (uint8_t*)"L/R:Move Enter:OK", 0x7BEF);
        ui_drawn = 1;
    }
//
//    // 澶勭悊鎸夐敭杈撳叆(宸﹂敭/鍙抽敭/纭)
//    if (BG_page.Data.last_pressed) {
//        uint8_t new_idx = (main_menu.current_idx - 1 + main_menu.item_count) % main_menu.item_count;
//        IconMenuSlider_SetInstant(&main_menu, new_idx);
//        BG_page.Data.last_pressed = 0;
//        need_redraw = 1;
//    }
//    if (BG_page.Data.next_pressed) {
//        uint8_t new_idx = (main_menu.current_idx + 1) % main_menu.item_count;
//        IconMenuSlider_SetInstant(&main_menu, new_idx);
//        BG_page.Data.next_pressed = 0;
//        need_redraw = 1;
//    }
//    if (BG_page.Data.enter_pressed) {
//        IconMenuSlider_Enter(&main_menu);
//        BG_page.Data.enter_pressed = 0;
//        ui_drawn = 0; // 閲嶇疆UI鏍囪锛岄〉闈㈠垏鎹㈠悗闇�閲嶇粯
//        return;
//    }
////
//     if (need_redraw) {
//#ifdef USE_FRAME_BUFFER
//        // 甯х紦鍐叉ā寮忥細鍏堟竻闄よ彍鍗曞尯鍩燂紝鐒跺悗缁樺埗锛屾渶鍚庝竴娆℃�鍒锋柊鍒板睆骞�        IconMenuSlider_ClearArea(&main_menu);
//        IconMenuSlider_Draw(&main_menu);
//        BG_lcd.FlushFrameBuffer(); // 涓�鎬у埛鏂版暣涓睆骞�- 杩欓噷灏辨槸涓濇粦鐨勫叧閿紒
//#else
//        // 浼犵粺妯″紡锛氱洿鎺ョ粯鍒跺埌灞忓箷
//        IconMenuSlider_ClearArea(&main_menu);
//        IconMenuSlider_Draw(&main_menu);
//#endif
//        need_redraw = 0;
//    }
}

void list_page()
{

    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1)
    {
       //  printf("BG_page.Data.running_id is %d\n", BG_page.Data.running_id);
        // Running the Setup code.
        BG_lcd.Clear(0x00);
        BGUI_tool.ShowString(16, 16, (uint8_t*)"THIS BG LIST", 0xFFFF);
        BGUI_tool.ShowString(50, 32, (uint8_t*)"UI PAGE!", 0xFFFF);
        BGUI_tool.ShowString(20, 60, (uint8_t*)"Press Enter->Detail", 0xFFFF);
        BGUI_tool.ShowString(20, 80, (uint8_t*)"Press Exit->Home", 0xFFFF);
        BG_page.Data.table[BG_page.Data.running_id].setup = 0;
    }

    // 澶勭悊鎸夐敭杈撳叆
    if (BG_page.Data.enter_pressed) {
        BG_page.SetPage(&BG_page, LIST_PAGE_IN);
        BG_page.Data.enter_pressed = 0;
    }

    if (BG_page.Data.exit_pressed) {
        BG_page.SetPage(&BG_page, HOME_PAGE);
        BG_page.Data.exit_pressed = 0;
    }

      //printf("BG_page.Data.running_id is %d\n", BG_page.Data.running_id);
}

#ifdef DYNAMIC
 BG_List* List;
#else
 BG_List List;
#endif

void list_page_in()
{

    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1)
    {
        printf("BG_page.Data.running_id is %d\n", BG_page.Data.running_id);

        List = BG_List_Init("GUITAR", BGUI_tool.Update, BGUI_tool.Clear);
#ifdef DYNAMIC
if(List->Data.init_flag==1){
        List->Append(List, "Dist", data[0], "val");
        List->Append(List, "Delay", data[1], "km");
        List->Append(List, "Chors", data[2], "m/s");
        List->Append(List, "Reverb", data[3], "cc");
        List->Append(List, "Pitch", data[4], "tt");
        List->Append(List, "Change", data[5], "gg");
        List->Append(List, "KKGO", data[6], "ff");
        List->Append(List, "CS GO", data[7], "ie");
        List->Append(List, "CF", data[8], "gogo");
        List->Data.init_flag=0;
}
 #else
    if(List.Data.init_flag==1){
        List.Append(&List, "Dist", data[0], "val");
        List.Append(&List, "Delay", data[1], "km");
        List.Append(&List, "Chors", data[2], "m/s");
        List.Append(&List, "Reverb", data[3], "cc");
        List.Append(&List, "Pitch", data[4], "tt");
        List.Append(&List, "Change", data[5], "gg");
        List.Append(&List, "KKGO", data[6], "ff");
        List.Append(&List, "CS GO", data[7], "ie");
        List.Append(&List, "CF", data[8], "gogo");
        List.Data.init_flag=0;
    }
 #endif

    }
#ifdef DYNAMIC
    if(List->Exit(List)==1){

       List->Data.exit_flag=0;
       BG_page.Exit(&BG_page);
       BG_List_DeInit(List);


    }

    if(BG_page.Data.last_pressed==1){
        List->Up(List);
    }
    if(BG_page.Data.next_pressed==1){
        List->Down(List);
    }
    if(BG_page.Data.enter_pressed==1){
       List->Enter(List);
    }



     List->Timer_update(List);
     List->Show(List);
#else
    if(BG_page.Data.last_pressed==1){
        List.Up(&List);
    }
    if(BG_page.Data.next_pressed==1){
        List.Down(&List);
    }
    if(BG_page.Data.enter_pressed==1){
       List.Enter(&List);
    }
    if(List.Exit(&List)==1){

       List.Data.exit_flag=0;
       BG_page.Exit(&BG_page);


    }


     List.Timer_update(&List);
     List.Show(&List);
#endif
}

/**
 * @brief BG_MenuSlider 演示页面实现
 */
void bg_menu_slider_page() {
    static uint32_t demo_timer = 0;
    
    // Page initialization
    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1) {
        BG_lcd.Clear(0xFF00);
        
        // 创建演示菜单项
        static BG_MenuSlider_Item demo_items[] = {
            BG_MENU_SLIDER_TEXT_ITEM("Music", demo_music_callback),
            BG_MENU_SLIDER_TEXT_ITEM("Games", demo_games_callback),
            BG_MENU_SLIDER_TEXT_ITEM("Settings", demo_settings_callback),
            BG_MENU_SLIDER_TEXT_ITEM("Photo", demo_photo_callback),
            BG_MENU_SLIDER_TEXT_ITEM("Tools", demo_tools_callback)
        };

        static BG_MenuSlider_Table demo_table = BG_MENU_SLIDER_TABLE(demo_items);

        // 创建BG_MenuSlider控件
        demo_menu_slider = BG_MenuSlider_Create(
            &BG_MENU_SLIDER_REGION_CENTER,  // 使用预定义的中心区域
            &BG_MENU_SLIDER_STYLE_DEFAULT,  // 使用默认样式
            &demo_table                     // 传入菜单项表
        );

        // 配置动画（平滑滑动 + 自动滑动）
        demo_menu_slider.SetAnimation(&demo_menu_slider, &BG_MENU_SLIDER_ANIM_AUTO);

        // 启用自动滑动
        demo_menu_slider.StartAutoSlide(&demo_menu_slider);
        
        // 显示页面标题和说明
        BGUI_tool.ShowString(20, 10, (uint8_t*)"BG_MenuSlider Demo", 0xFFFF);
        BGUI_tool.ShowString(10, LCD_HEIGHT - 30, (uint8_t*)"L/R:Slide Enter:OK", 0x7BEF);
        BGUI_tool.ShowString(10, LCD_HEIGHT - 15, (uint8_t*)"Exit:Back to Home", 0x7BEF);
        
        BG_page.Data.table[BG_page.Data.running_id].setup = 0;
        last_time_ms = 0;
        demo_timer = 0;
    }

    // 更新控件（动画和自动滑动）
    if (demo_menu_slider.IsInitialized(&demo_menu_slider)) {
        // 计算时间间隔（模拟，实际项目中应该使用真实的时间戳）
        uint32_t current_time = demo_timer++;
        uint32_t delta_ms = (current_time > last_time_ms) ? (current_time - last_time_ms) : 20;

        // 更新控件
        demo_menu_slider.Update(&demo_menu_slider);
        demo_menu_slider.AutoSlideUpdate(&demo_menu_slider, delta_ms);

        last_time_ms = current_time;
    }
    
    // 处理按键输入
    if (BG_page.Data.last_pressed) {  // 左键 - 向左滑动
        demo_menu_slider.SlideLeft(&demo_menu_slider);
        demo_menu_slider.StopAutoSlide(&demo_menu_slider);  // 用户操作时停止自动滑动
        BG_page.Data.last_pressed = 0;
    }
    
    if (BG_page.Data.next_pressed) {  // 右键 - 向右滑动
        demo_menu_slider.SlideRight(&demo_menu_slider);
        demo_menu_slider.StopAutoSlide(&demo_menu_slider);  // 用户操作时停止自动滑动
        BG_page.Data.next_pressed = 0;
    }
    
    if (BG_page.Data.enter_pressed) { // 确认键 - 选择当前项
        demo_menu_slider.Select(&demo_menu_slider);
        BG_page.Data.enter_pressed = 0;
        // 选择后3秒自动返回并重新启动自动滑动
        demo_timer = 0;
        return;
    }
    
    if (BG_page.Data.exit_pressed) {  // Exit key - return to home
        BG_page.SetPage(&BG_page, HOME_PAGE);
        BG_page.Data.exit_pressed = 0;
        return;
    }

    // 绘制控件（只在需要时重绘）
    if (demo_menu_slider.IsInitialized(&demo_menu_slider)) {
        demo_menu_slider.Draw(&demo_menu_slider);
        demo_menu_slider.Refresh(&demo_menu_slider);
    }

    // 自动返回逻辑：如果进入子功能3秒后自动返回并重新启动自动滑动
    if (demo_timer > 150) {  // 约3秒（假设20ms一次调用）
        demo_timer = 0;

        // 清屏并重新显示菜单
        BG_lcd.Clear(0x0000);
        BGUI_tool.ShowString(20, 10, (uint8_t*)"BG_MenuSlider Demo", 0xFFFF);
        BGUI_tool.ShowString(10, LCD_HEIGHT - 30, (uint8_t*)"L/R:Slide Enter:OK", 0x7BEF);
        BGUI_tool.ShowString(10, LCD_HEIGHT - 15, (uint8_t*)"Exit:Back to Home", 0x7BEF);

        // 重新启动自动滑动
        demo_menu_slider.StartAutoSlide(&demo_menu_slider);
    }
}

