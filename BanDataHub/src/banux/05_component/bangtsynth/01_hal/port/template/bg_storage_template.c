/**
 * @file bg_storage_template.c
 * @brief 移植模板：只读内存音源。把本文件复制到 port/<你的mcu>/ 并改名。
 *
 * 在工程里提供:
 *   const uint8_t bg_sf2_blob[];
 *   const uint32_t bg_sf2_blob_size;
 */
#ifndef BG_PORT_TEMPLATE
#define BG_PORT_TEMPLATE 0
#endif
#if BG_PORT_TEMPLATE

#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_storage.h"
#include "err_handle.h"
#include <string.h>

extern const uint8_t bg_sf2_blob[];
extern const uint32_t bg_sf2_blob_size;

static uint8_t s_ready;

static BG_ERR tpl_init(const char *path, BG_Storage_Mode_t mode)
{
    (void)path;
    (void)mode;
    s_ready = 1;
    return SUCCESS;
}

static BG_ERR tpl_deinit(void)
{
    s_ready = 0;
    return SUCCESS;
}

static int tpl_read(uint32_t offset, void *buffer, size_t size)
{
    if (!s_ready || !buffer || offset >= bg_sf2_blob_size) {
        return -1;
    }
    if (offset + size > bg_sf2_blob_size) {
        size = (size_t)(bg_sf2_blob_size - offset);
    }
    memcpy(buffer, bg_sf2_blob + offset, size);
    return (int)size;
}

static int tpl_write(uint32_t offset, const void *buffer, size_t size)
{
    (void)offset;
    (void)buffer;
    (void)size;
    return -1;
}

static BG_ERR tpl_erase(uint32_t offset, size_t size)
{
    (void)offset;
    (void)size;
    return ENABLE_INVALID_INPUT;
}

static BG_ERR tpl_sync(void)
{
    return SUCCESS;
}

static BG_ERR tpl_info(uint32_t *total_size, uint32_t *free_size)
{
    if (total_size) {
        *total_size = bg_sf2_blob_size;
    }
    if (free_size) {
        *free_size = 0;
    }
    return SUCCESS;
}

const BG_Storage_Driver_t bg_storage_driver_port = {
    .init     = tpl_init,
    .deinit   = tpl_deinit,
    .read     = tpl_read,
    .write    = tpl_write,
    .erase    = tpl_erase,
    .sync     = tpl_sync,
    .get_info = tpl_info,
};

#endif /* BANGTSYNTH_EN */
#endif /* BG_PORT_TEMPLATE */
