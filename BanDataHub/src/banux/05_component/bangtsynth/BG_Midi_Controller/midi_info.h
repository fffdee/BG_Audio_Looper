#ifndef _MIDI_INFO_H__
#define _MIDI_INFO_H__
#include "stdint.h"
typedef enum 
{
    NOTE_ON = 0x9,
    NOTE_OFF = 0x8,
    PROGRAM_CHANGE = 0xc,
    PITCH_BEND = 0xe,
    CHANNEL_AFTERTOUCH = 0xd,
    CC= 0xb, 


}BG_MIDI_INFO;

typedef enum 
{
    CTRL_00 = 0x0, 
    CTRL_01 = 0x1, 
    CTRL_05 = 0x5, 
    CTRL_06 = 0x6, 
    CTRL_07 = 0x7, 
    CTRL_10 = 0xa, 
    CTRL_11 = 0xb, 
    CTRL_64 = 0x40, 
    CTRL_65 = 0x41, 
    CTRL_66 = 0x42, 
    CTRL_67 = 0x43, 
    CTRL_71 = 0x47,
    CTRL_72,
    CTRL_73,
    CTRL_74,
    CTRL_75,
    CTRL_76,
    CTRL_77,
    CTRL_78,
    CTRL_84 = 0x54,
    CTRL_91 = 0x5b,
    CTRL_93 = 0x5d,
    CTRL_98 = 0x62,
    CTRL_99,
    CTRL_100,
    CTRL_101,
    CTRL_120 = 0x78,
    CTRL_121,
    CTRL_123 = 0x7b,
    CTRL_126 = 0x7e,
    CTRL_127,

}BG_MIDI_CC;

typedef struct
{
    uint8_t Note_Map[128];
    uint8_t last_note;
    uint8_t NoteOn_count;
    uint8_t program_index;
    uint8_t mono_poly_onoff;

}BG_Channel_Info;

typedef struct
{   
    
    uint8_t Channel_volume[16];
    uint8_t ms_sample;
    uint16_t sample_rate;
    BG_Channel_Info BG_channel_info[16];
    
}BG_MIDI_Data;


#endif