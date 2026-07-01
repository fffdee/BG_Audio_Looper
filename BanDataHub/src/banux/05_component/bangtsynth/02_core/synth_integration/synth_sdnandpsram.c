/**
 * @file synth_sdnandpsram.c
 * @brief SD卡 + NAND + PSRAM 合成器集成模块实现
 *
 * BanDataHub 适配: 无 NAND Flash，采用 SD→PSRAM 二级直读架构
 *   - SYNTH_SDNANDPSRAM_Init() 跳过 NAND 初始化和 SF2 拷贝
 *   - 使用 bg_storage_driver_bandatahub (PSRAM 直接读取)
 *   - psram_load_data_chunk() 从 PSRAM 样本区读取 (非 NAND)
 */

#include "product_def.h"

#if SYNTH_SD_NAND_PSRAM_EN

#include "synth_sdnandpsram.h"
#include "fat32_reader.h"
#include "fat32_diskio.h"
#ifndef BANDATAHUB
#include "nand_store.h"
#endif
#include "psram_buffer.h"
#include "soundbank_manager.h"
#include "flash_devices.h"
#include "bg_storage.h"
#include "bg_log.h"
#include "bg_config.h"
#include "bg_osal.h"
#include <string.h>
#include <stdlib.h>

/* BanDataHub PSRAM 样本区基地址 */
#ifdef BANDATAHUB
#define BDH_SF2_SAMPLE_BASE     0x000000u
/* BanDataHub: 使用 PSRAM 直接存储驱动 (跳过 NAND) */
extern const BG_Storage_Driver_t bg_storage_driver_bandatahub;
#endif

/* ============================================
 * 内部状态
 * ============================================ */

static SYNTH_Status_t g_synth_status = {
    .storage_ready = false,
    .psram_ready = false,
    .soundbank_ready = false,
    .sf2_size = 0
};

static SYNTH_NandDriverState_t g_nand_driver_state = {
    .initialized = false
};

/* ============================================
 * 程序预热状态机 (主循环驱动，无 RTOS 任务)
 * ============================================ */

/** 预热音符范围: C1(24)~C7(96), 步进 3 减轻 PSRAM 压力 */
#define SYNTH_PREFETCH_NOTE_MIN  24
#define SYNTH_PREFETCH_NOTE_MAX  96
#define SYNTH_PREFETCH_STEP      3

/** 连续两步预热之间的最小间隔 (ms)，避免阻塞主循环太久 */
#define SYNTH_PREFETCH_INTERVAL_MS  5

static uint8_t  g_prefetch_pending  = 0;  /* 1 = 有待执行的预热请求 */
static uint8_t  g_prefetch_program  = 0;  /* 待预热的 program 号 */
static uint8_t  g_prefetch_note     = SYNTH_PREFETCH_NOTE_MIN;  /* 当前预热进度 */
static uint32_t g_prefetch_last_ms  = 0;  /* 上次预热的时间戳 */

#ifndef BANDATAHUB
/* NAND 存储驱动函数声明 (BanDataHub 不使用) */
static BG_ERR synth_nand_init(const char *path, BG_Storage_Mode_t mode);
static BG_ERR synth_nand_deinit(void);
static int synth_nand_read(uint32_t offset, void *buffer, size_t size);
static int synth_nand_write(uint32_t offset, const void *buffer, size_t size);
static BG_ERR synth_nand_erase(uint32_t offset, size_t size);
static BG_ERR synth_nand_sync(void);
static BG_ERR synth_nand_get_info(uint32_t *total_size, uint32_t *free_size);

/* NAND 存储驱动实例 */
const BG_Storage_Driver_t synth_nand_storage_driver = {
    .init = synth_nand_init,
    .deinit = synth_nand_deinit,
    .read = synth_nand_read,
    .write = synth_nand_write,
    .erase = synth_nand_erase,
    .sync = synth_nand_sync,
    .get_info = synth_nand_get_info
};
#endif /* !BANDATAHUB */

/* 内部函数声明 */
#ifndef BANDATAHUB
static BG_ERR synth_copy_sf2_to_nand(const char *filename);
static BG_ERR synth_verify_sf2_in_nand(void);
#endif

/* ============================================
 * NAND 存储驱动实现 (仅 BanBox 等有 NAND 的平台)
 * ============================================ */

#ifndef BANDATAHUB

static BG_ERR synth_nand_init(const char *path, BG_Storage_Mode_t mode)
{
    BG_ERR ret;

    if (g_nand_driver_state.initialized) {
        return SUCCESS;
    }

    /* 验证 NAND 存储已初始化 */
    if (!g_synth_status.storage_ready) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "NAND store not available");
        return ENABLE_DEVICE_NOT_READY;
    }

    /* 验证 SF2 数据存在 */
    ret = synth_verify_sf2_in_nand();
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "SF2 data not found in NAND: %d", ret);
        return ret;
    }

    g_nand_driver_state.initialized = true;
    BG_LOG_I(BG_LOG_TAG_SYNTH, "NAND storage driver initialized");

    return SUCCESS;
}

static BG_ERR synth_nand_deinit(void)
{
    g_nand_driver_state.initialized = false;
    BG_LOG_I(BG_LOG_TAG_SYNTH, "NAND storage driver deinitialized");
    return SUCCESS;
}

static int synth_nand_read(uint32_t offset, void *buffer, size_t size)
{
    FlashDevice_t *nand_dev;
    BG_ERR ret;

    if (!g_nand_driver_state.initialized) {
        return -1;
    }

    if (offset >= g_nand_driver_state.sf2_size) {
        return 0; /* EOF */
    }

    if (offset + size > g_nand_driver_state.sf2_size) {
        size = g_nand_driver_state.sf2_size - offset;
    }

    /* 从 NAND 读取 SF2 数据 */
    nand_dev = FlashDevices_GetNandFlash();
    if (!nand_dev) {
        return -1;
    }

    ret = nand_dev->ops->read(nand_dev,
                                     g_nand_driver_state.sf2_data_start + offset,
                                     (uint8_t *)buffer, size);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "NAND read failed: offset=%u, size=%u", offset, size);
        return -1;
    }

    return (int)size;
}

static int synth_nand_write(uint32_t offset, const void *buffer, size_t size)
{
    /* NAND 存储驱动为只读 */
    BG_LOG_E(BG_LOG_TAG_SYNTH, "NAND storage driver is read-only");
    return -1;
}

static BG_ERR synth_nand_erase(uint32_t offset, size_t size)
{
    /* NAND 存储驱动为只读 */
    BG_LOG_E(BG_LOG_TAG_SYNTH, "NAND storage driver is read-only");
    return ENABLE_PERMISSION_DENIED;
}

static BG_ERR synth_nand_sync(void)
{
    /* NAND 自动同步 */
    return SUCCESS;
}

static BG_ERR synth_nand_get_info(uint32_t *total_size, uint32_t *free_size)
{
    if (total_size) *total_size = g_nand_driver_state.sf2_size;
    if (free_size) *free_size = 0; /* 只读 */
    return SUCCESS;
}

#endif /* !BANDATAHUB */

/* ============================================
 * 拷贝进度跟踪
 * ============================================ */

static volatile uint32_t g_copy_progress_bytes_done = 0;
static volatile uint32_t g_copy_progress_bytes_total = 0;

/**
 * 更新拷贝进度
 */
static void synth_update_copy_progress(uint32_t bytes_done, uint32_t bytes_total)
{
    g_copy_progress_bytes_done = bytes_done;
    g_copy_progress_bytes_total = bytes_total;
}

/**
 * 从 SF2 文件中查找音符数据的偏移和大小
 * 这是一个简化的实现，实际应该解析 SF2 的 pdta chunk
 *
 * 注意：这是一个临时的简化实现。在实际的SF2文件中，
 * 每个音符(sample)有不同的大小和位置，需要解析：
 * - shdr chunk (sample headers)
 * - pbag/igen chunks (preset zones)
 * - ibag/igen chunks (instrument zones)
 */
static BG_ERR synth_locate_note_data(uint8_t note, uint8_t program,
                                   uint32_t *nand_offset, uint32_t *data_size)
{
    /* 临时实现：返回一个固定的测试数据位置 */
    /* TODO: 实现真正的SF2解析逻辑 */

    /* 简化的映射：每个程序128个音符，每个音符64KB数据 */
    uint32_t program_offset = program * 128 * 65536; /* 128 notes * 64KB each */
    uint32_t note_offset = note * 65536; /* 64KB per note */

#ifdef BANDATAHUB
    /* BanDataHub: SF2 存储在 PSRAM 样本区起始 (0x000000)
     * 直接返回 SF2 文件内偏移 (不使用 NAND 绝对地址) */
    *nand_offset = program_offset + note_offset;
    *data_size = 65536;

    /* 检查是否超出 SF2 文件范围 */
    if (*nand_offset + *data_size > g_synth_status.sf2_size) {
        *nand_offset = 0;
        *data_size = 4096; /* 4KB最小值 */
    }
#else
    *nand_offset = SYNTH_SF2_NAND_BLOB_OFFSET + SYNTH_SF2_HEADER_SIZE +
                   program_offset + note_offset;
    *data_size = 65536; /* 64KB - 适合PSRAM缓冲区大小 */

    /* 检查是否超出 SF2 文件范围 */
    if (*nand_offset + *data_size > SYNTH_SF2_NAND_BLOB_OFFSET +
                                   SYNTH_SF2_HEADER_SIZE + g_synth_status.sf2_size) {
        BG_LOG_W(BG_LOG_TAG_SYNTH, "Note data out of range: note=%u, program=%u, "
                 "offset=0x%08X, size=%u, file_size=%u",
                 note, program, *nand_offset, *data_size, g_synth_status.sf2_size);

        /* 返回一个安全的默认值 */
        *nand_offset = SYNTH_SF2_NAND_BLOB_OFFSET + SYNTH_SF2_HEADER_SIZE;
        *data_size = 4096; /* 4KB最小值 */
    }
#endif

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Located note data: note=%u, program=%u, offset=0x%08X, size=%u",
             note, program, *nand_offset, *data_size);

    return SUCCESS;
}

/**
 * 根据音符查找对应的缓冲区ID
 */
static uint32_t synth_find_buffer_by_note(uint8_t note, uint8_t program)
{
    uint32_t i;

    for (i = 0; i < PSRAM_MAX_NOTE_BUFFERS; i++) {
        PSRAM_BufferInfo_t info;
        if (PSRAM_GetBufferInfo(i, &info) == SUCCESS) {
            if ((info.state == PSRAM_BUFFER_READY || info.state == PSRAM_BUFFER_PLAYING) &&
                info.note_number == note && info.program == program) {
                return i;
            }
        }
    }
    return UINT32_MAX; /* 未找到 */
}

#ifndef BANDATAHUB
static BG_ERR synth_copy_sf2_to_nand(const char *filename)
{
    BG_ERR ret;
    FAT32_FileHandle_t file_handle;
    SYNTH_SF2NandHeader_t header;
    uint8_t *chunk_buffer;
    uint32_t bytes_copied = 0;
    uint32_t total_size;
    uint32_t checksum;
    uint32_t remaining;
    FAT32_FileHandle_t checksum_handle;
    FlashDevice_t *nand_dev;
    uint32_t nand_offset;
    uint32_t chunk_size;
    int32_t read_bytes;

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Copying SF2 file to NAND: %s", filename);

    /* 打开 SD 卡上的 SF2 文件 */
    ret = FAT32_OpenFile(filename, &file_handle);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "Failed to open SF2 file: %s", filename);
        return ret;
    }

    total_size = file_handle.info.size;
    BG_LOG_I(BG_LOG_TAG_SYNTH, "SF2 file size: %u bytes", total_size);

    if (total_size > SYNTH_SF2_MAX_BLOB_SIZE) {
        FAT32_CloseFile(&file_handle);
        BG_LOG_E(BG_LOG_TAG_SYNTH, "SF2 file too large: %u > %u", total_size, SYNTH_SF2_MAX_BLOB_SIZE);
        return ENABLE_INVALID_INPUT;
    }

    /* 分配临时缓冲区 */
    chunk_buffer = (uint8_t *)malloc(SYNTH_SF2_COPY_CHUNK_SIZE);
    if (!chunk_buffer) {
        FAT32_CloseFile(&file_handle);
        return ENABLE_OUT_OF_MEMORY;
    }

    /* 准备 NAND 头部 */
    memset(&header, 0, sizeof(header));
    header.magic = SYNTH_SF2_HEADER_MAGIC;
    header.sf2_size = total_size;
    header.version = 1;
    strncpy(header.filename, filename, sizeof(header.filename) - 1);

    /* 计算整个文件的滚动校验和 (使用独立句柄，不改变原始读取位置) */
    checksum_handle = file_handle;
    checksum = 0x12345678;
    remaining = total_size;

    while (remaining > 0) {
        const uint8_t *cptr;
        uint32_t j;

        chunk_size = (remaining > SYNTH_SF2_COPY_CHUNK_SIZE) ?
                     SYNTH_SF2_COPY_CHUNK_SIZE : remaining;
        read_bytes = FAT32_ReadFile(&checksum_handle, chunk_buffer, chunk_size);
        if (read_bytes < 0) {
            free(chunk_buffer);
            FAT32_CloseFile(&file_handle);
            return ENABLE_IO_ERROR;
        }
        /* 滚动校验和累加 */
        cptr = chunk_buffer;
        for (j = 0; j < (uint32_t)read_bytes; j++) {
            checksum = (checksum << 5) + checksum + cptr[j];
        }
        remaining -= (uint32_t)read_bytes;
    }
    header.checksum = checksum;

    /* 写入 NAND 头部 */
    nand_dev = FlashDevices_GetNandFlash();
    if (!nand_dev) {
        free(chunk_buffer);
        FAT32_CloseFile(&file_handle);
        return ENABLE_DEVICE_NOT_READY;
    }

    ret = nand_dev->ops->write(nand_dev, SYNTH_SF2_NAND_BLOB_OFFSET,
                              (uint8_t *)&header, sizeof(header));
    if (ret != SUCCESS) {
        free(chunk_buffer);
        FAT32_CloseFile(&file_handle);
        BG_LOG_E(BG_LOG_TAG_SYNTH, "Failed to write NAND header");
        return ret;
    }

    /* 初始化进度跟踪 */
    synth_update_copy_progress(0, total_size);

    /* 复制 SF2 数据 */
    nand_offset = SYNTH_SF2_NAND_BLOB_OFFSET + SYNTH_SF2_HEADER_SIZE;
    remaining = total_size;

    while (remaining > 0) {
        chunk_size = (remaining > SYNTH_SF2_COPY_CHUNK_SIZE) ?
                     SYNTH_SF2_COPY_CHUNK_SIZE : remaining;

        /* 从 SD 读取 */
        read_bytes = FAT32_ReadFile(&file_handle, chunk_buffer, chunk_size);
        if (read_bytes < 0) {
            free(chunk_buffer);
            FAT32_CloseFile(&file_handle);
            BG_LOG_E(BG_LOG_TAG_SYNTH, "Failed to read from SD card");
            return ENABLE_IO_ERROR;
        }

        /* 写入 NAND */
        ret = nand_dev->ops->write(nand_dev, nand_offset, chunk_buffer, (uint32_t)read_bytes);
        if (ret != SUCCESS) {
            free(chunk_buffer);
            FAT32_CloseFile(&file_handle);
            BG_LOG_E(BG_LOG_TAG_SYNTH, "Failed to write to NAND");
            return ret;
        }

        nand_offset += (uint32_t)read_bytes;
        bytes_copied += (uint32_t)read_bytes;
        remaining -= (uint32_t)read_bytes;

        /* 更新进度 */
        synth_update_copy_progress(bytes_copied, total_size);
    }

    free(chunk_buffer);
    FAT32_CloseFile(&file_handle);

    /* 更新状态 */
    g_synth_status.sf2_size = total_size;
    strncpy(g_synth_status.sf2_filename, filename, sizeof(g_synth_status.sf2_filename) - 1);

    BG_LOG_I(BG_LOG_TAG_SYNTH, "SF2 copy completed: %u bytes", total_size);
    return SUCCESS;
}

static BG_ERR synth_verify_sf2_in_nand(void)
{
    SYNTH_SF2NandHeader_t header;
    FlashDevice_t *nand_dev;
    BG_ERR ret;

    nand_dev = FlashDevices_GetNandFlash();
    if (!nand_dev) {
        return ENABLE_DEVICE_NOT_READY;
    }

    /* 读取头部 */
    ret = nand_dev->ops->read(nand_dev, SYNTH_SF2_NAND_BLOB_OFFSET,
                                    (uint8_t *)&header, sizeof(header));
    if (ret != SUCCESS) {
        return ret;
    }

    /* 验证魔数 */
    if (header.magic != SYNTH_SF2_HEADER_MAGIC) {
        BG_LOG_W(BG_LOG_TAG_SYNTH, "Invalid NAND header magic: 0x%08X", header.magic);
        return ENABLE_FORMAT_ERROR;
    }

    /* 验证大小 */
    if (header.sf2_size == 0 || header.sf2_size > SYNTH_SF2_MAX_BLOB_SIZE) {
        BG_LOG_W(BG_LOG_TAG_SYNTH, "Invalid SF2 size in NAND: %u", header.sf2_size);
        return ENABLE_FORMAT_ERROR;
    }

    /* 更新驱动状态 */
    g_nand_driver_state.sf2_size = header.sf2_size;
    g_nand_driver_state.sf2_data_start = SYNTH_SF2_NAND_BLOB_OFFSET + SYNTH_SF2_HEADER_SIZE;

    BG_LOG_I(BG_LOG_TAG_SYNTH, "SF2 verified in NAND: size=%u bytes", header.sf2_size);
    return SUCCESS;
}
#endif /* !BANDATAHUB */

/* ============================================
 * 程序预热状态机实现
 * ============================================ */

void SYNTH_LoadProgram(uint8_t program)
{
    if (!g_synth_status.soundbank_ready) {
        BG_LOG_W(BG_LOG_TAG_SYNTH, "[LoadProgram] Not ready, skip program %u", program);
        return;
    }

    /* 记录待预热 program，重置进度；SYNTH_LoadTick() 会分步执行 */
    g_prefetch_program = program;
    g_prefetch_note    = SYNTH_PREFETCH_NOTE_MIN;
    g_prefetch_pending = 1;

    BG_LOG_I(BG_LOG_TAG_SYNTH, "[LoadProgram] Scheduled prefetch for program %u", program);
}

void SYNTH_LoadTick(void)
{
    uint32_t now;

    if (!g_prefetch_pending) {
        return;
    }

    /* 限制每次调用只执行一步，且两步间距不少于 SYNTH_PREFETCH_INTERVAL_MS */
    now = bg_get_tick_ms();
    if ((now - g_prefetch_last_ms) < SYNTH_PREFETCH_INTERVAL_MS) {
        return;
    }
    g_prefetch_last_ms = now;

    /* 执行一步预热 */
    if (PSRAM_PrefetchNote(g_prefetch_note, g_prefetch_program) != SUCCESS) {
        /* PSRAM 满或出错，停止本轮预热 */
        BG_LOG_W(BG_LOG_TAG_SYNTH, "[LoadTick] Prefetch stopped at note %u (PSRAM full?)",
                 g_prefetch_note);
        g_prefetch_pending = 0;
        return;
    }

    g_prefetch_note += SYNTH_PREFETCH_STEP;

    if (g_prefetch_note > SYNTH_PREFETCH_NOTE_MAX) {
        BG_LOG_I(BG_LOG_TAG_SYNTH, "[LoadTick] Prefetch complete for program %u",
                 g_prefetch_program);
        g_prefetch_pending = 0;
    }
}

/* ============================================
 * 公开接口实现
 * ============================================ */

BG_ERR SYNTH_SDNANDPSRAM_Init(void)
{
    BG_ERR ret;
#ifndef BANDATAHUB
    FAT32_FileInfo_t sf2_info;
#endif

#ifdef BANDATAHUB
    /* ============================================
     * BanDataHub: SD→PSRAM 二级直读架构 (无 NAND)
     *
     * FAT32_Init() 内部已注册 SD卡 diskio (BanDataHub特有)
     * bg_storage_driver_bandatahub.Init() 处理 SF2 查找+加载
     * ============================================ */
    BG_LOG_I(BG_LOG_TAG_SYNTH, "Initializing SD+PSRAM synthesizer integration (BanDataHub)");

    /* 1. 初始化 FAT32 (内部注册SD卡diskio + 挂载) */
    ret = FAT32_Init();
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "FAT32 init failed: %d", ret);
        return ret;
    }

    /* 2. 使用 BanDataHub PSRAM 存储驱动加载 SF2 到 PSRAM
     *    内部流程: 获取PSRAM设备 → 查找SD卡上SF2文件 → 分块加载到PSRAM
     */
    BG_Storage.SetDriver(&bg_storage_driver_bandatahub);
    ret = BG_Storage.Init(NULL, BG_STORAGE_MODE_READ_ONLY);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "BG_Storage (bandatahub) init failed: %d", ret);
        return ret;
    }

    /* 3. 初始化 PSRAM 音符缓冲区 (在 PSRAM 堆区, 用于复音缓存) */
    ret = PSRAM_BufferInit();
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "PSRAM buffer init failed: %d", ret);
        return ret;
    }

    /* 4. 初始化 soundbank 管理器 (通过 BG_Storage 从 PSRAM 读取 SF2) */
    ret = soundbank_manager.Init(0);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "Soundbank manager init failed: %d", ret);
        return ret;
    }

    /* 更新状态 */
    g_synth_status.storage_ready = true;
    g_synth_status.psram_ready = true;
    g_synth_status.soundbank_ready = true;

    BG_LOG_I(BG_LOG_TAG_SYNTH, "SD+PSRAM synthesizer integration initialized (BanDataHub)");
    return SUCCESS;

#else
    /* ============================================
     * 标准平台: SD→NAND→PSRAM 三级存储架构
     * ============================================ */
    BG_LOG_I(BG_LOG_TAG_SYNTH, "Initializing SD+NAND+PSRAM synthesizer integration");

    /* 0. 注册 SD 卡磁盘 IO 驱动并选中 */
    FAT32_DiskIO_Register(FAT32_DISK_SDCARD, &fat32_diskio_sdcard);
    FAT32_DiskIO_Select(FAT32_DISK_SDCARD);

    /* 1. 初始化 FAT32 读取器 */
    ret = FAT32_Init();
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "FAT32 init failed: %d", ret);
        return ret;
    }

    /* 2. 查找 SD 卡上的 SF2 文件 */
    ret = FAT32_FindFile("*.sf2", &sf2_info);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "No SF2 file found on SD card");
        return ret;
    }

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Found SF2 file: %s (%u bytes)", sf2_info.name, sf2_info.size);

    /* 3. 初始化 NAND 存储 */
    ret = NAND_StoreInit();
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "NAND store init failed: %d", ret);
        return ret;
    }

    /* 4. 检查 SF2 是否已在 NAND 中 */
    ret = synth_verify_sf2_in_nand();
    if (ret != SUCCESS) {
        /* 不存在，执行拷贝 */
        BG_LOG_I(BG_LOG_TAG_SYNTH, "SF2 not found in NAND, copying from SD...");
        ret = synth_copy_sf2_to_nand(sf2_info.name);
        if (ret != SUCCESS) {
            BG_LOG_E(BG_LOG_TAG_SYNTH, "SF2 copy failed: %d", ret);
            return ret;
        }
    }

    /* 5. 初始化 PSRAM 缓冲区 */
    ret = PSRAM_BufferInit();
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "PSRAM buffer init failed: %d", ret);
        return ret;
    }

    /* 6. 安装 NAND 存储驱动 */
    BG_Storage.SetDriver(&synth_nand_storage_driver);
    ret = BG_Storage.Init("nand_sf2", BG_STORAGE_MODE_READ_ONLY);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "BG_Storage init failed: %d", ret);
        return ret;
    }

    /* 7. 初始化 soundbank 管理器 */
    ret = soundbank_manager.Init(0); /* offset_addr = 0 (相对于 NAND SF2 数据) */
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "Soundbank manager init failed: %d", ret);
        return ret;
    }

    /* 更新状态 */
    g_synth_status.storage_ready = true;
    g_synth_status.psram_ready = true;
    g_synth_status.soundbank_ready = true;

    BG_LOG_I(BG_LOG_TAG_SYNTH, "SD+NAND+PSRAM synthesizer integration initialized");
    return SUCCESS;
#endif /* BANDATAHUB */
}

void SYNTH_SDNANDPSRAM_DeInit(void)
{
    /* 取消未完成的预热 */
    g_prefetch_pending = 0;

    if (g_synth_status.soundbank_ready) {
        soundbank_manager.DeInit();
        g_synth_status.soundbank_ready = false;
    }

    if (g_synth_status.storage_ready) {
        BG_Storage.DeInit();
        g_synth_status.storage_ready = false;
    }

    if (g_synth_status.psram_ready) {
        PSRAM_BufferDeInit();
        g_synth_status.psram_ready = false;
    }

#ifndef BANDATAHUB
    NAND_StoreDeInit();
#endif
    FAT32_DeInit();

    memset(&g_synth_status, 0, sizeof(g_synth_status));
#ifndef BANDATAHUB
    memset(&g_nand_driver_state, 0, sizeof(g_nand_driver_state));
#endif

    BG_LOG_I(BG_LOG_TAG_SYNTH, "SD+PSRAM synthesizer integration deinitialized");
}

BG_ERR SYNTH_SDNANDPSRAM_ReloadFromSD(void)
{
    BG_ERR ret;
    FAT32_FileInfo_t sf2_info;

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Reloading SF2 from SD card");

    /* 查找 SF2 文件 */
    ret = FAT32_FindFile("*.sf2", &sf2_info);
    if (ret != SUCCESS) {
        return ret;
    }

#ifdef BANDATAHUB
    /* BanDataHub: 直接重载到 PSRAM */
    BG_Storage.SetDriver(&bg_storage_driver_bandatahub);
    ret = BG_Storage.Init(NULL, BG_STORAGE_MODE_READ_ONLY);
    if (ret == SUCCESS) {
        g_synth_status.storage_ready = true;
        g_synth_status.psram_ready = true;
        g_synth_status.soundbank_ready = true;
    }
    return ret;
#else
    /* 拷贝到 NAND */
    return synth_copy_sf2_to_nand(sf2_info.name);
#endif
}

void SYNTH_SDNANDPSRAM_GetStatus(SYNTH_Status_t *status)
{
    if (status) {
        memcpy(status, &g_synth_status, sizeof(SYNTH_Status_t));
    }
}

void SYNTH_SDNANDPSRAM_GetCopyProgress(uint32_t *bytes_done, uint32_t *bytes_total)
{
    if (bytes_done) *bytes_done = g_copy_progress_bytes_done;
    if (bytes_total) *bytes_total = g_copy_progress_bytes_total;
}

void SYNTH_SDNANDPSRAM_NoteOn(uint8_t note, uint8_t velocity, uint8_t program)
{
    BG_ERR ret;
    PSRAM_BufferAlloc_t alloc_result;
    uint32_t source_offset;
    uint32_t data_size;
    PSRAM_NoteRequest_t request;

    if (!g_synth_status.psram_ready || !g_synth_status.soundbank_ready) {
        return;
    }

    /* 准备音符请求 */
    memset(&request, 0, sizeof(request));
    request.note = note;
    request.velocity = velocity;
    request.program = program;
    request.sample_rate = 44100; /* 默认采样率 */
    request.high_priority = (velocity > 100); /* 高力度优先 */

    /* 请求 PSRAM 缓冲区 */
    ret = PSRAM_RequestNoteBuffer(&request, &alloc_result);
    if (ret != SUCCESS) {
        BG_LOG_W(BG_LOG_TAG_SYNTH, "Failed to allocate PSRAM buffer for note %u", note);
        return;
    }

    /* 如果是缓存命中，直接激活声部 */
    if (alloc_result.from_cache) {
        sf2_note_on(note, velocity, program);
        return;
    }

    /* 缓存未命中，需要加载数据 (NAND或PSRAM) */
    ret = synth_locate_note_data(note, program, &source_offset, &data_size);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "Failed to locate note data: note=%u, program=%u", note, program);
        PSRAM_ReleaseNoteBuffer(alloc_result.buffer_id);
        return;
    }

    /* 异步加载到 PSRAM 缓冲区 */
    ret = PSRAM_LoadNoteData(alloc_result.buffer_id, source_offset, data_size);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SYNTH, "Failed to load note data: note=%u, buffer=%u",
                 note, alloc_result.buffer_id);
        PSRAM_ReleaseNoteBuffer(alloc_result.buffer_id);
        return;
    }

    /* 激活声部 (数据加载完成后会自动就绪) */
    sf2_note_on(note, velocity, program);

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Note ON: note=%u, program=%u, buffer=%u",
             note, program, alloc_result.buffer_id);
}

void SYNTH_SDNANDPSRAM_NoteOff(uint8_t note, uint8_t program)
{
    /* 查找对应的缓冲区ID并释放 */
    uint32_t buffer_id = synth_find_buffer_by_note(note, program);
    if (buffer_id != UINT32_MAX) {
        PSRAM_ReleaseNoteBuffer(buffer_id);
        BG_LOG_I(BG_LOG_TAG_SYNTH, "Released buffer for note=%u, program=%u, buffer=%u",
                 note, program, buffer_id);
    }

    /* 关闭声部 */
    sf2_note_off(note, program);

    BG_LOG_I(BG_LOG_TAG_SYNTH, "Note OFF: note=%u, program=%u", note, program);
}

#endif /* SYNTH_SD_NAND_PSRAM_EN */