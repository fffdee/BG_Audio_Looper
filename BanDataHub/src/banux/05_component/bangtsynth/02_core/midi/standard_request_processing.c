#include "bg_config.h"

#if BANGTSYNTH_EN

#include "standard_request_processing.h"
#include "midi_info.h"
#include "midi_controller.h"
#include "soundbank_manager.h"
#include "sf2_parser.h"
#include "bgs_parser.h"
#include <stdio.h>
#include <string.h>

#if SYNTH_SD_NAND_PSRAM_EN
#include "../synth_integration/synth_sdnandpsram.h"
#endif

void NoteOnHandle(uint8_t *data, uint8_t len);
void NoteOffHandle(uint8_t *data, uint8_t len);
void ProgramChange(uint8_t *data, uint8_t len);
void CCHandle(uint8_t *data, uint8_t len);
void PitchBendHandle(uint8_t *data, uint8_t len);



MIDI_Funcstion MIDI_funcstion[FUNC_COUNT] = {

    {NOTE_ON, NoteOnHandle},
    {NOTE_OFF, NoteOffHandle},
    {PROGRAM_CHANGE,ProgramChange},
    {CC,CCHandle},
    {PITCH_BEND,PitchBendHandle},
    
};


void ChannelVolume(uint8_t *data, uint8_t len);
void AllNoteOff(uint8_t *data, uint8_t len);
void MonoOn(uint8_t *data, uint8_t len);
void PolyOn(uint8_t *data, uint8_t len);
void SustainPedal(uint8_t *data, uint8_t len);
void ExpressionCtrl(uint8_t *data, uint8_t len);
void ModulationWheel(uint8_t *data, uint8_t len);
void AllSoundOff(uint8_t *data, uint8_t len);
void ResetAllControllers(uint8_t *data, uint8_t len);

MIDI_Funcstion CC_funcstion[CC_COUNT] = {

    {CTRL_01,ModulationWheel},
    {CTRL_07,ChannelVolume},
    {CTRL_11,ExpressionCtrl},
    {CTRL_64,SustainPedal},
    {CTRL_120,AllSoundOff},
    {CTRL_121,ResetAllControllers},
    {CTRL_123,AllNoteOff},
    {CTRL_126,MonoOn},
    {CTRL_127,PolyOn},

};


void NoteOnHandle(uint8_t *data, uint8_t len)
{
    uint8_t channel;
    uint8_t is_note = 1;  // 默认接受所有音�?

    channel = data[0] & 0x0F;
 
    /* v2.0: 使用统一�?bgs_note_on 接口（包含力度层选择�?*/
    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_BG) {
        BGS_Data *bgs_data;
        uint8_t program;
        uint8_t note;
        uint8_t velocity;

        bgs_data = soundbank_manager.GetBGSData();
        if (!bgs_data) return;
        
        program = BG_MIDI_data.BG_channel_info[channel].program_index;
        note = data[1];
        velocity = data[2];
        
        /* 处理单音模式 */
        if(BG_MIDI_data.BG_channel_info[channel].mono_poly_onoff == 1){
            BG_MIDI_data.BG_channel_info[channel].Note_Map[BG_MIDI_data.BG_channel_info[channel].last_note] = 0;
            BG_MIDI_data.BG_channel_info[channel].NoteOn_count = 0;    
        }
        
        /* v2.0: 调用 BGS 音符激活接口（自动选择力度层） */
        bgs_note_on(note, velocity, program);
        
        /* 检查是否成功选择了采�?*/
        is_note = 0;
        if (program < bgs_data->program_count) {
            int sample_idx;
            sample_idx = bgs_data->ProgramData[program].note_states[note].active_sample_idx;
            if (sample_idx >= 0) {
                is_note = 1;
                printf("Note %d ON (velocity=%d, sample=%d)\n", note, velocity, sample_idx);
            }
        }
    } else {
        /* SF2 格式 */
        uint8_t sf2_note = data[1];
        uint8_t sf2_vel  = data[2];
        uint8_t sf2_prog = BG_MIDI_data.BG_channel_info[channel].program_index;
        
        printf("[NoteOn-SF2] ch=%u note=%u vel=%u prog=%u\n", channel, sf2_note, sf2_vel, sf2_prog);
        
        /* 处理单音模式 */
        if(BG_MIDI_data.BG_channel_info[channel].mono_poly_onoff == 1){
            /* 关闭上一个音符的声部 */
            soundbank_manager.NoteOff(BG_MIDI_data.BG_channel_info[channel].last_note, sf2_prog);
            BG_MIDI_data.BG_channel_info[channel].Note_Map[BG_MIDI_data.BG_channel_info[channel].last_note] = 0;
            BG_MIDI_data.BG_channel_info[channel].NoteOn_count = 0;    
        }
        
        sf2_set_current_channel(channel);

#if SYNTH_SD_NAND_PSRAM_EN
        /* SD+NAND+PSRAM 方案: 通过 PSRAM 缓存路径触发 NoteOn */
        SYNTH_SDNANDPSRAM_NoteOn(sf2_note, sf2_vel, sf2_prog);
#else
        /* 普通路径: 直接分配 SF2 声部 */
        soundbank_manager.NoteOn(sf2_note, sf2_vel, sf2_prog);
        printf("[NoteOn-SF2] Voice allocated OK\n");
#endif
    }

    if (BG_MIDI_data.BG_channel_info[channel].NoteOn_count < 255 &&
        BG_MIDI_data.BG_channel_info[channel].Note_Map[data[1]] < 1 && is_note == 1 ){
        if(BG_MIDI_data.BG_channel_info[channel].mono_poly_onoff == 1){
            BG_MIDI_data.BG_channel_info[channel].NoteOn_count = 1;
        } else {
            BG_MIDI_data.BG_channel_info[channel].NoteOn_count++;
        }
    }
    
    BG_MIDI_data.BG_channel_info[channel].Note_Map[data[1]] = data[2];
    BG_MIDI_data.BG_channel_info[channel].last_note = data[1];
} 

void NoteOffHandle(uint8_t *data, uint8_t len)
{
    uint8_t channel, i;
    uint8_t note = data[1];
    uint8_t program;
    channel = data[0] & 0x0F;
    program = BG_MIDI_data.BG_channel_info[channel].program_index;
    
    /* 使用统一的音符关闭接口 */
#if SYNTH_SD_NAND_PSRAM_EN
    /* SD+NAND+PSRAM 方案: 释放 PSRAM 缓冲区并关闭 SF2 声部 */
    SYNTH_SDNANDPSRAM_NoteOff(note, program);
#else
    soundbank_manager.NoteOff(note, program);
#endif
    
    /* 清除 Note Map 和计�?*/
    if (BG_MIDI_data.BG_channel_info[channel].NoteOn_count > 0 && 
        BG_MIDI_data.BG_channel_info[channel].Note_Map[note] > 0)
        BG_MIDI_data.BG_channel_info[channel].NoteOn_count--;

    BG_MIDI_data.BG_channel_info[channel].Note_Map[note] = 0;
}


void ProgramChange(uint8_t *data, uint8_t len)
{
    uint8_t channel;
    channel = data[0] & 0x0F;
    AllNoteOff(data,len);
    
    BG_MIDI_data.BG_channel_info[channel].program_index = data[1];

#if SYNTH_SD_NAND_PSRAM_EN
    /* 触发异步 PSRAM 预热: 将该音色的常用音符提前缓存到 PSRAM */
    SYNTH_LoadProgram(data[1]);
#endif
}



void CCHandle(uint8_t *data, uint8_t len)
{   
    uint8_t cmd = 0;
    while(!(data[1]==CC_funcstion[cmd].Funcstion_ID)){
        cmd+=1;
        if(cmd>=CC_COUNT){
            return;
        }
    }
    CC_funcstion[cmd].MIDI_FUNC(data,len);
}



void ChannelVolume(uint8_t *data, uint8_t len)
{
    uint8_t channel;
    channel = data[0] & 0x0F;
    BG_MIDI_data.Channel_volume[channel] = data[2];
    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_SF2)
        sf2_channel_cc(channel, 0x07, data[2]);
}

void SustainPedal(uint8_t *data, uint8_t len)
{
    uint8_t channel;
    channel = data[0] & 0x0F;
    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_SF2)
        sf2_channel_cc(channel, 0x40, data[2]);
}

void ExpressionCtrl(uint8_t *data, uint8_t len)
{
    uint8_t channel;
    channel = data[0] & 0x0F;
    BG_MIDI_data.expression[channel] = data[2];
    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_SF2)
        sf2_channel_cc(channel, 0x0B, data[2]);
}

void ModulationWheel(uint8_t *data, uint8_t len)
{
    uint8_t channel;
    channel = data[0] & 0x0F;
    BG_MIDI_data.modulation[channel] = data[2];
    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_SF2)
        sf2_channel_cc(channel, 0x01, data[2]);
}

void AllSoundOff(uint8_t *data, uint8_t len)
{
    uint8_t channel;
    channel = data[0] & 0x0F;
    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_SF2)
        sf2_channel_cc(channel, 0x78, 0);
}

void ResetAllControllers(uint8_t *data, uint8_t len)
{
    uint8_t channel;
    channel = data[0] & 0x0F;
    BG_MIDI_data.pitch_bend[channel]    = 0;
    BG_MIDI_data.sustain_pedal[channel]  = 0;
    BG_MIDI_data.expression[channel]     = 127;
    BG_MIDI_data.modulation[channel]     = 0;
    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_SF2)
        sf2_channel_cc(channel, 0x79, 0);
}


void MonoOn(uint8_t *data, uint8_t len)
{   
    uint8_t channel;
    channel = data[0] & 0x0F;
    AllNoteOff(data,len);
    BG_MIDI_data.BG_channel_info[channel].mono_poly_onoff = 1;

}

void PolyOn(uint8_t *data, uint8_t len)
{
    uint8_t channel;
    channel = data[0] & 0x0F;
    BG_MIDI_data.BG_channel_info[channel].mono_poly_onoff = 0;
}

void AllNoteOff(uint8_t *data, uint8_t len){
    uint8_t channel;
    uint8_t program;
    channel = data[0] & 0x0F;
    program = BG_MIDI_data.BG_channel_info[channel].program_index;
    
    /* 使用统一的全部音符关闭接�?*/
    soundbank_manager.AllNoteOff(program);
    
    /* 清除所�?Note Map */
    memset((void*)BG_MIDI_data.BG_channel_info[channel].Note_Map, 0x00, 
           sizeof(BG_MIDI_data.BG_channel_info[channel].Note_Map));
    BG_MIDI_data.BG_channel_info[channel].NoteOn_count = 0;
    printf("note off all \n");
}

/**
 * Pitch Bend 处理
 * MIDI Pitch Bend: data[1]=LSB(7bit), data[2]=MSB(7bit)
 * 组合为 14-bit 值 (0..16383), 中心=8192, 转为 -8192..+8191
 */
void PitchBendHandle(uint8_t *data, uint8_t len)
{
    uint8_t channel;
    int16_t bend_value;

    channel = data[0] & 0x0F;

    /* 组合 14-bit 弯音值 */
    bend_value = (int16_t)(((uint16_t)data[2] << 7) | (uint16_t)data[1]) - 8192;

    BG_MIDI_data.pitch_bend[channel] = bend_value;

    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_SF2)
        sf2_pitch_bend(channel, bend_value);
}

#endif /* BANGTSYNTH_EN */
