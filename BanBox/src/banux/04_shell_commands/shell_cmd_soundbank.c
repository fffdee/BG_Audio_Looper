/**
 * @file shell_cmd_soundbank.c
 * @brief 音源管理 Shell 命令模块实现
 * 
 * 提供以下命令:
 *   sb -d <size>     下载音源到 Flash (通过数据包协议)
 *   sb -i            查看当前音源信息
 *   sb -e            擦除音源存储区
 *   sb -v            校验音源数据完整性
 *   sb -r <off> <len> 读取并打印音源数据 (十六进制)
 *   sb -l            加载音源 (初始化合成器)
 * 
 * 编译条件: BANGTSYNTH_EN
 */

#include "bg_config.h"
#ifdef BANGTSYNTH_EN

#include "shell_cmd_soundbank.h"
#include "bg_shell.h"
#include "bg_storage.h"
#include "soundbank_manager.h"
#include "bg_download_port.h"
#include "bg_log.h"
#include "bangtsynth_node.h"
#include "midi_controller.h"   /* BG_MIDI_data 直接访问 */
#include "dac_interface.h"
#include "dac.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)
#include "bg_soundbank_dl_protocol.h"
#include "BG_FlashMgr.h"
#endif

/* ============================================
 * 内部常量
 * ============================================ */
#define SB_FLASH_TOTAL_SIZE     (8 * 1024 * 1024)   /* Flash#1: 8MB */
#define SB_SECTOR_SIZE          (4096)                /* 4KB 扇区 */
#define SB_HEXDUMP_LINE_BYTES   16                    /* hex dump 每行字节数 */
#define SB_HEXDUMP_MAX_LEN      256                   /* hex dump 默认最大长度 */

/* ============================================
 * 下载进度回调
 * ============================================ */
static void download_progress_cb(size_t bytes_written, size_t total_size, void *user_data)
{
    uint32_t percent;

    (void)user_data;

    if (total_size > 0) {
        percent = (uint32_t)(bytes_written * 100 / total_size);
    } else {
        percent = 0;
    }
    Shell_Printf("\r  Progress: %u / %u bytes (%u%%)", 
                 (uint32_t)bytes_written, (uint32_t)total_size, percent);
}

/* ============================================
 * 命令: sb -d <size>  下载音源
 * ============================================ */
static int cmd_sb_download(int argc, char *argv[])
{
    uint32_t file_size;
#if (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)
    uint8_t resp_buf[DL_RESP_SIZE];
    BG_ERR ret;
#endif

    if (argc < 1) {
        Shell_Printf("Usage: sb -d <size_bytes>\n");
        Shell_Printf("  size_bytes: soundbank file size in bytes\n");
        return -1;
    }

    file_size = (uint32_t)strtoul(argv[0], NULL, 0);
    if (file_size == 0 || file_size > SB_FLASH_TOTAL_SIZE) {
        Shell_Printf("Error: invalid size %u (max %u)\n", file_size, SB_FLASH_TOTAL_SIZE);
        return -1;
    }

    Shell_Printf("=== Soundbank Download ===\n");
    Shell_Printf("  File size  : %u bytes\n", file_size);
    Shell_Printf("  Flash      : Storage partition (Flash#1)\n");

#if (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)
    /* 1. 检查 FlashMgr */
    if (!BG_FlashMgr.IsReady()) {
        Shell_Printf("Error: FlashMgr not ready!\n");
        return -1;
    }

    /* 2. 擦除目标区域 (按扇区) */
    {
        uint32_t erase_size;
        uint32_t erased;
        int32_t flash_ret;

        erase_size = (file_size + SB_SECTOR_SIZE - 1) & ~(SB_SECTOR_SIZE - 1);
        Shell_Printf("  Erasing    : %u bytes (%u sectors)...\n", 
                     erase_size, erase_size / SB_SECTOR_SIZE);
        
        erased = 0;
        while (erased < erase_size) {
            flash_ret = BG_FlashMgr.EraseStorageSector(erased);
            if (flash_ret != 0) {
                Shell_Printf("\nError: Erase failed at offset 0x%X\n", erased);
                return -1;
            }
            erased += SB_SECTOR_SIZE;
            /* 每 64KB 打印一次进度 */
            if ((erased % (64 * 1024)) == 0) {
                Shell_Printf("\r  Erased     : %u / %u KB", erased / 1024, erase_size / 1024);
            }
        }
        Shell_Printf("\n  Erase OK.\n");
    }

    /* 3. 初始化下载会话 */
    bg_download_port_session_init(file_size);

    /* 4. 发送 READY 响应 (告知主机可以开始发送) */
    Shell_Printf("  Waiting for data packets...\n");
    dl_build_response(resp_buf, DL_RSP_READY, 0, DL_STATUS_OK);
    Shell_WriteRaw(resp_buf, DL_RESP_SIZE);

    /* 5. 调用 soundbank_download() — 内部循环接收数据包并写入 Flash */
    ret = soundbank_manager.Download("shell_io", 0, file_size, 
                                      download_progress_cb, NULL);

    /* 6. 结束会话 */
    bg_download_port_session_deinit();

    Shell_Printf("\n");
    if (ret == SUCCESS) {
        Shell_Printf("  Download complete! (%u bytes)\n", file_size);
    } else {
        Shell_Printf("  Download FAILED! (err=%d)\n", ret);
        return -1;
    }

#else
    Shell_Printf("Error: Download only supported on BP10 platform\n");
    return -1;
#endif

    return 0;
}

/* ============================================
 * 命令: sb -i  查看音源信息
 * ============================================ */
static int cmd_sb_info(int argc, char *argv[])
{
    const char *info;
    uint32_t total_size = 0;
    uint32_t free_size = 0;
    SoundBank_Format fmt;

    (void)argc; (void)argv;

    Shell_Printf("=== Soundbank Info ===\n");

    /* 存储信息 */
    BG_Storage.GetInfo(&total_size, &free_size);
    Shell_Printf("  Storage    : %u KB total, %u KB free\n",
                 total_size / 1024, free_size / 1024);

    /* 音源信息 */
    info = soundbank_manager.GetInfo();
    fmt = soundbank_manager.GetFormat();

    Shell_Printf("  Format     : %s\n",
                 fmt == SOUNDBANK_FORMAT_BG  ? "BGS" :
                 fmt == SOUNDBANK_FORMAT_SF2 ? "SF2" : "Unknown");
    Shell_Printf("  Info       : %s\n", info ? info : "N/A");

    return 0;
}

/* ============================================
 * 命令: sb -e  擦除音源存储
 * ============================================ */
static int cmd_sb_erase(int argc, char *argv[])
{
    BG_ERR ret;
    uint32_t erase_size;
    uint32_t offset;

    (void)argc; (void)argv;

    Shell_Printf("=== Erase Soundbank Storage ===\n");

#if (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)
    if (!BG_FlashMgr.IsReady()) {
        Shell_Printf("Error: FlashMgr not ready!\n");
        return -1;
    }

    Shell_Printf("  Erasing entire Storage partition (8MB)...\n");
    
    erase_size = SB_FLASH_TOTAL_SIZE;
    offset = 0;
    while (offset < erase_size) {
        ret = (BG_ERR)BG_FlashMgr.EraseStorageSector(offset);
        if (ret != 0) {
            Shell_Printf("\nError: Erase failed at 0x%X\n", offset);
            return -1;
        }
        offset += SB_SECTOR_SIZE;
        if ((offset % (256 * 1024)) == 0) {
            Shell_Printf("\r  Progress: %u / %u KB", offset / 1024, erase_size / 1024);
        }
    }
    Shell_Printf("\n  Erase complete.\n");
#else
    ret = BG_Storage.Init(NULL, BG_STORAGE_MODE_READ_WRITE);
    if (ret != SUCCESS) {
        Shell_Printf("Error: Storage init failed (%d)\n", ret);
        return -1;
    }
    ret = BG_Storage.Erase(0, SB_FLASH_TOTAL_SIZE);
    BG_Storage.DeInit();
    if (ret == SUCCESS) {
        Shell_Printf("  Erase complete.\n");
    } else {
        Shell_Printf("  Erase failed (%d)\n", ret);
        return -1;
    }
#endif

    return 0;
}

/* ============================================
 * 命令: sb -v  校验音源数据
 * ============================================ */
static int cmd_sb_verify(int argc, char *argv[])
{
    uint8_t header[8];
    uint32_t magic;
    int rd;

    (void)argc; (void)argv;

    Shell_Printf("=== Verify Soundbank ===\n");

    /* 初始化存储层 */
    if (BG_Storage.Init(NULL, BG_STORAGE_MODE_READ_ONLY) != SUCCESS) {
        Shell_Printf("Error: Storage init failed\n");
        return -1;
    }

    /* 读取文件头 */
    rd = BG_Storage.Read(0, header, 8);
    if (rd < 8) {
        Shell_Printf("Error: Cannot read header\n");
        BG_Storage.DeInit();
        return -1;
    }

    magic = (uint32_t)header[0] | ((uint32_t)header[1] << 8) |
            ((uint32_t)header[2] << 16) | ((uint32_t)header[3] << 24);

    if (magic == 0x50534742) { /* "BGSP" */
        Shell_Printf("  Detected : BGSP packed soundbank\n");
        Shell_Printf("  Version  : %u\n", header[4]);
        Shell_Printf("  Files    : %u\n", header[5]);
        Shell_Printf("  Status   : VALID\n");
    } else if (magic == 0x46464952) { /* "RIFF" */
        Shell_Printf("  Detected : SF2 (SoundFont 2)\n");
        Shell_Printf("  Status   : VALID\n");
    } else if (magic == 0xFFFFFFFF) {
        Shell_Printf("  Storage  : EMPTY (erased)\n");
        Shell_Printf("  Status   : NO DATA\n");
    } else {
        Shell_Printf("  Magic    : 0x%08X\n", magic);
        Shell_Printf("  Status   : UNKNOWN format (may be BGS)\n");
    }

    BG_Storage.DeInit();
    return 0;
}

/* ============================================
 * 命令: sb -r <offset> [length]  读取音源数据
 * ============================================ */
static int cmd_sb_read(int argc, char *argv[])
{
    uint32_t offset = 0;
    uint32_t length = SB_HEXDUMP_MAX_LEN;
    uint8_t line_buf[SB_HEXDUMP_LINE_BYTES];
    uint32_t addr;
    int rd;
    int j;

    if (argc >= 1) {
        offset = (uint32_t)strtoul(argv[0], NULL, 0);
    }
    if (argc >= 2) {
        length = (uint32_t)strtoul(argv[1], NULL, 0);
        if (length > 4096) length = 4096;
    }

    Shell_Printf("=== Read Storage (offset=0x%X, len=%u) ===\n", offset, length);

    if (BG_Storage.Init(NULL, BG_STORAGE_MODE_READ_ONLY) != SUCCESS) {
        Shell_Printf("Error: Storage init failed\n");
        return -1;
    }

    for (addr = offset; addr < offset + length; addr += SB_HEXDUMP_LINE_BYTES) {
        uint32_t remain = (offset + length) - addr;
        uint32_t chunk = (remain < SB_HEXDUMP_LINE_BYTES) ? remain : SB_HEXDUMP_LINE_BYTES;

        rd = BG_Storage.Read(addr, line_buf, chunk);
        if (rd < (int)chunk) {
            Shell_Printf("Read error at 0x%X\n", addr);
            break;
        }

        Shell_Printf("  %06X: ", addr);
        for (j = 0; j < (int)chunk; j++) {
            Shell_Printf("%02X ", line_buf[j]);
        }
        /* ASCII */
        Shell_Printf(" |");
        for (j = 0; j < (int)chunk; j++) {
            char c = (char)line_buf[j];
            Shell_Printf("%c", (c >= 0x20 && c <= 0x7E) ? c : '.');
        }
        Shell_Printf("|\n");
    }

    BG_Storage.DeInit();
    return 0;
}

/* ============================================
 * 命令: sb -l [offset]  加载音源
 * ============================================ */
static int cmd_sb_load(int argc, char *argv[])
{
    uint32_t offset = 0;
    BG_ERR ret;

    if (argc >= 1) {
        offset = (uint32_t)strtoul(argv[0], NULL, 0);
    }

    Shell_Printf("=== Load Soundbank (offset=0x%X) ===\n", offset);

    /* 先反初始化旧音源 */
    soundbank_manager.DeInit();

    /* 初始化新音源 */
    ret = soundbank_manager.Init(offset);
    if (ret == SUCCESS) {
        Shell_Printf("  Load OK: %s\n", soundbank_manager.GetInfo());
    } else {
        Shell_Printf("  Load FAILED (err=%d)\n", ret);
        return -1;
    }

    return 0;
}

/* ============================================
 * 命令: sb -t <note> [velocity] [dur_ms] [program] [channel]  测试音符
 * 直接操作 MIDI 状态 + SF2 声部池, 绕过 MIDI 控制器派发
 * ============================================ */
static const char * const note_names[] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};



static int cmd_sb_test(int argc, char *argv[])
{
    uint8_t note;
    uint8_t velocity;
    uint32_t duration_ms;
    uint8_t program;
    uint8_t channel;
    int octave;

    /* 检查合成器是否可用 */
    if (!BanGTsynth_IsInitialized()) {
        Shell_Printf("Error: BanGTsynth not initialized!\n");
        return -1;
    }

    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_UNKNOWN) {
        Shell_Printf("Error: No soundbank loaded!\n");
        return -1;
    }

    /* 解析参数 */
    if (argc < 1) {
        Shell_Printf("Usage: sb -t <note> [vel] [dur_ms] [prog] [ch]\n");
        Shell_Printf("  note     : MIDI note number 0-127 (60=Middle C)\n");
        Shell_Printf("  vel      : velocity 1-127 (default: 100)\n");
        Shell_Printf("  dur_ms   : duration in ms (default: 500)\n");
        Shell_Printf("  prog     : MIDI program 0-127 (default: 0)\n");
        Shell_Printf("  ch       : MIDI channel 0-15 (default: 0, ch 9=drums)\n");
        Shell_Printf("Example: sb -t 60           (Middle C, ch 0)\n");
        Shell_Printf("         sb -t 36 127 1000 0 9  (Bass Drum on ch 9)\n");
        return -1;
    }

    note = (uint8_t)strtoul(argv[0], NULL, 0);
    velocity = (argc >= 2) ? (uint8_t)strtoul(argv[1], NULL, 0) : 100;
    duration_ms = (argc >= 3) ? (uint32_t)strtoul(argv[2], NULL, 0) : 500;
    program = (argc >= 4) ? (uint8_t)strtoul(argv[3], NULL, 0) : 0;
    channel = (argc >= 5) ? (uint8_t)strtoul(argv[4], NULL, 0) : 0;

    /* 限幅 */
    if (note > 127) note = 127;
    if (velocity == 0) velocity = 1;
    if (velocity > 127) velocity = 127;
    if (duration_ms == 0) duration_ms = 100;
    if (duration_ms > 10000) duration_ms = 10000;
    if (program > 127) program = 0;
    if (channel > 15) channel = 0;

    octave = ((int)note / 12) - 1;

    Shell_Printf("=== Test Note (Direct Mode) ===\n");
    Shell_Printf("  Note     : %u (%s%d)\n", note, note_names[note % 12], octave);
    Shell_Printf("  Velocity : %u\n", velocity);
    Shell_Printf("  Duration : %u ms\n", duration_ms);
    Shell_Printf("  Program  : %u\n", program);
    Shell_Printf("  Channel  : %u%s\n", channel, (channel == 9) ? " (Drums)" : "");
    Shell_Printf("  Soundbank: %s\n", soundbank_manager.GetInfo());

    /*
     * ★ 进入直接 DAC 模式: 阻止 SourceCallback 的 ReadActiveSamples,
     *   避免与 Shell 任务并发访问 g_voices[] 导致状态损坏
     */
    BanGTsynth_SetDirectMode(1);

    /*
     * Shell 直接验证模式 (完全绕过回调和效果图):
     *   Phase 1: 分配声部 + ReadSamples 验证合成器有数据
     *   Phase 2: ReadSamples → 打包立体声 → AudioDAC_DataSet 直写 DAC
     *   Phase 3: 释放声部
     *
     * 这条链路: soundbank_manager.NoteOn → ReadSamples → sf2_callback → DAC
     * 全部在 Shell 任务内完成, 不依赖音频回调/效果图/volatile
     */

    /* === Phase 1: 分配声部 + 验证 ReadSamples === */
    soundbank_manager.NoteOn(note, velocity, program);
    Shell_Printf("  [1] NoteOn(%u, %u, %u)\n", note, velocity, program);

    {
        int16_t test_buf[48];
        uint8_t rs;
        int k;
        int16_t max_s = 0;

        /* 第 1 次读取: 会触发 find_sample 初始化 */
        memset(test_buf, 0, sizeof(test_buf));
        rs = soundbank_manager.ReadSamples(test_buf, note, 48, program);
        Shell_Printf("  [2] ReadSamples#1: result=%u\n", rs);
        Shell_Printf("      buf[0..7]: ");
        for (k = 0; k < 8; k++) {
            Shell_Printf("%d ", (int)test_buf[k]);
            if (test_buf[k] > max_s) max_s = test_buf[k];
            if (-test_buf[k] > max_s) max_s = -test_buf[k];
        }
        Shell_Printf("\n      max_abs=%d\n", (int)max_s);

        /* 第 2 次读取: 确认持续产生数据 */
        memset(test_buf, 0, sizeof(test_buf));
        rs = soundbank_manager.ReadSamples(test_buf, note, 48, program);
        Shell_Printf("  [3] ReadSamples#2: result=%u buf[0]=%d buf[1]=%d\n",
                     rs, (int)test_buf[0], (int)test_buf[1]);

        if (!rs) {
            Shell_Printf("  ERROR: ReadSamples returns 0! Synth chain broken.\n");
            soundbank_manager.NoteOff(note, program);
            return -1;
        }
    }

    /* === Phase 2: 直写 DAC 循环 === */
    Shell_Printf("  [4] Direct DAC output for %u ms...\n", duration_ms);
    {
        int16_t pcm_buf[48];
        uint32_t dac_buf[48];
        TickType_t start_tick = xTaskGetTickCount();
        uint32_t total_samples = 0;
        uint32_t dac_full_count = 0;
        uint8_t rs = 1;
        uint16_t i;
        int16_t max_sample = 0;
        uint16_t su;
        uint16_t space;
        int16_t s;

        while (rs && (xTaskGetTickCount() - start_tick) < duration_ms) {
            space = AudioDAC_DataSpaceLenGet(DAC0);
            if (space >= 48) {
                memset(pcm_buf, 0, sizeof(pcm_buf));
                rs = soundbank_manager.ReadSamples(pcm_buf, note, 48, program);
                if (rs) {
                    for (i = 0; i < 48; i++) {
                        s = pcm_buf[i];
                        if (s > max_sample) max_sample = s;
                        if (-s > max_sample) max_sample = -s;
                        /* 打包为 uint32_t 立体声: 高16位=R, 低16位=L */
                        su = (uint16_t)s;
                        dac_buf[i] = ((uint32_t)su << 16) | (uint32_t)su;
                    }
                    AudioDAC_DataSet(DAC0, dac_buf, 48);
                    total_samples += 48;
                }
            } else {
                dac_full_count++;
                vTaskDelay(1);  /* DAC FIFO 满, 等 1ms */
            }
        }

        Shell_Printf("  [5] DAC done: %u samples, max_abs=%d, dac_waits=%u, final_rs=%u\n",
                     total_samples, (int)max_sample, dac_full_count, rs);
    }

    /* === Phase 3: 释放 === */
    soundbank_manager.NoteOff(note, program);
    Shell_Printf("  [6] NoteOff. Done.\n");

    /* ★ 恢复正常模式: 允许 SourceCallback 的 ReadActiveSamples */
    BanGTsynth_SetDirectMode(0);

    return 0;
}


/* ============================================
 * 测试: 通过 TriggerNoteOn + 效果图路径触发
 * sb -m <note> [vel] [dur] [prog] [ch]
 * ============================================ */
static int cmd_sb_midi(int argc, char *argv[])
{
    uint8_t note;
    uint8_t velocity;
    uint32_t duration_ms;
    uint8_t program;
    uint8_t channel;
    int octave;

    /* 检查 */
    if (!BanGTsynth_IsInitialized()) {
        Shell_Printf("Error: BanGTsynth not initialized!\n");
        return -1;
    }

    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_UNKNOWN) {
        Shell_Printf("Error: No soundbank loaded!\n");
        return -1;
    }

    /* 解析 */
    if (argc < 1) {
        Shell_Printf("Usage: sb -m <note> [vel] [dur] [prog] [ch]\n");
        Shell_Printf("  Trigger via TriggerNoteOn + effect graph (normal path)\n");
        return -1;
    }

    note = (uint8_t)strtoul(argv[0], NULL, 0);
    velocity = (argc >= 2) ? (uint8_t)strtoul(argv[1], NULL, 0) : 100;
    duration_ms = (argc >= 3) ? (uint32_t)strtoul(argv[2], NULL, 0) : 1000;
    program = (argc >= 4) ? (uint8_t)strtoul(argv[3], NULL, 0) : 0;
    channel = (argc >= 5) ? (uint8_t)strtoul(argv[4], NULL, 0) : 0;

    if (note > 127) note = 127;
    if (velocity == 0) velocity = 1;
    if (velocity > 127) velocity = 127;
    if (duration_ms == 0) duration_ms = 100;
    if (duration_ms > 10000) duration_ms = 10000;
    if (program > 127) program = 0;
    if (channel > 15) channel = 0;

    octave = ((int)note / 12) - 1;

    Shell_Printf("=== MIDI Mode Test (ReadActiveSamples) ===\n");
    Shell_Printf("  Note: %u (%s%d), Vel: %u, Dur: %u ms, Prog: %u, Ch: %u\n",
                 note, note_names[note % 12], octave, velocity, duration_ms, program, channel);

    /* 通过 TriggerNoteOn 触发 (直接调用 soundbank NoteOn, 回调通过 ReadActiveSamples 读取) */
    if (BanGTsynth_TriggerNoteOn(note, velocity, program, channel) == 0) {
        Shell_Printf("  [1] TriggerNoteOn OK (active=%u)\n",
                     BanGTsynth_GetActiveVoiceCount());
    } else {
        Shell_Printf("  [1] TriggerNoteOn FAILED (voice pool full)\n");
        return -1;
    }

    /* ★ 非阻塞: 调度定时 NoteOff, 立即返回 (不再使用 vTaskDelay) */
    if (BanGTsynth_ScheduleNoteOff(note, program, duration_ms) == 0) {
        Shell_Printf("  [2] NoteOff scheduled in %u ms (non-blocking)\n", duration_ms);
    } else {
        Shell_Printf("  [2] WARNING: NoteOff schedule failed (no free slot)\n");
    }

    Shell_Printf("  Command returns immediately. Audio plays via effect graph.\n");
    return 0;
}

/* ============================================
 * 播放鼓节奏模式 (非阻塞, Tick 驱动音序器)
 * sb -p [bpm] [bars]   — 启动鼓机
 * sb -p stop           — 停止鼓机
 * 音序器在 SourceCallback 中被 tick 驱动, 不使用 vTaskDelay
 * ============================================ */
static int cmd_sb_play_drum(int argc, char *argv[])
{
    uint32_t bpm;
    uint32_t bars;
    uint32_t beat_ms;
    uint8_t program = 0;

    /* 检查 */
    if (!BanGTsynth_IsInitialized()) {
        Shell_Printf("Error: BanGTsynth not initialized!\n");
        return -1;
    }

    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_UNKNOWN) {
        Shell_Printf("Error: No soundbank loaded!\n");
        return -1;
    }

    /* 检查 stop 命令 */
    if (argc >= 1 && (strcmp(argv[0], "stop") == 0 || strcmp(argv[0], "0") == 0)) {
        if (BanGTsynth_DrumSeq_IsRunning()) {
            BanGTsynth_DrumSeq_Stop();
            Shell_Printf("Drum sequencer stopped.\n");
        } else {
            Shell_Printf("Drum sequencer is not running.\n");
        }
        return 0;
    }

    /* 查询状态 */
    if (argc >= 1 && strcmp(argv[0], "status") == 0) {
        Shell_Printf("Drum sequencer: %s\n",
                     BanGTsynth_DrumSeq_IsRunning() ? "RUNNING" : "STOPPED");
        return 0;
    }

    /* 解析参数 */
    bpm = (argc >= 1) ? (uint32_t)strtoul(argv[0], NULL, 0) : 120;
    bars = (argc >= 2) ? (uint32_t)strtoul(argv[1], NULL, 0) : 2;

    if (bpm < 40) bpm = 40;
    if (bpm > 240) bpm = 240;
    if (bars == 0) bars = 1;
    if (bars > 32) bars = 32;

    beat_ms = 60000 / bpm;

    Shell_Printf("=== Drum Sequencer (Tick-Driven) ===\n");
    Shell_Printf("  BPM: %u (8th note = %u ms)\n", bpm, beat_ms / 2);
    Shell_Printf("  Bars: %u (4/4 time)\n", bars);
    Shell_Printf("  Pattern: Basic Rock Beat\n");
    Shell_Printf("  Use 'sb -p stop' to stop early\n");

    /* 启动音序器 (非阻塞, 立即返回) */
    if (BanGTsynth_DrumSeq_Start(bpm, bars, program) == 0) {
        Shell_Printf("  Sequencer started. Audio via effect graph.\n");
    } else {
        Shell_Printf("  ERROR: Failed to start sequencer!\n");
        return -1;
    }

    return 0;
}

/* ============================================
 * 测试: 效果图路径验证
 * sb -g [dur]
 * 生成 500Hz 测试音通过效果图输出,
 * 如果能听到声音说明图路径正常, 问题在 soundbank
 * ============================================ */
static int cmd_sb_graph_test(int argc, char *argv[])
{
    uint32_t duration_ms;

    if (!BanGTsynth_IsInitialized()) {
        Shell_Printf("Error: BanGTsynth not initialized!\n");
        return -1;
    }

    duration_ms = (argc >= 1) ? (uint32_t)strtoul(argv[0], NULL, 0) : 1000;
    if (duration_ms == 0) duration_ms = 500;
    if (duration_ms > 5000) duration_ms = 5000;

    Shell_Printf("=== Effect Graph Path Test ===\n");
    Shell_Printf("  Generating 500Hz square wave for %u ms\n", duration_ms);
    Shell_Printf("  Path: synth_in -> USB_BT_Mixer -> USB_BT_EQ -> Final_Mixer -> DRC -> DAC0\n");
    Shell_Printf("  (bypasses soundbank entirely)\n");

    BanGTsynth_StartTestTone(duration_ms);
    /* ★ 非阻塞: 测试音由 SourceCallback 内部倒计时自动停止, 无需 vTaskDelay */
    Shell_Printf("  Test tone started. Will auto-stop after %u ms.\n", duration_ms);
    return 0;
}

/* ============================================
 * 音量控制
 * sb -vol [volume]
 * 不带参数: 查询当前音量
 * 带参数: 设置音量 (0-100)
 * ============================================ */
static int cmd_sb_volume(int argc, char *argv[])
{
    uint8_t volume;

    if (!BanGTsynth_IsInitialized()) {
        Shell_Printf("Error: BanGTsynth not initialized!\n");
        return -1;
    }

    /* 查询当前音量 */
    if (argc < 1) {
        volume = BanGTsynth_GetVolume();
        Shell_Printf("Current volume: %u%% (0-100)\n", volume);
        return 0;
    }

    /* 设置音量 */
    volume = (uint8_t)strtoul(argv[0], NULL, 0);
    if (volume > 100) {
        Shell_Printf("Error: volume must be 0-100\n");
        return -1;
    }

    BanGTsynth_SetVolume(volume);
    Shell_Printf("Volume set to %u%%\n", volume);
    return 0;
}

/* ============================================
 * 选项定义
 * ============================================ */
static const ShellOpt_t sb_options[] = {
    OPT("d", "download", "<size>",     "Download soundbank via data packets",  cmd_sb_download),
    OPT("i", "info",     NULL,         "Show soundbank & storage info",        cmd_sb_info),
    OPT("e", "erase",    NULL,         "Erase soundbank storage",              cmd_sb_erase),
    OPT("v", "verify",   NULL,         "Verify soundbank data integrity",      cmd_sb_verify),
    OPT("r", "read",     "<off> [len]","Hex dump storage data",                cmd_sb_read),
    OPT("l", "load",     "[offset]",   "Load soundbank from storage",          cmd_sb_load),
    OPT("t", "test",     "<note> [vel] [dur] [prog]", "Play a test note (direct DAC)",  cmd_sb_test),
    OPT("m", "midi",     "<note> [vel] [dur] [prog] [ch]", "Test via MIDI+Graph", cmd_sb_midi),
    OPT("p", "play",     "[bpm] [bars]|stop", "Drum sequencer (tick-driven, non-blocking)", cmd_sb_play_drum),
    OPT("g", "graph",    "[dur]",      "Test graph path (500Hz tone, no soundbank)", cmd_sb_graph_test),
    OPT("V", "volume",   "[0-100]",    "Set/query synth volume (0=mute, 100=full)", cmd_sb_volume),
    OPT_END()
};

/* ============================================
 * 模块注册
 * ============================================ */
int ShellCmdSoundbank_Register(void)
{
    static const ShellModule_t sb_module = {
        "sb",
        "Soundbank management (download/info/erase/verify/test)",
        MOD_CAT_AUDIO,
        sb_options,
        11
    };
    return Shell_RegisterModule(&sb_module) ? 0 : -1;
}

#else  /* !BANGTSYNTH_EN */

#include "shell_cmd_soundbank.h"

int ShellCmdSoundbank_Register(void)
{
    /* 合成器未启用, 不注册命令 */
    return 0;
}

#endif /* BANGTSYNTH_EN */
