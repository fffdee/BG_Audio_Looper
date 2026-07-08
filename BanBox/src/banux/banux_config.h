/**
 *****************************************************************************
 * @file     banux_config.h
 * @brief    BanUX framework feature configuration
 *
 * This header centralizes BanUX feature switches. Board-specific values are
 * still selected in product_def.h; every macro here is guarded so external
 * board configuration can override the defaults.
 *****************************************************************************
 */

#ifndef __BANUX_CONFIG_H__
#define __BANUX_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Core framework
 *===========================================================================*/
#ifndef VFS_EN
#define VFS_EN                      0   /* Virtual file system */
#endif

#ifndef BG_EVENT_EN
#define BG_EVENT_EN                 1   /* Event publish/subscribe topics */
#endif

#ifndef SYS_LED_EN
#define SYS_LED_EN                  1   /* System status LED (GPIOA15) */
#endif

#ifndef EFFECT_GRAPHICS_EN
#define EFFECT_GRAPHICS_EN          1   /* Effect graph component */
#endif

/*===========================================================================
 * Hardware driver framework switches
 *===========================================================================*/
#ifndef HW_DRV_LCD_EN
#define HW_DRV_LCD_EN               0   /* ST7735 LCD driver (currently unused) */
#endif

#ifndef HW_DRV_FLASH_NOR_EN
#define HW_DRV_FLASH_NOR_EN         0   /* W25Qxx NOR Flash VFS driver */
#endif

#ifndef HW_DRV_FLASH_NAND_EN
#define HW_DRV_FLASH_NAND_EN        0   /* W25N02 NAND Flash VFS driver */
#endif

#ifndef HW_DRV_PSRAM_EN
#define HW_DRV_PSRAM_EN             0   /* ESP-PSRAM64H PSRAM VFS driver */
#endif

#ifndef HW_DRV_SDCARD_EN
#define HW_DRV_SDCARD_EN            0   /* SD Card VFS driver */
#endif

#ifndef HW_DRV_BATTERY_EN
#define HW_DRV_BATTERY_EN           1   /* Battery manager VFS driver */
#endif

#ifndef HW_DRV_USB_CDC_EN
#define HW_DRV_USB_CDC_EN           1   /* USB CDC VFS driver */
#endif

#ifndef HW_DRV_BT_EN
#define HW_DRV_BT_EN                1   /* Bluetooth/BLE VFS drivers */
#endif

/*===========================================================================
 * Concrete board device instances
 *===========================================================================*/
#ifndef HW_FLASH0_EN
#define HW_FLASH0_EN                0   /* NOR Flash #0 */
#endif

#ifndef HW_FLASH1_EN
#define HW_FLASH1_EN                0   /* NOR Flash #1 */
#endif

#ifndef HW_NAND0_EN
#define HW_NAND0_EN                 0   /* W25N02 NAND Flash */
#endif

#ifndef HW_PSRAM0_EN
#define HW_PSRAM0_EN                0   /* ESP-PSRAM64H PSRAM */
#endif

#ifndef HW_SDCARD0_EN
#define HW_SDCARD0_EN               0   /* SD Card over SDIO */
#endif

#ifndef HW_VOLUME_ADC_EN
#define HW_VOLUME_ADC_EN            0   /* Volume knob ADC */
#endif

/*===========================================================================
 * Feature modules
 *===========================================================================*/
#ifndef LINEIN_EN
#define LINEIN_EN                   0   /* Legacy dual line-in switch */
#endif

#ifndef LINE1_EN
#define LINE1_EN                    0   /* Line input 1 */
#endif

#ifndef LINE2_EN
#define LINE2_EN                    0   /* Line input 2 */
#endif

#ifndef MIC_EN
#define MIC_EN                      0   /* Microphone input */
#endif

#ifndef LINE1_INPUT_DETECT_EN
#define LINE1_INPUT_DETECT_EN       0   /* Line1 insertion detect */
#endif

#ifndef LINE2_INPUT_DETECT_EN
#define LINE2_INPUT_DETECT_EN       0   /* Line2 insertion detect */
#endif

#ifndef MIC_INPUT_DETECT_EN
#define MIC_INPUT_DETECT_EN         0   /* Microphone insertion detect */
#endif

#ifndef USB_EN
#define USB_EN                      HW_DRV_USB_CDC_EN
#endif

#ifndef FAT32_EN
#define FAT32_EN                    0   /* FAT32 filesystem */
#endif

#ifndef FLASH_TEST_EN
#define FLASH_TEST_EN               0   /* Flash test module */
#endif

#ifndef BOOTLOADER_EN
#define BOOTLOADER_EN               0   /* Bootloader support */
#endif

#ifndef POWER_ON_MUSIC_EN
#define POWER_ON_MUSIC_EN           0   /* Synthesized power-on music */
#endif

#ifndef BUTTON_POWER_ENABLE
#define BUTTON_POWER_ENABLE         0   /* Button-controlled power-on */
#endif

#ifndef REVERB_RAM_OPTIMIZE
#define REVERB_RAM_OPTIMIZE         0   /* Reverb RAM optimization */
#endif

#ifndef LOOPER_STORAGE_TYPE
#define LOOPER_STORAGE_TYPE         0   /* 0=auto, 1=PSRAM, 2=NAND, 3=NOR */
#endif

/*===========================================================================
 * Shell command switches
 *===========================================================================*/
#ifndef HW_CMD_FLASH_EN
#define HW_CMD_FLASH_EN             (HW_DRV_FLASH_NOR_EN || HW_DRV_FLASH_NAND_EN)
#endif

#ifndef HW_CMD_PSRAM_EN
#define HW_CMD_PSRAM_EN             HW_DRV_PSRAM_EN
#endif

#ifndef HW_CMD_FAT_EN
#define HW_CMD_FAT_EN               FAT32_EN
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BANUX_CONFIG_H__ */
