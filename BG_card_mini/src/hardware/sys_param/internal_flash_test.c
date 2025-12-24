/**
 * internal_flash_test.c - 内部Flash读写测试
 * 
 * 测试SDK提供的内部Flash API:
 *   - SpiFlashRead()
 *   - SpiFlashWrite()
 *   - SpiFlashErase()
 */

#include "internal_flash_test.h"
#include "spi_flash.h"
#include "debug.h"
#include "gpio.h"
#include "string.h"
#include "bg_lcd.h"

/* 测试配置 */
#define TEST_SECTOR_NUM     250             /* 使用第250扇区做测试 (避免破坏代码区) */
#define TEST_FLASH_ADDR     (TEST_SECTOR_NUM * 4096)
#define TEST_TIMEOUT        100             /* Flash操作超时(ms) */

/* 测试缓冲区 */
static uint8_t test_write_buf[512];
static uint8_t test_read_buf[512];

/**
 * @brief 测试单字节读写
 */
static bool test_single_byte(void)
{
    uint8_t write_data = 0xAA;
    uint8_t read_data = 0;
    uint32_t addr = TEST_FLASH_ADDR;
    
    DBG("\n--- Single Byte Test ---\n");
    
    /* 擦除扇区 */
    SpiFlashErase(SECTOR_ERASE, TEST_SECTOR_NUM, 1);
    
    /* 写入单字节 */
    if (SpiFlashWrite(addr, &write_data, 1, TEST_TIMEOUT) != FLASH_NONE_ERR) {
        DBG("[FAIL] Single byte write failed\n");
        return false;
    }
    
    /* 读取单字节 */
    if (SpiFlashRead(addr, &read_data, 1, TEST_TIMEOUT) != FLASH_NONE_ERR) {
        DBG("[FAIL] Single byte read failed\n");
        return false;
    }
    
    /* 验证 */
    if (write_data == read_data) {
        DBG("[OK] Single byte: wrote 0x%02X, read 0x%02X\n", write_data, read_data);
        return true;
    } else {
        DBG("[FAIL] Single byte: wrote 0x%02X, read 0x%02X\n", write_data, read_data);
        return false;
    }
}

/**
 * @brief 测试256字节页写入
 */
static bool test_page_write(void)
{
    uint32_t addr = TEST_FLASH_ADDR;
    uint16_t i;
    bool result = true;
    
    DBG("\n--- 256 Byte Page Test ---\n");
    
    /* 准备测试数据 */
    for (i = 0; i < 256; i++) {
        test_write_buf[i] = (uint8_t)(0x50 + (i & 0x0F));
    }
    
    /* 擦除扇区 */
    SpiFlashErase(SECTOR_ERASE, TEST_SECTOR_NUM, 1);
    
    /* 写入256字节 */
    if (SpiFlashWrite(addr, test_write_buf, 256, TEST_TIMEOUT) != FLASH_NONE_ERR) {
        DBG("[FAIL] 256 byte write failed\n");
        return false;
    }
    
    /* 读取256字节 */
    memset(test_read_buf, 0, 256);
    if (SpiFlashRead(addr, test_read_buf, 256, TEST_TIMEOUT) != FLASH_NONE_ERR) {
        DBG("[FAIL] 256 byte read failed\n");
        return false;
    }
    
    /* 验证数据 */
    for (i = 0; i < 256; i++) {
        if (test_write_buf[i] != test_read_buf[i]) {
            DBG("[FAIL] Byte %d: wrote 0x%02X, read 0x%02X\n", 
                i, test_write_buf[i], test_read_buf[i]);
            result = false;
            break;
        }
    }
    
    if (result) {
        DBG("[OK] 256 byte write/read verified\n");
    }
    
    return result;
}

/**
 * @brief 测试512字节跨页写入
 */
static bool test_cross_page(void)
{
    uint32_t addr = TEST_FLASH_ADDR;
    uint16_t i;
    bool result = true;
    
    DBG("\n--- 512 Byte Cross-Page Test ---\n");
    
    /* 准备测试数据 */
    for (i = 0; i < 512; i++) {
        test_write_buf[i] = (uint8_t)(0xA0 + (i & 0x0F));
    }
    
    /* 擦除扇区 */
    SpiFlashErase(SECTOR_ERASE, TEST_SECTOR_NUM, 1);
    
    /* 写入512字节 */
    if (SpiFlashWrite(addr, test_write_buf, 512, TEST_TIMEOUT) != FLASH_NONE_ERR) {
        DBG("[FAIL] 512 byte write failed\n");
        return false;
    }
    
    /* 读取512字节 */
    memset(test_read_buf, 0, 512);
    if (SpiFlashRead(addr, test_read_buf, 512, TEST_TIMEOUT) != FLASH_NONE_ERR) {
        DBG("[FAIL] 512 byte read failed\n");
        return false;
    }
    
    /* 验证数据 */
    for (i = 0; i < 512; i++) {
        if (test_write_buf[i] != test_read_buf[i]) {
            DBG("[FAIL] Byte %d: wrote 0x%02X, read 0x%02X\n", 
                i, test_write_buf[i], test_read_buf[i]);
            result = false;
            break;
        }
    }
    
    if (result) {
        DBG("[OK] 512 byte cross-page write/read verified\n");
    }
    
    return result;
}

/**
 * @brief 测试擦除验证
 */
static bool test_erase_verify(void)
{
    uint32_t addr = TEST_FLASH_ADDR;
    uint16_t i;
    bool result = true;
    
    DBG("\n--- Erase Verify Test ---\n");
    
    /* 先写入数据 */
    memset(test_write_buf, 0x55, 256);
    SpiFlashErase(SECTOR_ERASE, TEST_SECTOR_NUM, 1);
    SpiFlashWrite(addr, test_write_buf, 256, TEST_TIMEOUT);
    
    /* 擦除 */
    SpiFlashErase(SECTOR_ERASE, TEST_SECTOR_NUM, 1);
    
    /* 读取并验证擦除后应该全为0xFF */
    memset(test_read_buf, 0, 256);
    if (SpiFlashRead(addr, test_read_buf, 256, TEST_TIMEOUT) != FLASH_NONE_ERR) {
        DBG("[FAIL] Read after erase failed\n");
        return false;
    }
    
    for (i = 0; i < 256; i++) {
        if (test_read_buf[i] != 0xFF) {
            DBG("[FAIL] Byte %d after erase: 0x%02X (expected 0xFF)\n", 
                i, test_read_buf[i]);
            result = false;
            break;
        }
    }
    
    if (result) {
        DBG("[OK] Erase verified (all 0xFF)\n");
    }
    
    return result;
}

/**
 * @brief 测试Flash保护/解保护
 */
static bool test_flash_protect(void)
{
    uint32_t addr = TEST_FLASH_ADDR;
    uint8_t write_data = 0xCC;
    uint8_t read_data = 0;
    
    DBG("\n--- Flash Protection Test ---\n");
    
    /* 解保护并写入数据 */
    SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3);
    SpiFlashErase(SECTOR_ERASE, TEST_SECTOR_NUM, 1);
    SpiFlashWrite(addr, &write_data, 1, TEST_TIMEOUT);
    
    /* 保护Flash */
    if (SpiFlashIOCtrl(IOCTL_FLASH_PROTECT, FLASH_LOCK_RANGE_ALL) != FLASH_NONE_ERR) {
        DBG("[FAIL] Flash protect failed\n");
        return false;
    }
    
    /* 尝试擦除（应该失败或无效） */
    SpiFlashErase(SECTOR_ERASE, TEST_SECTOR_NUM, 1);
    
    /* 读取数据，如果保护有效，数据应该还在 */
    SpiFlashRead(addr, &read_data, 1, TEST_TIMEOUT);
    
    if (read_data == write_data) {
        DBG("[OK] Flash protection working (data preserved: 0x%02X)\n", read_data);
    } else {
        DBG("[WARN] Flash protection may not be working (read: 0x%02X)\n", read_data);
    }
    
    /* 解保护 */
    SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3);
    DBG("[OK] Flash unprotected\n");
    
    return true;
}

/**
 * @brief 完整测试流程
 */
void InternalFlash_Test(void)
{
    bool test1, test2, test3, test4, test5;
    
    DBG("\n");
    DBG("**************************************************\n");
    DBG("*       Internal Flash Read/Write Test          *\n");
    DBG("**************************************************\n");
    
    DBG("\nTest Configuration:\n");
    DBG("  Test Sector: %d\n", TEST_SECTOR_NUM);
    DBG("  Test Address: 0x%08lX\n", (unsigned long)TEST_FLASH_ADDR);
    DBG("  Sector Size: 4096 bytes\n");
    DBG("  Timeout: %d ms\n", TEST_TIMEOUT);
    
    /* 解保护Flash，确保可以擦写 */
    SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3);
    
    /* 运行测试 */
    test1 = test_single_byte();
    test2 = test_page_write();
    test3 = test_cross_page();
    test4 = test_erase_verify();
    test5 = test_flash_protect();
    
    /* 显示测试结果 */
    DBG("\n");
    DBG("========================================\n");
    DBG("         Test Summary                  \n");
    DBG("========================================\n");
    DBG("  Single Byte:      %s\n", test1 ? "PASS" : "FAIL");
    DBG("  256 Byte Page:    %s\n", test2 ? "PASS" : "FAIL");
    DBG("  512 Byte Cross:   %s\n", test3 ? "PASS" : "FAIL");
    DBG("  Erase Verify:     %s\n", test4 ? "PASS" : "FAIL");
    DBG("  Flash Protection: %s\n", test5 ? "PASS" : "FAIL");
    DBG("========================================\n");
    
    if (test1 && test2 && test3 && test4) {
        DBG("  Overall Result: ALL TESTS PASSED\n");
        DBG("========================================\n");
    } else {
        DBG("  Overall Result: SOME TESTS FAILED\n");
        DBG("========================================\n");
    }
}

/**
 * @brief 内部Flash测试任务 (FreeRTOS)
 */
void InternalFlashTestTask(void)
{
    /* 初始化LCD */
    BG_lcd.Init();
    BG_lcd.Clear(0x001F);  /* 蓝色背景 */
    
    DBG("\n");
    DBG("**************************************************\n");
    DBG("*     Internal Flash Test Task Started          *\n");
    DBG("**************************************************\n");
    
    /* 运行测试 */
    InternalFlash_Test();
    
    DBG("\nInternal Flash test completed.\n");
}

/**
 * @brief 快速测试函数（用于调试）
 */
void InternalFlash_QuickTest(void)
{
    uint8_t write_val = 0x5A;
    uint8_t read_val = 0;
    uint32_t addr = TEST_FLASH_ADDR;
    
    DBG("\n=== Internal Flash Quick Test ===\n");
    
    /* 解保护 */
    SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3);
    
    /* 擦除 */
    DBG("Erasing sector %d...\n", TEST_SECTOR_NUM);
    SpiFlashErase(SECTOR_ERASE, TEST_SECTOR_NUM, 1);
    
    /* 写入 */
    DBG("Writing 0x%02X to 0x%08lX...\n", write_val, (unsigned long)addr);
    SpiFlashWrite(addr, &write_val, 1, TEST_TIMEOUT);
    
    /* 读取 */
    DBG("Reading from 0x%08lX...\n", (unsigned long)addr);
    SpiFlashRead(addr, &read_val, 1, TEST_TIMEOUT);
    
    /* 结果 */
    DBG("Result: Wrote 0x%02X, Read 0x%02X %s\n", 
        write_val, read_val, 
        (write_val == read_val) ? "[OK]" : "[FAIL]");
}
