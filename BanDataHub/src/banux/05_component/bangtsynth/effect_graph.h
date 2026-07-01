/**
 * @file effect_graph.h
 * @brief BanDataHub 平台 Effect Graph 存根
 * 
 * BanDataHub 不使用 Effect Graph (EFFECT_GRAPHICS_EN=0),
 * 但 BanGTsynth 的 bangtsynth_node.h 引用了 EffectNode_t 类型。
 * 此文件提供最小存根以满足编译需求。
 */

#ifndef __EFFECT_GRAPH_H__
#define __EFFECT_GRAPH_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Effect Graph 节点类型存根 */
typedef struct EffectNode {
    int dummy;  /* 占位 */
} EffectNode_t;

#ifdef __cplusplus
}
#endif

#endif /* __EFFECT_GRAPH_H__ */
