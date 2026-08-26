/**
 * 复制为工程里的 bg_config_port.h，或合并进现有 bg_config_port.h。
 * 不要直接编译本文件。
 */
#ifndef BG_CONFIG_PORT_H
#define BG_CONFIG_PORT_H

#define BANGTSYNTH_EN               1
#define SYNTH_SD_NAND_PSRAM_EN      0   /* 仅内嵌/XIP 音源时保持 0 */

#define BG_CFG_HAS_NAND             0
#define BG_CFG_USE_HOST_FAT32       0
#define BG_CFG_EMBEDDED_SF2         1
#define BG_CFG_RUN_STARTUP_TESTS    0
#define BG_CFG_USE_PORT_STORAGE     1

#define ENABLE_SOUNDBANK_DOWNLOAD   0
#define ENABLE_KEYBOARD_INPUT       0

#ifndef BG_TARGET_PLATFORM
#define BG_TARGET_PLATFORM          BG_PLATFORM_STM32  /* 先在 bg_config.h 里加枚举或沿用已有值 */
#endif

#ifndef BG_SAMPLE_RATE
#define BG_SAMPLE_RATE              44100
#endif
#ifndef BG_MAX_POLYPHONY
#define BG_MAX_POLYPHONY            8
#endif
#ifndef BG_LOG_LEVEL
#define BG_LOG_LEVEL                1
#endif

#endif
