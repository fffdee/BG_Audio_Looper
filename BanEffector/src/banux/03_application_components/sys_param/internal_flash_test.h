/**
 * internal_flash_test.h - 内部Flash读写测试
 */

#ifndef __INTERNAL_FLASH_TEST_H__
#define __INTERNAL_FLASH_TEST_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 完整的内部Flash测试
 *        测试单字节、页写入、跨页写入、擦除验证、保护功能
 */
void InternalFlash_Test(void);

/**
 * @brief 内部Flash测试任务 (FreeRTOS任务函数)
 *        在main()中创建任务调用此函数
 */
void InternalFlashTestTask(void);

/**
 * @brief 快速测试函数（用于调试）
 *        简单的擦写读测试
 */
void InternalFlash_QuickTest(void);

#ifdef __cplusplus
}
#endif

#endif /* __INTERNAL_FLASH_TEST_H__ */
