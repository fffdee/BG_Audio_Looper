#ifndef _MIDI_CONTROLLER_H__
#define _MIDI_CONTROLLER_H__

#include <stdint.h>
#include "midi_info.h"

typedef struct
{
    void (*MIDI_Handle)(uint8_t*,uint8_t);  // 处理 MIDI 消息
    void (*UpdateState)(void);               // 定时器调用:更新状态
    void (*ProcessAudio)(void);              // 主循环调用:处理音频数据
    void (*Init)(void);                      // 初始化
    void (*ApplyVel)(short*, int, uint8_t); // 应用力度

}BG_MIDI_Controller;

extern BG_MIDI_Controller BG_MIDI_controller;

extern BG_MIDI_Data BG_MIDI_data;

#endif