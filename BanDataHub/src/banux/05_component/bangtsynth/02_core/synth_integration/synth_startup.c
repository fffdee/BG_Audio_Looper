/**
 * @file synth_startup.c
 * @brief SD+NAND+PSRAM 合成器启动集成
 *
 * 在系统启动时集成存储合成器架构。
 * BanDataHub: SD→PSRAM 二级直读 (无 NAND)
 * 标准平台: SD→NAND→PSRAM 三级存储
 */

#include "product_def.h"

#if SYNTH_SD_NAND_PSRAM_EN

#include "synth_sdnandpsram.h"
#include "bg_log.h"
#include "bg_osal.h"
#include <stdint.h>

/* ============================================
 * 启动配置
 * ============================================ */

/** 启动超时时间 (毫秒) */
#define SYNTH_STARTUP_TIMEOUT_MS    30000

/** 是否启用详细启动日志 */
#define SYNTH_STARTUP_VERBOSE       1

/* ============================================
 * 启动状态跟踪
 * ============================================ */

typedef struct {
    uint32_t start_time;
    bool fat32_init_done;
    bool nand_init_done;
    bool sf2_copy_done;
    bool psram_init_done;
    bool storage_driver_done;
    bool soundbank_init_done;
    bool tests_run;
    uint32_t tests_passed;
    char error_message[256];
} SYNTH_StartupState_t;

static SYNTH_StartupState_t g_startup_state = {0};

/* ============================================
 * 内部函数
 * ============================================ */

#if SYNTH_STARTUP_VERBOSE
#define STARTUP_LOG_I(msg, ...) BG_LOG_I(BG_LOG_TAG_SYNTH, "[STARTUP] " msg, ##__VA_ARGS__)
#define STARTUP_LOG_W(msg, ...) BG_LOG_W(BG_LOG_TAG_SYNTH, "[STARTUP] " msg, ##__VA_ARGS__)
#define STARTUP_LOG_E(msg, ...) BG_LOG_E(BG_LOG_TAG_SYNTH, "[STARTUP] " msg, ##__VA_ARGS__)
#else
#define STARTUP_LOG_I(msg, ...)
#define STARTUP_LOG_W(msg, ...)
#define STARTUP_LOG_E(msg, ...)
#endif

/**
 * 记录启动错误
 */
static void startup_set_error(const char *error_msg) {
    strncpy(g_startup_state.error_message, error_msg, sizeof(g_startup_state.error_message) - 1);
    STARTUP_LOG_E("Error: %s", error_msg);
}

/**
 * 检查启动超时
 */
static bool startup_check_timeout(void) {
    /* 使用 bg_osal tick 检查超时 */
    uint32_t current_time = bg_get_tick_ms();
    uint32_t elapsed_ms = current_time - g_startup_state.start_time;

    if (elapsed_ms > SYNTH_STARTUP_TIMEOUT_MS) {
        STARTUP_LOG_E("Startup timeout: %u ms elapsed", elapsed_ms);
        return true;
    }

    return false;
}

/* ============================================
 * 启动步骤实现
 * ============================================ */

/**
 * 步骤 1: 初始化 FAT32 读取器
 */
static bool startup_step_fat32_init(void) {
    STARTUP_LOG_I("Step 1: Initializing FAT32 reader");

    /* FAT32 初始化已在 SYNTH_SDNANDPSRAM_Init 中处理 */
    g_startup_state.fat32_init_done = true;
    STARTUP_LOG_I("FAT32 reader initialized");
    return true;
}

/**
 * 步骤 2: 初始化存储 (NAND 或 PSRAM 直读)
 */
static bool startup_step_nand_init(void) {
#ifdef BANDATAHUB
    STARTUP_LOG_I("Step 2: Loading SF2 to PSRAM (BanDataHub SD-direct)");
#else
    STARTUP_LOG_I("Step 2: Initializing NAND store");
#endif

    /* 存储初始化已在 SYNTH_SDNANDPSRAM_Init 中处理 */
    g_startup_state.nand_init_done = true;
    STARTUP_LOG_I("Storage initialized");
    return true;
}

/**
 * 步骤 3: SF2 文件加载
 */
static bool startup_step_sf2_copy(void) {
#ifdef BANDATAHUB
    STARTUP_LOG_I("Step 3: SF2 loaded to PSRAM (BanDataHub direct)");
#else
    STARTUP_LOG_I("Step 3: Copying SF2 to NAND");
#endif

    /* SF2 加载已在 SYNTH_SDNANDPSRAM_Init 中处理 */
    g_startup_state.sf2_copy_done = true;
    STARTUP_LOG_I("SF2 load completed");
    return true;
}

/**
 * 步骤 4: 初始化 PSRAM 缓冲区
 */
static bool startup_step_psram_init(void) {
    STARTUP_LOG_I("Step 4: Initializing PSRAM buffer");

    /* PSRAM 初始化已在 SYNTH_SDNANDPSRAM_Init 中处理 */
    g_startup_state.psram_init_done = true;
    STARTUP_LOG_I("PSRAM buffer initialized");
    return true;
}

/**
 * 步骤 5: 安装存储驱动
 */
static bool startup_step_storage_driver(void) {
#ifdef BANDATAHUB
    STARTUP_LOG_I("Step 5: Installing PSRAM storage driver (BanDataHub)");
#else
    STARTUP_LOG_I("Step 5: Installing NAND storage driver");
#endif

    /* 存储驱动安装已在 SYNTH_SDNANDPSRAM_Init 中处理 */
    g_startup_state.storage_driver_done = true;
    STARTUP_LOG_I("Storage driver installed");
    return true;
}

/**
 * 步骤 6: 初始化音源管理器
 */
static bool startup_step_soundbank_init(void) {
    STARTUP_LOG_I("Step 6: Initializing soundbank manager");

    /* 音源管理器初始化已在 SYNTH_SDNANDPSRAM_Init 中处理 */
    g_startup_state.soundbank_init_done = true;
    STARTUP_LOG_I("Soundbank manager initialized");
    return true;
}

/**
 * 步骤 7: 运行集成测试
 */
static bool startup_step_run_tests(void) {
    STARTUP_LOG_I("Step 7: Running integration tests");

    /* 运行测试 */
    g_startup_state.tests_passed = SYNTH_RunIntegrationTests();

    if (g_startup_state.tests_passed == 0) {
        startup_set_error("All integration tests failed");
        return false;
    }

    g_startup_state.tests_run = true;
    STARTUP_LOG_I("Integration tests completed: %u passed", g_startup_state.tests_passed);
    return true;
}

/* ============================================
 * 实际初始化集成
 * ============================================ */

/**
 * 在启动序列中调用的实际初始化函数
 * @return SUCCESS 或错误码
 */
static BG_ERR startup_init_all(void) {
    BG_ERR ret;
    
    STARTUP_LOG_I("Calling SYNTH_SDNANDPSRAM_Init to initialize all modules");
    ret = SYNTH_SDNANDPSRAM_Init();
    if (ret != SUCCESS) {
        STARTUP_LOG_E("SYNTH_SDNANDPSRAM_Init failed: %d", ret);
        return ret;
    }
    
    STARTUP_LOG_I("All modules initialized successfully");
    return SUCCESS;
}

/* ============================================
 * 公开接口
 * ============================================ */

/**
 * 执行完整的合成器启动序列
 *
 * 按顺序执行所有启动步骤，任何步骤失败都会导致启动失败。
 *
 * @return true=启动成功, false=启动失败
 */
bool SYNTH_StartupSequence(void) {
    bool success = true;

#ifdef BANDATAHUB
    STARTUP_LOG_I("=== Starting SD+PSRAM Synthesizer (BanDataHub) ===");
#else
    STARTUP_LOG_I("=== Starting SD+NAND+PSRAM Synthesizer ===");
#endif

    /* 重置启动状态 */
    memset(&g_startup_state, 0, sizeof(g_startup_state));
    g_startup_state.start_time = bg_get_tick_ms();

    /* 执行所有模块的真实初始化 */
    if (startup_init_all() != SUCCESS) {
        startup_set_error("Real initialization failed in startup_init_all");
        STARTUP_LOG_E("=== Synthesizer real init failed ===");
        return false;
    }

    /* 执行启动步骤 */
    do {
        /* 检查超时 */
        if (startup_check_timeout()) {
            startup_set_error("Startup timeout");
            success = false;
            break;
        }

        /* 步骤 1: FAT32 初始化 */
        if (!startup_step_fat32_init()) {
            success = false;
            break;
        }

        /* 步骤 2: NAND 初始化 */
        if (!startup_step_nand_init()) {
            success = false;
            break;
        }

        /* 步骤 3: SF2 拷贝 */
        if (!startup_step_sf2_copy()) {
            success = false;
            break;
        }

        /* 步骤 4: PSRAM 初始化 */
        if (!startup_step_psram_init()) {
            success = false;
            break;
        }

        /* 步骤 5: 存储驱动安装 */
        if (!startup_step_storage_driver()) {
            success = false;
            break;
        }

        /* 步骤 6: 音源管理器初始化 */
        if (!startup_step_soundbank_init()) {
            success = false;
            break;
        }

        /* 步骤 7: 运行测试 */
        if (!startup_step_run_tests()) {
            success = false;
            break;
        }

    } while (0);

    /* 输出启动结果 */
    if (success) {
        STARTUP_LOG_I("=== Synthesizer startup completed successfully ===");
    } else {
        STARTUP_LOG_E("=== Synthesizer startup failed: %s ===", g_startup_state.error_message);
    }

    return success;
}

/**
 * 获取启动状态信息
 * @param state 输出启动状态
 */
void SYNTH_GetStartupStatus(SYNTH_StartupState_t *state) {
    if (state) {
        memcpy(state, &g_startup_state, sizeof(SYNTH_StartupState_t));
    }
}

/**
 * 获取启动错误消息
 * @return 错误消息字符串，NULL=无错误
 */
const char* SYNTH_GetStartupError(void) {
    return g_startup_state.error_message[0] ? g_startup_state.error_message : NULL;
}

/**
 * 检查启动是否完成
 * @return true=已完成, false=未完成或失败
 */
bool SYNTH_IsStartupComplete(void) {
    return g_startup_state.soundbank_init_done && g_startup_state.tests_run;
}

#endif /* SYNTH_SD_NAND_PSRAM_EN */