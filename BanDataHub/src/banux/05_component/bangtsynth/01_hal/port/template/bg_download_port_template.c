/**
 * @file bg_download_port_template.c
 * @brief 移植模板：不支持 Shell 下载时的空实现
 */
#ifndef BG_PORT_TEMPLATE
#define BG_PORT_TEMPLATE 0
#endif
#if BG_PORT_TEMPLATE

#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_download_port.h"
#include <stddef.h>

#if (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)
void bg_download_port_session_init(uint32_t total_size)
{
    (void)total_size;
}

void bg_download_port_session_deinit(void)
{
}

const DL_Session_t *bg_download_port_get_session(void)
{
    return NULL;
}
#endif

int bg_download_port_read(const char *source, void *buffer, size_t size, size_t *bytes_read)
{
    (void)source;
    (void)buffer;
    (void)size;
    if (bytes_read) {
        *bytes_read = 0;
    }
    return -1;
}

#endif /* BANGTSYNTH_EN */
#endif /* BG_PORT_TEMPLATE */
