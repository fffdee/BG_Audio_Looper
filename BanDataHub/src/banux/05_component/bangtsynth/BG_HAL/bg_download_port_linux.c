#include "bangtsynth_legacy.h"
#if BANGTSYNTH_LEGACY

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "bg_config.h"
#if (BG_TARGET_PLATFORM == BG_PLATFORM_LINUX)

/**
 * BG_Download_Port - Linux 平台实现
 * 
 * 功能:
 * - 模拟MCU从串口接收音源数据的过程
 * - 在Linux上实现为从文件读取数据
 * 
 * 使用场景:
 * - 开发调试: 从本地SF2/BGS文件读取音源
 * - 测试验证: 模拟串口下载流程
 * 
 * MCU移植:
 * - 将 fopen/fread 替换为 UART_Receive() 等串口接收函数
 * - 保持接口不变,只需替换此文件
 */

#include "bg_download_port.h"
#include "bg_log.h"
#include <stdio.h>
#include <string.h>

/* 内部状态 */
static FILE *g_download_file = NULL;
static char g_current_source[256] = {0};

/**
 * 从文件读取数据 (模拟串口接收)
 */
int bg_download_port_read(const char *source, void *buffer, size_t size, size_t *bytes_read)
{
    if (!source || !buffer || !bytes_read) {
        BG_LOG_E(BG_LOG_TAG_HAL, "Invalid parameters\n");
        return -1;
    }
    
    /* 检查是否需要打开新文件 */
    if (!g_download_file || strcmp(source, g_current_source) != 0) {
        /* 关闭旧文件 */
        if (g_download_file) {
            fclose(g_download_file);
            g_download_file = NULL;
        }
        
        /* 打开新文件 */
        BG_LOG_I(BG_LOG_TAG_HAL, "Opening download source: %s\n", source);
        g_download_file = fopen(source, "rb");
        if (!g_download_file) {
            BG_LOG_E(BG_LOG_TAG_HAL, "Failed to open file: %s\n", source);
            return -1;
        }
        
        strncpy(g_current_source, source, sizeof(g_current_source) - 1);
        g_current_source[sizeof(g_current_source) - 1] = '\0';
    }
    
    /* 读取数据 */
    *bytes_read = fread(buffer, 1, size, g_download_file);
    
    /* 检查是否读取完毕 */
    if (*bytes_read == 0) {
        if (feof(g_download_file)) {
            BG_LOG_I(BG_LOG_TAG_HAL, "Download completed (EOF reached)\n");
            fclose(g_download_file);
            g_download_file = NULL;
            g_current_source[0] = '\0';
        } else if (ferror(g_download_file)) {
            BG_LOG_E(BG_LOG_TAG_HAL, "File read error\n");
            fclose(g_download_file);
            g_download_file = NULL;
            g_current_source[0] = '\0';
            return -1;
        }
    }
    
    return 0;
}

/*
 * MCU移植示例 (STM32):
 * 
 * #include "usart.h"  // STM32 HAL库
 * 
 * static uint8_t g_uart_num = 0;
 * 
 * int bg_download_port_read(const char *source, void *buffer, size_t size, size_t *bytes_read)
 * {
 *     // source = "UART1" / "UART2" 等
 *     
 *     // 解析串口号
 *     if (strcmp(source, "UART1") == 0) {
 *         g_uart_num = 1;
 *     } else if (strcmp(source, "UART2") == 0) {
 *         g_uart_num = 2;
 *     } else {
 *         return -1;
 *     }
 *     
 *     // 从串口接收数据 (阻塞/超时模式)
 *     HAL_StatusTypeDef ret = HAL_UART_Receive(&huart1, buffer, size, 1000);
 *     if (ret == HAL_OK) {
 *         *bytes_read = size;
 *         return 0;
 *     } else if (ret == HAL_TIMEOUT) {
 *         *bytes_read = 0;  // 超时,返回0表示暂无数据
 *         return 0;
 *     } else {
 *         return -1;  // 错误
 *     }
 * }
 */

#endif /* BG_TARGET_PLATFORM == BG_PLATFORM_LINUX */

#endif /* BANGTSYNTH_EN */

#endif /* BANGTSYNTH_LEGACY */
