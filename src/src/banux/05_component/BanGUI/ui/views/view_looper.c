/**
 * @file    view_looper.c
 * @brief   Looper View - Audio looper control implementation
 * @author  BG Card Team
 * @date    2025-01-08
 */

#include "view_looper.h"
#include "../components/comp_statusbar.h"
#include "bg_lcd.h"
#include "gui_tool.h"
#include <string.h>

/*===========================================================================
 * 绉佹湁甯搁噺
 *===========================================================================*/

#define SEG_BOX_WIDTH       35
#define SEG_BOX_HEIGHT      60
#define SEG_BOX_SPACING     5
#define SEG_BOX_START_X     5
#define SEG_BOX_START_Y     25

/* 娈电姸鎬侀鑹�*/
#define COLOR_INACTIVE      UI_DARK_GRAY
#define COLOR_RECORDING     UI_RED
#define COLOR_PLAYING       UI_GREEN
#define COLOR_STOPPED       UI_YELLOW

/*===========================================================================
 * 绉佹湁鍙橀噺
 *===========================================================================*/

static UI_View_t s_looper_view;
static uint8_t s_selected_seg = 0;

static struct {
    LooperSegState_t state;
    uint8_t progress;
} s_segments[LOOPER_MAX_SEGMENTS];

static const char* s_state_names[] = {
    "IDLE",
    "REC",
    "PLAY",
    "STOP"
};

/*===========================================================================
 * 绉佹湁鍑芥暟
 *===========================================================================*/

static uint16_t get_state_color(LooperSegState_t state)
{
    switch (state) {
        case LOOPER_SEG_RECORDING: return COLOR_RECORDING;
        case LOOPER_SEG_PLAYING:   return COLOR_PLAYING;
        case LOOPER_SEG_STOPPED:   return COLOR_STOPPED;
        default:                   return COLOR_INACTIVE;
    }
}

static void draw_segment_box(uint8_t index, bool selected)
{
    if (index >= LOOPER_MAX_SEGMENTS) return;
    
    uint16_t x = SEG_BOX_START_X + index * (SEG_BOX_WIDTH + SEG_BOX_SPACING);
    uint16_t y = SEG_BOX_START_Y;
    
    LooperSegState_t state = s_segments[index].state;
    uint16_t color = get_state_color(state);
    
    /* 缁樺埗鑳屾櫙妗�*/
    BG_lcd.Box(x, y, SEG_BOX_WIDTH, SEG_BOX_HEIGHT, UI_DARK_GRAY);
    
    /* 缁樺埗杩涘害鏉�*/
    if (state == LOOPER_SEG_PLAYING || state == LOOPER_SEG_RECORDING) {
        uint16_t progress_h = (SEG_BOX_HEIGHT - 4) * s_segments[index].progress / 100;
        BG_lcd.Box(x + 2, y + SEG_BOX_HEIGHT - 2 - progress_h, 
                   SEG_BOX_WIDTH - 4, progress_h, color);
    }
    
    /* 缁樺埗杈规 */
    uint16_t border_color = selected ? UI_CYAN : UI_GRAY;
    BG_lcd.DrawLine(x, y, x + SEG_BOX_WIDTH - 1, y, border_color);
    BG_lcd.DrawLine(x, y + SEG_BOX_HEIGHT - 1, x + SEG_BOX_WIDTH - 1, y + SEG_BOX_HEIGHT - 1, border_color);
    BG_lcd.DrawLine(x, y, x, y + SEG_BOX_HEIGHT - 1, border_color);
    BG_lcd.DrawLine(x + SEG_BOX_WIDTH - 1, y, x + SEG_BOX_WIDTH - 1, y + SEG_BOX_HEIGHT - 1, border_color);
    
    /* 缁樺埗娈电紪鍙�*/
    char seg_num = '1' + index;
    BG_lcd.ShowChar(x + SEG_BOX_WIDTH/2 - 4, y + 4, seg_num, UI_WHITE);
    
    /* 缁樺埗鐘舵�鏂囨湰 */
    const char* state_str = s_state_names[state];
    uint8_t str_len = strlen(state_str);
    uint8_t str_x = x + (SEG_BOX_WIDTH - str_len * 6) / 2;
    BGUI_tool.ShowString(str_x, y + SEG_BOX_HEIGHT + 4, (uint8_t*)state_str, color);
}

static void draw_all_segments(void)
{
    uint8_t i;
    for (i = 0; i < LOOPER_MAX_SEGMENTS; i++) {
        draw_segment_box(i, (i == s_selected_seg));
    }
}

static void draw_title(void)
{
    BGUI_tool.ShowString(50, 2, (uint8_t*)"LOOPER", UI_CYAN);
}

static void draw_instructions(void)
{
    uint16_t y = UI_SCREEN_HEIGHT - 20;
    BGUI_tool.ShowString(5, y, (uint8_t*)"</>:SEL  ENT:ACT  BCK:EXIT", UI_GRAY);
}

/*===========================================================================
 * View 鍥炶皟
 *===========================================================================*/

static void looper_on_enter(void)
{
    s_looper_view.visible = true;
    s_looper_view.dirty = true;
}

static void looper_on_exit(void)
{
    s_looper_view.visible = false;
}

static void looper_on_update(uint16_t delta_ms)
{
    (void)delta_ms;
    /* 浠�AudioLooper 妯″潡鑾峰彇鏈�柊鐘舵� */
    /* 杩欓噷鍙互娣诲姞杞閫昏緫鎴栬�浣跨敤鍥炶皟鏈哄埗 */
}

static void looper_on_draw(void)
{
    /* 使用UI系统的背景色清屏 */
    extern const BG_UI_t BG_UI;
    BG_lcd.Clear(BG_UI.GetBackgroundColor());
    
    /* 缁樺埗鐘舵�鏍�*/
    UI_StatusBar_Draw();
    
    /* 缁樺埗鏍囬 */
    draw_title();
    
    /* 缁樺埗鎵�湁娈�*/
    draw_all_segments();
    
    /* 缁樺埗鎿嶄綔璇存槑 */
    draw_instructions();
}

static bool looper_on_button(UI_BtnEventData_t* event)
{
    if (event->event != UI_BTN_EVT_CLICK) {
        return false;
    }
    
    switch (event->id) {
        case UI_BTN_UP:
            /* 鍚戝乏閫夋嫨娈�*/
            if (s_selected_seg > 0) {
                s_selected_seg--;
            } else {
                s_selected_seg = LOOPER_MAX_SEGMENTS - 1;
            }
            s_looper_view.dirty = true;
            return true;
            
        case UI_BTN_DOWN:
            /* 鍚戝彸閫夋嫨娈�*/
            if (s_selected_seg < LOOPER_MAX_SEGMENTS - 1) {
                s_selected_seg++;
            } else {
                s_selected_seg = 0;
            }
            s_looper_view.dirty = true;
            return true;
            
        case UI_BTN_ENTER:
            /* 婵�椿/鍒囨崲閫変腑娈�*/
            /* 杩欓噷搴旇璋冪敤 AudioLooper 鐨勬鎺у埗鍑芥暟 */
            /* AudioLooper.SegmentButtonPress(s_selected_seg); */
            BG_UI.ShowPopup("Looper", "Segment toggled", 500);
            s_looper_view.dirty = true;
            return true;
            
        case UI_BTN_BACK:
            /* 杩斿洖涓荤晫闈�*/
            BG_UI.SetState(UI_STATE_IDLE);
            return true;
            
        default:
            break;
    }
    
    return false;
}

/*===========================================================================
 * 鍏叡 API
 *===========================================================================*/

UI_View_t* View_Looper_Create(void)
{
    memset(&s_looper_view, 0, sizeof(s_looper_view));
    memset(s_segments, 0, sizeof(s_segments));
    
    s_looper_view.name = "Looper";
    s_looper_view.on_enter = looper_on_enter;
    s_looper_view.on_exit = looper_on_exit;
    s_looper_view.on_update = looper_on_update;
    s_looper_view.on_draw = looper_on_draw;
    s_looper_view.on_button = looper_on_button;
    s_looper_view.visible = false;
    s_looper_view.dirty = true;
    
    /* 娉ㄥ唽鍒�LOOPER 鐘舵� */
    BG_UI.RegisterView(UI_STATE_LOOPER, &s_looper_view);
    
    return &s_looper_view;
}

void View_Looper_Destroy(void)
{
    BG_UI.UnregisterView(&s_looper_view);
}

void View_Looper_SetSegmentState(uint8_t seg_index, LooperSegState_t state)
{
    if (seg_index < LOOPER_MAX_SEGMENTS) {
        s_segments[seg_index].state = state;
        if (s_looper_view.visible) {
            s_looper_view.dirty = true;
        }
    }
}

void View_Looper_SetSegmentProgress(uint8_t seg_index, uint8_t progress)
{
    if (seg_index < LOOPER_MAX_SEGMENTS) {
        s_segments[seg_index].progress = progress > 100 ? 100 : progress;
        if (s_looper_view.visible) {
            s_looper_view.dirty = true;
        }
    }
}

void View_Looper_Refresh(void)
{
    s_looper_view.dirty = true;
}
