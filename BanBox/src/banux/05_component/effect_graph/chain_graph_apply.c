/**
 * Chain Graph Application Module Implementation
 */

#include "chain_graph_apply.h"
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "sys_param.h"
#include "ctrlvars.h"
#include <string.h>
#include <stdio.h>
#include "debug.h"

#if EFFECT_GRAPHICS_EN

/**
 * @brief Allocate a free node ID from the node pool
 * @return node ID on success, -1 on failure
 */
static int alloc_node_id(void)
{
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    int i;

    if (!ac) return -1;

    // Find first free node ID
    for (i = 0; i < MAX_GRAPH_NODES; i++) {
        if ((ac->node_used_mask & (1 << i)) == 0) {
            ac->node_used_mask |= (1 << i);
            return i;
        }
    }

    return -1; // No free nodes
}

/**
 * @brief Free a node ID back to the pool
 * @param node_id Node ID to free
 */
static void free_node_id(int node_id)
{
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();

    if (ac && node_id >= 0 && node_id < MAX_GRAPH_NODES) {
        ac->node_used_mask &= ~(1 << node_id);
    }
}

/**
 * @brief Apply a chain graph configuration to the running EffectGraph instance
 * @param graph_idx Index of the graph in the audio chain
 * @return 0 on success, -1 on failure
 */
int ChainGraph_ApplyToEffectGraph(int graph_idx)
{
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    EffectGraph_t *chain_graph;
    GraphConfig_t config;
    NodeConfig_t nodes[MAX_GRAPH_NODES];
    EdgeConfig_t edges[MAX_GRAPH_EDGES];
    int i, node_idx;
    uint8_t node_id;

    if (!ac || graph_idx < 0 || graph_idx >= ac->graph_count) {
        DBG("[ChainGraph] Invalid graph index: %d\n", graph_idx);
        return -1;
    }

    chain_graph = &ac->graphs[graph_idx];
    DBG("[ChainGraph] Applying graph '%s' (idx=%d) to EffectGraph\n",
        chain_graph->name, graph_idx);
    DBG("[ChainGraph] Graph has %d nodes, %d edges\n",
        chain_graph->node_count, chain_graph->edge_count);

    // Build GraphConfig_t from chain graph
    memset(&config, 0, sizeof(GraphConfig_t));
    config.sample_rate = 48000;  // Default sample rate
    config.node_count = chain_graph->node_count;
    config.edge_count = chain_graph->edge_count;
    config.nodes = nodes;
    config.edges = edges;

    // Convert nodes from node pool
    for (i = 0; i < chain_graph->node_count && i < MAX_GRAPH_NODES; i++) {
        node_id = chain_graph->node_ids[i];
        if (node_id >= MAX_GRAPH_NODES) {
            DBG("[ChainGraph] Invalid node ID: %d\n", node_id);
            continue;
        }

        GraphNode_t *chain_node = &ac->node_pool[node_id];
        NodeConfig_t *node_config = &nodes[i];

        // Set node ID
        node_config->node_id = node_id;

        // Convert node type and subtype to EffectGraph types
        switch (chain_node->node_type) {
            case NODE_TYPE_SOURCE:
                // Convert source types
                switch (chain_node->subtype) {
                    case SOURCE_TYPE_GUITAR:
                        node_config->type = EFFECT_NODE_TYPE_SOURCE_ADC0;
                        node_config->name = "guitar_in";
                        break;
                    case SOURCE_TYPE_MIC:
                        node_config->type = EFFECT_NODE_TYPE_SOURCE_ADC1;
                        node_config->name = "mic_in";
                        break;
                    case SOURCE_TYPE_BT:
                        node_config->type = EFFECT_NODE_TYPE_SOURCE_BT_IN;
                        node_config->name = "bt_in";
                        break;
                    case SOURCE_TYPE_USB:
                        node_config->type = EFFECT_NODE_TYPE_SOURCE_USB_IN;
                        node_config->name = "usb_in";
                        break;
                    case SOURCE_TYPE_METRONOME:
                        node_config->type = EFFECT_NODE_TYPE_SOURCE_METRONOME;
                        node_config->name = "metronome";
                        break;
                    case SOURCE_TYPE_LOOPER_PLAY:
                        node_config->type = EFFECT_NODE_TYPE_SOURCE_LOOPER_PLAY;
                        node_config->name = "looper_play";
                        break;
#if BANGTSYNTH_EN
                    case SOURCE_TYPE_SYNTH:
                        node_config->type = EFFECT_NODE_TYPE_SOURCE_SYNTH;
                        node_config->name = "synth_in";
                        break;
#endif
                    default:
                        node_config->type = EFFECT_NODE_TYPE_SOURCE_ADC0;
                        node_config->name = "source";
                        break;
                }
                break;

            case NODE_TYPE_EFFECT:
                // Convert effect types
                switch (chain_node->subtype) {
                    case EFFECT_TYPE_EQ:
                        node_config->type = EFFECT_NODE_TYPE_EFFECT_EQ;
                        node_config->name = "eq";
                        break;
                    case EFFECT_TYPE_COMPRESSOR:
                        node_config->type = EFFECT_NODE_TYPE_EFFECT_DRC;
                        node_config->name = "drc";
                        break;
                    case EFFECT_TYPE_REVERB:
                        node_config->type = EFFECT_NODE_TYPE_EFFECT_REVERB;
                        node_config->name = "reverb";
                        break;
                    case EFFECT_TYPE_DELAY:
                        node_config->type = EFFECT_NODE_TYPE_EFFECT_DELAY;
                        node_config->name = "delay";
                        break;
                    case EFFECT_TYPE_EXPANDER:
                        node_config->type = EFFECT_NODE_TYPE_EFFECT_EXPANDER;
                        node_config->name = "expander";
                        break;
                    case EFFECT_TYPE_HOWLING:
                        node_config->type = EFFECT_NODE_TYPE_EFFECT_HOWLING;
                        node_config->name = "howling";
                        break;
                    case EFFECT_TYPE_GAIN:
                        node_config->type = EFFECT_NODE_TYPE_EFFECT_GAIN;
                        node_config->name = "gain";
                        break;
                    case EFFECT_TYPE_CHORUS:
                        node_config->type = EFFECT_NODE_TYPE_EFFECT_CHORUS;
                        node_config->name = "chorus";
                        break;
                    default:
                        node_config->type = EFFECT_NODE_TYPE_EFFECT_GAIN;
                        node_config->name = "effect";
                        break;
                }
                break;

            case NODE_TYPE_MIXER:
                node_config->type = EFFECT_NODE_TYPE_MIXER;
                node_config->name = "mixer";
                break;

            case NODE_TYPE_OUTPUT:
                // Convert output types
                switch (chain_node->subtype) {
                    case OUTPUT_TYPE_HEADPHONE:
                    case OUTPUT_TYPE_SPEAKER:
                        node_config->type = EFFECT_NODE_TYPE_SINK_DAC0;
                        node_config->name = "dac_out";
                        break;
                    case OUTPUT_TYPE_USB_OUT:
                        node_config->type = EFFECT_NODE_TYPE_SINK_USB_OUT;
                        node_config->name = "usb_out";
                        break;
                    case OUTPUT_TYPE_LOOPER_RECORD:
                        node_config->type = EFFECT_NODE_TYPE_SINK_LOOPER_RECORD;
                        node_config->name = "looper_record";
                        break;
                    default:
                        node_config->type = EFFECT_NODE_TYPE_SINK_USB_OUT;
                        node_config->name = "usb_out";
                        break;
                }
                break;

            default:
                DBG("[ChainGraph] Unknown node type: %d\n", chain_node->node_type);
                continue;
        }

        // Set enabled state
        node_config->enabled = chain_node->enabled;

        // Copy parameters from node pool
        if (chain_node->node_type == NODE_TYPE_EFFECT && chain_node->subtype == EFFECT_TYPE_EQ) {
            // Special handling for EQ parameters - load from params array
            // Layout: [band0...band9][pregain][filter_count][channel]
            // Each band: enable(1) type(1) f0(2) Q(2) gain(1) = 7 bytes, 10 bands = 70 bytes
            int param_idx = 0;
            int band;
            
            // Load bands: enable, type, f0, Q, gain for each band
            for (band = 0; band < 10; band++) {
                node_config->params.eq.band_enables[band] = (uint8_t)chain_node->params[param_idx++];
                node_config->params.eq.band_types[band] = (uint8_t)chain_node->params[param_idx++];
                
                // Load f0 (2 bytes, little endian)
                uint32_t f0 = chain_node->params[param_idx++];
                f0 |= (uint32_t)chain_node->params[param_idx++] << 8;
                node_config->params.eq.band_f0[band] = f0;
                
                // Load Q (2 bytes, little endian)
                uint32_t q = chain_node->params[param_idx++];
                q |= (uint32_t)chain_node->params[param_idx++] << 8;
                node_config->params.eq.band_Q[band] = q;
                
                // Load gain (1 byte, signed)
                node_config->params.eq.band_gains[band] = (int8_t)chain_node->params[param_idx++];
            }
            
            // Load pregain (1 byte)
            node_config->params.eq.pregain = (int16_t)(int8_t)chain_node->params[param_idx++];
            
            // Load filter_count (1 byte)
            node_config->params.eq.band_count = (uint8_t)chain_node->params[param_idx++];
            
            // Skip channel (1 byte) - not used in EffectParams_t
            param_idx++;
            
        } else {
            // For other node types, use memcpy
            memcpy(&node_config->params, &chain_node->params, sizeof(EffectParams_t));
        }
    }

    // Copy edges (connections)
    for (i = 0; i < chain_graph->edge_count && i < MAX_GRAPH_EDGES; i++) {
        GraphEdge_t *chain_edge = &chain_graph->edges[i];
        EdgeConfig_t *edge_config = &edges[i];

        edge_config->src_node_id = chain_edge->from_node;
        edge_config->dst_node_id = chain_edge->to_node;
        edge_config->src_port = 0;  // Default port
        edge_config->dst_port = 0;    // Default port
    }

    // Apply to running EffectGraph
    GraphError_t err = EffectGraph_CreateFromConfig(&config);
    if (err != GRAPH_OK) {
        DBG("[ChainGraph] Failed to apply graph config (error=%d)\n", err);
        DBG("[ChainGraph] Config: %d nodes, %d edges\n", config.node_count, config.edge_count);
        return -1;
    }

    // Rebuild the graph topology
    err = EffectGraph_Build();
    if (err != GRAPH_OK) {
        DBG("[ChainGraph] Failed to rebuild graph (error=%d)\n", err);
        return -1;
    }

    // Sync parameters to global audio effect units
    if (ChainGraph_SyncParamsToGlobalUnits() != 0) {
        DBG("[ChainGraph] Failed to sync parameters to global units\n");
        return -1;
    }

    DBG("[ChainGraph] Successfully applied graph '%s'\n", chain_graph->name);
    return 0;
}

/**
 * @brief Apply the currently active graph for headphones
 * @return 0 on success, -1 on failure
 */
int ChainGraph_ApplyActiveHP(void)
{
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    return ChainGraph_ApplyToEffectGraph(ac->active_graph_hp);
}

/**
 * @brief Apply the currently active graph for speakers
 * @return 0 on success, -1 on failure
 */
int ChainGraph_ApplyActiveSPK(void)
{
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    return ChainGraph_ApplyToEffectGraph(ac->active_graph_spk);
}

/**
 * @brief Apply a named graph to the running EffectGraph
 * @param name Graph name
 * @return 0 on success, -1 on failure
 */
int ChainGraph_ApplyByName(const char *name)
{
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    int i;

    for (i = 0; i < ac->graph_count; i++) {
        if (strcmp(ac->graphs[i].name, name) == 0) {
            return ChainGraph_ApplyToEffectGraph(i);
        }
    }

    DBG("[ChainGraph] Graph '%s' not found\n", name);
    return -1;
}

/**
 * @brief Auto-apply saved chain graphs on system startup
 * If saved graphs exist, apply the active ones; otherwise keep default preset
 * @return 0 on success, -1 on failure
 */
int ChainGraph_AutoApplyOnStartup(void)
{
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    int result = -1;

    // Check if we have saved chain graphs and validate data
    if (ac && ac->graph_count > 0 && ac->graph_count <= MAX_EFFECT_GRAPHS) {
        int valid_graphs = 0;
        int i;

        // Validate each graph
        for (i = 0; i < ac->graph_count; i++) {
            EffectGraph_t *graph = &ac->graphs[i];
            if (graph->node_count <= MAX_GRAPH_NODES &&
                graph->edge_count <= MAX_GRAPH_EDGES) {
                valid_graphs++;
            }
        }

        if (valid_graphs == 0) {
            DBG("[ChainGraph] No valid saved graphs found, keeping default preset\n");
            return 0;
        }

        DBG("[ChainGraph] Found %d valid saved graphs (out of %d), auto-applying active graphs...\n",
            valid_graphs, ac->graph_count);

        // Try to apply HP active graph first
        if (ac->active_graph_hp >= 0 && ac->active_graph_hp < ac->graph_count) {
            EffectGraph_t *hp_graph = &ac->graphs[ac->active_graph_hp];
            if (hp_graph->node_count <= MAX_GRAPH_NODES &&
                hp_graph->edge_count <= MAX_GRAPH_EDGES) {
                DBG("[ChainGraph] Applying active HP graph (idx=%d)\n", ac->active_graph_hp);
                result = ChainGraph_ApplyToEffectGraph(ac->active_graph_hp);
                if (result == 0) {
                    DBG("[ChainGraph] Successfully applied saved HP graph\n");
                    return 0;
                } else {
                    DBG("[ChainGraph] Failed to apply HP graph, trying SPK graph\n");
                }
            }
        }

        // If HP failed or not set, try SPK active graph
        if (ac->active_graph_spk >= 0 && ac->active_graph_spk < ac->graph_count) {
            EffectGraph_t *spk_graph = &ac->graphs[ac->active_graph_spk];
            if (spk_graph->node_count <= MAX_GRAPH_NODES &&
                spk_graph->edge_count <= MAX_GRAPH_EDGES) {
                DBG("[ChainGraph] Applying active SPK graph (idx=%d)\n", ac->active_graph_spk);
                result = ChainGraph_ApplyToEffectGraph(ac->active_graph_spk);
                if (result == 0) {
                    DBG("[ChainGraph] Successfully applied saved SPK graph\n");
                    return 0;
                } else {
                    DBG("[ChainGraph] Failed to apply SPK graph\n");
                }
            }
        }

        // If both failed, fall back to first valid graph
        for (i = 0; i < ac->graph_count; i++) {
            EffectGraph_t *graph = &ac->graphs[i];
            if (graph->node_count <= MAX_GRAPH_NODES &&
                graph->edge_count <= MAX_GRAPH_EDGES) {
                DBG("[ChainGraph] Applying first valid graph (idx=%d)\n", i);
                result = ChainGraph_ApplyToEffectGraph(i);
                if (result == 0) {
                    DBG("[ChainGraph] Successfully applied first valid graph\n");
                    return 0;
                }
            }
        }

        if (result != 0) {
            DBG("[ChainGraph] All saved graphs failed to apply, keeping default preset\n");
        }
    } else {
        DBG("[ChainGraph] No saved graphs found or invalid data, keeping default preset\n");
        result = 0;  // Not an error, just no saved graphs
    }

    return result;
}

/**
 * @brief Save the current EffectGraph state to a chain graph
 * @param graph_idx Index of the chain graph to save to
 * @return 0 on success, -1 on failure
 */
int ChainGraph_SaveFromEffectGraph(int graph_idx)
{
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    EffectGraphRuntime_t *effect_graph;
    EffectGraph_t *chain_graph;
    int i, j;

    if (!ac || graph_idx < 0 || graph_idx >= MAX_EFFECT_GRAPHS) {
        DBG("[ChainGraph] Invalid graph index: %d\n", graph_idx);
        return -1;
    }

    effect_graph = EffectGraph_GetInstance();
    if (!effect_graph) {
        DBG("[ChainGraph] EffectGraph not initialized\n");
        return -1;
    }

    chain_graph = &ac->graphs[graph_idx];

    // Free existing node IDs before clearing the graph
    for (i = 0; i < chain_graph->node_count && i < MAX_GRAPH_NODES; i++) {
        uint8_t node_id = chain_graph->node_ids[i];
        if (node_id < MAX_GRAPH_NODES) {
            free_node_id(node_id);
        }
    }

    // Clear existing graph
    memset(chain_graph, 0, sizeof(EffectGraph_t));
    memset(chain_graph->node_ids, 0xFF, MAX_GRAPH_NODES);

    // Set basic info
    snprintf(chain_graph->name, sizeof(chain_graph->name), "Saved_%d", graph_idx);
    chain_graph->node_count = effect_graph->node_count;

    DBG("[ChainGraph] Saving EffectGraph to chain graph %d (%d nodes)\n",
        graph_idx, effect_graph->node_count);

    // Save nodes
    for (i = 0; i < effect_graph->node_count && i < MAX_GRAPH_NODES; i++) {
        EffectNode_t *effect_node = &effect_graph->nodes[i];
        int node_id = alloc_node_id();

        if (node_id < 0) {
            DBG("[ChainGraph] No free node IDs available\n");
            return -1;
        }

        GraphNode_t *chain_node = &ac->node_pool[node_id];

        // Initialize chain node
        memset(chain_node, 0, sizeof(GraphNode_t));

        DBG("[ChainGraph] Saving node %d (type=%d, id=%d) to chain node %d\n",
            i, effect_node->type, effect_node->id, node_id);

        // Convert EffectGraph node type to chain node type
        switch (effect_node->type) {
            case EFFECT_NODE_TYPE_SOURCE_ADC0:
                chain_node->node_type = NODE_TYPE_SOURCE;
                chain_node->subtype = SOURCE_TYPE_GUITAR;
                break;
            case EFFECT_NODE_TYPE_SOURCE_ADC1:
                chain_node->node_type = NODE_TYPE_SOURCE;
                chain_node->subtype = SOURCE_TYPE_MIC;
                break;
            case EFFECT_NODE_TYPE_SOURCE_BT_IN:
                chain_node->node_type = NODE_TYPE_SOURCE;
                chain_node->subtype = SOURCE_TYPE_BT;
                break;
            case EFFECT_NODE_TYPE_SOURCE_USB_IN:
                chain_node->node_type = NODE_TYPE_SOURCE;
                chain_node->subtype = SOURCE_TYPE_USB;
                break;
            case EFFECT_NODE_TYPE_EFFECT_EQ:
                chain_node->node_type = NODE_TYPE_EFFECT;
                chain_node->subtype = EFFECT_TYPE_EQ;
                break;
            case EFFECT_NODE_TYPE_EFFECT_DRC:
                chain_node->node_type = NODE_TYPE_EFFECT;
                chain_node->subtype = EFFECT_TYPE_COMPRESSOR;
                break;
            case EFFECT_NODE_TYPE_EFFECT_REVERB:
                chain_node->node_type = NODE_TYPE_EFFECT;
                chain_node->subtype = EFFECT_TYPE_REVERB;
                break;
            case EFFECT_NODE_TYPE_EFFECT_DELAY:
                chain_node->node_type = NODE_TYPE_EFFECT;
                chain_node->subtype = EFFECT_TYPE_DELAY;
                break;
            case EFFECT_NODE_TYPE_MIXER:
                chain_node->node_type = NODE_TYPE_MIXER;
                chain_node->subtype = 0;
                break;
            case EFFECT_NODE_TYPE_SINK_DAC0:
                chain_node->node_type = NODE_TYPE_OUTPUT;
                chain_node->subtype = OUTPUT_TYPE_HEADPHONE;
                break;
            case EFFECT_NODE_TYPE_SINK_USB_OUT:
                chain_node->node_type = NODE_TYPE_OUTPUT;
                chain_node->subtype = 2; // USB output
                break;
            default:
                chain_node->node_type = NODE_TYPE_EFFECT;
                chain_node->subtype = 0;
                break;
        }

        // Copy node properties
        chain_node->enabled = effect_node->enabled;
        chain_node->volume = 100; // Default volume

        // Copy parameters - this is the key part!
        if (effect_node->type == EFFECT_NODE_TYPE_EFFECT_EQ) {
            // Special handling for EQ parameters - save to params array
            // Layout: [band0...band9][pregain][filter_count][channel]
            // Each band: enable(1) type(1) f0(2) Q(2) gain(1) = 7 bytes, 10 bands = 70 bytes
            int param_idx = 0;
            int band;
            
            // Save bands: enable, type, f0, Q, gain for each band
            for (band = 0; band < 10; band++) {
                chain_node->params[param_idx++] = (uint8_t)effect_node->params.eq.band_enables[band];
                chain_node->params[param_idx++] = (uint8_t)effect_node->params.eq.band_types[band];
                
                // Save f0 (2 bytes, little endian)
                uint32_t f0 = effect_node->params.eq.band_f0[band];
                chain_node->params[param_idx++] = (uint8_t)(f0 & 0xFF);
                chain_node->params[param_idx++] = (uint8_t)((f0 >> 8) & 0xFF);
                
                // Save Q (2 bytes, little endian)
                uint32_t q = effect_node->params.eq.band_Q[band];
                chain_node->params[param_idx++] = (uint8_t)(q & 0xFF);
                chain_node->params[param_idx++] = (uint8_t)((q >> 8) & 0xFF);
                
                // Save gain (1 byte, signed)
                chain_node->params[param_idx++] = (uint8_t)effect_node->params.eq.band_gains[band];
            }
            
            // Save pregain (1 byte)
            chain_node->params[param_idx++] = (uint8_t)effect_node->params.eq.pregain;
            
            // Save filter_count (1 byte)
            chain_node->params[param_idx++] = (uint8_t)effect_node->params.eq.band_count;
            
            // Save channel (1 byte) - not used, set to 0
            chain_node->params[param_idx++] = 0;
            
        } else {
            // For other node types, use memcpy
            memcpy(&chain_node->params, &effect_node->params, sizeof(EffectParams_t));
        }

        // Add to graph
        chain_graph->node_ids[i] = node_id;
    }

    // Save edges (connections) - copy from EffectGraph topology
    chain_graph->edge_count = effect_graph->edge_count;
    for (i = 0; i < effect_graph->edge_count && i < MAX_GRAPH_EDGES; i++) {
        EffectEdge_t *effect_edge = &effect_graph->edges[i];
        GraphEdge_t *chain_edge = &chain_graph->edges[i];

        // Convert node pointers to node IDs
        int src_idx = -1, dst_idx = -1;
        int j;

        // Find source node ID in our saved node list
        for (j = 0; j < effect_graph->node_count; j++) {
            if (&effect_graph->nodes[j] == effect_edge->src_node) {
                src_idx = j;
                break;
            }
        }

        // Find destination node ID in our saved node list
        for (j = 0; j < effect_graph->node_count; j++) {
            if (&effect_graph->nodes[j] == effect_edge->dst_node) {
                dst_idx = j;
                break;
            }
        }

        if (src_idx >= 0 && dst_idx >= 0) {
            chain_edge->from_node = chain_graph->node_ids[src_idx];
            chain_edge->to_node = chain_graph->node_ids[dst_idx];
        } else {
            DBG("[ChainGraph] Failed to find node IDs for edge %d\n", i);
            // Skip this edge
            chain_graph->edge_count--;
            i--;
        }
    }

    // Update graph count if this is a new graph
    if (graph_idx >= ac->graph_count) {
        ac->graph_count = graph_idx + 1;
    }

    // Mark parameters as modified
    SysParam_MarkModified();

    DBG("[ChainGraph] Successfully saved EffectGraph to chain graph %d\n", graph_idx);
    return 0;
}

/**
 * @brief Save the current EffectGraph state to a named chain graph
 * @param name Name of the chain graph to save to (creates if doesn't exist)
 * @return 0 on success, -1 on failure
 */
int ChainGraph_SaveByName(const char *name)
{
    SysParam_AudioChain_t *ac = SYSPARAM_AUDIOCHAIN();
    int i;

    if (!name || !ac) {
        return -1;
    }

    // Find existing graph by name
    for (i = 0; i < ac->graph_count; i++) {
        if (strcmp(ac->graphs[i].name, name) == 0) {
            return ChainGraph_SaveFromEffectGraph(i);
        }
    }

    // Create new graph if not found
    if (ac->graph_count >= MAX_EFFECT_GRAPHS) {
        DBG("[ChainGraph] Maximum graphs reached, cannot create new graph\n");
        return -1;
    }

    // Save to new graph
    int result = ChainGraph_SaveFromEffectGraph(ac->graph_count);
    if (result == 0) {
        // Set the name
        strncpy(ac->graphs[ac->graph_count - 1].name, name, GRAPH_NAME_LEN - 1);
        SysParam_MarkModified();
    }

    return result;
}

/**
 * @brief Sync EffectGraph node parameters to global audio effect units
 * This ensures that loaded parameters are applied to the actual audio processing units
 * @return 0 on success, -1 on failure
 */
int ChainGraph_SyncParamsToGlobalUnits(void)
{
    EffectGraphRuntime_t *effect_graph;
    int i;

    effect_graph = EffectGraph_GetInstance();
    if (!effect_graph) {
        DBG("[ChainGraph] EffectGraph not initialized\n");
        return -1;
    }

    // Declare extern variable at function scope
    extern ControlVariablesContext gCtrlVars;

    // Sync parameters for each node
    for (i = 0; i < effect_graph->node_count; i++) {
        EffectNode_t *node = &effect_graph->nodes[i];

        switch (node->type) {
            case EFFECT_NODE_TYPE_EFFECT_REVERB:
                /* Sync to global reverb unit */
                gCtrlVars.reverb_unit.roomsize_scale = (int32_t)node->params.reverb.room_size;
                gCtrlVars.reverb_unit.damping_scale = (int32_t)node->params.reverb.damping;
                gCtrlVars.reverb_unit.wet_scale = (int32_t)node->params.reverb.wet_dry;
                #if CFG_AUDIO_EFFECT_REVERB_EN
                AudioEffectReverbConfig(&gCtrlVars.reverb_unit);
                #endif
                break;

            case EFFECT_NODE_TYPE_EFFECT_DRC:
                /* Sync to global DRC unit */
                gCtrlVars.mic_drc_unit.threshold[0] = (int32_t)node->params.drc.threshold;
                gCtrlVars.mic_drc_unit.ratio[0] = (int32_t)node->params.drc.ratio;
                gCtrlVars.mic_drc_unit.attack_tc[0] = (int32_t)node->params.drc.attack;
                gCtrlVars.mic_drc_unit.release_tc[0] = (int32_t)node->params.drc.release;
                #if CFG_AUDIO_EFFECT_MIC_DRC_EN
                AudioEffectDRCConfig(&gCtrlVars.mic_drc_unit, 2, 48000);
                #endif
                break;

            case EFFECT_NODE_TYPE_EFFECT_EQ:
                /* Sync to global EQ unit - select based on node ID */
                {
                    EQUnit *target_eq_unit;
                    int band, i, filter_idx;
                    
                    // Select EQ unit based on node ID
                    if (node->id == 7) {
                        // MIC_OUT_EQ -> mic_out_eq_unit
                        target_eq_unit = &gCtrlVars.mic_out_eq_unit;
                    } else if (node->id == 10) {
                        // USB_BT_EQ -> music_out_eq_unit
                        target_eq_unit = &gCtrlVars.music_out_eq_unit;
                    } else {
                        // Default to mic_out_eq_unit
                        target_eq_unit = &gCtrlVars.mic_out_eq_unit;
                    }
                    
                    target_eq_unit->enable = node->enabled;
                    target_eq_unit->pregain = (int32_t)node->params.eq.pregain;
                    
                    // 首先复制所有eq_params
                    for (band = 0; band < node->params.eq.band_count && band < 10; band++) {
                        target_eq_unit->eq_params[band].gain = (int32_t)node->params.eq.band_gains[band] * 256;
                        target_eq_unit->eq_params[band].type = node->params.eq.band_types[band];
                        target_eq_unit->eq_params[band].f0 = node->params.eq.band_f0[band];
                        target_eq_unit->eq_params[band].Q = node->params.eq.band_Q[band];
                        target_eq_unit->eq_params[band].enable = node->params.eq.band_enables[band];
                    }
                    
                    /* 关键步骤1: 重置filter_count为0 */
                    target_eq_unit->filter_count = 0;
                    
                    /* 关键步骤2: 重建filter_params数组（只包含启用的频段） */
                    if (target_eq_unit->filter_params) {
                        for (i = 0; i < 10; i++) {
                            if (target_eq_unit->eq_params[i].enable) {
                                /* 参数有效性检查 - 防止f0=0或Q=0导致除零错误 */
                                uint16_t f0 = target_eq_unit->eq_params[i].f0;
                                int16_t Q = target_eq_unit->eq_params[i].Q;
                                int16_t type = target_eq_unit->eq_params[i].type;
                                
                                /* 检查f0有效性：必须大于0 */
                                if (f0 == 0) {
                                    f0 = 1000;  /* 默认1000Hz */
                                    target_eq_unit->eq_params[i].f0 = f0;
                                    DBG("[EQ] WARN: band%d f0=0 invalid, set to 1000Hz\n", i);
                                }
                                
                                /* 检查Q有效性：Q6.10格式，必须大于0 */
                                if (Q <= 0) {
                                    Q = 724;  /* 默认Q=0.707，Q6.10格式=724 */
                                    target_eq_unit->eq_params[i].Q = Q;
                                    DBG("[EQ] WARN: band%d Q<=0 invalid, set to 724\n", i);
                                }
                                
                                /* 检查type有效性：类型必须在合理范围内 */
                                if (type < 0 || type > 6) {
                                    type = 0;  /* 默认PEAKING */
                                    target_eq_unit->eq_params[i].type = type;
                                    DBG("[EQ] WARN: band%d type invalid, set to PEAKING\n", i);
                                }
                                
                                filter_idx = target_eq_unit->filter_count;
                                target_eq_unit->filter_params[filter_idx].Q    = Q;
                                target_eq_unit->filter_params[filter_idx].f0   = f0;
                                target_eq_unit->filter_params[filter_idx].gain = target_eq_unit->eq_params[i].gain;
                                target_eq_unit->filter_params[filter_idx].type = type;
                                target_eq_unit->filter_count++;
                            }
                        }
                    }
                    
                    DBG("[ChainGraph] EQ node %d: %d enabled bands\n", node->id, target_eq_unit->filter_count);
                    
                    // Configure the appropriate EQ unit (使用ClearBuf版本)
                    if (target_eq_unit->filter_count > 0 && target_eq_unit->ct != NULL) {
                        if (node->id == 7) {
                            #if CFG_AUDIO_EFFECT_MIC_OUT_EQ_EN
                            AudioEffectEQFilterClearBufConfig(target_eq_unit, 48000);
                            #endif
                        } else if (node->id == 10) {
                            #if CFG_AUDIO_EFFECT_MUSIC_OUT_EQ_EN
                            AudioEffectEQFilterClearBufConfig(target_eq_unit, 48000);
                            #endif
                        }
                    }
                }
                break;

            // Add other effect types as needed
            default:
                // No sync needed for other node types
                break;
        }
    }

    DBG("[ChainGraph] Synced parameters to global audio effect units\n");
    return 0;
}

#endif /* EFFECT_GRAPHICS_EN */