/**
 * @file soundbank_manager.h
 * @brief 统一音源格式管理器接口
 * 
 * 提供 BGS 和 SF2 两种音源格式的统一访问接口。
 * 通过函数指针表实现运行时格式切换。
 */
#ifndef __SOUNDBANK_MANAGER_H__
#define __SOUNDBANK_MANAGER_H__

#include <stdint.h>
#include <stddef.h>
#include "err_handle.h"
#include "hardware_interfance.h"  /* BG_ReadData (BGS_Data) */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 音源格式枚举
 * ============================================ */
typedef enum {
    SOUNDBANK_FORMAT_UNKNOWN = 0,
    SOUNDBANK_FORMAT_BG,            /* BGS 自有格式 */
    SOUNDBANK_FORMAT_SF2            /* SoundFont 2 格式 */
} SoundBank_Format;

/* ============================================
 * BGS 数据类型别名
 * ============================================ */
/** BGS_Data 是 BG_ReadData 的类型别名 (定义在 hardware_interfance.h) */
typedef BG_ReadData BGS_Data;

/* ============================================
 * 下载进度回调
 * ============================================ */
/**
 * 音源下载进度回调函数类型
 * @param bytes_written  已写入的字节数
 * @param total_size     总大小
 * @param user_data      用户自定义数据
 */
typedef void (*soundbank_download_progress_cb_t)(size_t bytes_written, size_t total_size, void *user_data);

/* ============================================
 * 音源管理器接口
 * ============================================ */
typedef struct {
    /**
     * 初始化音源管理器
     * @param offset_addr  音源数据在存储介质中的起始偏移
     * @return SUCCESS 或错误码
     */
    BG_ERR (*Init)(uint32_t offset_addr);

    /**
     * 释放音源资源
     * @return SUCCESS 或错误码
     */
    BG_ERR (*DeInit)(void);

    /**
     * 读取音频采样数据 (统一接口)
     * @param data     输出缓冲区 (int16_t PCM)
     * @param note     MIDI 音符号 (0-127)
     * @param count    请求的采样帧数
     * @param program  MIDI 程序号
     * @return 1=有数据, 0=播放结束或无数据
     */
    uint8_t (*ReadSamples)(short *data, uint32_t note, uint32_t count, uint8_t program);

    /**
     * 获取当前音源格式
     * @return SoundBank_Format 枚举值
     */
    SoundBank_Format (*GetFormat)(void);

    /**
     * 获取音源描述信息字符串
     * @return 只读字符串
     */
    const char* (*GetInfo)(void);

    /**
     * 获取 BGS 格式内部数据 (仅 BGS 格式有效)
     * @return BGS_Data 指针, 非 BGS 格式返回 NULL
     */
    BGS_Data* (*GetBGSData)(void);

    /**
     * 音符开启
     * @param note     MIDI 音符号
     * @param velocity 力度 (0-127)
     * @param program  MIDI 程序号
     */
    void (*NoteOn)(uint8_t note, uint8_t velocity, uint8_t program);

    /**
     * 音符关闭
     * @param note     MIDI 音符号
     * @param program  MIDI 程序号
     */
    void (*NoteOff)(uint8_t note, uint8_t program);

    /**
     * 关闭指定程序的所有音符
     * @param program  MIDI 程序号
     */
    void (*AllNoteOff)(uint8_t program);

    /**
     * 下载音源数据到存储设备
     * @param data_source   数据源标识 (文件路径或URL)
     * @param offset        写入偏移
     * @param size          数据大小 (0=自动检测)
     * @param progress_cb   进度回调 (可为 NULL)
     * @param user_data     传递给回调的用户数据
     * @return SUCCESS 或错误码
     */
    BG_ERR (*Download)(const char *data_source, uint32_t offset, size_t size,
                       soundbank_download_progress_cb_t progress_cb, void *user_data);

} SoundBank_Manager;

/* ============================================
 * 全局实例
 * ============================================ */
/** 音源管理器全局实例 (soundbank_manager.c 中定义) */
extern SoundBank_Manager soundbank_manager;

/* ============================================
 * 供解析器内部使用的辅助函数
 * ============================================ */

/**
 * 从当前音源文件读取数据 (自动计算绝对偏移)
 * @param offset   相对于当前音源文件起始的偏移
 * @param buffer   输出缓冲区
 * @param size     读取字节数
 * @return 实际读取的字节数, 失败返回 -1
 */
int soundbank_storage_read(uint32_t offset, void *buffer, size_t size);

/**
 * 获取当前音源文件在存储层中的偏移
 * @return 文件偏移量
 */
uint32_t soundbank_get_file_offset(void);

/**
 * 获取当前音源文件大小
 * @return 文件大小 (字节)
 */
uint32_t soundbank_get_file_size(void);

#ifdef __cplusplus
}
#endif

#endif /* __SOUNDBANK_MANAGER_H__ */
