/**
 * @file    bangtsynth_node.h
 * @brief   BanGTsynth 合成器 Effect Graph 源节点桥接层
 * @author  BanGO
 * @date    2026-02-27
 *
 * 功能:
 *   将 BanGTsynth 合成器封装为 Effect Graph 的 SOURCE 节点，
 *   作为音频数据产生源接入音频处理图。
 *
 * 依赖:
 *   - effect_graph.h (源节点回调类型)
 *   - midi_soundbank_bridge.h (合成器核心接口)
 *
 * 宏控制:
 *   BANGTSYNTH_EN - 在 product_def.h 中定义以启用此模块
 */

#ifndef __BANGTSYNTH_NODE_H__
#define __BANGTSYNTH_NODE_H__

#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include <stdint.h>
#include "effect_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化合成器节点
 *
 * 初始化 MIDI 控制器、音源管理器、音频处理流水线
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
 * 遍历所有活动 MIDI 通道，读取音源采样数据，
 * 混合复音后写入 out_buf（uint32_t 立体声格式）
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
 * 合成器始终可以产生数据（软件合成），返回固定帧长
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

#ifdef __cplusplus
}
#endif

#endif /* BANGTSYNTH_EN */

#endif /* __BANGTSYNTH_NODE_H__ */
