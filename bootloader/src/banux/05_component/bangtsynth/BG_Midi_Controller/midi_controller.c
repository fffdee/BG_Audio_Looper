#include "midi_controller.h"
#include "bg_config.h"
#include "hardware_interfance.h"
#include "standard_request _processing.h"

#include "midi_info.h"
#include "soundbank_manager.h"  // 浣跨敤缁熶竴闊虫簮绠＄悊鍣�

#if ENABLE_AUDIO_PROCESSOR
#include "bg_audio_processor.h"  // 闊抽澶勭悊鍣ㄦ祦姘寸嚎
#include "effects/bg_effect_drc.h"  // DRC 鏁堟灉
#include "effects/bg_effect_eq.h"   // EQ 鏁堟灉
#endif
#include "stdio.h"
#include "string.h"
#include <math.h>

volatile BG_MIDI_Data BG_MIDI_data; 

/* 鍐呴儴鍑芥暟澹版槑 */
void MIDI_UpdateState(void);      // 瀹氭椂鍣ㄨ皟鐢�鐘舵�鏇存柊
void MIDI_ProcessAudio(void);     // 涓诲惊鐜皟鐢�闊抽澶勭悊
void MIDI_Message_Handle(uint8_t *data, uint8_t len);
void keyborad_mesg_handle(char value);
void MIDI_Init();
void apply_vel(short *buffer, int numsamples, uint8_t volume);

BG_MIDI_Controller BG_MIDI_controller = {
    .MIDI_Handle = MIDI_Message_Handle,
    .UpdateState = MIDI_UpdateState,   // 瀹氭椂鍣ㄥ洖璋�
    .ProcessAudio = MIDI_ProcessAudio, // 涓诲惊鐜鐞�
    .Init = MIDI_Init,
    .ApplyVel = apply_vel,
};

void apply_vel(short *buffer, int numsamples, uint8_t volume) {
    int i;
    float volume_gain = (float)volume / 127.0f;
    for (i = 0; i < numsamples; ++i) {
        // 璋冩暣鏍锋湰鍊�
        buffer[i] = (short)(buffer[i] * volume_gain);
    }
}

int Re_Sample(int tone){

    float pitchShiftFactor = pow(2.0, (float)tone / 12.0); // 鍗囦竴涓叏闊崇殑鍥犲瓙
    int newSampleRate = (int)(SAMPLERATE * pitchShiftFactor);  
    return newSampleRate;
}


void MIDI_Init(){
    memset((void*)BG_MIDI_data.BG_channel_info,0,sizeof(BG_MIDI_data.BG_channel_info));
    
    /* 鍒濆鍖栭紦鏈烘ā鍧�*/
    DrumMachine_Init();
    
#if ENABLE_AUDIO_PROCESSOR
    {
        BG_EQ_Effect_t *eq;
        BG_DRC_Effect_t *drc;
        uint8_t eq_id;
        uint8_t drc_id;

        /* 鍒濆鍖栭煶棰戝鐞嗘祦姘寸嚎 */
        BG_AudioProcessor.Init();
        
        /* 鍒涘缓骞舵敞鍐�EQ 鏁堟灉 (浣跨敤娴佽闊充箰棰勮) */
        eq = bg_effect_eq_create();
        if (eq == NULL) {
            printf("[MIDI] ERROR: bg_effect_eq_create() failed (OOM), skipping EQ\n");
            goto audio_proc_done;
        }
        bg_effect_eq_preset_pop(eq);  // 榛樿浣跨敤娴佽闊充箰棰勮
        
        eq_id = BG_AudioProcessor.RegisterEffect(
            "EQ",
            bg_effect_eq_init,
            bg_effect_eq_process,
            bg_effect_eq_reset,
            eq,
            sizeof(BG_EQ_Effect_t)
        );
        
        /* 鍒涘缓骞舵敞鍐�DRC 鏁堟灉 */
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
 * 瀹氭椂鍣ㄥ洖璋冨嚱鏁�(1ms 涓柇)
 * 鍔熻兘: 鍙礋璐ｆ洿鏂�MIDI 鐘舵�,涓嶅鐞嗛煶棰戞暟鎹�
 */
void MIDI_UpdateState(void)
{
    // 瀹氭椂鍣ㄤ腑鍙仛杞婚噺绾х姸鎬佹洿鏂�
    // 渚嬪:鍖呯粶璁＄畻銆丩FO 鏇存柊绛�
    // 褰撳墠瀹炵幇:绌哄嚱鏁�鏈潵鍙坊鍔犵姸鎬佹洿鏂伴�杈�
}

/*
 * 闊抽鏁版嵁澶勭悊鍑芥暟 (涓诲惊鐜皟鐢�
 * 鍔熻兘: 澶勭悊鎵�湁闊抽鏁版嵁鐢熸垚鍜屾贩闊�
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
    
    /* 閬嶅巻鎵�湁 MIDI 閫氶亾 */
    for(ch = 0; ch < 16; ch++)
    {
        if(BG_MIDI_data.BG_channel_info[ch].NoteOn_count > 0) {
            // printf("channel %d play count is %d\n", ch, BG_MIDI_data.BG_channel_info[ch].NoteOn_count);
            
            /* 閬嶅巻鎵�湁闊崇 */
            for(note = 0; note < 128; note++) {
                if(BG_MIDI_data.BG_channel_info[ch].Note_Map[note] > 0) {
                    
                    /* 璇诲彇闊抽鏍锋湰 - 浣跨敤缁熶竴闊虫簮绠＄悊鍣�*/
                    if (!soundbank_manager.ReadSamples(temp_data, note, 48, BG_MIDI_data.BG_channel_info[ch].program_index))
                    {
                        /* 鏍锋湰鎾斁瀹屾垚,鍋滄璇ラ煶绗�*/
                        // printf(" stop ch:%d note:%d vel:%d\n", ch, note, BG_MIDI_data.BG_channel_info[ch].Note_Map[note]);
                        BG_MIDI_data.BG_channel_info[ch].Note_Map[note] = 0;
                        
                        if(BG_MIDI_data.BG_channel_info[ch].NoteOn_count > 0)
                            BG_MIDI_data.BG_channel_info[ch].NoteOn_count--;
                    }
                    else {
                        /* 搴旂敤鍔涘害鎺у埗 */
                        for(i = 0; i < 48; i++) {
                            data[polyphony_count][i] = (short)(temp_data[i] * 
                                ((float)BG_MIDI_data.BG_channel_info[ch].Note_Map[note] / 127.0f));
                        }
                        
                        data[polyphony_count][48] = 1;  // 鏍囪璇ラ�閬撴湁鏁�
                        polyphony_count++;
                        
                        if(BG_MIDI_data.BG_channel_info[ch].NoteOn_count > 0)
                            play_flag = 1;
                    }
                }
            }
        }
    }
    
    /* 娣烽煶鎵�湁娲诲姩鐨勫闊�*/
    for(channel = 0; channel < polyphony_count; channel++) {
        if(data[channel][48] > 0) {
            for(i = 0; i < 48; i++)
                play[i] += data[channel][i];
        }
    }
    
    /* 缁忚繃DRC澶勭悊闃叉澶嶉煶鍙犲姞澶辩湡 */
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
