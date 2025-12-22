/**
 * flash_test.h - New Flash driver framework test header file
 */

#ifndef __FLASH_TEST_H__
#define __FLASH_TEST_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Complete test for Flash driver framework
 * Tests include:
 * - Flash manager initialization
 * - Device enumeration
 * - Single byte read/write
 * - Page read/write (256 bytes)
 * - Cross-page read/write (512 bytes)
 * - Two NOR Flash devices (CS=A21, CS=A22)
 */
void FlashNewDriver_Test(void);

/**
 * @brief Quick function test (for debugging)
 * Only tests basic read/write functions, outputs concise results
 */
void FlashNewDriver_QuickTest(void);

#endif /* __FLASH_TEST_H__ */
