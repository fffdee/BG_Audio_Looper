/**
 * @file    ui_statusbar.c
 * @brief   椤堕儴鐘舵�鏍忔ā鍧楀疄鐜�
 * @author  BG Card Team
 * @date    2025-12-18
 */

#include "ui_statusbar.h"
#include "ui_config.h"
#include "bg_lcd.h"
#include "gpio.h"
#include "adc.h"
#include "otg_detect.h"
#include <string.h>
#include "dma.h"
/*===========================================================================
 * 鍥炬爣鏁版嵁 (8x8 浣嶅浘)
 *===========================================================================*/

/* 钃濈墮鍥炬爣 */
static const uint8_t icon_bt[] = {
    0x10, 0x18, 0x14, 0x72, 0x72, 0x14, 0x18, 0x10
};

/* 钃濈墮宸茶繛鎺ュ浘鏍�*/
static const uint8_t icon_bt_connected[] = {
    0x10, 0x18, 0x54, 0x72, 0x72, 0x54, 0x18, 0x10
};

/* 楹﹀厠椋庡浘鏍�*/
static const uint8_t icon_mic[] = {
    0x18, 0x24, 0x24, 0x24, 0x18, 0x18, 0x7E, 0x18
};

/* 鍚変粬鍥炬爣 */
static const uint8_t icon_guitar[] = {
    0x01, 0x03, 0x06, 0x0C, 0x38, 0x7C, 0x7C, 0x38
};

/* 鑰虫満鍥炬爣 */
static const uint8_t icon_headphone[] = {
    0x3C, 0x42, 0x42, 0x42, 0xE7, 0xE7, 0xE7, 0x42
};

/* 鎵０鍣ㄥ浘鏍�*/
static const uint8_t icon_speaker[] = {
    0x04, 0x0C, 0x1C, 0x7F, 0x7F, 0x1C, 0x0C, 0x04
};

/* USB鍥炬爣 */
static const uint8_t icon_usb[] = {
    0x18, 0x24, 0x24, 0x7E, 0x7E, 0x3C, 0x18, 0x18
};

/* 闊抽噺鍥炬爣 */
static const uint8_t icon_volume[] = {
    0x02, 0x06, 0x7E, 0x7E, 0x7E, 0x06, 0x02, 0x00
};

/* 闈欓煶鍥炬爣 */
static const uint8_t icon_mute[] = {
    0x42, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x66, 0x42
};

/*===========================================================================
 * 绉佹湁鍙橀噺
 *===========================================================================*/

static UI_StatusBarData_t statusbar_data;
static bool statusbar_visible;
static bool need_redraw;

/* 涓婁竴娆＄姸鎬�鐢ㄤ簬妫�祴鍙樺寲) */
static UI_StatusBarData_t last_data;

/*===========================================================================
 * 绉佹湁鍑芥暟
 *===========================================================================*/

/**
 * @brief 缁樺埗8x8鍥炬爣
 */
static void draw_icon(uint16_t x, uint16_t y, const uint8_t* icon, uint16_t color)
{
    if (!icon) return;
    
    uint8_t i, j;
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
 * @brief 缁樺埗钃濈墮鐘舵�鍥炬爣
 */
static void draw_bt_icon(void)
{
    uint16_t x = UI_ICON_BT_X;
    uint16_t y = UI_ICON_Y;
    
    /* 娓呴櫎鍥炬爣鍖哄煙 */
    BG_lcd.Box(x, y, UI_ICON_SIZE, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    
    const uint8_t* icon = NULL;
    uint16_t color = UI_COLOR_GRAY;
    
    switch (statusbar_data.bt_status) {
        case UI_BT_OFF:
            icon = icon_bt;
            color = UI_COLOR_DARK_GRAY;
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
 * @brief 缁樺埗闊抽杈撳叆鐘舵�鍥炬爣
 */
static void draw_adc_icon(void)
{
    uint16_t y = UI_ICON_Y;
    
    /* MIC 鍥炬爣 - 鐙珛浣嶇疆 */
    BG_lcd.Box(UI_ICON_MIC_X, y, UI_ICON_SIZE, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    if (statusbar_data.adc_source & UI_ADC_MIC) {
        draw_icon(UI_ICON_MIC_X, y, icon_mic, UI_COLOR_GREEN);
    }
    /* 鍚変粬鍥炬爣 - 鐙珛浣嶇疆 */
    BG_lcd.Box(UI_ICON_GUITAR_X, y, UI_ICON_SIZE, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    if (statusbar_data.adc_source & UI_ADC_GUITAR) {
        draw_icon(UI_ICON_GUITAR_X, y, icon_guitar, UI_COLOR_ORANGE);
    }
}

/**
 * @brief 缁樺埗闊抽杈撳嚭鐘舵�鍥炬爣
 */
static void draw_dac_icon(void)
{
    uint16_t y = UI_ICON_Y;
    
    /* 娓呴櫎鍥炬爣鍖哄煙 */
    BG_lcd.Box(UI_ICON_HP_X, y, UI_ICON_SIZE, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    
    /* 鑰虫満鍥炬爣鎴栨壃澹板櫒鍥炬爣锛堜簩閫変竴鏄剧ず鍦ㄥ悓涓�綅缃級 */
    if (statusbar_data.dac_output & UI_DAC_HP) {
    	draw_icon(UI_ICON_HP_X, y, icon_headphone, UI_COLOR_CYAN);

    } else if (statusbar_data.dac_output & UI_DAC_SPKR) {

  	  draw_icon(UI_ICON_HP_X, y, icon_speaker, UI_COLOR_WHITE);
    }
}

/**
 * @brief 缁樺埗USB鐘舵�鍥炬爣
 */
static void draw_usb_icon(void)
{
    uint16_t y = UI_ICON_Y;
    
    /* 娓呴櫎鍥炬爣鍖哄煙 */
    BG_lcd.Box(UI_ICON_USB_X, y, UI_ICON_SIZE, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    
    if (statusbar_data.usb_connected) {
        draw_icon(UI_ICON_USB_X, y, icon_usb, UI_COLOR_GREEN);
        UsbDeviceEnable();
    }else{
    	UsbDeviceDisable();
    }
}

/**
 * @brief 缁樺埗闊抽噺鎸囩ず
 */
static void draw_volume_icon(void)
{
    uint16_t x = UI_ICON_VOLUME_X;
    uint16_t y = UI_ICON_Y;
    
    /* 娓呴櫎鍥炬爣鍖哄煙 */
    BG_lcd.Box(x, y, 30, UI_ICON_SIZE, UI_STATUSBAR_BG_COLOR);
    
    /* 缁樺埗闊抽噺鍥炬爣鎴栭潤闊冲浘鏍�*/
    if (statusbar_data.muted) {
        draw_icon(x, y, icon_mute, UI_COLOR_RED);
    } else {
        draw_icon(x, y, icon_volume, UI_COLOR_WHITE);
    }
    
    /* 缁樺埗闊抽噺鏉�(绠�崟鐨�鏍兼寚绀� */
    x += 10;
    uint8_t bars = statusbar_data.volume / 17;  /* 0-2 bars */
    if (statusbar_data.volume > 0 && bars == 0) bars = 1;
    
    uint8_t i;
    for (i = 0; i < 3; i++) {
        uint16_t bar_color = (i < bars) ? UI_COLOR_GREEN : UI_COLOR_DARK_GRAY;
        uint16_t bar_h = 3 + i * 2;
        uint16_t bar_y = y + (8 - bar_h);
        BG_lcd.Box(x + i * 5, bar_y, 4, bar_h, bar_color);
    }
}

/**
 * @brief 鍒濆鍖栨娴嬪紩鑴�
 */
static void init_detect_pins(void)
{
    /* MIC妫�祴 - A30 (涓嬫媺,楂樼數骞虫湁鏁� */
    GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX30);
    GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX30);
    GPIO_RegOneBitClear(GPIO_A_PU, GPIO_INDEX30);
    GPIO_RegOneBitSet(GPIO_A_PD, GPIO_INDEX30);
    
    /* 鍚変粬妫�祴 - A29 (涓婃媺,浣庣數骞虫湁鏁� */
    GPIO_RegOneBitSet(GPIO_A_IE, GPIO_INDEX29);
    GPIO_RegOneBitClear(GPIO_A_OE, GPIO_INDEX29);
    GPIO_RegOneBitSet(GPIO_A_PU, GPIO_INDEX29);
    GPIO_RegOneBitClear(GPIO_A_PD, GPIO_INDEX29);
    
    /* 鑰虫満妫�祴 - B4 (涓婃媺,浣庣數骞虫湁鏁� */
    GPIO_RegOneBitSet(GPIO_B_IE, GPIO_INDEX4);
    GPIO_RegOneBitClear(GPIO_B_OE, GPIO_INDEX4);
    GPIO_RegOneBitSet(GPIO_B_PU, GPIO_INDEX4);
    GPIO_RegOneBitClear(GPIO_B_PD, GPIO_INDEX4);
}

/*===========================================================================
 * API 瀹炵幇
 *===========================================================================*/

void UI_StatusBar_Init(void)
{
    memset(&statusbar_data, 0, sizeof(statusbar_data));
    memset(&last_data, 0, sizeof(last_data));
    
    statusbar_data.bt_status = UI_BT_DISCONNECTED;
    statusbar_data.adc_source = UI_ADC_NONE;
    statusbar_data.dac_output = UI_DAC_SPKR;  /* 榛樿鎵０鍣ㄨ緭鍑�*/
    statusbar_data.volume = 50;
    statusbar_data.battery = 100;
    statusbar_data.muted = false;
    statusbar_data.usb_connected = false;
    statusbar_data.charging = false;
    
    statusbar_visible = true;
    need_redraw = true;
    
    /* 鍒濆鍖栨娴嬪紩鑴�*/
    init_detect_pins();
}

void UI_StatusBar_Draw(void)
{
    if (!statusbar_visible) return;
    
    /* 缁樺埗鑳屾櫙 */
    BG_lcd.Box(0, UI_STATUSBAR_Y, UI_SCREEN_WIDTH, UI_STATUSBAR_HEIGHT, UI_STATUSBAR_BG_COLOR);
    
    /* 缁樺埗鍚勫浘鏍�*/
    draw_bt_icon();
    draw_adc_icon();
    draw_dac_icon();
    draw_usb_icon();
    draw_volume_icon();
    
    /* 鏇存柊last_data */
    memcpy(&last_data, &statusbar_data, sizeof(statusbar_data));
    need_redraw = false;
}

void UI_StatusBar_Update(void)
{
    if (!statusbar_visible) return;
    
    /* 妫�煡鍙樺寲骞舵洿鏂�*/
    bool changed = false;
    
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
    
    if (changed) {
        memcpy(&last_data, &statusbar_data, sizeof(statusbar_data));
    }
}

void UI_StatusBar_ScanDetect(void)
{
    uint8_t new_adc = UI_ADC_NONE;
    uint8_t new_dac = UI_DAC_NONE;
    

    if (!GPIO_RegOneBitGet(UI_DET_MIC_PORT, UI_DET_MIC_PIN)) {
        new_adc |= UI_ADC_MIC;
    }
    
    /* 鎵弿鍚変粬妫�祴 (涓婃媺,浣庣數骞�鎻掑叆) */
    if (GPIO_RegOneBitGet(UI_DET_GUITAR_PORT, UI_DET_GUITAR_PIN)) {
        new_adc |= UI_ADC_GUITAR;
    }
    
    /* 鎵弿鑰虫満妫�祴 (涓婃媺,浣庣數骞�鎻掑叆) */
    if (GPIO_RegOneBitGet(UI_DET_HP_PORT, UI_DET_HP_PIN)) {
        new_dac |= UI_DAC_HP;
    } else {
        new_dac |= UI_DAC_SPKR;  /* 鏃犺�鏈哄垯浣跨敤鎵０鍣�*/
    }
    
    /* 鏇存柊鐘舵� */
    statusbar_data.adc_source = new_adc;
    statusbar_data.dac_output = new_dac;
    
    /* 鎵弿闊抽噺鏃嬮挳 (ADC28) */
    GPIO_RegOneBitSet(UI_VOLUME_ADC_PORT, UI_VOLUME_ADC_PIN);
    uint16_t adc_val = ADC_SingleModeDataGet(UI_VOLUME_ADC_CHANNEL);
    /* ADC 12浣� 杞崲涓�-100闊抽噺 */
    uint8_t new_volume = (uint8_t)((adc_val * 100) / 4095);
    if (new_volume > 100) new_volume = 100;
    statusbar_data.volume = new_volume;
    

    /* 鎵弿USB杩炴帴鐘舵� */
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
