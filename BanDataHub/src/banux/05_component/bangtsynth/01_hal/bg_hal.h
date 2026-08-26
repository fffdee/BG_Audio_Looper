/*****************************************************
 * BanGTsynth - 历史 HAL 汇总（Linux / 旧音频表）
 *
 * 新移植请实现这 5 个头，不要再扩本文件：
 *   bg_storage.h  bg_osal.h  bg_extmem.h  bg_mem.h  bg_log.h
 * 产品入口：bg_synth.h
 *****************************************************/

#ifndef _BG_HAL_H__
#define _BG_HAL_H__

#include <stddef.h>  // for size_t
#include <stdint.h>  // for uint8_t, uint32_t, etc.
#include "bg_config.h"
#include "err_handle.h"

/*============================================
 * 音频输出接口 (Audio Output Interface)
 * 说明: 平台需实现音频数据输出功能
 *============================================*/
typedef struct {
    /**
     * 初始化音频设备
     * @param bit_depth: 位深度 (8/16/24/32)
     * @param channels: 声道数 (1=单声道, 2=立体声)
     * @param sample_rate: 采样率 (Hz)
     * @return: BG_OK 成功, 其他值失败
     */
    BG_ERR (*init)(uint8_t bit_depth, uint8_t channels, uint32_t sample_rate);
    
    /**
     * 反初始化音频设备
     */
    void (*deinit)(void);
    
    /**
     * 启用/禁用音频输出
     * @param enable: 1=启用, 0=禁用
     */
    void (*enable)(uint8_t enable);
    
    /**
     * 获取音频设备状态
     * @return: 1=播放中, 0=停止
     */
    uint8_t (*get_state)(void);
    
    /**
     * 音频数据回调 (框架调用此函数填充音频缓冲区)
     * @param buffer: 音频缓冲区指针
     * @param frame_count: 帧数
     */
    void (*callback)(int16_t *buffer, uint32_t frame_count);
    
    /**
     * 播放循环 (在主循环中调用，用于非中断方式)
     */
    void (*play_loop)(void);
    
} bg_audio_interface_t;

/*============================================
 * 音频数据读取接口 (Audio Data Read Interface)
 * 说明: 平台需实现从存储介质读取音频样本
 *============================================*/

/*============================================
 * 音频数据读取接口 (Audio Data Read Interface)
 * 说明: 平台需实现从存储介质读取音频样本
 *============================================*/

/* 音符信息 */
typedef struct {
    uint8_t vel_id;        // 力度ID
    uint8_t note;          // MIDI音符号
    uint8_t min_note;      // 最小音符
    uint8_t max_note;      // 最大音符
    uint8_t min_vel;       // 最小力度
    uint8_t max_vel;       // 最大力度
    uint32_t address;      // 样本地址
} bg_note_info_t;

/* 音色程序数据 */
typedef struct {
    uint8_t bank_index;         // 音色库索引
    uint8_t program_index;      // 程序索引
    uint8_t name_len;          // 名称长度
    uint8_t descript_len;      // 描述长度
    uint8_t wav_header_count;  // WAV头数量
    uint8_t note_info_count;   // 音符信息数量
    uint8_t audio_width;       // 音频位深
    uint8_t type;              // 类型
    uint8_t channel;           // 声道数
    uint8_t vel_count;         // 力度层数
    uint8_t *name;             // 名称
    uint8_t *descript;         // 描述
    uint16_t frame;            // 帧数
    uint16_t file_count;       // 文件数量
    uint32_t samplerate;       // 采样率
    uint32_t base_address;     // 基地址
    uint32_t *byte_count;      // 字节数
    uint32_t *address_index;   // 地址索引
    bg_note_info_t *note_info; // 音符信息
} bg_program_data_t;

/* 音色库数据 */
typedef struct {
    uint8_t name_len;              // 作者名长度
    uint8_t email_len;             // 邮箱长度
    uint8_t *author_name;          // 作者名
    uint8_t *author_email;         // 作者邮箱
    uint8_t version[3];            // 版本号
    uint16_t program_count;        // 程序数量
    uint32_t base_header;          // 基础头地址
    uint32_t *base_address;        // 基地址数组
    bg_program_data_t *program_data; // 程序数据
} bg_soundbank_data_t;

typedef struct {
    /**
     * 初始化音频数据读取
     * @return: BG_OK 成功, 其他值失败
     */
    BG_ERR (*init)(void);
    
    /**
     * 反初始化
     */
    BG_ERR (*deinit)(void);
    
    /**
     * 读取音频样本数据
     * @param buffer: 输出缓冲区
     * @param note: MIDI音符号
     * @param frame_count: 读取帧数
     * @param velocity: 力度值
     * @return: 1=继续读取, 0=读取完成
     */
    uint8_t (*read_sample)(int16_t *buffer, uint32_t note, uint32_t frame_count, uint8_t velocity);
    
    /**
     * 获取音色库数据
     * @return: 音色库数据指针
     */
    bg_soundbank_data_t* (*get_soundbank)(void);
    
} bg_storage_interface_t;

/*============================================
 * 输入设备接口 (Input Interface)
 * 说明: 可选功能，用于键盘、按钮等输入
 *============================================*/
#if BG_ENABLE_KEYBOARD_INPUT
typedef struct {
    /**
     * 初始化输入设备
     * @return: BG_OK 成功, 其他值失败
     */
    BG_ERR (*init)(void);
    
    /**
     * 反初始化
     */
    void (*deinit)(void);
    
    /**
     * 输入轮询 (在主循环中调用)
     * @return: 键值或0
     */
    uint8_t (*poll)(void);
    
} bg_input_interface_t;
#endif

/*============================================
 * 定时器接口 (Timer Interface)
 * 说明: 用于MIDI控制、包络生成等定时任务
 *============================================*/
typedef struct {
    /**
     * 初始化定时器
     * @param interval_us: 定时间隔 (微秒)
     * @return: BG_OK 成功, 其他值失败
     */
    BG_ERR (*init)(uint32_t interval_us);
    
    /**
     * 反初始化定时器
     */
    void (*deinit)(void);
    
    /**
     * 设置定时器回调函数
     * @param callback: 回调函数指针
     */
    void (*set_callback)(void (*callback)(void));
    
    /**
     * 启动定时器
     */
    void (*start)(void);
    
    /**
     * 停止定时器
     */
    void (*stop)(void);
    
} bg_timer_interface_t;

/*============================================
 * 内存管理接口 (Memory Interface)
 * 说明: 标准内存操作，可映射到标准库或自定义实现
 *============================================*/
typedef struct {
    /**
     * 分配内存
     * @param size: 字节数
     * @return: 内存指针或 NULL
     */
    void* (*malloc)(uint32_t size);
    
    /**
     * 释放内存
     * @param ptr: 内存指针
     */
    void (*free)(void *ptr);
    
    /**
     * 内存复制
     */
    void* (*memcpy)(void *dest, const void *src, uint32_t n);
    
    /**
     * 内存设置
     */
    void* (*memset)(void *s, int c, uint32_t n);
    
} bg_memory_interface_t;

/*============================================
 * Flash存储接口 (Flash Storage Interface)
 * 说明: 用于音源文件写入到Flash存储
 *============================================*/
typedef struct {
    /**
     * 初始化Flash存储
     * @return: 0=成功, 其他值=失败
     */
    int (*init)(void);
    
    /**
     * 反初始化
     */
    void (*deinit)(void);
    
    /**
     * 擦除Flash区域
     * @param address: Flash地址
     * @param size: 擦除大小(字节)
     * @return: 0=成功, 其他值=失败
     */
    int (*erase)(uint32_t address, uint32_t size);
    
    /**
     * 写入数据到Flash
     * @param address: Flash地址
     * @param data: 数据缓冲区
     * @param size: 数据大小(字节)
     * @return: 0=成功, 其他值=失败
     */
    int (*write)(uint32_t address, const uint8_t *data, uint32_t size);
    
    /**
     * 从Flash读取数据
     * @param address: Flash地址
     * @param buffer: 输出缓冲区
     * @param size: 读取大小(字节)
     * @return: 0=成功, 其他值=失败
     */
    int (*read)(uint32_t address, uint8_t *buffer, uint32_t size);
    
    /**
     * 获取Flash信息
     * @param total_size: 输出总容量(字节)
     * @param block_size: 输出块大小(字节)
     * @return: 0=成功, 其他值=失败
     */
    int (*get_info)(uint32_t *total_size, uint32_t *block_size);
    
} bg_hal_storage_t;

/*============================================
 * 文件系统抽象接口 (File System HAL)
 * 
 * 目的: 让音源解析器与具体平台解耦
 * - Linux: 使用 stdio (fopen/fread/fseek)
 * - 嵌入式: 使用 Flash/SD/QSPI 等
 * - 测试: 使用内存模拟
 *============================================*/

/* 文件句柄类型 (平台相关) */
typedef void* bg_file_handle_t;

/* 文件定位模式 */
typedef enum {
    BG_SEEK_SET = 0,  // 从文件开头
    BG_SEEK_CUR = 1,  // 从当前位置
    BG_SEEK_END = 2   // 从文件末尾
} bg_seek_mode_t;

/* 文件系统接口 */
typedef struct {
    /**
     * 打开文件
     * @param filename 文件路径
     * @param mode 打开模式 ("rb" = 只读二进制)
     * @return 文件句柄, NULL表示失败
     */
    bg_file_handle_t (*open)(const char *filename, const char *mode);
    
    /**
     * 关闭文件
     * @param handle 文件句柄
     * @return 0=成功, 非0=失败
     */
    int (*close)(bg_file_handle_t handle);
    
    /**
     * 读取数据
     * @param buffer 输出缓冲区
     * @param size 每个元素大小
     * @param count 元素个数
     * @param handle 文件句柄
     * @return 实际读取的元素个数
     */
    size_t (*read)(void *buffer, size_t size, size_t count, bg_file_handle_t handle);
    
    /**
     * 文件定位
     * @param handle 文件句柄
     * @param offset 偏移量
     * @param whence 定位模式
     * @return 0=成功, 非0=失败
     */
    int (*seek)(bg_file_handle_t handle, long offset, bg_seek_mode_t whence);
    
    /**
     * 获取当前位置
     * @param handle 文件句柄
     * @return 当前文件位置, -1表示错误
     */
    long (*tell)(bg_file_handle_t handle);
    
    /**
     * 检测文件结束
     * @param handle 文件句柄
     * @return 非0=已到文件末尾, 0=未到末尾
     */
    int (*eof)(bg_file_handle_t handle);
    
} bg_filesystem_t;

/*============================================
 * HAL 接口实例 (需要在平台代码中定义)
 *============================================*/
extern bg_audio_interface_t bg_audio_interface;
extern bg_storage_interface_t bg_storage_interface;
extern bg_timer_interface_t bg_timer_interface;
extern bg_memory_interface_t bg_memory_interface;
extern bg_hal_storage_t bg_hal_storage;
extern bg_filesystem_t bg_filesystem;  // 文件系统HAL

#if BG_ENABLE_KEYBOARD_INPUT
extern bg_input_interface_t bg_input_interface;
#endif

/*============================================
 * HAL 初始化/反初始化
 * 说明: 平台需实现这两个函数
 *============================================*/

/**
 * 初始化所有硬件接口
 * @return: BG_OK 成功, 其他值失败
 */
BG_ERR bg_hal_init(void);

/**
 * 反初始化所有硬件接口
 */
void bg_hal_deinit(void);

#endif /* _BG_HAL_H__ */
