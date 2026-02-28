#include "midi_soundbank_bridge.h"
#include "midi_controller.h"
#include "soundbank_manager.h"
#include "hardware_interfance.h"
#include <stdio.h>
#include <string.h>

/**
 * MIDI 音源桥接模块实现
 * 
 * 职责:
 * - 管理 MIDI 控制器和音源管理器的生命周期
 * - 协调 MIDI 事件处理和音频数据生成
 * - 提供统一的接口供上层调用
 */

/* 内部状态 */
static uint8_t g_initialized = 0;
static MIDI_SoundBank_Config g_config;

/* 内部函数声明 */
static BG_ERR bridge_init(const MIDI_SoundBank_Config *config);
static BG_ERR bridge_deinit(void);
static void bridge_process_midi(uint8_t *data, uint8_t len);
static void bridge_process_audio(void);
static BG_ERR bridge_load_soundbank(const char *soundbank_path);
static const char* bridge_get_soundbank_info(void);
static void bridge_set_channel_program(uint8_t channel, uint8_t program);
static BG_Channel_Info* bridge_get_channel_info(uint8_t channel);
static void bridge_reset(void);

/* 接口实例 */
MIDI_SoundBank_Bridge midi_soundbank_bridge = {
    .Init = bridge_init,
    .DeInit = bridge_deinit,
    .ProcessMIDI = bridge_process_midi,
    .ProcessAudio = bridge_process_audio,
    .LoadSoundBank = bridge_load_soundbank,
    .GetSoundBankInfo = bridge_get_soundbank_info,
    .SetChannelProgram = bridge_set_channel_program,
    .GetChannelInfo = bridge_get_channel_info,
    .Reset = bridge_reset,
};

/**
 * 初始化桥接模块
 */
static BG_ERR bridge_init(const MIDI_SoundBank_Config *config)
{
    BG_ERR result;

    if (g_initialized) {
        printf("[MIDI Bridge] Warning: Already initialized, reinitializing\n");
        bridge_deinit();
    }
    
    /* 保存配置 */
    if (config) {
        memcpy(&g_config, config, sizeof(MIDI_SoundBank_Config));
    } else {
        /* 使用默认配置 */
        MIDI_SoundBank_Config default_config = MIDI_SOUNDBANK_DEFAULT_CONFIG;
        memcpy(&g_config, &default_config, sizeof(MIDI_SoundBank_Config));
    }
    
    /* 初始化 MIDI 控制器 */
    printf("[MIDI Bridge] Initializing MIDI controller\n");
    BG_MIDI_controller.Init();
    
    /* 加载音源 */
    if (g_config.soundbank_path) {
        printf("[MIDI Bridge] Loading soundbank: %s\n", g_config.soundbank_path);
        result = soundbank_manager.Init(g_config.soundbank_path);
        if (result != SUCCESS) {
            printf("[MIDI Bridge] Error: Failed to load soundbank\n");
            return result;
        }
        
        printf("[MIDI Bridge] Soundbank loaded: %s\n", 
               soundbank_manager.GetInfo());
    } else {
        printf("[MIDI Bridge] Warning: No soundbank loaded\n");
    }
    
    g_initialized = 1;
    printf("[MIDI Bridge] Initialized successfully\n");
    
    return SUCCESS;
}

/**
 * 释放桥接模块资源
 */
static BG_ERR bridge_deinit(void)
{
    if (!g_initialized) {
        return SUCCESS;
    }
    
    /* 释放音源 */
    soundbank_manager.DeInit();
    
    /* 重置 MIDI 控制器 */
    bridge_reset();
    
    g_initialized = 0;
    printf("[MIDI Bridge] Deinitialized\n");
    
    return SUCCESS;
}

/**
 * 处理 MIDI 消息
 */
static void bridge_process_midi(uint8_t *data, uint8_t len)
{
    if (!g_initialized) {
        printf("[MIDI Bridge] Error: Not initialized\n");
        return;
    }
    
    /* 转发到 MIDI 控制器处理 */
    BG_MIDI_controller.MIDI_Handle(data, len);
}

/**
 * 处理音频数据生成
 */
static void bridge_process_audio(void)
{
    if (!g_initialized) {
        return;
    }
    
    /* 调用 MIDI 控制器的音频处理 */
    BG_MIDI_controller.ProcessAudio();
}

/**
 * 加载新音源
 */
static BG_ERR bridge_load_soundbank(const char *soundbank_path)
{
    BG_ERR result;

    if (!soundbank_path) {
        printf("[MIDI Bridge] Error: Invalid soundbank path\n");
        return ENABLE_INVALID_INPUT;
    }
    
    /* 释放旧音源 */
    soundbank_manager.DeInit();
    
    /* 加载新音源 */
    printf("[MIDI Bridge] Loading soundbank: %s\n", soundbank_path);
    result = soundbank_manager.Init(soundbank_path);
    
    if (result == SUCCESS) {
        /* 更新配置 */
        g_config.soundbank_path = soundbank_path;
        printf("[MIDI Bridge] Soundbank loaded: %s\n", 
               soundbank_manager.GetInfo());
    } else {
        printf("[MIDI Bridge] Error: Failed to load soundbank\n");
    }
    
    return result;
}

/**
 * 获取当前音源信息
 */
static const char* bridge_get_soundbank_info(void)
{
    if (!g_initialized) {
        return "Not initialized";
    }
    
    return soundbank_manager.GetInfo();
}

/**
 * 设置通道音色
 */
static void bridge_set_channel_program(uint8_t channel, uint8_t program)
{
    if (!g_initialized || channel >= 16 || program >= 128) {
        return;
    }
    
    BG_MIDI_data.BG_channel_info[channel].program_index = program;
    printf("[MIDI Bridge] Channel %d: Program changed to %d\n", channel, program);
}

/**
 * 获取通道状态
 */
static BG_Channel_Info* bridge_get_channel_info(uint8_t channel)
{
    if (!g_initialized || channel >= 16) {
        return NULL;
    }
    
    return &BG_MIDI_data.BG_channel_info[channel];
}

/**
 * 复位所有通道
 */
static void bridge_reset(void)
{
    uint8_t ch;
    
    /* 清空所有通道状态 */
    memset(&BG_MIDI_data.BG_channel_info, 0, sizeof(BG_MIDI_data.BG_channel_info));
    
    /* 重新初始化默认值 */
    for (ch = 0; ch < 16; ch++) {
        BG_MIDI_data.BG_channel_info[ch].program_index = 0;
        BG_MIDI_data.BG_channel_info[ch].NoteOn_count = 0;
        BG_MIDI_data.BG_channel_info[ch].mono_poly_onoff = g_config.enable_polyphony;
    }
    
    printf("[MIDI Bridge] All channels reset\n");
}
