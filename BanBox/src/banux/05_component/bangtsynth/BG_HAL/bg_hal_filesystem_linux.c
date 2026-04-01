/**
 * BanGTsynth - 文件系统HAL Linux实现
 * 功能: 使用stdio实现文件访问接口
 * 作者: BanGO
 */

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "bg_config.h"
#if (BG_TARGET_PLATFORM == BG_PLATFORM_LINUX)

#include "bg_hal.h"
#include <stdio.h>

/*============================================
 * Linux平台实现 - 使用stdio
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

/*============================================
 * 导出文件系统HAL接口实例
 *============================================*/
bg_filesystem_t bg_filesystem = {
    .open = fs_open,
    .close = fs_close,
    .read = fs_read,
    .seek = fs_seek,
    .tell = fs_tell,
    .eof = fs_eof
};

#endif /* BG_TARGET_PLATFORM == BG_PLATFORM_LINUX */

#endif /* BANGTSYNTH_EN */
