/**
 * Chain Graph Application Module
 *
 * This module provides functionality to apply chain-managed graph configurations
 * to the actual running EffectGraph instance.
 */

#ifndef CHAIN_GRAPH_APPLY_H
#define CHAIN_GRAPH_APPLY_H

#include "effect_graph.h"
#include "effect_graph_config.h"
#include "sys_param.h"

/**
 * @brief Apply a chain graph configuration to the running EffectGraph instance
 * @param graph_idx Index of the graph in the audio chain
 * @return 0 on success, -1 on failure
 */
int ChainGraph_ApplyToEffectGraph(int graph_idx);

/**
 * @brief Apply the currently active graph for headphones
 * @return 0 on success, -1 on failure
 */
int ChainGraph_ApplyActiveHP(void);

/**
 * @brief Apply the currently active graph for speakers
 * @return 0 on success, -1 on failure
 */
int ChainGraph_ApplyActiveSPK(void);

/**
 * @brief Apply a named graph to the running EffectGraph
 * @param name Graph name
 * @return 0 on success, -1 on failure
 */
int ChainGraph_ApplyByName(const char *name);

/**
 * @brief Auto-apply saved chain graphs on system startup
 * If saved graphs exist, apply the active ones; otherwise keep default preset
 * @return 0 on success, -1 on failure
 */
int ChainGraph_AutoApplyOnStartup(void);

/**
 * @brief Save the current EffectGraph state to a chain graph
 * @param graph_idx Index of the chain graph to save to
 * @return 0 on success, -1 on failure
 */
int ChainGraph_SaveFromEffectGraph(int graph_idx);

/**
 * @brief Save the current EffectGraph state to a named chain graph
 * @param name Name of the chain graph to save to (creates if doesn't exist)
 * @return 0 on success, -1 on failure
 */
int ChainGraph_SaveByName(const char *name);

/**
 * @brief Sync EffectGraph node parameters to global audio effect units
 * This ensures that loaded parameters are applied to the actual audio processing units
 * @return 0 on success, -1 on failure
 */
int ChainGraph_SyncParamsToGlobalUnits(void);

#endif /* CHAIN_GRAPH_APPLY_H */