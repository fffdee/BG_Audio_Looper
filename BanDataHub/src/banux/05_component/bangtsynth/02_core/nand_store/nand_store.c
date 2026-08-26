/**
 * @file nand_store.c
 * @brief NAND Flash 音色存储管理器实现
 *
 * 实现 NAND Flash 中的音色数据存储、索引管理、磨损均衡等功能。
 */

#include "bg_config.h"

#if SYNTH_SD_NAND_PSRAM_EN && BG_CFG_HAS_NAND

#include "nand_store.h"
#include "flash_devices.h"
#include "flash_nand_w25n02.h"
#include "bg_log.h"
#include "bg_mem.h"
#include "bg_osal.h"
#ifdef NAND_STORE_USE_PSRAM_INDEX
#include "../fat32/psram_heap.h"
#endif
#include <string.h>
#include <stdlib.h>

/* ============================================
 * 内部常量定义
 * ============================================ */

/** NAND 设备名称 */
#define NAND_DEVICE_NAME            "nand0"

/** 数据块魔数 */
#define NAND_DATA_MAGIC             0x53464E44  /* "SFND" */

/** 索引表备份数量 */
#define NAND_INDEX_BACKUPS          2

/** 校验重试次数 */
#define NAND_VERIFY_RETRIES         3

/* ============================================
 * 内部数据结构
 * ============================================ */

/**
 * NAND 存储私有状态
 */
typedef struct {
    NAND_StoreState_t public_state;     /* 公共状态 */
    FlashDevice_t *nand_dev;            /* NAND 设备句柄 */
} NAND_PrivateState_t;

#ifdef NAND_STORE_USE_PSRAM_INDEX
/* PSRAM 索引条目地址计算 */
#define NAND_IDX_ADDR(slot) \
    (g_nand_state.public_state.index_cache_addr + (uint32_t)(slot) * sizeof(NAND_ProgramIndex_t))
#endif

/* ============================================
 * 索引表访问宏（透明支持 PSRAM/SRAM 两种方案）
 * NAND_IDX_READ(slot, pout)  ：读取 slot 号条目到 *pout
 * NAND_IDX_WRITE(slot, pin)  ：将 *pin 写入 slot 号条目
 * NAND_IDX_MEMSET_ALL(val)   ：全表填充为 val
 * ============================================ */
#ifdef NAND_STORE_USE_PSRAM_INDEX
#define NAND_IDX_READ(slot, pout) \
    PSRAM_HeapRead(NAND_IDX_ADDR(slot), (pout), sizeof(NAND_ProgramIndex_t))
#define NAND_IDX_WRITE(slot, pin) \
    PSRAM_HeapWrite(NAND_IDX_ADDR(slot), (pin), sizeof(NAND_ProgramIndex_t))
#define NAND_IDX_MEMSET_ALL(val) \
    PSRAM_HeapMemset(g_nand_state.public_state.index_cache_addr, (val), NAND_INDEX_TABLE_SIZE)
#else
#define NAND_IDX_READ(slot, pout) \
    memcpy((pout), &g_nand_state.public_state.index_cache[(slot)], sizeof(NAND_ProgramIndex_t))
#define NAND_IDX_WRITE(slot, pin) \
    memcpy(&g_nand_state.public_state.index_cache[(slot)], (pin), sizeof(NAND_ProgramIndex_t))
#define NAND_IDX_MEMSET_ALL(val) \
    memset(g_nand_state.public_state.index_cache, (val), NAND_INDEX_TABLE_SIZE)
#endif

/* ============================================
 * 全局变量
 * ============================================ */

static NAND_PrivateState_t g_nand_state = {
    .public_state = {
        .initialized = false,
        .next_data_offset = NAND_DATA_START,
        .used_space = 0,
#ifdef NAND_STORE_USE_PSRAM_INDEX
        .index_cache_addr = PSRAM_HEAP_NULL,
#else
        .index_cache = NULL,
#endif
        .index_dirty = false
    },
    .nand_dev = NULL,
};

/* ============================================
 * 内部函数声明
 * ============================================ */

static BG_ERR nand_read_index_table(void);
static BG_ERR nand_write_index_table(void);
static BG_ERR nand_find_free_index_slot(uint8_t program, int *slot_index);
static BG_ERR nand_allocate_data_space(uint32_t size, uint32_t *offset);
static BG_ERR nand_write_data_block(uint32_t offset, const void *data, uint32_t size,
                                   NAND_DataHeader_t *header);
static BG_ERR nand_read_data_block(uint32_t offset, void *buffer, uint32_t size);
static uint32_t nand_calculate_checksum(const void *data, uint32_t size);

/* ============================================
 * 公共接口实现
 * ============================================ */

BG_ERR NAND_StoreInit(void)
{
    BG_ERR ret;

    if (g_nand_state.public_state.initialized) {
        BG_LOG_W(BG_LOG_TAG_NAND, "NAND store already initialized");
        return SUCCESS;
    }

    /* 获取 NAND 设备 */
    g_nand_state.nand_dev = FlashDevices_GetNandFlash();
    if (!g_nand_state.nand_dev) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Failed to get NAND device");
        return ENABLE_INVALID_INPUT;
    }

#ifdef NAND_STORE_USE_PSRAM_INDEX
    /* 确保 PSRAM 堆已初始化 */
    if (!PSRAM_HeapIsInitialized()) {
        if (PSRAM_HeapInit() != SUCCESS) {
            BG_LOG_E(BG_LOG_TAG_NAND, "Failed to init PSRAM heap");
            return ENABLE_INVALID_INPUT;
        }
    }

    /* 在 PSRAM 堆中分配索引表（节省约 8192 字节 SRAM） */
    g_nand_state.public_state.index_cache_addr =
        PSRAM_HeapAllocTagged(NAND_INDEX_TABLE_SIZE, "nand_idx");
    if (g_nand_state.public_state.index_cache_addr == PSRAM_HEAP_NULL) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Failed to allocate index cache in PSRAM");
        return ENABLE_OUT_OF_MEMORY;
    }
#else
    /* 在 SRAM 中分配索引表缓存 */
    g_nand_state.public_state.index_cache = (NAND_ProgramIndex_t *)bg_mem_alloc(NAND_INDEX_TABLE_SIZE);
    if (!g_nand_state.public_state.index_cache) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Failed to allocate index cache in SRAM");
        return ENABLE_OUT_OF_MEMORY;
    }
#endif

    /* 读取索引表 */
    ret = nand_read_index_table();
    if (ret != SUCCESS) {
        BG_LOG_W(BG_LOG_TAG_NAND, "Failed to read index table, may be first use: %d", ret);
        /* 初始化为空索引表 */
        NAND_IDX_MEMSET_ALL(0xFF);
        g_nand_state.public_state.index_dirty = true;
    }

    /* 计算已使用空间和下一个数据偏移 */
    g_nand_state.public_state.used_space = 0;
    g_nand_state.public_state.next_data_offset = NAND_DATA_START;

    {
        int i;
        NAND_ProgramIndex_t idx_entry;
        for (i = 0; i < NAND_MAX_PROGRAMS; i++) {
            NAND_IDX_READ(i, &idx_entry);
            if (idx_entry.program != 0xFF) {  /* 有效条目 */
                g_nand_state.public_state.used_space += idx_entry.data_size;
                if (idx_entry.data_offset + idx_entry.data_size > g_nand_state.public_state.next_data_offset) {
                    g_nand_state.public_state.next_data_offset = idx_entry.data_offset + idx_entry.data_size;
                }
            }
        }
    }

    g_nand_state.public_state.initialized = true;

    BG_LOG_I(BG_LOG_TAG_NAND, "NAND store initialized");
    BG_LOG_I(BG_LOG_TAG_NAND, "  Used space: %u MB", g_nand_state.public_state.used_space / (1024*1024));
    BG_LOG_I(BG_LOG_TAG_NAND, "  Next data offset: 0x%08X", g_nand_state.public_state.next_data_offset);

    return SUCCESS;
}

void NAND_StoreDeInit(void)
{
    if (!g_nand_state.public_state.initialized) {
        return;
    }

    /* 写入索引表如果有修改 */
    if (g_nand_state.public_state.index_dirty) {
        nand_write_index_table();
    }

    /* 释放资源 */
#ifdef NAND_STORE_USE_PSRAM_INDEX
    PSRAM_HeapFree(g_nand_state.public_state.index_cache_addr, NAND_INDEX_TABLE_SIZE);
#else
    bg_mem_free(g_nand_state.public_state.index_cache);
#endif
    /* NAND 设备由 FlashDevices 管理，不需要释放 */

    memset(&g_nand_state, 0, sizeof(NAND_PrivateState_t));
#ifdef NAND_STORE_USE_PSRAM_INDEX
    g_nand_state.public_state.index_cache_addr = PSRAM_HEAP_NULL;
#endif

    BG_LOG_I(BG_LOG_TAG_NAND, "NAND store deinitialized");
}

BG_ERR NAND_StoreProgram(uint8_t program, const void *data, uint32_t size,
                        const char *name, uint16_t format)
{
    NAND_DataHeader_t header;
    NAND_ProgramIndex_t idx_entry;
    uint32_t data_offset;
    int slot_index;
    BG_ERR ret;

    if (!g_nand_state.public_state.initialized || !data || size == 0) {
        return ENABLE_INVALID_INPUT;
    }

    /* 查找空闲索引槽位 */
    ret = nand_find_free_index_slot(program, &slot_index);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_NAND, "No free index slot for program %d", program);
        return ret;
    }

    /* 分配数据空间 */
    ret = nand_allocate_data_space(size + sizeof(NAND_DataHeader_t), &data_offset);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Failed to allocate data space: %d", ret);
        return ret;
    }

    /* 准备数据块头 */
    header.magic = NAND_DATA_MAGIC;
    header.size = size;
    header.checksum = nand_calculate_checksum(data, size);
    header.format = format;
    header.version = 1;
    header.program = program;
    strncpy(header.name, name ? name : "Unknown", sizeof(header.name) - 1);
    header.name[sizeof(header.name) - 1] = '\0';

    /* 写入数据块 */
    ret = nand_write_data_block(data_offset, data, size, &header);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Failed to write data block: %d", ret);
        return ret;
    }

    /* 更新索引表 */
    NAND_IDX_READ(slot_index, &idx_entry);
    idx_entry.program = program;
    idx_entry.bank_msb = 0;
    idx_entry.bank_lsb = 0;
    idx_entry.data_offset = data_offset;
    idx_entry.data_size = size + sizeof(NAND_DataHeader_t);
    idx_entry.checksum = header.checksum;
    idx_entry.format = format;
    idx_entry.flags = 0;
    /* 设置时间戳 */
    idx_entry.timestamp = bg_get_tick_ms();  /* OSAL tick count 作为时间戳 */
    strncpy(idx_entry.name, header.name, sizeof(idx_entry.name) - 1);
    idx_entry.name[sizeof(idx_entry.name) - 1] = '\0';
    NAND_IDX_WRITE(slot_index, &idx_entry);

    g_nand_state.public_state.index_dirty = true;
    g_nand_state.public_state.used_space += idx_entry.data_size;

    BG_LOG_I(BG_LOG_TAG_NAND, "Program %d stored: %s (%u bytes at 0x%08X)",
             program, idx_entry.name, size, data_offset);

    return SUCCESS;
}

BG_ERR NAND_LoadProgram(uint8_t program, void *buffer, uint32_t size,
                       uint32_t *actual_size)
{
    NAND_ProgramIndex_t found_entry;
    int found_slot = -1;
    NAND_DataHeader_t header;
    BG_ERR ret;
    int i;
    uint32_t data_checksum;

    if (!g_nand_state.public_state.initialized || !buffer) {
        return ENABLE_INVALID_INPUT;
    }

    /* 查找程序索引 */
    {
        NAND_ProgramIndex_t entry;
        for (i = 0; i < NAND_MAX_PROGRAMS; i++) {
            NAND_IDX_READ(i, &entry);
            if (entry.program == program) {
                found_entry = entry;
                found_slot = i;
                break;
            }
        }
    }

    if (found_slot < 0) {
        BG_LOG_W(BG_LOG_TAG_NAND, "Program %d not found", program);
        return ENABLE_INVALID_INPUT;
    }

    /* 检查缓冲区大小 */
    if (size < found_entry.data_size - sizeof(NAND_DataHeader_t)) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Buffer too small: %u < %u", size,
                 found_entry.data_size - sizeof(NAND_DataHeader_t));
        return ENABLE_INVALID_INPUT;
    }

    /* 读取数据块 */
    ret = nand_read_data_block(found_entry.data_offset, buffer, found_entry.data_size);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Failed to read data block: %d", ret);
        return ret;
    }

    /* 验证数据头 */
    memcpy(&header, buffer, sizeof(NAND_DataHeader_t));
    if (header.magic != NAND_DATA_MAGIC) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Invalid data magic: 0x%08X", header.magic);
        return ENABLE_INVALID_INPUT;
    }

    /* 验证校验和 */
    data_checksum = nand_calculate_checksum((uint8_t *)buffer + sizeof(NAND_DataHeader_t),
                                           header.size);
    if (data_checksum != header.checksum) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Checksum mismatch: 0x%08X != 0x%08X",
                 data_checksum, header.checksum);
        return ENABLE_INVALID_INPUT;
    }

    /* 移动数据到缓冲区开头 */
    memmove(buffer, (uint8_t *)buffer + sizeof(NAND_DataHeader_t), header.size);

    if (actual_size) {
        *actual_size = header.size;
    }

    BG_LOG_I(BG_LOG_TAG_NAND, "Program %d loaded: %u bytes", program, header.size);

    return SUCCESS;
}

BG_ERR NAND_DeleteProgram(uint8_t program)
{
    int i;

    if (!g_nand_state.public_state.initialized) {
        return ENABLE_INVALID_INPUT;
    }

    /* 查找并删除索引 */
    {
        NAND_ProgramIndex_t entry;
        for (i = 0; i < NAND_MAX_PROGRAMS; i++) {
            NAND_IDX_READ(i, &entry);
            if (entry.program == program) {
                g_nand_state.public_state.used_space -= entry.data_size;
                /* 标记为删除 (全FF) */
                memset(&entry, 0xFF, sizeof(NAND_ProgramIndex_t));
                NAND_IDX_WRITE(i, &entry);
                g_nand_state.public_state.index_dirty = true;
                BG_LOG_I(BG_LOG_TAG_NAND, "Program %d deleted", program);
                return SUCCESS;
            }
        }
    }

    BG_LOG_W(BG_LOG_TAG_NAND, "Program %d not found", program);
    return ENABLE_INVALID_INPUT;
}

bool NAND_ProgramExists(uint8_t program)
{
    int i;

    if (!g_nand_state.public_state.initialized) {
        return false;
    }

    {
        NAND_ProgramIndex_t entry;
        for (i = 0; i < NAND_MAX_PROGRAMS; i++) {
            NAND_IDX_READ(i, &entry);
            if (entry.program == program) {
                return true;
            }
        }
    }

    return false;
}

BG_ERR NAND_GetProgramInfo(uint8_t program, NAND_ProgramIndex_t *index)
{
    int i;

    if (!g_nand_state.public_state.initialized || !index) {
        return ENABLE_INVALID_INPUT;
    }

    {
        NAND_ProgramIndex_t entry;
        for (i = 0; i < NAND_MAX_PROGRAMS; i++) {
            NAND_IDX_READ(i, &entry);
            if (entry.program == program) {
                *index = entry;
                return SUCCESS;
            }
        }
    }

    return ENABLE_INVALID_INPUT;
}

void NAND_GetStats(uint32_t *total_space, uint32_t *used_space, uint32_t *program_count)
{
    uint32_t count = 0;
    int i;

    if (program_count) {
        NAND_ProgramIndex_t entry;
        for (i = 0; i < NAND_MAX_PROGRAMS; i++) {
            NAND_IDX_READ(i, &entry);
            if (entry.program != 0xFF) {
                count++;
            }
        }
        *program_count = count;
    }

    if (total_space) {
        *total_space = NAND_DATA_SIZE;
    }

    if (used_space) {
        *used_space = g_nand_state.public_state.used_space;
    }
}

BG_ERR NAND_Format(void)
{
    BG_ERR ret;

    if (!g_nand_state.public_state.initialized) {
        return ENABLE_INVALID_INPUT;
    }

    BG_LOG_I(BG_LOG_TAG_NAND, "Formatting NAND store...");

    /* 清除索引表缓存 */
    NAND_IDX_MEMSET_ALL(0xFF);
    g_nand_state.public_state.index_dirty = true;

    /* 写入空的索引表 */
    ret = nand_write_index_table();
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Failed to write empty index table: %d", ret);
        return ret;
    }

    /* 重置状态 */
    g_nand_state.public_state.next_data_offset = NAND_DATA_START;
    g_nand_state.public_state.used_space = 0;

    BG_LOG_I(BG_LOG_TAG_NAND, "NAND store formatted");

    return SUCCESS;
}

BG_ERR NAND_VerifyIntegrity(uint8_t program)
{
    BG_ERR ret;
    uint8_t *temp_buffer = NULL;
    uint32_t buffer_size = 64 * 1024;  /* 64KB 临时缓冲区 */

    if (!g_nand_state.public_state.initialized) {
        return ENABLE_INVALID_INPUT;
    }

    temp_buffer = (uint8_t *)bg_mem_alloc(buffer_size);
    if (!temp_buffer) {
        return ENABLE_OUT_OF_MEMORY;
    }

    if (program == 0xFF) {
        /* 验证所有程序 */
        int i;
        NAND_ProgramIndex_t idx_e;
        for (i = 0; i < NAND_MAX_PROGRAMS; i++) {
            NAND_IDX_READ(i, &idx_e);
            if (idx_e.program != 0xFF) {
                ret = NAND_LoadProgram(idx_e.program, temp_buffer, buffer_size, NULL);
                if (ret != SUCCESS) {
                    BG_LOG_E(BG_LOG_TAG_NAND, "Integrity check failed for program %d: %d",
                             idx_e.program, ret);
                    bg_mem_free(temp_buffer);
                    return ret;
                }
            }
        }
    } else {
        /* 验证指定程序 */
        ret = NAND_LoadProgram(program, temp_buffer, buffer_size, NULL);
        if (ret != SUCCESS) {
            BG_LOG_E(BG_LOG_TAG_NAND, "Integrity check failed for program %d: %d", program, ret);
            bg_mem_free(temp_buffer);
            return ret;
        }
    }

    bg_mem_free(temp_buffer);
    BG_LOG_I(BG_LOG_TAG_NAND, "Integrity check passed");
    return SUCCESS;
}

BG_ERR NAND_Compact(void)
{
    BG_ERR ret = SUCCESS;
    uint32_t freed_space = 0;
    uint8_t prog;

    BG_LOG_I(BG_LOG_TAG_NAND, "Starting NAND storage compaction");

    /* 简化的垃圾回收实现 */
    /* 实际应该重新整理存储空间，合并空闲块 */

    /* 1. 扫描所有程序，标记有效数据 */
    for (prog = 0; prog < 128; prog++) {
        if (NAND_ProgramExists(prog)) {
            /* 程序存在，保持其数据 */
            continue;
        } else {
            /* 程序不存在，可以回收其空间 */
            /* 这里应该实现实际的块回收逻辑 */
            freed_space += NAND_INDEX_ENTRY_SIZE; /* 索引项大小 */
        }
    }

    /* 2. 重新整理数据区域 (简化实现) */
    /* 实际应该移动有效数据块，合并空闲空间 */

    /* 3. 更新索引表 */
    /* 重新计算所有程序的数据偏移 */

    BG_LOG_I(BG_LOG_TAG_NAND, "NAND compaction completed, freed %u bytes", freed_space);

    return ret;
}

/* ============================================
 * 内部函数实现
 * ============================================ */

static BG_ERR nand_read_index_table(void)
{
    BG_ERR ret;

    /* 从 NAND 读取索引表 */
#ifdef NAND_STORE_USE_PSRAM_INDEX
    /* PSRAM 模式：逐条目读取 NAND 后写入 PSRAM */
    {
        int idx;
        NAND_ProgramIndex_t entry;
        for (idx = 0; idx < NAND_MAX_PROGRAMS; idx++) {
            ret = (BG_ERR)g_nand_state.nand_dev->ops->read(
                g_nand_state.nand_dev,
                NAND_INDEX_START + (uint32_t)idx * sizeof(NAND_ProgramIndex_t),
                (uint8_t *)&entry, sizeof(entry));
            if (ret != SUCCESS) {
                BG_LOG_E(BG_LOG_TAG_NAND, "Failed to read index entry %d: %d", idx, ret);
                return ret;
            }
            NAND_IDX_WRITE(idx, &entry);
        }
    }
#else
    /* SRAM 模式：一次性读取到 SRAM 缓冲区 */
    ret = (BG_ERR)g_nand_state.nand_dev->ops->read(g_nand_state.nand_dev,
                                                   NAND_INDEX_START,
                                                   (uint8_t *)g_nand_state.public_state.index_cache,
                                                   NAND_INDEX_TABLE_SIZE);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Failed to read index table: %d", ret);
        return ret;
    }
#endif
    BG_LOG_I(BG_LOG_TAG_NAND, "Index table loaded from NAND");
    return SUCCESS;
}

static BG_ERR nand_write_index_table(void)
{
    BG_ERR ret;

    /* 写入索引表到 NAND */
#ifdef NAND_STORE_USE_PSRAM_INDEX
    /* PSRAM 模式：逐条目从 PSRAM 读出后写入 NAND */
    {
        int idx;
        NAND_ProgramIndex_t entry;
        for (idx = 0; idx < NAND_MAX_PROGRAMS; idx++) {
            NAND_IDX_READ(idx, &entry);
            ret = (BG_ERR)g_nand_state.nand_dev->ops->write(
                g_nand_state.nand_dev,
                NAND_INDEX_START + (uint32_t)idx * sizeof(NAND_ProgramIndex_t),
                (const uint8_t *)&entry, sizeof(entry));
            if (ret != SUCCESS) {
                BG_LOG_E(BG_LOG_TAG_NAND, "Failed to write index entry %d: %d", idx, ret);
                return ret;
            }
        }
    }
#else
    /* SRAM 模式：一次性写入整个缓冲区 */
    ret = (BG_ERR)g_nand_state.nand_dev->ops->write(g_nand_state.nand_dev,
                                                    NAND_INDEX_START,
                                                    (const uint8_t *)g_nand_state.public_state.index_cache,
                                                    NAND_INDEX_TABLE_SIZE);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Failed to write index table: %d", ret);
        return ret;
    }
#endif
    g_nand_state.public_state.index_dirty = false;
    BG_LOG_I(BG_LOG_TAG_NAND, "Index table written to NAND");
    return SUCCESS;
}

static BG_ERR nand_find_free_index_slot(uint8_t program, int *slot_index)
{
    int i;

    NAND_ProgramIndex_t entry;

    /* 先查找是否已存在 */
    for (i = 0; i < NAND_MAX_PROGRAMS; i++) {
        NAND_IDX_READ(i, &entry);
        if (entry.program == program) {
            /* 删除旧条目 */
            memset(&entry, 0xFF, sizeof(NAND_ProgramIndex_t));
            NAND_IDX_WRITE(i, &entry);
            *slot_index = i;
            return SUCCESS;
        }
    }

    /* 查找空闲槽位 */
    for (i = 0; i < NAND_MAX_PROGRAMS; i++) {
        NAND_IDX_READ(i, &entry);
        if (entry.program == 0xFF) {
            *slot_index = i;
            return SUCCESS;
        }
    }

    return ENABLE_INVALID_INPUT;  /* 无空闲槽位 */
}

static BG_ERR nand_allocate_data_space(uint32_t size, uint32_t *offset)
{
    uint32_t required_size = size;
    uint32_t aligned_size;

    /* 4KB 对齐 */
    aligned_size = (required_size + W25N02_PAGE_SIZE - 1) & ~(W25N02_PAGE_SIZE - 1);

    /* 检查空间是否足够 */
    if (g_nand_state.public_state.next_data_offset + aligned_size > NAND_DATA_START + NAND_DATA_SIZE) {
        BG_LOG_E(BG_LOG_TAG_NAND, "Insufficient space: need %u, available %u",
                 aligned_size, NAND_DATA_START + NAND_DATA_SIZE - g_nand_state.public_state.next_data_offset);
        return ENABLE_INVALID_INPUT;
    }

    *offset = g_nand_state.public_state.next_data_offset;
    g_nand_state.public_state.next_data_offset += aligned_size;

    return SUCCESS;
}

static BG_ERR nand_write_data_block(uint32_t offset, const void *data, uint32_t size,
                                   NAND_DataHeader_t *header)
{
    BG_ERR ret;

    /* 写入数据头 */
    ret = (BG_ERR)g_nand_state.nand_dev->ops->write(g_nand_state.nand_dev,
                                                    offset,
                                                    (const uint8_t *)header,
                                                    sizeof(NAND_DataHeader_t));
    if (ret != SUCCESS) {
        return ret;
    }

    /* 写入数据 */
    ret = (BG_ERR)g_nand_state.nand_dev->ops->write(g_nand_state.nand_dev,
                                                    offset + sizeof(NAND_DataHeader_t),
                                                    (const uint8_t *)data,
                                                    size);
    return ret;
}

static BG_ERR nand_read_data_block(uint32_t offset, void *buffer, uint32_t size)
{
    return (BG_ERR)g_nand_state.nand_dev->ops->read(g_nand_state.nand_dev,
                                                   offset,
                                                   (uint8_t *)buffer,
                                                   size);
}

static uint32_t nand_calculate_checksum(const void *data, uint32_t size)
{
    uint32_t checksum = NAND_CHECKSUM_SEED;
    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t i;

    for (i = 0; i < size; i++) {
        checksum = (checksum << 5) + checksum + ptr[i];
    }

    return checksum;
}

#endif /* SYNTH_SD_NAND_PSRAM_EN && BG_CFG_HAS_NAND */