#ifndef _HARDWARE_INTERFANCE_H__
#define _HARDWARE_INTERFANCE_H__

#include <stdint.h>
#include "err_handle.h"
/* BGS 类型/宏统一定义在 bgs_types.h，避免与 02_core/soundbank 重复定义 */
#include "bgs_types.h"

#define PCMDEVICE "default"
#define SAMPLERATE 48000
#define MAXCHANNELS 2
#define BYTESPERSAMPLE (sizeof(short) * MAXCHANNELS) // 2 bytes per sample * number of channels

#define BUFFER_SIZE ((SAMPLERATE / 1000))     // Number of samples per millisecond
#define MAXRING_BUFFER_SIZE (BUFFER_SIZE * 2) // Ring buffer size (double buffering)

#define FILECOUNT 4
#define SAMPLERATECOUNT 4
#define CH_COUNT 2
#define WIDITH_COUNT 4
#define MAX_FILE_COUNT 100
#define WAV_START 0x19000

// #define READ_LINUX_DEBUG

typedef struct
{
    void (*Enable)(uint8_t);
    void (*Init)(uint8_t, uint8_t, uint16_t);
    void (*DeInit)(void);
    void (*PlayLoop)(void);
    void (*Callbaclk)(uint16_t *);
    uint8_t (*GetState)(void);

} AudioPlay;

/**
 * @deprecated BG_reader 已废弃,请使用 soundbank_manager
 * 引入: #include "soundbank_manager.h"
 * 使用: soundbank_manager.Init(), soundbank_manager.ReadSamples() 等
 */
/* BG_reader 已在 bgs_types.h 中声明 */

extern AudioPlay audioPlay;

#endif
