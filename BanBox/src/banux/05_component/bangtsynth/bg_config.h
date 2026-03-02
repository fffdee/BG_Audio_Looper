/**
 * BanGTsynth 框架配置文件
 * 
 * 功能:
 * - 集中管理所有模块的功能开关
 * - 配置音频参数、缓冲区大小等
 * - 支持平台特定的配置
 * - 完全解耦 CMake,所有配置在此文件中定义
 * 
 * 使用方法:
 * - 方式1: 手动修改此文件中的宏定义
 * - 方式2: 使用图形化配置工具: python components/config_tool.py
 * - 修改后重新编译生效 (不需要修改 CMakeLists.txt)
 * 
 * 条件编译说明:
 * - 将 ENABLE_XXX 设为 0 可禁用对应模块
 * - 代码中使用 #if ENABLE_XXX 进行条件编译
 * - CMakeLists.txt 会编译所有源文件,但禁用的模块不会被链接使用
 */

#ifndef _BG_CONFIG_H__
#define _BG_CONFIG_H__

/* ============================================
 * 平台配置
 * ============================================ */
#include "product_def.h"
#ifdef BANGTSYNTH_EN
/**
 * 目标平台选择
 * 可选值: BG_PLATFORM_LINUX, BG_PLATFORM_STM32, BG_PLATFORM_ESP32, BG_PLATFORM_BP10
 */
#define BG_PLATFORM_LINUX       1
#define BG_PLATFORM_STM32       2
#define BG_PLATFORM_ESP32       3
#define BG_PLATFORM_BP10        4

#ifndef BG_TARGET_PLATFORM
#define BG_TARGET_PLATFORM      BG_PLATFORM_BP10
#endif

/* ============================================
 * 功能模块裁剪
 * ============================================ */

/**
 * MIDI 控制器
 * 功能: MIDI 消息解析、音符管理、通道管理
 * 资源占用: ~2KB RAM, ~5KB Flash
 */
#ifndef ENABLE_MIDI_CONTROLLER
#define ENABLE_MIDI_CONTROLLER  1  // 1=启用, 0=禁用
#endif
#
/**
 * 混音器模块
 * 功能: 多音轨混音、音量控制、声道平衡
 * 资源占用: ~4KB RAM, ~3KB Flash
 */
#ifndef ENABLE_MIXER
#define ENABLE_MIXER            1  // 1=启用, 0=禁用
#endif

/**
 * 音频处理器
 * 功能: 动态范围压缩(DRC)、均衡器(EQ)、效果器
 * 资源占用: ~8KB RAM, ~10KB Flash
 */
#ifndef ENABLE_AUDIO_PROCESSOR
#define ENABLE_AUDIO_PROCESSOR  1  // 1=启用, 0=禁用
#endif

/**
 * 包络生成器
 * 功能: ADSR 包络控制、力度响应
 * 资源占用: ~1KB RAM, ~2KB Flash
 * 依赖: SF2 音源需要此模块
 */
#ifndef ENABLE_ENVELOPE_GENERATOR
#define ENABLE_ENVELOPE_GENERATOR 1  // 1=启用, 0=禁用
#endif

/**
 * 音序器
 * 功能: MIDI 序列播放、节拍控制
 * 资源占用: ~6KB RAM, ~8KB Flash
 */
#ifndef ENABLE_SEQUENCER
#define ENABLE_SEQUENCER        0  // 1=启用, 0=禁用 (默认禁用)
#endif

/**
 * USB MIDI 输入
 * 功能: 通过 USB 接收 MIDI 消息
 * 资源占用: ~2KB RAM, ~4KB Flash
 * 依赖: Linux ALSA / STM32 USB 库
 */
#ifndef ENABLE_USB_MIDI
#define ENABLE_USB_MIDI         0  // 1=启用, 0=禁用
#endif

/**
 * 键盘输入
 * 功能: 键盘映射到 MIDI 音符,用于调试
 * 资源占用: ~1KB RAM, ~2KB Flash
 * 依赖: Linux termios / 无依赖(MCU)
 */
#ifndef ENABLE_KEYBOARD_INPUT
#define ENABLE_KEYBOARD_INPUT   1  // 1=启用, 0=禁用
#endif

/**
 * 音源下载接口
 * 功能: 支持运行时下载音源到存储设备
 * 资源占用: ~2KB RAM, ~3KB Flash
 */
#ifndef ENABLE_SOUNDBANK_DOWNLOAD
#define ENABLE_SOUNDBANK_DOWNLOAD 1  // 1=启用, 0=禁用
#endif

/* ============================================
 * 合成器内存优化裁剪 (目标: ≤40KB 静态RAM)
 * ============================================ */

/**
 * SF2 格式支持
 * 功能: SoundFont 2 音源格式解析和播放
 * 禁用后只支持 BGS 自有格式,节省 ~4KB RAM + ~8KB Flash
 */
#ifndef SYNTH_ENABLE_SF2
#define SYNTH_ENABLE_SF2        1  // 1=启用, 0=禁用
#endif

/**
 * X-Fi 引擎支持 (SF2 子功能)
 * 功能: 支持 Creative X-Fi 编码的 SF2 文件
 * 禁用后仅支持标准 SF2 引擎,减少代码体积
 */
#ifndef SYNTH_ENABLE_XFI_ENGINE
#define SYNTH_ENABLE_XFI_ENGINE 0  // 1=启用, 0=禁用
#endif

/**
 * 音源程序(音色)数量上限
 * 标准 GM: 128, 精简模式: 8~16
 * 每个程序槽约 28 字节静态 + 动态采样数据
 */
#ifndef SYNTH_MAX_PROGRAMS
#define SYNTH_MAX_PROGRAMS      16
#endif

/* ============================================
 * 默认内嵌音源选择
 * ============================================ */

/**
 * 默认音源选择
 * 可选值:
 *   BG_SOUNDBANK_4OPFM        - 4OPFM.SF2 (FM打击乐, 998KB) [待实现]
 *   BG_SOUNDBANK_SOFT_PIANO   - Thrift Store Spinet Piano (钢琴音色, 391KB) [已实现]
 * 
 * 音源数据位于: BanBox/src/banux/05_component/bangtsynth/durm_data/sf2_source.c
 */
#define BG_SOUNDBANK_4OPFM        1
#define BG_SOUNDBANK_SOFT_PIANO   2

#ifndef BG_DEFAULT_SOUNDBANK
//#define BG_DEFAULT_SOUNDBANK    BG_SOUNDBANK_4OPFM  // 默认使用 4OPFM (待实现)
#define BG_DEFAULT_SOUNDBANK    BG_SOUNDBANK_SOFT_PIANO  // 默认使用 Thrift Store Spinet Piano (内嵌音源)
#endif

/**
 * 名称缓冲区大小 (字节)
 * SF2 银行名/引擎名缓冲区 (标准: 256, 精简: 32)
 */
#ifndef SYNTH_NAME_BUFFER_SIZE
#define SYNTH_NAME_BUFFER_SIZE  32
#endif

/**
 * SF2 声部池大小
 * 控制 SF2 最大同时发声数, 默认与 BG_MAX_POLYPHONY 一致
 */
#ifndef SYNTH_MAX_VOICES
#define SYNTH_MAX_VOICES        BG_MAX_POLYPHONY
#endif

/* ============================================
 * 音频参数配置
 * ============================================ */

/**
 * 采样率 (Hz)
 * 可选值: 44100, 48000, 96000
 * 建议: 48000 (CD 音质)
 */
#ifndef BG_SAMPLE_RATE
#if BG_TARGET_PLATFORM == BG_PLATFORM_BP10
#define BG_SAMPLE_RATE          44100  /* BP10: 与系统 DAC 采样率一致 */
#else
#define BG_SAMPLE_RATE          48000  
#endif
#endif

/**
 * 采样位深 (bits)
 * 可选值: 16, 24
 * 建议: 16 (足够,节省内存)
 */
#ifndef BG_SAMPLE_WIDTH
#define BG_SAMPLE_WIDTH         16  
#endif

/**
 * 声道数
 * 可选值: 1 (单声道), 2 (立体声)
 */
#ifndef BG_CHANNELS
#define BG_CHANNELS             1  
#endif

/**
 * 最大复音数
 * 说明: 同时发声的音符数量
 * 范围: 1-128
 * 资源影响: 每个复音 ~1KB RAM
 */
#ifndef BG_MAX_POLYPHONY
#if BG_TARGET_PLATFORM == BG_PLATFORM_BP10
#define BG_MAX_POLYPHONY        8   /* BP10: 限制复音数以节省 RAM 和 CPU */
#else
#define BG_MAX_POLYPHONY        64  
#endif
#endif

/**
 * 音频缓冲区大小 (frames)
 * 说明: 每次处理的样本帧数
 * 建议: 48 (1ms @ 48kHz), 96 (2ms), 192 (4ms)
 * 延迟影响: 越小延迟越低,但 CPU 占用越高
 */
#ifndef BG_AUDIO_BUFFER_SIZE
#define BG_AUDIO_BUFFER_SIZE    48  
#endif

/**
 * ALSA 硬件缓冲区大小 (ms)
 * 说明: 音频设备的缓冲时长
 * 建议: 8-32ms
 * 影响: 越大越稳定但延迟越高
 */
#ifndef BG_ALSA_BUFFER_MS
#define BG_ALSA_BUFFER_MS       16  
#endif

/**
 * ALSA Period 大小 (ms)
 * 说明: 每次中断的数据块大小
 * 建议: buffer_ms / 4
 */
#ifndef BG_ALSA_PERIOD_MS
#define BG_ALSA_PERIOD_MS       4  
#endif

/* ============================================
 * 存储配置
 * ============================================ */

/**
 * 音源存储大小 (bytes)
 * 说明: soundbank.bin 文件的固定大小
 * 可选值: 16MB, 32MB, 64MB
 */
#ifndef BG_STORAGE_SIZE
#define BG_STORAGE_SIZE         33554432  // 32MB
#endif

/**
 * 存储扇区大小 (bytes)
 * 说明: Flash 擦除的最小单位
 * 建议: 4096 (4KB)
 */
#ifndef BG_STORAGE_SECTOR_SIZE
#define BG_STORAGE_SECTOR_SIZE  4096  
#endif

/* ============================================
 * MIDI 配置
 * ============================================ */

/**
 * MIDI 通道数
 * 标准: 16 通道
 */
#ifndef BG_MIDI_CHANNELS
#define BG_MIDI_CHANNELS        16  
#endif

/**
 * MIDI 音符范围
 * 标准: 0-127
 */
#ifndef BG_MIDI_NOTE_MIN
#define BG_MIDI_NOTE_MIN        0  
#endif

#ifndef BG_MIDI_NOTE_MAX
#define BG_MIDI_NOTE_MAX        127  
#endif

/**
 * MIDI 程序数量 (音色数)
 * 标准: 128 (GM 标准)
 */
#ifndef BG_MIDI_PROGRAMS
#define BG_MIDI_PROGRAMS        128  
#endif

/* ============================================
 * 调试配置
 * ============================================ */

/**
 * 日志级别
 * 0 = 关闭, 1 = 错误, 2 = 警告, 3 = 信息, 4 = 调试
 */
#ifndef BG_LOG_LEVEL
#define BG_LOG_LEVEL            2  // 默认显示到 INFO
#endif

/**
 * 模块调试开关
 * 1 = 启用该模块的详细日志
 */
#ifndef BG_DEBUG_MIDI
#define BG_DEBUG_MIDI           0  
#endif

#ifndef BG_DEBUG_AUDIO_PROC
#define BG_DEBUG_AUDIO_PROC     1  
#endif

#ifndef BG_DEBUG_SOUNDBANK
#define BG_DEBUG_SOUNDBANK      1  
#endif

#ifndef BG_DEBUG_EFFECT_DRC
#define BG_DEBUG_EFFECT_DRC     1  
#endif

#ifndef BG_DEBUG_EFFECT_EQ
#define BG_DEBUG_EFFECT_EQ      1  
#endif

/* ============================================
 * 性能优化配置
 * ============================================ */

/**
 * 启用快速数学运算
 * 1 = 使用查表/近似算法代替精确计算
 * 影响: 提升 ~20% 性能,精度损失 <1%
 */
#ifndef BG_ENABLE_FAST_MATH
#define BG_ENABLE_FAST_MATH     1  
#endif

/**
 * 启用 SIMD 优化
 * 1 = 使用 SIMD 指令加速音频处理
 * 依赖: ARM NEON / x86 SSE2
 */
#ifndef BG_ENABLE_SIMD
#define BG_ENABLE_SIMD          0  // 默认禁用,需要平台支持
#endif

/**
 * 主循环延时 (微秒)
 * 说明: 控制 ProcessAudio() 调用频率
 * 建议: 500 (2000Hz) ~ 1000 (1000Hz)
 * 影响: 越小 CPU 占用越高,响应越快
 */
#ifndef BG_MAIN_LOOP_DELAY_US
#define BG_MAIN_LOOP_DELAY_US   400  
#endif

/* ============================================
 * 内存配置 (适用于嵌入式平台)
 * ============================================ */

/**
 * 动态内存分配
 * 0 = 使用静态内存池, 1 = 使用 malloc/free
 */
#ifndef BG_USE_DYNAMIC_MEMORY
#if BG_TARGET_PLATFORM == BG_PLATFORM_BP10
#define BG_USE_DYNAMIC_MEMORY   1  /* BP10: 使用 FreeRTOS pvPortMalloc */
#else
#define BG_USE_DYNAMIC_MEMORY   1  
#endif
#endif

/**
 * 静态内存池大小 (bytes)
 * 仅当 BG_USE_DYNAMIC_MEMORY=0 时使用
 */
#ifndef BG_MEMORY_POOL_SIZE
#define BG_MEMORY_POOL_SIZE     131072  // 128KB
#endif

/* ============================================
 * 配置验证 (编译时检查)
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

#if ENABLE_ENVELOPE_GENERATOR == 0 && ENABLE_SOUNDBANK_DOWNLOAD == 1
#warning "SF2 soundbank requires ENABLE_ENVELOPE_GENERATOR=1"
#endif

/* ============================================
 * 配置摘要宏 (用于打印配置信息)
 * ============================================ */

#define BG_CONFIG_STRING \
    "BanGTsynth Configuration:\n" \
    "  Platform: " _BG_PLATFORM_NAME "\n" \
    "  Sample Rate: " _STR(BG_SAMPLE_RATE) " Hz\n" \
    "  Channels: " _STR(BG_CHANNELS) "\n" \
    "  Max Polyphony: " _STR(BG_MAX_POLYPHONY) "\n" \
    "  Buffer Size: " _STR(BG_AUDIO_BUFFER_SIZE) " frames\n" \
    "  Modules: MIDI=" _STR(ENABLE_MIDI_CONTROLLER) \
             " Mixer=" _STR(ENABLE_MIXER) \
             " AudioProc=" _STR(ENABLE_AUDIO_PROCESSOR) "\n"

#define _STR(x) #x
#define _XSTR(x) _STR(x)

#if BG_TARGET_PLATFORM == BG_PLATFORM_LINUX
#define _BG_PLATFORM_NAME "Linux"
#elif BG_TARGET_PLATFORM == BG_PLATFORM_STM32
#define _BG_PLATFORM_NAME "STM32"
#elif BG_TARGET_PLATFORM == BG_PLATFORM_ESP32
#define _BG_PLATFORM_NAME "ESP32"
#elif BG_TARGET_PLATFORM == BG_PLATFORM_BP10
#define _BG_PLATFORM_NAME "BP10"
#else
#define _BG_PLATFORM_NAME "Unknown"
#endif

/* ============================================
 * 兼容性定义 (旧版本宏名称映射)
 * ============================================ */

/* 功能模块开关兼容性 */
#define BG_ENABLE_MIXER             ENABLE_MIXER
#define BG_ENABLE_MIDI_CONTROLLER   ENABLE_MIDI_CONTROLLER
#define BG_ENABLE_AUDIO_PROCESSOR   ENABLE_AUDIO_PROCESSOR
#define BG_ENABLE_USB_MIDI          ENABLE_USB_MIDI
#define BG_ENABLE_KEYBOARD_INPUT    ENABLE_KEYBOARD_INPUT

/* 音频参数兼容性 */
#define BG_AUDIO_BIT_DEPTH          16  
#define BG_MAX_CHANNELS             BG_CHANNELS
#define BG_MS_SAMPLE                (BG_SAMPLE_RATE / 1000)
#define BG_BUFFER_SIZE              BG_AUDIO_BUFFER_SIZE
#define BG_MAX_RING_BUFFER_SIZE     (BG_AUDIO_BUFFER_SIZE * 2)
#define BG_BYTES_PER_SAMPLE         (sizeof(int16_t) * BG_CHANNELS)

/* 混音器配置兼容性 */
#if ENABLE_MIXER
    #define BG_MAX_MIX_COUNT        10
    #define BG_MIX_BUF_COUNT        1024
#endif

/* 文件系统配置 */
#define BG_MAX_FILE_COUNT           100
#define BG_WAV_START_ADDRESS        0x19000

/* BGS 文件格式配置 */
#define BG_FILE_HEADER_BYTE         1
#define BG_PROGRAM_COUNT_BYTE       2
#define BG_FILE_VERSION_BYTE        3
#define BG_FILE_ENCODER_BYTE        1
#define BG_FILE_AUTHOR_BYTE         1
#define BG_FILE_EMAIL_BYTE          1

#define BG_PROGRAM_HEADER_BYTE      2
#define BG_PROGRAM_BANK_BYTE        1
#define BG_PROGRAM_INDEX_BYTE       1
#define BG_PROGRAM_NAME_BYTE        1
#define BG_PROGRAM_DESCRIPT_BYTE    1
#define BG_PROGRAM_TOTAL_BYTE       4
#define BG_PROGRAM_TYPE_BYTE        1

#define BG_WAV_HEADER_BYTE          1
#define BG_WAV_FILE_COUNT_BYTE      2
#define BG_WAV_SAMPLERATE_BYTE      4
#define BG_WAV_DEPTH_BYTE           1
#define BG_WAV_CHANNEL_BYTE         1
#define BG_WAV_FILESIZE_BYTE        4

#define BG_NOTE_HEADER_BYTE         1
#define BG_NOTE_BYTE                1
#define BG_NOTE_MIN_BYTE            1
#define BG_NOTE_MAX_BYTE            1
#define BG_VEL_COUNT_BYTE           1
#define BG_VEL_ID_BYTE              1
#define BG_VEL_MIN_BYTE             1
#define BG_VEL_MAX_BYTE             1

#define BG_DEBUG_ENABLED            1  

/* 错误码类型定义 */
typedef enum {
    BG_OK = 0,
    BG_ERROR,
    BG_ERROR_INVALID_PARAM,
    BG_ERROR_NOT_INITIALIZED,
    BG_ERROR_BUSY,
    BG_ERROR_TIMEOUT,
    BG_ERROR_NO_MEMORY
} bg_status_t;

#endif /* _BG_CONFIG_H__ */
#endif
