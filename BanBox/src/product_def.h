#ifndef PRODUCT_DEF_H
#define PRODUCT_DEF_H

// product_def.h
// Author: [Your Name]
// Created: [Date]
// Description: Product definitions and macros for BanBox project.

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 板级选择 (只能定义一个)
 *===========================================================================*/
// #define BANBOX_1_0       /* 旧版本板子: 2x NOR Flash */
 #define BANBOX_1_0_V2    /* 旧版本板子存储版本二: 引脚不变, NOR#0→PSRAM, NOR#1→NAND */
//#define BANBOX_II        /* 新版本板子 */

/*===========================================================================
 * 硬件驱动层配置（父级宏，控制硬件驱动编译）
 *===========================================================================*/

#ifdef BANBOX_1_0
/* BANBOX_1_0 硬件资源: 2×W25Qxx NOR Flash, LCD, Battery, USB, BT */
#define HW_DRV_LCD_EN           0   /* ST7735 LCD 驱动 - 已移除 */
#define HW_DRV_FLASH_NOR_EN     1   /* W25Qxx NOR Flash 驱动 (2片) */
#define HW_DRV_FLASH_NAND_EN    0   /* 无 NAND Flash */
#define HW_DRV_PSRAM_EN         0   /* 无 PSRAM */
#define HW_DRV_SDCARD_EN        0   /* 无 SD Card */
#define HW_DRV_BATTERY_EN       1   /* 电池管理驱动 */
#define HW_DRV_USB_CDC_EN       1   /* USB CDC 串口驱动 */
#define HW_DRV_BT_EN            1   /* 蓝牙/BLE 驱动 */
#endif // BANBOX_1_0

#ifdef BANBOX_1_0_V2
/* BANBOX_1_0_V2 硬件资源: W25N02 NAND + ESP-PSRAM64H (复用旧板引脚) */
#define HW_DRV_LCD_EN           0   /* ST7735 LCD 驱动 */
#define HW_DRV_FLASH_NOR_EN     0   /* 无 NOR Flash (被 NAND+PSRAM 取代) */
#define HW_DRV_FLASH_NAND_EN    1   /* W25N02 NAND Flash 驱动 */
#define HW_DRV_PSRAM_EN         1   /* ESP-PSRAM64H PSRAM 驱动 */
#define HW_DRV_SDCARD_EN        0   /* 无 SD Card */
#define HW_DRV_BATTERY_EN       1   /* 电池管理驱动 */
#define HW_DRV_USB_CDC_EN       1   /* USB CDC 串口驱动 */
#define HW_DRV_BT_EN            1   /* 蓝牙/BLE 驱动 */
#endif // BANBOX_1_0_V2

#ifdef BANBOX_1_1
/* BANBOX_1_1 硬件资源 (待完善) */
#define HW_DRV_LCD_EN           0   /* LCD 驱动 - 已移除 */
#define HW_DRV_FLASH_NOR_EN     1
#define HW_DRV_FLASH_NAND_EN    0
#define HW_DRV_PSRAM_EN         0
#define HW_DRV_SDCARD_EN        0
#define HW_DRV_BATTERY_EN       1
#define HW_DRV_USB_CDC_EN       1
#define HW_DRV_BT_EN            1
#endif // BANBOX_1_1

#ifdef BANBOX_II
/* BANBOX_II 硬件资源: NOR + NAND + PSRAM + SD Card (新引脚布局) */
#define HW_DRV_LCD_EN           0   /* ST7735 LCD 驱动 - 已移除 */
#define HW_DRV_FLASH_NOR_EN     1   /* W25Qxx NOR Flash 驱动 (1片) */
#define HW_DRV_FLASH_NAND_EN    1   /* W25N02 NAND Flash 驱动 */
#define HW_DRV_PSRAM_EN         1   /* ESP-PSRAM64H PSRAM 驱动 */
#define HW_DRV_SDCARD_EN        1   /* SD Card SDIO 驱动 */
#define HW_DRV_BATTERY_EN       1   /* 电池管理驱动 */
#define HW_DRV_USB_CDC_EN       1   /* USB CDC 串口驱动 */
#define HW_DRV_BT_EN            1   /* 蓝牙/BLE 驱动 */
#endif // BANBOX_II

/*===========================================================================
 * 内存优化配置
 *===========================================================================*/
/* Reverb内存优化：减小Effect Graph节点缓冲池以释放RAM给ReverbContext
 * ReverbContext需要~57.5KB，内部RAM只有~52KB可用
 * 将EFFECT_GRAPH_BUFFER_SIZE从200降到128，节省6336 bytes
 * 设置为0则使用默认值200（不优化） */
#define REVERB_RAM_OPTIMIZE     1

/*===========================================================================
 * 功能层配置（子级宏，依赖硬件驱动）
 *===========================================================================*/

/* 音频输入功能 */
#ifdef BANBOX_1_0
#define LINEIN_EN               1   /* Line In 输入 (双路) */
#define MIC_EN                  1   /* 麦克风输入 (双路) */
#define LINE1_INPUT_DETECT_EN   1   /* Line1 自动检测 */
#define LINE2_INPUT_DETECT_EN   1   /* Line2 自动检测 */
#define MIC_INPUT_DETECT_EN     1   /* 麦克风自动检测 */
#elif defined(BANBOX_1_0_V2)
#define LINEIN_EN               1
#define MIC_EN                  1
#define LINE1_INPUT_DETECT_EN   1
#define LINE2_INPUT_DETECT_EN   1
#define MIC_INPUT_DETECT_EN     1
#elif defined(BANBOX_1_1)
#define SOFT_POWER_MGR_EN       1   /* 软件电源管理 */
#define LINE1_EN                1   /* Line1 输入 */
#define LINE2_EN                1   /* Line2 输入 */
#define MIC_EN                  1
#define LINE1_INPUT_DETECT_EN   1
#define LINE2_INPUT_DETECT_EN   1
#define MIC_INPUT_DETECT_EN     1
#elif defined(BANBOX_II)
#define LINE1_EN                1
#define LINE2_EN                1
/* BanBox_II 无麦克风输入 */
#define MIC_EN                  0
#define LINE1_INPUT_DETECT_EN   1
#define LINE2_INPUT_DETECT_EN   1
#define MIC_INPUT_DETECT_EN     0
#else
#define LINEIN_EN               0
#define MIC_EN                  0
#define LINE1_INPUT_DETECT_EN   0
#define LINE2_INPUT_DETECT_EN   0
#define MIC_INPUT_DETECT_EN     0
#endif

/* 核心功能 */
#define VFS_EN                  0   /* 虚拟文件系统 */
#define BG_EVENT_EN             1   /* 事件发布-订阅系统 (话题订阅) */
#define EFFECT_GRAPHICS_EN      1   /* 音效处理图 */

/* 开机音乐 (POWER_ON_MUSIC_EN=0: 关闭，使用 RemindSound MP3 提示音)
 * POWER_ON_MUSIC_EN=1: 正弦波实时合成 C 和弦 (零 Flash 占用) */
#define POWER_ON_MUSIC_EN       0  /* 开机音乐播放模块 */

/* Looper 存储类型选择
 * 0 = 自动（根据 HW_PSRAM0_EN / HW_NAND0_EN 检测，推荐）
 * 1 = 强制 PSRAM (ESP-PSRAM64H，支持叠录)
 * 2 = 强制 NAND Flash (W25N02)
 * 3 = 强制 NOR Flash (W25Qxx)
 * 见 audio_looper.h 中 LOOPER_STORAGE_TYPE_xxx 常量定义 */
#define LOOPER_STORAGE_TYPE     0   /* 自动检测 */

/* USB 功能 (依赖 HW_DRV_USB_CDC_EN) */
#if HW_DRV_USB_CDC_EN
#define USB_EN                  1   /* USB 功能总开关 */
#else
#define USB_EN                  0
#endif

/* MIDI 合成器已移除 — 节省 ~885 KB Flash */

/* FAT32 文件系统 (依赖 NAND 或 SD Card) */
#if HW_DRV_FLASH_NAND_EN || HW_DRV_SDCARD_EN
#define FAT32_EN                0   /* 启用 FAT32 文件系统 */
#else
#define FAT32_EN                0   /* 父级硬件未满足 */
#endif

/* Bootloader: 使用 SDK Flash Boot 升级方式 (SD卡/U盘/PC) */
#define BOOTLOADER_EN           0

/* Flash 测试模块 (RAM优化: 设0可释放~13KB, 测试时设1) */
#define FLASH_TEST_EN           0

/* 按钮开机控制 */
#ifdef BANBOX_1_0
#define BUTTON_POWER_ENABLE     0   /* 上电直接开机，不需要按按钮 */
#elif defined(BANBOX_1_0_V2)
#define BUTTON_POWER_ENABLE     0
#elif defined(BANBOX_II)
#define BUTTON_POWER_ENABLE     1   /* 需长按按钮1秒才能开机 */
#else
#define BUTTON_POWER_ENABLE     0
#endif

/*===========================================================================
 * Shell 命令层配置（依赖硬件驱动和功能模块）
 *===========================================================================*/

/* Flash 命令 (依赖任一 Flash 硬件) */
#if HW_DRV_FLASH_NOR_EN || HW_DRV_FLASH_NAND_EN
#define HW_CMD_FLASH_EN         1
#else
#define HW_CMD_FLASH_EN         0
#endif

/* PSRAM 命令 (依赖 PSRAM 硬件) */
#if HW_DRV_PSRAM_EN
#define HW_CMD_PSRAM_EN         1
#else
#define HW_CMD_PSRAM_EN         0
#endif

/* FAT32 命令 (依赖 FAT32 功能) */
#if FAT32_EN
#define HW_CMD_FAT_EN           1
#else
#define HW_CMD_FAT_EN           0
#endif

/*===========================================================================
 * 硬件设备实例配置（子级：具体设备，依赖硬件驱动）
 *===========================================================================*/

#ifdef BANBOX_1_0
/* 旧板子配置 - 两片 NOR Flash */
#define HW_FLASH0_EN            1   /* 有 NOR Flash #0 */
#define HW_FLASH1_EN            1   /* 有 NOR Flash #1 */
#define HW_NAND0_EN             0   /* 无 NAND Flash */
#define HW_PSRAM0_EN            0   /* 无 PSRAM */
#define HW_SDCARD0_EN           0   /* 无 SD Card */
#define HW_VOLUME_ADC_EN        1   /* 有音量旋钮 ADC */
#elif defined(BANBOX_1_0_V2)
/* 旧板子存储版本二 - 引脚不变, NOR#0→PSRAM(A21), NOR#1→NAND(A22) */
#define HW_FLASH0_EN            0   /* NOR#0 引脚被 PSRAM 复用 */
#define HW_FLASH1_EN            0   /* NOR#1 引脚被 NAND 复用 */
#define HW_NAND0_EN             1   /* 有 NAND Flash (W25N02, 原NOR#1 引脚) */
#define HW_PSRAM0_EN            1   /* 有 PSRAM (ESP-PSRAM64H, 原NOR#0 引脚) */
#define HW_SDCARD0_EN           0   /* 无 SD Card */
#define HW_VOLUME_ADC_EN        1   /* 有音量旋钮 ADC */
#elif defined(BANBOX_II)
/* 新板子配置 - NOR + NAND + PSRAM + SD Card */
#define HW_FLASH0_EN            1   /* 有 NOR Flash #0 */
#define HW_FLASH1_EN            0   /* 无第二片 NOR Flash */
#define HW_NAND0_EN             1   /* 有 NAND Flash (W25N02) */
#define HW_PSRAM0_EN            1   /* 有 PSRAM (ESP-PSRAM64H) */
#define HW_SDCARD0_EN           1   /* 有 SD Card (SDIO) */
#define HW_VOLUME_ADC_EN        0   /* 无音量旋钮（引脚被NOR CS占用）*/
#else
/* 默认配置 */
#define HW_FLASH0_EN            0
#define HW_FLASH1_EN            0
#define HW_NAND0_EN             0
#define HW_PSRAM0_EN            0
#define HW_SDCARD0_EN           0
#define HW_VOLUME_ADC_EN        0 
#endif

/*===========================================================================
 * 硬件引脚配置
 *===========================================================================*/

#ifdef BANBOX_1_0
/* 旧板子配置 - 两片 NOR Flash */
/* PSRAM 不存在，GPIO 别名占位 (不会被引用) */
#define HW_PSRAM0_CS_GPIO_IE    GPIO_A_IE
#define HW_PSRAM0_CS_GPIO_OE    GPIO_A_OE
#define HW_PSRAM0_CS_GPIO_OUT   GPIO_A_OUT
#define HW_FLASH0_CS_PIN        21      /* NOR Flash #0 CS: GPIO_A21 */
#define HW_FLASH1_CS_PIN        22      /* NOR Flash #1 CS: GPIO_A22 */
#define HW_NAND0_CS_PIN         0       /* 旧板子无 NAND Flash */
#define HW_PSRAM0_CS_PIN        0       /* 旧板子无 PSRAM */
#define HW_PSRAM0_CS_GPIO_PORT  GPIO_A_IN

#define HW_SDCARD_DAT_PIN       0
#define HW_SDCARD_CLK_PIN       0
#define HW_SDCARD_CMD_PIN       0
#define HW_SDIO_PORT            HAL_SDIO_PORT_A20_A21_A22

/* ADC 配置 */
#define HW_VOLUME_ADC_GPIO_PORT GPIO_A_ANA_EN
#define HW_VOLUME_ADC_GPIO_PIN  GPIO_INDEX28
#define HW_VOLUME_ADC_CHANNEL   ADC_CHANNEL_GPIOA28

#define HW_BATTERY_ADC_GPIO_PORT GPIO_A_ANA_EN
#define HW_BATTERY_ADC_GPIO_PIN  GPIO_INDEX31
#define HW_BATTERY_ADC_CHANNEL   ADC_CHANNEL_GPIOA31

#elif defined(BANBOX_1_0_V2)
/* 旧板子存储版本二 - 引脚不变, NOR#0→PSRAM(A21), NOR#1→NAND(A22) */
/* PSRAM CS 在 GPIO_A (A21) */
#define HW_PSRAM0_CS_GPIO_IE    GPIO_A_IE
#define HW_PSRAM0_CS_GPIO_OE    GPIO_A_OE
#define HW_PSRAM0_CS_GPIO_OUT   GPIO_A_OUT
#define HW_FLASH0_CS_PIN        21      /* 保留占位, 实际 slot0 = PSRAM */
#define HW_FLASH1_CS_PIN        22      /* 保留占位 */
#define HW_NAND0_CS_PIN         22      /* NAND Flash CS: GPIO_A22 (原NOR#1) */
#define HW_PSRAM0_CS_PIN        21      /* PSRAM CS: GPIO_A21 (原NOR#0) */
#define HW_PSRAM0_CS_GPIO_PORT  GPIO_A_IN

#define HW_SDCARD_DAT_PIN       0
#define HW_SDCARD_CLK_PIN       0
#define HW_SDCARD_CMD_PIN       0
#define HW_SDIO_PORT            HAL_SDIO_PORT_A20_A21_A22

/* ADC 配置 */
#define HW_VOLUME_ADC_GPIO_PORT GPIO_A_ANA_EN
#define HW_VOLUME_ADC_GPIO_PIN  GPIO_INDEX28
#define HW_VOLUME_ADC_CHANNEL   ADC_CHANNEL_GPIOA28

#define HW_BATTERY_ADC_GPIO_PORT GPIO_A_ANA_EN
#define HW_BATTERY_ADC_GPIO_PIN  GPIO_INDEX31
#define HW_BATTERY_ADC_CHANNEL   ADC_CHANNEL_GPIOA31

/* LED 指示灯 (GPIO_A16) */
#define HW_LED_GPIO_PIN          GPIO_INDEX16

#elif defined(BANBOX_II)
/* 新板子配置 - 只有一片 NOR Flash */
/* PSRAM CS 在 GPIO_B (B6) */
#define HW_PSRAM0_CS_GPIO_IE    GPIO_B_IE
#define HW_PSRAM0_CS_GPIO_OE    GPIO_B_OE
#define HW_PSRAM0_CS_GPIO_OUT   GPIO_B_OUT
#define HW_FLASH0_CS_PIN        28      /* NOR Flash #0 CS: GPIO_A28 */
#define HW_FLASH1_CS_PIN        22      /* 无效，仅占位 */
#define HW_NAND0_CS_PIN         29      /* NAND Flash CS: GPIO_A29 */
#define HW_PSRAM0_CS_PIN        6       /* PSRAM CS: GPIO_B6 */
#define HW_PSRAM0_CS_GPIO_PORT  GPIO_B_IN

#define HW_SDCARD_DAT_PIN       15      /* SD Card DAT: GPIO_A15 */
#define HW_SDCARD_CLK_PIN       16      /* SD Card CLK: GPIO_A16 */
#define HW_SDCARD_CMD_PIN       17      /* SD Card CMD: GPIO_A17 */
#define HW_SDIO_PORT            HAL_SDIO_PORT_A15_A16_A17

/* ADC 配置 */
#define HW_BATTERY_ADC_GPIO_PORT GPIO_A_ANA_EN
#define HW_BATTERY_ADC_GPIO_PIN  GPIO_INDEX31
#define HW_BATTERY_ADC_CHANNEL   ADC_CHANNEL_GPIOA31

#else
/* 默认配置 */
#define HW_FLASH0_CS_PIN        21
#define HW_FLASH1_CS_PIN        22
#define HW_NAND0_CS_PIN         23
#define HW_PSRAM0_CS_PIN        24
#define HW_PSRAM0_CS_GPIO_PORT  GPIO_A_IN

#define HW_SDCARD_DAT_PIN       20
#define HW_SDCARD_CLK_PIN       21
#define HW_SDCARD_CMD_PIN       22

#define HW_VOLUME_ADC_GPIO_PORT GPIO_A_ANA_EN
#define HW_VOLUME_ADC_GPIO_PIN  GPIO_INDEX28
#define HW_VOLUME_ADC_CHANNEL   ADC_CHANNEL_GPIOA28

#define HW_BATTERY_ADC_GPIO_PORT GPIO_A_ANA_EN
#define HW_BATTERY_ADC_GPIO_PIN  GPIO_INDEX31
#define HW_BATTERY_ADC_CHANNEL   ADC_CHANNEL_GPIOA31

#endif    

#include "banux/banux_config.h"

#ifdef __cplusplus
}
#endif

#endif // PRODUCT_DEF_H
