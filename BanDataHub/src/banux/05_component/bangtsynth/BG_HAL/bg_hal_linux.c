/*****************************************************
 * BanGTsynth - Linux 平台接口实现
 * 功能: Linux ALSA 音频系统适配 (移植参考)
 * 作者: BanGO
 * 说明: 其他平台移植时，参考此文件实现 bg_hal.h 中定义的接口
 *****************************************************/

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "bg_config.h"
#if (BG_TARGET_PLATFORM == BG_PLATFORM_LINUX)

#include "bg_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

/* 引入平台相关的实现 */
#include "play.h"
#include "soundbank_manager.h"

#if BG_ENABLE_KEYBOARD_INPUT
#include "input_handle.h"
#endif

/*============================================
 * 音频输出接口实现
 *============================================*/
/*============================================
 * 音频输出接口实现
 *============================================*/
static BG_ERR audio_init(uint8_t bit_depth, uint8_t channels, uint32_t sample_rate) {
    init_audio_device(bit_depth, channels, sample_rate);
    return BG_OK;
}

static void audio_deinit(void) {
    /* 实现音频设备关闭 */
}

static void audio_enable(uint8_t enable) {
    bg_play_enable(enable);
}

static uint8_t audio_get_state(void) {
    return 0; /* 返回播放状态 */
}

static void audio_callback(int16_t *buffer, uint32_t frame_count) {
    PlayCallback((uint16_t*)buffer);
}

static void audio_play_loop(void) {
    bg_play_loop();
}

/* 导出音频接口实例 */
bg_audio_interface_t bg_audio_interface = {
    .init = audio_init,
    .deinit = audio_deinit,
    .enable = audio_enable,
    .get_state = audio_get_state,
    .callback = audio_callback,
    .play_loop = audio_play_loop
};

/*============================================
 * 存储接口实现 (音频数据读取)
 *============================================*/
/*============================================
 * 存储接口实现 (音频数据读取)
 *============================================*/
static BG_ERR storage_init(void) {
    /* 音源初始化现在通过 soundbank_manager 完成 */
    /* 此函数保留为兼容性接口,实际初始化在 main 中调用 */
    return BG_OK;
}

static BG_ERR storage_deinit(void) {
    return soundbank_manager.DeInit();
}

static uint8_t storage_read_sample(int16_t *buffer, uint32_t note, uint32_t frame_count, uint8_t velocity) {
    return soundbank_manager.ReadSamples((short*)buffer, note, frame_count, velocity);
}

static bg_soundbank_data_t* storage_get_soundbank(void) {
    /* 返回音色库数据 */
    return NULL;
}

/* 导出存储接口实例 */
bg_storage_interface_t bg_storage_interface = {
    .init = storage_init,
    .deinit = storage_deinit,
    .read_sample = storage_read_sample,
    .get_soundbank = storage_get_soundbank
};

/*============================================
 * 输入设备接口实现
 *============================================*/
/*============================================
 * 输入设备接口实现
 *============================================*/
#if BG_ENABLE_KEYBOARD_INPUT

extern BG_Input_Handle BG_input_handle;

static BG_ERR input_init(void) {
    BG_input_handle.KeyBoardInit();
    return BG_OK;
}

static void input_deinit(void) {
    BG_input_handle.KeyBoardDeInit();
}

static uint8_t input_poll(void) {
    return BG_input_handle.KeyBoardLoop();
}

/* 导出输入接口实例 */
bg_input_interface_t bg_input_interface = {
    .init = input_init,
    .deinit = input_deinit,
    .poll = input_poll
};

#endif /* BG_ENABLE_KEYBOARD_INPUT */

/*============================================
 * 定时器接口实现
 *============================================*/
/*============================================
 * 定时器接口实现
 *============================================*/
static void (*timer_user_callback)(void) = NULL;

static void timer_signal_handler(int signum) {
    if (timer_user_callback) {
        timer_user_callback();
    }
}

static BG_ERR timer_init(uint32_t interval_us) {
    struct itimerval itv;
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = interval_us;
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = interval_us;
    
    signal(SIGALRM, timer_signal_handler);
    setitimer(ITIMER_REAL, &itv, NULL);
    
    return BG_OK;
}

static void timer_deinit(void) {
    struct itimerval itv = {0};
    setitimer(ITIMER_REAL, &itv, NULL);
}

static void timer_set_callback(void (*callback)(void)) {
    timer_user_callback = callback;
}

static void timer_start(void) {
    /* Linux 定时器初始化后自动启动 */
}

static void timer_stop(void) {
    timer_deinit();
}

/* 导出定时器接口实例 */
bg_timer_interface_t bg_timer_interface = {
    .init = timer_init,
    .deinit = timer_deinit,
    .set_callback = timer_set_callback,
    .start = timer_start,
    .stop = timer_stop
};

/*============================================
 * 内存管理接口实现
 *============================================*/
/* 导出内存接口实例 (直接映射到标准库) */
bg_memory_interface_t bg_memory_interface = {
    .malloc = malloc,
    .free = free,
    .memcpy = memcpy,
    .memset = memset
};

/*============================================
 * 文件系统HAL接口实现 (Linux - stdio)
 *============================================*/
static bg_file_handle_t fs_open(const char *filename, const char *mode)
{
    return (bg_file_handle_t)fopen(filename, mode);
}

static int fs_close(bg_file_handle_t handle)
{
    if (!handle) return -1;
    return fclose((FILE*)handle);
}

static size_t fs_read(void *buffer, size_t size, size_t count, bg_file_handle_t handle)
{
    if (!handle) return 0;
    return fread(buffer, size, count, (FILE*)handle);
}

static int fs_seek(bg_file_handle_t handle, long offset, bg_seek_mode_t whence)
{
    if (!handle) return -1;
    
    int stdio_whence;
    switch (whence) {
        case BG_SEEK_SET: stdio_whence = SEEK_SET; break;
        case BG_SEEK_CUR: stdio_whence = SEEK_CUR; break;
        case BG_SEEK_END: stdio_whence = SEEK_END; break;
        default: return -1;
    }
    
    return fseek((FILE*)handle, offset, stdio_whence);
}

static long fs_tell(bg_file_handle_t handle)
{
    if (!handle) return -1;
    return ftell((FILE*)handle);
}

static int fs_eof(bg_file_handle_t handle)
{
    if (!handle) return 1;
    return feof((FILE*)handle);
}

/* 导出文件系统接口实例 */
bg_filesystem_t bg_filesystem = {
    .open = fs_open,
    .close = fs_close,
    .read = fs_read,
    .seek = fs_seek,
    .tell = fs_tell,
    .eof = fs_eof
};

/*============================================
 * HAL 总初始化函数
 *============================================*/
/*============================================
 * HAL 总初始化函数
 *============================================*/
BG_ERR bg_hal_init(void) {
    BG_ERR err;
    
    /* 初始化音频输出 */
    err = bg_audio_interface.init(BG_AUDIO_BIT_DEPTH, BG_MAX_CHANNELS, BG_SAMPLE_RATE);
    if (err != BG_OK) return err;
    
    /* 初始化存储/读取 */
    err = bg_storage_interface.init();
    if (err != BG_OK) return err;
    
#if BG_ENABLE_KEYBOARD_INPUT
    /* 初始化输入设备 */
    err = bg_input_interface.init();
    if (err != BG_OK) return err;
#endif
    
    return BG_OK;
}

void bg_hal_deinit(void) {
    bg_audio_interface.deinit();
    bg_storage_interface.deinit();
    
#if BG_ENABLE_KEYBOARD_INPUT
    bg_input_interface.deinit();
#endif
}

/*============================================
 * Flash 存储接口实现 (Linux: 使用文件模拟Flash)
 *============================================*/

#define FLASH_FILE_PATH "./flash_storage.bin"
#define FLASH_TOTAL_SIZE (16 * 1024 * 1024)  // 16MB模拟Flash
#define FLASH_BLOCK_SIZE 4096                 // 4KB块大小

static FILE *g_flash_file = NULL;

static int linux_flash_init(void)
{
    /* 打开或创建Flash文件 */
    g_flash_file = fopen(FLASH_FILE_PATH, "r+b");
    if (!g_flash_file) {
        /* 文件不存在,创建新文件 */
        g_flash_file = fopen(FLASH_FILE_PATH, "w+b");
        if (!g_flash_file) {
            perror("Cannot create flash file");
            return -1;
        }
        
        /* 初始化为全0xFF (模拟擦除状态) */
        uint8_t empty_block[FLASH_BLOCK_SIZE];
        uint32_t i;
        
        memset(empty_block, 0xFF, FLASH_BLOCK_SIZE);
        
        for (i = 0; i < FLASH_TOTAL_SIZE / FLASH_BLOCK_SIZE; i++) {
            fwrite(empty_block, 1, FLASH_BLOCK_SIZE, g_flash_file);
        }
        fflush(g_flash_file);
        rewind(g_flash_file);
    }
    
    printf("[HAL-Linux] Flash storage initialized (%s, %d MB)\n", 
           FLASH_FILE_PATH, FLASH_TOTAL_SIZE / (1024 * 1024));
    return 0;
}

static void linux_flash_deinit(void)
{
    if (g_flash_file) {
        fclose(g_flash_file);
        g_flash_file = NULL;
    }
}

static int linux_flash_erase(uint32_t address, uint32_t size)
{
    if (!g_flash_file) return -1;
    
    if (address + size > FLASH_TOTAL_SIZE) {
        fprintf(stderr, "[HAL-Linux] Flash erase out of range\n");
        return -1;
    }
    
    /* 擦除为0xFF */
    uint8_t erase_buffer[FLASH_BLOCK_SIZE];
    memset(erase_buffer, 0xFF, FLASH_BLOCK_SIZE);
    
    uint32_t erased = 0;
    while (erased < size) {
        uint32_t chunk = (size - erased > FLASH_BLOCK_SIZE) ? 
                         FLASH_BLOCK_SIZE : (size - erased);
        
        if (fseek(g_flash_file, address + erased, SEEK_SET) != 0) {
            perror("Flash erase seek failed");
            return -1;
        }
        
        if (fwrite(erase_buffer, 1, chunk, g_flash_file) != chunk) {
            perror("Flash erase write failed");
            return -1;
        }
        
        erased += chunk;
    }
    
    fflush(g_flash_file);
    return 0;
}

static int linux_flash_write(uint32_t address, const uint8_t *data, uint32_t size)
{
    if (!g_flash_file || !data) return -1;
    
    if (address + size > FLASH_TOTAL_SIZE) {
        fprintf(stderr, "[HAL-Linux] Flash write out of range\n");
        return -1;
    }
    
    if (fseek(g_flash_file, address, SEEK_SET) != 0) {
        perror("Flash write seek failed");
        return -1;
    }
    
    if (fwrite(data, 1, size, g_flash_file) != size) {
        perror("Flash write failed");
        return -1;
    }
    
    fflush(g_flash_file);
    return 0;
}

static int linux_flash_read(uint32_t address, uint8_t *buffer, uint32_t size)
{
    if (!g_flash_file || !buffer) return -1;
    
    if (address + size > FLASH_TOTAL_SIZE) {
        fprintf(stderr, "[HAL-Linux] Flash read out of range\n");
        return -1;
    }
    
    if (fseek(g_flash_file, address, SEEK_SET) != 0) {
        perror("Flash read seek failed");
        return -1;
    }
    
    if (fread(buffer, 1, size, g_flash_file) != size) {
        perror("Flash read failed");
        return -1;
    }
    
    return 0;
}

static int linux_flash_get_info(uint32_t *total_size, uint32_t *block_size)
{
    if (total_size) *total_size = FLASH_TOTAL_SIZE;
    if (block_size) *block_size = FLASH_BLOCK_SIZE;
    return 0;
}

bg_hal_storage_t bg_hal_storage = {
    .init = linux_flash_init,
    .deinit = linux_flash_deinit,
    .erase = linux_flash_erase,
    .write = linux_flash_write,
    .read = linux_flash_read,
    .get_info = linux_flash_get_info
};

#endif /* BG_TARGET_PLATFORM == BG_PLATFORM_LINUX */

#endif /* BANGTSYNTH_EN */
