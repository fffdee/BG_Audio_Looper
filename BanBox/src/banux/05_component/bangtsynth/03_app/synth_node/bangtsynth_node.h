/**
 * @file    bangtsynth_node.h
 * @brief   BanGTsynth 合成器 Effect Graph 源节点桥接层
 * @author  BanGO
 * @date    2026-03-01
 *
 * 功能:
 *   将 BanGTsynth 合成器封装为 Effect Graph 的 SOURCE 节点，
 *   作为音频数据产生源接入音频处理图。
 *
 * 架构要点 (v4 - FreeRTOS 消息队列):
 *   NDS32 BP10 跨任务内存可见性不可靠 (volatile/DSB/sync均无效),
 *   因此使用 FreeRTOS xQueue 作为唯一跨任务通信机制:
 *   - Shell/BLE 任务: TriggerNoteOn/Off → xQueueSend
 *   - 主任务回调: SourceCallback → xQueueReceive → NoteOn/Off + ReadSamples
 *   - g_voices[] 只在主任务中访问, 完全消除跨任务共享内存问题
 *
 * 复音: 最大 8 复音 (SYNTH_MAX_VOICES)
 *
 * 依赖:
 *   - effect_graph.h (源节点回调类型)
 *   - soundbank_manager.h (NoteOn/Off/ReadActiveSamples)
 *   - FreeRTOS queue.h (xQueue)
 *
 * 宏控制:
 *   BANGTSYNTH_EN - 在 product_def.h 中定义以启用此模块
 */

#ifndef __BANGTSYNTH_NODE_H__
#define __BANGTSYNTH_NODE_H__

#include "product_def.h"

#if BANGTSYNTH_EN

#include <stdint.h>
#include "effect_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化合成器节点
 *
 * 初始化 MIDI 控制器、音源管理器
 * 需要在 Effect Graph 创建之后、Audio_Init 过程中调用
 *
 * @return 0=成功, -1=失败
 */
int BanGTsynth_Node_Init(void);

/**
 * @brief 反初始化合成器节点
 */
void BanGTsynth_Node_DeInit(void);

/**
 * @brief 合成器源节点回调 - 供 Effect Graph 调用
 *
 * 通过 soundbank_manager.ReadActiveSamples() 跨编译单元调用
 * sf2_parser 内部 g_voices[], 读取所有活跃声部的混合音频。
 * 编译器无法跨编译单元缓存 g_voices 状态, 保证可见性。
 *
 * @param node     Effect Graph 节点指针
 * @param out_buf  输出缓冲区 (uint32_t: 高16位=R, 低16位=L)
 * @param max_len  最大样本数
 * @return 实际产生的样本数
 */
uint16_t BanGTsynth_SourceCallback(EffectNode_t *node, uint32_t *out_buf, uint16_t max_len);

/**
 * @brief 合成器可用数据量查询回调
 *
 * @param node  Effect Graph 节点指针
 * @return 可用样本数
 */
uint16_t BanGTsynth_GetAvailCallback(EffectNode_t *node);

/**
 * @brief 发送 MIDI 消息到合成器
 *
 * @param data  MIDI 消息字节数组
 * @param len   消息长度
 */
void BanGTsynth_SendMIDI(uint8_t *data, uint8_t len);

/**
 * @brief 检查合成器是否已初始化
 * @return 1=已初始化, 0=未初始化
 */
uint8_t BanGTsynth_IsInitialized(void);

/**
 * @brief 触发 NoteOn (任务安全, 可从任何任务调用)
 *
 * 通过 FreeRTOS 消息队列将 NoteOn 命令传递到主任务回调执行,
 * 避免跨任务直接写入 g_voices[] 的内存可见性问题。
 *
 * @param note     MIDI 音符号 0-127
 * @param velocity 力度 1-127
 * @param program  音色号 0-127
 * @param channel  MIDI 通道 0-15
 * @return 0=成功, -1=队列满或未初始化
 */
int BanGTsynth_TriggerNoteOn(uint8_t note, uint8_t velocity, uint8_t program, uint8_t channel);

/**
 * @brief 触发 NoteOff (任务安全)
 *
 * 直接调用 soundbank_manager.NoteOff()
 *
 * @param note     MIDI 音符号 0-127
 * @param program  音色号 0-127
 * @param channel  MIDI 通道 0-15
 * @return 0=成功, -1=未找到匹配音符
 */
int BanGTsynth_TriggerNoteOff(uint8_t note, uint8_t program, uint8_t channel);

/**
 * @brief 查询当前活跃复音数
 * @return 活跃复音数 (0 ~ SYNTH_MAX_VOICES)
 */
uint8_t BanGTsynth_GetActiveVoiceCount(void);

/**
 * @brief 启动测试音 (500Hz方波, 通过效果图路径输出)
 * 
 * 用于独立验证效果图路径是否正常:
 * synth_in → USB_BT_Mixer → USB_BT_EQ → Final_Mixer → DRC → DAC0_Out
 * 完全绕过 soundbank/ReadActiveSamples，直接生成波形
 *
 * @param duration_ms 持续时长(毫秒)
 */
void BanGTsynth_StartTestTone(uint32_t duration_ms);

/**
 * @brief 停止测试音
 */
void BanGTsynth_StopTestTone(void);

/**
 * @brief 设置直接 DAC 模式 (防止跨任务竞争)
 *
 * sb -t / sb -p 在 Shell 任务中直接操作 g_voices[] 和 ReadSamples,
 * 同时 SourceCallback 也会通过 ReadActiveSamples 访问 g_voices[]。
 * 设置 DirectMode=1 后，SourceCallback 跳过 ReadActiveSamples，
 * 避免两个任务并发修改声部池导致状态损坏。
 *
 * @param enable 1=进入直接模式, 0=恢复正常图路径
 */
void BanGTsynth_SetDirectMode(uint8_t enable);

/**
 * @brief 启动鼓机音序器 (非阻塞, tick驱动)
 *
 * Shell 命令只调用此函数设置参数并启动, 立即返回。
 * 音序器在 SourceCallback 中被 tick 驱动, 
 * 根据 xTaskGetTickCount() 自动触发 NoteOn/Off。
 *
 * @param bpm     速度 (40-240)
 * @param bars    小节数 (1-32)
 * @param program 音色号 (0-127)
 * @return 0=成功, -1=未初始化
 */
int BanGTsynth_DrumSeq_Start(uint32_t bpm, uint32_t bars, uint8_t program);

/**
 * @brief 停止鼓机音序器
 *
 * 设置停止标志, 残留音符在下次 SourceCallback 中自动清理。
 */
void BanGTsynth_DrumSeq_Stop(void);

/**
 * @brief 查询鼓机音序器是否正在运行
 * @return 1=运行中, 0=已停止
 */
uint8_t BanGTsynth_DrumSeq_IsRunning(void);

/**
 * @brief 调度定时 NoteOff (非阻塞, 用于 sb -m)
 *
 * 在 SourceCallback 中检查 tick, 到时间后自动执行 NoteOff。
 * 替代 vTaskDelay + TriggerNoteOff 的阻塞方式。
 *
 * @param note      MIDI 音符号
 * @param program   音色号
 * @param delay_ms  延迟时间(毫秒)
 * @return 0=成功, -1=无空闲槽位
 */
int BanGTsynth_ScheduleNoteOff(uint8_t note, uint8_t program, uint32_t delay_ms);

/**
 * @brief 设置合成器音量
 *
 * @param volume 音量 (0-100, 100=原始音量, 0=静音)
 */
void BanGTsynth_SetVolume(uint8_t volume);

/**
 * @brief 获取当前合成器音量
 * @return 音量 (0-100)
 */
uint8_t BanGTsynth_GetVolume(void);

#ifdef __cplusplus
}
#endif

#endif /* BANGTSYNTH_EN */

#endif /* __BANGTSYNTH_NODE_H__ */
