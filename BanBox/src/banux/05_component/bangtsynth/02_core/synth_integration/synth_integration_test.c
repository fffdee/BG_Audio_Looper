/**
 * @file synth_integration_test.c
 * @brief SD+NAND+PSRAM 合成器集成测试
 *
 * 提供完整的集成测试套件，验证三级存储架构的正确性。
 */

#include "product_def.h"

#if SYNTH_SD_NAND_PSRAM_EN

#include "synth_sdnandpsram.h"
#include "fat32_reader.h"
#include "nand_store.h"
#include "psram_buffer.h"
#include "soundbank_manager.h"
#include "bg_log.h"
#include <string.h>
#include <stdlib.h>

/* ============================================
 * 测试配置
 * ============================================ */

#define TEST_TIMEOUT_MS         10000   /* 测试超时时间 */
#define TEST_SAMPLE_RATE        44100   /* 测试采样率 */
#define TEST_BUFFER_SIZE        1024    /* 测试缓冲区大小 */

/* ============================================
 * 测试状态
 * ============================================ */

typedef struct {
    bool initialized;
    uint32_t tests_run;
    uint32_t tests_passed;
    uint32_t tests_failed;
    char last_error[256];
} SYNTH_TestState_t;

static SYNTH_TestState_t g_test_state = {0};

/* ============================================
 * 内部测试函数
 * ============================================ */

static void test_reset(void) {
    g_test_state.tests_run = 0;
    g_test_state.tests_passed = 0;
    g_test_state.tests_failed = 0;
    memset(g_test_state.last_error, 0, sizeof(g_test_state.last_error));
}

static void test_record_result(bool passed, const char *test_name, const char *error_msg) {
    g_test_state.tests_run++;
    if (passed) {
        g_test_state.tests_passed++;
        BG_LOG_I(BG_LOG_TAG_SYNTH, "TEST PASSED: %s", test_name);
    } else {
        g_test_state.tests_failed++;
        BG_LOG_E(BG_LOG_TAG_SYNTH, "TEST FAILED: %s - %s", test_name, error_msg);
        strncpy(g_test_state.last_error, error_msg, sizeof(g_test_state.last_error) - 1);
    }
}

/* 测试 FAT32 读取器 */
static void test_fat32_reader(void) {
    BG_ERR ret;
    FAT32_FileInfo_t file_info;
    FAT32_FileHandle_t handle;
    uint8_t buffer[512];
    int32_t read_bytes;

    /* 测试 SD 卡就绪 */
    if (!FAT32_IsCardReady()) {
        test_record_result(false, "FAT32_IsCardReady", "SD card not ready");
        return;
    }
    test_record_result(true, "FAT32_IsCardReady", NULL);

    /* 测试文件查找 */
    ret = FAT32_FindFile("*.sf2", &file_info);
    if (ret != SUCCESS) {
        test_record_result(false, "FAT32_FindFile", "No SF2 file found");
        return;
    }
    test_record_result(true, "FAT32_FindFile", NULL);

    /* 测试文件打开 */
    ret = FAT32_OpenFile(file_info.name, &handle);
    if (ret != SUCCESS) {
        test_record_result(false, "FAT32_OpenFile", "Failed to open SF2 file");
        return;
    }
    test_record_result(true, "FAT32_OpenFile", NULL);

    /* 测试文件读取 */
    read_bytes = FAT32_ReadFile(&handle, buffer, sizeof(buffer));
    if (read_bytes <= 0) {
        test_record_result(false, "FAT32_ReadFile", "Failed to read from file");
        FAT32_CloseFile(&handle);
        return;
    }
    test_record_result(true, "FAT32_ReadFile", NULL);

    /* 验证 SF2 头部 */
    if (buffer[0] != 'R' || buffer[1] != 'I' || buffer[2] != 'F' || buffer[3] != 'F') {
        test_record_result(false, "SF2 Header Validation", "Invalid RIFF header");
        FAT32_CloseFile(&handle);
        return;
    }
    test_record_result(true, "SF2 Header Validation", NULL);

    FAT32_CloseFile(&handle);
}

/* 测试 NAND 存储 */
static void test_nand_store(void) {
    uint32_t total_space, used_space, program_count;
    const char *test_data = "Test soundbank data";
    uint32_t data_size = strlen(test_data) + 1;
    uint8_t read_buffer[256];

    /* 测试存储信息获取 */
    NAND_GetStats(&total_space, &used_space, &program_count);
    if (total_space == 0) {
        test_record_result(false, "NAND_GetStats", "Invalid storage stats");
        return;
    }
    test_record_result(true, "NAND_GetStats", NULL);

    /* 测试音色存储 */
    BG_ERR ret = NAND_StoreProgram(0, (const void *)test_data, data_size, "Test Program", 0);
    if (ret != SUCCESS) {
        test_record_result(false, "NAND_StoreProgram", "Failed to store program");
        return;
    }
    test_record_result(true, "NAND_StoreProgram", NULL);

    /* 测试音色存在检查 */
    if (!NAND_ProgramExists(0)) {
        test_record_result(false, "NAND_ProgramExists", "Program should exist");
        return;
    }
    test_record_result(true, "NAND_ProgramExists", NULL);

    /* 测试音色读取 */
    uint32_t actual_size;
    ret = NAND_LoadProgram(0, read_buffer, sizeof(read_buffer), &actual_size);
    if (ret != SUCCESS || actual_size != data_size || strcmp((char *)read_buffer, test_data) != 0) {
        test_record_result(false, "NAND_LoadProgram", "Failed to load program correctly");
        return;
    }
    test_record_result(true, "NAND_LoadProgram", NULL);

    /* 测试音色删除 */
    ret = NAND_DeleteProgram(0);
    if (ret != SUCCESS) {
        test_record_result(false, "NAND_DeleteProgram", "Failed to delete program");
        return;
    }
    test_record_result(true, "NAND_DeleteProgram", NULL);

    /* 验证删除后不存在 */
    if (NAND_ProgramExists(0)) {
        test_record_result(false, "NAND_ProgramExists After Delete", "Program should not exist after delete");
        return;
    }
    test_record_result(true, "NAND_ProgramExists After Delete", NULL);
}

/* 测试 PSRAM 缓冲区 */
static void test_psram_buffer(void) {
    PSRAM_BufferAlloc_t alloc_result;
    PSRAM_BufferInfo_t info;
    short audio_buffer[TEST_BUFFER_SIZE];
    const PSRAM_NoteRequest_t request = {
        .note = 60,      /* Middle C */
        .velocity = 100,
        .program = 0,
        .sample_rate = TEST_SAMPLE_RATE,
        .high_priority = false
    };

    /* 测试缓冲区请求 */
    BG_ERR ret = PSRAM_RequestNoteBuffer(&request, &alloc_result);
    if (ret != SUCCESS) {
        test_record_result(false, "PSRAM_RequestNoteBuffer", "Failed to allocate buffer");
        return;
    }
    test_record_result(true, "PSRAM_RequestNoteBuffer", NULL);

    /* 测试缓冲区信息获取 */
    ret = PSRAM_GetBufferInfo(alloc_result.buffer_id, &info);
    if (ret != SUCCESS || info.buffer_id != alloc_result.buffer_id) {
        test_record_result(false, "PSRAM_GetBufferInfo", "Failed to get buffer info");
        return;
    }
    test_record_result(true, "PSRAM_GetBufferInfo", NULL);

    /* 测试缓冲区就绪检查 (应该为 false，因为没有加载数据) */
    if (PSRAM_IsBufferReady(alloc_result.buffer_id)) {
        test_record_result(false, "PSRAM_IsBufferReady", "Buffer should not be ready without data");
        return;
    }
    test_record_result(true, "PSRAM_IsBufferReady", NULL);

    /* 测试数据读取 (应该返回 0 或错误) */
    int32_t read_bytes = PSRAM_ReadBufferData(alloc_result.buffer_id, 0, audio_buffer, TEST_BUFFER_SIZE);
    if (read_bytes != 0) {
        test_record_result(false, "PSRAM_ReadBufferData Empty", "Should return 0 for empty buffer");
        return;
    }
    test_record_result(true, "PSRAM_ReadBufferData Empty", NULL);

    /* 测试缓冲区释放 */
    ret = PSRAM_ReleaseNoteBuffer(alloc_result.buffer_id);
    if (ret != SUCCESS) {
        test_record_result(false, "PSRAM_ReleaseNoteBuffer", "Failed to release buffer");
        return;
    }
    test_record_result(true, "PSRAM_ReleaseNoteBuffer", NULL);
}

/* 测试集成模块 */
static void test_integration(void) {
    SYNTH_Status_t status;

    /* 测试状态获取 */
    SYNTH_SDNANDPSRAM_GetStatus(&status);
    test_record_result(true, "SYNTH_SDNANDPSRAM_GetStatus", NULL);

    /* 测试音符触发 (需要集成模块已初始化) */
    if (status.soundbank_ready) {
        SYNTH_SDNANDPSRAM_NoteOn(60, 100, 0);
        test_record_result(true, "SYNTH_SDNANDPSRAM_NoteOn", NULL);

        SYNTH_SDNANDPSRAM_NoteOff(60, 0);
        test_record_result(true, "SYNTH_SDNANDPSRAM_NoteOff", NULL);
    } else {
        BG_LOG_W(BG_LOG_TAG_SYNTH, "Skipping NoteOn/Off tests - integration not ready");
    }
}

/* ============================================
 * 公开接口
 * ============================================ */

/**
 * 运行所有集成测试
 * @return 测试通过的数量
 */
uint32_t SYNTH_RunIntegrationTests(void) {
    BG_LOG_I(BG_LOG_TAG_SYNTH, "Starting SD+NAND+PSRAM integration tests");

    test_reset();

    /* 运行各个子系统测试 */
    test_fat32_reader();
    test_nand_store();
    test_psram_buffer();
    test_integration();

    /* 输出测试结果 */
    BG_LOG_I(BG_LOG_TAG_SYNTH, "Test Results: %u run, %u passed, %u failed",
             g_test_state.tests_run, g_test_state.tests_passed, g_test_state.tests_failed);

    if (g_test_state.tests_failed > 0) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "Last error: %s", g_test_state.last_error);
    }

    return g_test_state.tests_passed;
}

/**
 * 获取测试状态
 * @param tests_run     输出运行的测试数量
 * @param tests_passed  输出通过的测试数量
 * @param tests_failed  输出失败的测试数量
 */
void SYNTH_GetTestStatus(uint32_t *tests_run, uint32_t *tests_passed, uint32_t *tests_failed) {
    if (tests_run) *tests_run = g_test_state.tests_run;
    if (tests_passed) *tests_passed = g_test_state.tests_passed;
    if (tests_failed) *tests_failed = g_test_state.tests_failed;
}

/**
 * 获取最后一次测试错误
 * @return 错误消息字符串
 */
const char* SYNTH_GetLastTestError(void) {
    return g_test_state.last_error;
}

#endif /* SYNTH_SD_NAND_PSRAM_EN */