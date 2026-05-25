/**
 **************************************************************************************
 * @file    audio_looper.c
 * @brief   Audio looper functions implementation
 *
 * @author  BanGO
 * @version V1.0.0
 *
 * @Copyright (C) 2025, Audio Looper Project. All rights reserved.
 ************************ *****************************************************/

#include "audio_looper.h"
#include "debug.h"
#include "flash_devices.h"    /* 直接使用底层Flash API */
#include "flash_nor_w25qxx.h" /* W25Qxx_EraseBlockStart (多Flash路径) */
#include "BG_FlashMgr.h"      /* 兼容性保留 */
#include "looper_storage.h"    /* 存储抽象层 */
#include "sys_param.h"    /* 系统参数持久化存储 */
#include "type.h"
#include <nds32_intrinsic.h>
#include <math.h>
#include <string.h>

/* 静态辅助函数前置声明 */
static void metronome_update_timing_params(void);
static float metronome_generate_sine_sample(float freq, float* phase);
static void metronome_advance_beat(void);

#if LOOPER_MULTI_FLASH_ENABLE
/**
 * @brief 轮询指定Flash设备的擦除状态，完成时自动清除对应位
 *
 * 当 chip_erase_pending_mask 的 bit[dev_id] 置位时调用：
 *   - 若芯片仍在擦除 → 返回 1（调用方应跳过本次处理）
 *   - 若擦除完成    → 清除该位；所有设备均完成时持久化 CLEAN 标志；返回 0
 *
 * @param dev_id  Flash设备号 (0 ~ LOOPER_FLASH_DEV_COUNT-1)
 * @return 1=该Flash仍在擦除（应跳过）；0=空闲（可读写）
 */
static uint8_t looper_poll_erase_pending(uint8_t dev_id)
{
    if (!(g_loop_manager.chip_erase_pending_mask & (1u << dev_id))) {
        return 0;  /* 该设备未处于擦除等待状态 */
    }

    if (FlashPartition_LooperIsErasingByDev(dev_id)) {
        return 1;  /* 仍在擦除 */
    }

    /* 擦除完成，清除该位 */
    g_loop_manager.chip_erase_pending_mask &= (uint8_t)(~(1u << dev_id));
    DBG("[Looper] Flash dev%d erase complete, pending_mask=0x%02X\n",
        dev_id, g_loop_manager.chip_erase_pending_mask);

    if (g_loop_manager.chip_erase_pending_mask == 0) {
        /* 所有Flash均擦除完毕，持久化CLEAN标志 */
        SYSPARAM_LOOPER()->flash_status = LOOPER_FLASH_STATUS_CLEAN;
        SysParam_Save();
        DBG("[Looper] All flash erases complete, status=CLEAN saved\n");
    }
    return 0;
}
#endif /* LOOPER_MULTI_FLASH_ENABLE */

// 全局Loop管理器，归纳所有looper相关变量
LoopManager_t g_loop_manager = {
    .state = LOOP_STATE_IDLE,
    .flash_type = FLASH_TYPE_NOR,
    .sector_address = 0,
    .record_length = 0,
    .play_position = 0,
    .is_initialized = 0,
    .is_new_recording = 0,
    .current_segment = 0,
    .active_segments = 0,
    .page_size = LOOPER_PSRAM_PAGE_SIZE,
    .boundary_samples_valid = 0
};

/** 下位机定时操作状态（音频线程仅写 pending_* 位；主循环写其余字段） */
LooperTimedOps_t g_looper_timed_ops = {0};

/* 录制源缓冲区指针 (由 bg_audio_io_manager 在每帧录制前设置) */
uint32_t *g_looper_src_mic    = NULL;  /* ADC1 原始麦克风立体声 */
uint32_t *g_looper_src_linein = NULL;  /* ADC0 原始 LineIn 立体声 */

/* ============================================================================
 * 单声道录制辅助函数
 * ============================================================================ */

/* 从立体声 uint32_t 抽取左声道存储为 int16_t 字节流 (LE) */
static void extractMonoLeft(const uint32_t *input, uint8_t *output, uint16_t count)
{
    uint16_t i;
    for (i = 0; i < count; i++) {
        uint16_t s = (uint16_t)(input[i] & 0xFFFF);
        output[i * 2]     = (uint8_t)(s & 0xFF);
        output[i * 2 + 1] = (uint8_t)((s >> 8) & 0xFF);
    }
}

/* 从立体声 uint32_t 抽取右声道存储为 int16_t 字节流 (LE) */
static void extractMonoRight(const uint32_t *input, uint8_t *output, uint16_t count)
{
    uint16_t i;
    for (i = 0; i < count; i++) {
        uint16_t s = (uint16_t)((input[i] >> 16) & 0xFFFF);
        output[i * 2]     = (uint8_t)(s & 0xFF);
        output[i * 2 + 1] = (uint8_t)((s >> 8) & 0xFF);
    }
}

/* ============================================================================
 * 存储抽象层集成
 * ============================================================================ */

/**
 * @brief 初始化存储抽象层
 * 
 * @details 执行以下操作：
 * 1. 根据硬件配置注册对应的存储适配器
 * 2. 检查是否已执行带宽测试
 * 3. 如果未测试，执行带宽测试并保存结果到 sys_param
 * 4. 根据性能参数设置 looper 的最大同时段数
 */
static void loop_init_storage_layer(void)
{
    LooperStorageStatus_t status;
    LooperStoragePerf_t perf;
    const LooperStorageOps_t *ops = NULL;
    LooperStorageType_t storage_type;
    
    DBG("[Looper] Initializing storage abstraction layer...\n");
    
    /* 1. 根据编译时宏选择存储类型（LOOPER_STORAGE_TYPE 优先；AUTO 时自动检测硬件）*/
#if LOOPER_STORAGE_TYPE == LOOPER_STORAGE_TYPE_PSRAM
    ops = LooperStorageAdapter_GetPsramOps();
    storage_type = LOOPER_STORAGE_PSRAM;
    DBG("[Looper] Using PSRAM storage (forced by LOOPER_STORAGE_TYPE)\n");
#elif LOOPER_STORAGE_TYPE == LOOPER_STORAGE_TYPE_NAND
    ops = LooperStorageAdapter_GetNandFlashOps();
    storage_type = LOOPER_STORAGE_NAND_FLASH;
    DBG("[Looper] Using NAND Flash storage (forced by LOOPER_STORAGE_TYPE)\n");
#elif LOOPER_STORAGE_TYPE == LOOPER_STORAGE_TYPE_NOR
    ops = LooperStorageAdapter_GetNorFlashOps();
    storage_type = LOOPER_STORAGE_NOR_FLASH;
    DBG("[Looper] Using NOR Flash storage (forced by LOOPER_STORAGE_TYPE)\n");
#else /* LOOPER_STORAGE_TYPE_AUTO: 根据硬件宏自动检测 */
    /* 优先 PSRAM（支持叠录），其次 NAND，默认 PSRAM */
#if HW_PSRAM0_EN
    ops = LooperStorageAdapter_GetPsramOps();
    storage_type = LOOPER_STORAGE_PSRAM;
    DBG("[Looper] Using PSRAM storage (auto-detected)\n");
#elif HW_NAND0_EN
    ops = LooperStorageAdapter_GetNandFlashOps();
    storage_type = LOOPER_STORAGE_NAND_FLASH;
    DBG("[Looper] Using NAND Flash storage (auto-detected)\n");
#else
    ops = LooperStorageAdapter_GetPsramOps();
    storage_type = LOOPER_STORAGE_PSRAM;
    DBG("[Looper] Using PSRAM storage as default\n");
#endif
#endif /* LOOPER_STORAGE_TYPE */
    
    /* 2. 注册存储设备 */
    status = LooperStorage_Register(&g_looper_storage, ops, storage_type);
    if (status != LOOPER_STORAGE_OK) {
        DBG("[Looper] Failed to register storage device: %d\n", status);
        return;
    }
    
    /* 3. 初始化存储设备 */
    status = LooperStorage_Init(&g_looper_storage);
    if (status != LOOPER_STORAGE_OK) {
        DBG("[Looper] Failed to initialize storage device: %d\n", status);
        return;
    }
    
    /* 4. 检查是否已执行带宽测试 */
    if (SYSPARAM_LOOPER()->bandwidth_tested == 0) {
        /* 未测试，执行带宽测试 */
        DBG("[Looper] Bandwidth test not performed, running benchmark...\n");
        
        status = LooperStorage_Benchmark(&g_looper_storage, &perf);
        if (status == LOOPER_STORAGE_OK) {
            /* 测试成功，保存结果到 sys_param */
            SYSPARAM_LOOPER()->storage_type = (uint8_t)storage_type;
            SYSPARAM_LOOPER()->write_speed_kbps = perf.write_speed_kbps;
            SYSPARAM_LOOPER()->read_speed_kbps = perf.read_speed_kbps;
            SYSPARAM_LOOPER()->max_concurrent_tracks = perf.max_concurrent_tracks;
            SYSPARAM_LOOPER()->bandwidth_tested = 1;
            SYSPARAM_LOOPER()->support_overdub = perf.support_overdub;
            
            /* 保存到 Flash */
            SysParam_Save();
            
            DBG("[Looper] Benchmark completed: write=%lu KB/s, read=%lu KB/s, max_tracks=%lu, overdub=%u\n",
                (unsigned long)perf.write_speed_kbps,
                (unsigned long)perf.read_speed_kbps,
                perf.max_concurrent_tracks,
                perf.support_overdub);
        } else {
            DBG("[Looper] Benchmark failed: %d, using defaults\n", status);
            /* 测试失败，使用保守的默认值 */
            SYSPARAM_LOOPER()->storage_type = (uint8_t)storage_type;
            SYSPARAM_LOOPER()->max_concurrent_tracks = 1;
            SYSPARAM_LOOPER()->bandwidth_tested = 0;
            SYSPARAM_LOOPER()->support_overdub = (storage_type == LOOPER_STORAGE_PSRAM) ? 1 : 0;
        }
    } else {
        /* 已测试，从 sys_param 加载性能参数 */
        DBG("[Looper] Loading performance data from sys_param\n");
        DBG("[Looper] Storage type: %u, max tracks: %u, overdub: %u\n",
            SYSPARAM_LOOPER()->storage_type,
            SYSPARAM_LOOPER()->max_concurrent_tracks,
            SYSPARAM_LOOPER()->support_overdub);
    }
    
    /* 5. 根据性能参数设置 looper 限制 */
    g_loop_manager.max_concurrent_segments = SYSPARAM_LOOPER()->max_concurrent_tracks;
    g_loop_manager.support_overdub = SYSPARAM_LOOPER()->support_overdub;
    g_loop_manager.overdub_mix_mode = 1; /* 默认相加混音 */
    
    /* 6. 设置存储就绪标志
     * PSRAM 是 volatile RAM，上电即为空白，无需擦除，立即就绪。
     * NOR/NAND Flash 可能需要先执行异步整片擦除（由 loop_check_flash_init_on_boot
     * 管理），在擦除完成前 storage_ready=0 禁止录制写入。 */
    if (storage_type == LOOPER_STORAGE_PSRAM) {
        g_loop_manager.storage_ready = 1;
    } else {
        g_loop_manager.storage_ready = 0; /* 等待 loop_check_flash_init_on_boot 完成 */
    }
    
    DBG("[Looper] Storage layer initialized successfully\n");
}

/* ============================================================================
 * 叠录功能实现
 * ============================================================================ */

/**
 * @brief 检查当前存储设备是否支持叠录
 */
uint8_t loop_is_overdub_supported(void)
{
    return g_loop_manager.support_overdub;
}

/**
 * @brief 设置指定段的叠录模式
 */
void loop_set_overdub_mode(uint8_t segment_index, uint8_t enabled)
{
    if (segment_index >= MAX_SEGMENTS) {
        DBG("[Looper] Invalid segment index for overdub: %u\n", segment_index);
        return;
    }
    
    if (!g_loop_manager.support_overdub) {
        DBG("[Looper] Overdub not supported by current storage\n");
        return;
    }
    
    g_loop_manager.segments[segment_index].overdub_enabled = enabled ? 1 : 0;
    DBG("[Looper] Segment %u overdub mode: %s\n", segment_index, enabled ? "ON" : "OFF");
}

/**
 * @brief 获取指定段的叠录模式
 */
uint8_t loop_get_overdub_mode(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return 0;
    }
    
    return g_loop_manager.segments[segment_index].overdub_enabled;
}

/**
 * @brief 设置叠录混音模式
 */
void loop_set_overdub_mix_mode(uint8_t mix_mode)
{
    if (mix_mode > 2) {
        mix_mode = 1; /* 默认相加混音 */
    }
    
    g_loop_manager.overdub_mix_mode = mix_mode;
    DBG("[Looper] Overdub mix mode set to: %u\n", mix_mode);
}

/**
 * @brief 获取叠录混音模式
 */
uint8_t loop_get_overdub_mix_mode(void)
{
    return g_loop_manager.overdub_mix_mode;
}

// 校验相关变量（归纳到结构体）
static int16_t ReadBuf[96];

// 录音/播放统计信息（归纳到结构体）
static struct {
    uint32_t recording_sample_count;
    uint32_t playback_sample_count;
    int16_t last_recorded_sample;
    int16_t first_playback_sample;
} g_loop_stats = {0};

/* ============================================================================
 * IO缓冲区实例与辅助函数 (LOOPER_IO_BUFFER_ENABLE)
 *
 * 录制写缓冲: 音频回调→RAM→looper_flush_io()→Flash
 * 播放读缓存: Flash→looper_flush_io()→RAM→音频回调
 *
 * 注意: s_write_ring 和 s_read_cache 分配到 PSRAM 而非内部 SRAM，
 *       因为内部 SRAM 仅 192KB，已被其他模块占满 (~190KB)，
 * ============================================================================ */
#if LOOPER_IO_BUFFER_ENABLE

/* IO 缓冲实例 (放在 BSS 段，内部 SRAM)
 *
 * s_write_ring[4]: 8×256 + 6 ≈ 2054 字节/段 × 4 = 8216 字节
 * s_read_cache[4]: 8×256 + 7 ≈ 2055 字节/段 × 4 = 8220 字节
 * 总计 ≈ 16.4KB */
static LooperWriteRing_t  s_write_ring[MAX_SEGMENTS];
static LooperReadCache_t  s_read_cache[MAX_SEGMENTS];

/* 写环形缓冲: 已缓冲页数 */
static uint8_t wring_count(const LooperWriteRing_t *r) {
    int d = (int)r->head - (int)r->tail;
    return (uint8_t)((d >= 0) ? d : d + LOOPER_WRITE_BUF_PAGES);
}
/* 写环形缓冲: 剩余可写槽位 (留1空位区分满/空) */
static uint8_t wring_space(const LooperWriteRing_t *r) {
    return (uint8_t)(LOOPER_WRITE_BUF_PAGES - 1u - wring_count(r));
}
/* 读环形缓存: 已缓存页数 */
static uint8_t rcache_count(const LooperReadCache_t *c) {
    int d = (int)c->head - (int)c->tail;
    return (uint8_t)((d >= 0) ? d : d + LOOPER_READ_CACHE_PAGES);
}
/* 读环形缓存: 剩余可填槽位 */
static uint8_t rcache_space(const LooperReadCache_t *c) {
    return (uint8_t)(LOOPER_READ_CACHE_PAGES - 1u - rcache_count(c));
}

/* 重置指定段的写缓冲 */
static void looper_reset_write_ring(uint8_t seg) {
    LooperWriteRing_t *r = &s_write_ring[seg];
    r->head = 0;
    r->tail = 0;
    r->flush_page = 0;
}
/* 重置指定段的读缓存 */
static void looper_reset_read_cache_internal(uint8_t seg) {
    LooperReadCache_t *c = &s_read_cache[seg];
    c->head = 0;
    c->tail = 0;
    c->prefetch_page = 0;
    c->active = 0;
}
/* 重置所有段的缓冲区 */
static void looper_reset_all_buffers(void) {
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        looper_reset_write_ring(i);
        looper_reset_read_cache_internal(i);
    }
}

/**
 * @brief 清零 IO 缓冲数组（BSS 已由启动代码清零，此处仅保证重入安全）
 */
static uint8_t looper_alloc_io_buffers(void)
{
    uint32_t write_ring_size = sizeof(LooperWriteRing_t) * MAX_SEGMENTS;
    uint32_t read_cache_size = sizeof(LooperReadCache_t) * MAX_SEGMENTS;

    memset(s_write_ring, 0, write_ring_size);
    memset(s_read_cache, 0, read_cache_size);

    DBG("[Looper] IO buffers cleared in SRAM: wr=%lu bytes, rc=%lu bytes\n",
        (unsigned long)write_ring_size, (unsigned long)read_cache_size);
    return 1;
}

#endif /* LOOPER_IO_BUFFER_ENABLE */


void convertUint8ArrayToInt16Array(const uint8_t *input, int16_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; ++i) {
            // 假设系统是小端
            int16_t sample = (int16_t)(input[i * 2]) | ((int16_t)(input[i * 2 + 1]) << 8);
            output[i] = sample;
        }
}

void convertUint32ArrayToUint8Array(const uint32_t *input, uint8_t *output, size_t size) {
	size_t i;
	for (i = 0; i < size; i++) {
        // 每个uint32_t转换为4个uint8_t，保持双声道数据完整
        // 假设系统是小端，uint32_t格式为: [右声道低8位][右声道高8位][左声道低8位][左声道高8位]
        output[i * 4]     = (uint8_t)(input[i] & 0xFF);         // 右声道低8位
        output[i * 4 + 1] = (uint8_t)((input[i] >> 8) & 0xFF);  // 右声道高8位
        output[i * 4 + 2] = (uint8_t)((input[i] >> 16) & 0xFF); // 左声道低8位
        output[i * 4 + 3] = (uint8_t)((input[i] >> 24) & 0xFF); // 左声道高8位
    }
}

void convertUint8ArrayToUint32Array(const uint8_t *input, uint32_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; i++) {
        // 将4个uint8_t重新组合为1个uint32_t，恢复双声道数据
        output[i] = (uint32_t)input[i * 4] |
                    ((uint32_t)input[i * 4 + 1] << 8) |
                    ((uint32_t)input[i * 4 + 2] << 16) |
                    ((uint32_t)input[i * 4 + 3] << 24);
    }
}

void convertInt16ArrayToUint8Array(const int16_t *input, uint8_t *output, size_t size) {
    size_t i;
    for (i = 0; i < size; ++i) {
        // 假设系统是小端
        output[i * 2] = (uint8_t)(input[i] & 0xFF); // 低8位
        output[i * 2 + 1] = (uint8_t)((input[i] >> 8) & 0xFF); // 高8位
    }
}
/**
 * @brief 初始化Loop管理器
 */
void loop_init(void)
{
    memset(&g_loop_manager, 0, sizeof(LoopManager_t));
    
    g_loop_manager.state = LOOP_STATE_IDLE;
    g_loop_manager.flash_type = FLASH_TYPE_NOR;  // 改为默认使用NAND Flash
    g_loop_manager.mode = LOOP_MODE_FREE;        // 默认使用自由模式
    g_loop_manager.sector_address = 0;
    g_loop_manager.record_length = 0;
    g_loop_manager.play_position = 0;
    g_loop_manager.is_initialized = 1;
    g_loop_manager.is_new_recording = 0;
    
    // 初始化多段录音参数
    g_loop_manager.current_segment = 0;
    g_loop_manager.active_segments = 0;
    g_loop_manager.page_size = LOOPER_PSRAM_PAGE_SIZE;

    // 初始化所有段信息
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address = 0;
        g_loop_manager.segments[i].length_pages = 0;
        g_loop_manager.segments[i].length_bytes = 0;
        g_loop_manager.segments[i].play_position = 0;
        g_loop_manager.segments[i].state = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].is_active = 0;
        g_loop_manager.segments[i].rec_partial_count = 0;
        g_loop_manager.segments[i].play_page_offset = 0;
        g_loop_manager.segments[i].play_page_valid = 0;
#if LOOPER_MULTI_FLASH_ENABLE
        g_loop_manager.segments[i].flash_dev_id = (uint8_t)(i % LOOPER_FLASH_DEV_COUNT);
#endif
    }
    
    // 初始化统计信息
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count = 0;
    g_loop_stats.last_recorded_sample = 0;
    g_loop_stats.first_playback_sample = 0;
    
    // 初始化节拍器
    metronome_init();
    
    /* 从 SysParam 加载各段音量。
     * 0xFF(255) = 出厂/未初始化标志，读到时默认使用 100%。
     * 0 是合法的静音值，会被原样加载（不再被误认为"未设置"）。 */
    {
        uint8_t v;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            v = SYSPARAM_LOOPER()->segment_volume[i];
            g_loop_manager.segment_volume[i] = (v == 0xFF) ? 100 : v;
        }
    }

    /* 从 SysParam 加载各段录制源 */
    {
        uint8_t s;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            s = SYSPARAM_LOOPER()->segment_rec_source[i];
            g_loop_manager.segments[i].rec_source =
                (s <= LOOP_REC_SRC_ALL_MIX) ? s : LOOP_REC_SRC_ALL_MIX;
        }
    }

#if LOOPER_IO_BUFFER_ENABLE
    /* 初始化IO缓冲区 */
    looper_reset_all_buffers();
#endif

    /* 先初始化存储抽象层（确定存储类型后，才能决定是否需要 Flash 擦除） */
    loop_init_storage_layer();

    /* PSRAM 是 volatile RAM，上电即空，无需擦除检查。
     * NOR/NAND Flash 需检查上次是否干净；若非 CLEAN 则触发异步全片擦除。
     * storage_ready 由 loop_init_storage_layer() 设置：PSRAM=1，NOR/NAND=0 */
    if (!g_loop_manager.storage_ready) {
        loop_check_flash_init_on_boot();
    }

    /* 不再在初始化时擦除Flash，由用户手动触发（编码器右转）*/
    DBG("Loop manager initialized with multi-segment support (Flash erase deferred)\n");
    //loop_handle_button_press();
}

/**
 * @brief 使用指定Flash类型初始化Loop管理器
 * @param flash_type 要使用的Flash类型
 */
void loop_init_with_flash_type(FlashType_t flash_type)
{
    memset(&g_loop_manager, 0, sizeof(LoopManager_t));
    
    g_loop_manager.state = LOOP_STATE_IDLE;
    g_loop_manager.flash_type = flash_type;  // 使用指定的Flash类型
    g_loop_manager.mode = LOOP_MODE_FREE;    // 默认使用自由模式
    g_loop_manager.sector_address = 0;
    g_loop_manager.record_length = 0;
    g_loop_manager.play_position = 0;
    g_loop_manager.is_initialized = 1;
    g_loop_manager.is_new_recording = 0;
#if LOOPER_MULTI_FLASH_ENABLE
    g_loop_manager.chip_erase_pending_mask = 0;
#else
    g_loop_manager.chip_erase_pending = 0;
#endif
    
    // 初始化多段录音参数
    g_loop_manager.current_segment = 0;
    g_loop_manager.active_segments = 0;
    g_loop_manager.page_size = LOOPER_PSRAM_PAGE_SIZE;

    // 初始化所有段信息
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address = 0;
        g_loop_manager.segments[i].length_pages = 0;
        g_loop_manager.segments[i].length_bytes = 0;
        g_loop_manager.segments[i].play_position = 0;
        g_loop_manager.segments[i].state = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].is_active = 0;
        g_loop_manager.segments[i].rec_partial_count = 0;
        g_loop_manager.segments[i].play_page_offset = 0;
        g_loop_manager.segments[i].play_page_valid = 0;
#if LOOPER_MULTI_FLASH_ENABLE
        g_loop_manager.segments[i].flash_dev_id = (uint8_t)(i % LOOPER_FLASH_DEV_COUNT);
#endif
    }
    
    // 初始化统计信息
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count = 0;
    g_loop_stats.last_recorded_sample = 0;
    g_loop_stats.first_playback_sample = 0;
    
    // 初始化节拍器
    metronome_init();
    
    /* 从 SysParam 加载各段音量。0xFF = 出厂未初始化，默认 100%；0 = 合法静音。 */
    {
        uint8_t v;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            v = SYSPARAM_LOOPER()->segment_volume[i];
            g_loop_manager.segment_volume[i] = (v == 0xFF) ? 100 : v;
        }
    }

    /* 开机检查Looper Flash是否已初始化，否则触发全片擦除 */
    loop_check_flash_init_on_boot();

    /* 不再在初始化时擦除Flash，由用户手动触发（编码器右转）*/
    DBG("Loop manager initialized with %s Flash support (Flash erase deferred)\n",
        g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");

#if LOOPER_IO_BUFFER_ENABLE
    /* 在 PSRAM 中分配 IO 缓冲数组 (避免内部 SRAM 溢出) */
    if (!looper_alloc_io_buffers()) {
        DBG("[Looper] WARNING: IO buffer allocation failed, falling back to direct mode\n");
        /* 分配失败时静默降级为直接写入模式，不影响基本功能 */
    }
#endif
}

/**
 * @brief 重置Loop管理器（包括擦除Flash数据）
 */
void loop_reset(void)
{
#if LOOPER_IO_BUFFER_ENABLE
    looper_reset_all_buffers();
#endif

    /* 1. 将所有正在录制/播放的段重置为未激活状态 */
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address  = 0;
        g_loop_manager.segments[i].start_address2 = 0;
        g_loop_manager.segments[i].region1_pages  = 0;
        g_loop_manager.segments[i].region2_pages  = 0;
        g_loop_manager.segments[i].length_pages  = 0;
        g_loop_manager.segments[i].length_bytes  = 0;
        g_loop_manager.segments[i].play_position = 0;
        g_loop_manager.segments[i].state         = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].is_active     = 0;
        g_loop_manager.segments[i].rec_partial_count = 0;
        g_loop_manager.segments[i].play_page_offset  = 0;
        g_loop_manager.segments[i].play_page_valid   = 0;
#if LOOPER_MULTI_FLASH_ENABLE
        g_loop_manager.segments[i].flash_dev_id  = (uint8_t)(i % LOOPER_FLASH_DEV_COUNT);
#endif
    }

    /* 2. 重置管理器状态变量 */
    g_loop_manager.state              = LOOP_STATE_IDLE;
    g_loop_manager.sector_address     = 0;
    g_loop_manager.record_length      = 0;
    g_loop_manager.play_position      = 0;
    g_loop_manager.is_new_recording   = 0;
    g_loop_manager.current_segment    = 0;
    g_loop_manager.active_segments    = 0;

    /* 3. 清空统计信息 */
    g_loop_stats.recording_sample_count = 0;
    g_loop_stats.playback_sample_count  = 0;
    g_loop_stats.last_recorded_sample   = 0;
    g_loop_stats.first_playback_sample  = 0;

#if LOOPER_MULTI_FLASH_ENABLE
    /* 4. 并行异步全片擦除所有 Looper Flash，擦除完成前阻止录制和播放 */
    {
        uint8_t dev;
        g_loop_manager.chip_erase_pending_mask = 0;
        for (dev = 0; dev < LOOPER_FLASH_DEV_COUNT; dev++) {
            FlashStatus_t ret = FlashPartition_LooperEraseChipAsyncByDev(dev);
            if (ret == FLASH_OK) {
                g_loop_manager.chip_erase_pending_mask |= (uint8_t)(1u << dev);
            } else {
                DBG("[Looper] Reset: Flash dev%d erase failed (%d)\n", dev, ret);
            }
        }
        if (g_loop_manager.chip_erase_pending_mask) {
            DBG("[Looper] Reset: async chip erase started (mask=0x%02X), REC/PLAY blocked\n",
                g_loop_manager.chip_erase_pending_mask);
        } else {
            DBG("[Looper] Reset: Flash erase failed for all devices, proceeding without erase\n");
        }
    }
#else
    /* 4. 异步全片擦除 Looper Flash (#0) */
    FlashPartition_LooperEraseChipAsync();
    g_loop_manager.chip_erase_pending = 1;
    DBG("[Looper] Reset: async chip erase started, REC/PLAY blocked\n");
#endif /* LOOPER_MULTI_FLASH_ENABLE */
}

/**
 * @brief Flash全片擦除初始化（清除所有Looper数据并重新准备录制）
 *
 * 调用后 chip_erase_pending_mask 相应位置 1，在 Flash 擦除完成前
 * 所有录制和播放操作均被阻塞。
 * 每个音频帧自动轮询 BUSY 位，擦除完成后自动解除阻塞。
 */
void loop_flash_erase_reinit(void)
{
    if (!g_loop_manager.is_initialized) {
        DBG("[Looper] Not initialized, call loop_init() first\n");
        return;
    }

    /* 1. 立即停止所有活动 */
    g_loop_manager.state = LOOP_STATE_IDLE;

#if LOOPER_IO_BUFFER_ENABLE
    looper_reset_all_buffers();
#endif

    /* 2. 清空所有段信息 */
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address  = 0;
        g_loop_manager.segments[i].start_address2 = 0;
        g_loop_manager.segments[i].region1_pages  = 0;
        g_loop_manager.segments[i].region2_pages  = 0;
        g_loop_manager.segments[i].length_pages  = 0;
        g_loop_manager.segments[i].length_bytes  = 0;
        g_loop_manager.segments[i].play_position = 0;
        g_loop_manager.segments[i].state         = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].is_active     = 0;
        g_loop_manager.segments[i].rec_partial_count = 0;
        g_loop_manager.segments[i].play_page_offset  = 0;
        g_loop_manager.segments[i].play_page_valid   = 0;
#if LOOPER_MULTI_FLASH_ENABLE
        g_loop_manager.segments[i].flash_dev_id  = (uint8_t)(i % LOOPER_FLASH_DEV_COUNT);
#endif
    }
    g_loop_manager.current_segment  = 0;
    g_loop_manager.active_segments  = 0;
    g_loop_manager.sector_address   = 0;
    g_loop_manager.record_length    = 0;
    g_loop_manager.play_position    = 0;
    g_loop_manager.is_new_recording = 0;

#if LOOPER_MULTI_FLASH_ENABLE
    /* 3. 并行异步全片擦除所有 Looper Flash */
    {
        uint8_t dev;
        g_loop_manager.chip_erase_pending_mask = 0;
        for (dev = 0; dev < LOOPER_FLASH_DEV_COUNT; dev++) {
            FlashStatus_t ret = FlashPartition_LooperEraseChipAsyncByDev(dev);
            if (ret == FLASH_OK) {
                g_loop_manager.chip_erase_pending_mask |= (uint8_t)(1u << dev);
            } else {
                DBG("[Looper] Flash erase reinit: dev%d failed (%d)\n", dev, ret);
            }
        }
        if (!g_loop_manager.chip_erase_pending_mask) {
            DBG("[Looper] Flash erase reinit failed for all devices\n");
            return;
        }
    }
    DBG("[Looper] Flash chip erase started (mask=0x%02X), REC/PLAY blocked until complete\n",
        g_loop_manager.chip_erase_pending_mask);
#else
    /* 3. 异步全片擦除 Looper Flash (#0) */
    {
        FlashStatus_t ret = FlashPartition_LooperEraseChipAsync();
        if (ret != FLASH_OK) {
            DBG("[Looper] Flash erase reinit failed\n");
            return;
        }
        g_loop_manager.chip_erase_pending = 1;
    }
    DBG("[Looper] Flash chip erase started, REC/PLAY blocked until complete\n");
#endif /* LOOPER_MULTI_FLASH_ENABLE */
}

/* ============================================================================
 * 边录边擦（erase-ahead）状态机
 *
 * 每个录制段独立维护一个擦除状态机：
 *   erased_up_to   : Flash 绝对偏移，表示"此地址以下已全部擦完"
 *   block_cur      : 当前正在擦的块起始地址（= erased_up_to 处的块）
 *   pending        : 1 = 当前块擦除命令已发出/正在等待完成
 *   block_issued   : 1 = 擦除命令已发出（轮询等待），0 = 尚未发出
 *
 * 写阻塞条件：write_offset 所在块 >= erased_up_to（该块尚未擦除）
 * erase-ahead 触发条件：写指针距下一块边界 ≤ LOOPER_ERASE_AHEAD_PAGES
 *                        且下一块尚未擦除且无在途擦除任务
 * ============================================================================ */

typedef struct {
    volatile uint8_t  pending;      /* 1 = 当前块擦除正在进行 */
    uint8_t  seg_idx;               /* 所属段索引 */
    uint32_t erased_up_to;          /* Flash 绝对偏移：此地址以下已擦完 */
    uint32_t block_cur;             /* 当前正在擦的块起始地址 */
    uint8_t  block_issued;          /* 1 = 擦除命令已发出，轮询等待 */
} LooperPartialErase_t;

static LooperPartialErase_t s_partial_erase[MAX_SEGMENTS]; /* 每段独立 */

/**
 * @brief 启动指定段的局部块擦除
 * @param seg_idx  段索引（0 或 1），仅支持单 Flash 模式
 * @param max_sec  最大录制秒数，决定需要擦除的字节数
 */
/**
 * @brief 计算新段的动态 Flash 起始地址
 *        = 所有其他已激活段末尾最大值，向上对齐到 64KB 块边界
 * @param exclude_seg  跳过此段（自身），设为 MAX_SEGMENTS 表示不跳过任何段
 * @return 动态起始地址（绝对 Flash 偏移）
 */
static uint32_t looper_compute_dynamic_seg_start(uint8_t exclude_seg)
{
    uint32_t max_end = 0;
    uint8_t i;

    for (i = 0; i < MAX_SEGMENTS; i++) {
        uint32_t end;
        if (i == exclude_seg) continue;
        if (!g_loop_manager.segments[i].is_active) continue;
        end = g_loop_manager.segments[i].start_address +
              (uint32_t)g_loop_manager.segments[i].length_pages * g_loop_manager.page_size;
        if (end > max_end) {
            max_end = end;
        }
    }

    /* 向上对齐到 64KB 块边界 */
    if (max_end % LOOPER_FLASH_BLOCK_SIZE != 0u) {
        max_end = ((max_end / LOOPER_FLASH_BLOCK_SIZE) + 1u) * LOOPER_FLASH_BLOCK_SIZE;
    }
    return max_end;
}

/* ============================================================================
 * PSRAM 动态分配器
 *
 * PSRAM 无需擦除，可直接按地址写入。
 * 分配策略：
 *   1. 扫描所有其他已激活段的占用区域（含分裂的第二区域）
 *   2. 计算空闲区域列表，按大小降序排序
 *   3. 分配最大空闲块为 region1；若存在次大块，预置为 region2
 *      录制时页数超出 region1_pages 后自动溢入 region2
 * ============================================================================ */

/* 区域描述符（占用区或空闲区） */
typedef struct {
    uint32_t start;        /* 起始地址（字节偏移）*/
    uint32_t length_pages; /* 长度（页数）*/
} PsramFreeReg_t;

/**
 * @brief PSRAM 动态空间分配：为段分配最优地址（支持分裂分配）
 * @param seg_idx  待分配段索引
 * @return 1=成功，0=无可用空间
 */
static uint8_t looper_psram_alloc(uint8_t seg_idx)
{
    PsramFreeReg_t occupied[MAX_SEGMENTS * 2]; /* 最多 8 个占用区域 */
    PsramFreeReg_t free_reg[MAX_SEGMENTS * 2 + 1]; /* 最多 9 个空闲区域 */
    uint8_t occ_cnt  = 0;
    uint8_t free_cnt = 0;
    uint32_t cursor  = 0;
    SegmentInfo_t *seg;
    uint8_t i;
    uint8_t j;
    uint8_t k;

    /* 1. 收集其他已激活段的占用区域 */
    for (i = 0; i < MAX_SEGMENTS; i++) {
        SegmentInfo_t *s;
        uint32_t r1_pages;
        if (i == seg_idx) continue;
        s = &g_loop_manager.segments[i];
        if (!s->is_active || s->length_pages == 0u) continue;

        /* region1 */
        if (occ_cnt < (uint8_t)(MAX_SEGMENTS * 2)) {
            r1_pages = (s->start_address2 != 0u && s->region1_pages > 0u)
                       ? s->region1_pages : s->length_pages;
            occupied[occ_cnt].start        = s->start_address;
            occupied[occ_cnt].length_pages = r1_pages;
            occ_cnt++;
        }
        /* region2（分裂分配时）*/
        if (s->start_address2 != 0u && s->region1_pages > 0u &&
            s->length_pages > s->region1_pages) {
            if (occ_cnt < (uint8_t)(MAX_SEGMENTS * 2)) {
                occupied[occ_cnt].start        = s->start_address2;
                occupied[occ_cnt].length_pages = s->length_pages - s->region1_pages;
                occ_cnt++;
            }
        }
    }

    /* 2. 按 start 升序排序（插入排序） */
    for (j = 1u; j < occ_cnt; j++) {
        PsramFreeReg_t tmp = occupied[j];
        k = j;
        while (k > 0u && occupied[k - 1u].start > tmp.start) {
            occupied[k] = occupied[k - 1u];
            k--;
        }
        occupied[k] = tmp;
    }

    /* 3. 计算空闲区域（occupied 之间的空隙 + 末尾剩余）*/
    for (i = 0; i < occ_cnt; i++) {
        uint32_t occ_end;
        if (occupied[i].start > cursor) {
            uint32_t gap_pages = (occupied[i].start - cursor) / LOOPER_PSRAM_PAGE_SIZE;
            if (gap_pages > 0u && free_cnt < (uint8_t)(MAX_SEGMENTS * 2 + 1)) {
                free_reg[free_cnt].start        = cursor;
                free_reg[free_cnt].length_pages = gap_pages;
                free_cnt++;
            }
        }
        occ_end = occupied[i].start + occupied[i].length_pages * LOOPER_PSRAM_PAGE_SIZE;
        if (occ_end > cursor) cursor = occ_end;
    }
    /* 末尾空闲 */
    if (cursor < LOOPER_FLASH_TOTAL_SIZE && free_cnt < (uint8_t)(MAX_SEGMENTS * 2 + 1)) {
        uint32_t tail_pages = (LOOPER_FLASH_TOTAL_SIZE - cursor) / LOOPER_PSRAM_PAGE_SIZE;
        if (tail_pages > 0u) {
            free_reg[free_cnt].start        = cursor;
            free_reg[free_cnt].length_pages = tail_pages;
            free_cnt++;
        }
    }

    if (free_cnt == 0u) {
        DBG("[PSRAM] looper_psram_alloc: no free space for seg%d\n", (int)seg_idx);
        return 0;
    }

    /* 4. 按 length_pages 降序排序（最大在前）*/
    for (j = 1u; j < free_cnt; j++) {
        PsramFreeReg_t tmp = free_reg[j];
        k = j;
        while (k > 0u && free_reg[k - 1u].length_pages < tmp.length_pages) {
            free_reg[k] = free_reg[k - 1u];
            k--;
        }
        free_reg[k] = tmp;
    }

    /* 5. 分配：region1 = 最大空闲块，region2 = 次大块（可选，录制溢出时使用）*/
    seg = &g_loop_manager.segments[seg_idx];
    seg->start_address  = free_reg[0].start;
    seg->region1_pages  = free_reg[0].length_pages;
    seg->start_address2 = 0u;
    seg->region2_pages  = 0u;
    if (free_cnt >= 2u) {
        seg->start_address2 = free_reg[1u].start;
        seg->region2_pages  = free_reg[1u].length_pages;
    }

    DBG("[PSRAM] Alloc seg%d: r1=0x%06lX(%lu p), r2=%s(0x%06lX, %lu p)\n",
        (int)seg_idx,
        (unsigned long)seg->start_address, (unsigned long)seg->region1_pages,
        (seg->start_address2 != 0u) ? "yes" : "none",
        (unsigned long)seg->start_address2, (unsigned long)seg->region2_pages);
    return 1;
}

/**
 * @brief 初始化指定段的边录边擦状态机并触发首块擦除
 *        调用前须先设置好 segment->start_address
 * @param seg_idx  段索引 0-(MAX_SEGMENTS-1)
 */
static void looper_init_erase_ahead(uint8_t seg_idx)
{
    LooperPartialErase_t *pe = &s_partial_erase[seg_idx];
    uint32_t start = g_loop_manager.segments[seg_idx].start_address;

    pe->seg_idx      = seg_idx;
    pe->erased_up_to = start;   /* 尚未擦除任何块 */
    pe->block_cur    = start;   /* 首块待擦 */
    pe->block_issued = 0;
    pe->pending      = 1;       /* 触发首块擦除 */
}

/**
 * @brief 重置指定段并触发首块擦除（动态起始地址，无需预先指定时长）
 * @param seg_idx  段索引 0-(MAX_SEGMENTS-1)
 */
void loop_init_segment_region(uint8_t seg_idx)
{
    uint32_t seg_flash_start;

    if (seg_idx >= MAX_SEGMENTS) {
        DBG("[Looper] loop_init_segment_region: invalid seg_idx %d\n", (int)seg_idx);
        return;
    }
    if (!g_loop_manager.is_initialized) {
        DBG("[Looper] loop_init_segment_region: not initialized\n");
        return;
    }

    /* 动态计算起始地址：所有其他已激活段末尾最大值，64KB 对齐 */
    seg_flash_start = looper_compute_dynamic_seg_start(seg_idx);

    if (seg_flash_start + LOOPER_FLASH_BLOCK_SIZE > LOOPER_FLASH_TOTAL_SIZE) {
        DBG("[Looper] loop_init_segment_region: seg%d no Flash space (start=0x%06lX)\n",
            (int)seg_idx, (unsigned long)seg_flash_start);
        return;
    }

    DBG("[Looper] ErasedAhead seg%d: dynamic start=0x%06lX\n",
        (int)seg_idx, (unsigned long)seg_flash_start);

    /* 重置该段状态 */
#if LOOPER_IO_BUFFER_ENABLE
    looper_reset_write_ring(seg_idx);
    looper_reset_read_cache_internal(seg_idx);
#endif
    g_loop_manager.segments[seg_idx].start_address    = seg_flash_start;
    g_loop_manager.segments[seg_idx].start_address2   = 0u;
    g_loop_manager.segments[seg_idx].region1_pages    = 0u;
    g_loop_manager.segments[seg_idx].region2_pages    = 0u;
    g_loop_manager.segments[seg_idx].length_pages     = 0;
    g_loop_manager.segments[seg_idx].length_bytes     = 0;
    g_loop_manager.segments[seg_idx].play_position    = 0;
    g_loop_manager.segments[seg_idx].state            = SEGMENT_INACTIVE;
    g_loop_manager.segments[seg_idx].is_active        = 0;
    g_loop_manager.segments[seg_idx].trim_start_page  = 0;
    g_loop_manager.segments[seg_idx].trim_end_page    = 0;
    g_loop_manager.segments[seg_idx].rec_partial_count = 0;
    g_loop_manager.segments[seg_idx].play_page_offset  = 0;
    g_loop_manager.segments[seg_idx].play_page_valid   = 0;

    /* 标记 Flash 已使用 */
    SYSPARAM_LOOPER()->flash_status = LOOPER_FLASH_STATUS_USED;
    SysParam_MarkModified();

    /* 初始化边录边擦状态机，触发首块擦除 */
    looper_init_erase_ahead(seg_idx);

    DBG("[Looper] ErasedAhead seg%d: ready, first block 0x%06lX queued\n",
        (int)seg_idx, (unsigned long)seg_flash_start);
}

/**
 * @brief 推进边录边擦状态机（在 looper_flush_io 里每帧调用）
 *
 * 状态机：
 *   pending=1, block_issued=0 → 发出块擦除命令
 *   pending=1, block_issued=1 → 轮询完成 → 更新 erased_up_to，清 pending
 *   段仍在 RECORDING 时自动链接下一块（保持 1 块领先）
 */
#if LOOPER_IO_BUFFER_ENABLE
static void looper_advance_partial_erase(uint8_t seg_idx)
{
    LooperPartialErase_t *pe = &s_partial_erase[seg_idx];
    FlashStatus_t ret;

    if (!pe->pending) return;

    if (pe->block_issued) {
        /* 轮询当前块擦除是否完成 */
#if LOOPER_MULTI_FLASH_ENABLE
        {
            uint8_t dev_id = (uint8_t)(seg_idx % LOOPER_FLASH_DEV_COUNT);
            if (FlashPartition_LooperIsErasingByDev(dev_id)) return;
        }
#else
        if (FlashPartition_LooperIsErasing()) return;
#endif
        /* 当前块擦除完成，更新水位线 */
        pe->erased_up_to = pe->block_cur + LOOPER_FLASH_BLOCK_SIZE;
        pe->block_issued = 0;
        pe->pending      = 0;
        DBG("[Looper] ErasedBlock seg%d 0x%06lX done, erased_up_to=0x%06lX\n",
            (int)seg_idx,
            (unsigned long)pe->block_cur,
            (unsigned long)pe->erased_up_to);

        /* 段仍在录制时，自动链接擦除下一块（始终保持 1 块领先） */
        if (g_loop_manager.segments[seg_idx].state == SEGMENT_RECORDING &&
            pe->erased_up_to < LOOPER_FLASH_TOTAL_SIZE) {
            pe->block_cur    = pe->erased_up_to;
            pe->pending      = 1;
            pe->block_issued = 0;
            DBG("[Looper] ErasedAhead seg%d: queue next block 0x%06lX\n",
                (int)seg_idx, (unsigned long)pe->block_cur);
        }
        return;
    }

    /* 发出块擦除命令（非阻塞） */
    if (pe->block_cur >= LOOPER_FLASH_TOTAL_SIZE) {
        pe->pending = 0;
        DBG("[Looper] ErasedAhead seg%d: reached end of Flash, stop\n", (int)seg_idx);
        return;
    }

#if LOOPER_MULTI_FLASH_ENABLE
    {
        uint8_t dev_id = (uint8_t)(seg_idx % LOOPER_FLASH_DEV_COUNT);
        FlashDevice_t *dev = FlashDevices_GetDevice(dev_id);
        if (dev && dev->initialized) {
            ret = W25Qxx_EraseBlockStart(dev, pe->block_cur);
        } else {
            ret = FLASH_ERR_NOT_INIT;
        }
    }
#else
    ret = FlashPartition_LooperEraseBlockAsync(pe->block_cur);
#endif

    if (ret == FLASH_OK) {
        pe->block_issued = 1;
        DBG("[Looper] ErasedAhead seg%d: erasing block 0x%06lX\n",
            (int)seg_idx, (unsigned long)pe->block_cur);
    } else {
        /* 跳过出错块，推进水位线后继续 */
        DBG("[Looper] ErasedAhead seg%d: cmd failed at 0x%06lX, skip\n",
            (int)seg_idx, (unsigned long)pe->block_cur);
        pe->erased_up_to  = pe->block_cur + LOOPER_FLASH_BLOCK_SIZE;
        pe->block_cur    += LOOPER_FLASH_BLOCK_SIZE;
        pe->pending       = 0;
    }
}
#endif /* LOOPER_IO_BUFFER_ENABLE */

uint8_t loop_segment_partial_erase_pending(uint8_t seg_idx)
{
    if (seg_idx >= MAX_SEGMENTS) return 0;
    return s_partial_erase[seg_idx].pending;
}

/**
 * @brief 处理按键按下事件，支持段选择
 * 当段未激活时，第一次按下进入录音模式，再次按下停止录音并且开始播放
 * 当段播放时，按下停止播放
 * 当段停止时，按下开始播放
 * @param segment_index 段索引 (0-3)，如果为-1则使用传统模式
 */
void loop_handle_button_press(int8_t segment_index)
{
    if (!g_loop_manager.is_initialized) {
        DBG("Loop manager not initialized\n");
        return;
    }

#if LOOPER_MULTI_FLASH_ENABLE
    /* Flash擦除：轮询所有设备BUSY状态，全部完成才放行 */
    if (g_loop_manager.chip_erase_pending_mask) {
        uint8_t dev;
        for (dev = 0; dev < LOOPER_FLASH_DEV_COUNT; dev++) {
            looper_poll_erase_pending(dev);
        }
        if (g_loop_manager.chip_erase_pending_mask) {
            DBG("[Looper] Flash erase in progress (mask=0x%02X), input ignored\n",
                g_loop_manager.chip_erase_pending_mask);
            return;
        }
        DBG("[Looper] All chip erases done (detected on button), unblocked\n");
    }
#else
    /* 全片擦除中先通过存储抽象层查询 BUSY */
    if (g_loop_manager.chip_erase_pending) {
        if (LooperStorage_IsBusy(&g_looper_storage)) {
            DBG("[Looper] Flash erase in progress, input ignored\n");
            return;
        }
        g_loop_manager.chip_erase_pending = 0;
        g_loop_manager.storage_ready = 1;
        SYSPARAM_LOOPER()->flash_status = LOOPER_FLASH_STATUS_CLEAN;
        SysParam_Save();
        DBG("[Looper] Chip erase done (detected on button), status=CLEAN saved\n");
    }
#endif /* LOOPER_MULTI_FLASH_ENABLE */
    
    // 如果指定了段索引，使用新的段控制
    if (segment_index >= 0 && segment_index < MAX_SEGMENTS) {
        loop_handle_segment_button(segment_index);
        return;
    }
    
    // 传统模式：维持向后兼容性
    
    switch(g_loop_manager.state)
    {
        case LOOP_STATE_IDLE:
            // 空闲状态：开始录制新段
            if (g_loop_manager.active_segments < MAX_SEGMENTS) {
                loop_start_new_segment();
                DBG("Start recording segment %d using %s Flash\n",
                    g_loop_manager.current_segment + 1,
                    g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
            } else {
                // 已达到最大段数，清除所有段重新开始
                loop_clear_all_segments();
                loop_start_new_segment();
                DBG("Max segments reached, cleared all and start new recording using %s Flash\n",
                    g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
            }
            break;
            
        case LOOP_STATE_RECORDING:
            // 录制状态：停止当前段录制并开始混音播放
            // 查找正在录制的段
            {
                uint8_t recording_segment = MAX_SEGMENTS;
                uint8_t i;
                for (i = 0; i < MAX_SEGMENTS; i++) {
                    if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
                        recording_segment = i;
                        break;
                    }
                }
                
                if (recording_segment < MAX_SEGMENTS) {
                    loop_stop_current_segment(recording_segment);
                    DBG("Stop recording segment %d, start playing %d segments\n",
                        recording_segment + 1, g_loop_manager.active_segments);
                } else {
                    DBG("No recording segment found\n");
                    g_loop_manager.state = LOOP_STATE_PLAYING;
                }
            }
            break;
            
        case LOOP_STATE_PLAYING:
            // 播放状态：如果还可以录制更多段，则开始录制下一段
            if (g_loop_manager.active_segments < MAX_SEGMENTS) {
                loop_start_new_segment();
                DBG("Start recording segment %d while playing using %s Flash\n", 
                    g_loop_manager.current_segment + 1,
                    g_loop_manager.flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND");
            } else {
                // 已达到最大段数，停止播放回到空闲状态
                g_loop_manager.state = LOOP_STATE_IDLE;
                DBG("Max segments reached, stop playing\n");
            }
            break;
            
        default:
            g_loop_manager.state = LOOP_STATE_IDLE;
            break;
    }
}

/**
 * @brief 处理编码器左转事件：清除所有段
 */
void loop_handle_encoder_left(void)
{
    if (!g_loop_manager.is_initialized) {
        return;
    }
    
    DBG("Encoder left: clear all segments\n");
    loop_clear_all_segments();
    g_loop_manager.state = LOOP_STATE_IDLE;

}

/**
 * @brief 处理编码器右转事件：停止一切活动并擦除全片
 */
void loop_handle_encoder_right(void)
{
    if (!g_loop_manager.is_initialized) {
        DBG("Loop manager not initialized\n");
        return;
    }
    
    // 停止所有活动
    g_loop_manager.state = LOOP_STATE_IDLE;

    
    // 擦除Looper分区 (7MB) - 使用新API
    DBG("Encoder right: Erasing Looper partition (7MB)\n");
    int32_t erase_result = BG_FlashMgr.EraseLooperAll();
    
    if (erase_result < 0) {
        DBG("Flash erase failed: %ld\n", (long)erase_result);
        return;
    }
    
    // 重置所有变量
    g_loop_manager.record_length = 0;
    g_loop_manager.play_position = 0;
    g_loop_manager.sector_address = 0;
    g_loop_manager.is_new_recording = 1;
    
    DBG("Looper partition erased, system reset to idle\n");
}

/**
 * @brief 设置Flash类型
 * @param flash_type Flash类型 (FLASH_TYPE_NOR 或 FLASH_TYPE_NAND)
 */
void loop_set_flash_type(FlashType_t flash_type)
{
    if (flash_type != FLASH_TYPE_NOR && flash_type != FLASH_TYPE_NAND) {
        DBG("Invalid flash type: %d\n", flash_type);
        return;
    }
    
    g_loop_manager.flash_type = flash_type;
    DBG("Flash type set to: %s (value=%d)\n", flash_type == FLASH_TYPE_NOR ? "NOR" : "NAND", flash_type);
    
    // 注：新flash方案不需要切换flash类型，Looper固定在Flash#0
}

/**
 * @brief 获取当前Flash类型
 * @return 当前Flash类型
 */
FlashType_t loop_get_flash_type(void)
{
    return g_loop_manager.flash_type;
}

/**
 * @brief 获取当前Flash设备ID (用于BG_flash_manager调用)
 * @note 硬件只使用NOR Flash，此函数已废弃，保留向后兼容
 * @return DEV_NOR
 */
uint8_t loop_get_flash_device_id(void)
{
    // 硬件只支持NOR Flash，始终返回DEV_NOR
    // 新API (BG_FlashMgr) 不需要device_id
    return 0;  // DEV_NOR
}

/**
 * @brief 停止录制并准备播放
 */
void loop_stop_recording(void)
{
    if (g_loop_manager.state == LOOP_STATE_RECORDING) {
        // 硬件只使用NOR Flash，无需特殊处理

        // 记录录制长度并重置播放位置
        g_loop_manager.record_length = g_loop_manager.sector_address;
        g_loop_manager.play_position = 0;
        g_loop_manager.state = LOOP_STATE_PLAYING;

        
        DBG("Recording stopped manually: total_samples=%lu, record_length=%lu bytes, last_sample=%d\n",
            (unsigned long)g_loop_stats.recording_sample_count, (unsigned long)g_loop_manager.record_length, g_loop_stats.last_recorded_sample);
        
        // 重置地址指针准备下次录制
        g_loop_manager.sector_address = 0;
        g_loop_stats.recording_sample_count = 0;
    }
}

/**
 * @brief 处理录制逻辑
 * @param audio_data 音频数据
 * @param buffer 缓冲区
 * @param length 数据长度
 */
void loop_process_recording(int16_t* audio_data, uint8_t* buffer, uint16_t length)
{
    if (g_loop_manager.state != LOOP_STATE_RECORDING) {
        return;  // 不在录制状态
    }

    // 移除record_flag依赖，直接处理音频数据
    // 录制应该基于音频数据可用性，而不是定时器

    // 只使用Flash录制模式
    {
        // Flash录制逻辑 - 确保长度参数正确
        
        // 数据校验：检查输入音频数据是否有效
        uint16_t non_zero_count = 0;
        int32_t amplitude_sum = 0;
        int16_t max_amplitude = 0;
        uint16_t i;
        for (i = 0; i < length; i++) {
            int16_t sample = audio_data[i];
            if (sample != 0) {
                non_zero_count++;
                amplitude_sum += (sample < 0) ? -sample : sample;  // 手动实现abs
                if ((sample < 0 ? -sample : sample) > (max_amplitude < 0 ? -max_amplitude : max_amplitude)) {
                    max_amplitude = sample;
                }
            }
        }
        
        // 记录统计信息
        g_loop_stats.recording_sample_count += length;
        if (length > 0) {
            g_loop_stats.last_recorded_sample = audio_data[length - 1];
        }
        
        // 如果输入信号太弱，提示调整增益
        if (g_loop_stats.recording_sample_count % 200 == 0 && non_zero_count > 0) {
            int32_t avg_amplitude = amplitude_sum / non_zero_count;
            if (avg_amplitude < 100) {  // 信号较弱
                DBG("WARNING: Input signal weak, avg_amp=%ld, max=%d, consider increasing gain\n",
                    (long)avg_amplitude, max_amplitude);
            }
        }
        
        convertInt16ArrayToUint8Array(audio_data, buffer, length);
        
        // Flash页面大小通常是256字节，我们写入length*2字节的数据
        uint32_t bytes_to_write = length * 2;  // 16位音频转8位需要*2
        
        // 直接使用底层Flash API
        FlashStatus_t write_result = FlashPartition_LooperWrite(g_loop_manager.sector_address, buffer, bytes_to_write);
        
        if (write_result != FLASH_OK) {
            DBG("Flash write error at offset %lu: %d\n", 
                (unsigned long)g_loop_manager.sector_address, write_result);
            // 写入失败，停止录音
            g_loop_manager.record_length = g_loop_manager.sector_address;
            g_loop_manager.play_position = 0;
            g_loop_manager.state = LOOP_STATE_PLAYING;
            return;
        }

        g_loop_stats.recording_sample_count++;
        g_loop_manager.sector_address += bytes_to_write;  // 按实际写入字节数递增

//        if (rec % 500 == 0) {  // 减少调试输出频率，避免影响实时性
//            //DBG("Flash recording: packets=%d, addr=%d, bytes=%d, nonzero=%d, avg_amp=%d, last_sample=%d\n",
//                rec, g_loop_manager.sector_address, bytes_to_write, non_zero_count,
//                non_zero_count > 0 ? amplitude_sum / non_zero_count : 0, last_recorded_sample);
//        }
        
        // 检查Flash存储空间 - Looper分区是7MB
        uint32_t looper_max_size = 7 * 1024 * 1024;  // 7MB Looper分区
        if (g_loop_manager.sector_address >= looper_max_size) {
            DBG("Looper partition full, stop recording. Offset: %lu, Max: %lu\n", 
                (unsigned long)g_loop_manager.sector_address, (unsigned long)looper_max_size);
            
            g_loop_manager.record_length = g_loop_manager.sector_address;  // 正确记录录制长度
            g_loop_manager.play_position = 0;  // 重置播放位置
            g_loop_manager.state = LOOP_STATE_PLAYING;

            DBG("Recording finished: total_samples=%lu, record_length=%lu, last_sample=%d\n", 
                (unsigned long)g_loop_stats.recording_sample_count, (unsigned long)g_loop_manager.record_length, g_loop_stats.last_recorded_sample);
            
            g_loop_stats.recording_sample_count = 0;
            g_loop_stats.playback_sample_count = 0;
        }
        
        // 移除外部变量依赖 - 不再需要同步sectorAddress
    }
}

/**
 * @brief 处理播放逻辑
 * @param output_data 输出音频数据
 * @param buffer 缓冲区
 * @param length 数据长度
 */
void loop_process_playback(int16_t* output_data, uint8_t* buffer, uint16_t length)
{
    if (g_loop_manager.state != LOOP_STATE_PLAYING) {
        return;  // 不在播放状态，保持原始音频数据不变
    }
    
    uint16_t i;
    
    // 只使用Flash播放模式
    {
        // Flash播放逻辑
        if (g_loop_manager.record_length == 0) {
            DBG("No recorded data in flash, record_length=0\n");
            return;  // 没有录制数据，保持原始音频
        }
        
        // 确保播放位置有效
        if (g_loop_manager.play_position >= g_loop_manager.record_length) {
            g_loop_manager.play_position = 0;
        }
        
        // 计算要读取的字节数
        uint32_t bytes_to_read = length * 2;  // 16位音频需要读取length*2字节

        // 确保不会超过录制长度
        if (g_loop_manager.play_position + bytes_to_read > g_loop_manager.record_length) {
            bytes_to_read = g_loop_manager.record_length - g_loop_manager.play_position;
            if (bytes_to_read == 0 || bytes_to_read % 2 != 0) {
                // 已到末尾或奇数字节，重新开始
                g_loop_manager.play_position = 0;
                bytes_to_read = (length * 2 > g_loop_manager.record_length) ?
                               g_loop_manager.record_length : length * 2;
                if (bytes_to_read % 2 != 0) bytes_to_read--;  // 确保偶数字节
                g_loop_stats.playback_sample_count++;
                DBG("Flash loop restart, count: %lu, length: %lu, reading: %lu\n",
                    (unsigned long)g_loop_stats.playback_sample_count, (unsigned long)g_loop_manager.record_length, (unsigned long)bytes_to_read);
            }
        }
        
        // 确保有效的读取长度
        if (bytes_to_read == 0) {
            DBG("Warning: bytes_to_read=0, skipping playback\n");
            return;
        }
        
        // 读取Flash数据 - 直接使用底层API
        FlashStatus_t read_result = FlashPartition_LooperRead(g_loop_manager.play_position, buffer, bytes_to_read);
        
        if (read_result != FLASH_OK) {
            DBG("Flash read error at offset %lu: %d\n", 
                (unsigned long)g_loop_manager.play_position, read_result);
            return;
        }
        
        convertUint8ArrayToInt16Array(buffer, ReadBuf, bytes_to_read/2);
        
        // 数据校验：检查读取的音频数据
        uint16_t valid_samples = bytes_to_read / 2;
        uint16_t non_zero_read = 0;
        int32_t read_amplitude_sum = 0;
        uint16_t j;
        for (j = 0; j < valid_samples; j++) {
            if (ReadBuf[j] != 0) {
                non_zero_read++;
                read_amplitude_sum += (ReadBuf[j] < 0) ? -ReadBuf[j] : ReadBuf[j];  // 手动实现abs
            }
        }
        
        // 记录第一个播放的样本用于校验
        if (g_loop_manager.play_position == 0 && valid_samples > 0) {
            g_loop_stats.first_playback_sample = ReadBuf[0];
            DBG("First playback sample: %d (should match last recorded: %d)\n",
                g_loop_stats.first_playback_sample, g_loop_stats.last_recorded_sample);
        }
        
        // 混合音频数据
        uint16_t samples_to_mix = (valid_samples < length) ? valid_samples : length;
        for (i = 0; i < samples_to_mix; i++) {
            int32_t mixed = (int32_t)output_data[i] + (int32_t)ReadBuf[i];
            output_data[i] = __nds32__clips(mixed, 15);  // 16位饱和限制
        }
        
        g_loop_stats.playback_sample_count += samples_to_mix;
        g_loop_manager.play_position += bytes_to_read;
        
        // 移除外部变量依赖
        // sectorAddress = g_loop_manager.play_position;
    }
}

/**
 * @brief 定时器更新函数，在1ms中断中调用
 * 处理所有需要实时更新的状态
 */
void loop_timer_update(void)
{
    if (!g_loop_manager.is_initialized) {
        return;
    }
    
    // 可以在这里添加需要定时更新的逻辑
    // 例如：LED指示、状态监控等

    // 移除外部变量同步
    // sectorAddress = (g_loop_manager.state == LOOP_STATE_PLAYING) ?
    //                g_loop_manager.play_position : g_loop_manager.sector_address;
}

/**
 * @brief 获取当前循环状态
 */
LoopState_t loop_get_state(void)
{
    return g_loop_manager.state;
}

/**
 * @brief 检查是否正在录制
 */
uint8_t loop_is_recording(void)
{
    return (g_loop_manager.state == LOOP_STATE_RECORDING || 
            g_loop_manager.state == LOOP_STATE_RECORDING_AND_PLAYING) ? 1 : 0;
}

/**
 * @brief 检查是否正在播放
 */
uint8_t loop_is_playing(void)
{
    return (g_loop_manager.state == LOOP_STATE_PLAYING ||
            g_loop_manager.state == LOOP_STATE_RECORDING_AND_PLAYING) ? 1 : 0;
}

/**
 * @brief 获取当前地址
 */
uint32_t loop_get_current_address(void)
{
    return (g_loop_manager.state == LOOP_STATE_PLAYING) ? 
           g_loop_manager.play_position : g_loop_manager.sector_address;
}

/**
 * @brief 获取录制长度
 */
uint32_t loop_get_record_length(void)
{
    return g_loop_manager.record_length;
}

// ============================================================================
// 循环模式控制函数实现
// ============================================================================

/**
 * @brief 设置循环模式
 * @param mode 要设置的循环模式
 */
void loop_set_mode(LoopMode_t mode)
{
    if (mode == LOOP_MODE_SONG || mode == LOOP_MODE_FREE) {
        g_loop_manager.mode = mode;
        DBG("Loop mode set to %s\n", mode == LOOP_MODE_SONG ? "SONG" : "FREE");
        
        // 如果切换到歌曲模式，需要重新计算主段信息
        if (mode == LOOP_MODE_SONG) {
            loop_update_master_segment_info();
        }
    }
}

/**
 * @brief 获取当前循环模式
 * @return 当前循环模式
 */
LoopMode_t loop_get_mode(void)
{
    return g_loop_manager.mode;
}

/**
 * @brief 检查是否为歌曲模式
 * @return 1如果是歌曲模式，0如果不是
 */
uint8_t loop_is_song_mode(void)
{
    return (g_loop_manager.mode == LOOP_MODE_SONG) ? 1 : 0;
}

/**
 * @brief 检查是否为自由模式
 * @return 1如果是自由模式，0如果不是
 */
uint8_t loop_is_free_mode(void)
{
    return (g_loop_manager.mode == LOOP_MODE_FREE) ? 1 : 0;
}

/**
 * @brief 更新主段信息（内部使用）
 * 在歌曲模式下，找到最长的段作为主段，用于循环基准
 */
void loop_update_master_segment_info(void)
{
    uint32_t max_length = 0;
    uint8_t master_index = 0;
    uint8_t i;
    
    // 找到最长的段
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].is_active && 
            g_loop_manager.segments[i].length_bytes > max_length) {
            max_length = g_loop_manager.segments[i].length_bytes;
            master_index = i;
        }
    }
    
    g_loop_manager.master_segment_length = max_length;
    g_loop_manager.master_segment_index = master_index;
    
    if (max_length > 0) {
        DBG("Master segment updated: index %u, length %u bytes\n", 
            (unsigned int)master_index, (unsigned int)max_length);
    }
}

/**
 * @brief 将页索引转换为实际存储地址（支持分裂区域）
 *
 * 分裂分配时（start_address2 != 0 且 region1_pages > 0）：
 *   page_idx <  region1_pages → region1: start_address + page_idx * PAGE_SIZE
 *   page_idx >= region1_pages → region2: start_address2 + (page_idx - region1_pages) * PAGE_SIZE
 *
 * 单区域时（start_address2 == 0 或 region1_pages == 0）：
 *   所有页均在 region1: start_address + page_idx * PAGE_SIZE
 *
 * @param seg       段信息指针
 * @param page_idx  页索引（连续编号，跨越两个区域）
 * @return          对应的绝对字节偏移地址
 */
static uint32_t seg_page_to_addr(const SegmentInfo_t *seg, uint32_t page_idx)
{
    if (seg->start_address2 != 0u && seg->region1_pages > 0u &&
        page_idx >= seg->region1_pages) {
        return seg->start_address2 +
               (page_idx - seg->region1_pages) * LOOPER_PSRAM_PAGE_SIZE;
    }
    return seg->start_address + page_idx * LOOPER_PSRAM_PAGE_SIZE;
}

/**
 * @brief 段录制处理函数 - 基于段实例
 * @param segment_index 要录制的段索引
 * @param audio_data uint32_t格式的音频数据
 * @param buffer 临时缓冲区用于数据转换
 * @param length uint32_t数据的数量
 */
void loop_process_segment_recording(uint8_t segment_index, uint32_t* audio_data, uint8_t* buffer, uint16_t length)
{
    if (segment_index >= MAX_SEGMENTS) {
        return;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state != SEGMENT_RECORDING) {
        return;
    }

#if LOOPER_MULTI_FLASH_ENABLE
    if (looper_poll_erase_pending(segment->flash_dev_id)) {
        return;
    }
#else
    if (g_loop_manager.chip_erase_pending) {
        if (LooperStorage_IsBusy(&g_looper_storage)) {
            return;
        }
        g_loop_manager.chip_erase_pending = 0;
        g_loop_manager.storage_ready = 1;
    }
#endif
    
    if (length > 48) {
        length = 48;
    }
    
    {
    uint8_t incoming_bytes[192]; /* max 48 samples × 4 bytes */
    uint32_t incoming_len;
    uint32_t src_offset = 0;
    uint32_t *src_buf;
    uint8_t is_mono = LOOP_REC_SRC_IS_MONO(segment->rec_source);

    /* 根据录制源选择数据缓冲区并转换 */
    switch (segment->rec_source) {
    case LOOP_REC_SRC_MIC_L:
    case LOOP_REC_SRC_MIC_R:
        src_buf = g_looper_src_mic;
        break;
    case LOOP_REC_SRC_LINEIN_L:
    case LOOP_REC_SRC_LINEIN_R:
        src_buf = g_looper_src_linein;
        break;
    case LOOP_REC_SRC_ALL_MIX:
    default:
        src_buf = audio_data; /* 混音后的信号 (guitar_buf_out) */
        break;
    }
    if (src_buf == NULL) return;

    if (is_mono) {
        /* 单声道：从立体声源中抽取一个声道，2字节/采样 */
        if (segment->rec_source == LOOP_REC_SRC_MIC_L ||
            segment->rec_source == LOOP_REC_SRC_LINEIN_L) {
            extractMonoLeft(src_buf, incoming_bytes, length);
        } else {
            extractMonoRight(src_buf, incoming_bytes, length);
        }
        incoming_len = length * 2;
    } else {
        /* 双声道：4字节/采样 */
        convertUint32ArrayToUint8Array(src_buf, incoming_bytes, length);
        incoming_len = length * 4;
    }
    
    while (src_offset < incoming_len) {
        uint32_t space = LOOPER_PSRAM_PAGE_SIZE - segment->rec_partial_count;
        uint32_t to_copy = incoming_len - src_offset;
        if (to_copy > space) to_copy = space;
        
        memcpy(&segment->rec_partial_buf[segment->rec_partial_count],
               &incoming_bytes[src_offset], to_copy);
        
        segment->rec_partial_count += to_copy;
        src_offset += to_copy;
        
        if (segment->rec_partial_count >= LOOPER_PSRAM_PAGE_SIZE) {
            uint32_t write_offset;

            /* PSRAM 容量检查：超出两区域总页容量时停止录制 */
#if LOOPER_USE_STORAGE_ABSTRACTION
            if (g_looper_storage.initialized &&
                g_looper_storage.info.type == LOOPER_STORAGE_PSRAM &&
                segment->region1_pages > 0u) {
                uint32_t max_pages = segment->region1_pages + segment->region2_pages;
                /* 即将写入 length_pages 页，若等于/超出容量则停止 */
                if (segment->length_pages >= max_pages ||
                    (segment->length_pages >= segment->region1_pages &&
                     segment->start_address2 == 0u)) {
                    segment->state = SEGMENT_STOPPED;
                    DBG("[PSRAM] seg%d storage full (%lu/%lu pages), stop recording\n",
                        (int)segment_index,
                        (unsigned long)segment->length_pages,
                        (unsigned long)max_pages);
                    break;
                }
            }
#endif

            /* 用 seg_page_to_addr 计算写入地址（支持分裂区域）*/
            write_offset = seg_page_to_addr(segment, segment->length_pages);
            
#if LOOPER_IO_BUFFER_ENABLE
            {
                LooperWriteRing_t *ring = &s_write_ring[segment_index];
                if (wring_space(ring) > 0) {
                    memcpy(ring->buf[ring->head], segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE);
                    ring->head = (ring->head + 1) % LOOPER_WRITE_BUF_PAGES;
                }
            }
#else
#if LOOPER_USE_STORAGE_ABSTRACTION
            {
                LooperStorageStatus_t write_result;
                
                if (segment->overdub_enabled && g_loop_manager.support_overdub) {
                    write_result = LooperStorage_OverdubWrite(&g_looper_storage, write_offset, 
                                segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE,
                                g_loop_manager.overdub_mix_mode);
                    if (write_result != LOOPER_STORAGE_OK) {
                        DBG("Overdub write error at offset %lu: %d\n",
                            (unsigned long)write_offset, write_result);
                        segment->state = SEGMENT_INACTIVE;
                        return;
                    }
                } else {
                    write_result = LooperStorage_Write(&g_looper_storage, write_offset, 
                                segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE);
                    if (write_result != LOOPER_STORAGE_OK) {
                        DBG("Storage write error at offset %lu: %d\n",
                            (unsigned long)write_offset, write_result);
                        segment->state = SEGMENT_INACTIVE;
                        return;
                    }
                }
            }
#else
#if LOOPER_MULTI_FLASH_ENABLE
            {
                FlashStatus_t write_result = FlashPartition_LooperWriteByDev(
                    segment->flash_dev_id, write_offset, segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE);
                if (write_result != FLASH_OK) {
                    DBG("Flash dev%d write error at offset %lu: %d\n",
                        segment->flash_dev_id, (unsigned long)write_offset, write_result);
                    segment->state = SEGMENT_INACTIVE;
                    return;
                }
            }
#else
            {
                FlashStatus_t write_result = FlashPartition_LooperWrite(
                    write_offset, segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE);
                if (write_result != FLASH_OK) {
                    DBG("Flash write error at offset %lu: %d\n",
                        (unsigned long)write_offset, write_result);
                    segment->state = SEGMENT_INACTIVE;
                    return;
                }
            }
#endif /* LOOPER_MULTI_FLASH_ENABLE */
#endif /* LOOPER_USE_STORAGE_ABSTRACTION */
#endif /* LOOPER_IO_BUFFER_ENABLE */
            
            segment->length_pages++;
            segment->length_bytes = segment->length_pages * LOOPER_PSRAM_PAGE_SIZE;
            segment->rec_partial_count = 0;
        }
    }
    } /* end of block scope for incoming_bytes etc. */
}

/**
 * @brief 总录制处理函数 - 处理所有正在录制的段
 * @param audio_data uint32_t格式的音频数据
 * @param buffer 临时缓冲区用于数据转换
 * @param length uint32_t数据的数量
 */
void loop_process_recording_uint32(uint32_t* audio_data, uint8_t* buffer, uint16_t length)
{
#if LOOPER_MULTI_FLASH_ENABLE
    /* 每帧轮询各设备BUSY状态，更新 pending_mask（不再全局阻塞，
     * 各段函数内部会对本段绑定的Flash做独立检查）               */
    if (g_loop_manager.chip_erase_pending_mask) {
        uint8_t dev;
        for (dev = 0; dev < LOOPER_FLASH_DEV_COUNT; dev++) {
            looper_poll_erase_pending(dev);
        }
    }
#else
    /* 全片擦除期间：通过存储抽象层查询 BUSY */
    if (g_loop_manager.chip_erase_pending) {
        if (LooperStorage_IsBusy(&g_looper_storage)) {
            return;
        }
        g_loop_manager.chip_erase_pending = 0;
        g_loop_manager.storage_ready = 1;  /* 擦除完成，存储后端就绪 */
        SYSPARAM_LOOPER()->flash_status = LOOPER_FLASH_STATUS_CLEAN;
        SysParam_Save();
    }
#endif /* LOOPER_MULTI_FLASH_ENABLE */

    // 处理所有正在录制的段
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
            loop_process_segment_recording(i, audio_data, buffer, length);
        }
    }
}

/**
 * @brief 段播放处理函数 - 基于段实例
 * @param segment_index 要播放的段索引
 * @param output_data uint32_t格式的输出音频数据（用于混音）
 * @param buffer 临时缓冲区用于数据转换
 * @param length uint32_t数据的数量
 * @return 1=成功播放, 0=无数据播放
 */
uint8_t loop_process_segment_playback(uint8_t segment_index, uint32_t* output_data, uint8_t* buffer, uint16_t length)
{
    static uint32_t play_call_count = 0;
    
    if (segment_index >= MAX_SEGMENTS) {
        return 0;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state != SEGMENT_PLAYING || !segment->is_active) {
        return 0;
    }

    if (segment->length_pages == 0) {
        return 0;
    }

#if LOOPER_MULTI_FLASH_ENABLE
    if (looper_poll_erase_pending(segment->flash_dev_id)) {
        return 0;
    }
#else
    if (g_loop_manager.chip_erase_pending) {
        if (LooperStorage_IsBusy(&g_looper_storage)) {
            return 0;
        }
        g_loop_manager.chip_erase_pending = 0;
        g_loop_manager.storage_ready = 1;
    }
#endif
    
    uint32_t loop_start = segment->trim_start_page;
    uint32_t loop_end   = (segment->trim_end_page > 0 && segment->trim_end_page <= segment->length_pages)
                          ? segment->trim_end_page : segment->length_pages;

    uint8_t is_mono = LOOP_REC_SRC_IS_MONO(segment->rec_source);
    uint32_t samples_per_page = is_mono ? LOOPER_SAMPLES_PER_PAGE_MONO : LOOPER_SAMPLES_PER_PAGE;

    uint32_t samples_needed = (length < 48) ? length : 48;
    uint32_t segment_data[48];
    uint32_t samples_served = 0;
    uint32_t abs_sample_start = segment->play_position * samples_per_page + segment->play_page_offset;
    
    while (samples_served < samples_needed) {
        if (!segment->play_page_valid || 
            segment->play_page_offset >= samples_per_page) {
            
            if (segment->play_position >= loop_end) {
                segment->play_position = loop_start;
#if LOOPER_IO_BUFFER_ENABLE
                {
                    LooperReadCache_t *cache = &s_read_cache[segment_index];
                    if (cache->active) {
                        cache->prefetch_page = loop_start;
                        cache->head = 0;
                        cache->tail = 0;
                        cache->active = 0;
                    }
                }
#endif
                {
                    LooperTimedOps_t *ops = &g_looper_timed_ops;
                    if (ops->chain_armed && (ops->chain_stop_seg == (uint8_t)segment_index)) {
                        ops->pending_chain = 1;
                    }
                    if (ops->join_armed) {
                        ops->pending_join = 1;
                    }
                    if (ops->deferred_stop_mask & (1u << (uint8_t)segment_index)) {
                        ops->pending_wait_finish |= (1u << (uint8_t)segment_index);
                        ops->deferred_stop_mask  &= (uint8_t)(~(1u << (uint8_t)segment_index));
                    }
                    if (ops->sr_armed && (ops->sr_trigger_seg == (uint8_t)segment_index)) {
                        ops->pending_sr = 1;
                        ops->sr_armed   = 0;
                    }
                    if (ops->sr_autostop_armed && (ops->sr_trigger_seg == (uint8_t)segment_index)) {
                        ops->pending_sr_stop   = 1;
                        ops->sr_autostop_armed = 0;
                    }
                }
            }
            
#if LOOPER_IO_BUFFER_ENABLE
            {
                LooperReadCache_t *cache = &s_read_cache[segment_index];
                if (!cache->active) {
                    looper_init_read_cache(segment_index);
                }
                if (rcache_count(cache) == 0) {
                    memset(&segment_data[samples_served], 0, (samples_needed - samples_served) * 4);
                    segment->play_position++;
                    break;
                }
                memcpy(segment->play_page_buf, cache->buf[cache->tail], LOOPER_PSRAM_PAGE_SIZE);
                cache->tail = (cache->tail + 1) % LOOPER_READ_CACHE_PAGES;
            }
#else
            {
                uint32_t read_offset = seg_page_to_addr(segment, segment->play_position);
#if LOOPER_USE_STORAGE_ABSTRACTION
                LooperStorageStatus_t read_result = LooperStorage_Read(&g_looper_storage, read_offset, 
                                                    segment->play_page_buf, LOOPER_PSRAM_PAGE_SIZE);
                if (read_result != LOOPER_STORAGE_OK) {
                    DBG("Storage read error at offset %lu: %d\n",
                        (unsigned long)read_offset, read_result);
                }
#else
#if LOOPER_MULTI_FLASH_ENABLE
                {
                    FlashStatus_t read_result = FlashPartition_LooperReadByDev(
                        segment->flash_dev_id, read_offset, segment->play_page_buf, LOOPER_PSRAM_PAGE_SIZE);
                    if (read_result != FLASH_OK) {
                        DBG("Flash dev%d read error at offset %lu: %d\n",
                            segment->flash_dev_id, (unsigned long)read_offset, read_result);
                        memset(&segment_data[samples_served], 0, (samples_needed - samples_served) * 4);
                        break;
                    }
                }
#else
                {
                    FlashStatus_t read_result = FlashPartition_LooperRead(
                        read_offset, segment->play_page_buf, LOOPER_PSRAM_PAGE_SIZE);
                    if (read_result != FLASH_OK) {
                        DBG("Flash read error at offset %lu: %d\n",
                            (unsigned long)read_offset, read_result);
                        memset(&segment_data[samples_served], 0, (samples_needed - samples_served) * 4);
                        break;
                    }
                }
#endif /* LOOPER_MULTI_FLASH_ENABLE */
#endif /* LOOPER_USE_STORAGE_ABSTRACTION */
            }
#endif /* LOOPER_IO_BUFFER_ENABLE */
            
            segment->play_page_offset = 0;
            segment->play_page_valid = 1;
        }
        
        {
        uint32_t available = samples_per_page - segment->play_page_offset;
        uint32_t to_take = samples_needed - samples_served;
        uint32_t i;
        if (to_take > available) to_take = available;
        
        if (is_mono) {
            /* 单声道：从 int16_t 页数据扩展为双声道 uint32_t */
            int16_t *mono_samples = (int16_t*)segment->play_page_buf;
            for (i = 0; i < to_take; i++) {
                uint16_t s = (uint16_t)mono_samples[segment->play_page_offset + i];
                segment_data[samples_served + i] = ((uint32_t)s << 16) | ((uint32_t)s & 0xFFFF);
            }
        } else {
            uint32_t *page_samples = (uint32_t*)segment->play_page_buf;
            for (i = 0; i < to_take; i++) {
                segment_data[samples_served + i] = page_samples[segment->play_page_offset + i];
            }
        }
        
        segment->play_page_offset += to_take;
        samples_served += to_take;
        
        if (segment->play_page_offset >= samples_per_page) {
            segment->play_position++;
            segment->play_page_valid = 0;
        }
        }
    }
    
    play_call_count++;
    
    if (abs_sample_start < 16) {
        uint16_t j;
        for (j = 0; j < samples_needed && (abs_sample_start + j) < 16; j++) {
            int16_t left = (int16_t)(segment_data[j] & 0xFFFF);
            int16_t right = (int16_t)((segment_data[j] >> 16) & 0xFFFF);
            uint16_t fade_factor = ((abs_sample_start + j) * 100) / 16;
            left = (int16_t)((int32_t)left * fade_factor / 100);
            right = (int16_t)((int32_t)right * fade_factor / 100);
            segment_data[j] = ((uint32_t)(uint16_t)right << 16) | ((uint32_t)(uint16_t)left & 0xFFFF);
        }
    }
    
    uint8_t vol = g_loop_manager.segment_volume[segment_index];
    if (vol > 100) vol = 100;

    uint16_t j;
    for (j = 0; j < samples_needed; j++) {
        int16_t seg_left = (int16_t)(segment_data[j] & 0xFFFF);
        int16_t seg_right = (int16_t)((segment_data[j] >> 16) & 0xFFFF);
        int16_t out_left = (int16_t)(output_data[j] & 0xFFFF);
        int16_t out_right = (int16_t)((output_data[j] >> 16) & 0xFFFF);
        
        int32_t new_left = (int32_t)out_left + ((int32_t)seg_left * vol / 100);
        int32_t new_right = (int32_t)out_right + ((int32_t)seg_right * vol / 100);
        
        new_left = __nds32__clips(new_left, 15);
        new_right = __nds32__clips(new_right, 15);
        
        output_data[j] = ((uint32_t)(uint16_t)new_right << 16) | ((uint32_t)(uint16_t)new_left & 0xFFFF);
    }

    return 1;
}

/**
 * @brief 总播放处理函数 - 混音所有正在播放的段
 * @param output_data uint32_t格式的输出音频数据
 * @param buffer 临时缓冲区用于数据转换
 * @param length uint32_t数据的数量
 */
void loop_process_playback_uint32(uint32_t* output_data, uint8_t* buffer, uint16_t length)
{
#if LOOPER_MULTI_FLASH_ENABLE
    /* 每帧轮询各设备BUSY状态，更新 pending_mask
     * 各段播放函数内部会对本段绑定的Flash做独立检查，不再全局阻塞 */
    if (g_loop_manager.chip_erase_pending_mask) {
        uint8_t dev;
        for (dev = 0; dev < LOOPER_FLASH_DEV_COUNT; dev++) {
            looper_poll_erase_pending(dev);
        }
    }
#else
    /* 全片擦除期间：通过存储抽象层查询 BUSY，输出静音等待完成 */
    if (g_loop_manager.chip_erase_pending) {
        if (LooperStorage_IsBusy(&g_looper_storage)) {
            uint16_t i;
            for (i = 0; i < length; i++) { output_data[i] = 0; }
            return;
        }
        g_loop_manager.chip_erase_pending = 0;
        g_loop_manager.storage_ready = 1;  /* 擦除完成，存储后端就绪 */
        SYSPARAM_LOOPER()->flash_status = LOOPER_FLASH_STATUS_CLEAN;
        SysParam_Save();
    }
#endif /* LOOPER_MULTI_FLASH_ENABLE */

    // 清零输出缓冲区
    uint16_t i;
    for (i = 0; i < length; i++) {
        output_data[i] = 0;
    }
    
    // 统计播放的段数
    uint8_t playing_count = 0;
    
    // 处理所有正在播放的段
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (loop_process_segment_playback(i, output_data, buffer, length)) {
            playing_count++;
        }
    }
    
    /* 不再做全局 1.5x 增益：该增益会掩盖分段音量控制的效果。
     * 分段音量（0-100）已在 loop_process_segment_playback 中按比例缩放：
     *   vol=100 → 录制原始电平（最大），vol=0 → 完全静音。
     * 用户通过 "looper -V <seg> <vol>" 分别调节每段音量。       */
    (void)playing_count;
    
    /* 注意：Effect Graph 模式下节拍器由图的 Metronome 源节点单独处理，
     * 此处不再调用 metronome_process_audio，避免重复叠加。
     * 传统模式 (AudioLoopMinimal) 会在调用本函数后显式调用 metronome_process_audio。 */
}

/**
 * @brief 开始录制新段
 */
void loop_start_new_segment(void)
{
    // 找到第一个未激活的段
    uint8_t new_segment = MAX_SEGMENTS;
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (!g_loop_manager.segments[i].is_active) {
            new_segment = i;
            break;
        }
    }
    
    if (new_segment >= MAX_SEGMENTS) {
        DBG("Cannot start new segment: maximum segments reached\n");
        return;
    }

#if LOOPER_MULTI_FLASH_ENABLE
    /* 按段索引轮询分配Flash设备：段0→dev0, 段1→dev1, 段2→dev0, 段3→dev1
     * 也可通过 loop_set_segment_flash() 在录制前手动绑定               */
    uint8_t flash_dev_id = (uint8_t)(new_segment % LOOPER_FLASH_DEV_COUNT);
#else
    uint8_t flash_dev_id = 0; /* 单Flash模式不使用 */
    (void)flash_dev_id;  /* 消除编译警告：单Flash模式下不使用此变量 */
#endif /* LOOPER_MULTI_FLASH_ENABLE */
    (void)new_segment;  /* 在单Flash模式下未使用此参数 */
    /* 在该Flash设备内寻找已用空间的末尾，新段紧接其后
     * 每颗Flash内各段独立寻址，互不影响                                  */
    uint32_t max_end_address = 0x000000;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].is_active &&
#if LOOPER_MULTI_FLASH_ENABLE
            g_loop_manager.segments[i].flash_dev_id == flash_dev_id &&
#endif
            g_loop_manager.segments[i].length_pages > 0) {
            uint32_t end_addr = g_loop_manager.segments[i].start_address +
                               g_loop_manager.segments[i].length_pages * g_loop_manager.page_size;
            if (end_addr > max_end_address) {
                max_end_address = end_addr;
            }
        }
    }

    // 页对齐
    if (max_end_address % g_loop_manager.page_size != 0) {
        max_end_address = ((max_end_address / g_loop_manager.page_size) + 1) * g_loop_manager.page_size;
    }

    /* ── 地址分配：PSRAM 动态空闲区分配 / NOR Flash 接续末尾 ── */
    uint32_t start_address = 0u;
    {
        /* 先把分裂区域字段清零 */
        g_loop_manager.segments[new_segment].start_address2 = 0u;
        g_loop_manager.segments[new_segment].region1_pages  = 0u;
        g_loop_manager.segments[new_segment].region2_pages  = 0u;

#if LOOPER_USE_STORAGE_ABSTRACTION
        if (g_looper_storage.initialized &&
            g_looper_storage.info.type == LOOPER_STORAGE_PSRAM) {
            /* PSRAM 模式：动态空闲区分配（无需擦除）*/
            if (!looper_psram_alloc(new_segment)) {
                DBG("No PSRAM space for new segment\n");
                return;
            }
            start_address = g_loop_manager.segments[new_segment].start_address;
        } else
#endif /* LOOPER_USE_STORAGE_ABSTRACTION */
        {
            /* NOR/NAND Flash 模式：接续已有段末尾 + 边录边擦 */
            start_address = max_end_address;
            g_loop_manager.segments[new_segment].start_address = start_address;
            looper_init_erase_ahead(new_segment);
        }

#if LOOPER_MULTI_FLASH_ENABLE
        DBG("Segment %d: Flash dev%d, start_address = 0x%08lX (CS%d)\n",
            new_segment, flash_dev_id, (unsigned long)start_address,
            (flash_dev_id == 0) ? FLASH0_CS_PIN : FLASH1_CS_PIN);
#else
        DBG("Segment %d: start_address = 0x%08lX\n",
            new_segment, (unsigned long)start_address);
#endif
    }

    // 初始化新段
    /* start_address 已由分配块设置，仅初始化其余字段 */
    g_loop_manager.segments[new_segment].length_pages  = 0;
    g_loop_manager.segments[new_segment].length_bytes  = 0;
    g_loop_manager.segments[new_segment].is_active     = 1;
    g_loop_manager.segments[new_segment].state         = SEGMENT_RECORDING;
    g_loop_manager.segments[new_segment].play_position = 0;
    g_loop_manager.segments[new_segment].rec_partial_count = 0;
    g_loop_manager.segments[new_segment].play_page_offset  = 0;
    g_loop_manager.segments[new_segment].play_page_valid   = 0;
#if LOOPER_MULTI_FLASH_ENABLE
    g_loop_manager.segments[new_segment].flash_dev_id  = flash_dev_id;
#endif
    
    // 更新活跃段计数
    /* 第一段录制开始时，将存储状态标记为USED并保存
     * PSRAM 是 volatile（掉电即失），不需要标记 USED，否则下次启动会误擦 NOR Flash */
    if (g_loop_manager.active_segments == 0) {
#if LOOPER_USE_STORAGE_ABSTRACTION
        if (!g_looper_storage.initialized ||
            g_looper_storage.info.type != LOOPER_STORAGE_PSRAM)
#endif
        {
            SYSPARAM_LOOPER()->flash_status = LOOPER_FLASH_STATUS_USED;
            SysParam_Save();
            DBG("[Looper] Flash status saved as USED (first segment recording)\n");
        }
    }
    g_loop_manager.active_segments++;
    
    // 智能更新全局状态：如果有其他段在播放，状态会变为RECORDING_AND_PLAYING
    loop_update_global_state();
    g_loop_manager.is_new_recording = 1;
    g_loop_stats.recording_sample_count = 0;

#if LOOPER_IO_BUFFER_ENABLE
    /* 初始化新段的写缓冲 */
    looper_reset_write_ring(new_segment);
#endif
    
#if LOOPER_MULTI_FLASH_ENABLE
    DBG("Started segment %d on Flash dev%d at 0x%08lX\n",
        new_segment, flash_dev_id, (unsigned long)start_address);
#else
    DBG("Started segment %d at 0x%08lX\n",
        new_segment, (unsigned long)start_address);
#endif
}

/**
 * @brief 停止指定段录制
 * @param segment_index 要停止的段索引
 */
void loop_stop_current_segment(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        DBG("Invalid segment index: %d\n", segment_index);
        return;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state != SEGMENT_RECORDING) {
        DBG("Segment %d is not recording\n", segment_index);
        return;
    }
    
#if LOOPER_IO_BUFFER_ENABLE
    looper_flush_write_all(segment_index);
#endif

    if (segment->rec_partial_count > 0) {
        memset(&segment->rec_partial_buf[segment->rec_partial_count], 0,
               LOOPER_PSRAM_PAGE_SIZE - segment->rec_partial_count);
        uint32_t flush_offset = seg_page_to_addr(segment, segment->length_pages);
#if LOOPER_USE_STORAGE_ABSTRACTION
        LooperStorageStatus_t flush_result = LooperStorage_Write(&g_looper_storage, flush_offset, 
                                            segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE);
        if (flush_result == LOOPER_STORAGE_OK) {
            segment->length_pages++;
            segment->length_bytes = segment->length_pages * LOOPER_PSRAM_PAGE_SIZE;
        }
#else
#if LOOPER_MULTI_FLASH_ENABLE
        FlashStatus_t flush_result = FlashPartition_LooperWriteByDev(
            segment->flash_dev_id, flush_offset, segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE);
#else
        FlashStatus_t flush_result = FlashPartition_LooperWrite(
            flush_offset, segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE);
#endif
        if (flush_result == FLASH_OK) {
            segment->length_pages++;
            segment->length_bytes = segment->length_pages * LOOPER_PSRAM_PAGE_SIZE;
        }
#endif
        segment->rec_partial_count = 0;
    }

#if LOOPER_USE_STORAGE_ABSTRACTION
    /* NAND Flash 适配器使用页缓冲，录制结束时强制刷出最后一个不完整物理页 */
    LooperStorage_Flush(&g_looper_storage);
#endif

    // 在录制结尾复制前面的数据，确保循环平滑
    uint8_t copy_pages_to_add = 10;  // 复制前10页数据
    uint8_t copy_buffer[LOOPER_PSRAM_PAGE_SIZE];
    
    // 确保有数据可以复制
    if (segment->length_pages == 0) {
        DBG("Warning: No data recorded for segment %d, marking inactive\n", segment_index);
        segment->state = SEGMENT_INACTIVE;
        segment->is_active = 0;
        g_loop_manager.active_segments = (g_loop_manager.active_segments > 0)
                                         ? g_loop_manager.active_segments - 1 : 0;
        loop_update_global_state();
        return;
    }
    
    // 写入复制的数据页到当前段结尾
    uint8_t page_count;
    for (page_count = 0; page_count < copy_pages_to_add; page_count++) {
        // 计算要复制的源页地址（循环使用段开头的数据）
        uint32_t source_page_index = page_count % segment->length_pages;
        uint32_t source_offset = segment->start_address + source_page_index * LOOPER_PSRAM_PAGE_SIZE;
        
        /* 读取源页数据（同一颗Flash，读写dev_id相同） */
#if LOOPER_USE_STORAGE_ABSTRACTION
        /* 使用存储抽象层 */
        LooperStorageStatus_t read_result = LooperStorage_Read(&g_looper_storage, source_offset, copy_buffer, LOOPER_PSRAM_PAGE_SIZE);
        if (read_result != LOOPER_STORAGE_OK) {
            DBG("Storage read error at offset %lu: %d\n",
                (unsigned long)source_offset, read_result);
            /* 使用静音页进行平滑匹尾, 不中断复制 */
            memset(copy_buffer, 0, LOOPER_PSRAM_PAGE_SIZE);
        }
#else
        /* 使用传统 Flash API */
#if LOOPER_MULTI_FLASH_ENABLE
        FlashStatus_t read_result = FlashPartition_LooperReadByDev(
            segment->flash_dev_id, source_offset, copy_buffer, LOOPER_PSRAM_PAGE_SIZE);
        if (read_result != FLASH_OK) {
            DBG("Flash dev%d read error at offset %lu: %d\n",
                segment->flash_dev_id, (unsigned long)source_offset, read_result);
#else
        FlashStatus_t read_result = FlashPartition_LooperRead(
            source_offset, copy_buffer, LOOPER_PSRAM_PAGE_SIZE);
        if (read_result != FLASH_OK) {
            DBG("Flash read error at offset %lu: %d\n",
                (unsigned long)source_offset, read_result);
#endif /* LOOPER_MULTI_FLASH_ENABLE */
            break;
        }
#endif /* LOOPER_USE_STORAGE_ABSTRACTION */
        
        /* 写入到段结尾（同一颗Flash） */
        uint32_t dest_offset = segment->start_address + 
                               (segment->length_pages + page_count) * LOOPER_PSRAM_PAGE_SIZE;
#if LOOPER_USE_STORAGE_ABSTRACTION
        /* 使用存储抽象层 */
        LooperStorageStatus_t write_result = LooperStorage_Write(&g_looper_storage, dest_offset, copy_buffer, LOOPER_PSRAM_PAGE_SIZE);
        if (write_result != LOOPER_STORAGE_OK) {
            DBG("Storage write error at offset %lu: %d\n",
                (unsigned long)dest_offset, write_result);
            break;
        }
#else
        /* 使用传统 Flash API */
#if LOOPER_MULTI_FLASH_ENABLE
        FlashStatus_t write_result = FlashPartition_LooperWriteByDev(
            segment->flash_dev_id, dest_offset, copy_buffer, LOOPER_PSRAM_PAGE_SIZE);
        if (write_result != FLASH_OK) {
            DBG("Flash dev%d write error at offset %lu: %d\n",
                segment->flash_dev_id, (unsigned long)dest_offset, write_result);
#else
        FlashStatus_t write_result = FlashPartition_LooperWrite(
            dest_offset, copy_buffer, LOOPER_PSRAM_PAGE_SIZE);
        if (write_result != FLASH_OK) {
            DBG("Flash write error at offset %lu: %d\n",
                (unsigned long)dest_offset, write_result);
#endif /* LOOPER_MULTI_FLASH_ENABLE */
            break;
        }
#endif /* LOOPER_USE_STORAGE_ABSTRACTION */
    }
    
    // 更新段长度（包含复制的页）
    segment->length_pages += copy_pages_to_add;
    segment->length_bytes = segment->length_pages * LOOPER_PSRAM_PAGE_SIZE;
    
    DBG("Stop segment %d: recorded %lu pages (%lu bytes) with end-copy (copied %d pages)\n", 
        segment_index, (unsigned long)(segment->length_pages - copy_pages_to_add),
        (unsigned long)segment->length_bytes, copy_pages_to_add);
    
    if (segment->length_pages == 0) {
        // 如果没有录制任何数据，标记段为无效
        segment->is_active = 0;
        segment->state = SEGMENT_INACTIVE;
        DBG("Segment %d has no data, marked as inactive\n", segment_index);
    } else {
        // 设置段为播放状态
        segment->state = SEGMENT_PLAYING;
        segment->play_position = 0;
        segment->play_page_offset = 0;
        segment->play_page_valid = 0;

#if LOOPER_IO_BUFFER_ENABLE
        /* 初始化读缓存，预填播放数据 */
        looper_init_read_cache(segment_index);
#endif

        DBG("Stopped segment %d: %lu pages, set to PLAYING state\n",
            segment_index, (unsigned long)segment->length_pages);
    }
    
    // 更新全局状态
    loop_update_global_state();
}

/**
 * @brief 获取已录制段数
 */
uint8_t loop_get_segment_count(void)
{
    return g_loop_manager.active_segments;
}

/**
 * @brief 清除所有段
 */
void loop_clear_all_segments(void)
{
    uint8_t i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        g_loop_manager.segments[i].start_address  = 0;
        g_loop_manager.segments[i].start_address2 = 0;
        g_loop_manager.segments[i].region1_pages  = 0;
        g_loop_manager.segments[i].region2_pages  = 0;
        g_loop_manager.segments[i].length_pages = 0;
        g_loop_manager.segments[i].length_bytes = 0;
        g_loop_manager.segments[i].is_active = 0;
        g_loop_manager.segments[i].state = SEGMENT_INACTIVE;
        g_loop_manager.segments[i].play_position = 0;
        g_loop_manager.segments[i].rec_partial_count = 0;
        g_loop_manager.segments[i].play_page_offset = 0;
        g_loop_manager.segments[i].play_page_valid = 0;
    }
    
    g_loop_manager.active_segments = 0;
    g_loop_manager.current_segment = 0;
    g_loop_manager.sector_address = 0;
    g_loop_manager.play_position = 0;
    
    // 擦除Looper分区 - 使用新API
    DBG("Clearing all segments and erasing Looper partition...\n");
    int32_t erase_result = BG_FlashMgr.EraseLooperAll();
    if (erase_result < 0) {
        DBG("Flash erase failed: %ld\n", (long)erase_result);
    } else {
        DBG("All segments cleared\n");
    }
}

/**
 * @brief 仅将指定段状态重置为 INACTIVE，不触发 Flash 擦除操作
 *
 * 用于 App 侧"删除单段录音"场景：Android 发送 looper -c <idx>，
 * 固件仅标记该段为空闲；下次对该段录制时将从原 start_address 开始覆写。
 * 注意：flash 物理内容仍存在，但由于 length_pages=0、state=INACTIVE，
 *       固件播放/录制逻辑不会读取旧数据。
 *
 * @param segment_index  段索引 (0 ~ MAX_SEGMENTS-1)
 */
void loop_clear_segment(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        DBG("[Looper] clear_segment: invalid index %d\n", segment_index);
        return;
    }

    /* 若该段正在录制或播放，先令其停止 */
    SegmentState_t st = g_loop_manager.segments[segment_index].state;
    if (st == SEGMENT_RECORDING || st == SEGMENT_PLAYING) {
        g_loop_manager.segments[segment_index].state = SEGMENT_STOPPED;
        DBG("[Looper] clear_segment: segment %d force-stopped\n", segment_index);
    }

    /* 重置段元数据（含 PSRAM 分裂区域字段）*/
    g_loop_manager.segments[segment_index].start_address2  = 0;
    g_loop_manager.segments[segment_index].region1_pages   = 0;
    g_loop_manager.segments[segment_index].region2_pages   = 0;
    g_loop_manager.segments[segment_index].length_pages   = 0;
    g_loop_manager.segments[segment_index].length_bytes   = 0;
    g_loop_manager.segments[segment_index].is_active      = 0;
    g_loop_manager.segments[segment_index].state          = SEGMENT_INACTIVE;
    g_loop_manager.segments[segment_index].play_position  = 0;
    g_loop_manager.segments[segment_index].trim_start_page = 0;
    g_loop_manager.segments[segment_index].trim_end_page   = 0;
    g_loop_manager.segments[segment_index].rec_partial_count = 0;
    g_loop_manager.segments[segment_index].play_page_offset  = 0;
    g_loop_manager.segments[segment_index].play_page_valid   = 0;

    /* 重新统计活跃段数 */
    uint8_t i, cnt = 0;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].state != SEGMENT_INACTIVE) cnt++;
    }
    g_loop_manager.active_segments = cnt;

    DBG("[Looper] Segment %d cleared (INACTIVE, no flash erase)\n", segment_index);
}

// ============================================================================
// 单段精细控制函数实现
// ============================================================================

/**
 * @brief 处理指定段的按键操作
 * @param segment_index 段索引 (0-3)
 */
void loop_handle_segment_button(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        DBG("Invalid segment index: %d\n", segment_index);
        return;
    }

#if LOOPER_MULTI_FLASH_ENABLE
    /* Flash擦除：轮询所有设备BUSY状态，全部完成才放行 */
    if (g_loop_manager.chip_erase_pending_mask) {
        uint8_t dev;
        for (dev = 0; dev < LOOPER_FLASH_DEV_COUNT; dev++) {
            looper_poll_erase_pending(dev);
        }
        if (g_loop_manager.chip_erase_pending_mask) {
            DBG("[Looper] Flash erase in progress, input ignored\n");
            return;
        }
        DBG("[Looper] Chip erase done (detected on segment button), unblocked\n");
    }
#else
    /* 全片擦除中通过存储抽象层查询 BUSY */
    if (g_loop_manager.chip_erase_pending) {
        if (LooperStorage_IsBusy(&g_looper_storage)) {
            DBG("[Looper] Flash erase in progress, input ignored\n");
            return;
        }
        g_loop_manager.chip_erase_pending = 0;
        g_loop_manager.storage_ready = 1;
        DBG("[Looper] Chip erase done (detected on segment button)\n");
    }
#endif /* LOOPER_MULTI_FLASH_ENABLE */

    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    switch (segment->state) {
        case SEGMENT_INACTIVE:
            /* 局部擦除进行中时，拒绝新的录制请求，避免数据损坏 */
            if (loop_segment_partial_erase_pending(segment_index)) {
                DBG("[Looper] Segment %d: partial erase in progress, recording blocked\n",
                    segment_index);
                return;
            }
            // 段未激活：开始录制
            loop_set_segment_recording(segment_index);
            DBG("Segment %d: INACTIVE -> RECORDING\n", segment_index);
            break;
            
        case SEGMENT_RECORDING:
        {
            // 段录制中：停止录制并开始播放
            loop_stop_current_segment(segment_index);
            DBG("Segment %d: RECORDING -> PLAYING (stopped recording)\n", segment_index);
            break;
        }
            
        case SEGMENT_PLAYING:
            // 段播放中：停止播放
            loop_set_segment_stopped(segment_index);
            DBG("Segment %d: PLAYING -> STOPPED\n", segment_index);
            break;
            
        case SEGMENT_STOPPED:
            // 段已停止：开始播放
            loop_set_segment_playing(segment_index);
            DBG("Segment %d: STOPPED -> PLAYING\n", segment_index);
            break;
    }
}

/**
 * @brief 获取指定段的状态
 * @param segment_index 段索引 (0-3)
 * @return 段状态
 */
SegmentState_t loop_get_segment_state(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return SEGMENT_INACTIVE;
    }
    return g_loop_manager.segments[segment_index].state;
}

/**
 * @brief 获取指定段已录制的页数
 * @param segment_index 段索引 (0-3)
 * @return 已录制页数（0 表示没有数据）
 */
uint32_t loop_get_segment_length_pages(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return 0;
    }
    return g_loop_manager.segments[segment_index].length_pages;
}

/**
 * @brief 根据各段状态智能更新全局状态
 */
void loop_update_global_state(void)
{
    uint8_t has_recording = 0;
    uint8_t has_playing = 0;
    uint8_t i;
    
    // 统计各段状态
    for (i = 0; i < MAX_SEGMENTS; i++) {
        if (g_loop_manager.segments[i].state == SEGMENT_RECORDING) {
            has_recording = 1;
        }
        if (g_loop_manager.segments[i].state == SEGMENT_PLAYING) {
            has_playing = 1;
        }
    }
    
    // 根据段状态设置全局状态
    if (has_recording && has_playing) {
        g_loop_manager.state = LOOP_STATE_RECORDING_AND_PLAYING;
        DBG("Global state updated: RECORDING_AND_PLAYING (recording=%d, playing=%d)\n", has_recording, has_playing);
    } else if (has_recording) {
        g_loop_manager.state = LOOP_STATE_RECORDING;
        DBG("Global state updated: RECORDING\n");
    } else if (has_playing) {
        g_loop_manager.state = LOOP_STATE_PLAYING;
        DBG("Global state updated: PLAYING\n");
    } else {
        g_loop_manager.state = LOOP_STATE_IDLE;
        DBG("Global state updated: IDLE\n");
    }
}

/**
 * @brief 设置段进入录制状态
 * @param segment_index 段索引 (0-3)
 */
void loop_set_segment_recording(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state == SEGMENT_INACTIVE) {
#if !LOOPER_MULTI_FLASH_ENABLE
        /* ── 单录制限制：同一时刻只允许一段处于 RECORDING 状态 ── */
        {
            uint8_t rec_i;
            for (rec_i = 0; rec_i < MAX_SEGMENTS; rec_i++) {
                if (rec_i != segment_index &&
                    g_loop_manager.segments[rec_i].state == SEGMENT_RECORDING) {
                    DBG("[Looper] seg%d: blocked — seg%d already recording\n",
                        (int)segment_index, (int)rec_i);
                    return;
                }
            }
        }

        /* ── 初始化分裂区域字段 ── */
        segment->start_address2 = 0u;
        segment->region1_pages  = 0u;
        segment->region2_pages  = 0u;

        /* ── 存储空间分配（PSRAM 动态空闲区 / NOR Flash 边录边擦）── */
#if LOOPER_USE_STORAGE_ABSTRACTION
        if (g_looper_storage.initialized &&
            g_looper_storage.info.type == LOOPER_STORAGE_PSRAM) {
            /* PSRAM 模式：动态空闲区分配（无需擦除）*/
            if (!looper_psram_alloc(segment_index)) {
                DBG("[Looper] seg%d: PSRAM full, cannot start recording\n",
                    (int)segment_index);
                return;
            }
            /* looper_psram_alloc 已设好 start_address / region1_pages / region2_pages */
        } else
#endif /* LOOPER_USE_STORAGE_ABSTRACTION */
        {
            /* NOR/NAND Flash 模式：动态分配 + 边录边擦 */
            uint32_t seg_flash_start;
            LooperPartialErase_t *pe;
            pe = &s_partial_erase[segment_index];
            if (pe->pending || pe->erased_up_to > segment->start_address) {
                /* 已预初始化，使用现有 start_address 和擦除状态 */
                seg_flash_start = segment->start_address;
            } else {
                /* 未预初始化：动态计算 start_address 并触发首块擦除 */
                seg_flash_start = looper_compute_dynamic_seg_start(segment_index);
                if (seg_flash_start + LOOPER_FLASH_BLOCK_SIZE > LOOPER_FLASH_TOTAL_SIZE) {
                    DBG("[Looper] seg%d: no Flash space (start=0x%06lX)\n",
                        (int)segment_index, (unsigned long)seg_flash_start);
                    return;
                }
                segment->start_address = seg_flash_start;
                looper_init_erase_ahead(segment_index);
            }
        }

        segment->length_pages  = 0;
        segment->length_bytes  = 0;
        segment->is_active     = 1;
        segment->state         = SEGMENT_RECORDING;
        segment->play_position = 0;
        segment->rec_partial_count = 0;
        segment->play_page_offset  = 0;
        segment->play_page_valid   = 0;

        if (g_loop_manager.active_segments == 0) {
#if LOOPER_USE_STORAGE_ABSTRACTION
            if (!g_looper_storage.initialized ||
                g_looper_storage.info.type != LOOPER_STORAGE_PSRAM)
#endif
            {
                SYSPARAM_LOOPER()->flash_status = LOOPER_FLASH_STATUS_USED;
                SysParam_Save();
                DBG("[Looper] Flash status saved as USED (seg%d first recording)\n", segment_index);
            }
        }
        g_loop_manager.active_segments++;

        loop_update_global_state();
        g_loop_manager.is_new_recording = 1;
        g_loop_stats.recording_sample_count = 0;

#if LOOPER_IO_BUFFER_ENABLE
        looper_reset_write_ring(segment_index);
#endif
        DBG("[Looper] Segment %d: INACTIVE->RECORDING at 0x%06lX (dynamic alloc)\n",
            (int)segment_index, (unsigned long)segment->start_address);
        return;
#else
        /* 多Flash模式：保留原来的 loop_start_new_segment() 逻辑 */
        loop_start_new_segment();
        return;
#endif
    }

    /* 段已激活但不是 RECORDING 时，直接切换到 RECORDING 状态 */
    segment->state = SEGMENT_RECORDING;
    
#if LOOPER_IO_BUFFER_ENABLE
    /* 初始化写缓冲 */
    looper_reset_write_ring(segment_index);
#endif

    // 智能更新全局状态：不干扰其他段的播放
    loop_update_global_state();
    g_loop_manager.is_new_recording = 1;
    g_loop_stats.recording_sample_count = 0;
}

/**
 * @brief 设置段进入播放状态
 * @param segment_index 段索引 (0-3)
 */
void loop_set_segment_playing(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    // 检查段是否可以播放：必须是已激活的段，并且不是INACTIVE状态
    if (segment->state == SEGMENT_INACTIVE) {
        DBG("Cannot play segment %d: segment is inactive\n", segment_index);
        return;
    }
    
    // 如果段还没有数据，不能播放
    if (segment->length_pages == 0) {
        DBG("Cannot play segment %d: no recorded data\n", segment_index);
        return;
    }
    
    segment->state = SEGMENT_PLAYING;
    segment->play_position = 0;
    segment->play_page_offset = 0;
    segment->play_page_valid = 0;

#if LOOPER_IO_BUFFER_ENABLE
    /* 初始化读缓存，预填播放数据 */
    looper_init_read_cache(segment_index);
#endif

    // 智能更新全局状态：不干扰其他段
    loop_update_global_state();
    
    DBG("Segment %d set to PLAYING state\n", segment_index);
}

/**
 * @brief 设置段进入停止状态
 * @param segment_index 段索引 (0-3)
 */
void loop_set_segment_stopped(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return;
    }
    
    SegmentInfo_t* segment = &g_loop_manager.segments[segment_index];
    
    if (segment->state == SEGMENT_RECORDING) {
#if LOOPER_IO_BUFFER_ENABLE
        looper_flush_write_all(segment_index);
#endif
        if (segment->rec_partial_count > 0) {
            memset(&segment->rec_partial_buf[segment->rec_partial_count], 0,
                   LOOPER_PSRAM_PAGE_SIZE - segment->rec_partial_count);
            uint32_t flush_offset = segment->start_address + 
                                    segment->length_pages * LOOPER_PSRAM_PAGE_SIZE;
#if LOOPER_USE_STORAGE_ABSTRACTION
            LooperStorage_Write(&g_looper_storage, flush_offset, 
                               segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE);
#else
#if LOOPER_MULTI_FLASH_ENABLE
            FlashPartition_LooperWriteByDev(segment->flash_dev_id, flush_offset, 
                                            segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE);
#else
            FlashPartition_LooperWrite(flush_offset, segment->rec_partial_buf, LOOPER_PSRAM_PAGE_SIZE);
#endif
#endif
            segment->length_pages++;
            segment->rec_partial_count = 0;
        }
        segment->length_bytes = segment->length_pages * LOOPER_PSRAM_PAGE_SIZE;
        
        DBG("Segment %d recording stopped: %lu pages\n", 
            segment_index, (unsigned long)segment->length_pages);
    }
    
#if LOOPER_IO_BUFFER_ENABLE
    /* 停止播放/录制时释放缓冲区 */
    looper_reset_write_ring(segment_index);
    looper_reset_read_cache_internal(segment_index);
#endif

    segment->state = SEGMENT_STOPPED;
    segment->play_page_valid = 0;
    segment->play_page_offset = 0;
    loop_update_global_state();
    
    DBG("Segment %d set to STOPPED state\n", segment_index);
}

/**
 * @brief 检查指定段是否在录制
 * @param segment_index 段索引 (0-3)
 * @return 1=正在录制, 0=未录制
 */
uint8_t loop_is_segment_recording(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return 0;
    }
    return (g_loop_manager.segments[segment_index].state == SEGMENT_RECORDING) ? 1 : 0;
}

/**
 * @brief 检查指定段是否在播放
 * @param segment_index 段索引 (0-3)
 * @return 1=正在播放, 0=未播放
 */
uint8_t loop_is_segment_playing(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) {
        return 0;
    }
    return (g_loop_manager.segments[segment_index].state == SEGMENT_PLAYING) ? 1 : 0;
}

/* ============================================================================
 * 定时操作（Looper_TimedOps）实现
 * ============================================================================ */

/**
 * @brief 清除所有定时操作状态（looper reset 时调用）
 */
void Looper_TimedOps_Reset(void)
{
    memset(&g_looper_timed_ops, 0, sizeof(g_looper_timed_ops));
    DBG("[TimedOps] Reset\n");
}

/**
 * @brief 主循环中消费 pending 标志，执行停止/启动，并发送 BLE 通知包。
 *
 * 通知包格式 (5字节):
 *   [0xAA][0x55][0x23][0x01][timed_ops_state]
 *   timed_ops_state 位定义:
 *     bit0 = chain_armed      (衔接等待中)
 *     bit1 = join_armed       (接入等待中)
 *     bit2 = wait_finish[0]   (seg0 等待播完)
 *     bit3 = wait_finish[1]   (seg1 等待播完)
 *     bit4 = deferred_stop[0] (seg0 延迟停止挂起)
 *     bit5 = deferred_stop[1] (seg1 延迟停止挂起)
 */
void Looper_TimedOps_Process(void)
{
    LooperTimedOps_t *ops = &g_looper_timed_ops;
    uint8_t did_something = 0;

    /* Shell_WriteRaw 前置声明（不引入头文件依赖） */
    extern void Shell_WriteRaw(const uint8_t *data, uint16_t len);

    /* ── 1. 衔接处理 ─────────────────────────────────────────────────── */
    if (ops->pending_chain) {
        uint8_t stop_seg  = ops->chain_stop_seg;
        uint8_t start_seg = ops->chain_start_seg;
        ops->pending_chain = 0;
        ops->chain_armed   = 0;
        /* 先停后起，顺序很重要 */
        if (stop_seg < MAX_SEGMENTS) {
            loop_set_segment_stopped(stop_seg);
            DBG("[TimedOps] Chain: seg%d stopped\n", stop_seg);
        }
        if (start_seg < MAX_SEGMENTS) {
            loop_set_segment_playing(start_seg);
            DBG("[TimedOps] Chain: seg%d started\n", start_seg);
        }
        did_something = 1;
    }

    /* ── 2. 接入处理 ─────────────────────────────────────────────────── */
    if (ops->pending_join) {
        uint8_t start_seg = ops->join_start_seg;
        ops->pending_join = 0;
        ops->join_armed   = 0;
        if (start_seg < MAX_SEGMENTS) {
            loop_set_segment_playing(start_seg);
            DBG("[TimedOps] Join: seg%d started\n", start_seg);
        }
        did_something = 1;
    }

    /* ── 3. 同步录制处理 ─────────────────────────────────────────────── */
    if (ops->pending_sr) {
        uint8_t rec_seg = ops->sr_record_seg;
        ops->pending_sr = 0;
        if (rec_seg < MAX_SEGMENTS) {
            loop_set_segment_recording(rec_seg);
            /* match 模式：等待 trigger_seg 下次回绕时自动停止（等长录制） */
            if (ops->sr_match) {
                ops->sr_autostop_armed = 1;
            }
            DBG("[TimedOps] SyncRec: seg%d started recording at boundary (match=%d)\n",
                rec_seg, ops->sr_match);
        }
        did_something = 1;
    }

    /* ── 3b. 同步录制等长自动停止 ──────────────────────────────────────── */
    if (ops->pending_sr_stop) {
        uint8_t rec_seg = ops->sr_record_seg;
        ops->pending_sr_stop = 0;
        if (rec_seg < MAX_SEGMENTS &&
            loop_get_segment_state(rec_seg) == SEGMENT_RECORDING) {
            loop_stop_current_segment(rec_seg);  /* 精确停止并切换到 PLAYING */
            DBG("[TimedOps] SyncRec auto-stop: seg%d stopped at boundary -> PLAYING\n", rec_seg);
        }
        did_something = 1;
    }

    /* ── 4. 延迟停止处理 ─────────────────────────────────────────────── */
    if (ops->pending_wait_finish) {
        uint8_t mask = ops->pending_wait_finish;
        ops->pending_wait_finish = 0;
        uint8_t i;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            if (mask & (1u << i)) {
                loop_set_segment_stopped(i);
                /* 一次性：执行后清除对应段的 wait-finish 偏好，避免按钮卡在激活态 */
                ops->wait_finish_mask &= (uint8_t)(~(1u << i));
                DBG("[TimedOps] WaitFinish: seg%d stopped (mask cleared)\n", i);
            }
        }
        did_something = 1;
    }

    /* ── 5. 发送 BLE 通知 ────────────────────────────────────────────── */
    if (did_something) {
        uint8_t state_byte = 0;
        if (ops->chain_armed)              state_byte |= (1u << 0);
        if (ops->join_armed)               state_byte |= (1u << 1);
        if (ops->wait_finish_mask  & 0x01) state_byte |= (1u << 2);
        if (ops->wait_finish_mask  & 0x02) state_byte |= (1u << 3);
        if (ops->deferred_stop_mask & 0x01) state_byte |= (1u << 4);
        if (ops->deferred_stop_mask & 0x02) state_byte |= (1u << 5);
        if (ops->sr_armed)                 state_byte |= (1u << 6); /* bit6: SR 激活中 */
        uint8_t buf[5];
        buf[0] = 0xAA;
        buf[1] = 0x55;
        buf[2] = 0x23;  /* type: timed-ops notify */
        buf[3] = 0x01;  /* payload length */
        buf[4] = state_byte;
        Shell_WriteRaw(buf, sizeof(buf));
    }
}



/**
 * @brief AudioLooper接口：初始化
 */
static void AudioLooper_Init(void) {
    loop_init();
}

/**
 * @brief AudioLooper接口：使用指定Flash类型初始化
 */
static void AudioLooper_InitWithFlashType(FlashType_t flash_type) {
    loop_init_with_flash_type(flash_type);
}

/**
 * @brief AudioLooper接口：重置
 */
static void AudioLooper_Reset(void) {
    loop_reset();
}

/**
 * @brief AudioLooper接口：设置Flash类型
 */
static LoopResult_t AudioLooper_SetFlashType(FlashType_t flash_type) {
    loop_set_flash_type(flash_type);
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：处理按键按下
 */
static LoopResult_t AudioLooper_ButtonPress(void) {
    loop_handle_button_press(-1);  // 使用传统模式
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：处理指定段的按键按下
 * @param segment_index 段索引 (0-3)
 */
static LoopResult_t AudioLooper_SegmentButtonPress(uint8_t segment_index) {
    if (segment_index >= MAX_SEGMENTS) {
        return LOOP_RESULT_ERROR;
    }
    loop_handle_button_press(segment_index);
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：处理编码器左转
 */
static LoopResult_t AudioLooper_EncoderLeft(void) {
    loop_handle_encoder_left();
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：处理编码器右转
 */
static LoopResult_t AudioLooper_EncoderRight(void) {
    loop_handle_encoder_right();
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：停止录制
 */
static LoopResult_t AudioLooper_StopRecording(void) {
    loop_stop_recording();
    return LOOP_RESULT_OK;
}

/**
 * @brief AudioLooper接口：处理录制（16位）
 */
static void AudioLooper_ProcessRecording(int16_t* audio_data, uint8_t* buffer, uint16_t length) {
    loop_process_recording(audio_data, buffer, length);
}

/**
 * @brief AudioLooper接口：处理播放（16位）
 */
static void AudioLooper_ProcessPlayback(int16_t* output_data, uint8_t* buffer, uint16_t length) {
    loop_process_playback(output_data, buffer, length);
}

/**
 * @brief AudioLooper接口：处理录制（32位）
 */
static void AudioLooper_ProcessRecording32(uint32_t* audio_data, uint8_t* buffer, uint16_t length) {
    loop_process_recording_uint32(audio_data, buffer, length);
}

/**
 * @brief AudioLooper接口：处理播放（32位）
 */
static void AudioLooper_ProcessPlayback32(uint32_t* output_data, uint8_t* buffer, uint16_t length) {
    loop_process_playback_uint32(output_data, buffer, length);
}

/**
 * @brief AudioLooper接口：获取状态
 */
static LoopStatus_t AudioLooper_GetStatus(void) {
    LoopStatus_t status;

    status.current_state = g_loop_manager.state;
    status.active_segments = g_loop_manager.active_segments;
    status.current_segment = g_loop_manager.current_segment;
    status.total_recorded_bytes = g_loop_manager.record_length;
    status.total_play_time_ms = 0; // 可根据需要计算
    status.flash_type = g_loop_manager.flash_type;
    status.is_recording = (g_loop_manager.state == LOOP_STATE_RECORDING) ? 1 : 0;
    status.is_playing = (g_loop_manager.state == LOOP_STATE_PLAYING) ? 1 : 0;

    return status;
}

/**
 * @brief AudioLooper接口：检查是否正在录制
 */
static uint8_t AudioLooper_IsRecording(void) {
    return loop_is_recording();
}

/**
 * @brief AudioLooper接口：检查是否正在播放
 */
static uint8_t AudioLooper_IsPlaying(void) {
    return loop_is_playing();
}

/**
 * @brief AudioLooper接口：获取当前地址
 */
static uint32_t AudioLooper_GetCurrentAddress(void) {
    return loop_get_current_address();
}

/**
 * @brief AudioLooper接口：获取录制长度
 */
static uint32_t AudioLooper_GetRecordLength(void) {
    return loop_get_record_length();
}

/**
 * @brief AudioLooper接口：定时器更新
 */
static void AudioLooper_TimerUpdate(void) {
    loop_timer_update();
}

// ============================================================================
// 节拍器模块实现
// ============================================================================

/**
 * @brief 初始化节拍器（使用默认设置）
 */
void metronome_init(void) {
    // 初始化节拍器配置为默认值
    g_loop_manager.metronome.state = METRONOME_OFF;
    g_loop_manager.metronome.config.bpm = METRONOME_DEFAULT_BPM;
    g_loop_manager.metronome.config.beats_per_measure = METRONOME_DEFAULT_BEATS_PER_MEASURE;
    g_loop_manager.metronome.config.downbeat_freq = METRONOME_DEFAULT_DOWNBEAT_FREQ;
    g_loop_manager.metronome.config.regular_beat_freq = METRONOME_DEFAULT_REGULAR_BEAT_FREQ;
    g_loop_manager.metronome.config.beat_duration_ms = METRONOME_DEFAULT_BEAT_DURATION;
    g_loop_manager.metronome.config.volume = METRONOME_DEFAULT_VOLUME;
    
    // 初始化运行时状态
    g_loop_manager.metronome.sample_counter = 0;
    g_loop_manager.metronome.beat_sample_counter = 0;
    g_loop_manager.metronome.current_beat = 0;
    g_loop_manager.metronome.is_beat_active = 0;
    g_loop_manager.metronome.current_beat_type = BEAT_TYPE_DOWNBEAT;
    g_loop_manager.metronome.sine_phase = 0.0f;
    
    // 更新计时参数
    metronome_update_timing_params();
    
    DBG("Metronome initialized: BPM=%d, beats_per_measure=%d\n", 
        g_loop_manager.metronome.config.bpm, 
        g_loop_manager.metronome.config.beats_per_measure);
}

/**
 * @brief 重置节拍器状态
 */
void metronome_reset(void) {
    g_loop_manager.metronome.sample_counter = 0;
    g_loop_manager.metronome.beat_sample_counter = 0;
    g_loop_manager.metronome.current_beat = 0;
    g_loop_manager.metronome.is_beat_active = 0;
    g_loop_manager.metronome.current_beat_type = BEAT_TYPE_DOWNBEAT;
    g_loop_manager.metronome.sine_phase = 0.0f;
}

/**
 * @brief 配置节拍器参数
 * @param config 节拍器配置结构体指针
 */
void metronome_configure(const MetronomeConfig_t* config) {
    if (config == NULL) return;
    
    // 验证并设置BPM
    if (config->bpm >= METRONOME_MIN_BPM && config->bpm <= METRONOME_MAX_BPM) {
        g_loop_manager.metronome.config.bpm = config->bpm;
    }
    
    // 验证并设置每小节拍数
    if (config->beats_per_measure >= METRONOME_MIN_BEATS_PER_MEASURE &&
        config->beats_per_measure <= METRONOME_MAX_BEATS_PER_MEASURE) {
        g_loop_manager.metronome.config.beats_per_measure = config->beats_per_measure;
    }
    
    // 设置频率参数
    g_loop_manager.metronome.config.downbeat_freq = config->downbeat_freq;
    g_loop_manager.metronome.config.regular_beat_freq = config->regular_beat_freq;
    g_loop_manager.metronome.config.beat_duration_ms = config->beat_duration_ms;
    
    // 验证并设置音量
    if (config->volume >= 0.0f && config->volume <= 1.0f) {
        g_loop_manager.metronome.config.volume = config->volume;
    }
    
    // 更新计时参数
    metronome_update_timing_params();
    
    DBG("Metronome configured: BPM=%d, beats=%d, vol=%.2f\n", 
        g_loop_manager.metronome.config.bpm,
        g_loop_manager.metronome.config.beats_per_measure,
        g_loop_manager.metronome.config.volume);
}

/**
 * @brief 切换节拍器开关状态
 */
void metronome_toggle(void) {
    if (g_loop_manager.metronome.state == METRONOME_OFF) {
        metronome_enable();
    } else {
        metronome_disable();
    }
}

/**
 * @brief 启用节拍器
 */
void metronome_enable(void) {
    g_loop_manager.metronome.state = METRONOME_ON;
    metronome_reset();  // 重置状态，从第一拍开始
    DBG("Metronome enabled\n");
}

/**
 * @brief 禁用节拍器
 */
void metronome_disable(void) {
    g_loop_manager.metronome.state = METRONOME_OFF;
    DBG("Metronome disabled\n");
}

/**
 * @brief 检查节拍器是否启用
 * @return 1如果启用，0如果禁用
 */
uint8_t metronome_is_enabled(void) {
    return (g_loop_manager.metronome.state == METRONOME_ON);
}

/**
 * @brief 设置BPM
 * @param bpm 节拍速度（60-200）
 */
void metronome_set_bpm(uint16_t bpm) {
    if (bpm >= METRONOME_MIN_BPM && bpm <= METRONOME_MAX_BPM) {
        g_loop_manager.metronome.config.bpm = bpm;
        metronome_update_timing_params();
        DBG("Metronome BPM set to %d\n", bpm);
    }
}

/**
 * @brief 设置每小节拍数
 * @param beats 每小节拍数（2-8）
 */
void metronome_set_beats_per_measure(uint8_t beats) {
    if (beats >= METRONOME_MIN_BEATS_PER_MEASURE && beats <= METRONOME_MAX_BEATS_PER_MEASURE) {
        g_loop_manager.metronome.config.beats_per_measure = beats;
        // 重置当前拍子以避免超出范围
        if (g_loop_manager.metronome.current_beat >= beats) {
            g_loop_manager.metronome.current_beat = 0;
        }
        DBG("Metronome beats per measure set to %d\n", beats);
    }
}

/**
 * @brief 设置音量
 * @param volume 音量系数（0.0-1.0）
 */
void metronome_set_volume(float volume) {
    if (volume >= 0.0f && volume <= 1.0f) {
        g_loop_manager.metronome.config.volume = volume;
        DBG("Metronome volume set to %.2f\n", volume);
    }
}

/**
 * @brief 设置下拍频率
 * @param freq 下拍频率（Hz）
 */
void metronome_set_downbeat_freq(uint16_t freq) {
    g_loop_manager.metronome.config.downbeat_freq = freq;
    DBG("Metronome downbeat frequency set to %d Hz\n", freq);
}

/**
 * @brief 设置普通拍频率
 * @param freq 普通拍频率（Hz）
 */
void metronome_set_regular_beat_freq(uint16_t freq) {
    g_loop_manager.metronome.config.regular_beat_freq = freq;
    DBG("Metronome regular beat frequency set to %d Hz\n", freq);
}

/**
 * @brief 设置节拍持续时间
 * @param duration_ms 节拍持续时间（毫秒）
 */
void metronome_set_beat_duration(uint16_t duration_ms) {
    g_loop_manager.metronome.config.beat_duration_ms = duration_ms;
    metronome_update_timing_params();
    DBG("Metronome beat duration set to %d ms\n", duration_ms);
}

/**
 * @brief 获取当前BPM
 * @return 当前BPM值
 */
uint16_t metronome_get_bpm(void) {
    return g_loop_manager.metronome.config.bpm;
}

/**
 * @brief 获取每小节拍数
 * @return 每小节拍数
 */
uint8_t metronome_get_beats_per_measure(void) {
    return g_loop_manager.metronome.config.beats_per_measure;
}

/**
 * @brief 获取当前音量
 * @return 当前音量系数
 */
float metronome_get_volume(void) {
    return g_loop_manager.metronome.config.volume;
}

/**
 * @brief 获取下拍频率
 * @return 下拍频率（Hz）
 */
uint16_t metronome_get_downbeat_freq(void) {
    return g_loop_manager.metronome.config.downbeat_freq;
}

/**
 * @brief 获取普通拍频率
 * @return 普通拍频率（Hz）
 */
uint16_t metronome_get_regular_beat_freq(void) {
    return g_loop_manager.metronome.config.regular_beat_freq;
}

/**
 * @brief 获取节拍持续时间
 * @return 节拍持续时间（毫秒）
 */
uint16_t metronome_get_beat_duration(void) {
    return g_loop_manager.metronome.config.beat_duration_ms;
}

/**
 * @brief 获取当前拍子索引
 * @return 当前拍子索引（0开始）
 */
uint8_t metronome_get_current_beat(void) {
    return g_loop_manager.metronome.current_beat;
}

/**
 * @brief 获取当前拍子类型
 * @return 当前拍子类型
 */
BeatType_t metronome_get_current_beat_type(void) {
    return g_loop_manager.metronome.current_beat_type;
}

/**
 * @brief 检查当前是否在播放节拍声音
 * @return 1如果正在播放节拍声音，0如果不是
 */
uint8_t metronome_is_beat_active(void) {
    return g_loop_manager.metronome.is_beat_active;
}

/**
 * @brief 处理节拍器音频输出（主要功能）
 * @param output_data 输出音频数据缓冲区（uint32_t格式，包含左右声道）
 * @param length 音频数据长度（样本数，不是字节数）
 */
void metronome_process_audio(uint32_t* output_data, uint16_t length) {
    if (!metronome_is_enabled() || output_data == NULL || length == 0) {
        return;
    }
    
    uint16_t i;
    for (i = 0; i < length; i++) {
        // 检查是否需要开始新的节拍
        if (g_loop_manager.metronome.sample_counter >= g_loop_manager.metronome.beat_interval_samples) {
            // 开始新的节拍
            g_loop_manager.metronome.is_beat_active = 1;
            g_loop_manager.metronome.beat_sample_counter = 0;
            g_loop_manager.metronome.sine_phase = 0.0f;
            
            // 确定拍子类型
            if (g_loop_manager.metronome.current_beat == 0) {
                g_loop_manager.metronome.current_beat_type = BEAT_TYPE_DOWNBEAT;
            } else {
                g_loop_manager.metronome.current_beat_type = BEAT_TYPE_REGULAR;
            }
            
            // 重置采样计数器
            g_loop_manager.metronome.sample_counter = 0;
            
            // 推进到下一拍
            metronome_advance_beat();
        }
        
        // 生成节拍声音样本
        int16_t sample = 0;
        if (g_loop_manager.metronome.is_beat_active) {
            // 选择频率
            float freq = (g_loop_manager.metronome.current_beat_type == BEAT_TYPE_DOWNBEAT) ? 
                         g_loop_manager.metronome.config.downbeat_freq : 
                         g_loop_manager.metronome.config.regular_beat_freq;
            
            // 生成正弦波样本
            float sine_sample = metronome_generate_sine_sample(freq, &g_loop_manager.metronome.sine_phase);
            
            // 应用音量和转换为16位整数
            sample = (int16_t)(sine_sample * g_loop_manager.metronome.config.volume * 32767.0f);
            
            // 更新节拍样本计数器
            g_loop_manager.metronome.beat_sample_counter++;
            
            // 检查节拍是否结束
            if (g_loop_manager.metronome.beat_sample_counter >= g_loop_manager.metronome.beat_duration_samples) {
                g_loop_manager.metronome.is_beat_active = 0;
            }
        }
        
        // 将样本混合到输出（假设uint32_t包含左右声道）
        // 提取当前左右声道
        int16_t left = (int16_t)(output_data[i] & 0xFFFF);
        int16_t right = (int16_t)((output_data[i] >> 16) & 0xFFFF);
        
        // 添加节拍器信号到两个声道
        left = (int16_t)(((int32_t)left + (int32_t)sample) / 2);  // 简单混合
        right = (int16_t)(((int32_t)right + (int32_t)sample) / 2);
        
        // 防止溢出
        if (left > 32767) left = 32767;
        if (left < -32768) left = -32768;
        if (right > 32767) right = 32767;
        if (right < -32768) right = -32768;
        
        // 重新打包
        output_data[i] = ((uint32_t)(uint16_t)right << 16) | (uint16_t)left;
        
        // 推进总采样计数器
        g_loop_manager.metronome.sample_counter++;
    }
}

/**
 * @brief 将节拍器音频混合到输出（替代接口）
 * @param output_data 输出音频数据缓冲区
 * @param length 音频数据长度
 */
void metronome_mix_audio(uint32_t* output_data, uint16_t length) {
    metronome_process_audio(output_data, length);
}

/**
 * @brief 更新计时参数（内部使用）
 */
static void metronome_update_timing_params(void) {
    // 计算节拍间隔（样本数）
    // BPM = 每分钟拍数，所以每拍间隔 = 60秒 / BPM
    // 样本数 = 间隔秒数 * 采样率
    uint32_t beat_interval_ms = (60000 / g_loop_manager.metronome.config.bpm);  // 毫秒
    g_loop_manager.metronome.beat_interval_samples = (beat_interval_ms * METRONOME_SAMPLE_RATE) / 1000;
    
    // 计算节拍持续时间（样本数）
    g_loop_manager.metronome.beat_duration_samples =
        (g_loop_manager.metronome.config.beat_duration_ms * METRONOME_SAMPLE_RATE) / 1000;
}

/**
 * @brief 生成正弦波样本（内部使用）
 * @param freq 频率（Hz）
 * @param phase 相位累积器指针
 * @return 正弦波样本值（-1.0到1.0）
 */
static float metronome_generate_sine_sample(float freq, float* phase) {
    float sample = sinf(*phase);
    *phase += 2.0f * M_PI * freq / METRONOME_SAMPLE_RATE;
    
    // 防止相位累积器溢出
    if (*phase >= 2.0f * M_PI) {
        *phase -= 2.0f * M_PI;
    }
    
    return sample;
}

/**
 * @brief 推进到下一拍（内部使用）
 */
static void metronome_advance_beat(void) {
    g_loop_manager.metronome.current_beat++;
    if (g_loop_manager.metronome.current_beat >= g_loop_manager.metronome.config.beats_per_measure) {
        g_loop_manager.metronome.current_beat = 0;
    }
}

// ============================================================================
// Looper 段音量控制
// ============================================================================

/**
 * @brief 设置指定段的播放音量
 * @param segment_index 段索引 (0-3)
 * @param volume 音量 0-100
 */
void loop_set_segment_volume(uint8_t segment_index, uint8_t volume)
{
    if (segment_index >= MAX_SEGMENTS) {
        DBG("[Looper] set_segment_volume: invalid index %d\n", segment_index);
        return;
    }
    if (volume > 100) volume = 100;
    g_loop_manager.segment_volume[segment_index] = volume;
    /* 持久化到SysParam */
    SYSPARAM_LOOPER()->segment_volume[segment_index] = volume;
    SysParam_Save();
    DBG("[Looper] Segment %d volume set to %d%%\n", segment_index, volume);
}

/**
 * @brief 获取指定段的播放音量
 * @param segment_index 段索引 (0-3)
 * @return 音量 0-100
 */
uint8_t loop_get_segment_volume(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) return 100;
    return g_loop_manager.segment_volume[segment_index];
}

// ============================================================================
// 段裁剪控制
// ============================================================================

/**
 * @brief 设置段的循环裁剪起止页
 * @param segment_index 段索引
 * @param start_page    循环起始页（0=从头播放）
 * @param end_page      循环终止页（0=到录制末尾）
 *
 * 说明：仅影响播放/预读范围，不修改Flash上的录制数据。
 *       若 end_page > length_pages 则自动钳制到 length_pages。
 */
void loop_set_segment_trim(uint8_t segment_index, uint32_t start_page, uint32_t end_page)
{
    if (segment_index >= MAX_SEGMENTS) {
        DBG("[Looper] set_segment_trim: invalid index %d\n", segment_index);
        return;
    }
    SegmentInfo_t *seg = &g_loop_manager.segments[segment_index];

    /* 边界钳制 */
    if (end_page > 0 && end_page > seg->length_pages) {
        end_page = seg->length_pages;
    }
    if (start_page > 0 && end_page > 0 && start_page >= end_page) {
        DBG("[Looper] set_segment_trim: start_page(%lu) >= end_page(%lu), ignored\n",
            (unsigned long)start_page, (unsigned long)end_page);
        return;
    }

    seg->trim_start_page = start_page;
    seg->trim_end_page   = end_page;

    /* 若当前正在播放此段，将播放指针钳制到新的有效范围 */
    if (seg->state == SEGMENT_PLAYING && seg->is_active) {
        uint32_t loop_start = start_page;
        uint32_t loop_end   = (end_page > 0) ? end_page : seg->length_pages;
        if (seg->play_position < loop_start || seg->play_position >= loop_end) {
            seg->play_position = loop_start;
#if LOOPER_IO_BUFFER_ENABLE
            {
                LooperReadCache_t *cache = &s_read_cache[segment_index];
                cache->prefetch_page = loop_start;
                cache->head = 0;
                cache->tail = 0;
                cache->active = 0;  /* 触发重新初始化 */
            }
#endif
        }
    }

    DBG("[Looper] Segment %d trim set: start=%lu end=%lu (length=%lu)\n",
        segment_index,
        (unsigned long)start_page, (unsigned long)end_page,
        (unsigned long)seg->length_pages);
}

/**
 * @brief 获取段当前的裁剪起止页
 */
void loop_get_segment_trim(uint8_t segment_index, uint32_t *start_page, uint32_t *end_page)
{
    if (segment_index >= MAX_SEGMENTS || !start_page || !end_page) return;
    *start_page = g_loop_manager.segments[segment_index].trim_start_page;
    *end_page   = g_loop_manager.segments[segment_index].trim_end_page;
}

// ============================================================================
// 段Flash绑定控制
// ============================================================================

/**
 * @brief 将指定段绑定到特定的Flash设备
 *
 * 必须在该段开始录制之前调用。绑定后，该段的所有录制写入和播放读取
 * 均通过对应Flash的CS引脚（硬件上的片选信号）访问该颗W25Q64。
 *
 * 典型用法（实现边录边放）：
 *   loop_set_segment_flash(0, 0);  // 段0 → Flash#0 (CS=GPIOA21)
 *   loop_set_segment_flash(1, 1);  // 段1 → Flash#1 (CS=GPIOA22)
 *   // 播放段0（读Flash#0）的同时录制段1（写Flash#1），两颗Flash互不干扰
 *
 * @param segment_index  段索引 (0 ~ MAX_SEGMENTS-1)
 * @param flash_dev_id   Flash设备号 (0 ~ LOOPER_FLASH_DEV_COUNT-1)
 */
#if LOOPER_MULTI_FLASH_ENABLE
void loop_set_segment_flash(uint8_t segment_index, uint8_t flash_dev_id)
{
    if (segment_index >= MAX_SEGMENTS) {
        DBG("[Looper] set_segment_flash: invalid segment index %d\n", segment_index);
        return;
    }
    if (flash_dev_id >= LOOPER_FLASH_DEV_COUNT) {
        DBG("[Looper] set_segment_flash: invalid flash_dev_id %d (max=%d)\n",
            flash_dev_id, LOOPER_FLASH_DEV_COUNT - 1);
        return;
    }
    g_loop_manager.segments[segment_index].flash_dev_id = flash_dev_id;
    DBG("[Looper] Segment %d bound to Flash dev%d (CS%d)\n",
        segment_index, flash_dev_id,
        (flash_dev_id == 0) ? FLASH0_CS_PIN : FLASH1_CS_PIN);
}

/**
 * @brief 获取指定段当前绑定的Flash设备号
 * @param segment_index 段索引
 * @return Flash设备号 (0 ~ LOOPER_FLASH_DEV_COUNT-1)
 */
uint8_t loop_get_segment_flash(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) return 0;
    return g_loop_manager.segments[segment_index].flash_dev_id;
}
#endif /* LOOPER_MULTI_FLASH_ENABLE */

// ============================================================================
// IO缓冲区管理函数 (LOOPER_IO_BUFFER_ENABLE)
// ============================================================================

#if LOOPER_IO_BUFFER_ENABLE

/**
 * @brief 初始化/重填指定段的播放读缓存（一次性阻塞预填全部槽位）
 *
 * 在段首次进入播放状态、循环回绕时调用。
 * 阻塞时间 ≈ LOOPER_READ_CACHE_PAGES × 0.1ms（SPI快读）
 *
 * @param segment_index 段索引 (0 ~ MAX_SEGMENTS-1)
 */
void looper_init_read_cache(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) return;
    if (!s_read_cache) return;

    SegmentInfo_t *seg = &g_loop_manager.segments[segment_index];
    LooperReadCache_t *cache = &s_read_cache[segment_index];

    /* 计算有效循环起止页 */
    uint32_t trim_start = seg->trim_start_page;
    uint32_t trim_end   = (seg->trim_end_page > 0 && seg->trim_end_page <= seg->length_pages)
                          ? seg->trim_end_page : seg->length_pages;
    uint32_t loop_pages = trim_end - trim_start;

    cache->head = 0;
    cache->tail = 0;
    cache->prefetch_page = seg->play_position;
    cache->active = 1;

    /* 预填全部可用槽位（留1空位区分满/空） */
    uint8_t fill = LOOPER_READ_CACHE_PAGES - 1;
    uint8_t i;

    if (seg->length_pages == 0 || loop_pages == 0) {
        cache->active = 0;
        return;
    }
    if ((uint32_t)fill > loop_pages) {
        fill = (uint8_t)loop_pages;
    }

    for (i = 0; i < fill; i++) {
        uint32_t offset = seg_page_to_addr(seg, cache->prefetch_page);
#if LOOPER_USE_STORAGE_ABSTRACTION
        /* 使用存储抽象层：读取失败时终止预填，避免缓存中存放无效数据 */
        if (LooperStorage_Read(&g_looper_storage, offset, cache->buf[cache->head],
                               LOOPER_PAGE_DATA_SIZE) != LOOPER_STORAGE_OK) {
            break;
        }
#else
        /* 使用传统 Flash API */
#if LOOPER_MULTI_FLASH_ENABLE
        FlashPartition_LooperReadByDev(seg->flash_dev_id, offset,
                                       cache->buf[cache->head],
                                       LOOPER_PAGE_DATA_SIZE);
#else
        FlashPartition_LooperRead(offset, cache->buf[cache->head],
                                  LOOPER_PAGE_DATA_SIZE);
#endif /* LOOPER_MULTI_FLASH_ENABLE */
#endif /* LOOPER_USE_STORAGE_ABSTRACTION */
        cache->head = (cache->head + 1) % LOOPER_READ_CACHE_PAGES;
        cache->prefetch_page++;
        if (cache->prefetch_page >= trim_end) {
            cache->prefetch_page = trim_start;
        }
    }

    DBG("[IOBuf] Read cache init seg%d: filled %d pages, prefetch@%lu\n",
        segment_index, fill, (unsigned long)cache->prefetch_page);
}

/**
 * @brief 将指定段的写缓冲全部刷入Flash（阻塞直到清空）
 *
 * 在录制停止/段状态切换前调用，确保所有录制数据落盘。
 *
 * @param segment_index 段索引
 */
void looper_flush_write_all(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) return;
    if (!s_write_ring) return;

    SegmentInfo_t *seg = &g_loop_manager.segments[segment_index];
    LooperWriteRing_t *ring = &s_write_ring[segment_index];

    while (wring_count(ring) > 0) {
        uint32_t write_offset = seg_page_to_addr(seg, ring->flush_page);
#if LOOPER_MULTI_FLASH_ENABLE
        FlashPartition_LooperWriteByDev(seg->flash_dev_id, write_offset,
                                        ring->buf[ring->tail],
                                        LOOPER_PAGE_DATA_SIZE);
#else
        FlashPartition_LooperWrite(write_offset, ring->buf[ring->tail],
                                   LOOPER_PAGE_DATA_SIZE);
#endif
        ring->tail = (ring->tail + 1) % LOOPER_WRITE_BUF_PAGES;
        ring->flush_page++;
    }

    DBG("[IOBuf] Write ring flushed seg%d: %lu pages total\n",
        segment_index, (unsigned long)ring->flush_page);
}

/**
 * @brief 每帧IO泵：刷写缓冲 + 填读缓存
 *
 * 每个音频帧在 DAC 输出之后调用一次。
 * 为每个活跃段最多执行 1次写 + 1次读，
 * 控制单次调用耗时 ≈ (0.8ms写 + 0.1ms读) × 活跃段数。
 */
void looper_flush_io(void)
{
    uint8_t i;

    /* ---- 写缓冲刷出（每段最多刷1页） ---- */
    for (i = 0; i < MAX_SEGMENTS; i++) {
        SegmentInfo_t *seg = &g_loop_manager.segments[i];
        LooperWriteRing_t *ring = &s_write_ring[i];

        if (seg->state != SEGMENT_RECORDING) continue;
        if (wring_count(ring) == 0) continue;

#if LOOPER_MULTI_FLASH_ENABLE
        if (looper_poll_erase_pending(seg->flash_dev_id)) continue;
#else
        if (g_loop_manager.chip_erase_pending) continue;
        /* 写地址所在 64KB 块尚未擦除时阻塞写入（erase-ahead 水位线检查）*/
#if !defined(LOOPER_STORAGE_TYPE) || (LOOPER_STORAGE_TYPE != LOOPER_STORAGE_TYPE_PSRAM)
        {
            uint32_t write_offset_chk = seg_page_to_addr(seg, (uint32_t)ring->flush_page);
            if (write_offset_chk >= s_partial_erase[i].erased_up_to) continue;
        }
#endif
#endif

        {
        LooperStorageStatus_t wr;
        uint32_t write_offset = seg_page_to_addr(seg, ring->flush_page);
#if LOOPER_USE_STORAGE_ABSTRACTION
        /* 使用存储抽象层 */
        wr = LooperStorage_Write(&g_looper_storage, write_offset, ring->buf[ring->tail], LOOPER_PAGE_DATA_SIZE);
#else
        /* 使用传统 Flash API */
#if LOOPER_MULTI_FLASH_ENABLE
        wr = (LooperStorageStatus_t)FlashPartition_LooperWriteByDev(seg->flash_dev_id, write_offset,
                                        ring->buf[ring->tail],
                                        LOOPER_PAGE_DATA_SIZE);
#else
        wr = (LooperStorageStatus_t)FlashPartition_LooperWrite(write_offset, ring->buf[ring->tail],
                                   LOOPER_PAGE_DATA_SIZE);
#endif /* LOOPER_MULTI_FLASH_ENABLE */
#endif /* LOOPER_USE_STORAGE_ABSTRACTION */
        /* 只有写入成功才推进指针，避免写失败时丢失数据 */
        if (wr == LOOPER_STORAGE_OK) {
            ring->tail = (ring->tail + 1) % LOOPER_WRITE_BUF_PAGES;
            ring->flush_page++;
        }
        }
    }

    /* ---- 读缓存预填（每段最多填1页） ---- */
    for (i = 0; i < MAX_SEGMENTS; i++) {
        SegmentInfo_t *seg = &g_loop_manager.segments[i];
        LooperReadCache_t *cache = &s_read_cache[i];

        if (seg->state != SEGMENT_PLAYING || !cache->active) continue;
        if (rcache_space(cache) == 0) continue;

#if LOOPER_MULTI_FLASH_ENABLE
        if (looper_poll_erase_pending(seg->flash_dev_id)) continue;
#else
        if (g_loop_manager.chip_erase_pending) continue;
        /* 读取的是已录制数据，不需要检查擦除状态 */
#endif

        {
        LooperStorageStatus_t rr;
        uint32_t read_offset = seg_page_to_addr(seg, cache->prefetch_page);
#if LOOPER_USE_STORAGE_ABSTRACTION
        /* 使用存储抽象层 */
        rr = LooperStorage_Read(&g_looper_storage, read_offset, cache->buf[cache->head], LOOPER_PAGE_DATA_SIZE);
#else
        /* 使用传统 Flash API */
#if LOOPER_MULTI_FLASH_ENABLE
        rr = (LooperStorageStatus_t)FlashPartition_LooperReadByDev(seg->flash_dev_id, read_offset,
                                       cache->buf[cache->head],
                                       LOOPER_PAGE_DATA_SIZE);
#else
        rr = (LooperStorageStatus_t)FlashPartition_LooperRead(read_offset, cache->buf[cache->head],
                                  LOOPER_PAGE_DATA_SIZE);
#endif /* LOOPER_MULTI_FLASH_ENABLE */
#endif /* LOOPER_USE_STORAGE_ABSTRACTION */
        /* 只有读取成功才推进指针，避免缓存中填入无效数据 */
        if (rr == LOOPER_STORAGE_OK) {
            cache->head = (cache->head + 1) % LOOPER_READ_CACHE_PAGES;
            cache->prefetch_page++;
            {
                uint32_t te = (seg->trim_end_page > 0 && seg->trim_end_page <= seg->length_pages)
                              ? seg->trim_end_page : seg->length_pages;
                if (cache->prefetch_page >= te) {
                    cache->prefetch_page = seg->trim_start_page;
                }
            }
        }
        }
    }

    /* ---- 局部擦除状态机推进（每帧最多推进一步） ---- */
    {
        uint8_t seg_idx;
        for (seg_idx = 0; seg_idx < MAX_SEGMENTS; seg_idx++) {
            if (s_partial_erase[seg_idx].pending) {
                looper_advance_partial_erase(seg_idx);
                break;  /* 每帧只推进一个段，减少 CPU 占用 */
            }
        }
    }
}

#endif /* LOOPER_IO_BUFFER_ENABLE */

// ============================================================================
// Looper Flash 状态管理
// ============================================================================

/**
 * @brief 开机检查Looper Flash是否已初始化（全片擦除过）
 *
 * 读取SysParam中的flash_status标志：
 *   - LOOPER_FLASH_STATUS_CLEAN: Flash已全片擦除，直接使用
 *   - LOOPER_FLASH_STATUS_USED : Flash已被使用，触发全片擦除后再使用
 *
 * 应在loop_init()/loop_init_with_flash_type()末尾调用。
 */
void loop_check_flash_init_on_boot(void)
{
    uint8_t status;

    /* 如果SysParam尚未初始化（magic不匹配），视为USED（安全起见需擦除） */
    if (SysParam_Get()->magic != SYS_PARAM_MAGIC) {
        status = LOOPER_FLASH_STATUS_USED;
        DBG("[Looper] Boot: SysParam not initialized, treating flash as USED\n");
    } else {
        status = SYSPARAM_LOOPER()->flash_status;
        DBG("[Looper] Boot: flash_status = %d (%s)\n",
            status,
            status == LOOPER_FLASH_STATUS_CLEAN ? "CLEAN" : "USED/UNKNOWN");
    }

    if (status != LOOPER_FLASH_STATUS_CLEAN) {
#if LOOPER_MULTI_FLASH_ENABLE
        /* Flash曾被使用过，或初次上电（尚无有效参数），并行擦除所有 Looper Flash */
        g_loop_manager.chip_erase_pending_mask = 0;
        for (dev = 0; dev < LOOPER_FLASH_DEV_COUNT; dev++) {
            FlashStatus_t ret = FlashPartition_LooperEraseChipAsyncByDev(dev);
            if (ret == FLASH_OK) {
                g_loop_manager.chip_erase_pending_mask |= (uint8_t)(1u << dev);
            } else {
                DBG("[Looper] Boot: dev%d chip erase failed (%d)\n", dev, ret);
            }
        }
        if (g_loop_manager.chip_erase_pending_mask) {
            DBG("[Looper] Boot: chip erase started (mask=0x%02X), REC/PLAY blocked\n",
                g_loop_manager.chip_erase_pending_mask);
        }
        /* CLEAN标志待擦除完成后由轮询代码写入 */
#else
        /* 单Flash模式：擦除 Flash#0 */
        {
            FlashStatus_t ret2 = FlashPartition_LooperEraseChipAsync();
            if (ret2 == FLASH_OK) {
                g_loop_manager.chip_erase_pending = 1;
                DBG("[Looper] Boot: chip erase started, REC/PLAY blocked\n");
            } else {
                DBG("[Looper] Boot: chip erase failed (%d)\n", ret2);
            }
        }
#endif /* LOOPER_MULTI_FLASH_ENABLE */
    } else {
        DBG("[Looper] Boot: flash is CLEAN, no erase needed\n");
    }
}

/**
 * @brief 退出Looper界面时调用
 *
 * 若Looper Flash已被录音使用（flash_status = USED），则触发全片异步擦除，
 * 擦除完成后自动将flash_status更新为CLEAN并写入SysParam。
 */
void loop_on_app_exit(void)
{
    uint8_t status;
    uint8_t i;
    FlashStatus_t ret;

    status = SYSPARAM_LOOPER()->flash_status;
    DBG("[Looper] App exit: flash_status = %d\n", status);

    if (status == LOOPER_FLASH_STATUS_USED) {
        /* 停止所有活动 */
        g_loop_manager.state = LOOP_STATE_IDLE;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            g_loop_manager.segments[i].state    = SEGMENT_INACTIVE;
            g_loop_manager.segments[i].is_active = 0;
            g_loop_manager.segments[i].length_pages = 0;
            g_loop_manager.segments[i].length_bytes = 0;
            g_loop_manager.segments[i].play_position = 0;
            g_loop_manager.segments[i].rec_partial_count = 0;
            g_loop_manager.segments[i].play_page_offset  = 0;
            g_loop_manager.segments[i].play_page_valid   = 0;
        }
        g_loop_manager.active_segments = 0;
        g_loop_manager.sector_address  = 0;
        g_loop_manager.record_length   = 0;
        g_loop_manager.play_position   = 0;

#if LOOPER_USE_STORAGE_ABSTRACTION
        /* PSRAM 是 volatile，掉电即失，无需擦除。只需更新状态位并设置就绪。 */
        if (g_looper_storage.initialized &&
            g_looper_storage.info.type == LOOPER_STORAGE_PSRAM) {
            SYSPARAM_LOOPER()->flash_status = LOOPER_FLASH_STATUS_CLEAN;
            SysParam_Save();
            g_loop_manager.storage_ready = 1;
            DBG("[Looper] App exit: PSRAM is volatile, marked CLEAN immediately\n");
            return;
        }
#endif /* LOOPER_USE_STORAGE_ABSTRACTION */

#if LOOPER_MULTI_FLASH_ENABLE
        /* 并行异步擦除所有 Looper Flash，完成后由轮询线程保存CLEAN */
        g_loop_manager.chip_erase_pending_mask = 0;
        for (dev = 0; dev < LOOPER_FLASH_DEV_COUNT; dev++) {
            ret = FlashPartition_LooperEraseChipAsyncByDev(dev);
            if (ret == FLASH_OK) {
                g_loop_manager.chip_erase_pending_mask |= (uint8_t)(1u << dev);
            } else {
                DBG("[Looper] App exit: dev%d chip erase failed (%d)\n", dev, ret);
            }
        }
        if (g_loop_manager.chip_erase_pending_mask) {
            DBG("[Looper] App exit: chip erase started (mask=0x%02X, flash was USED)\n",
                g_loop_manager.chip_erase_pending_mask);
        }
#else
        /* 单Flash模式：异步擦除 Flash#0 */
        ret = FlashPartition_LooperEraseChipAsync();
        if (ret == FLASH_OK) {
            g_loop_manager.chip_erase_pending = 1;
            DBG("[Looper] App exit: chip erase started (flash was USED)\n");
        } else {
            DBG("[Looper] App exit: chip erase failed (%d)\n", ret);
        }
#endif /* LOOPER_MULTI_FLASH_ENABLE */
    } else {
        DBG("[Looper] App exit: flash is CLEAN, no erase needed\n");
    }
}

// ============================================================================
// AudioLooper接口函数（段音量 & Flash生命周期 & Flash绑定）
// ============================================================================

static void AudioLooper_SetSegmentVolume(uint8_t segment_index, uint8_t volume) {
    loop_set_segment_volume(segment_index, volume);
}

static uint8_t AudioLooper_GetSegmentVolume(uint8_t segment_index) {
    return loop_get_segment_volume(segment_index);
}

static void AudioLooper_SetSegmentTrim(uint8_t segment_index, uint32_t start_page, uint32_t end_page) {
    loop_set_segment_trim(segment_index, start_page, end_page);
}

static void AudioLooper_GetSegmentTrim(uint8_t segment_index, uint32_t *start_page, uint32_t *end_page) {
    loop_get_segment_trim(segment_index, start_page, end_page);
}

/* ============================================================================
 * 段录制源控制
 * ============================================================================ */
void loop_set_segment_rec_source(uint8_t segment_index, uint8_t source)
{
    if (segment_index >= MAX_SEGMENTS) return;
    if (source > LOOP_REC_SRC_ALL_MIX) source = LOOP_REC_SRC_ALL_MIX;
    g_loop_manager.segments[segment_index].rec_source = source;
    SYSPARAM_LOOPER()->segment_rec_source[segment_index] = source;
    SysParam_Save();
}

uint8_t loop_get_segment_rec_source(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) return LOOP_REC_SRC_ALL_MIX;
    return g_loop_manager.segments[segment_index].rec_source;
}

uint8_t loop_is_segment_mono(uint8_t segment_index)
{
    if (segment_index >= MAX_SEGMENTS) return 0;
    return LOOP_REC_SRC_IS_MONO(g_loop_manager.segments[segment_index].rec_source);
}

static void AudioLooper_SetSegmentRecSource(uint8_t segment_index, uint8_t source) {
    loop_set_segment_rec_source(segment_index, source);
}
static uint8_t AudioLooper_GetSegmentRecSource(uint8_t segment_index) {
    return loop_get_segment_rec_source(segment_index);
}

#if LOOPER_MULTI_FLASH_ENABLE
static void AudioLooper_SetSegmentFlash(uint8_t segment_index, uint8_t flash_dev_id) {
    loop_set_segment_flash(segment_index, flash_dev_id);
}

static uint8_t AudioLooper_GetSegmentFlash(uint8_t segment_index) {
    return loop_get_segment_flash(segment_index);
}
#endif /* LOOPER_MULTI_FLASH_ENABLE */

static void AudioLooper_CheckFlashInitOnBoot(void) {
    loop_check_flash_init_on_boot();
}

static void AudioLooper_OnAppExit(void) {
    loop_on_app_exit();
}

#if LOOPER_IO_BUFFER_ENABLE
static void AudioLooper_FlushIO(void) {
    looper_flush_io();
}
#endif

// ============================================================================
// AudioLooper接口函数（模式控制）
// ============================================================================

/**
 * @brief AudioLooper接口：设置循环模式
 */
static void AudioLooper_SetMode(LoopMode_t mode) {
    loop_set_mode(mode);
}

/**
 * @brief AudioLooper接口：获取当前循环模式
 */
static LoopMode_t AudioLooper_GetMode(void) {
    return loop_get_mode();
}

/**
 * @brief AudioLooper接口：检查是否为歌曲模式
 */
static uint8_t AudioLooper_IsSongMode(void) {
    return loop_is_song_mode();
}

/**
 * @brief AudioLooper接口：检查是否为自由模式
 */
static uint8_t AudioLooper_IsFreeMode(void) {
    return loop_is_free_mode();
}

/**
 * @brief AudioLooper接口：切换节拍器开关
 */
static void AudioLooper_MetronomeToggle(void) {
    metronome_toggle();
}

/**
 * @brief AudioLooper接口：设置BPM
 */
static void AudioLooper_MetronomeSetBPM(uint16_t bpm) {
    metronome_set_bpm(bpm);
}

/**
 * @brief AudioLooper接口：设置每小节拍数
 */
static void AudioLooper_MetronomeSetBeatsPerMeasure(uint8_t beats) {
    metronome_set_beats_per_measure(beats);
}

/**
 * @brief AudioLooper接口：设置节拍器音量
 */
static void AudioLooper_MetronomeSetVolume(float volume) {
    metronome_set_volume(volume);
}

/**
 * @brief AudioLooper接口：检查节拍器是否开启
 */
static uint8_t AudioLooper_MetronomeIsEnabled(void) {
    return metronome_is_enabled();
}

/**
 * @brief AudioLooper接口：获取当前BPM
 */
static uint16_t AudioLooper_MetronomeGetBPM(void) {
    return metronome_get_bpm();
}

/**
 * @brief AudioLooper接口：获取每小节拍数
 */
static uint8_t AudioLooper_MetronomeGetBeatsPerMeasure(void) {
    return metronome_get_beats_per_measure();
}

// ============================================================================
// 叠录功能接口前向声明
// ============================================================================

uint8_t AudioLooper_IsOverdubSupported(void);
void AudioLooper_SetOverdubMode(uint8_t segment_index, uint8_t enabled);
uint8_t AudioLooper_GetOverdubMode(uint8_t segment_index);
void AudioLooper_SetOverdubMixMode(uint8_t mix_mode);
uint8_t AudioLooper_GetOverdubMixMode(void);

// ============================================================================
// 全局AudioLooper接口实例（类似BG_flash_manager）
// ============================================================================
AudioLooper_t AudioLooper __attribute__((section(".data"))) = {
    .Init = AudioLooper_Init,
    .InitWithFlashType = AudioLooper_InitWithFlashType,
    .Reset = AudioLooper_Reset,
    .SetFlashType = AudioLooper_SetFlashType,
    .ButtonPress = AudioLooper_ButtonPress,
    .SegmentButtonPress = AudioLooper_SegmentButtonPress,
    .EncoderLeft = AudioLooper_EncoderLeft,
    .EncoderRight = AudioLooper_EncoderRight,
    .StopRecording = AudioLooper_StopRecording,
    .ProcessRecording = AudioLooper_ProcessRecording,
    .ProcessPlayback = AudioLooper_ProcessPlayback,
    .ProcessRecording32 = AudioLooper_ProcessRecording32,
    .ProcessPlayback32 = AudioLooper_ProcessPlayback32,
    .GetStatus = AudioLooper_GetStatus,
    .IsRecording = AudioLooper_IsRecording,
    .IsPlaying = AudioLooper_IsPlaying,
    .GetCurrentAddress = AudioLooper_GetCurrentAddress,
    .GetRecordLength = AudioLooper_GetRecordLength,
    .TimerUpdate = AudioLooper_TimerUpdate,
    
    // 模式控制
    .SetMode = AudioLooper_SetMode,
    .GetMode = AudioLooper_GetMode,
    .IsSongMode = AudioLooper_IsSongMode,
    .IsFreeMode = AudioLooper_IsFreeMode,
    
    // 节拍器控制
    .MetronomeToggle = AudioLooper_MetronomeToggle,
    .MetronomeSetBPM = AudioLooper_MetronomeSetBPM,
    .MetronomeSetBeatsPerMeasure = AudioLooper_MetronomeSetBeatsPerMeasure,
    .MetronomeSetVolume = AudioLooper_MetronomeSetVolume,
    .MetronomeIsEnabled = AudioLooper_MetronomeIsEnabled,
    .MetronomeGetBPM = AudioLooper_MetronomeGetBPM,
    .MetronomeGetBeatsPerMeasure = AudioLooper_MetronomeGetBeatsPerMeasure,

    // 段音量控制
    .SetSegmentVolume = AudioLooper_SetSegmentVolume,
    .GetSegmentVolume = AudioLooper_GetSegmentVolume,

    // 段裁剪控制
    .SetSegmentTrim = AudioLooper_SetSegmentTrim,
    .GetSegmentTrim = AudioLooper_GetSegmentTrim,

    // 段录制源控制
    .SetSegmentRecSource = AudioLooper_SetSegmentRecSource,
    .GetSegmentRecSource = AudioLooper_GetSegmentRecSource,

#if LOOPER_MULTI_FLASH_ENABLE
    // 段Flash绑定控制
    .SetSegmentFlash  = AudioLooper_SetSegmentFlash,
    .GetSegmentFlash  = AudioLooper_GetSegmentFlash,
#endif /* LOOPER_MULTI_FLASH_ENABLE */

    // Flash生命周期管理
    .CheckFlashInitOnBoot = AudioLooper_CheckFlashInitOnBoot,
    .OnAppExit = AudioLooper_OnAppExit,

    // 叠录功能支持
    .IsOverdubSupported = AudioLooper_IsOverdubSupported,
    .SetOverdubMode = AudioLooper_SetOverdubMode,
    .GetOverdubMode = AudioLooper_GetOverdubMode,
    .SetOverdubMixMode = AudioLooper_SetOverdubMixMode,
    .GetOverdubMixMode = AudioLooper_GetOverdubMixMode,

#if LOOPER_IO_BUFFER_ENABLE
    // IO缓冲区刷新
    .FlushIO = AudioLooper_FlushIO,
#endif
};

/* ============================================================================
 * AudioLooper 接口函数实现 - 叠录功能
 * ============================================================================ */

/**
 * @brief 检查是否支持叠录
 */
uint8_t AudioLooper_IsOverdubSupported(void)
{
    return loop_is_overdub_supported();
}

/**
 * @brief 设置段叠录模式
 */
void AudioLooper_SetOverdubMode(uint8_t segment_index, uint8_t enabled)
{
    loop_set_overdub_mode(segment_index, enabled);
}

/**
 * @brief 获取段叠录模式
 */
uint8_t AudioLooper_GetOverdubMode(uint8_t segment_index)
{
    return loop_get_overdub_mode(segment_index);
}

/**
 * @brief 设置叠录混音模式
 */
void AudioLooper_SetOverdubMixMode(uint8_t mix_mode)
{
    loop_set_overdub_mix_mode(mix_mode);
}

/**
 * @brief 获取叠录混音模式
 */
uint8_t AudioLooper_GetOverdubMixMode(void)
{
    return loop_get_overdub_mix_mode();
}

/* ============================================================================
 * 定时器驱动的异步 I/O 实现（解决 PSRAM 总线争用噪声）
 *
 * 使用硬件定时器 Timer2Interrupt (1ms) 驱动，每次 tick 执行一次 I/O
 * ============================================================================ */

#if LOOPER_IO_BUFFER_ENABLE

/* 定时器驱动状态 */
static volatile uint8_t  s_looper_io_timer_active = 0;   /* 定时器是否激活 */

/**
 * @brief Looper I/O 定时器 tick 函数（在 Timer2Interrupt 中调用）
 *
 * 每 1ms 被硬件定时器中断调用一次，立即执行 I/O 操作。
 * 在定时器中断上下文中执行，与音频主循环完全隔离。
 */
void LooperIO_TimerTick(void)
{
    if (!s_looper_io_timer_active) {
        return;  /* 未激活，直接返回 */
    }
    
    /* 每次tick都执行I/O（1ms间隔足够，不会过载） */
    looper_flush_io();
    
    /* 调试：每1000次打印一次状态 */
    static uint32_t dbg_cnt = 0;
    if (++dbg_cnt >= 1000) {
        dbg_cnt = 0;
        DBG("[LooperIO] Tick OK, active=%d\n", s_looper_io_timer_active);
    }
}

/**
 * @brief 初始化 Looper I/O 定时器驱动系统
 *
 * 仅初始化状态变量，实际驱动由硬件定时器 Timer2Interrupt 提供。
 * 需要在 Timer2Interrupt 中手动添加 LooperIO_TimerTick() 调用。
 */
void LooperIO_TimerInit(void)
{
    s_looper_io_timer_active = 0;
    
    DBG("[LooperIO] Timer driver initialized (hw: Timer2, 1ms/tick)\n");
}

/**
 * @brief 启动 Looper I/O 定时器
 *
 * 当录制或播放开始时调用，允许定时器 tick 执行 I/O 操作。
 */
void LooperIO_TimerStart(void)
{
    s_looper_io_timer_active = 1;
}

/**
 * @brief 停止 Looper I/O 定时器
 *
 * 当所有录制和播放停止时调用。
 * 停止后立即执行最后一次 flush，确保缓冲区数据落盘。
 */
void LooperIO_TimerStop(void)
{
    s_looper_io_timer_active = 0;
    
    /* 立即执行最后一次 flush，确保数据完整性 */
    looper_flush_io();
}

/**
 * @brief 紧急 flush 写缓冲区（在音频回调后调用）
 *
 * 当定时器驱动 I/O 不足时（缓冲区快满），由音频循环紧急调用此函数。
 * 只刷写写缓冲区，不执行读缓存预填和擦除，减少耗时。
 *
 * 设计原则：
 * - 尽量快速完成，避免影响下一帧音频
 * - 每次最多刷 2 页，控制单次耗时 < 0.5ms
 */
void looper_emergency_flush(void)
{
    uint8_t i;
    static uint32_t emergency_cnt = 0;
    
    for (i = 0; i < MAX_SEGMENTS; i++) {
        SegmentInfo_t *seg = &g_loop_manager.segments[i];
        LooperWriteRing_t *ring = &s_write_ring[i];

        if (seg->state != SEGMENT_RECORDING) continue;
        if (wring_count(ring) == 0) continue;

#if !defined(LOOPER_STORAGE_TYPE) || (LOOPER_STORAGE_TYPE != LOOPER_STORAGE_TYPE_PSRAM)
        /* Flash 模式：检查擦除状态 */
#if LOOPER_MULTI_FLASH_ENABLE
        if (looper_poll_erase_pending(seg->flash_dev_id)) continue;
#else
        if (g_loop_manager.chip_erase_pending) continue;
        /* 写地址所在块尚未擦除时阻塞写入（erase-ahead 水位线检查）*/
        {
            uint32_t write_offset_ea = seg_page_to_addr(seg, (uint32_t)ring->flush_page);
            if (write_offset_ea >= s_partial_erase[i].erased_up_to) continue;
        }
#endif
#endif

        {
            LooperStorageStatus_t wr;
            uint32_t write_offset = seg_page_to_addr(seg, ring->flush_page);

#if LOOPER_USE_STORAGE_ABSTRACTION
            wr = LooperStorage_Write(&g_looper_storage, write_offset, ring->buf[ring->tail], LOOPER_PAGE_DATA_SIZE);
#else
#if LOOPER_MULTI_FLASH_ENABLE
            wr = (LooperStorageStatus_t)FlashPartition_LooperWriteByDev(seg->flash_dev_id, write_offset,
                                    ring->buf[ring->tail],
                                    LOOPER_PAGE_DATA_SIZE);
#else
            wr = (LooperStorageStatus_t)FlashPartition_LooperWrite(write_offset, ring->buf[ring->tail],
                                       LOOPER_PAGE_DATA_SIZE);
#endif
#endif

            if (wr == LOOPER_STORAGE_OK) {
                ring->tail = (ring->tail + 1) % LOOPER_WRITE_BUF_PAGES;
                ring->flush_page++;
                emergency_cnt++;
                
                /* 调试：每100次打印 */
                if (emergency_cnt % 100 == 1) {
                    DBG("[LooperIO] Emergency flush #%lu seg%d ok\n",
                        (unsigned long)emergency_cnt, i);
                }
            } else {
                /* 写入失败：打印错误但不阻塞 */
                static uint32_t err_cnt = 0;
                if (++err_cnt % 200 == 1) {
                    DBG("[LooperIO] Emergency flush err #%lu seg%d: status=%d\n",
                        (unsigned long)err_cnt, i, wr);
                }
            }
            
            /* 每次紧急 flush 最多处理 1 个段，避免超时 */
            break;
        }
    }
}
#endif /* LOOPER_IO_BUFFER_ENABLE */
