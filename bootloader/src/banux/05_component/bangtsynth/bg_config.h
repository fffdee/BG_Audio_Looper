/**
 * BanGTsynth 妗嗘灦閰嶇疆鏂囦欢
 *
 * 鍔熻兘:
 * - 闆嗕腑绠＄悊鎵�湁妯″潡鐨勫姛鑳藉紑鍏�
 * - 閰嶇疆闊抽鍙傛暟銆佺紦鍐插尯澶у皬绛�
 * - 鏀寔骞冲彴鐗瑰畾鐨勯厤缃�
 * - 瀹屽叏瑙ｈ� CMake,鎵�湁閰嶇疆鍦ㄦ鏂囦欢涓畾涔�
 * 
 * 浣跨敤鏂规硶:
 * - 鏂瑰紡1: 鎵嬪姩淇敼姝ゆ枃浠朵腑鐨勫畯瀹氫箟
 * - 鏂瑰紡2: 浣跨敤鍥惧舰鍖栭厤缃伐鍏� python components/config_tool.py
 * - 淇敼鍚庨噸鏂扮紪璇戠敓鏁�(涓嶉渶瑕佷慨鏀�CMakeLists.txt)
 * 
 * 鏉′欢缂栬瘧璇存槑:
 * - 灏�ENABLE_XXX 璁句负 0 鍙鐢ㄥ搴旀ā鍧�
 * - 浠ｇ爜涓娇鐢�#if ENABLE_XXX 杩涜鏉′欢缂栬瘧
 * - CMakeLists.txt 浼氱紪璇戞墍鏈夋簮鏂囦欢,浣嗙鐢ㄧ殑妯″潡涓嶄細琚摼鎺ヤ娇鐢�
 */

#ifndef _BG_CONFIG_H__
#define _BG_CONFIG_H__

/* ============================================
 * 骞冲彴閰嶇疆
 * ============================================ */
#define BANGTSYNTH_EN
#ifdef BANGTSYNTH_EN
/**
 * 鐩爣骞冲彴閫夋嫨
 * 鍙�鍊� BG_PLATFORM_LINUX, BG_PLATFORM_STM32, BG_PLATFORM_ESP32, BG_PLATFORM_BP10
 */
#define BG_PLATFORM_LINUX       1
#define BG_PLATFORM_STM32       2
#define BG_PLATFORM_ESP32       3
#define BG_PLATFORM_BP10        4

#ifndef BG_TARGET_PLATFORM
#define BG_TARGET_PLATFORM      BG_PLATFORM_BP10
#endif

/* ============================================
 * 鍔熻兘妯″潡瑁佸壀
 * ============================================ */

/**
 * MIDI 鎺у埗鍣�
 * 鍔熻兘: MIDI 娑堟伅瑙ｆ瀽銆侀煶绗︾鐞嗐�閫氶亾绠＄悊
 * 璧勬簮鍗犵敤: ~2KB RAM, ~5KB Flash
 */
#ifndef ENABLE_MIDI_CONTROLLER
#define ENABLE_MIDI_CONTROLLER  1  // 1=鍚敤, 0=绂佺敤
#endif
#
/**
 * 娣烽煶鍣ㄦā鍧�
 * 鍔熻兘: 澶氶煶杞ㄦ贩闊炽�闊抽噺鎺у埗銆佸０閬撳钩琛�
 * 璧勬簮鍗犵敤: ~4KB RAM, ~3KB Flash
 */
#ifndef ENABLE_MIXER
#define ENABLE_MIXER            1  // 1=鍚敤, 0=绂佺敤
#endif

/**
 * 闊抽澶勭悊鍣�
 * 鍔熻兘: 鍔ㄦ�鑼冨洿鍘嬬缉(DRC)銆佸潎琛″櫒(EQ)銆佹晥鏋滃櫒
 * 璧勬簮鍗犵敤: ~8KB RAM, ~10KB Flash
 */
#ifndef ENABLE_AUDIO_PROCESSOR
#define ENABLE_AUDIO_PROCESSOR  0  // 0=绂佺敤锛圗ffect Graph宸茬鐞嗘墍鏈塃Q/DRC锛岄伩鍏嶉噸澶嶅垎閰嶅爢鍐呭瓨锛�
#endif

/**
 * 鍖呯粶鐢熸垚鍣�
 * 鍔熻兘: ADSR 鍖呯粶鎺у埗銆佸姏搴﹀搷搴�
 * 璧勬簮鍗犵敤: ~1KB RAM, ~2KB Flash
 * 渚濊禆: SF2 闊虫簮闇�姝ゆā鍧�
 */
#ifndef ENABLE_ENVELOPE_GENERATOR
#define ENABLE_ENVELOPE_GENERATOR 1  // 1=鍚敤, 0=绂佺敤
#endif

/**
 * 闊冲簭鍣�
 * 鍔熻兘: MIDI 搴忓垪鎾斁銆佽妭鎷嶆帶鍒�
 * 璧勬簮鍗犵敤: ~6KB RAM, ~8KB Flash
 */
#ifndef ENABLE_SEQUENCER
#define ENABLE_SEQUENCER        0  // 1=鍚敤, 0=绂佺敤 (榛樿绂佺敤)
#endif

/**
 * USB MIDI 杈撳叆
 * 鍔熻兘: 閫氳繃 USB 鎺ユ敹 MIDI 娑堟伅
 * 璧勬簮鍗犵敤: ~2KB RAM, ~4KB Flash
 * 渚濊禆: Linux ALSA / STM32 USB 搴�
 */
#ifndef ENABLE_USB_MIDI
#define ENABLE_USB_MIDI         0  // 1=鍚敤, 0=绂佺敤
#endif

/**
 * 閿洏杈撳叆
 * 鍔熻兘: 閿洏鏄犲皠鍒�MIDI 闊崇,鐢ㄤ簬璋冭瘯
 * 璧勬簮鍗犵敤: ~1KB RAM, ~2KB Flash
 * 渚濊禆: Linux termios / 鏃犱緷璧�MCU)
 */
#ifndef ENABLE_KEYBOARD_INPUT
#define ENABLE_KEYBOARD_INPUT   1  // 1=鍚敤, 0=绂佺敤
#endif

/**
 * 闊虫簮涓嬭浇鎺ュ彛
 * 鍔熻兘: 鏀寔杩愯鏃朵笅杞介煶婧愬埌瀛樺偍璁惧
 * 璧勬簮鍗犵敤: ~2KB RAM, ~3KB Flash
 */
#ifndef ENABLE_SOUNDBANK_DOWNLOAD
#define ENABLE_SOUNDBANK_DOWNLOAD 1  // 1=鍚敤, 0=绂佺敤
#endif

/* ============================================
 * 鍚堟垚鍣ㄥ唴瀛樹紭鍖栬鍓�(鐩爣: 鈮�0KB 闈欐�RAM)
 * ============================================ */

/**
 * SF2 鏍煎紡鏀寔
 * 鍔熻兘: SoundFont 2 闊虫簮鏍煎紡瑙ｆ瀽鍜屾挱鏀�
 * 绂佺敤鍚庡彧鏀寔 BGS 鑷湁鏍煎紡,鑺傜渷 ~4KB RAM + ~8KB Flash
 */
#ifndef SYNTH_ENABLE_SF2
#define SYNTH_ENABLE_SF2        1  // 1=鍚敤, 0=绂佺敤
#endif

/**
 * X-Fi 寮曟搸鏀寔 (SF2 瀛愬姛鑳�
 * 鍔熻兘: 鏀寔 Creative X-Fi 缂栫爜鐨�SF2 鏂囦欢
 * 绂佺敤鍚庝粎鏀寔鏍囧噯 SF2 寮曟搸,鍑忓皯浠ｇ爜浣撶Н
 */
#ifndef SYNTH_ENABLE_XFI_ENGINE
#define SYNTH_ENABLE_XFI_ENGINE 0  // 1=鍚敤, 0=绂佺敤
#endif

/**
 * 闊虫簮绋嬪簭(闊宠壊)鏁伴噺涓婇檺
 * 鏍囧噯 GM: 128, 绮剧畝妯″紡: 8~16
 * 姣忎釜绋嬪簭妲界害 28 瀛楄妭闈欐� + 鍔ㄦ�閲囨牱鏁版嵁
 */
#ifndef SYNTH_MAX_PROGRAMS
#define SYNTH_MAX_PROGRAMS      16
#endif

/* ============================================
 * 榛樿鍐呭祵闊虫簮閫夋嫨
 * ============================================ */

/**
 * 榛樿闊虫簮閫夋嫨
 * 鍙�鍊�
 *   BG_SOUNDBANK_4OPFM        - 4OPFM.SF2 (FM鎵撳嚮涔� 998KB) [寰呭疄鐜癩
 *   BG_SOUNDBANK_SOFT_PIANO   - Thrift Store Spinet Piano (閽㈢惔闊宠壊, 391KB) [宸插疄鐜癩
 * 
 * 闊虫簮鏁版嵁浣嶄簬: BanBox/src/banux/05_component/bangtsynth/durm_data/sf2_source.c
 */
#define BG_SOUNDBANK_4OPFM        1
#define BG_SOUNDBANK_SOFT_PIANO   2

#ifndef BG_DEFAULT_SOUNDBANK
//#define BG_DEFAULT_SOUNDBANK    BG_SOUNDBANK_4OPFM  // 榛樿浣跨敤 4OPFM (寰呭疄鐜�
#define BG_DEFAULT_SOUNDBANK    BG_SOUNDBANK_SOFT_PIANO  // 榛樿浣跨敤 Thrift Store Spinet Piano (鍐呭祵闊虫簮)
#endif

/**
 * 鍚嶇О缂撳啿鍖哄ぇ灏�(瀛楄妭)
 * SF2 閾惰鍚�寮曟搸鍚嶇紦鍐插尯 (鏍囧噯: 256, 绮剧畝: 32)
 */
#ifndef SYNTH_NAME_BUFFER_SIZE
#define SYNTH_NAME_BUFFER_SIZE  32
#endif

/**
 * SF2 澹伴儴姹犲ぇ灏�
 * 鎺у埗 SF2 鏈�ぇ鍚屾椂鍙戝０鏁� 榛樿涓�BG_MAX_POLYPHONY 涓�嚧
 */
#ifndef SYNTH_MAX_VOICES
#define SYNTH_MAX_VOICES        BG_MAX_POLYPHONY
#endif

/* ============================================
 * 闊抽鍙傛暟閰嶇疆
 * ============================================ */

/**
 * 閲囨牱鐜�(Hz)
 * 鍙�鍊� 44100, 48000, 96000
 * 寤鸿: 48000 (CD 闊宠川)
 */
#ifndef BG_SAMPLE_RATE
#if BG_TARGET_PLATFORM == BG_PLATFORM_BP10
#define BG_SAMPLE_RATE          44100  /* BP10: 涓庣郴缁�DAC 閲囨牱鐜囦竴鑷�*/
#else
#define BG_SAMPLE_RATE          48000  
#endif
#endif

/**
 * 閲囨牱浣嶆繁 (bits)
 * 鍙�鍊� 16, 24
 * 寤鸿: 16 (瓒冲,鑺傜渷鍐呭瓨)
 */
#ifndef BG_SAMPLE_WIDTH
#define BG_SAMPLE_WIDTH         16  
#endif

/**
 * 澹伴亾鏁�
 * 鍙�鍊� 1 (鍗曞０閬�, 2 (绔嬩綋澹�
 */
#ifndef BG_CHANNELS
#define BG_CHANNELS             1  
#endif

/**
 * 鏈�ぇ澶嶉煶鏁�
 * 璇存槑: 鍚屾椂鍙戝０鐨勯煶绗︽暟閲�
 * 鑼冨洿: 1-128
 * 璧勬簮褰卞搷: 姣忎釜澶嶉煶 ~1KB RAM
 */
#ifndef BG_MAX_POLYPHONY
#if BG_TARGET_PLATFORM == BG_PLATFORM_BP10
#define BG_MAX_POLYPHONY        8   /* BP10: 闄愬埗澶嶉煶鏁颁互鑺傜渷 RAM 鍜�CPU */
#else
#define BG_MAX_POLYPHONY        64  
#endif
#endif

/**
 * 闊抽缂撳啿鍖哄ぇ灏�(frames)
 * 璇存槑: 姣忔澶勭悊鐨勬牱鏈抚鏁�
 * 寤鸿: 48 (1ms @ 48kHz), 96 (2ms), 192 (4ms)
 * 寤惰繜褰卞搷: 瓒婂皬寤惰繜瓒婁綆,浣�CPU 鍗犵敤瓒婇珮
 */
#ifndef BG_AUDIO_BUFFER_SIZE
#define BG_AUDIO_BUFFER_SIZE    48  
#endif

/**
 * ALSA 纭欢缂撳啿鍖哄ぇ灏�(ms)
 * 璇存槑: 闊抽璁惧鐨勭紦鍐叉椂闀�
 * 寤鸿: 8-32ms
 * 褰卞搷: 瓒婂ぇ瓒婄ǔ瀹氫絾寤惰繜瓒婇珮
 */
#ifndef BG_ALSA_BUFFER_MS
#define BG_ALSA_BUFFER_MS       16  
#endif

/**
 * ALSA Period 澶у皬 (ms)
 * 璇存槑: 姣忔涓柇鐨勬暟鎹潡澶у皬
 * 寤鸿: buffer_ms / 4
 */
#ifndef BG_ALSA_PERIOD_MS
#define BG_ALSA_PERIOD_MS       4  
#endif

/* ============================================
 * 瀛樺偍閰嶇疆
 * ============================================ */

/**
 * 闊虫簮瀛樺偍澶у皬 (bytes)
 * 璇存槑: soundbank.bin 鏂囦欢鐨勫浐瀹氬ぇ灏�
 * 鍙�鍊� 16MB, 32MB, 64MB
 */
#ifndef BG_STORAGE_SIZE
#define BG_STORAGE_SIZE         33554432  // 32MB
#endif

/**
 * 瀛樺偍鎵囧尯澶у皬 (bytes)
 * 璇存槑: Flash 鎿﹂櫎鐨勬渶灏忓崟浣�
 * 寤鸿: 4096 (4KB)
 */
#ifndef BG_STORAGE_SECTOR_SIZE
#define BG_STORAGE_SECTOR_SIZE  4096  
#endif

/* ============================================
 * MIDI 閰嶇疆
 * ============================================ */

/**
 * MIDI 閫氶亾鏁�
 * 鏍囧噯: 16 閫氶亾
 */
#ifndef BG_MIDI_CHANNELS
#define BG_MIDI_CHANNELS        16  
#endif

/**
 * MIDI 闊崇鑼冨洿
 * 鏍囧噯: 0-127
 */
#ifndef BG_MIDI_NOTE_MIN
#define BG_MIDI_NOTE_MIN        0  
#endif

#ifndef BG_MIDI_NOTE_MAX
#define BG_MIDI_NOTE_MAX        127  
#endif

/**
 * MIDI 绋嬪簭鏁伴噺 (闊宠壊鏁�
 * 鏍囧噯: 128 (GM 鏍囧噯)
 */
#ifndef BG_MIDI_PROGRAMS
#define BG_MIDI_PROGRAMS        128  
#endif

/* ============================================
 * 璋冭瘯閰嶇疆
 * ============================================ */

/**
 * 鏃ュ織绾у埆
 * 0 = 鍏抽棴, 1 = 閿欒, 2 = 璀﹀憡, 3 = 淇℃伅, 4 = 璋冭瘯
 */
#ifndef BG_LOG_LEVEL
#define BG_LOG_LEVEL            2  // 榛樿鏄剧ず鍒�INFO
#endif

/**
 * 妯″潡璋冭瘯寮�叧
 * 1 = 鍚敤璇ユā鍧楃殑璇︾粏鏃ュ織
 */
#ifndef BG_DEBUG_MIDI
#define BG_DEBUG_MIDI           0  
#endif

#ifndef BG_DEBUG_AUDIO_PROC
#define BG_DEBUG_AUDIO_PROC     1  
#endif

#ifndef BG_DEBUG_SOUNDBANK
#define BG_DEBUG_SOUNDBANK      1  
#endif

#ifndef BG_DEBUG_EFFECT_DRC
#define BG_DEBUG_EFFECT_DRC     1  
#endif

#ifndef BG_DEBUG_EFFECT_EQ
#define BG_DEBUG_EFFECT_EQ      1  
#endif

/* ============================================
 * 鎬ц兘浼樺寲閰嶇疆
 * ============================================ */

/**
 * 鍚敤蹇�鏁板杩愮畻
 * 1 = 浣跨敤鏌ヨ〃/杩戜技绠楁硶浠ｆ浛绮剧‘璁＄畻
 * 褰卞搷: 鎻愬崌 ~20% 鎬ц兘,绮惧害鎹熷け <1%
 */
#ifndef BG_ENABLE_FAST_MATH
#define BG_ENABLE_FAST_MATH     1  
#endif

/**
 * 鍚敤 SIMD 浼樺寲
 * 1 = 浣跨敤 SIMD 鎸囦护鍔犻�闊抽澶勭悊
 * 渚濊禆: ARM NEON / x86 SSE2
 */
#ifndef BG_ENABLE_SIMD
#define BG_ENABLE_SIMD          0  // 榛樿绂佺敤,闇�骞冲彴鏀寔
#endif

/**
 * 涓诲惊鐜欢鏃�(寰)
 * 璇存槑: 鎺у埗 ProcessAudio() 璋冪敤棰戠巼
 * 寤鸿: 500 (2000Hz) ~ 1000 (1000Hz)
 * 褰卞搷: 瓒婂皬 CPU 鍗犵敤瓒婇珮,鍝嶅簲瓒婂揩
 */
#ifndef BG_MAIN_LOOP_DELAY_US
#define BG_MAIN_LOOP_DELAY_US   400  
#endif

/* ============================================
 * 鍐呭瓨閰嶇疆 (閫傜敤浜庡祵鍏ュ紡骞冲彴)
 * ============================================ */

/**
 * 鍔ㄦ�鍐呭瓨鍒嗛厤
 * 0 = 浣跨敤闈欐�鍐呭瓨姹� 1 = 浣跨敤 malloc/free
 */
#ifndef BG_USE_DYNAMIC_MEMORY
#if BG_TARGET_PLATFORM == BG_PLATFORM_BP10
#define BG_USE_DYNAMIC_MEMORY   1  /* BP10: 浣跨敤 FreeRTOS pvPortMalloc */
#else
#define BG_USE_DYNAMIC_MEMORY   1  
#endif
#endif

/**
 * 闈欐�鍐呭瓨姹犲ぇ灏�(bytes)
 * 浠呭綋 BG_USE_DYNAMIC_MEMORY=0 鏃朵娇鐢�
 */
#ifndef BG_MEMORY_POOL_SIZE
#define BG_MEMORY_POOL_SIZE     131072  // 128KB
#endif

/* ============================================
 * 閰嶇疆楠岃瘉 (缂栬瘧鏃舵鏌�
 * ============================================ */

#if BG_MAX_POLYPHONY > 128
#error "BG_MAX_POLYPHONY must be <= 128"
#endif

#if BG_SAMPLE_RATE != 44100 && BG_SAMPLE_RATE != 48000 && BG_SAMPLE_RATE != 96000
#error "BG_SAMPLE_RATE must be 44100, 48000, or 96000"
#endif

#if BG_CHANNELS != 1 && BG_CHANNELS != 2
#error "BG_CHANNELS must be 1 or 2"
#endif

#if ENABLE_ENVELOPE_GENERATOR == 0 && ENABLE_SOUNDBANK_DOWNLOAD == 1
#warning "SF2 soundbank requires ENABLE_ENVELOPE_GENERATOR=1"
#endif

/* ============================================
 * 閰嶇疆鎽樿瀹�(鐢ㄤ簬鎵撳嵃閰嶇疆淇℃伅)
 * ============================================ */

#define BG_CONFIG_STRING \
    "BanGTsynth Configuration:\n" \
    "  Platform: " _BG_PLATFORM_NAME "\n" \
    "  Sample Rate: " _STR(BG_SAMPLE_RATE) " Hz\n" \
    "  Channels: " _STR(BG_CHANNELS) "\n" \
    "  Max Polyphony: " _STR(BG_MAX_POLYPHONY) "\n" \
    "  Buffer Size: " _STR(BG_AUDIO_BUFFER_SIZE) " frames\n" \
    "  Modules: MIDI=" _STR(ENABLE_MIDI_CONTROLLER) \
             " Mixer=" _STR(ENABLE_MIXER) \
             " AudioProc=" _STR(ENABLE_AUDIO_PROCESSOR) "\n"

#define _STR(x) #x
#define _XSTR(x) _STR(x)

#if BG_TARGET_PLATFORM == BG_PLATFORM_LINUX
#define _BG_PLATFORM_NAME "Linux"
#elif BG_TARGET_PLATFORM == BG_PLATFORM_STM32
#define _BG_PLATFORM_NAME "STM32"
#elif BG_TARGET_PLATFORM == BG_PLATFORM_ESP32
#define _BG_PLATFORM_NAME "ESP32"
#elif BG_TARGET_PLATFORM == BG_PLATFORM_BP10
#define _BG_PLATFORM_NAME "BP10"
#else
#define _BG_PLATFORM_NAME "Unknown"
#endif

/* ============================================
 * 鍏煎鎬у畾涔�(鏃х増鏈畯鍚嶇О鏄犲皠)
 * ============================================ */

/* 鍔熻兘妯″潡寮�叧鍏煎鎬�*/
#define BG_ENABLE_MIXER             ENABLE_MIXER
#define BG_ENABLE_MIDI_CONTROLLER   ENABLE_MIDI_CONTROLLER
#define BG_ENABLE_AUDIO_PROCESSOR   ENABLE_AUDIO_PROCESSOR
#define BG_ENABLE_USB_MIDI          ENABLE_USB_MIDI
#define BG_ENABLE_KEYBOARD_INPUT    ENABLE_KEYBOARD_INPUT

/* 闊抽鍙傛暟鍏煎鎬�*/
#define BG_AUDIO_BIT_DEPTH          16  
#define BG_MAX_CHANNELS             BG_CHANNELS
#define BG_MS_SAMPLE                (BG_SAMPLE_RATE / 1000)
#define BG_BUFFER_SIZE              BG_AUDIO_BUFFER_SIZE
#define BG_MAX_RING_BUFFER_SIZE     (BG_AUDIO_BUFFER_SIZE * 2)
#define BG_BYTES_PER_SAMPLE         (sizeof(int16_t) * BG_CHANNELS)

/* 娣烽煶鍣ㄩ厤缃吋瀹规� */
#if ENABLE_MIXER
    #define BG_MAX_MIX_COUNT        10
    #define BG_MIX_BUF_COUNT        1024
#endif

/* 鏂囦欢绯荤粺閰嶇疆 */
#define BG_MAX_FILE_COUNT           100
#define BG_WAV_START_ADDRESS        0x19000

/* BGS 鏂囦欢鏍煎紡閰嶇疆 */
#define BG_FILE_HEADER_BYTE         1
#define BG_PROGRAM_COUNT_BYTE       2
#define BG_FILE_VERSION_BYTE        3
#define BG_FILE_ENCODER_BYTE        1
#define BG_FILE_AUTHOR_BYTE         1
#define BG_FILE_EMAIL_BYTE          1

#define BG_PROGRAM_HEADER_BYTE      2
#define BG_PROGRAM_BANK_BYTE        1
#define BG_PROGRAM_INDEX_BYTE       1
#define BG_PROGRAM_NAME_BYTE        1
#define BG_PROGRAM_DESCRIPT_BYTE    1
#define BG_PROGRAM_TOTAL_BYTE       4
#define BG_PROGRAM_TYPE_BYTE        1

#define BG_WAV_HEADER_BYTE          1
#define BG_WAV_FILE_COUNT_BYTE      2
#define BG_WAV_SAMPLERATE_BYTE      4
#define BG_WAV_DEPTH_BYTE           1
#define BG_WAV_CHANNEL_BYTE         1
#define BG_WAV_FILESIZE_BYTE        4

#define BG_NOTE_HEADER_BYTE         1
#define BG_NOTE_BYTE                1
#define BG_NOTE_MIN_BYTE            1
#define BG_NOTE_MAX_BYTE            1
#define BG_VEL_COUNT_BYTE           1
#define BG_VEL_ID_BYTE              1
#define BG_VEL_MIN_BYTE             1
#define BG_VEL_MAX_BYTE             1

#define BG_DEBUG_ENABLED            1  

/* 閿欒鐮佺被鍨嬪畾涔�*/
typedef enum {
    BG_OK = 0,
    BG_ERROR,
    BG_ERROR_INVALID_PARAM,
    BG_ERROR_NOT_INITIALIZED,
    BG_ERROR_BUSY,
    BG_ERROR_TIMEOUT,
    BG_ERROR_NO_MEMORY
} bg_status_t;

#endif /* _BG_CONFIG_H__ */
#endif
