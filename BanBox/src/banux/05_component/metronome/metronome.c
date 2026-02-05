/**
 **************************************************************************************
 * @file    metronome.c
 * @brief   独立节拍器模块实现
 *          从audio_looper模块独立出来，作为效果图的数据源节点
 *
 * @author  BanGO
 * @version V1.0.0
 *
 * @Copyright (C) 2025, Audio Looper Project. All rights reserved.
 **************************************************************************************
 */

#include "metronome.h"
#include "debug.h"
#include <nds32_intrinsic.h>
#include <math.h>
#include <string.h>

/* ============================================================================
 * 静态变量定义
 * ============================================================================ */

/* 全局节拍器运行时状态 */
static MetronomeRuntime_t g_metronome_runtime = {
    .state = METRONOME_OFF,
    .config = {
        .bpm = METRONOME_DEFAULT_BPM,
        .beats_per_measure = METRONOME_DEFAULT_BEATS_PER_MEASURE,
        .downbeat_freq = METRONOME_DEFAULT_DOWNBEAT_FREQ,
        .regular_beat_freq = METRONOME_DEFAULT_REGULAR_BEAT_FREQ,
        .beat_duration_ms = METRONOME_DEFAULT_BEAT_DURATION,
        .volume = METRONOME_DEFAULT_VOLUME
    },
    .beat_interval_samples = 0,
    .beat_duration_samples = 0,
    .sample_counter = 0,
    .beat_sample_counter = 0,
    .current_beat = 0,
    .is_beat_active = 0,
    .current_beat_type = BEAT_TYPE_DOWNBEAT,
    .sine_phase = 0.0f
};

/* ============================================================================
 * 内部辅助函数声明
 * ============================================================================ */
static void metronome_update_timing_params(void);
static float metronome_generate_sine_sample(float freq, float* phase);
static void metronome_advance_beat(void);

/* ============================================================================
 * 内部辅助函数实现
 * ============================================================================ */

/**
 * @brief 更新节拍器计时参数
 */
static void metronome_update_timing_params(void) {
    // 计算节拍间隔（样本数）
    // BPM = 每分钟拍数，所以每拍间隔 = 60秒 / BPM
    // 样本数 = 间隔秒数 * 采样率
    uint32_t beat_interval_ms = (60000 / g_metronome_runtime.config.bpm);  // 毫秒
    g_metronome_runtime.beat_interval_samples = (beat_interval_ms * METRONOME_SAMPLE_RATE) / 1000;

    // 计算节拍持续时间（样本数）
    g_metronome_runtime.beat_duration_samples =
        (g_metronome_runtime.config.beat_duration_ms * METRONOME_SAMPLE_RATE) / 1000;
}

/**
 * @brief 生成正弦波样本
 * @param freq 频率（Hz）
 * @param phase 相位累加器指针
 * @return 正弦波样本值（-1.0到1.0）
 */
static float metronome_generate_sine_sample(float freq, float* phase) {
    float sample = sinf(*phase);
    *phase += 2.0f * M_PI * freq / METRONOME_SAMPLE_RATE;

    // 防止相位累积器溢出
    if (*phase >= 2.0f * M_PI) {
        *phase -= 2.0f * M_PI;
    }

    return sample;
}

/**
 * @brief 推进到下一拍
 */
static void metronome_advance_beat(void) {
    g_metronome_runtime.current_beat++;
    if (g_metronome_runtime.current_beat >= g_metronome_runtime.config.beats_per_measure) {
        g_metronome_runtime.current_beat = 0;
    }
}

/* ============================================================================
 * 公共API实现
 * ============================================================================ */

/**
 * @brief 初始化节拍器
 */
void Metronome_Init(void) {
    memset(&g_metronome_runtime, 0, sizeof(MetronomeRuntime_t));

    g_metronome_runtime.state = METRONOME_OFF;
    g_metronome_runtime.config.bpm = METRONOME_DEFAULT_BPM;
    g_metronome_runtime.config.beats_per_measure = METRONOME_DEFAULT_BEATS_PER_MEASURE;
    g_metronome_runtime.config.downbeat_freq = METRONOME_DEFAULT_DOWNBEAT_FREQ;
    g_metronome_runtime.config.regular_beat_freq = METRONOME_DEFAULT_REGULAR_BEAT_FREQ;
    g_metronome_runtime.config.beat_duration_ms = METRONOME_DEFAULT_BEAT_DURATION;
    g_metronome_runtime.config.volume = METRONOME_DEFAULT_VOLUME;

    // 初始化运行时状态
    g_metronome_runtime.sample_counter = 0;
    g_metronome_runtime.beat_sample_counter = 0;
    g_metronome_runtime.current_beat = 0;
    g_metronome_runtime.is_beat_active = 0;
    g_metronome_runtime.current_beat_type = BEAT_TYPE_DOWNBEAT;
    g_metronome_runtime.sine_phase = 0.0f;

    // 更新计时参数
    metronome_update_timing_params();

    DBG("Metronome initialized: BPM=%d, beats_per_measure=%d\n",
        g_metronome_runtime.config.bpm,
        g_metronome_runtime.config.beats_per_measure);
}

/**
 * @brief 重置节拍器状态
 */
void Metronome_Reset(void) {
    g_metronome_runtime.sample_counter = 0;
    g_metronome_runtime.beat_sample_counter = 0;
    g_metronome_runtime.current_beat = 0;
    g_metronome_runtime.is_beat_active = 0;
    g_metronome_runtime.current_beat_type = BEAT_TYPE_DOWNBEAT;
    g_metronome_runtime.sine_phase = 0.0f;
}

/**
 * @brief 配置节拍器参数
 * @param config 节拍器配置结构体指针
 */
void Metronome_Configure(const MetronomeConfig_t* config) {
    if (config == NULL) return;

    // 验证并设置BPM
    if (config->bpm >= METRONOME_MIN_BPM && config->bpm <= METRONOME_MAX_BPM) {
        g_metronome_runtime.config.bpm = config->bpm;
    }

    // 验证并设置每小节拍数
    if (config->beats_per_measure >= METRONOME_MIN_BEATS_PER_MEASURE &&
        config->beats_per_measure <= METRONOME_MAX_BEATS_PER_MEASURE) {
        g_metronome_runtime.config.beats_per_measure = config->beats_per_measure;
    }

    // 设置频率参数
    g_metronome_runtime.config.downbeat_freq = config->downbeat_freq;
    g_metronome_runtime.config.regular_beat_freq = config->regular_beat_freq;
    g_metronome_runtime.config.beat_duration_ms = config->beat_duration_ms;

    // 验证并设置音量
    if (config->volume >= 0.0f && config->volume <= 1.0f) {
        g_metronome_runtime.config.volume = config->volume;
    }

    // 更新计时参数
    metronome_update_timing_params();

    DBG("Metronome configured: BPM=%d, beats=%d, vol=%.2f\n",
        g_metronome_runtime.config.bpm,
        g_metronome_runtime.config.beats_per_measure,
        g_metronome_runtime.config.volume);
}

/**
 * @brief 切换节拍器开关状态
 */
void Metronome_Toggle(void) {
    if (g_metronome_runtime.state == METRONOME_OFF) {
        Metronome_Enable();
    } else {
        Metronome_Disable();
    }
}

/**
 * @brief 启用节拍器
 */
void Metronome_Enable(void) {
    g_metronome_runtime.state = METRONOME_ON;
    Metronome_Reset();  // 重置状态，从第一拍开始
    DBG("Metronome enabled\n");
}

/**
 * @brief 禁用节拍器
 */
void Metronome_Disable(void) {
    g_metronome_runtime.state = METRONOME_OFF;
    DBG("Metronome disabled\n");
}

/**
 * @brief 检查节拍器是否启用
 * @return 1如果启用，0如果禁用
 */
uint8_t Metronome_IsEnabled(void) {
    return (g_metronome_runtime.state == METRONOME_ON);
}

/**
 * @brief 设置BPM
 * @param bpm 节拍速度（60-200）
 */
void Metronome_SetBPM(uint16_t bpm) {
    if (bpm >= METRONOME_MIN_BPM && bpm <= METRONOME_MAX_BPM) {
        g_metronome_runtime.config.bpm = bpm;
        metronome_update_timing_params();
        DBG("Metronome BPM set to %d\n", bpm);
    }
}

/**
 * @brief 设置每小节拍数
 * @param beats 每小节拍数（2-8）
 */
void Metronome_SetBeatsPerMeasure(uint8_t beats) {
    if (beats >= METRONOME_MIN_BEATS_PER_MEASURE && beats <= METRONOME_MAX_BEATS_PER_MEASURE) {
        g_metronome_runtime.config.beats_per_measure = beats;
        // 重置当前拍子以避免超出范围
        if (g_metronome_runtime.current_beat >= beats) {
            g_metronome_runtime.current_beat = 0;
        }
        DBG("Metronome beats per measure set to %d\n", beats);
    }
}

/**
 * @brief 设置音量
 * @param volume 音量系数（0.0-1.0）
 */
void Metronome_SetVolume(float volume) {
    if (volume >= 0.0f && volume <= 1.0f) {
        g_metronome_runtime.config.volume = volume;
        DBG("Metronome volume set to %.2f\n", volume);
    }
}

/**
 * @brief 设置下拍频率
 * @param freq 下拍频率（Hz）
 */
void Metronome_SetDownbeatFreq(uint16_t freq) {
    g_metronome_runtime.config.downbeat_freq = freq;
    DBG("Metronome downbeat frequency set to %d Hz\n", freq);
}

/**
 * @brief 设置普通拍频率
 * @param freq 普通拍频率（Hz）
 */
void Metronome_SetRegularBeatFreq(uint16_t freq) {
    g_metronome_runtime.config.regular_beat_freq = freq;
    DBG("Metronome regular beat frequency set to %d Hz\n", freq);
}

/**
 * @brief 设置节拍持续时间
 * @param duration_ms 节拍持续时间（毫秒）
 */
void Metronome_SetBeatDuration(uint16_t duration_ms) {
    g_metronome_runtime.config.beat_duration_ms = duration_ms;
    metronome_update_timing_params();
    DBG("Metronome beat duration set to %d ms\n", duration_ms);
}

/**
 * @brief 获取当前BPM
 * @return 当前BPM值
 */
uint16_t Metronome_GetBPM(void) {
    return g_metronome_runtime.config.bpm;
}

/**
 * @brief 获取每小节拍数
 * @return 每小节拍数
 */
uint8_t Metronome_GetBeatsPerMeasure(void) {
    return g_metronome_runtime.config.beats_per_measure;
}

/**
 * @brief 获取当前音量
 * @return 当前音量系数
 */
float Metronome_GetVolume(void) {
    return g_metronome_runtime.config.volume;
}

/**
 * @brief 获取下拍频率
 * @return 下拍频率（Hz）
 */
uint16_t Metronome_GetDownbeatFreq(void) {
    return g_metronome_runtime.config.downbeat_freq;
}

/**
 * @brief 获取普通拍频率
 * @return 普通拍频率（Hz）
 */
uint16_t Metronome_GetRegularBeatFreq(void) {
    return g_metronome_runtime.config.regular_beat_freq;
}

/**
 * @brief 获取节拍持续时间
 * @return 节拍持续时间（毫秒）
 */
uint16_t Metronome_GetBeatDuration(void) {
    return g_metronome_runtime.config.beat_duration_ms;
}

/**
 * @brief 获取当前拍子索引
 * @return 当前拍子索引（0开始）
 */
uint8_t Metronome_GetCurrentBeat(void) {
    return g_metronome_runtime.current_beat;
}

/**
 * @brief 获取当前拍子类型
 * @return 当前拍子类型
 */
BeatType_t Metronome_GetCurrentBeatType(void) {
    return g_metronome_runtime.current_beat_type;
}

/**
 * @brief 检查当前是否在播放节拍声音
 * @return 1如果正在播放节拍声音，0如果不是
 */
uint8_t Metronome_IsBeatActive(void) {
    return g_metronome_runtime.is_beat_active;
}

/**
 * @brief 生成节拍器音频数据（作为效果图源节点使用）
 *        【重要】这里只生成节拍器自己的数据，不做混音！
 *        混音由Mixer节点完成，使用简单的uint32_t累加
 * @param output_data 输出音频数据缓冲区（uint32_t格式，包含左右声道）
 * @param max_len 最大输出长度（样本数）
 * @return 实际生成的样本数
 */
uint16_t Metronome_GenerateAudio(uint32_t* output_data, uint16_t max_len) {
    if (output_data == NULL || max_len == 0) {
        return 0;
    }
    
    /* 如果节拍器未启用，返回静音数据 */
    if (!Metronome_IsEnabled()) {
        uint16_t i;
        for (i = 0; i < max_len; i++) {
            output_data[i] = 0;
        }
        return max_len;
    }

    uint16_t i;
    uint16_t generated_samples = 0;

    for (i = 0; i < max_len; i++) {
        // 检查是否需要开始新的节拍
        if (g_metronome_runtime.sample_counter >= g_metronome_runtime.beat_interval_samples) {
            // 开始新的节拍
            g_metronome_runtime.is_beat_active = 1;
            g_metronome_runtime.beat_sample_counter = 0;
            g_metronome_runtime.sine_phase = 0.0f;

            // 确定拍子类型
            if (g_metronome_runtime.current_beat == 0) {
                g_metronome_runtime.current_beat_type = BEAT_TYPE_DOWNBEAT;
            } else {
                g_metronome_runtime.current_beat_type = BEAT_TYPE_REGULAR;
            }

            // 重置采样计数器
            g_metronome_runtime.sample_counter = 0;

            // 推进到下一拍
            metronome_advance_beat();
        }

        // 生成节拍声音样本
        int16_t sample = 0;
        if (g_metronome_runtime.is_beat_active) {
            // 选择频率
            float freq = (g_metronome_runtime.current_beat_type == BEAT_TYPE_DOWNBEAT) ?
                         g_metronome_runtime.config.downbeat_freq :
                         g_metronome_runtime.config.regular_beat_freq;

            // 生成正弦波样本
            float sine_sample = metronome_generate_sine_sample(freq, &g_metronome_runtime.sine_phase);

            // 应用音量和转换为16位整数
            sample = (int16_t)(sine_sample * g_metronome_runtime.config.volume * 32767.0f);

            // 更新节拍样本计数器
            g_metronome_runtime.beat_sample_counter++;

            // 检查节拍是否结束
            if (g_metronome_runtime.beat_sample_counter >= g_metronome_runtime.beat_duration_samples) {
                g_metronome_runtime.is_beat_active = 0;
            }
        }

        // 【关键修改】直接输出节拍器数据，不混音！
        // 使用uint32_t立体声格式：高16位=右声道，低16位=左声道
        // 节拍器是单声道，复制到左右声道
        output_data[i] = ((uint32_t)(uint16_t)sample << 16) | ((uint32_t)(uint16_t)sample);

        // 推进总采样计数器
        g_metronome_runtime.sample_counter++;
        generated_samples++;
    }

    return generated_samples;
}

/**
 * @brief 处理节拍器音频输出（主要功能）
 * @param output_data 输出音频数据缓冲区（uint32_t格式，包含左右声道）
 * @param length 音频数据长度（样本数，不是字节数）
 */
void Metronome_ProcessAudio(uint32_t* output_data, uint16_t length) {
    Metronome_GenerateAudio(output_data, length);
}

/**
 * @brief 将节拍器音频混合到输出（替代接口）
 * @param output_data 输出音频数据缓冲区
 * @param length 音频数据长度
 */
void Metronome_MixAudio(uint32_t* output_data, uint16_t length) {
    Metronome_ProcessAudio(output_data, length);
}

/**
 * @brief 获取可用数据量（节拍器始终有数据可用）
 * @return 可用样本数
 */
uint16_t Metronome_GetAvailableData(void) {
    // 节拍器作为源节点，始终可以生成数据
    return 48;  // 返回一个合理的帧大小
}

/**
 * @brief 获取运行时状态指针（供外部访问）
 * @return 运行时状态结构体指针
 */
MetronomeRuntime_t* Metronome_GetRuntime(void) {
    return &g_metronome_runtime;
}

/* ============================================================================
 * 全局节拍器模块接口实例
 * ============================================================================ */
Metronome_Interface_t MetronomeModule = {
    .Init = Metronome_Init,
    .Reset = Metronome_Reset,
    .Configure = Metronome_Configure,
    .Toggle = Metronome_Toggle,
    .Enable = Metronome_Enable,
    .Disable = Metronome_Disable,
    .IsEnabled = Metronome_IsEnabled,

    .SetBPM = Metronome_SetBPM,
    .SetBeatsPerMeasure = Metronome_SetBeatsPerMeasure,
    .SetVolume = Metronome_SetVolume,
    .SetDownbeatFreq = Metronome_SetDownbeatFreq,
    .SetRegularBeatFreq = Metronome_SetRegularBeatFreq,
    .SetBeatDuration = Metronome_SetBeatDuration,

    .GetBPM = Metronome_GetBPM,
    .GetBeatsPerMeasure = Metronome_GetBeatsPerMeasure,
    .GetVolume = Metronome_GetVolume,
    .GetDownbeatFreq = Metronome_GetDownbeatFreq,
    .GetRegularBeatFreq = Metronome_GetRegularBeatFreq,
    .GetBeatDuration = Metronome_GetBeatDuration,

    .GetCurrentBeat = Metronome_GetCurrentBeat,
    .GetCurrentBeatType = Metronome_GetCurrentBeatType,
    .IsBeatActive = Metronome_IsBeatActive,

    .GenerateAudio = Metronome_GenerateAudio,
    .GetAvailableData = Metronome_GetAvailableData
};
