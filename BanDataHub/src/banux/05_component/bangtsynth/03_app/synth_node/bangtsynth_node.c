/**
 * @file    bangtsynth_node.c
 * @brief   BanGTsynth �ϳ�??Effect Graph Դ�ڵ��ŽӲ�ʵ��
 * @author  BanGO
 * @date    2026-03-01
 *
 * �ܹ� (v4 ??FreeRTOS ��Ϣ����):
 *   NDS32 BP10 �Ŀ������ڴ�ɼ��Բ��ɿ� (volatile + DSB + __sync_synchronize
 *   ���޷�ʹ Shell �����д����������????
 *
 *   ��˸��� FreeRTOS xQueue ��Ϊ������ͨ�Ż���:
 *   - Shell/BLE ����: TriggerNoteOn/Off ������Ϣ������ (xQueueSend)
 *   - �������?? SourceCallback �ȴӶ���ȡ��??(xQueueReceive, ����??,
 *                 ����������������ִ�� sf2_note_on/off
 *   - g_voices[] ֻ���������б���?? ��ȫ�������������ڴ���??
 *
 * ����: ��??SYNTH_MAX_VOICES (8) ͬʱ����
 *
 * ���?? BANGTSYNTH_EN
 */

#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bangtsynth_node.h"
#include "../drum_machine/drum_machine.h"
#include "midi_controller.h"
#include "../../02_core/soundbank/soundbank_manager.h"
#include "bg_config.h"
#include "debug.h"
#include "bg_osal.h"
#include <string.h>

/*============================================================================
 * ���Ժ��??(��Ϊ 0 �ɹر����е�����??
 *===========================================================================*/
#define BANGTSYNTH_DEBUG_EN  0

#if BANGTSYNTH_DEBUG_EN
#define SYNTH_DBG(...)  DBG(__VA_ARGS__)
#else
#define SYNTH_DBG(...)  do {} while(0)
#endif

/*============================================================================
 * ��������
 *===========================================================================*/

/* �ϳ���ÿ�δ�����֡�� (??AudioLoopWithGraph MIN_FRAME=48 ƥ��) */
#define SYNTH_FRAME_SIZE    48

/* ��Ϣ�������: �����??8 ??NoteOn/Off ��Ϣ */
#define SYNTH_QUEUE_DEPTH   8

/*============================================================================
 * ��Ϣ�������Ͷ���
 *===========================================================================*/

/* ��Ϣ���� */
#define SYNTH_MSG_NOTE_ON    1
#define SYNTH_MSG_NOTE_OFF   2
#define SYNTH_MSG_PITCH_BEND 3
#define SYNTH_MSG_CC         4

/* ��Ϣ�ṹ (8 �ֽ�, ����) */
typedef struct {
    uint8_t type;       /* SYNTH_MSG_NOTE_ON / NOTE_OFF / PITCH_BEND / CC */
    uint8_t note;       /* NoteOn����, NoteOffʱ���� / CC��� / bend_lsb */
    uint8_t velocity;   /* NoteOn���� / CCֵ / bend_msb */
    uint8_t program;    /* ��ɫ�� 0-127 */
    uint8_t channel;    /* MIDI ͨ�� 0-15 */
} SynthMsg_t;

/*============================================================================
 * �ڲ�״??
 *===========================================================================*/
static uint8_t g_synth_initialized = 0;

/* OSAL ��Ϣ���о�� */
static bg_queue_t g_synth_queue = NULL;

/* ��������??(���� Shell ��ʾ) */
static uint8_t g_trigger_count = 0;

/* ��ϴ�����־ (�ص��ڲ����ú���?? ���ٿ���?? */
static uint8_t g_diag_trigger = 0;

/* ֱ�� DAC ģʽ��־: sb -t / sb -p ����ʱ�� 1,
 * ��ֹ SourceCallback ??ReadActiveSamples ���� g_voices[],
 * ���� Shell ����������񲢷��޸������ص���״̬��??*/
static volatile uint8_t g_direct_mode = 0;

/* ������ģ?? 0=����, >0 ��ʾʣ�����ʱ��(�ص�����) */
static volatile uint32_t g_test_tone_remaining = 0;
static uint32_t g_test_tone_phase = 0;

/* �м仺��??*/
static int16_t g_synth_mix_buf[SYNTH_FRAME_SIZE];

/*============================================================================
 * ��ʱ NoteOff (���� sb -m ������ģʽ)
 *===========================================================================*/
#define SCHED_NOTEOFF_MAX        8

typedef struct {
    uint8_t    active;
    uint8_t    note;
    uint8_t    program;
    uint32_t off_tick;
} ScheduledNoteOff_t;

static ScheduledNoteOff_t g_sched_noteoff[SCHED_NOTEOFF_MAX] = {{0}};

/* �������� (0-100, Ĭ��80) */
static uint8_t g_synth_volume = 80;

/*============================================================================
 * ��ʼ??/ ����ʼ��
 *===========================================================================*/

int BanGTsynth_Node_Init(void)
{
    if (g_synth_initialized) {
        SYNTH_DBG("[Synth] Already initialized\n");
        return 0;
    }

    SYNTH_DBG("[Synth] Initializing BanGTsynth node...\n");

    /* ������Ϣ���� */
    g_synth_queue = bg_queue_create(SYNTH_QUEUE_DEPTH, sizeof(SynthMsg_t));
    if (!g_synth_queue) {
        SYNTH_DBG("[Synth] ERROR: Failed to create message queue!\n");
        return -1;
    }

    /* ��ʼ??MIDI ����??(������Ƶ������ˮ?? */
    BG_MIDI_controller.Init();

    g_trigger_count = 0;
    g_diag_trigger = 0;
    g_synth_volume = 80;
    g_synth_initialized = 1;
    SYNTH_DBG("[Synth] BanGTsynth initialized OK (built: " __DATE__ " " __TIME__ ")\n");
    SYNTH_DBG("[Synth] Architecture: MessageQueue (Shell->Queue->MainTask)\n");

    return 0;
}

void BanGTsynth_Node_DeInit(void)
{
    if (!g_synth_initialized) {
        return;
    }

    soundbank_manager.DeInit();
    DrumMachine_DeInit();

    g_trigger_count = 0;
    g_synth_initialized = 0;
    SYNTH_DBG("[Synth] BanGTsynth node deinitialized\n");
}

/*============================================================================
 * ����??API: TriggerNoteOn / TriggerNoteOff
 *
 * ͨ�� FreeRTOS ��Ϣ���н�����ݵ�������ص�ִ??
 * Shell/BLE ����ֻ�� xQueueSend, ������ص��� xQueueReceive + NoteOn/Off
 *===========================================================================*/

int BanGTsynth_TriggerNoteOn(uint8_t note, uint8_t velocity, uint8_t program, uint8_t channel)
{
    SynthMsg_t msg;
    int result;

    if (!g_synth_initialized || !g_synth_queue) {
        SYNTH_DBG("[Synth] TriggerNoteOn: not initialized\n");
        return -1;
    }

    msg.type = SYNTH_MSG_NOTE_ON;
    msg.note = note;
    msg.velocity = velocity;
    msg.program = program;
    msg.channel = channel;

    /* ��������??(���������������?? */
    result = bg_queue_send(g_synth_queue, &msg, BG_OSAL_NO_WAIT);
    if (result != BG_OSAL_OK) {
        /* ����??- ���Զ�����������??*/
        SynthMsg_t dummy;
        bg_queue_recv(g_synth_queue, &dummy, BG_OSAL_NO_WAIT);
        result = bg_queue_send(g_synth_queue, &msg, BG_OSAL_NO_WAIT);
    }

    g_trigger_count++;
    SYNTH_DBG("[Synth] TriggerNoteOn: note=%u vel=%u prog=%u ch=%u count=%u (queued=%s)\n",
        note, velocity, program, channel, g_trigger_count,
        (result == BG_OSAL_OK) ? "OK" : "FAIL");
    return (result == BG_OSAL_OK) ? 0 : -1;
}

int BanGTsynth_TriggerNoteOff(uint8_t note, uint8_t program, uint8_t channel)
{
    SynthMsg_t msg;
    int result;

    if (!g_synth_initialized || !g_synth_queue) {
        return -1;
    }

    msg.type = SYNTH_MSG_NOTE_OFF;
    msg.note = note;
    msg.velocity = 0;
    msg.program = program;
    msg.channel = channel;

    result = bg_queue_send(g_synth_queue, &msg, BG_OSAL_NO_WAIT);
    if (result != BG_OSAL_OK) {
        SynthMsg_t dummy;
        bg_queue_recv(g_synth_queue, &dummy, BG_OSAL_NO_WAIT);
        result = bg_queue_send(g_synth_queue, &msg, BG_OSAL_NO_WAIT);
    }

    if (g_trigger_count > 0) g_trigger_count--;
    SYNTH_DBG("[Synth] TriggerNoteOff: note=%u prog=%u count=%u\n",
        note, program, g_trigger_count);
    return 0;
}

uint8_t BanGTsynth_GetActiveVoiceCount(void)
{
    return g_trigger_count;
}

/*============================================================================
 * Effect Graph Դ�ڵ��??(������������)
 *
 * 1. �ȴ���Ϣ����ȡ�����д�����??NoteOn/Off ��Ϣ, ����������ִ??
 * 2. ��ͨ�� soundbank_manager.ReadActiveSamples() ��ȡ��Ƶ
 * 3. g_voices[] ֻ�ڴ˻�??����??�б�д��Ͷ�?? �޿�������
 *===========================================================================*/

/* ���������е�������??(��������ص���������ִ��) */
static void synth_process_queue(void)
{
    SynthMsg_t msg;
    uint8_t processed = 0;
    uint8_t note_on_count = 0;
    uint8_t note_off_count = 0;

    /* ������ѭ?? ȡ��������������??*/
    while (g_synth_queue &&
           bg_queue_recv(g_synth_queue, &msg, BG_OSAL_NO_WAIT) == BG_OSAL_OK) {
        switch (msg.type) {
            case SYNTH_MSG_NOTE_ON:
                soundbank_manager.SetChannel(msg.channel);
                soundbank_manager.NoteOn(msg.note, msg.velocity, msg.program);
                g_diag_trigger = 1;
                note_on_count++;
                processed++;
                break;

            case SYNTH_MSG_NOTE_OFF:
                soundbank_manager.NoteOff(msg.note, msg.program);
                note_off_count++;
                processed++;
                break;

            case SYNTH_MSG_PITCH_BEND:
                {
                    int16_t bend_val = (int16_t)(((uint16_t)msg.velocity << 7) | (uint16_t)msg.note) - 8192;
                    soundbank_manager.PitchBend(msg.channel, bend_val);
                }
                processed++;
                break;

            case SYNTH_MSG_CC:
                soundbank_manager.CC(msg.channel, msg.note, msg.velocity);
                processed++;
                break;

            default:
                break;
        }
    }

    if (processed > 0) {
        SYNTH_DBG("[Synth] Queue: processed %u msgs (on=%u off=%u)\n",
            processed, note_on_count, note_off_count);
    }
}

/*============================================================================
 * ��ʱ NoteOff tick (??SourceCallback �������������е�??
 * sb -m ?? TriggerNoteOn �������??NoteOff, ���� vTaskDelay
 *===========================================================================*/
static void scheduled_noteoff_tick(void)
{
    uint32_t now = bg_get_tick_ms();
    uint8_t i;

    for (i = 0; i < SCHED_NOTEOFF_MAX; i++) {
        if (g_sched_noteoff[i].active &&
            (int32_t)(now - g_sched_noteoff[i].off_tick) >= 0) {
            soundbank_manager.NoteOff(g_sched_noteoff[i].note, g_sched_noteoff[i].program);
            SYNTH_DBG("[Synth] SchedNoteOff: note=%u prog=%u\n",
                g_sched_noteoff[i].note, g_sched_noteoff[i].program);
            g_sched_noteoff[i].active = 0;
        }
    }
}

uint32_t BanGTsynth_RenderS16(int16_t *out, uint32_t frames)
{
    uint32_t i;
    uint32_t chunk_len;
    uint32_t offset;
    uint8_t active;

    if (!out || frames == 0) {
        return 0;
    }

    memset(out, 0, frames * sizeof(int16_t));

    if (!g_synth_initialized) {
        return frames;
    }

    if (g_test_tone_remaining > 0) {
        g_test_tone_remaining--;
        for (i = 0; i < frames; i++) {
            out[i] = (g_test_tone_phase < 48) ? 16000 : -16000;
            g_test_tone_phase = (g_test_tone_phase + 1) % 96;
        }
        return frames;
    }

    if (g_direct_mode) {
        return frames;
    }

    synth_process_queue();
    DrumMachine_Tick();
    scheduled_noteoff_tick();

    for (offset = 0; offset < frames; offset += chunk_len) {
        chunk_len = frames - offset;
        if (chunk_len > SYNTH_FRAME_SIZE) {
            chunk_len = SYNTH_FRAME_SIZE;
        }

        active = soundbank_manager.ReadActiveSamples(g_synth_mix_buf, chunk_len);
        if (active == 0) {
            continue;
        }

        for (i = 0; i < chunk_len; i++) {
            int32_t sample_vol = ((int32_t)g_synth_mix_buf[i] * g_synth_volume) / 100;
            if (sample_vol > 32767) sample_vol = 32767;
            if (sample_vol < -32768) sample_vol = -32768;
            out[offset + i] = (int16_t)sample_vol;
        }
    }

    return frames;
}

uint16_t BanGTsynth_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
    uint16_t i;
    uint16_t sample_u16;
    uint16_t offset;
    uint16_t chunk;

    (void)node;
    memset(out_buf, 0, max_len * sizeof(uint32_t));

    offset = 0;
    while (offset < max_len) {
        chunk = (uint16_t)(max_len - offset);
        if (chunk > SYNTH_FRAME_SIZE) {
            chunk = SYNTH_FRAME_SIZE;
        }
        BanGTsynth_RenderS16(g_synth_mix_buf, chunk);
        for (i = 0; i < chunk; i++) {
            sample_u16 = (uint16_t)g_synth_mix_buf[i];
            out_buf[offset + i] = ((uint32_t)sample_u16 << 16) | (uint32_t)sample_u16;
        }
        offset = (uint16_t)(offset + chunk);
    }

    return max_len;
}

uint16_t BanGTsynth_GetAvailCallback(EffectNode_t *node)
{
    (void)node;
    return SYNTH_FRAME_SIZE;
}

/*============================================================================
 * MIDI ��Ϣ�ӿ�
 *===========================================================================*/

void BanGTsynth_SendMIDI(uint8_t *data, uint8_t len)
{
    if (!g_synth_initialized) {
        SYNTH_DBG("[Synth] ERROR: SendMIDI called but not initialized!\n");
        return;
    }

    SYNTH_DBG("[Synth] MIDI RX: ");
    {
        uint8_t i;
        for (i = 0; i < len; i++) {
            SYNTH_DBG("%02X ", data[i]);
        }
        SYNTH_DBG("\n");
    }

    BG_MIDI_controller.MIDI_Handle(data, len);
}

uint8_t BanGTsynth_IsInitialized(void)
{
    return g_synth_initialized;
}

/*============================================================================
 * �Ļ���������??API
 *===========================================================================*/

int BanGTsynth_DrumSeq_Start(uint32_t bpm, uint32_t bars, uint8_t program)
{
    (void)bars;  /* DrumMachine uses loop mode instead of bar count */

    if (!g_synth_initialized) {
        SYNTH_DBG("[DrumSeq] Not initialized!\n");
        return -1;
    }

    /* Stop if already running */
    if (DrumMachine_IsPlaying()) {
        DrumMachine_Stop();
    }

    /* Configure and start DrumMachine */
    DrumMachine_SetBPM((uint16_t)bpm);
    DrumMachine_SetProgram(program);
    DrumMachine_LoadPreset(DM_PRESET_ROCK);

    return DrumMachine_Play(1);
}

void BanGTsynth_DrumSeq_Stop(void)
{
    DrumMachine_Stop();
}

uint8_t BanGTsynth_DrumSeq_IsRunning(void)
{
    return DrumMachine_IsPlaying();
}

/*============================================================================
 * ��ʱ NoteOff ���� API (sb -m ������ģ??
 *===========================================================================*/

int BanGTsynth_ScheduleNoteOff(uint8_t note, uint8_t program, uint32_t delay_ms)
{
    uint8_t i;
    uint32_t off_tick = bg_get_tick_ms() + delay_ms;

    for (i = 0; i < SCHED_NOTEOFF_MAX; i++) {
        if (!g_sched_noteoff[i].active) {
            g_sched_noteoff[i].note = note;
            g_sched_noteoff[i].program = program;
            g_sched_noteoff[i].off_tick = off_tick;
            g_sched_noteoff[i].active = 1;
            SYNTH_DBG("[Synth] ScheduleNoteOff: note=%u prog=%u delay=%ums\n",
                note, program, delay_ms);
            return 0;
        }
    }
    SYNTH_DBG("[Synth] ScheduleNoteOff: no free slot!\n");
    return -1;
}
/*============================================================================
 * �������ƹ��� API
 *===========================================================================*/

void BanGTsynth_SetVolume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    g_synth_volume = volume;
    SYNTH_DBG("[Synth] Volume set to %u%%\n", volume);
}

uint8_t BanGTsynth_GetVolume(void)
{
    return g_synth_volume;
}
/**
 * ��������?? ͨ��Ч��ͼ·�������??
 * ��֤ synth_in ??USB_BT_Mixer ??USB_BT_EQ ??Final_Mixer ??DRC ??DAC0_Out
 * @param duration_ms ����ʱ��(����)
 */
void BanGTsynth_StartTestTone(uint32_t duration_ms)
{
    /* ÿ����Լ 1 ����??(48 samples @ 48kHz = 1ms) */
    g_test_tone_remaining = duration_ms;
    g_test_tone_phase = 0;
    SYNTH_DBG("[Synth] Test tone started: %u ms (500Hz square wave)\n", duration_ms);
}

void BanGTsynth_StopTestTone(void)
{
    g_test_tone_remaining = 0;
    SYNTH_DBG("[Synth] Test tone stopped\n");
}

/**
 * ����ֱ�� DAC ģʽ
 * sb -t / sb -p ??Shell ������ֱ�Ӳ�??g_voices[] ??ReadSamples,
 * ����ͬʱ��ֹ SourceCallback ??ReadActiveSamples ��������??
 * @param enable 1=����ֱ��ģʽ(����ReadActiveSamples), 0=�ָ�����
 */
void BanGTsynth_SetDirectMode(uint8_t enable)
{
    g_direct_mode = enable;
    SYNTH_DBG("[Synth] DirectMode %s\n", enable ? "ON (SourceCB skips ReadActiveSamples)" : "OFF");
}

#endif /* BANGTSYNTH_EN */
