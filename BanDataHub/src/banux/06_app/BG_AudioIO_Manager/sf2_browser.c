/**
 * @file sf2_browser.c
 * @brief 128x64 OLED 列出 TF 根目录 .sf2，编码器选择后加载
 *
 * 短按：进入列表 / 确认加载
 * 旋转：移动高亮
 * 长按：重新扫描 TF
 */
#include "product_def.h"
#include "sf2_browser.h"

#if HW_DRV_SSD1306_EN && BANGTSYNTH_EN

#include "ssd1306.h"
#include "bg_sf2_sd.h"
#include "bg_synth.h"
#include "otg_device_stor.h"
#include <stdio.h>
#include <string.h>

#if HW_DRV_ENCODER_EN
#include "rotary_encoder.h"
#endif

#define UI_ROWS          4
#define NAME_SHOW_MAX    16

typedef enum {
    BR_HOME = 0,
    BR_LIST,
    BR_LOAD,
    BR_MSG
} BrState_t;

static BrState_t s_state = BR_HOME;
static int s_sel;
static int s_scroll;
static uint8_t s_dirty = 1;
static uint16_t s_msg_ttl;
static char s_msg[20];
static uint8_t s_hold_latched;

static void draw_clipped_name(uint8_t x, uint8_t y, const char *name, uint8_t invert)
{
    char buf[NAME_SHOW_MAX + 1];
    uint8_t i;

    memset(buf, 0, sizeof(buf));
    for (i = 0; i < NAME_SHOW_MAX && name[i]; i++) {
        buf[i] = name[i];
    }
    if (invert) {
        SSD1306_FillRect(0, y, 128, 10, 1);
        SSD1306_DrawString(x, y + 1, buf, 1, 0);
    } else {
        SSD1306_DrawString(x, y + 1, buf, 1, 1);
    }
}

static void enter_list(void)
{
    int n;

    if (OTG_DeviceStorIsBusy()) {
        strncpy(s_msg, "USB disk busy", sizeof(s_msg) - 1);
        s_msg_ttl = 80;
        s_state = BR_MSG;
        s_dirty = 1;
        return;
    }

    n = bg_sf2_sd_scan();
    s_sel = 0;
    s_scroll = 0;
    if (n <= 0) {
        strncpy(s_msg, "No .sf2 on TF", sizeof(s_msg) - 1);
        s_msg_ttl = 80;
        s_state = BR_MSG;
    } else {
        s_state = BR_LIST;
    }
    s_dirty = 1;
}

static void do_load(void)
{
    const char *name;
    int ret;

    name = bg_sf2_sd_name(s_sel);
    if (!name) {
        s_state = BR_HOME;
        s_dirty = 1;
        return;
    }
    if (OTG_DeviceStorIsBusy()) {
        strncpy(s_msg, "USB disk busy", sizeof(s_msg) - 1);
        s_msg_ttl = 80;
        s_state = BR_MSG;
        s_dirty = 1;
        return;
    }

    s_state = BR_LOAD;
    s_dirty = 1;
    /* 先刷新 “Loading” 再阻塞拷贝 */
    {
        SSD1306_Clear();
        SSD1306_DrawString(0, 0, "Loading", 2, 1);
        SSD1306_DrawString(0, 24, name, 1, 1);
        SSD1306_Update();
    }

    ret = bg_synth_load_file(name);
    if (ret == 0) {
        strncpy(s_msg, "Load OK", sizeof(s_msg) - 1);
    } else {
        strncpy(s_msg, "Load FAIL", sizeof(s_msg) - 1);
    }
    s_msg_ttl = 60;
    s_state = BR_MSG;
    s_dirty = 1;
}

static void draw_home(void)
{
    const char *cur = bg_sf2_sd_current();

    SSD1306_Clear();
    SSD1306_DrawString(0, 0, "BanDataHub", 1, 1);
    SSD1306_DrawString(0, 12, "CDC+Udisk", 1, 1);
    SSD1306_DrawString(0, 24, "Bank:", 1, 1);
    if (cur && cur[0]) {
        draw_clipped_name(36, 24, cur, 0);
    } else {
        SSD1306_DrawString(36, 25, "(none)", 1, 1);
    }
    if (OTG_DeviceStorIsBusy()) {
        SSD1306_DrawString(0, 40, "PC using TF", 1, 1);
    } else {
        SSD1306_DrawString(0, 40, "Click: pick SF2", 1, 1);
    }
    SSD1306_DrawString(0, 54, "Long: rescan", 1, 1);
    SSD1306_Update();
}

static void draw_list(void)
{
    int n = bg_sf2_sd_count();
    int i;
    char title[20];

    if (s_sel < 0) {
        s_sel = 0;
    }
    if (n > 0 && s_sel >= n) {
        s_sel = n - 1;
    }
    if (s_sel < s_scroll) {
        s_scroll = s_sel;
    }
    if (s_sel >= s_scroll + UI_ROWS) {
        s_scroll = s_sel - UI_ROWS + 1;
    }

    SSD1306_Clear();
    sprintf(title, "SF2 %d/%d", n ? (s_sel + 1) : 0, n);
    SSD1306_DrawString(0, 0, title, 1, 1);

    for (i = 0; i < UI_ROWS; i++) {
        int idx = s_scroll + i;
        uint8_t y = (uint8_t)(12 + i * 12);
        if (idx >= n) {
            break;
        }
        draw_clipped_name(2, y, bg_sf2_sd_name(idx), (idx == s_sel) ? 1 : 0);
    }
    SSD1306_Update();
}

static void draw_msg(void)
{
    SSD1306_Clear();
    SSD1306_DrawString(0, 20, s_msg, 1, 1);
    SSD1306_Update();
}

void Sf2Browser_Init(void)
{
    s_state = BR_HOME;
    s_sel = 0;
    s_scroll = 0;
    s_dirty = 1;
}

void Sf2Browser_Tick(void)
{
    int16_t delta = 0;
    uint8_t click = 0;
    uint8_t hold = 0;

#if HW_DRV_ENCODER_EN
    delta = RotaryEncoder_GetDelta();
    click = RotaryEncoder_IsButtonPressed();
    hold = RotaryEncoder_IsButtonLongPressed();
#endif

    if (!hold) {
        s_hold_latched = 0;
    } else if (s_hold_latched) {
        hold = 0;
    } else {
        s_hold_latched = 1;
    }

    if (s_state == BR_MSG) {
        if (s_msg_ttl > 0) {
            s_msg_ttl--;
        }
        if (s_msg_ttl == 0 || click) {
            s_state = BR_HOME;
            s_dirty = 1;
        }
    } else if (s_state == BR_HOME) {
        if (click) {
            enter_list();
        } else if (hold) {
            enter_list();
        }
    } else if (s_state == BR_LIST) {
        if (delta) {
            s_sel += (delta > 0) ? 1 : -1;
            s_dirty = 1;
        }
        if (click) {
            do_load();
        } else if (hold) {
            s_state = BR_HOME;
            s_dirty = 1;
        }
    }

    if (!s_dirty) {
        return;
    }
    s_dirty = 0;

    if (s_state == BR_HOME) {
        draw_home();
    } else if (s_state == BR_LIST) {
        draw_list();
    } else if (s_state == BR_MSG) {
        draw_msg();
    }
}

#else /* !OLED or !synth */

void Sf2Browser_Init(void) {}
void Sf2Browser_Tick(void) {}

#endif
