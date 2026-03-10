/**
 * @file    comp_statusbar.c
 * @brief   Status bar implementation (compatible with old UI system)
 * @author  BG Card Team
 * @date    2025-01-09
 */

#include "comp_statusbar.h"
#include "bg_lcd.h"
#include "gui_tool.h"
#include "gpio.h"
#include "adc.h"
#include "otg_detect.h"
#include <string.h>
#include "battery_drv.h"

/*===========================================================================
 * Icon definitions (8x8 pixel bitmaps)
 *===========================================================================*/

/* Bluetooth icon */
static const uint8_t icon_bt[] = {
    0x10, 0x18, 0x14, 0x72, 0x72, 0x14, 0x18, 0x10
};

/* Bluetooth connected icon */
static const uint8_t icon_bt_connected[] = {
    0x10, 0x18, 0x54, 0x72, 0x72, 0x54, 0x18, 0x10
};

/* MIC icon */
static const uint8_t icon_mic[] = {
    0x18, 0x24, 0x24, 0x24, 0x18, 0x18, 0x7E, 0x18
};

/* Guitar icon */
static const uint8_t icon_guitar[] = {
    0x01, 0x03, 0x06, 0x0C, 0x38, 0x7C, 0x7C, 0x38
};

/* Headphone icon */
static const uint8_t icon_headphone[] = {
    0x3C, 0x42, 0x42, 0x42, 0xE7, 0xE7, 0xE7, 0x42
};

/* Speaker icon */
static const uint8_t icon_speaker[] = {
    0x04, 0x0C, 0x1C, 0x7F, 0x7F, 0x1C, 0x0C, 0x04
};

/* USB icon */
static const uint8_t icon_usb[] = {
    0x18, 0x24, 0x24, 0x7E, 0x7E, 0x3C, 0x18, 0x18
};

/* Volume icon */
static const uint8_t icon_volume[] = {
    0x02, 0x06, 0x7E, 0x7E, 0x7E, 0x06, 0x02, 0x00
};

/* Mute icon */
static const uint8_t icon_mute[] = {
    0x42, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x66, 0x42
};

/*===========================================================================
 * Private variables
 *===========================================================================*/

static UI_StatusBarData_t statusbar_data;
static bool statusbar_visible;
static bool need_redraw;

/* Last status bar data (for change detection, redraw optimization) */
static UI_StatusBarData_t last_data;

/*===========================================================================
 * Private functions
 *===========================================================================*/

/**
 * @brief Draw battery icon and percentage
 */
static void UI_StatusBar_DrawBattery(void)
{
    uint16_t bat_x = UI_SCREEN_WIDTH - 60;
    uint16_t bat_y = 1;
    uint8_t i;

    /* Draw battery outline */
    BG_lcd.Box(bat_x, bat_y, 23, 10, 0xFFFF);
    BG_lcd.Box(bat_x + 24, bat_y + 2, 3, 6, 0xFFFF);

    /* Draw battery level grids */
    for(i = 0; i < statusbar_data.battery_grid; i++) {
        uint16_t fill_x = bat_x + 2 + (i * 4);
        BG_lcd.Box(fill_x, bat_y + 2, 3, 6, 0x07E0);
    }
}

/**
 * @brief Draws an 8x8 icon
 */
static void draw_icon(uint16_t x, uint16_t y, const uint8_t* icon, uint16_t color)
{
    uint8_t i, j;
    if (!icon) return;
    
    for (i = 0; i < 8; i++) {
        uint8_t row = icon[i];
        for (j = 0; j < 8; j++) {
            if (row & (0x80 >> j)) {
                BG_lcd.DrawPoint(x + j, y + i, color);
            }
        }
    }
}

/**
 * @brief Draws the Bluetooth status icon
 */
static void draw_bt_icon(void)
{
    uint16_t x = UI_ICON_BT_X;
    uint16_t y = UI_ICON_Y;
    const uint8_t* icon = NULL;
    uint16_t color = UI_COLOR_BLACK ;
    
    /* Clear icon area */
    BG_lcd.Box(x, y, UI_ICON_SIZE, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    
    switch (statusbar_data.bt_status) {
        case UI_BT_OFF:
            icon = icon_bt;
            color =UI_COLOR_BLACK;
            break;
        case UI_BT_DISCONNECTED:
            icon = icon_bt;
            color = UI_COLOR_WHITE;
            break;
        case UI_BT_CONNECTING:
            icon = icon_bt;
            color = UI_COLOR_YELLOW;
            break;
        case UI_BT_CONNECTED:
        case UI_BT_PLAYING:
            icon = icon_bt_connected;
            color = UI_COLOR_CYAN;
            break;
    }
    
    if (icon) {
        draw_icon(x, y, icon, color);
    }
}

/**
 * @brief Draws the ADC input status icons (MIC, Guitar)
 */
static void draw_adc_icon(void)
{
    uint16_t y = UI_ICON_Y;
    
    /* MIC icon - fixed position */
    BG_lcd.Box(UI_ICON_MIC_X, y, UI_ICON_SIZE, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    if (statusbar_data.adc_source & UI_ADC_MIC) {
        draw_icon(UI_ICON_MIC_X, y, icon_mic, UI_COLOR_GREEN);
    }
    /* Guitar icon - fixed position */
    BG_lcd.Box(UI_ICON_GUITAR_X, y, UI_ICON_SIZE, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    if (statusbar_data.adc_source & UI_ADC_GUITAR) {
        draw_icon(UI_ICON_GUITAR_X, y, icon_guitar, UI_COLOR_ORANGE);
    }
}

/**
 * @brief Draws the DAC output status icon (Headphone or Speaker)
 */
static void draw_dac_icon(void)
{
    uint16_t y = UI_ICON_Y;
    
    /* Clear icon area */
    BG_lcd.Box(UI_ICON_HP_X, y, UI_ICON_SIZE, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    
    if (statusbar_data.dac_output & UI_DAC_HP) {
        draw_icon(UI_ICON_HP_X, y, icon_headphone, UI_COLOR_CYAN);
    } else if (statusbar_data.dac_output & UI_DAC_SPKR) {
        draw_icon(UI_ICON_HP_X, y, icon_speaker, UI_COLOR_WHITE);
    }
}

/**
 * @brief Draws the USB connection status icon
 */
static void draw_usb_icon(void)
{
    uint16_t y = UI_ICON_Y;
    
    /* Clear icon area */
    BG_lcd.Box(UI_ICON_USB_X, y, UI_ICON_SIZE, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    
    /* 仅负责 UI 显示；UsbDeviceEnable/Disable 已由音频系统的 USB_HotplugCheck() 处理，
     * 此处不再调用硬件接口，避免与音频系统产生重入竞争。 */
    if (statusbar_data.usb_connected) {
        draw_icon(UI_ICON_USB_X, y, icon_usb, UI_COLOR_GREEN);
    }
}

/**
 * @brief Draws the Volume level and Mute status icon
 */
static void draw_volume_icon(void)
{
    uint16_t x = UI_ICON_VOLUME_X;
    uint16_t y = UI_ICON_Y;
    uint8_t bars, i;
    
    /* Clear icon area */
    BG_lcd.Box(x, y, 30, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    
    /* Draw mute icon in red, or volume icon in white */
    if (statusbar_data.muted) {
        draw_icon(x, y, icon_mute, UI_COLOR_RED);
    } else {
        draw_icon(x, y, icon_volume, UI_COLOR_WHITE);
    }
    
    /* Draw volume bars (3 levels) */
    x += 10;
    bars = statusbar_data.volume / 17;  /* 0-2 bars */
    if (statusbar_data.volume > 0 && bars == 0) bars = 1;
    
    for (i = 0; i < 3; i++) {
        uint16_t bar_color = (i < bars) ? UI_COLOR_GREEN : UI_COLOR_DARK_GRAY;
        uint16_t bar_h = 3 + i * 2;
        uint16_t bar_y = y + (8 - bar_h);
        BG_lcd.Box(x + i * 5, bar_y, 4, bar_h, bar_color);
    }
}

/**
 * @brief Initializes the detection pins for MIC, Guitar, Headphone
 */
static void init_detect_pins(void)
{
    /* MIC detection - A30 (input, pull-down) */
    GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX30);
    GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX30);
    GPIO_RegOneBitClear(GPIO_A_PU, GPIO_INDEX30);
    GPIO_RegOneBitSet(GPIO_A_PD, GPIO_INDEX30);
    
    /* Guitar detection - A29 (input, pull-up) */
    GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX29);
    GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX29);
    GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX29);
    GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX29);
    
    /* Headphone detection - B4 (input, pull-up) */
    GPIO_RegOneBitSet(GPIO_B_IE, GPIO_INDEX4);
    GPIO_RegOneBitClear(GPIO_B_OE, GPIO_INDEX4);
    GPIO_RegOneBitSet(GPIO_B_PU, GPIO_INDEX4);
    GPIO_RegOneBitClear(GPIO_B_PD, GPIO_INDEX4);
}

/*===========================================================================
 * API functions
 *===========================================================================*/

void UI_StatusBar_Init(void)
{
    memset(&statusbar_data, 0, sizeof(statusbar_data));
    memset(&last_data, 0, sizeof(last_data));
    statusbar_visible = true;
    need_redraw = true;
    
    /* Initialize with default values */
    statusbar_data.volume = 50;
    statusbar_data.battery = 100;
    statusbar_data.battery_grid = 4;
    statusbar_data.bt_status = UI_BT_OFF;
}

void Comp_StatusBar_Init(void)
{
    /* Alias for UI_StatusBar_Init - compatibility wrapper */
    UI_StatusBar_Init();
}

void UI_StatusBar_Draw(void)
{
    if (!statusbar_visible) return;
    
    /* Clear status bar area */
    BG_lcd.Box(0, 0, UI_SCREEN_WIDTH, UI_STATUSBAR_HEIGHT, UI_COLOR_BLACK);
    
    /* 浣跨敤涓撻棬鐨勭粯鍒跺嚱鏁帮紝鏄剧ず鍥炬爣鑰屼笉鏄瓧绗�*/
    draw_bt_icon();
    draw_adc_icon();
    draw_dac_icon();
    draw_usb_icon();
    draw_volume_icon();
    
    /* Draw battery */
    UI_StatusBar_DrawBattery();
    
    last_data = statusbar_data;
    need_redraw = false;
}

void UI_StatusBar_Update(void)
{
    bool changed = false;
    
    if (!statusbar_visible) return;
    
    /* Check and update changed fields */
    if (statusbar_data.bt_status != last_data.bt_status) {
        draw_bt_icon();
        changed = true;
    }
    
    if (statusbar_data.adc_source != last_data.adc_source) {
        draw_adc_icon();
        changed = true;
    }
    
    if (statusbar_data.dac_output != last_data.dac_output) {
        draw_dac_icon();
        changed = true;
    }
    
    if (statusbar_data.usb_connected != last_data.usb_connected) {
        draw_usb_icon();
        changed = true;
    }
    
    if (statusbar_data.volume != last_data.volume || 
        statusbar_data.muted != last_data.muted) {
        draw_volume_icon();
        changed = true;
    }
    
    if (statusbar_data.battery_level != last_data.battery_level) {
        UI_StatusBar_DrawBattery();
        changed = true;
    }
    
    if (changed) {
        memcpy(&last_data, &statusbar_data, sizeof(statusbar_data));
    }
}

void UI_StatusBar_ScanDetect(void)
{
    uint8_t new_adc = UI_ADC_NONE;
    uint8_t new_dac = UI_DAC_NONE;

    /* Detect MIC input (A30, pull-down) */
    if (!GPIO_RegOneBitGet(UI_DET_MIC_PORT, UI_DET_MIC_PIN)) {
        new_adc |= UI_ADC_MIC;
    }
    
    /* Detect Guitar input (A29, pull-up) */
    if (GPIO_RegOneBitGet(UI_DET_GUITAR_PORT, UI_DET_GUITAR_PIN)) {
        new_adc |= UI_ADC_GUITAR;
    }
    
    /* Detect Headphone or Speaker output (B4, pull-up) */
    if (GPIO_RegOneBitGet(UI_DET_HP_PORT, UI_DET_HP_PIN)) {
        new_dac |= UI_DAC_HP;
    } else {
        new_dac |= UI_DAC_SPKR;
    }
    
    /* Update status data */
    statusbar_data.adc_source = new_adc;
    statusbar_data.dac_output = new_dac;
    
    /* Volume detection (ADC28) */
    GPIO_RegOneBitSet(UI_VOLUME_ADC_PORT, UI_VOLUME_ADC_PIN);
    {
        uint16_t adc_val = ADC_SingleModeDataGet(UI_VOLUME_ADC_CHANNEL);
        uint8_t new_volume = (uint8_t)((adc_val * 100) / 4095);
        if (new_volume > 100) new_volume = 100;
        statusbar_data.volume = new_volume;
    }
    
    /* Battery detection */
    statusbar_data.battery_level = battery_get_soc();
    if (statusbar_data.battery_level < 100) {
        statusbar_data.battery_grid = statusbar_data.battery_level / 20 + 1;
    } else {
        statusbar_data.battery_grid = statusbar_data.battery_level / 20;
    }
    
    /* Update USB connection status */
    statusbar_data.usb_connected = OTG_PortDeviceIsLink();
}

void UI_StatusBar_SetBTStatus(UI_BTStatus_t status)
{
    statusbar_data.bt_status = status;
}

void UI_StatusBar_SetADCSource(uint8_t source)
{
    statusbar_data.adc_source = source;
}

void UI_StatusBar_SetDACOutput(uint8_t output)
{
    statusbar_data.dac_output = output;
}

void UI_StatusBar_SetVolume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    statusbar_data.volume = volume;
}

void UI_StatusBar_SetMuted(bool muted)
{
    statusbar_data.muted = muted;
}

void UI_StatusBar_SetUSBConnected(bool connected)
{
    statusbar_data.usb_connected = connected;
}

UI_StatusBarData_t* UI_StatusBar_GetData(void)
{
    return &statusbar_data;
}

uint16_t UI_StatusBar_GetHeight(void)
{
    return statusbar_visible ? UI_STATUSBAR_HEIGHT : 0;
}

void UI_StatusBar_SetVisible(bool visible)
{
    if (statusbar_visible != visible) {
        statusbar_visible = visible;
        need_redraw = true;
    }
}

bool UI_StatusBar_IsVisible(void)
{
    return statusbar_visible;
}


