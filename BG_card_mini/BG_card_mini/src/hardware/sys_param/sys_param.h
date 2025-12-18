/**
 * sys_param.h - 系统参数掉电保存模块
 * 
 * 功能:
 *   - 使用芯片内部Flash保存系统参数
 *   - 上电自动读取并恢复参数
 *   - 支持各模块通过Shell命令保存参数
 *   - 模块化参数管理
 * 
 * 使用方法:
 *   1. 上电初始化: SysParam_Init()
 *   2. 获取参数: SysParam_Get()->audio.volume
 *   3. 修改参数: SysParam_Get()->audio.volume = 80;
 *   4. 保存参数: SysParam_Save() 或 shell命令 "param -s"
 * 
 * 内部Flash API (SDK提供):
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
 * 版本和配置
 *===========================================================================*/

#define SYS_PARAM_VERSION       0x0102      /* 参数版本号，修改结构体时需要更新 */
#define SYS_PARAM_MAGIC         0x50415241  /* "PARA" 魔术字 */

/* 内部Flash存储配置 
 * BP1048 内部Flash通常为1MB，使用最后一个4KB扇区存储参数
 * 扇区号 = 地址 / 4096
 */
#define SYS_PARAM_SECTOR_NUM    255         /* 使用第255扇区 (最后一个) */
#define SYS_PARAM_FLASH_ADDR    (SYS_PARAM_SECTOR_NUM * 4096)  /* 0xFF000 */
#define SYS_PARAM_SECTOR_SIZE   4096        /* 扇区大小 4KB */
#define SYS_PARAM_FLASH_TIMEOUT 100         /* Flash操作超时 (ms) */

/*===========================================================================
 * 参数结构定义
 *===========================================================================*/

/* 系统模块参数 */
typedef struct {
    uint8_t  boot_count;        /* 启动次数 (0-255循环) */
    uint8_t  language;          /* 语言设置 0:中文 1:英文 */
    uint8_t  brightness;        /* LCD亮度 0-100 */
    uint8_t  standby_time;      /* 待机时间 (分钟, 0=禁用) */
    uint8_t  debug_level;       /* 调试级别 0:关闭 1:错误 2:警告 3:信息 4:调试 */
    uint8_t  reserved[3];       /* 预留对齐 */
} SysParam_System_t;

/* 音频模块参数 */
typedef struct {
    uint8_t  master_volume;     /* 主音量 0-100 */
    uint8_t  mic_volume;        /* 麦克风音量 0-100 */
    uint8_t  effect_type;       /* 音效类型 */
    uint8_t  eq_mode;           /* EQ模式 0:平坦 1:流行 2:摇滚 3:古典 4:自定义 */
    int8_t   eq_bass;           /* 低音增益 -12 ~ +12 dB */
    int8_t   eq_mid;            /* 中音增益 -12 ~ +12 dB */
    int8_t   eq_treble;         /* 高音增益 -12 ~ +12 dB */
    uint8_t  mic_echo;          /* 麦克风混响 0-100 */
    uint8_t  mic_reverb;        /* 麦克风回声 0-100 */
    uint8_t  noise_gate;        /* 噪声门限 0-100 */
    uint8_t  reserved[2];       /* 预留对齐 */
} SysParam_Audio_t;

/* Looper模块参数 */
typedef struct {
    uint8_t  loop_count;        /* 循环轨道数 1-4 */
    uint8_t  overdub_mode;      /* 叠录模式 0:替换 1:混合 */
    uint8_t  quantize;          /* 量化开关 0:关 1:开 */
    uint8_t  click_volume;      /* 节拍器音量 0-100 */
    uint16_t tempo;             /* 默认BPM 40-240 */
    uint8_t  time_signature;    /* 拍号 0:4/4 1:3/4 2:6/8 */
    uint8_t  fade_time;         /* 淡入淡出时间 (10ms单位) */
    uint32_t max_loop_time;     /* 最大录音时间 (ms) */
} SysParam_Looper_t;

/* 蓝牙模块参数 */
typedef struct {
    uint8_t  enabled;           /* 蓝牙使能 */
    uint8_t  discoverable;      /* 可发现模式 */
    uint8_t  auto_connect;      /* 自动连接上次设备 */
    uint8_t  a2dp_volume;       /* A2DP音量 0-100 */
    char     device_name[16];   /* 设备名称 */
    uint8_t  paired_addr[6];    /* 上次配对设备地址 */
    uint8_t  reserved[2];       /* 预留对齐 */
} SysParam_Bluetooth_t;

/* 编码器/旋钮参数 */
typedef struct {
    uint8_t  sensitivity;       /* 灵敏度 1-10 */
    uint8_t  acceleration;      /* 加速度 0:关 1-5:加速级别 */
    uint8_t  direction;         /* 方向 0:正常 1:反转 */
    uint8_t  click_action;      /* 按键动作 */
    uint8_t  long_press_time;   /* 长按时间 (100ms单位) */
    uint8_t  reserved[3];       /* 预留对齐 */
} SysParam_Encoder_t;

/* LCD显示参数 */
typedef struct {
    uint8_t  contrast;          /* 对比度 0-100 */
    uint8_t  color_scheme;      /* 配色方案 0:默认 1:高对比 2:护眼 */
    uint8_t  font_size;         /* 字体大小 0:小 1:中 2:大 */
    uint8_t  screen_saver;      /* 屏保时间 (秒, 0=禁用) */
    uint16_t bg_color;          /* 背景颜色 RGB565 */
    uint16_t fg_color;          /* 前景颜色 RGB565 */
} SysParam_LCD_t;

/* 用户自定义参数区 (供扩展使用) */
typedef struct {
    uint8_t  data[32];          /* 用户数据 */
} SysParam_User_t;

/*===========================================================================
 * 主参数结构体
 *===========================================================================*/

typedef struct {
    /* 头部信息 */
    uint32_t magic;             /* 魔术字，用于验证 */
    uint16_t version;           /* 参数版本号 */
    uint16_t size;              /* 结构体大小 */
    uint32_t crc32;             /* CRC校验 */
    uint32_t write_count;       /* 写入次数 */
    
    /* 各模块参数 */
    SysParam_System_t    system;      /* 系统参数 */
    SysParam_Audio_t     audio;       /* 音频参数 */
    SysParam_Looper_t    looper;      /* Looper参数 */
    SysParam_Bluetooth_t bluetooth;   /* 蓝牙参数 */
    SysParam_Encoder_t   encoder;     /* 编码器参数 */
    SysParam_LCD_t       lcd;         /* LCD参数 */
    SysParam_User_t      user;        /* 用户自定义 */
    
    /* 预留空间 */
    uint8_t reserved[64];       /* 预留扩展 */
    
} SysParam_t;

/*===========================================================================
 * 状态码定义
 *===========================================================================*/

typedef enum {
    SYSPARAM_OK = 0,            /* 成功 */
    SYSPARAM_ERR_FLASH,         /* Flash操作错误 */
    SYSPARAM_ERR_CRC,           /* CRC校验失败 */
    SYSPARAM_ERR_VERSION,       /* 版本不匹配 */
    SYSPARAM_ERR_MAGIC,         /* 魔术字错误 */
    SYSPARAM_ERR_NOT_INIT,      /* 未初始化 */
    SYSPARAM_ERR_PARAM,         /* 参数错误 */
} SysParam_Status_t;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化系统参数模块
 *        上电时调用，自动从Flash读取参数
 *        如果读取失败则加载默认值
 * @return SYSPARAM_OK 成功
 */
SysParam_Status_t SysParam_Init(void);

/**
 * @brief 保存所有参数到Flash
 * @return SYSPARAM_OK 成功
 */
SysParam_Status_t SysParam_Save(void);

/**
 * @brief 获取参数结构体指针
 *        可直接读写参数，修改后需调用Save保存
 * @return 参数结构体指针
 */
SysParam_t* SysParam_Get(void);

/**
 * @brief 恢复默认参数
 *        不会自动保存，需手动调用Save
 * @return SYSPARAM_OK 成功
 */
SysParam_Status_t SysParam_LoadDefault(void);

/**
 * @brief 检查参数是否已修改（与Flash中的不同）
 * @return true 已修改，false 未修改
 */
bool SysParam_IsModified(void);

/**
 * @brief 获取写入次数
 * @return 写入次数
 */
uint32_t SysParam_GetWriteCount(void);

/**
 * @brief 打印当前参数
 */
void SysParam_Print(void);

/**
 * @brief 打印指定模块参数
 * @param module 模块名称: "system", "audio", "looper", "bt", "encoder", "lcd"
 */
void SysParam_PrintModule(const char *module);

/*===========================================================================
 * Shell 命令接口
 *===========================================================================*/

/**
 * @brief 注册Shell命令
 */
void SysParam_RegisterShellCommands(void);

/**
 * @brief Shell命令处理函数
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 0成功
 * 
 * 命令格式:
 *   param -s              保存所有参数
 *   param -d              恢复默认值
 *   param -p              打印所有参数
 *   param -p <module>     打印指定模块参数
 *   param -i              打印参数信息（版本、写入次数等）
 * 
 * 模块参数命令 (示例):
 *   audio -s              保存音频参数
 *   audio vol <0-100>     设置主音量
 *   audio mic <0-100>     设置麦克风音量
 *   looper -s             保存Looper参数
 *   looper tempo <40-240> 设置BPM
 */
int SysParam_ShellCmd(int argc, char *argv[]);

/*===========================================================================
 * 便捷宏定义
 *===========================================================================*/

/* 快速获取各模块参数 */
#define SYSPARAM_SYSTEM()       (&SysParam_Get()->system)
#define SYSPARAM_AUDIO()        (&SysParam_Get()->audio)
#define SYSPARAM_LOOPER()       (&SysParam_Get()->looper)
#define SYSPARAM_BLUETOOTH()    (&SysParam_Get()->bluetooth)
#define SYSPARAM_ENCODER()      (&SysParam_Get()->encoder)
#define SYSPARAM_LCD()          (&SysParam_Get()->lcd)
#define SYSPARAM_USER()         (&SysParam_Get()->user)

/* 快速读写示例 */
/* 读取: uint8_t vol = SYSPARAM_AUDIO()->master_volume; */
/* 写入: SYSPARAM_AUDIO()->master_volume = 80; SysParam_Save(); */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_PARAM_H__ */
