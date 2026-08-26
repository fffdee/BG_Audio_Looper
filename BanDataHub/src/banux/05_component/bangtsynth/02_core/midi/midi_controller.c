#include "bg_config.h"

#if BANGTSYNTH_EN

#include "midi_controller.h"
#include "bg_config.h"
#include "standard_request_processing.h"
#include "midi_info.h"
#include "soundbank_manager.h"
#include "../../03_app/drum_machine/drum_machine.h"
#include "stdio.h"
#include "string.h"

volatile BG_MIDI_Data BG_MIDI_data;

/* 内部函数声明 */
static void MIDI_UpdateState(void);
static void MIDI_ProcessAudio(void);
static void MIDI_Message_Handle(uint8_t *data, uint8_t len);
static void MIDI_Init(void);

BG_MIDI_Controller BG_MIDI_controller = {
    .MIDI_Handle = MIDI_Message_Handle,
    .UpdateState = MIDI_UpdateState,
    .ProcessAudio = MIDI_ProcessAudio,
    .Init = MIDI_Init,
    .ApplyVel = NULL,
};

static void MIDI_Init(void)
{
    int ch;
    memset((void*)BG_MIDI_data.BG_channel_info, 0, sizeof(BG_MIDI_data.BG_channel_info));

    /* 初始化 CC / Pitch Bend 状态 */
    for (ch = 0; ch < 16; ch++) {
        BG_MIDI_data.Channel_volume[ch] = 100;   /* GM 默认音量 */
        BG_MIDI_data.pitch_bend[ch]     = 0;
        BG_MIDI_data.sustain_pedal[ch]  = 0;
        BG_MIDI_data.expression[ch]     = 127;   /* GM 默认表情 */
        BG_MIDI_data.modulation[ch]     = 0;
    }
    BG_MIDI_data.bend_range = 2;  /* 默认 ±2 半音 */

    /* 初始化鼓机模块 */
    DrumMachine_Init();
}

/*
 * 定时器回调函数 (1ms 中断)
 * 当前为空桩, 预留给包络/LFO 等状态更新
 */
static void MIDI_UpdateState(void)
{
}

/*
 * 旧版音频处理路径 (已废弃)
 * BP10 平台通过 bangtsynth_node.c 的 SourceCallback + ReadActiveSamples 处理音频,
 * 此函数保留空桩以保持接口兼容
 */
static void MIDI_ProcessAudio(void)
{
}

static void MIDI_Message_Handle(uint8_t *data, uint8_t len)
{
    uint8_t state;
    uint8_t i;
    state = (data[0] >> 4) & 0x0F;

    if (len > 0 && len < 4)
    {
        for (i = 0; i < FUNC_COUNT; i++) {
            if (state == MIDI_funcstion[i].Funcstion_ID)
                MIDI_funcstion[i].MIDI_FUNC(data, len);
        }
    }
}

#endif /* BANGTSYNTH_EN */