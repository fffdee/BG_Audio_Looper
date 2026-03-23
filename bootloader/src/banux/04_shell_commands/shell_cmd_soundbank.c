/**
 * @file shell_cmd_soundbank.c
 * @brief 闊虫簮绠＄悊 Shell 鍛戒护妯″潡瀹炵幇
 * 
 * 鎻愪緵浠ヤ笅鍛戒护:
 *   sb -d <size>     涓嬭浇闊虫簮鍒�Flash (閫氳繃鏁版嵁鍖呭崗璁�
 *   sb -i            鏌ョ湅褰撳墠闊虫簮淇℃伅
 *   sb -e            鎿﹂櫎闊虫簮瀛樺偍鍖�
 *   sb -v            鏍￠獙闊虫簮鏁版嵁瀹屾暣鎬�
 *   sb -r <off> <len> 璇诲彇骞舵墦鍗伴煶婧愭暟鎹�(鍗佸叚杩涘埗)
 *   sb -l            鍔犺浇闊虫簮 (鍒濆鍖栧悎鎴愬櫒)
 * 
 * 缂栬瘧鏉′欢: BANGTSYNTH_EN
 */

#include "bg_config.h"
#ifdef BANGTSYNTH_EN

#include "shell_cmd_soundbank.h"
#include "bg_shell.h"
#include "bg_storage.h"
#include "soundbank_manager.h"
#include "bg_download_port.h"
#include "bg_log.h"
#include "midi_controller.h"   /* BG_MIDI_data 鐩存帴璁块棶 */
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
 * 鍐呴儴甯搁噺
 * ============================================ */
#define SB_FLASH_TOTAL_SIZE     (8 * 1024 * 1024)   /* Flash#1: 8MB */
#define SB_SECTOR_SIZE          (4096)                /* 4KB 鎵囧尯 */
#define SB_HEXDUMP_LINE_BYTES   16                    /* hex dump 姣忚瀛楄妭鏁�*/
#define SB_HEXDUMP_MAX_LEN      256                   /* hex dump 榛樿鏈�ぇ闀垮害 */

/* ============================================
 * 涓嬭浇杩涘害鍥炶皟
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
 * 鍛戒护: sb -d <size>  涓嬭浇闊虫簮
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
    /* 1. 妫�煡 FlashMgr */
    if (!BG_FlashMgr.IsReady()) {
        Shell_Printf("Error: FlashMgr not ready!\n");
        return -1;
    }

    /* 2. 鎿﹂櫎鐩爣鍖哄煙 (鎸夋墖鍖� */
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
            /* 姣�64KB 鎵撳嵃涓�杩涘害 */
            if ((erased % (64 * 1024)) == 0) {
                Shell_Printf("\r  Erased     : %u / %u KB", erased / 1024, erase_size / 1024);
            }
        }
        Shell_Printf("\n  Erase OK.\n");
    }

    /* 3. 鍒濆鍖栦笅杞戒細璇�*/
    bg_download_port_session_init(file_size);

    /* 4. 鍙戦� READY 鍝嶅簲 (鍛婄煡涓绘満鍙互寮�鍙戦�) */
    Shell_Printf("  Waiting for data packets...\n");
    dl_build_response(resp_buf, DL_RSP_READY, 0, DL_STATUS_OK);
    Shell_WriteRaw(resp_buf, DL_RESP_SIZE);

    /* 5. 璋冪敤 soundbank_download() 鈥�鍐呴儴寰幆鎺ユ敹鏁版嵁鍖呭苟鍐欏叆 Flash */
    ret = soundbank_manager.Download("shell_io", 0, file_size, 
                                      download_progress_cb, NULL);

    /* 6. 缁撴潫浼氳瘽 */
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
 * 鍛戒护: sb -i  鏌ョ湅闊虫簮淇℃伅
 * ============================================ */
static int cmd_sb_info(int argc, char *argv[])
{
    const char *info;
    uint32_t total_size = 0;
    uint32_t free_size = 0;
    SoundBank_Format fmt;

    (void)argc; (void)argv;

    Shell_Printf("=== Soundbank Info ===\n");

    /* 瀛樺偍淇℃伅 */
    BG_Storage.GetInfo(&total_size, &free_size);
    Shell_Printf("  Storage    : %u KB total, %u KB free\n",
                 total_size / 1024, free_size / 1024);

    /* 闊虫簮淇℃伅 */
    info = soundbank_manager.GetInfo();
    fmt = soundbank_manager.GetFormat();

    Shell_Printf("  Format     : %s\n",
                 fmt == SOUNDBANK_FORMAT_BG  ? "BGS" :
                 fmt == SOUNDBANK_FORMAT_SF2 ? "SF2" : "Unknown");
    Shell_Printf("  Info       : %s\n", info ? info : "N/A");

    return 0;
}

/* ============================================
 * 鍛戒护: sb -e  鎿﹂櫎闊虫簮瀛樺偍
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
 * 鍛戒护: sb -v  鏍￠獙闊虫簮鏁版嵁
 * ============================================ */
static int cmd_sb_verify(int argc, char *argv[])
{
    uint8_t header[8];
    uint32_t magic;
    int rd;

    (void)argc; (void)argv;

    Shell_Printf("=== Verify Soundbank ===\n");

    /* 鍒濆鍖栧瓨鍌ㄥ眰 */
    if (BG_Storage.Init(NULL, BG_STORAGE_MODE_READ_ONLY) != SUCCESS) {
        Shell_Printf("Error: Storage init failed\n");
        return -1;
    }

    /* 璇诲彇鏂囦欢澶�*/
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
 * 鍛戒护: sb -r <offset> [length]  璇诲彇闊虫簮鏁版嵁
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
 * 鍛戒护: sb -l [offset]  鍔犺浇闊虫簮
 * ============================================ */
static int cmd_sb_load(int argc, char *argv[])
{
    uint32_t offset = 0;
    BG_ERR ret;

    if (argc >= 1) {
        offset = (uint32_t)strtoul(argv[0], NULL, 0);
    }

    Shell_Printf("=== Load Soundbank (offset=0x%X) ===\n", offset);

    /* 鍏堝弽鍒濆鍖栨棫闊虫簮 */
    soundbank_manager.DeInit();

    /* 鍒濆鍖栨柊闊虫簮 */
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
 * 鍛戒护: sb -t <note> [velocity] [dur_ms] [program] [channel]  娴嬭瘯闊崇
 * 鐩存帴鎿嶄綔 MIDI 鐘舵� + SF2 澹伴儴姹� 缁曡繃 MIDI 鎺у埗鍣ㄦ淳鍙�
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

    /* 妫�煡鍚堟垚鍣ㄦ槸鍚﹀彲鐢�*/
    if (!BanGTsynth_IsInitialized()) {
        Shell_Printf("Error: BanGTsynth not initialized!\n");
        return -1;
    }

    if (soundbank_manager.GetFormat() == SOUNDBANK_FORMAT_UNKNOWN) {
        Shell_Printf("Error: No soundbank loaded!\n");
        return -1;
    }

    /* 瑙ｆ瀽鍙傛暟 */
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

    /* 闄愬箙 */
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
     * 鈽�杩涘叆鐩存帴 DAC 妯″紡: 闃绘 SourceCallback 鐨�ReadActiveSamples,
     *   閬垮厤涓�Shell 浠诲姟骞跺彂璁块棶 g_voices[] 瀵艰嚧鐘舵�鎹熷潖


    /*
     * Shell 鐩存帴楠岃瘉妯″紡 (瀹屽叏缁曡繃鍥炶皟鍜屾晥鏋滃浘):
     *   Phase 1: 鍒嗛厤澹伴儴 + ReadSamples 楠岃瘉鍚堟垚鍣ㄦ湁鏁版嵁
     *   Phase 2: ReadSamples 鈫�鎵撳寘绔嬩綋澹�鈫�AudioDAC_DataSet 鐩村啓 DAC
     *   Phase 3: 閲婃斁澹伴儴
     *
     * 杩欐潯閾捐矾: soundbank_manager.NoteOn 鈫�ReadSamples 鈫�sf2_callback 鈫�DAC
     * 鍏ㄩ儴鍦�Shell 浠诲姟鍐呭畬鎴� 涓嶄緷璧栭煶棰戝洖璋�鏁堟灉鍥�volatile
     */

    /* === Phase 1: 鍒嗛厤澹伴儴 + 楠岃瘉 ReadSamples === */
    soundbank_manager.NoteOn(note, velocity, program);
    Shell_Printf("  [1] NoteOn(%u, %u, %u)\n", note, velocity, program);

    {
        int16_t test_buf[48];
        uint8_t rs;
        int k;
        int16_t max_s = 0;

        /* 绗�1 娆¤鍙� 浼氳Е鍙�find_sample 鍒濆鍖�*/
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

        /* 绗�2 娆¤鍙� 纭鎸佺画浜х敓鏁版嵁 */
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

    /* === Phase 2: 鐩村啓 DAC 寰幆 === */
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
                        /* 鎵撳寘涓�uint32_t 绔嬩綋澹� 楂�6浣�R, 浣�6浣�L */
                        su = (uint16_t)s;
                        dac_buf[i] = ((uint32_t)su << 16) | (uint32_t)su;
                    }
                    AudioDAC_DataSet(DAC0, dac_buf, 48);
                    total_samples += 48;
                }
            } else {
                dac_full_count++;
                vTaskDelay(1);  /* DAC FIFO 婊� 绛�1ms */
            }
        }

        Shell_Printf("  [5] DAC done: %u samples, max_abs=%d, dac_waits=%u, final_rs=%u\n",
                     total_samples, (int)max_sample, dac_full_count, rs);
    }

    /* === Phase 3: 閲婃斁 === */
    soundbank_manager.NoteOff(note, program);
    Shell_Printf("  [6] NoteOff. Done.\n");


    return 0;
}



/* ============================================
 * 闊抽噺鎺у埗
 * sb -vol [volume]
 * 涓嶅甫鍙傛暟: 鏌ヨ褰撳墠闊抽噺
 * 甯﹀弬鏁� 璁剧疆闊抽噺 (0-100)
 * ============================================ */
static int cmd_sb_volume(int argc, char *argv[])
{
    uint8_t volume;

    if (!BanGTsynth_IsInitialized()) {
        Shell_Printf("Error: BanGTsynth not initialized!\n");
        return -1;
    }

    /* 鏌ヨ褰撳墠闊抽噺 */
    if (argc < 1) {
        volume = BanGTsynth_GetVolume();
        Shell_Printf("Current volume: %u%% (0-100)\n", volume);
        return 0;
    }

    /* 璁剧疆闊抽噺 */
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
 * 閫夐」瀹氫箟
 * ============================================ */
static const ShellOpt_t sb_options[] = {
    OPT("d", "download", "<size>",     "Download soundbank via data packets",  cmd_sb_download),
    OPT("i", "info",     NULL,         "Show soundbank & storage info",        cmd_sb_info),
    OPT("e", "erase",    NULL,         "Erase soundbank storage",              cmd_sb_erase),
    OPT("v", "verify",   NULL,         "Verify soundbank data integrity",      cmd_sb_verify),
    OPT("r", "read",     "<off> [len]","Hex dump storage data",                cmd_sb_read),
    OPT("l", "load",     "[offset]",   "Load soundbank from storage",          cmd_sb_load),
    OPT("t", "test",     "<note> [vel] [dur] [prog]", "Play a test note (direct DAC)",  cmd_sb_test),
    OPT("V", "volume",   "[0-100]",    "Set/query synth volume (0=mute, 100=full)", cmd_sb_volume),
    OPT_END()
};

/* ============================================
 * 妯″潡娉ㄥ唽
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
    /* 鍚堟垚鍣ㄦ湭鍚敤, 涓嶆敞鍐屽懡浠�*/
    return 0;
}

#endif /* BANGTSYNTH_EN */
