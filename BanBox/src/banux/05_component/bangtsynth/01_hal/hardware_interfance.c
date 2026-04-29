#include "product_def.h"

#if BANGTSYNTH_EN

#include "hardware_interfance.h"
#include "bg_config.h"
#include "bgs_parser.h"

/*
 * AudioPlay 接口:
 * Linux 平台 — 使用 ALSA play.h 的实际音频函数
 * BP10 等嵌入式 — 音频输出由 Effect Graph 节点 (bangtsynth_node) 处理,
 *                此接口仅保留空桩以满足编译依赖
 */
#if (BG_TARGET_PLATFORM == BG_PLATFORM_LINUX)

/* Linux: 这些函数在 play.c (ALSA) 中实现 */
extern void init_audio_device(uint8_t, uint8_t, uint16_t);
extern void PlayCallback(uint16_t *);
extern void bg_play_loop(void);
extern void bg_play_enable(uint8_t);

AudioPlay audioPlay = {
    .Init = init_audio_device,
    .Callbaclk = PlayCallback,
    .PlayLoop = bg_play_loop,
    .Enable = bg_play_enable
};

#else /* BP10 等嵌入式平台 */

static void stub_audio_init(uint8_t a, uint8_t b, uint16_t c) { (void)a; (void)b; (void)c; }
static void stub_audio_callback(uint16_t *buf) { (void)buf; }
static void stub_play_loop(void) {}
static void stub_play_enable(uint8_t en) { (void)en; }

AudioPlay audioPlay = {
    .Enable = stub_play_enable,
    .Init = stub_audio_init,
    .PlayLoop = stub_play_loop,
    .Callbaclk = stub_audio_callback
};

#endif /* BG_TARGET_PLATFORM */

BG_Reader BG_reader = {
    .Init = bgs_init,
    .DeInit = bgs_deinit,
    .Callback = bgs_read_callback,
};

#endif /* BANGTSYNTH_EN */
