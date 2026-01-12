/**
 * @file    view_boot.c
 * @brief   Boot Splash Screen View Implementation
 * @author  BG Card Team
 * @date    2025-01-08
 * 
 * 寮�満鐣岄潰瀹炵幇锛� *   - 鍔ㄧ敾鐘舵�鏈洪┍鍔ㄨ繘搴︽潯
 *   - 涓嶄娇鐢�vTaskDelay 绛夌‖寤惰繜
 *   - 鍔ㄧ敾瀹屾垚鍚庤嚜鍔ㄥ垏鎹㈠埌 UI_STATE_IDLE
 */

#include "view_boot.h"
#include "bg_lcd.h"
#include "gui_tool.h"
#include <string.h>

/*===========================================================================
 * 瀹忓畾涔� *===========================================================================*/

/* 杩涘害鏉″竷灞�弬鏁�*/
#define PROGRESS_X      20
#define PROGRESS_Y      105
#define PROGRESS_WIDTH  120
#define PROGRESS_HEIGHT 6

/* 鍔ㄧ敾鍙傛暟 */
#define PROGRESS_STEP   5       /* 姣忔鏇存柊澧炲姞5% */
#define UPDATE_INTERVAL 30      /* 鏇存柊闂撮殧: 30ms */
#define SPLASH_DURATION 300     /* Logo 鏄剧ず鍚庡仠鐣欐椂闂� 300ms */

/* 鐗堟湰淇℃伅 */
#define VERSION_STRING  "v1.0.0"

/*===========================================================================
 * 鍐呴儴鐘舵�
 *===========================================================================*/

/**
 * @brief Boot 瑙嗗浘鐘舵�鏈� */
typedef enum {
    BOOT_STATE_INIT = 0,        /* 鍒濆鍖栵紝鏄剧ず Logo */
    BOOT_STATE_PROGRESS,        /* 杩涘害鏉″姩鐢讳腑 */
    BOOT_STATE_EXIT             /* 鍑嗗閫�嚭 */
} BootState_t;

/**
 * @brief Boot 瑙嗗浘鍐呴儴鏁版嵁
 */
typedef struct {
    BootState_t state;          /* 褰撳墠鐘舵� */
    uint8_t progress;           /* 杩涘害鏉¤繘搴�(0-100) */
    uint32_t timer;             /* 璁℃椂鍣ㄧ疮绉椂闂�(ms) */
    bool first_draw;            /* 棣栨缁樺埗鏍囧織 */
} BootViewData_t;

static BootViewData_t s_boot_data = {
    .state = BOOT_STATE_INIT,
    .progress = 0,
    .timer = 0,
    .first_draw = true
};

/*===========================================================================
 * 鍐呴儴鍑芥暟澹版槑
 *===========================================================================*/

static void boot_draw_logo(void);
static void boot_draw_progress_bar(uint8_t progress);

/*===========================================================================
 * View 鐢熷懡鍛ㄦ湡鍥炶皟
 *===========================================================================*/

/**
 * @brief 杩涘叆 Boot 瑙嗗浘
 */
static void boot_on_enter(void) {
    /* 閲嶇疆鐘舵� */
    s_boot_data.state = BOOT_STATE_INIT;
    s_boot_data.progress = 0;
    s_boot_data.timer = 0;
    s_boot_data.first_draw = true;
    
    /* 娓呭睆 */
    BG_lcd.Clear(0x0000);  /* 榛戣壊鑳屾櫙 */
}

/**
 * @brief 閫�嚭 Boot 瑙嗗浘
 */
static void boot_on_exit(void) {
    /* 娓呭睆锛屽噯澶囧垏鎹㈠埌涓荤晫闈�*/
    BG_lcd.Clear(0x0000);
}

/**
 * @brief 鏇存柊 Boot 瑙嗗浘锛堢姸鎬佹満椹卞姩锛� * @param delta_ms 璺濈涓婃鏇存柊鐨勬椂闂达紙姣锛� */
static void boot_on_update(uint16_t delta_ms) {
    s_boot_data.timer += delta_ms;
    
    switch (s_boot_data.state) {
        case BOOT_STATE_INIT:
            /* 鍒濆鍖栫姸鎬侊細鏄剧ず Logo */
            boot_draw_logo();
            s_boot_data.state = BOOT_STATE_PROGRESS;
            s_boot_data.timer = 0;  /* 閲嶇疆璁℃椂鍣紝鍑嗗杩涘害鏉″姩鐢�*/
            break;
            
        case BOOT_STATE_PROGRESS:
            /* 杩涘害鏉″姩鐢荤姸鎬�*/
            if (s_boot_data.timer >= UPDATE_INTERVAL) {
                s_boot_data.timer = 0;  /* 閲嶇疆璁℃椂鍣�*/
                
                /* 鏇存柊杩涘害 */
                s_boot_data.progress += PROGRESS_STEP;
                if (s_boot_data.progress > 100) {
                    s_boot_data.progress = 100;
                }
                
                /* 缁樺埗杩涘害鏉�*/
                boot_draw_progress_bar(s_boot_data.progress);
                
                /* 杩涘害瀹屾垚锛岀瓑寰呬竴娈垫椂闂村悗鍒囨崲 */
                if (s_boot_data.progress >= 100) {
                    s_boot_data.state = BOOT_STATE_EXIT;
                    s_boot_data.timer = 0;
                }
            }
            break;
            
        case BOOT_STATE_EXIT:
            /* 绛夊緟鍋滅暀鏃堕棿鍚庨�鍑�*/
            if (s_boot_data.timer >= SPLASH_DURATION) {
                /* 鑷姩鍒囨崲鍒颁富鐣岄潰 */
                extern const BG_UI_t BG_UI;
                BG_UI.SetState(UI_STATE_IDLE);
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief 缁樺埗 Boot 瑙嗗浘
 */
static void boot_on_draw(void) {
    /* 鍦�on_update 涓凡鎸夐渶缁樺埗锛岃繖閲屽彲浠ヤ负绌�*/
    /* 濡傛灉闇�寮哄埗鍒锋柊锛屽彲浠ユ牴鎹姸鎬侀噸缁�*/
}

/**
 * @brief 澶勭悊鎸夐挳浜嬩欢
 * @param event 鎸夐挳浜嬩欢
 * @return true: 浜嬩欢宸插鐞� false: 浜嬩欢鏈鐞� */
static bool boot_on_button(UI_BtnEventData_t* event) {
    /* Boot 鐣岄潰涓嶅鐞嗘寜閽簨浠�*/
    /* 濡傛灉鐢ㄦ埛甯屾湜璺宠繃寮�満鍔ㄧ敾锛屽彲浠ュ湪杩欓噷澶勭悊 */
    
    /* 渚嬪锛氭寜浠绘剰閿烦杩�*/
    if (event->event == UI_BTN_EVT_CLICK) {
        /* 鐩存帴鍒囨崲鍒颁富鐣岄潰 */
        extern const BG_UI_t BG_UI;
        BG_UI.SetState(UI_STATE_IDLE);
        return true;
    }
    
    return false;
}

/* Default Logo bitmap (8x16, "BG" text) - 涓庡弬鑰冪増鏈竴鑷�*/
static const uint8_t default_logo[] = {
    /* B */
    0x7E, 0x42, 0x42, 0x7C, 0x42, 0x42, 0x7E, 0x00,
    /* G */
    0x3C, 0x42, 0x40, 0x4E, 0x42, 0x42, 0x3C, 0x00,
};

/*===========================================================================
 * 鍐呴儴缁樺浘鍑芥暟
 *===========================================================================*/

/**
 * @brief 缁樺埗 Logo 鍜屽垵濮嬫枃瀛楋紙涓庡弬鑰冪増鏈竴鑷达級
 */
static void boot_draw_logo(void) {
    uint16_t logo_x = (160 - 64) / 2;  /* UI_SCREEN_WIDTH = 160 */
    uint16_t logo_y = 20;
    uint8_t i, j, k;
    
    /* 娓呭睆 - 榛戣壊鑳屾櫙 */
    BG_lcd.Clear(0x0000);
    
    /* Draw large "BG" text - 4鍊嶆斁澶�*/
    /* B - scale up 4x - 闈掕壊 */
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
    /* G - scale up 4x - 缁胯壊 */
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
    
    /* 鏄剧ず浜у搧鍚嶇О "BG Card Mini" - 灞呬腑 */
    BGUI_tool.ShowStringLarge(55, 60, (uint8_t*)"BanBOX", 0xFFFF);
    
    /* 鏄剧ず鐗堟湰鍙�- 灞呬腑 */
    BGUI_tool.ShowString(55, 90, (uint8_t*)VERSION_STRING, 0x8410);  /* 鐏拌壊 */
    
    /* 鐗堟潈淇℃伅 - 搴曢儴灞呬腑 */
    BGUI_tool.ShowString(28, 113, (uint8_t*)"(C) 2025 BanGO", 0x2104);  /* 娣辩伆鑹�*/
    
    /* 缁樺埗杩涘害鏉″妗�*/
    BG_lcd.Box(PROGRESS_X, PROGRESS_Y, PROGRESS_WIDTH, PROGRESS_HEIGHT, 0xFFFF);
}

/**
 * @brief 缁樺埗杩涘害鏉� * @param progress 杩涘害 (0-100)
 */
static void boot_draw_progress_bar(uint8_t progress) {
    uint16_t fill_width;
    
    if (progress > 100) {
        progress = 100;
    }
    
    /* 璁＄畻濉厖瀹藉害 */
    fill_width = (PROGRESS_WIDTH - 4) * progress / 100;
    
    /* 缁樺埗杩涘害鏉″～鍏�*/
    BG_lcd.Box(PROGRESS_X + 2, PROGRESS_Y + 2, 
               fill_width, PROGRESS_HEIGHT - 4, 0x07E0);
}

/*===========================================================================
 * View 瀵硅薄
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
 * 鍏叡鎺ュ彛
 *===========================================================================*/

/**
 * @brief 鑾峰彇 Boot 瑙嗗浘
 */
UI_View_t* View_Boot_Get(void) {
    return &s_view_boot;
}

/**
 * @brief 鍒涘缓 Boot 瑙嗗浘 (涓庡叾浠朧iew淇濇寔涓�嚧鐨凙PI)
 */
UI_View_t* View_Boot_Create(void) {
    /* 閲嶇疆鐘舵� */
    s_boot_data.state = BOOT_STATE_INIT;
    s_boot_data.progress = 0;
    s_boot_data.timer = 0;
    s_boot_data.first_draw = true;
    
    /* 娉ㄥ唽瑙嗗浘鍒�UI 绯荤粺 */
    extern const BG_UI_t BG_UI;
    BG_UI.RegisterView(UI_STATE_BOOT, &s_view_boot);
    
    return &s_view_boot;
}

/**
 * @brief 鍒濆鍖�Boot 瑙嗗浘 (鍏煎鏃PI)
 */
void View_Boot_Init(void) {
    View_Boot_Create();
}
