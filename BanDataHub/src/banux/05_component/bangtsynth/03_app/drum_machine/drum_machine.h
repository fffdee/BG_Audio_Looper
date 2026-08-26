/**
 * @file    drum_machine.h
 * @brief   独立鼓机模块 - 节拍编辑、非阻塞播放、Flash存储、BLE音源下载
 * @author  BanGO
 * @date    2026-06-01
 *
 * 架构:
 *   独立于合成器模块 (bangtsynth_node), 拥有自己的:
 *   - 16步×8轨音序数据 (可编辑, 支持命令行和App)
 *   - Tick驱动非阻塞播放 (在SourceCallback主任务上下文中)
 *   - Flash存储 (Pattern保存/加载到Storage分区)
 *   - BLE鼓机音源下载 (独立音源区, 复用下载协议)
 *
 * 音频输出:
 *   通过 soundbank_manager.NoteOn/Off() 在主任务上下文中直接调用,
 *   与合成器共享声部池和音源。
 *
 * Flash布局 (Storage分区 Flash#1, 共8MB):
 *   0x000000 - 0x5FFFFF : 合成器音源 (6MB)
 *   0x600000 - 0x600FFF : 鼓机配置头 (4KB)
 *   0x601000 - 0x608FFF : 节拍Pattern存储 (8槽×4KB)
 *   0x610000 - 0x7FFFFF : 鼓机专用音源 (~1.9MB)
 *
 * 依赖:
 *   - soundbank_manager.h (NoteOn/NoteOff)
 *   - bg_osal.h (bg_get_tick_ms)
 *   - DrumMachine_StorageOps_t (可移植存储接口)
 *
 * 宏控制:
 *   BANGTSYNTH_EN - 与合成器共用编译开关
 */

#ifndef __DRUM_MACHINE_H__
#define __DRUM_MACHINE_H__

#include "bg_config.h"

#if BANGTSYNTH_EN

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 常量定义
 *===========================================================================*/

#define DM_STEPS_PER_PATTERN    16      /* 每Pattern步数 (16分音符, 4/4拍) */
#define DM_TRACKS               8       /* 轨道数 (乐器数) */
#define DM_NAME_MAX_LEN         16      /* Pattern名称最大长度 */

/* General MIDI 鼓音符号 (通道10标准) */
#define DM_NOTE_KICK            36      /* Bass Drum */
#define DM_NOTE_SNARE           38      /* Acoustic Snare */
#define DM_NOTE_HIHAT_C         42      /* Closed Hi-Hat */
#define DM_NOTE_HIHAT_O         46      /* Open Hi-Hat */
#define DM_NOTE_TOM_L           45      /* Low Tom */
#define DM_NOTE_TOM_M           47      /* Mid Tom */
#define DM_NOTE_TOM_H           50      /* High Tom */
#define DM_NOTE_CRASH           49      /* Crash Cymbal */

/* 轨道位号 (在步bitmask中) */
#define DM_BIT_KICK             0
#define DM_BIT_SNARE            1
#define DM_BIT_HIHAT_C          2
#define DM_BIT_HIHAT_O          3
#define DM_BIT_TOM_L            4
#define DM_BIT_TOM_M            5
#define DM_BIT_TOM_H            6
#define DM_BIT_CRASH            7

/* 预设Pattern编号 */
#define DM_PATTERN_MAGIC        0x444D5054  /* "DMPT" */
#define DM_CONFIG_MAGIC         0x444D4347  /* "DMCG" */

/* 预设Pattern编号 */
#define DM_PRESET_ROCK          0
#define DM_PRESET_POP           1
#define DM_PRESET_FUNK          2
#define DM_PRESET_LATIN         3
#define DM_PRESET_COUNT         4

/*============================================================================
 * 数据结构
 *===========================================================================*/

/**
 * 鼓机Pattern
 * 16步×8轨 MIDI 音序数据, 内存格式
 */
typedef struct {
    uint32_t magic;                         /* DM_PATTERN_MAGIC */
    char     name[DM_NAME_MAX_LEN];         /* Pattern名称 */
    uint16_t bpm;                           /* 速度 (40-240) */
    uint8_t  steps[DM_STEPS_PER_PATTERN];   /* 每步bitmask: bit0=Kick, bit1=Snare, ... */
    uint8_t  velocity[DM_TRACKS];           /* 每轨道力度 (1-127, 默认100) */
    uint8_t  swing;                         /* Swing量 (0-100, 0=无swing) */
    uint8_t  reserved[15];                  /* 保留 (使结构体对齐到偶数大小) */
    uint16_t crc;                           /* CRC16校验 */
} DrumPattern_t;

/**
 * 鼓机运行时配置
 */
typedef struct {
    uint32_t magic;                         /* DM_CONFIG_MAGIC */
    uint8_t  active_slot;                   /* 当前活跃Pattern槽号 */
    uint8_t  volume;                        /* 鼓机音量 (0-100) */
    uint8_t  program;                       /* 音色号 (默认0) */
    uint8_t  reserved[33];                  /* 保留 */
    uint16_t crc;                           /* CRC16校验 */
} DrumConfig_t;

/**
 * 鼓机轨道信息 (只读, 用于UI查询)
 */
typedef struct {
    const char *name;                       /* 轨道名称 */
    const char *short_name;                 /* 短名称 (3字符, 用于ASCII显示) */
    uint8_t     note;                       /* MIDI音符号 */
    uint8_t     bit;                        /* 在步bitmask中的位号 */
} DrumTrackInfo_t;

/*============================================================================
 * 公共 API
 *===========================================================================*/

/*--- 初始化 ---*/

/**
 * @brief 初始化鼓机模块
 * 从Flash加载配置和当前Pattern, 如不存在则使用默认值
 * @return 0=成功, -1=失败
 */
int DrumMachine_Init(void);

/**
 * @brief 反初始化鼓机模块
 */
void DrumMachine_DeInit(void);

/**
 * @brief 鼓机tick处理 (在SourceCallback主任务上下文中调用)
 * 非阻塞, 根据xTaskGetTickCount()驱动音序器播放
 */
void DrumMachine_Tick(void);

/*--- 播放控制 ---*/

/**
 * @brief 开始播放当前Pattern
 * @param loop  1=循环播放, 0=播放一次
 * @return 0=成功, -1=未初始化
 */
int DrumMachine_Play(uint8_t loop);

/**
 * @brief 停止播放
 */
void DrumMachine_Stop(void);

/**
 * @brief 查询是否正在播放
 * @return 1=播放中, 0=停止
 */
uint8_t DrumMachine_IsPlaying(void);

/**
 * @brief 获取当前播放步号 (用于UI同步)
 * @return 当前步号 (0-15), 0xFF=未播放
 */
uint8_t DrumMachine_GetCurrentStep(void);

/*--- BPM / 音量 / 音色 ---*/

void DrumMachine_SetBPM(uint16_t bpm);
uint16_t DrumMachine_GetBPM(void);

void DrumMachine_SetVolume(uint8_t vol);
uint8_t DrumMachine_GetVolume(void);

void DrumMachine_SetProgram(uint8_t program);
uint8_t DrumMachine_GetProgram(void);

/*--- Pattern编辑 ---*/

/**
 * @brief 设置某步某轨道的开关
 * @param step   步号 (0-15)
 * @param track  轨道号 (0-7)
 * @param on     1=开, 0=关
 * @return 0=成功, -1=参数非法
 */
int DrumMachine_SetStep(uint8_t step, uint8_t track, uint8_t on);

/**
 * @brief 获取某步某轨道的开关状态
 * @param step   步号 (0-15)
 * @param track  轨道号 (0-7)
 * @return 1=开, 0=关, -1=参数非法
 */
int DrumMachine_GetStep(uint8_t step, uint8_t track);

/**
 * @brief 设置某步的完整bitmask
 * @param step   步号 (0-15)
 * @param mask   bitmask (bit0=Kick, bit1=Snare, ...)
 * @return 0=成功, -1=参数非法
 */
int DrumMachine_SetStepMask(uint8_t step, uint8_t mask);

/**
 * @brief 获取某步的完整bitmask
 * @param step   步号 (0-15)
 * @return bitmask, 0xFF=参数非法
 */
uint8_t DrumMachine_GetStepMask(uint8_t step);

/**
 * @brief 清空所有步
 */
void DrumMachine_ClearPattern(void);

/**
 * @brief 加载预设Pattern
 * @param preset  预设号 (DM_PRESET_ROCK/POP/FUNK/LATIN)
 * @return 0=成功, -1=无此预设
 */
int DrumMachine_LoadPreset(uint8_t preset);

/**
 * @brief 获取轨道信息
 * @param track  轨道号 (0-7)
 * @return 轨道信息指针, NULL=非法
 */
const DrumTrackInfo_t* DrumMachine_GetTrackInfo(uint8_t track);

/**
 * @brief 获取当前Pattern的只读指针
 * @return Pattern指针
 */
const DrumPattern_t* DrumMachine_GetPattern(void);

#ifdef __cplusplus
}
#endif

#endif /* BANGTSYNTH_EN */
#endif /* __DRUM_MACHINE_H__ */
