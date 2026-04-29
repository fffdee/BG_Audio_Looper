#ifndef _BG_LOG_H__
#define _BG_LOG_H__

#include <stdint.h>

/**
 * BanGTsynth 日志系统
 * 功能: 提供统一的日志接口，解耦 printf 依赖
 * 特性: 支持模块标签、级别控制、调试开关、自定义输出函数
 * 
 * 使用示例:
 * 
 * 1. 基本使用:
 *    BG_Log.Init();
 *    BG_LOG_I(BG_LOG_TAG_MIDI, "MIDI initialized\n");
 * 
 * 2. 自定义串口输出 (嵌入式):
 *    void uart_output(bg_log_level_t level, bg_log_tag_t tag, const char *msg) {
 *        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 1000);
 *    }
 *    BG_Log.SetOutputFunc(uart_output);
 * 
 * 3. 文件日志输出:
 *    FILE *log_file = fopen("app.log", "a");
 *    void file_output(bg_log_level_t level, bg_log_tag_t tag, const char *msg) {
 *        fprintf(log_file, "%s", msg);
 *        fflush(log_file);
 *    }
 *    BG_Log.SetOutputFunc(file_output);
 * 
 * 4. RTT输出 (SEGGER):
 *    void rtt_output(bg_log_level_t level, bg_log_tag_t tag, const char *msg) {
 *        SEGGER_RTT_printf(0, "%s", msg);
 *    }
 *    BG_Log.SetOutputFunc(rtt_output);
 */

/* 日志级别 */
typedef enum {
    BG_LOG_LEVEL_ERROR = 0,     // 错误
    BG_LOG_LEVEL_WARN,          // 警告
    BG_LOG_LEVEL_INFO,          // 信息
    BG_LOG_LEVEL_DEBUG          // 调试
} bg_log_level_t;

/* 日志模块标签 */
typedef enum {
    BG_LOG_TAG_SYSTEM = 0,      // 系统
    BG_LOG_TAG_HAL,             // 硬件抽象层
    BG_LOG_TAG_MIXER,           // 混音器
    BG_LOG_TAG_MIDI,            // MIDI 控制器
    BG_LOG_TAG_AUDIO_PROC,      // 音频处理器
    BG_LOG_TAG_SOUNDBANK,       // 音色库
    BG_LOG_TAG_SEQUENCER,       // 音序器
    BG_LOG_TAG_ENVELOPE,        // 包络发生器
    BG_LOG_TAG_EFFECT_DRC,      // DRC 效果
    BG_LOG_TAG_EFFECT_EQ,       // EQ 效果
    BG_LOG_TAG_PLAY,            // 播放模块
    BG_LOG_TAG_FAT32,           // FAT32 文件系统
    BG_LOG_TAG_NAND,            // NAND Flash 存储
    BG_LOG_TAG_PSRAM,           // PSRAM 缓冲区
    BG_LOG_TAG_SYNTH,           // 合成器集成模块
    BG_LOG_TAG_MAX
} bg_log_tag_t;

/**
 * 日志输出函数类型 (用户可自定义实现)
 * @param level 日志级别
 * @param tag 模块标签
 * @param message 格式化后的完整消息 (包含前缀 [LEVEL][TAG])
 * 
 * 示例 - 串口输出:
 *   void uart_log_output(bg_log_level_t level, bg_log_tag_t tag, const char *msg) {
 *       uart_send_string(msg);
 *   }
 * 
 * 示例 - 文件输出:
 *   void file_log_output(bg_log_level_t level, bg_log_tag_t tag, const char *msg) {
 *       fprintf(log_file, "%s", msg);
 *       fflush(log_file);
 *   }
 * 
 * 示例 - RTT输出 (嵌入式):
 *   void rtt_log_output(bg_log_level_t level, bg_log_tag_t tag, const char *msg) {
 *       SEGGER_RTT_printf(0, "%s", msg);
 *   }
 */
typedef void (*bg_log_output_func_t)(bg_log_level_t level, bg_log_tag_t tag, const char *message);

/**
 * 日志接口
 */
typedef struct {
    /**
     * 初始化日志系统
     */
    void (*Init)(void);
    
    /**
     * 设置日志输出函数 (用于替换默认的 printf)
     * @param func 输出函数指针 (NULL=恢复默认 printf)
     */
    void (*SetOutputFunc)(bg_log_output_func_t func);
    
    /**
     * 设置全局日志级别
     * @param level 日志级别
     */
    void (*SetLevel)(bg_log_level_t level);
    
    /**
     * 设置模块调试开关
     * @param tag 模块标签
     * @param enable 1=启用, 0=禁用
     */
    void (*SetModuleDebug)(bg_log_tag_t tag, uint8_t enable);
    
    /**
     * 输出日志
     * @param level 日志级别
     * @param tag 模块标签
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    void (*Print)(bg_log_level_t level, bg_log_tag_t tag, const char *format, ...);
    
} bg_log_t;

/* 导出日志接口实例 */
extern bg_log_t BG_Log;

/* 便捷宏定义 - 统一接口 */
#define BG_LOG_E(tag, fmt, ...) BG_Log.Print(BG_LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#define BG_LOG_W(tag, fmt, ...) BG_Log.Print(BG_LOG_LEVEL_WARN, tag, fmt, ##__VA_ARGS__)
#define BG_LOG_I(tag, fmt, ...) BG_Log.Print(BG_LOG_LEVEL_INFO, tag, fmt, ##__VA_ARGS__)

/* 调试宏 - 根据配置开关控制 */
#ifdef BG_DEBUG_ENABLED
    #define BG_LOG_D(tag, fmt, ...) BG_Log.Print(BG_LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#else
    #define BG_LOG_D(tag, fmt, ...)
#endif

#endif /* _BG_LOG_H__ */
