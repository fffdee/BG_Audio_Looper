/**
 * @file bgs_types.h
 * @brief BGS 音源格式类型定义
 *
 * 从 hardware_interfance.h 中提取的纯数据类型，
 * 无平台依赖，可直接移植。
 *
 * BGS 解析器 (bgs_parser.c) 和 MIDI 处理依赖这些类型。
 * 新代码请 #include "bgs_types.h" 代替 #include "hardware_interfance.h"
 */

#ifndef BG_BGS_TYPES_H__
#define BG_BGS_TYPES_H__

#include <stdint.h>
#include "err_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * BGS 文件格式字段长度常量
 * ============================================ */

#define BGS_FILE_HEADER_BYTE        1
#define BGS_PROGRAM_COUNT_BYTE      2
#define BGS_FILE_VERSION_BYTE       3
#define BGS_FILE_ENCODER_BYTE       1
#define BGS_FILE_AUTHOR_BYTE        1
#define BGS_FILE_EMAIL_BYTE         1

#define BGS_PROGRAM_HEADER_BYTE     2
#define BGS_PROGRAM_BANK_BYTE       1
#define BGS_PROGRAM_INDEX_BYTE      1
#define BGS_PROGRAM_NAME_BYTE       1
#define BGS_PROGRAM_DESCRIPT_BYTE   1
#define BGS_PROGRAM_TOTAL_BYTE      4
#define BGS_PROGRAM_TYPE_BYTE       1

#define BGS_WAV_HEADER_BYTE         1
#define BGS_WAV_FILE_COUNT_BYTE     2
#define BGS_WAV_SAMPLERATE_BYTE     4
#define BGS_WAV_DEPTH_BYTE          1
#define BGS_WAV_CHANNEL_BYTE        1
#define BGS_WAV_FILESIZE_BYTE       4
#define BGS_NOTE_HEADER_BYTE        1
#define BGS_NOTE_BYTE               1
#define BGS_NOTE_MIN_BYTE           1
#define BGS_NOTE_MAX_BYTE           1
#define BGS_VEL_COUNT_BYTE          1
#define BGS_VEL_ID_BYTE             1
#define BGS_VEL_MIN_BYTE            1
#define BGS_VEL_MAX_BYTE            1

/* 向后兼容旧宏名 (无 BGS_ 前缀) */
#define FILE_HEADER_BYTE        BGS_FILE_HEADER_BYTE
#define PROGRAM_COUNT_BYTE      BGS_PROGRAM_COUNT_BYTE
#define FILE_VERSION_BYTE       BGS_FILE_VERSION_BYTE
#define FILE_ENCODER_BYTE       BGS_FILE_ENCODER_BYTE
#define FILE_AUTHOR_BYTE        BGS_FILE_AUTHOR_BYTE
#define FILE_EMAIL_BYTE         BGS_FILE_EMAIL_BYTE
#define PROGRAM_HEADER_BYTE     BGS_PROGRAM_HEADER_BYTE
#define PROGRAM_BANK_BYTE       BGS_PROGRAM_BANK_BYTE
#define PROGRAM_INDEX_BYTE      BGS_PROGRAM_INDEX_BYTE
#define PROGRAM_NAME_BYTE       BGS_PROGRAM_NAME_BYTE
#define PROGRAM_DESCRIPT_BYTE   BGS_PROGRAM_DESCRIPT_BYTE
#define PROGRAM_TOTAL_BYTE      BGS_PROGRAM_TOTAL_BYTE
#define PROGRAM_TYPE_BYTE       BGS_PROGRAM_TYPE_BYTE
#define WAV_HEADER_BYTE         BGS_WAV_HEADER_BYTE
#define WAV_FILE_COUNT_BYTE     BGS_WAV_FILE_COUNT_BYTE
#define WAV_SAMPLERATE_BYTE     BGS_WAV_SAMPLERATE_BYTE
#define WAV_DEPTH_BYTE          BGS_WAV_DEPTH_BYTE
#define WAV_CHANNEL_BYTE        BGS_WAV_CHANNEL_BYTE
#define WAV_FILESIZE_BYTE       BGS_WAV_FILESIZE_BYTE
#define NOTE_HEADER_BYTE        BGS_NOTE_HEADER_BYTE
#define NOTE_BYTE               BGS_NOTE_BYTE
#define NOTE_MIN_BYTE           BGS_NOTE_MIN_BYTE
#define NOTE_MAX_BYTE           BGS_NOTE_MAX_BYTE
#define VEL_COUNT_BYTE          BGS_VEL_COUNT_BYTE
#define VEL_ID_BYTE             BGS_VEL_ID_BYTE
#define VEL_MIN_BYTE            BGS_VEL_MIN_BYTE
#define VEL_MAX_BYTE            BGS_VEL_MAX_BYTE

/* ============================================
 * BGS 数据类型
 * ============================================ */

/** 音符映射信息 */
typedef struct {
    uint8_t  vel_id;
    uint8_t  note;
    uint8_t  min_note;
    uint8_t  max_note;
    uint8_t  min_vel;
    uint8_t  max_vel;
    uint32_t address;
} Read_Note_Info;

/** 音符活跃状态 (每个 program 最多 128 个音符) */
typedef struct {
    int8_t   active_sample_idx;   /* 当前激活的采样索引, -1 表示无效 */
    uint8_t  velocity;            /* 当前音符的力度值 */
} BG_Note_State;

/** BGS Program 数据 */
typedef struct {
    uint8_t  bank_index;
    uint8_t  program_index;
    uint8_t  name_len;
    uint8_t  descript_len;
    uint8_t  wav_header_count;
    uint8_t  note_info_count;
    uint8_t  audio_width;
    uint8_t  type;
    uint8_t  Ch;
    uint8_t  vel_count;
    uint8_t *name;
    uint8_t *descript;
    uint16_t frame;
    uint16_t file_count;
    uint32_t samplerate;
    uint32_t biaadress;
    uint32_t *bytecount;
    uint32_t *address_index;
    Read_Note_Info *Note_Info;
    BG_Note_State  note_states[128];
} BG_ProgramData;

/** BGS 文件全局数据 */
typedef struct {
    uint8_t  name_len;
    uint8_t  email_len;
    uint8_t *author_name;
    uint8_t *author_email;
    uint8_t  version[3];
    uint16_t program_count;
    uint32_t biaheader;
    uint32_t *base_address;
    BG_ProgramData *ProgramData;
} BG_ReadData;

/** BGS_Data 是 BG_ReadData 的别名 */
typedef BG_ReadData BGS_Data;

/** BGS 格式读取器接口 (回调式) */
typedef struct {
    BG_ERR  (*Init)(void);
    BG_ERR  (*DeInit)(void);
    uint8_t (*Callback)(short *, uint32_t, uint32_t, uint8_t);
    BG_ReadData Data;
} BG_Reader;

/** 全局 BGS 读取器实例 (在 bgs_parser.c 或 hardware_interfance.c 中定义) */
extern BG_Reader BG_reader;

#ifdef __cplusplus
}
#endif

#endif /* BG_BGS_TYPES_H__ */
