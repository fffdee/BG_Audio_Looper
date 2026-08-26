#include "bangtsynth_legacy.h"
#if BANGTSYNTH_LEGACY

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "midi_controller.h"
#include "bg_config.h"
#include "hardware_interfance.h"
#include "standard_request _processing.h"

#include "midi_info.h"
#include "soundbank_manager.h"  // 使用统一音源管理器

#if ENABLE_AUDIO_PROCESSOR
#include "bg_audio_processor.h"  // 音频处理器流水线
#include "effects/bg_effect_drc.h"  // DRC 效果
#include "effects/bg_effect_eq.h"   // EQ 效果
#endif
#include "stdio.h"
#include "string.h"
#include <math.h>

volatile BG_MIDI_Data BG_MIDI_data; 

/* 内部函数声明 */
void MIDI_UpdateState(void);      // 定时器调用:状态更新
void MIDI_ProcessAudio(void);     // 主循环调用:音频处理
void MIDI_Message_Handle(uint8_t *data, uint8_t len);
void keyborad_mesg_handle(char value);
void MIDI_Init();
void apply_vel(short *buffer, int numsamples, uint8_t volume);

BG_MIDI_Controller BG_MIDI_controller = {
    .MIDI_Handle = MIDI_Message_Handle,
    .UpdateState = MIDI_UpdateState,   // 定时器回调
    .ProcessAudio = MIDI_ProcessAudio, // 主循环处理
    .Init = MIDI_Init,
    .ApplyVel = apply_vel,
};

void apply_vel(short *buffer, int numsamples, uint8_t volume) {
    int i;
    float volume_gain = (float)volume / 127.0f;
    for (i = 0; i < numsamples; ++i) {
        // 调整样本值
        buffer[i] = (short)(buffer[i] * volume_gain);
    }
}

int Re_Sample(int tone){

    float pitchShiftFactor = pow(2.0, (float)tone / 12.0); // 升一个全音的因子
    int newSampleRate = (int)(SAMPLERATE * pitchShiftFactor);  
    return newSampleRate;
}


void MIDI_Init(){
    memset((void*)BG_MIDI_data.BG_channel_info,0,sizeof(BG_MIDI_data.BG_channel_info));
    
    /* 初始化鼓机模块 */
    DrumMachine_Init();
    
#if ENABLE_AUDIO_PROCESSOR
    {
        BG_EQ_Effect_t *eq;
        BG_DRC_Effect_t *drc;
        uint8_t eq_id;
        uint8_t drc_id;

        /* 初始化音频处理流水线 */
        BG_AudioProcessor.Init();
        
        /* 创建并注册 EQ 效果 (使用流行音乐预设) */
        eq = bg_effect_eq_create();
        if (eq == NULL) {
            printf("[MIDI] ERROR: bg_effect_eq_create() failed (OOM), skipping EQ\n");
            goto audio_proc_done;
        }
        bg_effect_eq_preset_pop(eq);  // 默认使用流行音乐预设
        
        eq_id = BG_AudioProcessor.RegisterEffect(
            "EQ",
            bg_effect_eq_init,
            bg_effect_eq_process,
            bg_effect_eq_reset,
            eq,
            sizeof(BG_EQ_Effect_t)
        );
        
        /* 创建并注册 DRC 效果 */
        drc = bg_effect_drc_create(
            0.7f,   // threshold: 70%
            4.0f,   // ratio: 4:1
            1.0f,   // attack: 1ms
            50.0f   // release: 50ms
        );
        if (drc == NULL) {
            printf("[MIDI] ERROR: bg_effect_drc_create() failed (OOM), skipping DRC\n");
            goto audio_proc_done;
        }
        
        drc_id = BG_AudioProcessor.RegisterEffect(
            "DRC",
            bg_effect_drc_init,
            bg_effect_drc_process,
            bg_effect_drc_reset,
            drc,
            sizeof(BG_DRC_Effect_t)
        );
        
        printf("[MIDI] Audio pipeline initialized: EQ(ID=%d) -> DRC(ID=%d)\n", eq_id, drc_id);
        audio_proc_done:;
    }
#endif /* ENABLE_AUDIO_PROCESSOR */
}


/*
 * 定时器回调函数 (1ms 中断)
 * 功能: 只负责更新 MIDI 状态,不处理音频数据
 */
void MIDI_UpdateState(void)
{
    // 定时器中只做轻量级状态更新
    // 例如:包络计算、LFO 更新等
    // 当前实现:空函数,未来可添加状态更新逻辑
}

/*
 * 音频数据处理函数 (主循环调用)
 * 功能: 处理所有音频数据生成和混音
 */
void MIDI_ProcessAudio(void)
{
    short data[BG_MAX_POLYPHONY][49] = {0};
    short temp_data[48];
    short play[48] = {0}; 
    uint8_t polyphony_count = 0;
    uint8_t play_flag = 0;
    
    uint8_t ch;
    uint8_t note;
    uint8_t i;
    uint8_t channel;
    
    /* 遍历所有 MIDI 通道 */
    for(ch = 0; ch < 16; ch++)
    {
        if(BG_MIDI_data.BG_channel_info[ch].NoteOn_count > 0) {
            // printf("channel %d play count is %d\n", ch, BG_MIDI_data.BG_channel_info[ch].NoteOn_count);
            
            /* 遍历所有音符 */
            for(note = 0; note < 128; note++) {
                if(BG_MIDI_data.BG_channel_info[ch].Note_Map[note] > 0) {
                    
                    /* 读取音频样本 - 使用统一音源管理器 */
                    if (!soundbank_manager.ReadSamples(temp_data, note, 48, BG_MIDI_data.BG_channel_info[ch].program_index))
                    {
                        /* 样本播放完成,停止该音符 */
                        // printf(" stop ch:%d note:%d vel:%d\n", ch, note, BG_MIDI_data.BG_channel_info[ch].Note_Map[note]);
                        BG_MIDI_data.BG_channel_info[ch].Note_Map[note] = 0;
                        
                        if(BG_MIDI_data.BG_channel_info[ch].NoteOn_count > 0)
                            BG_MIDI_data.BG_channel_info[ch].NoteOn_count--;
                    }
                    else {
                        /* 应用力度控制 */
                        for(i = 0; i < 48; i++) {
                            data[polyphony_count][i] = (short)(temp_data[i] * 
                                ((float)BG_MIDI_data.BG_channel_info[ch].Note_Map[note] / 127.0f));
                        }
                        
                        data[polyphony_count][48] = 1;  // 标记该通道有效
                        polyphony_count++;
                        
                        if(BG_MIDI_data.BG_channel_info[ch].NoteOn_count > 0)
                            play_flag = 1;
                    }
                }
            }
        }
    }
    
    /* 混音所有活动的复音 */
    for(channel = 0; channel < polyphony_count; channel++) {
        if(data[channel][48] > 0) {
            for(i = 0; i < 48; i++)
                play[i] += data[channel][i];
        }
    }
    
    /* 经过DRC处理防止复音叠加失真 */
    if(play_flag) {
#if ENABLE_AUDIO_PROCESSOR
        short processed[48];
        BG_AudioProcessor.Process(play, processed, 48);
        audioPlay.Callbaclk((uint16_t*)processed);
#else
        audioPlay.Callbaclk((uint16_t*)play);
#endif
    }
}

void MIDI_Message_Handle(uint8_t *data, uint8_t len)
{
    uint8_t state;
    uint8_t i;
    state = (data[0]>>4)&0x0F;
    
    if(len > 0 && len < 4)
    {
        for(i=0; i<FUNC_COUNT;i++){

            if((state==MIDI_funcstion[i].Funcstion_ID))
                MIDI_funcstion[i].MIDI_FUNC(data, len);
                
        } 
        
    }


}

#endif /* BANGTSYNTH_EN */

#endif /* BANGTSYNTH_LEGACY */
