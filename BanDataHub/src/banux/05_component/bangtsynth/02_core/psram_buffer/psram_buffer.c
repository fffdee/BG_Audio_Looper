/**
 * @file psram_buffer.c
 * @brief PSRAM 音符缓冲区管理器实现
 *
 * 实现 PSRAM 中的音符数据缓冲区管理，支持 LRU 缓存、多线程并发访问�?
 */

#include "product_def.h"

#if SYNTH_SD_NAND_PSRAM_EN

#include "psram_buffer.h"
#include "flash_devices.h"
#ifndef BANDATAHUB
#include "nand_store.h"
#endif
#include "synth_sdnandpsram.h"
#include "bg_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* OSAL 抽象�?(替代直接 FreeRTOS 依赖) */
#include "bg_osal.h"

/* ============================================
 * 内部常量定义
 * ============================================ */

/** 缓冲区表大小 (存储在PSRAM末尾2MB�? */
#define PSRAM_BUFFER_TABLE_SIZE       (PSRAM_MAX_NOTE_BUFFERS * sizeof(PSRAM_BufferInfo_t))

/** DMA传输块大�?(4KB) */
#define PSRAM_DMA_CHUNK_SIZE          4096

/** 加载超时时间 (ms) */
#define PSRAM_LOAD_TIMEOUT_MS         5000

/** 垃圾回收阈�?(空闲缓冲区少于此值时触发GC) */
#define PSRAM_GC_THRESHOLD            8

/* ============================================
 * 内部数据结构
 * ============================================ */

/**
 * PSRAM 缓冲区管理器私有状�?
 */
typedef struct {
    PSRAM_BufferManager_t public_state;    /* 公共状�?*/
    bg_mutex_t mutex;                      /* 互斥�?*/
    bg_task_t  loader_task;                /* 数据加载任务 */
    bg_queue_t load_queue;                 /* 加载请求队列 */
} PSRAM_PrivateState_t;

/**
 * 数据加载请求
 */
typedef struct {
    uint32_t buffer_id;            /* 目标缓冲区ID */
    uint32_t nand_offset;          /* NAND数据偏移 */
    uint32_t data_size;            /* 数据大小 */
    uint32_t checksum;             /* 期望校验�?*/
} PSRAM_LoadRequest_t;

/* ============================================
 * 全局变量
 * ============================================ */

static PSRAM_PrivateState_t g_psram_state = {
    .public_state = {
        .initialized = false,
        .buffer_table = NULL,
        .next_timestamp = 0,
        .total_buffers = PSRAM_MAX_NOTE_BUFFERS,
        .free_buffers = PSRAM_MAX_NOTE_BUFFERS,
        .loading_buffers = 0,
        .ready_buffers = 0,
        .playing_buffers = 0
    },
    .mutex = NULL,
    .loader_task = NULL,
    .load_queue = NULL
};

/* ============================================
 * 内部函数声明
 * ============================================ */

static BG_ERR psram_allocate_buffer(uint32_t *buffer_id);
static BG_ERR psram_find_lru_buffer(uint32_t *buffer_id);
static BG_ERR psram_evict_buffer(uint32_t buffer_id);
static void psram_update_stats(void);
static uint32_t psram_calculate_checksum(const void *data, uint32_t size);
static void psram_loader_task(void *param);
static BG_ERR psram_load_data_chunk(uint32_t buffer_id, uint32_t nand_offset,
                                   uint32_t psram_offset, uint32_t size);

/* ============================================
 * 公共接口实现
 * ============================================ */

BG_ERR PSRAM_BufferInit(void)
{
    BG_ERR ret;
    FlashDevice_t *psram_dev;
    uint32_t i;

    if (g_psram_state.public_state.initialized) {
        BG_LOG_W(BG_LOG_TAG_PSRAM, "PSRAM buffer manager already initialized");
        return SUCCESS;
    }

    /* 获取 PSRAM 设备 */
    psram_dev = FlashDevices_GetPsramFlash();
    if (!psram_dev) {
        BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to get PSRAM device");
        return ENABLE_INVALID_INPUT;
    }

    /* 分配缓冲区信息表内存 */
    g_psram_state.public_state.buffer_table = (PSRAM_BufferInfo_t *)malloc(PSRAM_BUFFER_TABLE_SIZE);
    if (!g_psram_state.public_state.buffer_table) {
        BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to allocate buffer table");
        return ENABLE_OUT_OF_MEMORY;
    }

    /* 初始化缓冲区信息�?*/
    memset(g_psram_state.public_state.buffer_table, 0, PSRAM_BUFFER_TABLE_SIZE);
    for (i = 0; i < PSRAM_MAX_NOTE_BUFFERS; i++) {
        PSRAM_BufferInfo_t *buffer = &g_psram_state.public_state.buffer_table[i];
        buffer->buffer_id = i;
        buffer->address = PSRAM_BUFFER_POOL_START + i * PSRAM_NOTE_BUFFER_SIZE;
        buffer->size = PSRAM_NOTE_BUFFER_SIZE;
        buffer->state = PSRAM_BUFFER_FREE;
        buffer->note_number = 0xFF;
        buffer->program = 0xFF;
    }

    /* 创建互斥�?*/
    g_psram_state.mutex = bg_mutex_create();
    if (!g_psram_state.mutex) {
        BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to create mutex");
        free(g_psram_state.public_state.buffer_table);
        return ENABLE_INVALID_INPUT;
    }

    /* 创建加载请求队列 */
    g_psram_state.load_queue = bg_queue_create(16, sizeof(PSRAM_LoadRequest_t));
    if (!g_psram_state.load_queue) {
        BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to create load queue");
        bg_mutex_delete(g_psram_state.mutex);
        free(g_psram_state.public_state.buffer_table);
        return ENABLE_INVALID_INPUT;
    }

    /* 创建数据加载任务 */
    if (bg_task_create(psram_loader_task, "PSRAM_Loader",
                       2048, NULL, 2, &g_psram_state.loader_task) != BG_OSAL_OK) {
        BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to create loader task");
        bg_queue_delete(g_psram_state.load_queue);
        bg_mutex_delete(g_psram_state.mutex);
        free(g_psram_state.public_state.buffer_table);
        return ENABLE_INVALID_INPUT;
    }

    g_psram_state.public_state.initialized = true;

    BG_LOG_I(BG_LOG_TAG_PSRAM, "PSRAM buffer manager initialized");
    BG_LOG_I(BG_LOG_TAG_PSRAM, "  Total buffers: %u", PSRAM_MAX_NOTE_BUFFERS);
    BG_LOG_I(BG_LOG_TAG_PSRAM, "  Buffer size: %u KB each", PSRAM_NOTE_BUFFER_SIZE / 1024);

    return SUCCESS;
}

void PSRAM_BufferDeInit(void)
{
    if (!g_psram_state.public_state.initialized) {
        return;
    }

    /* 删除加载任务 */
    if (g_psram_state.loader_task) {
        bg_task_delete(g_psram_state.loader_task);
        g_psram_state.loader_task = NULL;
    }

    /* 删除队列和互斥锁 */
    if (g_psram_state.load_queue) {
        bg_queue_delete(g_psram_state.load_queue);
        g_psram_state.load_queue = NULL;
    }

    if (g_psram_state.mutex) {
        bg_mutex_delete(g_psram_state.mutex);
        g_psram_state.mutex = NULL;
    }

    /* 释放内存 */
    free(g_psram_state.public_state.buffer_table);
    g_psram_state.public_state.buffer_table = NULL;

    memset(&g_psram_state, 0, sizeof(PSRAM_PrivateState_t));

    BG_LOG_I(BG_LOG_TAG_PSRAM, "PSRAM buffer manager deinitialized");
}

BG_ERR PSRAM_RequestNoteBuffer(const PSRAM_NoteRequest_t *request,
                              PSRAM_BufferAlloc_t *alloc_result)
{
    BG_ERR ret = SUCCESS;
    uint32_t buffer_id;
    uint32_t i;
    PSRAM_BufferInfo_t *buffer;
    bool from_cache = false;

    if (!g_psram_state.public_state.initialized || !request || !alloc_result) {
        return ENABLE_INVALID_INPUT;
    }

    /* 获取互斥�?*/
    if (bg_mutex_lock(g_psram_state.mutex, BG_OSAL_WAIT_FOREVER) != BG_OSAL_OK) {
        return ENABLE_INVALID_INPUT;
    }

    /* 先检查缓存中是否已有相同音符 */
    for (i = 0; i < PSRAM_MAX_NOTE_BUFFERS; i++) {
        buffer = &g_psram_state.public_state.buffer_table[i];
        if (buffer->state == PSRAM_BUFFER_READY &&
            buffer->note_number == request->note &&
            buffer->program == request->program) {
            /* 找到缓存的音�?*/
            buffer_id = i;
            from_cache = true;
            /* 直接更新时间戳，避免持锁时重�?*/
            buffer->last_access = g_psram_state.public_state.next_timestamp++;
            buffer->access_count++;
            break;
        }
    }

    /* 如果缓存中没有，分配新缓冲区 */
    if (!from_cache) {
        ret = psram_allocate_buffer(&buffer_id);
        if (ret != SUCCESS) {
            bg_mutex_unlock(g_psram_state.mutex);
            return ret;
        }

        buffer = &g_psram_state.public_state.buffer_table[buffer_id];
        buffer->note_number = request->note;
        buffer->program = request->program;
        buffer->sample_rate = request->sample_rate;
        buffer->state = PSRAM_BUFFER_LOADING;
        psram_update_stats();
    }

    /* 填充分配结果 */
    alloc_result->buffer_id = buffer_id;
    alloc_result->address = buffer->address;
    alloc_result->size = buffer->size;
    alloc_result->from_cache = from_cache;

    bg_mutex_unlock(g_psram_state.mutex);

    BG_LOG_I(BG_LOG_TAG_PSRAM, "Note buffer allocated: id=%u, note=%u, program=%u, cache=%d",
             buffer_id, request->note, request->program, from_cache);

    return ret;
}

BG_ERR PSRAM_ReleaseNoteBuffer(uint32_t buffer_id)
{
    PSRAM_BufferInfo_t *buffer;

    if (!g_psram_state.public_state.initialized || buffer_id >= PSRAM_MAX_NOTE_BUFFERS) {
        return ENABLE_INVALID_INPUT;
    }

    /* 获取互斥�?*/
    if (bg_mutex_lock(g_psram_state.mutex, BG_OSAL_WAIT_FOREVER) != BG_OSAL_OK) {
        return ENABLE_INVALID_INPUT;
    }

    buffer = &g_psram_state.public_state.buffer_table[buffer_id];

    /* 只能释放READY或ERROR状态的缓冲�?*/
    if (buffer->state != PSRAM_BUFFER_READY && buffer->state != PSRAM_BUFFER_ERROR) {
        bg_mutex_unlock(g_psram_state.mutex);
        return ENABLE_INVALID_INPUT;
    }

    /* 重置缓冲区状�?*/
    buffer->state = PSRAM_BUFFER_FREE;
    buffer->note_number = 0xFF;
    buffer->program = 0xFF;
    buffer->data_size = 0;
    buffer->checksum = 0;
    buffer->access_count = 0;

    psram_update_stats();

    bg_mutex_unlock(g_psram_state.mutex);

    BG_LOG_I(BG_LOG_TAG_PSRAM, "Note buffer released: id=%u", buffer_id);

    return SUCCESS;
}

BG_ERR PSRAM_LoadNoteData(uint32_t buffer_id, uint32_t nand_offset, uint32_t data_size)
{
    PSRAM_LoadRequest_t request;
    PSRAM_BufferInfo_t *buffer;

    if (!g_psram_state.public_state.initialized || buffer_id >= PSRAM_MAX_NOTE_BUFFERS) {
        return ENABLE_INVALID_INPUT;
    }

    if (data_size > PSRAM_NOTE_BUFFER_SIZE) {
        BG_LOG_E(BG_LOG_TAG_PSRAM, "Data size too large: %u > %u", data_size, PSRAM_NOTE_BUFFER_SIZE);
        return ENABLE_INVALID_INPUT;
    }

    /* 获取互斥�?*/
    if (bg_mutex_lock(g_psram_state.mutex, BG_OSAL_WAIT_FOREVER) != BG_OSAL_OK) {
        return ENABLE_INVALID_INPUT;
    }

    buffer = &g_psram_state.public_state.buffer_table[buffer_id];
    if (buffer->state != PSRAM_BUFFER_LOADING) {
        bg_mutex_unlock(g_psram_state.mutex);
        return ENABLE_INVALID_INPUT;
    }

    /* 准备加载请求 */
    request.buffer_id = buffer_id;
    request.nand_offset = nand_offset;
    request.data_size = data_size;

    /* �?NAND 读取校验�?(如果 NAND 存储可用) */
    NAND_ProgramIndex_t index;
    if (NAND_GetProgramInfo(0, &index) == SUCCESS) {  /* 假设 program 0 包含校验和信�?*/
        request.checksum = index.checksum;
    } else {
        request.checksum = 0;  /* 无校验和 */
    }

    /* 发送加载请求到队列 */
    if (bg_queue_send(g_psram_state.load_queue, &request, BG_OSAL_NO_WAIT) != BG_OSAL_OK) {
        BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to send load request to queue");
        bg_mutex_unlock(g_psram_state.mutex);
        return ENABLE_INVALID_INPUT;
    }

    bg_mutex_unlock(g_psram_state.mutex);

    BG_LOG_I(BG_LOG_TAG_PSRAM, "Note data load requested: buffer=%u, nand_offset=0x%08X, size=%u",
             buffer_id, nand_offset, data_size);

    return SUCCESS;
}

bool PSRAM_IsBufferReady(uint32_t buffer_id)
{
    PSRAM_BufferInfo_t *buffer;

    if (!g_psram_state.public_state.initialized || buffer_id >= PSRAM_MAX_NOTE_BUFFERS) {
        return false;
    }

    buffer = &g_psram_state.public_state.buffer_table[buffer_id];
    return (buffer->state == PSRAM_BUFFER_READY);
}

BG_ERR PSRAM_GetBufferInfo(uint32_t buffer_id, PSRAM_BufferInfo_t *info)
{
    if (!g_psram_state.public_state.initialized || buffer_id >= PSRAM_MAX_NOTE_BUFFERS || !info) {
        return ENABLE_INVALID_INPUT;
    }

    /* 获取互斥�?*/
    if (bg_mutex_lock(g_psram_state.mutex, BG_OSAL_WAIT_FOREVER) != BG_OSAL_OK) {
        return ENABLE_INVALID_INPUT;
    }

    memcpy(info, &g_psram_state.public_state.buffer_table[buffer_id], sizeof(PSRAM_BufferInfo_t));

    bg_mutex_unlock(g_psram_state.mutex);

    return SUCCESS;
}

int32_t PSRAM_ReadBufferData(uint32_t buffer_id, uint32_t offset, void *data, uint32_t size)
{
    PSRAM_BufferInfo_t *buffer;
    FlashDevice_t *psram_dev;
    uint32_t read_size;
    BG_ERR ret;

    if (!g_psram_state.public_state.initialized || buffer_id >= PSRAM_MAX_NOTE_BUFFERS || !data) {
        return -1;
    }

    buffer = &g_psram_state.public_state.buffer_table[buffer_id];
    if (buffer->state != PSRAM_BUFFER_READY && buffer->state != PSRAM_BUFFER_PLAYING) {
        return -1;
    }

    /* 检查读取范�?*/
    if (offset >= buffer->data_size) {
        return 0;
    }

    if (offset + size > buffer->data_size) {
        read_size = buffer->data_size - offset;
    } else {
        read_size = size;
    }

    /* 获取 PSRAM 设备 */
    psram_dev = FlashDevices_GetPsramFlash();
    if (!psram_dev) {
        return -1;
    }

    /* �?PSRAM 读取数据 */
    ret = (BG_ERR)psram_dev->ops->read(psram_dev, buffer->address + offset, (uint8_t *)data, read_size);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to read buffer data: buffer=%u, offset=%u, size=%u",
                 buffer_id, offset, read_size);
        return -1;
    }

    /* 更新访问时间 */
    PSRAM_UpdateAccessTime(buffer_id);

    return read_size;
}

void PSRAM_UpdateAccessTime(uint32_t buffer_id)
{
    PSRAM_BufferInfo_t *buffer;

    if (!g_psram_state.public_state.initialized || buffer_id >= PSRAM_MAX_NOTE_BUFFERS) {
        return;
    }

    /* 获取互斥�?*/
    if (bg_mutex_lock(g_psram_state.mutex, BG_OSAL_WAIT_FOREVER) != BG_OSAL_OK) {
        return;
    }

    buffer = &g_psram_state.public_state.buffer_table[buffer_id];
    buffer->last_access = g_psram_state.public_state.next_timestamp++;
    buffer->access_count++;

    bg_mutex_unlock(g_psram_state.mutex);
}

uint32_t PSRAM_GarbageCollect(uint32_t target_free_buffers)
{
    uint32_t freed_count = 0;
    uint32_t current_free = g_psram_state.public_state.free_buffers;

    if (!g_psram_state.public_state.initialized) {
        return 0;
    }

    /* 如果空闲缓冲区已经够了，不需要GC */
    if (current_free >= target_free_buffers) {
        return 0;
    }

    /* 获取互斥�?*/
    if (bg_mutex_lock(g_psram_state.mutex, BG_OSAL_WAIT_FOREVER) != BG_OSAL_OK) {
        return 0;
    }

    /* 释放最久未使用的缓冲区 */
    while (current_free < target_free_buffers && freed_count < PSRAM_MAX_NOTE_BUFFERS) {
        uint32_t lru_buffer_id;

        if (psram_find_lru_buffer(&lru_buffer_id) != SUCCESS) {
            break;  /* 没有更多可释放的缓冲�?*/
        }

        if (psram_evict_buffer(lru_buffer_id) == SUCCESS) {
            freed_count++;
            current_free++;
        }
    }

    psram_update_stats();

    bg_mutex_unlock(g_psram_state.mutex);

    BG_LOG_I(BG_LOG_TAG_PSRAM, "Garbage collected: freed %u buffers", freed_count);

    return freed_count;
}

void PSRAM_GetStats(uint32_t *total_buffers, uint32_t *free_buffers,
                   uint32_t *ready_buffers, uint32_t *playing_buffers)
{
    if (!g_psram_state.public_state.initialized) {
        return;
    }

    if (total_buffers) *total_buffers = g_psram_state.public_state.total_buffers;
    if (free_buffers) *free_buffers = g_psram_state.public_state.free_buffers;
    if (ready_buffers) *ready_buffers = g_psram_state.public_state.ready_buffers;
    if (playing_buffers) *playing_buffers = g_psram_state.public_state.playing_buffers;
}

BG_ERR PSRAM_PrefetchNote(uint8_t note, uint8_t program)
{
    PSRAM_BufferAlloc_t alloc_result;
    BG_ERR ret;
    int8_t offset;

    /* 简化的预测性预加载实现 */
    /* 预加载当前音符的相邻音符 (+/- 2 个半�? */

    for (offset = -2; offset <= 2; offset++) {
        uint8_t prefetch_note;
        bool already_cached;
        uint32_t i;
        uint32_t prefetch_data_size;
        uint32_t prefetch_nand_offset;
        PSRAM_NoteRequest_t request;

        if (offset == 0) continue; /* 跳过当前音符 */

        prefetch_note = (uint8_t)((int)note + (int)offset);
        if (prefetch_note > 127) continue; /* 超出 MIDI 范围 */

        /* 检查是否已在缓存中 */
        already_cached = false;
        for (i = 0; i < PSRAM_MAX_NOTE_BUFFERS; i++) {
            PSRAM_BufferInfo_t info;
            if (PSRAM_GetBufferInfo(i, &info) == SUCCESS) {
                if (info.note_number == prefetch_note && info.program == program &&
                    (info.state == PSRAM_BUFFER_READY || info.state == PSRAM_BUFFER_LOADING)) {
                    already_cached = true;
                    break;
                }
            }
        }

        if (already_cached) continue;

        /* 分配缓冲�?*/
        memset(&request, 0, sizeof(request));
        request.note = prefetch_note;
        request.velocity = 64; /* 中等力度 */
        request.program = program;
        request.sample_rate = 44100;
        request.high_priority = false;

        ret = PSRAM_RequestNoteBuffer(&request, &alloc_result);
        if (ret != SUCCESS) {
            continue; /* 分配失败，跳�?*/
        }

        /* 计算数据位置并开始加�?*/
        /* 这里使用简化的定位，实际应该调�?synth_locate_note_data */
        /* 由于这里没有访问synth_integration模块，我们使用简化的计算 */
        prefetch_data_size = PSRAM_NOTE_BUFFER_SIZE; /* 64KB */
#ifdef BANDATAHUB
        /* BanDataHub: SF2 在 PSRAM 样本区 (0x000000)，直接使用文件内偏移 */
        prefetch_nand_offset = ((uint32_t)prefetch_note * prefetch_data_size);
#else
        prefetch_nand_offset = SYNTH_SF2_NAND_BLOB_OFFSET + SYNTH_SF2_HEADER_SIZE +
                              ((uint32_t)prefetch_note * prefetch_data_size);
#endif

        if (prefetch_nand_offset > 0) {
            ret = PSRAM_LoadNoteData(alloc_result.buffer_id, prefetch_nand_offset, prefetch_data_size);
            if (ret == SUCCESS) {
                BG_LOG_I(BG_LOG_TAG_PSRAM, "Prefetched note: %u (program %u)", prefetch_note, program);
            }
        }
    }

    return SUCCESS;
}

BG_ERR PSRAM_SetBufferState(uint32_t buffer_id, PSRAM_BufferState_t state)
{
    PSRAM_BufferInfo_t *buffer;

    if (!g_psram_state.public_state.initialized || buffer_id >= PSRAM_MAX_NOTE_BUFFERS) {
        return ENABLE_INVALID_INPUT;
    }

    /* 获取互斥�?*/
    if (bg_mutex_lock(g_psram_state.mutex, BG_OSAL_WAIT_FOREVER) != BG_OSAL_OK) {
        return ENABLE_INVALID_INPUT;
    }

    buffer = &g_psram_state.public_state.buffer_table[buffer_id];
    buffer->state = state;

    psram_update_stats();

    bg_mutex_unlock(g_psram_state.mutex);

    BG_LOG_I(BG_LOG_TAG_PSRAM, "Buffer state set: id=%u, state=%d", buffer_id, state);

    return SUCCESS;
}

void PSRAM_FlushAllBuffers(void)
{
    uint32_t i;

    if (!g_psram_state.public_state.initialized) {
        return;
    }

    /* 获取互斥�?*/
    if (bg_mutex_lock(g_psram_state.mutex, BG_OSAL_WAIT_FOREVER) != BG_OSAL_OK) {
        return;
    }

    /* 重置所有缓冲区 */
    for (i = 0; i < PSRAM_MAX_NOTE_BUFFERS; i++) {
        PSRAM_BufferInfo_t *buffer = &g_psram_state.public_state.buffer_table[i];
        buffer->state = PSRAM_BUFFER_FREE;
        buffer->note_number = 0xFF;
        buffer->program = 0xFF;
        buffer->data_size = 0;
        buffer->checksum = 0;
        buffer->access_count = 0;
    }

    psram_update_stats();

    bg_mutex_unlock(g_psram_state.mutex);

    BG_LOG_I(BG_LOG_TAG_PSRAM, "All buffers flushed");
}

/* ============================================
 * 内部函数实现
 * ============================================ */

static BG_ERR psram_allocate_buffer(uint32_t *buffer_id)
{
    PSRAM_BufferInfo_t *buffer;
    uint32_t i;
    uint32_t lru_id;

    /* 查找空闲缓冲�?*/
    for (i = 0; i < PSRAM_MAX_NOTE_BUFFERS; i++) {
        buffer = &g_psram_state.public_state.buffer_table[i];
        if (buffer->state == PSRAM_BUFFER_FREE) {
            *buffer_id = i;
            return SUCCESS;
        }
    }

    /* 没有空闲缓冲区，直接驱逐LRU缓冲�?(内部调用，无需重复加锁) */
    if (psram_find_lru_buffer(&lru_id) != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_PSRAM, "No free buffers available");
        return ENABLE_INVALID_INPUT;
    }
    if (psram_evict_buffer(lru_id) != SUCCESS) {
        return ENABLE_INVALID_INPUT;
    }
    *buffer_id = lru_id;
    return SUCCESS;
}

static BG_ERR psram_find_lru_buffer(uint32_t *buffer_id)
{
    uint32_t oldest_timestamp = UINT32_MAX;
    uint32_t lru_id = UINT32_MAX;
    uint32_t i;
    PSRAM_BufferInfo_t *buffer;

    /* 查找最久未使用的READY状态缓冲区 */
    for (i = 0; i < PSRAM_MAX_NOTE_BUFFERS; i++) {
        buffer = &g_psram_state.public_state.buffer_table[i];
        if (buffer->state == PSRAM_BUFFER_READY && buffer->last_access < oldest_timestamp) {
            oldest_timestamp = buffer->last_access;
            lru_id = i;
        }
    }

    if (lru_id == UINT32_MAX) {
        return ENABLE_INVALID_INPUT;  /* 没有可释放的缓冲�?*/
    }

    *buffer_id = lru_id;
    return SUCCESS;
}

static BG_ERR psram_evict_buffer(uint32_t buffer_id)
{
    PSRAM_BufferInfo_t *buffer = &g_psram_state.public_state.buffer_table[buffer_id];

    /* 只能驱逐READY状态的缓冲�?*/
    if (buffer->state != PSRAM_BUFFER_READY) {
        return ENABLE_INVALID_INPUT;
    }

    /* 重置缓冲区状�?*/
    buffer->state = PSRAM_BUFFER_FREE;
    buffer->note_number = 0xFF;
    buffer->program = 0xFF;
    buffer->data_size = 0;
    buffer->checksum = 0;

    BG_LOG_I(BG_LOG_TAG_PSRAM, "Buffer evicted: id=%u", buffer_id);

    return SUCCESS;
}

static void psram_update_stats(void)
{
    uint32_t free_count = 0;
    uint32_t loading_count = 0;
    uint32_t ready_count = 0;
    uint32_t playing_count = 0;
    uint32_t i;

    for (i = 0; i < PSRAM_MAX_NOTE_BUFFERS; i++) {
        PSRAM_BufferState_t state = g_psram_state.public_state.buffer_table[i].state;
        switch (state) {
            case PSRAM_BUFFER_FREE: free_count++; break;
            case PSRAM_BUFFER_LOADING: loading_count++; break;
            case PSRAM_BUFFER_READY: ready_count++; break;
            case PSRAM_BUFFER_PLAYING: playing_count++; break;
            default: break;
        }
    }

    g_psram_state.public_state.free_buffers = free_count;
    g_psram_state.public_state.loading_buffers = loading_count;
    g_psram_state.public_state.ready_buffers = ready_count;
    g_psram_state.public_state.playing_buffers = playing_count;
}

static uint32_t psram_calculate_checksum(const void *data, uint32_t size)
{
    uint32_t checksum = 0x12345678;
    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t i;

    for (i = 0; i < size; i++) {
        checksum = (checksum << 5) + checksum + ptr[i];
    }

    return checksum;
}

static void psram_loader_task(void *param)
{
    PSRAM_LoadRequest_t request;
    BG_ERR ret;

    BG_LOG_I(BG_LOG_TAG_PSRAM, "PSRAM loader task started");

    while (1) {
        uint32_t checksum;
        /* 等待加载请求 */
        if (bg_queue_recv(g_psram_state.load_queue, &request, BG_OSAL_WAIT_FOREVER) != BG_OSAL_OK) {
            continue;
        }

        BG_LOG_I(BG_LOG_TAG_PSRAM, "Processing load request: buffer=%u, offset=0x%08X, size=%u",
                 request.buffer_id, request.nand_offset, request.data_size);

        /* 执行数据加载 */
        ret = psram_load_data_chunk(request.buffer_id, request.nand_offset, 0, request.data_size);
        if (ret != SUCCESS) {
            BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to load data chunk: %d", ret);

            /* 获取互斥锁更新缓冲区状�?*/
            if (bg_mutex_lock(g_psram_state.mutex, BG_OSAL_WAIT_FOREVER) == BG_OSAL_OK) {
                PSRAM_BufferInfo_t *buffer = &g_psram_state.public_state.buffer_table[request.buffer_id];
                buffer->state = PSRAM_BUFFER_ERROR;
                psram_update_stats();
                bg_mutex_unlock(g_psram_state.mutex);
            }
            continue;
        }

        /* 计算校验�?*/
        checksum = 0;
        if (request.checksum != 0) {
            /* 读取缓冲区数据计算校验和并验�?*/
            uint8_t *verify_buffer = (uint8_t *)malloc(request.data_size);
            if (verify_buffer) {
                /* 直接从PSRAM读取已加载的数据进行校验和计�?*/
                FlashDevice_t *psram_dev = FlashDevices_GetPsramFlash();
                if (psram_dev) {
                    PSRAM_BufferInfo_t *buffer = &g_psram_state.public_state.buffer_table[request.buffer_id];
                    BG_ERR verify_ret = psram_dev->ops->read(psram_dev, buffer->address,
                                                           verify_buffer, request.data_size);
                    if (verify_ret == SUCCESS) {
                        checksum = psram_calculate_checksum(verify_buffer, request.data_size);
                        if (checksum != request.checksum) {
                            BG_LOG_W(BG_LOG_TAG_PSRAM, "Checksum mismatch: expected=0x%08X, got=0x%08X",
                                    request.checksum, checksum);
                            /* 可以选择标记缓冲区为错误状�?*/
                        } else {
                            BG_LOG_I(BG_LOG_TAG_PSRAM, "Checksum verified: 0x%08X", checksum);
                        }
                    } else {
                        BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to read PSRAM for checksum verification");
                    }
                }
                free(verify_buffer);
            } else {
                BG_LOG_W(BG_LOG_TAG_PSRAM, "Failed to allocate buffer for checksum verification");
            }
        }

        /* 获取互斥锁更新缓冲区状�?*/
        if (bg_mutex_lock(g_psram_state.mutex, BG_OSAL_WAIT_FOREVER) == BG_OSAL_OK) {
            PSRAM_BufferInfo_t *buffer = &g_psram_state.public_state.buffer_table[request.buffer_id];
            buffer->state = PSRAM_BUFFER_READY;
            buffer->data_size = request.data_size;
            buffer->checksum = checksum;
            psram_update_stats();

            BG_LOG_I(BG_LOG_TAG_PSRAM, "Buffer ready: id=%u, size=%u", request.buffer_id, request.data_size);

            bg_mutex_unlock(g_psram_state.mutex);
        }
    }
}

static BG_ERR psram_load_data_chunk(uint32_t buffer_id, uint32_t nand_offset,
                                   uint32_t psram_offset, uint32_t size)
{
    PSRAM_BufferInfo_t *buffer;
    FlashDevice_t *src_dev, *psram_dev;
    uint8_t *temp_buffer;
    uint32_t remaining = size;
    uint32_t chunk_size;
    uint32_t current_src_offset;
    uint32_t current_psram_offset;
    BG_ERR ret;

#ifdef BANDATAHUB
    /* BanDataHub: SF2数据已在PSRAM样本区 (0x000000~0x5FFFFF)
     * 源设备也是 PSRAM, nand_offset 是 SF2 内部偏移 */
    src_dev = FlashDevices_GetPsramFlash();
    psram_dev = FlashDevices_GetPsramFlash();
    if (!src_dev || !psram_dev) {
        return ENABLE_INVALID_INPUT;
    }
#else
    /* 获取设备 */
    src_dev = FlashDevices_GetNandFlash();
    psram_dev = FlashDevices_GetPsramFlash();
    if (!src_dev || !psram_dev) {
        return ENABLE_INVALID_INPUT;
    }
#endif

    buffer = &g_psram_state.public_state.buffer_table[buffer_id];

    /* 分配临时缓冲�?*/
    temp_buffer = (uint8_t *)malloc(PSRAM_DMA_CHUNK_SIZE);
    if (!temp_buffer) {
        return ENABLE_OUT_OF_MEMORY;
    }

    /* 分块传输数据 */
    current_src_offset = nand_offset;
    current_psram_offset = buffer->address + psram_offset;

    while (remaining > 0) {
        chunk_size = (remaining > PSRAM_DMA_CHUNK_SIZE) ? PSRAM_DMA_CHUNK_SIZE : remaining;

        /* 从源设备读取数据到临时缓冲区 */
        ret = (BG_ERR)src_dev->ops->read(src_dev, current_src_offset, temp_buffer, chunk_size);
        if (ret != SUCCESS) {
            BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to read from source: offset=0x%08X, size=%u",
                     current_src_offset, chunk_size);
            free(temp_buffer);
            return ret;
        }

        /* 将数据写�?PSRAM */
        ret = (BG_ERR)psram_dev->ops->write(psram_dev, current_psram_offset, temp_buffer, chunk_size);
        if (ret != SUCCESS) {
            BG_LOG_E(BG_LOG_TAG_PSRAM, "Failed to write to PSRAM: offset=0x%08X, size=%u",
                     current_psram_offset, chunk_size);
            free(temp_buffer);
            return ret;
        }

        current_src_offset += chunk_size;
        current_psram_offset += chunk_size;
        remaining -= chunk_size;
    }

    free(temp_buffer);

    BG_LOG_I(BG_LOG_TAG_PSRAM, "Data chunk loaded: buffer=%u, src_offset=0x%08X, psram_offset=0x%08X, size=%u",
             buffer_id, nand_offset, buffer->address + psram_offset, size);

    return SUCCESS;
}

#endif /* SYNTH_SD_NAND_PSRAM_EN */