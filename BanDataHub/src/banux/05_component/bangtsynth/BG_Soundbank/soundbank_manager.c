#include "bangtsynth_legacy.h"
#if BANGTSYNTH_LEGACY

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "soundbank_manager.h"
#include "bgs_parser.h"
#include "sf2_parser.h"
#include "bg_storage.h"
#include "bg_config.h"
#include "bg_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * 音源格式管理器实现
 */

/* Soundbank Pack 格式定义 */
#define BGSP_MAGIC          0x50534742  // "BGSP"
#define BGSP_VERSION        1
#define BGSP_HEADER_SIZE    256
#define BGSP_ENTRY_SIZE     64

typedef struct {
    char name[48];
    uint32_t type;      // 0=BGS, 1=SF2
    uint32_t offset;
    uint32_t size;
    uint32_t reserved;
} __attribute__((packed)) BGSP_FileEntry;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t file_count;
    uint8_t reserved[244];
} __attribute__((packed)) BGSP_Header;

/* 内部状态 */
static SoundBank_Format g_current_format = SOUNDBANK_FORMAT_UNKNOWN;
static uint8_t g_initialized = 0;
static char g_info_buffer[256];
static uint32_t g_base_offset = 0;          // 用户指定的基地址偏移
static uint32_t g_current_file_offset = 0;  // 当前音源在 bin 中的偏移(相对于base_offset)
static uint32_t g_current_file_size = 0;    // 当前音源的大小

/* 固定的音源bin文件路径 */
#define SOUNDBANK_BIN_PATH  "soundbank.bin"

/* 内部函数声明 */
static BG_ERR soundbank_init(uint32_t offset_addr);
static BG_ERR soundbank_deinit(void);
static uint8_t soundbank_read_samples(short *data, uint32_t note, uint32_t count, uint8_t program);
static SoundBank_Format soundbank_get_format(void);
static const char* soundbank_get_info(void);
static BGS_Data* soundbank_get_bgs_data(void);
static void soundbank_note_on(uint8_t note, uint8_t velocity, uint8_t program);
static void soundbank_note_off(uint8_t note, uint8_t program);
static void soundbank_all_note_off(uint8_t program);
static void soundbank_pitch_bend(uint8_t channel, int16_t value);
static void soundbank_cc(uint8_t channel, uint8_t cc_num, uint8_t value);
static void soundbank_set_channel(uint8_t channel);
static BG_ERR soundbank_download(const char *data_source, uint32_t offset, size_t size,
                                 soundbank_download_progress_cb_t progress_cb, void *user_data);
static uint8_t soundbank_read_active_samples(short *data, uint32_t count);



/* 接口实例 */
SoundBank_Manager soundbank_manager = {
    .Init = soundbank_init,
    .DeInit = soundbank_deinit,
    .ReadSamples = soundbank_read_samples,
    .GetFormat = soundbank_get_format,
    .GetInfo = soundbank_get_info,
    .GetBGSData = soundbank_get_bgs_data,
    .NoteOn = soundbank_note_on,
    .NoteOff = soundbank_note_off,
    .AllNoteOff = soundbank_all_note_off,
    .PitchBend = soundbank_pitch_bend,
    .CC = soundbank_cc,
    .SetChannel = soundbank_set_channel,
    .Download = soundbank_download,
    .ReadActiveSamples = soundbank_read_active_samples,
};

/**
 * 初始化音源(从固定bin文件的指定偏移加载)
 */
static BG_ERR soundbank_init(uint32_t offset_addr)
{
    BG_ERR ret;
    uint32_t magic;
    BGSP_Header header;
    BGSP_FileEntry entry;
    uint32_t entry_offset;
    uint32_t total_size;
    uint32_t free_size;

    if (g_initialized) {
        BG_LOG_W(BG_LOG_TAG_SOUNDBANK, " Already initialized, deinitializing first\n");
        soundbank_deinit();
    }
    
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Loading soundbank from offset 0x%08X\n", offset_addr);
    
    /* 保存基地址偏移 */
    g_base_offset = offset_addr;
    
    /* 初始化存储层,使用固定的bin文件路径 */
    ret = BG_Storage.Init(SOUNDBANK_BIN_PATH, BG_STORAGE_MODE_READ_ONLY);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Failed to init storage: %d\n", ret);
        return ret;
    }
    
    /* 读取文件头,检测是BGSP打包文件还是单个音源文件 */
    if (BG_Storage.Read(g_base_offset, &magic, sizeof(uint32_t)) != sizeof(uint32_t)) {
        BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Failed to read file magic\n");
        BG_Storage.DeInit();
        return ENABLE_INVALID_INPUT;
    }
    
    /* 检查是否为BGSP打包文件 */
    if (magic == BGSP_MAGIC) {
        /* BGSP打包文件格式 */
        if (BG_Storage.Read(g_base_offset, &header, sizeof(BGSP_Header)) != sizeof(BGSP_Header)) {
            BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Failed to read BGSP header\n");
            BG_Storage.DeInit();
            return ENABLE_INVALID_INPUT;
        }
        
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "BGSP pack version %u, %u files\n", 
                 header.version, header.file_count);
        
        if (header.file_count == 0) {
            BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "No files in soundbank\n");
            BG_Storage.DeInit();
            return ENABLE_INVALID_INPUT;
        }
        
        /* 读取第一个文件条目 */
        entry_offset = g_base_offset + BGSP_HEADER_SIZE;
        
        if (BG_Storage.Read(entry_offset, &entry, sizeof(BGSP_FileEntry)) != sizeof(BGSP_FileEntry)) {
            BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Failed to read file entry\n");
            BG_Storage.DeInit();
            return ENABLE_INVALID_INPUT;
        }
        
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Loading: %s (type=%u, off=0x%X, size=%u)\n",
                 entry.name, entry.type, entry.offset, entry.size);
        
        /* 保存当前文件信息(相对于base_offset) */
        g_current_file_offset = entry.offset;
        g_current_file_size = entry.size;
        
        /* 根据类型初始化解析器 */
        if (entry.type == 0) {
            g_current_format = SOUNDBANK_FORMAT_BG;
            ret = bgs_init();
            if (ret == SUCCESS) {
                snprintf(g_info_buffer, sizeof(g_info_buffer), "BG Format - %s", entry.name);
            }
        } else if (entry.type == 1) {
            g_current_format = SOUNDBANK_FORMAT_SF2;
            ret = sf2_parser.Init(NULL);
            if (ret == SUCCESS) {
                snprintf(g_info_buffer, sizeof(g_info_buffer), "SF2 Format - %s", entry.name);
            }
        } else {
            BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Unknown file type: %u\n", entry.type);
            ret = ENABLE_INVALID_INPUT;
        }
    } 
    else if (magic == 0x46464952) {  // "RIFF" - SF2文件
        /* 直接加载SF2文件 */
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Loading single SF2 file from offset\n");
        
        total_size = 0;
        free_size = 0;
        BG_Storage.GetInfo(&total_size, &free_size);
        
        g_current_file_offset = g_base_offset;
        g_current_file_size = total_size;
        g_current_format = SOUNDBANK_FORMAT_SF2;
        
        ret = sf2_parser.Init(NULL);
        if (ret == SUCCESS) {
            snprintf(g_info_buffer, sizeof(g_info_buffer), "SF2 Format");
        }
    }
    else {
        /* 尝试作为BGS文件加载 */
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Loading single BGS file from offset\n");
        
        total_size = 0;
        free_size = 0;
        BG_Storage.GetInfo(&total_size, &free_size);
        
        g_current_file_offset = g_base_offset;
        g_current_file_size = total_size;
        g_current_format = SOUNDBANK_FORMAT_BG;
        
        ret = bgs_init();
        if (ret == SUCCESS) {
            snprintf(g_info_buffer, sizeof(g_info_buffer), "BG Format");
        }
    }
    
    if (ret == SUCCESS) {
        g_initialized = 1;
        BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Initialized successfully: %s\n", g_info_buffer);
    } else {
        BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Initialization failed\n");
        g_current_format = SOUNDBANK_FORMAT_UNKNOWN;
        BG_Storage.DeInit();
    }
    
    return ret;
}

/**
 * 释放音源资源
 */
static BG_ERR soundbank_deinit(void)
{
    if (!g_initialized) {
        return SUCCESS;
    }
    
    BG_ERR result = SUCCESS;
    
    switch (g_current_format) {
        case SOUNDBANK_FORMAT_BG:
            result = bgs_deinit();
            break;
            
        case SOUNDBANK_FORMAT_SF2:
            result = sf2_parser.DeInit();
            break;
            
        default:
            break;
    }
    
    /* 释放存储层 */
    BG_Storage.DeInit();
    
    g_current_format = SOUNDBANK_FORMAT_UNKNOWN;
    g_initialized = 0;
    g_current_file_offset = 0;
    g_current_file_size = 0;
    
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Deinitialized\n");
    
    return result;
}

/**
 * 读取音频样本 (统一接口)
 */
static uint8_t soundbank_read_samples(short *data, uint32_t note, uint32_t count, uint8_t program)
{
    if (!g_initialized || !data) {
        static uint8_t rs_uninit_cnt = 0;
        if (++rs_uninit_cnt <= 3) {
            printf("[SBM] RS_FAIL: g_init=%u data=%p note=%u prog=%u\n",
                g_initialized, data, (unsigned)note, (unsigned)program);
        }
        return 0;
    }
    
    switch (g_current_format) {
        case SOUNDBANK_FORMAT_BG:
            /* v2.0: BGS使用预选采样模式,在NoteOn时已确定采样索引 */
            return bgs_read_callback(data, note, count, program);
            
        case SOUNDBANK_FORMAT_SF2:
            return sf2_parser.Callback(data, note, count, program);
            
        default:
            /* 未知格式，返回静音 */
            printf("[SBM] RS_UNKNOWN_FMT: fmt=%d note=%u\n", g_current_format, (unsigned)note);
            memset(data, 0, count * sizeof(short));
            return 0;
    }
}

/**
 * 获取当前音源格式
 */
static SoundBank_Format soundbank_get_format(void)
{
    return g_current_format;
}

/**
 * 获取音源信息字符�?
 */
static const char* soundbank_get_info(void)
{
    if (!g_initialized) {
        return "Not initialized";
    }
    
    return g_info_buffer;
}

/**
 * 获取BGS格式数据
 */
static BGS_Data* soundbank_get_bgs_data(void)
{
    if (g_current_format == SOUNDBANK_FORMAT_BG) {
        return bgs_get_data();
    }
    return NULL;
}

/**
 * 统一的音符开启接�?
 */
static void soundbank_note_on(uint8_t note, uint8_t velocity, uint8_t program)
{
    printf("[SBM] NoteOn: fmt=%d note=%u vel=%u prog=%u\n",
           g_current_format, note, velocity, program);
    if (g_current_format == SOUNDBANK_FORMAT_BG) {
        bgs_note_on(note, velocity, program);
    } else if (g_current_format == SOUNDBANK_FORMAT_SF2) {
        sf2_note_on(note, velocity, program);
    }
}

/**
 * 统一的音符关闭接�?
 */
static void soundbank_note_off(uint8_t note, uint8_t program)
{
    if (g_current_format == SOUNDBANK_FORMAT_BG) {
        bgs_note_off(note, program);
    } else if (g_current_format == SOUNDBANK_FORMAT_SF2) {
        sf2_note_off(note, program);
    }
}

/**
 * 统一的全部音符关闭接口
 */
static void soundbank_all_note_off(uint8_t program)
{
    if (g_current_format == SOUNDBANK_FORMAT_BG) {
        bgs_all_note_off(program);
    } else if (g_current_format == SOUNDBANK_FORMAT_SF2) {
        sf2_reset_all_notes(program);
    }
}

static void soundbank_pitch_bend(uint8_t channel, int16_t value)
{
    (void)channel;
    (void)value;
    /* 旧 BG_Soundbank 路径未实现弯音；完整实现见 02_core/soundbank */
}

static void soundbank_cc(uint8_t channel, uint8_t cc_num, uint8_t value)
{
    (void)channel;
    (void)cc_num;
    (void)value;
}

static void soundbank_set_channel(uint8_t channel)
{
    (void)channel;
}

/* ============================================
 * 内部辅助函数 (供解析器使用)
 * ============================================ */

/**
 * 从存储层读取当前音源数据
 */
int soundbank_storage_read(uint32_t offset, void *buffer, size_t size)
{
    /* 注意: 不检查 g_initialized,因为这个函数会在解析器初始化时被调用 */
    
    /* 计算绝对偏移: base_offset + file_offset + 数据内偏移 */
    uint32_t absolute_offset = g_current_file_offset + offset;
    
    /* 检查是否超出当前文件范围 */
    if (g_current_file_size > 0 && offset + size > g_current_file_size) {
        BG_LOG_W(BG_LOG_TAG_SOUNDBANK, "Read exceeds file boundary (offset=%u, size=%zu, file_size=%u)\n", 
                 offset, size, g_current_file_size);
        return -1;
    }
    
    return BG_Storage.Read(absolute_offset, buffer, size);
}

/**
 * 读取所有活跃声部的混合音频 (统一接口)
 * 注: 现在 NoteOn/Off 和 ReadActiveSamples 均在主任务回调中执行,
 *     无跨任务共享内存问题
 */
static uint8_t soundbank_read_active_samples(short *data, uint32_t count)
{
    if (!g_initialized || !data) {
        if (data) memset(data, 0, count * sizeof(short));
        return 0;
    }

    switch (g_current_format) {
        case SOUNDBANK_FORMAT_SF2:
            return sf2_read_active_samples(data, count);

        default:
            memset(data, 0, count * sizeof(short));
            return 0;
    }
}

/**
 * 获取当前音源文件在存储层中的偏移
 */
uint32_t soundbank_get_file_offset(void)
{
    return g_current_file_offset;
}

/**
 * 获取当前音源文件大小
 */
uint32_t soundbank_get_file_size(void)
{
    return g_current_file_size;
}

/* ============================================
 * 下载接口实现 (可选功能, ENABLE_SOUNDBANK_DOWNLOAD 控制)
 * ============================================ */

#if ENABLE_SOUNDBANK_DOWNLOAD

/**
 * 平台相关的数据读取接口声明
 * 由 HAL 层的平台驱动实现(bg_download_port_xxx.c)
 */
extern int bg_download_port_read(const char *source, void *buffer, size_t size, size_t *bytes_read);

/**
 * 下载音源数据到存储设备
 */
static BG_ERR soundbank_download(const char *data_source, uint32_t offset, size_t size,
                                 soundbank_download_progress_cb_t progress_cb, void *user_data)
{
    BG_ERR ret;
    uint8_t *buffer;
    size_t total_written = 0;
    uint32_t current_offset;
    size_t bytes_read;
    int read_ret;
    int write_ret;

    if (!data_source) {
        BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Invalid data source\n");
        return ENABLE_INVALID_INPUT;
    }
    
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Starting download from '%s' to offset 0x%08X\n", 
             data_source, offset);
    
    /* 初始化存储层为写模式 */
    ret = BG_Storage.Init(SOUNDBANK_BIN_PATH, BG_STORAGE_MODE_WRITE_ONLY);
    if (ret != SUCCESS) {
        BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Failed to init storage for writing: %d\n", ret);
        return ret;
    }
    
    /* 缓冲区: 64KB 分块传输 */
    #define DOWNLOAD_BUFFER_SIZE (64 * 1024)
    buffer = (uint8_t*)malloc(DOWNLOAD_BUFFER_SIZE);
    if (!buffer) {
        BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Failed to allocate download buffer\n");
        BG_Storage.DeInit();
        return ENABLE_INVALID_INPUT;
    }
    
    current_offset = offset;
    
    /* 循环读取并写入数据 */
    while (1) {
        /* 调用平台相关的读取接口 (由HAL层实现) */
        bytes_read = 0;
        read_ret = bg_download_port_read(data_source, buffer, DOWNLOAD_BUFFER_SIZE, &bytes_read);
        
        if (read_ret < 0) {
            BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Failed to read from data source\n");
            ret = ENABLE_INVALID_INPUT;
            break;
        }
        
        if (bytes_read == 0) {
            /* 数据读取完毕 */
            break;
        }
        
        /* 写入存储层 */
        write_ret = BG_Storage.Write(current_offset, buffer, bytes_read);
        if (write_ret < 0 || (size_t)write_ret != bytes_read) {
            BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Failed to write to storage\n");
            ret = ENABLE_INVALID_INPUT;
            break;
        }
        
        total_written += bytes_read;
        current_offset += bytes_read;
        
        /* 调用进度回调 */
        if (progress_cb) {
            progress_cb(total_written, size > 0 ? size : total_written, user_data);
        }
        
        /* 如果指定了大小,检查是否完成 */
        if (size > 0 && total_written >= size) {
            break;
        }
    }
    
    /* 同步数据到存储设备 */
    if (ret == SUCCESS) {
        ret = BG_Storage.Sync();
        if (ret != SUCCESS) {
            BG_LOG_E(BG_LOG_TAG_SOUNDBANK, "Failed to sync storage\n");
        } else {
            BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Download completed: %zu bytes written\n", total_written);
        }
    }
    
    /* 清理资源 */
    free(buffer);
    BG_Storage.DeInit();
    
    return ret;
}

#else  /* !ENABLE_SOUNDBANK_DOWNLOAD */

/* 下载功能禁用时的桩函数 */
static BG_ERR soundbank_download(const char *data_source, uint32_t offset, size_t size,
                                 soundbank_download_progress_cb_t progress_cb, void *user_data)
{
    (void)data_source; (void)offset; (void)size; (void)progress_cb; (void)user_data;
    BG_LOG_W(BG_LOG_TAG_SOUNDBANK, "Download disabled (ENABLE_SOUNDBANK_DOWNLOAD=0)\n");
    return ENABLE_INVALID_INPUT;
}

#endif /* ENABLE_SOUNDBANK_DOWNLOAD */

#endif /* BANGTSYNTH_EN */
#endif /* BANGTSYNTH_LEGACY */
