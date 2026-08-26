/**
 * BanGTsynth 合成器配置文件
 * 
 * 集中管理所有模块的功能开关和参数配置。
 * 目标平台: BP10 (NDS32)
 */

#ifndef _BG_CONFIG_H__
#define _BG_CONFIG_H__

#include "bg_config_port.h"
#if BANGTSYNTH_EN

/* ============================================
 * 平台配置
 * ============================================ */
#define BG_PLATFORM_LINUX       1
#define BG_PLATFORM_STM32       2
#define BG_PLATFORM_ESP32       3
#define BG_PLATFORM_BP10        4

#ifndef BG_TARGET_PLATFORM
#define BG_TARGET_PLATFORM      BG_PLATFORM_BP10
#endif

/* ============================================
 * 功能模块开关
 * ============================================ */

/** MIDI 控制器 (~2KB RAM, ~5KB Flash) */
#ifndef ENABLE_MIDI_CONTROLLER
#define ENABLE_MIDI_CONTROLLER      1
#endif

/** 音频处理器 - 已由 Effect Graph 接管, 保持禁用 */
#ifndef ENABLE_AUDIO_PROCESSOR
#define ENABLE_AUDIO_PROCESSOR      0
#endif

/** 包络生成器 (~1KB RAM, ~2KB Flash) */
#ifndef ENABLE_ENVELOPE_GENERATOR
#define ENABLE_ENVELOPE_GENERATOR   1
#endif

/** 键盘输入 (仅 Linux 调试用) */
#ifndef ENABLE_KEYBOARD_INPUT
#define ENABLE_KEYBOARD_INPUT       0
#endif

/** 音源下载接口 */
#ifndef ENABLE_SOUNDBANK_DOWNLOAD
#define ENABLE_SOUNDBANK_DOWNLOAD   0
#endif

/* ============================================
 * 合成器核心参数
 * ============================================ */

/** SF2 格式支持 */
#ifndef SYNTH_ENABLE_SF2
#define SYNTH_ENABLE_SF2            1
#endif

/** X-Fi 引擎 (SF2 子功能, 默认禁用) */
#ifndef SYNTH_ENABLE_XFI_ENGINE
#define SYNTH_ENABLE_XFI_ENGINE     0
#endif

/** 音色数量上限 (每程序槽 ~28 字节) */
#ifndef SYNTH_MAX_PROGRAMS
#define SYNTH_MAX_PROGRAMS          16
#endif

/** 名称缓冲区大小 (字节) */
#ifndef SYNTH_NAME_BUFFER_SIZE
#define SYNTH_NAME_BUFFER_SIZE      32
#endif

/** SF2 声部池大小 (最大同时发声数) */
#ifndef SYNTH_MAX_VOICES
#define SYNTH_MAX_VOICES            BG_MAX_POLYPHONY
#endif

/* ============================================
 * 默认内嵌音源
 * ============================================ */
#define BG_SOUNDBANK_4OPFM          1
#define BG_SOUNDBANK_SOFT_PIANO     2

#ifndef BG_DEFAULT_SOUNDBANK
#define BG_DEFAULT_SOUNDBANK        BG_SOUNDBANK_SOFT_PIANO
#endif

/* ============================================
 * 音频参数
 * ============================================ */

#ifndef BG_SAMPLE_RATE
#if BG_TARGET_PLATFORM == BG_PLATFORM_BP10
#define BG_SAMPLE_RATE              44100
#else
#define BG_SAMPLE_RATE              48000
#endif
#endif

#ifndef BG_SAMPLE_WIDTH
#define BG_SAMPLE_WIDTH             16
#endif

#ifndef BG_CHANNELS
#define BG_CHANNELS                 1
#endif

#ifndef BG_MAX_POLYPHONY
#if BG_TARGET_PLATFORM == BG_PLATFORM_BP10
#define BG_MAX_POLYPHONY            8
#else
#define BG_MAX_POLYPHONY            64
#endif
#endif

#ifndef BG_AUDIO_BUFFER_SIZE
#define BG_AUDIO_BUFFER_SIZE        48
#endif

/* ============================================
 * 存储配置
 * ============================================ */

#ifndef BG_STORAGE_SIZE
#if (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)
#define BG_STORAGE_SIZE             (8 * 1024 * 1024)   /* BP10: 8MB */
#else
#define BG_STORAGE_SIZE             (64 * 1024 * 1024)  /* 其他: 64MB */
#endif
#endif

#ifndef BG_STORAGE_SECTOR_SIZE
#define BG_STORAGE_SECTOR_SIZE      4096
#endif

/* ============================================
 * MIDI 配置
 * ============================================ */
#ifndef BG_MIDI_CHANNELS
#define BG_MIDI_CHANNELS            16
#endif

/* ============================================
 * 调试配置
 * ============================================ */

/** 日志级别: 0=关闭, 1=错误, 2=警告, 3=信息, 4=调试 */
#ifndef BG_LOG_LEVEL
#define BG_LOG_LEVEL                2
#endif

#ifndef BG_DEBUG_MIDI
#define BG_DEBUG_MIDI               0
#endif

#ifndef BG_DEBUG_AUDIO_PROC
#define BG_DEBUG_AUDIO_PROC         1
#endif

#ifndef BG_DEBUG_SOUNDBANK
#define BG_DEBUG_SOUNDBANK          1
#endif

#ifndef BG_DEBUG_EFFECT_DRC
#define BG_DEBUG_EFFECT_DRC         1
#endif

#ifndef BG_DEBUG_EFFECT_EQ
#define BG_DEBUG_EFFECT_EQ          1
#endif

/* ============================================
 * 内存配置
 * ============================================ */

#ifndef BG_USE_DYNAMIC_MEMORY
#define BG_USE_DYNAMIC_MEMORY       1
#endif

#ifndef BG_ENABLE_FAST_MATH
#define BG_ENABLE_FAST_MATH         1
#endif

/* ============================================
 * 编译时校验
 * ============================================ */

#if BG_MAX_POLYPHONY > 128
#error "BG_MAX_POLYPHONY must be <= 128"
#endif

#if BG_SAMPLE_RATE != 44100 && BG_SAMPLE_RATE != 48000 && BG_SAMPLE_RATE != 96000
#error "BG_SAMPLE_RATE must be 44100, 48000, or 96000"
#endif

#if BG_CHANNELS != 1 && BG_CHANNELS != 2
#error "BG_CHANNELS must be 1 or 2"
#endif

#endif /* BANGTSYNTH_EN */

/* ============================================
 * 兼容性别名 (旧代码引用)
 * 必须放在 BANGTSYNTH_EN 外部，保证始终可用
 * ============================================ */
#ifndef BG_AUDIO_BIT_DEPTH
#define BG_AUDIO_BIT_DEPTH          16
#endif
#ifndef BG_MAX_CHANNELS
#define BG_MAX_CHANNELS             BG_CHANNELS
#endif
#ifndef BG_MS_SAMPLE
#define BG_MS_SAMPLE                (BG_SAMPLE_RATE / 1000)
#endif
#ifndef BG_BUFFER_SIZE
#define BG_BUFFER_SIZE              BG_AUDIO_BUFFER_SIZE
#endif
#ifndef BG_BYTES_PER_SAMPLE
#define BG_BYTES_PER_SAMPLE         (sizeof(int16_t) * BG_CHANNELS)
#endif
#ifndef BG_ENABLE_KEYBOARD_INPUT
#define BG_ENABLE_KEYBOARD_INPUT    ENABLE_KEYBOARD_INPUT
#endif
#ifndef BG_AUDIO_BUFFER_SIZE
#define BG_AUDIO_BUFFER_SIZE        48
#endif
#ifndef BG_CHANNELS
#define BG_CHANNELS                 1
#endif
#ifndef BG_SAMPLE_RATE
#define BG_SAMPLE_RATE              44100
#endif
#ifndef ENABLE_KEYBOARD_INPUT
#define ENABLE_KEYBOARD_INPUT       0
#endif

#endif /* _BG_CONFIG_H__ */