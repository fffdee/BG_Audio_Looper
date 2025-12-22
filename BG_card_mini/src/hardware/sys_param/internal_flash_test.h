/**
 * internal_flash_test.h - Internal Flash read/write test
 */

#ifndef __INTERNAL_FLASH_TEST_H__
#define __INTERNAL_FLASH_TEST_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Complete internal Flash test
 *        Tests single byte, page write, cross-page write, erase verification, and protection functions
 */
void InternalFlash_Test(void);

/**
 * @brief Internal Flash test task (FreeRTOS task function)
 *        Call this function when creating a task in main()
 */
void InternalFlashTestTask(void);

/**
 * @brief Quick test function (for debugging)
 *        Simple erase/write/read test
 */
void InternalFlash_QuickTest(void);

#ifdef __cplusplus
}
#endif

#endif /* __INTERNAL_FLASH_TEST_H__ */
