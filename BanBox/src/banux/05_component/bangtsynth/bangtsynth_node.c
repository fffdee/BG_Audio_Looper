/**
 * @file    bangtsynth_node.c
 * @brief   BanGTsynth 合成�?Effect Graph 源节点桥接层实现
 * @author  BanGO
 * @date    2026-03-01
 *
 * 架构 (v4 �?FreeRTOS 消息队列):
 *   NDS32 BP10 的跨任务内存可见性不可靠 (volatile + DSB + __sync_synchronize
 *   均无法使 Shell 任务的写入对主任务可�?�?
 *
 *   因此改用 FreeRTOS xQueue 作为跨任务通信机制:
 *   - Shell/BLE 任务: TriggerNoteOn/Off 发送消息到队列 (xQueueSend)
 *   - 主任务回�? SourceCallback 先从队列取消�?(xQueueReceive, 非阻�?,
 *                 在主任务上下文中执行 sf2_note_on/off
 *   - g_voices[] 只在主任务中被访�? 完全消除跨任务共享内存问�?
 *
 * 复音: 最�?SYNTH_MAX_VOICES (8) 同时发声
 *
 * 宏控�? BANGTSYNTH_EN
 */

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include "bangtsynth_node.h"
#include "drum_machine.h"
#include "midi_controller.h"
#include "midi_soundbank_bridge.h"
#include "soundbank_manager.h"
#include "bg_config.h"
#include "debug.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <string.h>

/*============================================================================
 * 调试宏控�?(设为 0 可关闭所有调试输�?
 *===========================================================================*/
#define BANGTSYNTH_DEBUG_EN  0

#if BANGTSYNTH_DEBUG_EN
#define SYNTH_DBG(...)  DBG(__VA_ARGS__)
#else
#define SYNTH_DBG(...)  do {} while(0)
#endif

/*============================================================================
 * 常量定义
 *===========================================================================*/

/* 合成器每次处理的帧长 (�?AudioLoopWithGraph MIN_FRAME=48 匹配) */
#define SYNTH_FRAME_SIZE    48

/* 消息队列深度: 最多排�?8 �?NoteOn/Off 消息 */
#define SYNTH_QUEUE_DEPTH   8

/*============================================================================
 * 消息队列类型定义
 *===========================================================================*/

/* 消息类型 */
#define SYNTH_MSG_NOTE_ON   1
#define SYNTH_MSG_NOTE_OFF  2

/* 消息结构 (8 字节, 紧凑) */
typedef struct {
    uint8_t type;       /* SYNTH_MSG_NOTE_ON / NOTE_OFF */
    uint8_t note;       /* MIDI note 0-127 */
    uint8_t velocity;   /* NoteOn力度, NoteOff时忽�?*/
    uint8_t program;    /* 音色�?0-127 */
} SynthMsg_t;

/*============================================================================
 * 内部状�?
 *===========================================================================*/
static uint8_t g_synth_initialized = 0;

/* FreeRTOS 消息队列句柄 */
static xQueueHandle g_synth_queue = NULL;

/* 触发计数�?(仅供 Shell 显示) */
static uint8_t g_trigger_count = 0;

/* 诊断触发标志 (回调内部设置和清�? 不再跨任�? */
static uint8_t g_diag_trigger = 0;

/* 直接 DAC 模式标志: sb -t / sb -p 运行时置 1,
 * 阻止 SourceCallback �?ReadActiveSamples 访问 g_voices[],
 * 避免 Shell 任务和主任务并发修改声部池导致状态损�?*/
static volatile uint8_t g_direct_mode = 0;

/* 测试音模�? 0=正常, >0 表示剩余持续时间(回调次数) */
static volatile uint32_t g_test_tone_remaining = 0;
static uint32_t g_test_tone_phase = 0;

/* 中间缓冲�?*/
static int16_t g_synth_mix_buf[SYNTH_FRAME_SIZE];

/*============================================================================
 * 鼓机音序�?(Tick-Driven Drum Sequencer)
 * 非阻塞设�? Shell 命令只设置状态并立即返回,
 * SourceCallback 每次被调用时检�?tick 驱动音序进度
 *===========================================================================*/

/* 鼓音符定�?(General MIDI Drum Map) */
#define DRUM_NOTE_KICK   36
#define DRUM_NOTE_SNARE  38
#define DRUM_NOTE_HIHAT  42
#define DRUM_NOTE_OPHAT  46
#define DRUM_NOTE_CRASH  49

/* 鼓节奏每步最多同时触发的音符�?*/
#define DRUM_MAX_NOTES_PER_STEP  3

/* 鼓节奏步�?(8分音�? 每小�?�? */
#define DRUM_STEPS_PER_BAR       8

/* 定时 NoteOff 最大同时数 */
#define SCHED_NOTEOFF_MAX        8

/* 鼓节奏步定义 */
typedef struct {
    uint8_t notes[DRUM_MAX_NOTES_PER_STEP];
    uint8_t count;
} DrumPatternStep_t;

/* 鼓机音序器状�?*/
typedef struct {
    uint8_t  running;              /* 是否正在播放 */
    uint8_t  current_step;         /* 当前�?(0 ~ DRUM_STEPS_PER_BAR-1) */
    uint8_t  current_bar;          /* 当前小节 */
    uint8_t  total_bars;           /* 总小节数 */
    uint8_t  program;              /* 音色�?*/
    uint8_t  prev_notes[DRUM_MAX_NOTES_PER_STEP]; /* 上一步的音符 (需NoteOff) */
    uint8_t  prev_note_count;      /* 上一步音符数 */
    uint32_t step_duration_ticks;  /* 每步时长 (FreeRTOS ticks) */
    TickType_t next_step_tick;     /* 下一步触发时�?*/
} DrumSequencer_t;

/* 定时 NoteOff 条目 (用于 sb -m 非阻塞模�? */
typedef struct {
    uint8_t    active;
    uint8_t    note;
    uint8_t    program;
    TickType_t off_tick;           /* 触发 NoteOff 的时�?*/
} ScheduledNoteOff_t;

static DrumSequencer_t g_drum_seq = {0};
static ScheduledNoteOff_t g_sched_noteoff[SCHED_NOTEOFF_MAX] = {{0}};

/* 音量控制 (0-100, 默认80) */
static uint8_t g_synth_volume = 80;

/* Basic Rock Beat 鼓节�?(4/4�? 每小�?�?分音�?:
 * Beat:  1    &    2    &    3    &    4    &
 * Kick:  X              X    X              X
 * Snare:           X                   X
 * HiHat: X    X    X    X    X    X    X    X
 *                  (Open)              (Open)
 */
static const DrumPatternStep_t g_rock_pattern[DRUM_STEPS_PER_BAR] = {
    {{DRUM_NOTE_KICK,  DRUM_NOTE_HIHAT, 0},            2},  /* Beat 1   */
    {{DRUM_NOTE_HIHAT, 0,               0},            1},  /* Beat 1&  */
    {{DRUM_NOTE_SNARE, DRUM_NOTE_HIHAT, 0},            2},  /* Beat 2   */
    {{DRUM_NOTE_KICK,  DRUM_NOTE_OPHAT, 0},            2},  /* Beat 2&  */
    {{DRUM_NOTE_KICK,  DRUM_NOTE_HIHAT, 0},            2},  /* Beat 3   */
    {{DRUM_NOTE_HIHAT, 0,               0},            1},  /* Beat 3&  */
    {{DRUM_NOTE_SNARE, DRUM_NOTE_HIHAT, 0},            2},  /* Beat 4   */
    {{DRUM_NOTE_KICK,  DRUM_NOTE_OPHAT, 0},            2},  /* Beat 4&  */
};

/*============================================================================
 * 初始�?/ 反初始化
 *===========================================================================*/

int BanGTsynth_Node_Init(void)
{
    if (g_synth_initialized) {
        SYNTH_DBG("[Synth] Already initialized\n");
        return 0;
    }

    SYNTH_DBG("[Synth] Initializing BanGTsynth node...\n");

    /* 创建消息队列 */
    g_synth_queue = xQueueCreate(SYNTH_QUEUE_DEPTH, sizeof(SynthMsg_t));
    if (!g_synth_queue) {
        SYNTH_DBG("[Synth] ERROR: Failed to create message queue!\n");
        return -1;
    }

    /* 初始�?MIDI 控制�?(包含音频处理流水�? */
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

    midi_soundbank_bridge.DeInit();

    g_trigger_count = 0;
    g_synth_initialized = 0;
    SYNTH_DBG("[Synth] BanGTsynth node deinitialized\n");
}

/*============================================================================
 * 跨任�?API: TriggerNoteOn / TriggerNoteOff
 *
 * 通过 FreeRTOS 消息队列将命令传递到主任务回调执�?
 * Shell/BLE 任务只做 xQueueSend, 主任务回调做 xQueueReceive + NoteOn/Off
 *===========================================================================*/

int BanGTsynth_TriggerNoteOn(uint8_t note, uint8_t velocity, uint8_t program, uint8_t channel)
{
    SynthMsg_t msg;
    portBASE_TYPE result;

    (void)channel;

    if (!g_synth_initialized || !g_synth_queue) {
        SYNTH_DBG("[Synth] TriggerNoteOn: not initialized\n");
        return -1;
    }

    msg.type = SYNTH_MSG_NOTE_ON;
    msg.note = note;
    msg.velocity = velocity;
    msg.program = program;

    /* 非阻塞发�?(队列满则丢弃最旧消�? */
    result = xQueueSend(g_synth_queue, &msg, 0);
    if (result != pdPASS) {
        /* 队列�?- 尝试丢弃队首再重�?*/
        SynthMsg_t dummy;
        xQueueReceive(g_synth_queue, &dummy, 0);
        result = xQueueSend(g_synth_queue, &msg, 0);
    }

    g_trigger_count++;
    SYNTH_DBG("[Synth] TriggerNoteOn: note=%u vel=%u prog=%u ch=%u count=%u (queued=%s)\n",
        note, velocity, program, channel, g_trigger_count,
        (result == pdPASS) ? "OK" : "FAIL");
    return (result == pdPASS) ? 0 : -1;
}

int BanGTsynth_TriggerNoteOff(uint8_t note, uint8_t program, uint8_t channel)
{
    SynthMsg_t msg;
    portBASE_TYPE result;

    (void)channel;

    if (!g_synth_initialized || !g_synth_queue) {
        return -1;
    }

    msg.type = SYNTH_MSG_NOTE_OFF;
    msg.note = note;
    msg.velocity = 0;
    msg.program = program;

    result = xQueueSend(g_synth_queue, &msg, 0);
    if (result != pdPASS) {
        SynthMsg_t dummy;
        xQueueReceive(g_synth_queue, &dummy, 0);
        result = xQueueSend(g_synth_queue, &msg, 0);
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
 * Effect Graph 源节点回�?(主任务上下文)
 *
 * 1. 先从消息队列取出所有待处理�?NoteOn/Off 消息, 在主任务中执�?
 * 2. 再通过 soundbank_manager.ReadActiveSamples() 读取音频
 * 3. g_voices[] 只在此回�?主任�?中被写入和读�? 无跨任务共享
 *===========================================================================*/

/* 处理队列中的所有消�?(在主任务回调上下文中执行) */
static void synth_process_queue(void)
{
    SynthMsg_t msg;
    uint8_t processed = 0;
    uint8_t note_on_count = 0;
    uint8_t note_off_count = 0;

    /* 非阻塞循�? 取出队列中所有消�?*/
    while (g_synth_queue &&
           xQueueReceive(g_synth_queue, &msg, 0) == pdPASS) {
        switch (msg.type) {
            case SYNTH_MSG_NOTE_ON:
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
 * 鼓机音序�?tick (�?SourceCallback 主任务上下文中调�?
 * 每次检查当�?tick,  到时间就 NoteOff 上一�?�?NoteOn 当前�?�?步进
 *===========================================================================*/
static void drum_sequencer_tick(void)
{
    TickType_t now;
    uint8_t i;
    const DrumPatternStep_t *step;

    /* 已停�?�?清理残留音符 */
    if (!g_drum_seq.running) {
        if (g_drum_seq.prev_note_count > 0) {
            for (i = 0; i < g_drum_seq.prev_note_count; i++) {
                soundbank_manager.NoteOff(g_drum_seq.prev_notes[i], g_drum_seq.program);
            }
            SYNTH_DBG("[DrumSeq] Cleanup %u notes\n", g_drum_seq.prev_note_count);
            g_drum_seq.prev_note_count = 0;
        }
        return;
    }

    now = xTaskGetTickCount();

    /* 还没到下一步的时间 */
    if ((int32_t)(now - g_drum_seq.next_step_tick) < 0) {
        return;
    }

    /* NoteOff 上一步的音符 */
    for (i = 0; i < g_drum_seq.prev_note_count; i++) {
        soundbank_manager.NoteOff(g_drum_seq.prev_notes[i], g_drum_seq.program);
    }
    g_drum_seq.prev_note_count = 0;

    /* 检查是否播完所有小�?*/
    if (g_drum_seq.current_bar >= g_drum_seq.total_bars) {
        g_drum_seq.running = 0;
        SYNTH_DBG("[DrumSeq] Complete (%u bars)\n", g_drum_seq.total_bars);
        return;
    }

    /* 获取当前步的音符 */
    step = &g_rock_pattern[g_drum_seq.current_step];

    /* NoteOn 当前�?*/
    for (i = 0; i < step->count; i++) {
        uint8_t note = step->notes[i];
        /* 第一小节第一�? Crash 替代 HiHat */
        if (g_drum_seq.current_bar == 0 && g_drum_seq.current_step == 0 &&
            note == DRUM_NOTE_HIHAT) {
            note = DRUM_NOTE_CRASH;
        }
        soundbank_manager.NoteOn(note, 100, g_drum_seq.program);
        g_drum_seq.prev_notes[i] = note;
    }
    g_drum_seq.prev_note_count = step->count;

    /* 步进 */
    g_drum_seq.current_step++;
    if (g_drum_seq.current_step >= DRUM_STEPS_PER_BAR) {
        g_drum_seq.current_step = 0;
        g_drum_seq.current_bar++;
    }

    /* 设置下一步触发时�?*/
    g_drum_seq.next_step_tick = now + g_drum_seq.step_duration_ticks;
}

/*============================================================================
 * 定时 NoteOff tick (�?SourceCallback 主任务上下文中调�?
 * sb -m �? TriggerNoteOn 后调度延�?NoteOff, 无需 vTaskDelay
 *===========================================================================*/
static void scheduled_noteoff_tick(void)
{
    TickType_t now = xTaskGetTickCount();
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

uint16_t BanGTsynth_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len)
{
    uint16_t i;
    uint16_t chunk_len;
    uint16_t offset;
    uint8_t active;
    uint16_t sample_u16;
    static uint32_t call_count = 0;
    static uint32_t active_cb_count = 0;  /* 有活跃音频的回调次数 */

    (void)node;

    /* 清零输出 */
    memset(out_buf, 0, max_len * sizeof(uint32_t));

    if (!g_synth_initialized) {
        return max_len;
    }

    call_count++;

    /* �?测试音模�? 生成 500Hz 方波，独立验证效果图路径 */
    if (g_test_tone_remaining > 0) {
        g_test_tone_remaining--;
        for (i = 0; i < max_len; i++) {
            /* 500Hz 方波 @ 48kHz = 96 samples per cycle */
            int16_t tone = (g_test_tone_phase < 48) ? 16000 : -16000;
            g_test_tone_phase = (g_test_tone_phase + 1) % 96;
            sample_u16 = (uint16_t)tone;
            out_buf[i] = ((uint32_t)sample_u16 << 16) | (uint32_t)sample_u16;
        }
        if (call_count % 500 == 0) {
            SYNTH_DBG("[Synth] TEST_TONE: remaining=%u out[0]=0x%08X\n",
                g_test_tone_remaining, out_buf[0]);
        }
        return max_len;
    }

    /* �?直接 DAC 模式: sb -t / sb -p 正在 Shell 任务中直接操�?g_voices[],
     *   此时 SourceCallback 必须跳过 ReadActiveSamples 以避免并发访�?*/
    if (g_direct_mode) {
        /* 定期提示 (�?5000 �? */
        if (call_count % 5000 == 0) {
            SYNTH_DBG("[Synth] CB#%u DIRECT_MODE active, skipping ReadActiveSamples\n", call_count);
        }
        return max_len;
    }

    /* �?关键: 先处理队列中�?NoteOn/Off 消息 (在主任务执行!) */
    synth_process_queue();

    /* �?鼓机音序�?tick: 根据时间自动触发 NoteOn/Off */
    drum_sequencer_tick();
    /* ③b 独立鼓机模块 tick (DrumMachine) */
    DrumMachine_Tick();
    /* �?定时 NoteOff tick: sb -m 调度的延迟关�?*/
    scheduled_noteoff_tick();

    /* �?诊断: NoteOn 后立即验�?g_voices 状�?*/
    if (g_diag_trigger) {
        SYNTH_DBG("[Synth] POST_QUEUE: diag_trigger set, checking ReadActiveSamples...\n");
    }

    /* 分块处理: 每次 SYNTH_FRAME_SIZE 样本 */
    for (offset = 0; offset < max_len; offset += chunk_len) {
        chunk_len = max_len - offset;
        if (chunk_len > SYNTH_FRAME_SIZE) {
            chunk_len = SYNTH_FRAME_SIZE;
        }

        active = soundbank_manager.ReadActiveSamples(g_synth_mix_buf, chunk_len);

        if (active > 0) {
            active_cb_count++;

            /* 一次性诊�? TriggerNoteOn 后首次检测到活跃音频 */
            if (g_diag_trigger) {
                int16_t max_abs = 0;
                for (i = 0; i < chunk_len && i < 8; i++) {
                    int16_t v = g_synth_mix_buf[i];
                    if (v < 0) v = -v;
                    if (v > max_abs) max_abs = v;
                }
                SYNTH_DBG("[Synth] DIAG_ACTIVE: CB#%u active=%u chunk=%u max_len=%u max_abs=%d\n",
                    call_count, active, chunk_len, max_len, (int)max_abs);
                SYNTH_DBG("[Synth]   mix[0..7]: %d %d %d %d %d %d %d %d\n",
                    (int)g_synth_mix_buf[0], (int)g_synth_mix_buf[1],
                    (int)g_synth_mix_buf[2], (int)g_synth_mix_buf[3],
                    (int)g_synth_mix_buf[4], (int)g_synth_mix_buf[5],
                    (int)g_synth_mix_buf[6], (int)g_synth_mix_buf[7]);
                g_diag_trigger = 0;
            }

            /* 周期性活跃日�?*/
            if (active_cb_count % 200 == 0) {
                SYNTH_DBG("[Synth] ACTIVE #%u: CB#%u mix[0]=%d\n",
                    active_cb_count, call_count, (int)g_synth_mix_buf[0]);
            }

            /*
             * 直接打包原始采样�?uint32_t 立体�?(绕过 BG_AudioProcessor)
             * sb -t 也不使用 AudioProcessor, 直接�?DAC 有声
             * ★ 应用音量: vol=0~100, 计算时使用 int32 防止溢出
             */
            for (i = 0; i < chunk_len; i++) {
                int32_t sample_vol = ((int32_t)g_synth_mix_buf[i] * g_synth_volume) / 100;
                if (sample_vol > 32767) sample_vol = 32767;
                if (sample_vol < -32768) sample_vol = -32768;
                sample_u16 = (uint16_t)sample_vol;
                out_buf[offset + i] = ((uint32_t)sample_u16 << 16) | (uint32_t)sample_u16;
            }

            /* 一次�? 打印打包后的 out_buf �?*/
            if (active_cb_count == 1) {
                SYNTH_DBG("[Synth]   out[0..3]: 0x%08X 0x%08X 0x%08X 0x%08X\n",
                    out_buf[offset], out_buf[offset+1],
                    out_buf[offset+2], out_buf[offset+3]);
            }
        }
    }

    /* �?诊断: NoteOn 已处理但 ReadActiveSamples 始终返回 0 */
    if (g_diag_trigger) {
        SYNTH_DBG("[Synth] DIAG_FAIL: NoteOn queued OK but ReadActiveSamples returned 0!\n");
        SYNTH_DBG("[Synth]   active_cbs=%u, max_len=%u\n", active_cb_count, max_len);
        g_diag_trigger = 0;
    }

    /* 定期心跳日志 (包含活跃统计) */
    if (call_count % 10000 == 0) {
        SYNTH_DBG("[Synth] CB#%u heartbeat (active_cbs=%u direct=%u)\n",
            call_count, active_cb_count, g_direct_mode);
    }

    return max_len;
}

uint16_t BanGTsynth_GetAvailCallback(EffectNode_t *node)
{
    (void)node;
    return SYNTH_FRAME_SIZE;
}

/*============================================================================
 * MIDI 消息接口
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
 * 鼓机音序器公�?API
 *===========================================================================*/

int BanGTsynth_DrumSeq_Start(uint32_t bpm, uint32_t bars, uint8_t program)
{
    uint32_t beat_ms;

    if (!g_synth_initialized) {
        SYNTH_DBG("[DrumSeq] Not initialized!\n");
        return -1;
    }

    /* 先停止可能正在运行的音序�?*/
    if (g_drum_seq.running) {
        g_drum_seq.running = 0;
        /* 残留音符会在下次 SourceCallback �?drum_sequencer_tick() 中清�?*/
    }

    if (bpm < 40) bpm = 40;
    if (bpm > 240) bpm = 240;
    if (bars == 0) bars = 1;
    if (bars > 32) bars = 32;

    beat_ms = 60000 / bpm;

    g_drum_seq.current_step = 0;
    g_drum_seq.current_bar = 0;
    g_drum_seq.total_bars = (uint8_t)bars;
    g_drum_seq.program = program;
    g_drum_seq.prev_note_count = 0;
    g_drum_seq.step_duration_ticks = (beat_ms / 2) / portTICK_PERIOD_MS;
    g_drum_seq.next_step_tick = xTaskGetTickCount(); /* 立即开始第一�?*/
    g_drum_seq.running = 1;

    SYNTH_DBG("[DrumSeq] Start: BPM=%u bars=%u step_ticks=%u\n",
        bpm, bars, g_drum_seq.step_duration_ticks);
    return 0;
}

void BanGTsynth_DrumSeq_Stop(void)
{
    if (g_drum_seq.running) {
        g_drum_seq.running = 0;
        SYNTH_DBG("[DrumSeq] Stop requested\n");
    }
}

uint8_t BanGTsynth_DrumSeq_IsRunning(void)
{
    return g_drum_seq.running;
}

/*============================================================================
 * 定时 NoteOff 公共 API (sb -m 非阻塞模�?
 *===========================================================================*/

int BanGTsynth_ScheduleNoteOff(uint8_t note, uint8_t program, uint32_t delay_ms)
{
    uint8_t i;
    TickType_t off_tick = xTaskGetTickCount() + delay_ms / portTICK_PERIOD_MS;

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
 * 音量控制公共 API
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
 * 启动测试�? 通过效果图路径输出方�?
 * 验证 synth_in �?USB_BT_Mixer �?USB_BT_EQ �?Final_Mixer �?DRC �?DAC0_Out
 * @param duration_ms 持续时长(毫秒)
 */
void BanGTsynth_StartTestTone(uint32_t duration_ms)
{
    /* 每毫秒约 1 个回�?(48 samples @ 48kHz = 1ms) */
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
 * 设置直接 DAC 模式
 * sb -t / sb -p �?Shell 任务中直接操�?g_voices[] �?ReadSamples,
 * 必须同时阻止 SourceCallback �?ReadActiveSamples 并发访问�?
 * @param enable 1=进入直接模式(跳过ReadActiveSamples), 0=恢复正常
 */
void BanGTsynth_SetDirectMode(uint8_t enable)
{
    g_direct_mode = enable;
    SYNTH_DBG("[Synth] DirectMode %s\n", enable ? "ON (SourceCB skips ReadActiveSamples)" : "OFF");
}

#endif /* BANGTSYNTH_EN */
