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
// #define BANBOX_1_0_V2    /* 旧版本板子存储版本二: 引脚不变, NOR#0→PSRAM, NOR#1→NAND */
//#define BANBOX_II        /* 新版本板子 */
 #define BANDATAHUB       /* BanDataHub 新板子: SSD1306+PSRAM+SDCard+Encoder+ADC */

/*===========================================================================
 * 硬件驱动层配置（父级宏，控制硬件驱动编译）
 *===========================================================================*/

#ifdef BANBOX_1_0
/* BANBOX_1_0 硬件资源: 2×W25Qxx NOR Flash, LCD, Battery, USB, BT */
#define HW_DRV_LCD_EN           1   /* ST7735 LCD 驱动 */
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
#define HW_DRV_LCD_EN           1
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
#define HW_DRV_LCD_EN           1   /* ST7735 LCD 驱动 */
#define HW_DRV_FLASH_NOR_EN     1   /* W25Qxx NOR Flash 驱动 (1片) */
#define HW_DRV_FLASH_NAND_EN    1   /* W25N02 NAND Flash 驱动 */
#define HW_DRV_PSRAM_EN         1   /* ESP-PSRAM64H PSRAM 驱动 */
#define HW_DRV_SDCARD_EN        1   /* SD Card SDIO 驱动 */
#define HW_DRV_BATTERY_EN       1   /* 电池管理驱动 */
#define HW_DRV_USB_CDC_EN       1   /* USB CDC 串口驱动 */
#define HW_DRV_BT_EN            1   /* 蓝牙/BLE 驱动 */
#endif // BANBOX_II

#ifdef BANDATAHUB
/* BANDATAHUB 硬件资源: SSD1306 OLED + PSRAM + SD Card + Encoder + ADC Volume */
#define HW_DRV_LCD_EN           0   /* ST7735 LCD 驱动 - 不使用 */
#define HW_DRV_SSD1306_EN       1   /* SSD1306 OLED IIC 驱动 */
#define HW_DRV_ENCODER_EN       1   /* 旋转编码器驱动 (带按键) */
#define HW_DRV_FLASH_NOR_EN     0   /* 无 NOR Flash */
#define HW_DRV_FLASH_NAND_EN    0   /* 无 NAND Flash */
#define HW_DRV_PSRAM_EN         1   /* ESP-PSRAM64H PSRAM 驱动 */
#define HW_DRV_SDCARD_EN        1   /* SD Card SDIO 驱动 */
#define HW_DRV_BATTERY_EN       1   /* 无电池管理 */
#define HW_DRV_USB_CDC_EN       1   /* USB CDC 串口驱动 */
#define HW_DRV_BT_EN            1   /* 无蓝牙/BLE */
#endif // BANDATAHUB

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
#elif defined(BANDATAHUB)
#define LINEIN_EN               1
#define MIC_EN                  0
#define LINE1_INPUT_DETECT_EN   0
#define LINE2_INPUT_DETECT_EN   0
#define MIC_INPUT_DETECT_EN     0
#else
#define LINEIN_EN               0
#define MIC_EN                  0
#define LINE1_INPUT_DETECT_EN   0
#define LINE2_INPUT_DETECT_EN   0
#define MIC_INPUT_DETECT_EN     0
#endif

/* 核心功能 */
#define VFS_EN                  1   /* 虚拟文件系统 */
#define BG_EVENT_EN             0   /* 事件发布-订阅系统 (话题订阅) */
#ifdef BANDATAHUB
#define EFFECT_GRAPHICS_EN      0   /* 音效处理图 - BanDataHub不需要 */
#else
#define EFFECT_GRAPHICS_EN      1   /* 音效处理图 */
#endif

/* USB 功能 (依赖 HW_DRV_USB_CDC_EN) */
#if HW_DRV_USB_CDC_EN
#define USB_EN                  1   /* USB 功能总开关 */
#else
#define USB_EN                  0
#endif

/* MIDI 合成器功能 (依赖 NOR Flash) */
#ifdef BANBOX_1_0
#define BANGTSYNTH_EN           0   /* 暂时禁用：节省 ~885 KB Flash */
#elif defined(BANBOX_II)
#define BANGTSYNTH_EN           0   /* 暂未移植到新板 */
#elif defined(BANDATAHUB)
#define BANGTSYNTH_EN           1   /* BanDataHub: SD卡+PSRAM方案启用合成器 */
#else
//#define BANGTSYNTH_EN           0
#endif

/* 高级合成器功能 (SD卡 + PSRAM 直读方案, 无需NAND) */
#if defined(BANDATAHUB) && HW_DRV_SDCARD_EN && HW_DRV_PSRAM_EN
#define SYNTH_SD_NAND_PSRAM_EN  1   /* BanDataHub: SD卡直读 + PSRAM缓存 */
#elif HW_DRV_FLASH_NAND_EN && HW_DRV_PSRAM_EN
#define SYNTH_SD_NAND_PSRAM_EN  0   /* 待启用：需 NAND + PSRAM */
#else
#define SYNTH_SD_NAND_PSRAM_EN  0   /* 父级硬件未满足 */
#endif

/* FAT32 文件系统 (依赖 NAND 或 SD Card) */
#if HW_DRV_FLASH_NAND_EN || HW_DRV_SDCARD_EN
#define FAT32_EN                1   /* 启用 FAT32 文件系统 (SD 文件访问需要) */
#else
#define FAT32_EN                0   /* 父级硬件未满足 */
#endif

/* Bootloader: 使用 SDK Flash Boot 升级方式 (SD卡/U盘/PC) */
#define BOOTLOADER_EN           0

/* CDC 文件管理器 (NAND Flash 下载接口) */
#if HW_DRV_USB_CDC_EN && HW_DRV_FLASH_NAND_EN
#define CDC_FILE_MANAGER_EN     0   /* 启用 USB CDC NAND 下载功能 */
#else
#define CDC_FILE_MANAGER_EN     0   /* 父级硬件未满足 */
#endif

/* 按钮开机控制 */
#ifdef BANBOX_1_0
#define BUTTON_POWER_ENABLE     0   /* 上电直接开机，不需要按按钮 */
#elif defined(BANBOX_1_0_V2)
#define BUTTON_POWER_ENABLE     0
#elif defined(BANBOX_II)
#define BUTTON_POWER_ENABLE     1   /* 需长按按钮1秒才能开机 */
#elif defined(BANDATAHUB)
#define BUTTON_POWER_ENABLE     0   /* 暂时上电直接开机，不需要按按钮 */
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
#if FAT32_EN && !defined(BANDATAHUB)
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
#elif defined(BANDATAHUB)
/* BanDataHub 新板子配置 - PSRAM + SD Card + SSD1306 + Encoder */
#define HW_FLASH0_EN            0   /* 无 NOR Flash */
#define HW_FLASH1_EN            0   /* 无 NOR Flash */
#define HW_NAND0_EN             0   /* 无 NAND Flash */
#define HW_PSRAM0_EN            1   /* 有 PSRAM (ESP-PSRAM64H) */
#define HW_SDCARD0_EN           1   /* 有 SD Card (SDIO) */
#define HW_VOLUME_ADC_EN        1   /* 有音量旋钮 ADC */
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

#elif defined(BANDATAHUB)
/* BanDataHub 新板子引脚配置 */

/* PSRAM CS 在 GPIO_B (B4) */
#define HW_PSRAM0_CS_GPIO_IE    GPIO_B_IE
#define HW_PSRAM0_CS_GPIO_OE    GPIO_B_OE
#define HW_PSRAM0_CS_GPIO_OUT   GPIO_B_OUT
#define HW_FLASH0_CS_PIN        0       /* 无 NOR Flash */
#define HW_FLASH1_CS_PIN        0       /* 无 NOR Flash */
#define HW_NAND0_CS_PIN         0       /* 无 NAND Flash */
#define HW_PSRAM0_CS_PIN        4       /* PSRAM CS: GPIO_B4 */
#define HW_PSRAM0_CS_GPIO_PORT  GPIO_B_IN

/* SD Card SDIO 引脚 */
#define HW_SDCARD_DAT_PIN       15      /* SD Card DAT: GPIO_A15 */
#define HW_SDCARD_CLK_PIN       16      /* SD Card CLK: GPIO_A16 */
#define HW_SDCARD_CMD_PIN       17      /* SD Card CMD: GPIO_A17 */
#define HW_SDIO_PORT            HAL_SDIO_PORT_A15_A16_A17
#define HW_SDCARD_DET_PIN       5       /* SD Card DET: GPIO_B5 */

/* SSD1306 IIC 引脚配置 */
#define HW_SSD1306_I2C_ADDR     0x3C    /* SSD1306 I2C 地址 */
#define HW_SSD1306_SCL_PIN      31      /* SSD1306 SCL: GPIO_A31 */
#define HW_SSD1306_SDA_PIN      30      /* SSD1306 SDA: GPIO_A30 */

/* 编码器引脚配置 */
#define HW_ENCODER_A_PIN        20      /* 编码器 A 相: GPIO_A20 */
#define HW_ENCODER_B_PIN        21      /* 编码器 B 相: GPIO_A21 */
#define HW_ENCODER_BTN_PIN      22      /* 编码器按键: GPIO_A22 */

/* 音量旋钮 ADC */
#define HW_VOLUME_ADC_GPIO_PORT GPIO_A_ANA_EN
#define HW_VOLUME_ADC_GPIO_PIN  GPIO_INDEX29
#define HW_VOLUME_ADC_CHANNEL   ADC_CHANNEL_GPIOA29

/* 电池 ADC */
#define HW_BATTERY_ADC_GPIO_PORT GPIO_A_ANA_EN
#define HW_BATTERY_ADC_GPIO_PIN  GPIO_INDEX28
#define HW_BATTERY_ADC_CHANNEL   ADC_CHANNEL_GPIOA28

/* 电源按钮 */
#define HW_PWR_BTN_DET_PIN      0       /* 电源按钮检测: GPIO_A0 */
#define HW_PWR_BTN_HOLD_PIN     1       /* 电源保持: GPIO_A1 */

/* MIC 插入检测与模拟开关 */
#define HW_MIC_DET_PIN          23      /* MIC 插入检测: GPIO_A23 */
#define HW_MIC_SWITCH_PIN       24      /* MIC 模拟开关切换: GPIO_A24 */

/* 立体声输入模式切换 */
#define HW_STEREO_SWITCH_PIN    6       /* 立体声输入模式切换: GPIO_B6 */

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





#ifdef __cplusplus
}
#endif

#endif // PRODUCT_DEF_H
