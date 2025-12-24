/**
 * sys_param.h - 绯荤粺鍙傛暟鎺夌數淇濆瓨妯″潡
 *
 * 鍔熻兘:
 *   - 浣跨敤鑺墖鍐呴儴Flash淇濆瓨绯荤粺鍙傛暟
 *   - 涓婄數鑷姩璇诲彇骞舵仮澶嶅弬鏁�
 *   - 鏀寔鍚勬ā鍧楅�杩嘢hell鍛戒护淇濆瓨鍙傛暟
 *   - 妯″潡鍖栧弬鏁扮鐞�
 *
 * 浣跨敤鏂规硶:
 *   1. 涓婄數鍒濆鍖� SysParam_Init()
 *   2. 鑾峰彇鍙傛暟: SysParam_Get()->audio.volume
 *   3. 淇敼鍙傛暟: SysParam_Get()->audio.volume = 80;
 *   4. 淇濆瓨鍙傛暟: SysParam_Save() 鎴�shell鍛戒护 "param -s"
 * 
 * 鍐呴儴Flash API (SDK鎻愪緵):
 *   - SpiFlashRead(addr, buf, len, timeout)
 *   - SpiFlashWrite(addr, buf, len, timeout)
 *   - SpiFlashErase(SECTOR_ERASE, sector_num, wait)
 *   - SpiFlashIOCtrl(IOCTL_FLASH_UNPROTECT, "\x35\xBA\x69", 3)
 */

#ifndef __SYS_PARAM_H__
#define __SYS_PARAM_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 鐗堟湰鍜岄厤缃�
 *===========================================================================*/

#define SYS_PARAM_VERSION       0x0102      /* 鍙傛暟鐗堟湰鍙凤紝淇敼缁撴瀯浣撴椂闇�鏇存柊 */
#define SYS_PARAM_MAGIC         0x50415241  /* "PARA" 榄旀湳瀛�*/

/* 鍐呴儴Flash瀛樺偍閰嶇疆
 * BP1048 鍐呴儴Flash閫氬父涓�MB锛屼娇鐢ㄦ渶鍚庝竴涓�KB鎵囧尯瀛樺偍鍙傛暟
 * 鎵囧尯鍙�= 鍦板潃 / 4096
 */
#define SYS_PARAM_SECTOR_NUM    255         /* 浣跨敤绗�55鎵囧尯 (鏈�悗涓�釜) */
#define SYS_PARAM_FLASH_ADDR    (SYS_PARAM_SECTOR_NUM * 4096)  /* 0xFF000 */
#define SYS_PARAM_SECTOR_SIZE   4096        /* 鎵囧尯澶у皬 4KB */
#define SYS_PARAM_FLASH_TIMEOUT 100         /* Flash鎿嶄綔瓒呮椂 (ms) */

/*===========================================================================
 * 鍙傛暟缁撴瀯瀹氫箟
 *===========================================================================*/

typedef enum{

	NORMAL_BOOT = 0,
	CHARGE_BOOT,
	BT_REBOOT,
	BOOT_STATUS_MAX,

}SYS_BOOT_STATUS;
/* 绯荤粺妯″潡鍙傛暟 */
typedef struct {
	uint8_t boot_mode;
} SysParam_System_t;

/* 闊抽妯″潡鍙傛暟 */
typedef struct {
    uint8_t  master_volume;     /* 涓婚煶閲�0-100 */
    uint8_t  mic_volume;        /* 楹﹀厠椋庨煶閲�0-100 */
    uint8_t  effect_type;       /* 闊虫晥绫诲瀷 */
    uint8_t  eq_mode;           /* EQ妯″紡 0:骞冲潶 1:娴佽 2:鎽囨粴 3:鍙ゅ吀 4:鑷畾涔�*/
    int8_t   eq_bass;           /* 浣庨煶澧炵泭 -12 ~ +12 dB */
    int8_t   eq_mid;            /* 涓煶澧炵泭 -12 ~ +12 dB */
    int8_t   eq_treble;         /* 楂橀煶澧炵泭 -12 ~ +12 dB */
    uint8_t  mic_echo;          /* 楹﹀厠椋庢贩鍝�0-100 */
    uint8_t  mic_reverb;        /* 楹﹀厠椋庡洖澹�0-100 */
    uint8_t  noise_gate;        /* 鍣０闂ㄩ檺 0-100 */
    uint8_t  reserved[2];       /* 棰勭暀瀵归綈 */
} SysParam_Audio_t;

/* Looper妯″潡鍙傛暟 */
typedef struct {
    uint8_t  loop_count;        /* 寰幆杞ㄩ亾鏁�1-4 */
    uint8_t  overdub_mode;      /* 鍙犲綍妯″紡 0:鏇挎崲 1:娣峰悎 */
    uint8_t  quantize;          /* 閲忓寲寮�叧 0:鍏�1:寮�*/
    uint8_t  click_volume;      /* 鑺傛媿鍣ㄩ煶閲�0-100 */
    uint16_t tempo;             /* 榛樿BPM 40-240 */
    uint8_t  time_signature;    /* 鎷嶅彿 0:4/4 1:3/4 2:6/8 */
    uint8_t  fade_time;         /* 娣″叆娣″嚭鏃堕棿 (10ms鍗曚綅) */
    uint32_t max_loop_time;     /* 鏈�ぇ褰曢煶鏃堕棿 (ms) */
} SysParam_Looper_t;

/* 钃濈墮妯″潡鍙傛暟 */
typedef struct {
    uint8_t  enabled;           /* 钃濈墮浣胯兘 */
    uint8_t  discoverable;      /* 鍙彂鐜版ā寮�*/
    uint8_t  auto_connect;      /* 鑷姩杩炴帴涓婃璁惧 */
    uint8_t  a2dp_volume;       /* A2DP闊抽噺 0-100 */
    char     device_name[16];   /* 璁惧鍚嶇О */
    uint8_t  paired_addr[6];    /* 涓婃閰嶅璁惧鍦板潃 */
    uint8_t  reserved[2];       /* 棰勭暀瀵归綈 */
} SysParam_Bluetooth_t;

/* 缂栫爜鍣�鏃嬮挳鍙傛暟 */
typedef struct {
    uint8_t  sensitivity;       /* 鐏垫晱搴�1-10 */
    uint8_t  acceleration;      /* 鍔犻�搴�0:鍏�1-5:鍔犻�绾у埆 */
    uint8_t  direction;         /* 鏂瑰悜 0:姝ｅ父 1:鍙嶈浆 */
    uint8_t  click_action;      /* 鎸夐敭鍔ㄤ綔 */
    uint8_t  long_press_time;   /* 闀挎寜鏃堕棿 (100ms鍗曚綅) */
    uint8_t  reserved[3];       /* 棰勭暀瀵归綈 */
} SysParam_Encoder_t;

/* LCD鏄剧ず鍙傛暟 */
typedef struct {
    uint8_t  contrast;          /* 瀵规瘮搴�0-100 */
    uint8_t  color_scheme;      /* 閰嶈壊鏂规 0:榛樿 1:楂樺姣�2:鎶ょ溂 */
    uint8_t  font_size;         /* 瀛椾綋澶у皬 0:灏�1:涓�2:澶�*/
    uint8_t  screen_saver;      /* 灞忎繚鏃堕棿 (绉� 0=绂佺敤) */
    uint16_t bg_color;          /* 鑳屾櫙棰滆壊 RGB565 */
    uint16_t fg_color;          /* 鍓嶆櫙棰滆壊 RGB565 */
} SysParam_LCD_t;



/* 鐢ㄦ埛鑷畾涔夊弬鏁板尯 (渚涙墿灞曚娇鐢� */
typedef struct {
    uint8_t  data[32];          /* 鐢ㄦ埛鏁版嵁 */
} SysParam_User_t;

/*===========================================================================
 * 涓诲弬鏁扮粨鏋勪綋
 *===========================================================================*/

typedef struct {
    /* 澶撮儴淇℃伅 */
     uint32_t magic;             /* 榄旀湳瀛楋紝鐢ㄤ簬楠岃瘉 */
//    uint16_t version;           /* 鍙傛暟鐗堟湰鍙�*/
//    uint16_t size;              /* 缁撴瀯浣撳ぇ灏�*/
//    uint32_t crc32;             /* CRC鏍￠獙 */
     uint32_t write_count;       /* 鍐欏叆娆℃暟 */
//
    /* 鍚勬ā鍧楀弬鏁�*/
    SysParam_System_t    system;      /* 绯荤粺鍙傛暟 */
//    SysParam_Audio_t     audio;       /* 闊抽鍙傛暟 */
//    SysParam_Looper_t    looper;      /* Looper鍙傛暟 */
//    SysParam_Bluetooth_t bluetooth;   /* 钃濈墮鍙傛暟 */
//    SysParam_Encoder_t   encoder;     /* 缂栫爜鍣ㄥ弬鏁�*/
//    SysParam_LCD_t       lcd;         /* LCD鍙傛暟 */
//    SysParam_hardware_t  hardware;
//    SysParam_User_t      user;        /* 鐢ㄦ埛鑷畾涔�*/
    
    
} SysParam_t;

/*===========================================================================
 * 鐘舵�鐮佸畾涔�
 *===========================================================================*/

typedef enum {
    SYSPARAM_OK = 0,            /* 鎴愬姛 */
    SYSPARAM_ERR_FLASH,         /* Flash鎿嶄綔閿欒 */
    SYSPARAM_ERR_CRC,           /* CRC鏍￠獙澶辫触 */
    SYSPARAM_ERR_VERSION,       /* 鐗堟湰涓嶅尮閰�*/
    SYSPARAM_ERR_MAGIC,         /* 榄旀湳瀛楅敊璇�*/
    SYSPARAM_ERR_NOT_INIT,      /* 鏈垵濮嬪寲 */
    SYSPARAM_ERR_PARAM,         /* 鍙傛暟閿欒 */
} SysParam_Status_t;

/*===========================================================================
 * API 鍑芥暟
 *===========================================================================*/

/**
 * @brief 鍒濆鍖栫郴缁熷弬鏁版ā鍧�
 *        涓婄數鏃惰皟鐢紝鑷姩浠嶧lash璇诲彇鍙傛暟
 *        濡傛灉璇诲彇澶辫触鍒欏姞杞介粯璁ゅ�
 * @return SYSPARAM_OK 鎴愬姛
 */
SysParam_Status_t SysParam_Init(void);

/**
 * @brief 淇濆瓨鎵�湁鍙傛暟鍒癋lash
 * @return SYSPARAM_OK 鎴愬姛
 */
SysParam_Status_t SysParam_Save(void);

/**
 * @brief 鑾峰彇鍙傛暟缁撴瀯浣撴寚閽�
 *        鍙洿鎺ヨ鍐欏弬鏁帮紝淇敼鍚庨渶璋冪敤Save淇濆瓨
 * @return 鍙傛暟缁撴瀯浣撴寚閽�
 */
SysParam_t* SysParam_Get(void);

/**
 * @brief 鎭㈠榛樿鍙傛暟
 *        涓嶄細鑷姩淇濆瓨锛岄渶鎵嬪姩璋冪敤Save
 * @return SYSPARAM_OK 鎴愬姛
 */
SysParam_Status_t SysParam_LoadDefault(void);

/**
 * @brief 妫�煡鍙傛暟鏄惁宸蹭慨鏀癸紙涓嶧lash涓殑涓嶅悓锛�
 * @return true 宸蹭慨鏀癸紝false 鏈慨鏀�
 */
bool SysParam_IsModified(void);

/**
 * @brief 鑾峰彇鍐欏叆娆℃暟
 * @return 鍐欏叆娆℃暟
 */
uint32_t SysParam_GetWriteCount(void);

/**
 * @brief 鎵撳嵃褰撳墠鍙傛暟
 */
void SysParam_Print(void);

/**
 * @brief 鎵撳嵃鎸囧畾妯″潡鍙傛暟
 * @param module 妯″潡鍚嶇О: "system", "audio", "looper", "bt", "encoder", "lcd"
 */
void SysParam_PrintModule(const char *module);

/*===========================================================================
 * Shell 鍛戒护鎺ュ彛
 *===========================================================================*/

/**
 * @brief 娉ㄥ唽Shell鍛戒护
 */
void SysParam_RegisterShellCommands(void);

/**
 * @brief Shell鍛戒护澶勭悊鍑芥暟
 * @param argc 鍙傛暟鏁伴噺
 * @param argv 鍙傛暟鏁扮粍
 * @return 0鎴愬姛
 * 
 * 鍛戒护鏍煎紡:
 *   param -s              淇濆瓨鎵�湁鍙傛暟
 *   param -d              鎭㈠榛樿鍊�
 *   param -p              鎵撳嵃鎵�湁鍙傛暟
 *   param -p <module>     鎵撳嵃鎸囧畾妯″潡鍙傛暟
 *   param -i              鎵撳嵃鍙傛暟淇℃伅锛堢増鏈�鍐欏叆娆℃暟绛夛級
 * 
 * 妯″潡鍙傛暟鍛戒护 (绀轰緥):
 *   audio -s              淇濆瓨闊抽鍙傛暟
 *   audio vol <0-100>     璁剧疆涓婚煶閲�
 *   audio mic <0-100>     璁剧疆楹﹀厠椋庨煶閲�
 *   looper -s             淇濆瓨Looper鍙傛暟
 *   looper tempo <40-240> 璁剧疆BPM
 */
int SysParam_ShellCmd(int argc, char *argv[]);

/*===========================================================================
 * 渚挎嵎瀹忓畾涔�
 *===========================================================================*/

/* 蹇�鑾峰彇鍚勬ā鍧楀弬鏁�*/
#define SYSPARAM_SYSTEM()       (&SysParam_Get()->system)
#define SYSPARAM_AUDIO()        (&SysParam_Get()->audio)
#define SYSPARAM_LOOPER()       (&SysParam_Get()->looper)
#define SYSPARAM_BLUETOOTH()    (&SysParam_Get()->bluetooth)
#define SYSPARAM_ENCODER()      (&SysParam_Get()->encoder)
#define SYSPARAM_LCD()          (&SysParam_Get()->lcd)
#define SYSPARAM_USER()         (&SysParam_Get()->user)

/* 蹇�璇诲啓绀轰緥 */
/* 璇诲彇: uint8_t vol = SYSPARAM_AUDIO()->master_volume; */
/* 鍐欏叆: SYSPARAM_AUDIO()->master_volume = 80; SysParam_Save(); */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_PARAM_H__ */
