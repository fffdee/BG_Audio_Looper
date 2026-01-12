/**
 * flash_test.h - 新 Flash 驱动架构测试头文件
 */

#ifndef __FLASH_TEST_H__
#define __FLASH_TEST_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Flash 驱动架构完整测试
 * 测试包括：
 * - Flash 管理器初始化
 * - 设备枚举
 * - 单字节读写
 * - 页读写（256字节）
 * - 跨页读写（512字节）
 * - 两个 NOR Flash 设备（CS=A21, CS=A22）
 */
void FlashNewDriver_Test(void);

/**
 * @brief 快速功能测试（用于调试）
 * 仅测试基本的读写功能，输出简洁
 */
void FlashNewDriver_QuickTest(void);

#endif /* __FLASH_TEST_H__ */
