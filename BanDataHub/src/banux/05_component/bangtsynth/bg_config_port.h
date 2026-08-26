/**
 * @file bg_config_port.h
 * @brief 产品/板级 → BanGTsynth 功能开关（core 只认这些宏，不认板名）
 */
#ifndef BG_CONFIG_PORT_H
#define BG_CONFIG_PORT_H

/* 本仓库产品头：独立移植时可删掉此 include，改为在工程里预定义开关 */
#include "product_def.h"

#ifndef BANGTSYNTH_EN
#define BANGTSYNTH_EN 0
#endif

#ifndef SYNTH_SD_NAND_PSRAM_EN
#define SYNTH_SD_NAND_PSRAM_EN 0
#endif

/*
 * 板级映射。新增 MCU：复制 01_hal/port/template/bg_config_port.template.h
 * 的一组开关到这里，或编译命令 -DBG_CFG_*。
 */
#if defined(BANDATAHUB)
#ifndef BG_CFG_HAS_NAND
#define BG_CFG_HAS_NAND             0
#endif
#ifndef BG_CFG_USE_HOST_FAT32
#define BG_CFG_USE_HOST_FAT32       1
#endif
#ifndef BG_CFG_EMBEDDED_SF2
#define BG_CFG_EMBEDDED_SF2         0
#endif
#ifndef BG_CFG_RUN_STARTUP_TESTS
#define BG_CFG_RUN_STARTUP_TESTS    0
#endif
#ifndef BG_CFG_USE_PORT_STORAGE
#define BG_CFG_USE_PORT_STORAGE     1
#endif
#ifndef ENABLE_SOUNDBANK_DOWNLOAD
#define ENABLE_SOUNDBANK_DOWNLOAD   0
#endif
#ifndef ENABLE_KEYBOARD_INPUT
#define ENABLE_KEYBOARD_INPUT       0
#endif
#elif defined(BANBOX_II) || defined(BANBOX_1_0_V2)
#ifndef BG_CFG_HAS_NAND
#define BG_CFG_HAS_NAND             1
#endif
#ifndef BG_CFG_USE_HOST_FAT32
#define BG_CFG_USE_HOST_FAT32       0
#endif
#ifndef BG_CFG_EMBEDDED_SF2
#define BG_CFG_EMBEDDED_SF2         1
#endif
#ifndef BG_CFG_RUN_STARTUP_TESTS
#define BG_CFG_RUN_STARTUP_TESTS    1
#endif
#ifndef BG_CFG_USE_PORT_STORAGE
#define BG_CFG_USE_PORT_STORAGE     0
#endif
#endif

#ifndef BG_CFG_HAS_NAND
#define BG_CFG_HAS_NAND             0
#endif
#ifndef BG_CFG_USE_HOST_FAT32
#define BG_CFG_USE_HOST_FAT32       0
#endif
#ifndef BG_CFG_EMBEDDED_SF2
#define BG_CFG_EMBEDDED_SF2         1
#endif
#ifndef BG_CFG_RUN_STARTUP_TESTS
#define BG_CFG_RUN_STARTUP_TESTS    0
#endif
#ifndef BG_CFG_USE_PORT_STORAGE
#define BG_CFG_USE_PORT_STORAGE     1
#endif

#endif /* BG_CONFIG_PORT_H */
