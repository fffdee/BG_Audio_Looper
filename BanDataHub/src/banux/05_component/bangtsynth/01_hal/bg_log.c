#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* 模块调试开关表 */
static uint8_t g_module_debug[BG_LOG_TAG_MAX];

/* 全局日志级别 */
static bg_log_level_t g_log_level = BG_LOG_LEVEL_INFO;

/* 自定义输出函数 */
static bg_log_output_func_t g_output_func = NULL;

/* 模块标签名称表 */
static const char *g_tag_names[] = {
    "SYSTEM",
    "HAL",
    "MIXER",
    "MIDI",
    "AudioProc",
    "Soundbank",
    "Sequencer",
    "Envelope",
    "DRC",
    "EQ",
    "Play",
    "FAT32",
    "NAND",
    "PSRAM",
    "SYNTH"
};

/* 日志级别名称表 */
static const char *g_level_names[] = {
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG"
};

/* 内部函数声明 */
static void log_init(void);
static void log_set_output_func(bg_log_output_func_t func);
static void log_set_level(bg_log_level_t level);
static void log_set_module_debug(bg_log_tag_t tag, uint8_t enable);
static void log_print(bg_log_level_t level, bg_log_tag_t tag, const char *format, ...);

/* 日志接口实例 */
bg_log_t BG_Log __attribute__((section(".data"))) = {
    .Init = log_init,
    .SetOutputFunc = log_set_output_func,
    .SetLevel = log_set_level,
    .SetModuleDebug = log_set_module_debug,
    .Print = log_print
};

/**
 * 初始化日志系统
 */
static void log_init(void)
{
    /* 默认所有模块调试关闭 */
    memset(g_module_debug, 0, sizeof(g_module_debug));
    
    /* 默认日志级别 */
    g_log_level = BG_LOG_LEVEL_INFO;
    
    /* 默认输出函数 */
    g_output_func = NULL;
    
    printf("[Log] Initialized - Level: %s\n", g_level_names[g_log_level]);
}

/**
 * 设置日志输出函数
 */
static void log_set_output_func(bg_log_output_func_t func)
{
    g_output_func = func;
}

/**
 * 设置全局日志级别
 */
static void log_set_level(bg_log_level_t level)
{
    if (level <= BG_LOG_LEVEL_DEBUG) {
        g_log_level = level;
        printf("[Log] Level set to: %s\n", g_level_names[level]);
    }
}

/**
 * 设置模块调试开关
 */
static void log_set_module_debug(bg_log_tag_t tag, uint8_t enable)
{
    if (tag < BG_LOG_TAG_MAX) {
        g_module_debug[tag] = enable ? 1 : 0;
        printf("[Log] Module [%s] debug: %s\n", 
               g_tag_names[tag], enable ? "ON" : "OFF");
    }
}

/**
 * 输出日志
 */
static void log_print(bg_log_level_t level, bg_log_tag_t tag, const char *format, ...)
{
    char buffer[256];
    int len = 0;
    va_list args;

    /* 级别过滤 */
    if (level > g_log_level) {
        return;
    }
    
    /* DEBUG 级别检查模块开关 */
    if (level == BG_LOG_LEVEL_DEBUG) {
        if (tag >= BG_LOG_TAG_MAX || !g_module_debug[tag]) {
            return;
        }
    }
    
    /* 格式化消息 */
    
    /* 添加日志前缀: [LEVEL][TAG] */
    len = snprintf(buffer, sizeof(buffer), "[%s][%s] ", 
                   g_level_names[level], 
                   tag < BG_LOG_TAG_MAX ? g_tag_names[tag] : "UNKNOWN");
    
    /* 添加用户消息 */
    va_start(args, format);
    vsnprintf(buffer + len, sizeof(buffer) - len, format, args);
    va_end(args);
    
    /* 使用自定义输出函数或默认 printf */
    if (g_output_func) {
        g_output_func(level, tag, buffer);
    } else {
        printf("%s", buffer);
    }
}

#endif /* BANGTSYNTH_EN */
