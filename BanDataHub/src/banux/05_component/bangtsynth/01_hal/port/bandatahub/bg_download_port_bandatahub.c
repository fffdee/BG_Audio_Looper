/**
 * @file bg_download_port_bandatahub.c
 * @brief BanDataHub 下载端口桩：音源走 SD/U盘，不走 Flash Shell 下载
 */
#include "product_def.h"

#if BANGTSYNTH_EN && defined(BANDATAHUB)

#include "bg_download_port.h"
#include <stddef.h>

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

#endif /* BANGTSYNTH_EN && BANDATAHUB */
