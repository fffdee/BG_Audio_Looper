/**
 * @file bg_sf2_sd.h
 * @brief TF 卡根目录 .sf2 目录与加载（BanDataHub）
 */
#ifndef BG_SF2_SD_H
#define BG_SF2_SD_H

#include <stdint.h>
#include "err_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BG_SF2_SD_MAX
#define BG_SF2_SD_MAX       24
#endif
#ifndef BG_SF2_SD_NAME_MAX
#define BG_SF2_SD_NAME_MAX  32
#endif

int bg_sf2_sd_scan(void);
int bg_sf2_sd_count(void);
const char *bg_sf2_sd_name(int index);
uint32_t bg_sf2_sd_size(int index);
const char *bg_sf2_sd_current(void);
int bg_sf2_sd_is_loaded(void);

/** 把指定文件拷进 PSRAM（不解析）。filename=NULL 则用目录第一项。 */
BG_ERR bg_sf2_sd_load_psram(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* BG_SF2_SD_H */
