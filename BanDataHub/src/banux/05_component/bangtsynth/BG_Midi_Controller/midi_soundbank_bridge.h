#ifndef _MIDI_SOUNDBANK_BRIDGE_H__
#define _MIDI_SOUNDBANK_BRIDGE_H__

#include <stdint.h>
#include "err_handle.h"
#include "midi_info.h"

/**
 * MIDI 音源桥接模块
 * 
 * 功能:
 * - 连接 MIDI 控制器和音源管理器
 * - 管理音源加载和切换
 * - 提供 MIDI 事件到音频数据的完整处理链
 * - 支持多通道复音合成
 * 
 * 处理流程:
 * 1. MIDI消息 → MIDI控制器解析
 * 2. Note On/Off → 更新通道状态
 * 3. Program Change → 切换音色
 * 4. 音频处理循环 → 从音源读取样本 → 混音输出
 */

/* 音源配置 */
typedef struct {
    const char *soundbank_path;     // 音源文件路径
    uint8_t auto_detect_format;     // 自动检测格式 (1=启用, 0=禁用)
    uint8_t enable_polyphony;       // 启用复音 (1=启用, 0=单音)
    uint8_t max_polyphony;          // 最大复音数 (1-32)
} MIDI_SoundBank_Config;

/* MIDI 音源桥接接口 */
typedef struct {
    /**
     * 初始化桥接模块
     * 
     * 功能:
     * - 初始化 MIDI 控制器
     * - 加载音源文件
     * - 初始化通道状态
     * 
     * @param config 音源配置
     * @return SUCCESS=成功, 错误码=失败
     */
    BG_ERR (*Init)(const MIDI_SoundBank_Config *config);
    
    /**
     * 释放桥接模块资源
     * 
     * @return SUCCESS=成功
     */
    BG_ERR (*DeInit)(void);
    
    /**
     * 处理 MIDI 消息
     * 
     * 接收外部MIDI设备的消息并更新内部状态
     * 
     * @param data MIDI消息数据 (包含状态字节和数据字节)
     * @param len 消息长度
     */
    void (*ProcessMIDI)(uint8_t *data, uint8_t len);
    
    /**
     * 处理音频数据生成
     * 
     * 在主循环中调用，生成混音后的音频数据
     * 
     * 功能:
     * - 遍历所有活动音符
     * - 从音源读取样本数据
     * - 应用力度控制
     * - 混音所有复音
     * - 输出到音频接口
     */
    void (*ProcessAudio)(void);
    
    /**
     * 加载新音源
     * 
     * 支持运行时动态切换音源文件
     * 
     * @param soundbank_path 音源文件路径
     * @return SUCCESS=成功, 错误码=失败
     */
    BG_ERR (*LoadSoundBank)(const char *soundbank_path);
    
    /**
     * 获取当前音源信息
     * 
     * @return 音源信息字符串 (格式、版本等)
     */
    const char* (*GetSoundBankInfo)(void);
    
    /**
     * 设置通道音色 (Program Change)
     * 
     * @param channel MIDI通道 (0-15)
     * @param program 音色号 (0-127)
     */
    void (*SetChannelProgram)(uint8_t channel, uint8_t program);
    
    /**
     * 获取通道状态
     * 
     * @param channel MIDI通道 (0-15)
     * @return 通道信息指针
     */
    BG_Channel_Info* (*GetChannelInfo)(uint8_t channel);
    
    /**
     * 复位所有通道
     * 
     * 停止所有音符，清除所有状态
     */
    void (*Reset)(void);
    
} MIDI_SoundBank_Bridge;

/* 导出接口实例 */
extern MIDI_SoundBank_Bridge midi_soundbank_bridge;

/* 默认配置 */
#define MIDI_SOUNDBANK_DEFAULT_CONFIG {     \
    .soundbank_path = NULL,                 \
    .auto_detect_format = 1,                \
    .enable_polyphony = 1,                  \
    .max_polyphony = 32,                    \
}

#endif /* _MIDI_SOUNDBANK_BRIDGE_H__ */
