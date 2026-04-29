#ifndef _STRANDARD_REQUEST_PROCESSING_H__
#define _STRANDARD_REQUEST_PROCESSING_H__

#include<stdint.h>

#define FUNC_COUNT 5
#define CC_COUNT 48      
typedef struct 
{
    uint8_t Funcstion_ID;
    void (* MIDI_FUNC)(uint8_t*,uint8_t);

}MIDI_Funcstion;

extern MIDI_Funcstion MIDI_funcstion [FUNC_COUNT];

#endif