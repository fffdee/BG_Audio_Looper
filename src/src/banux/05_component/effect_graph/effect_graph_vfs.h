/**
 *****************************************************************************
 * @file     effect_graph_vfs.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     06-January-2026
 * @brief    效果图虚拟文件系统 - 将效果图参数映射到文件系统目录
 *****************************************************************************
 * @attention
 *
 * 本模块在Shell VFS中创建/audio目录，支持多个效果图实例，
 * 每个效果图的节点和参数都可以通过文件系统路径访问和修改。
 *
 * 目录结构示例：
 *   /audio
 *       ├── graph0/              (默认效果图)
 *       │   ├── info             (只读：图信息)
 *       │   ├── preset           (读写：当前预设ID)
 *       │   ├── nodes/
 *       │   │   ├── 0_adc0/
 *       │   │   │   ├── enabled  (读写：0/1)
 *       │   │   │   └── bypass   (读写：0/1)
 *       │   │   ├── 3_drc/
 *       │   │   │   ├── enabled
 *       │   │   │   ├── bypass
 *       │   │   │   ├── threshold (读写：参数值)
 *       │   │   │   ├── ratio
 *       │   │   │   ├── attack
 *       │   │   │   └── release
 *       │   │   └── ...
 *       │   └── snapshots/       (快照目录)
 *       │       ├── 0_name
 *       │       ├── 1_name
 *       │       └── ...
 *       └── graph1/              (可动态创建更多图)
 *
 * 使用示例：
 *   $ cd /audio/graph0/nodes/3_drc
 *   $ cat threshold              # 读取参数
 *   $ echo -20 > threshold       # 设置参数
 *   $ cat enabled                # 查看启用状态
 *   $ echo 1 > enabled           # 启用节点
 *   $ cd /audio
 *   $ ls                         # 列出所有效果图
 *
 *****************************************************************************
 */

#ifndef __EFFECT_GRAPH_VFS_H__
#define __EFFECT_GRAPH_VFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "vfs.h"
#include "effect_graph.h"
#include "effect_graph_config.h"  /* For GraphPreset_t */

/*******************************************************************************
 * 类型定义
 ******************************************************************************/

/**
 * @brief 效果图VFS错误码
 */
typedef enum {
    GRAPH_VFS_OK = 0,
    GRAPH_VFS_ERR_NO_MEMORY,
    GRAPH_VFS_ERR_NOT_FOUND,
    GRAPH_VFS_ERR_INVALID_PARAM,
    GRAPH_VFS_ERR_ALREADY_EXISTS,
    GRAPH_VFS_ERR_NOT_INITIALIZED,
} GraphVfsError_t;

/**
 * @brief 效果图实例VFS句柄
 */
typedef struct {
    char name[16];              /* 图名称，如"graph0" */
    EffectGraph_t *graph;       /* 关联的效果图实例 */
    VfsNode_t *root_dir;        /* 图根目录节点 */
    VfsNode_t *info_node;       /* info节点 */
    VfsNode_t *preset_node;     /* preset节点 */
    VfsNode_t *nodes_dir;       /* nodes/目录 */
    VfsNode_t *snapshots_dir;   /* snapshots/目录 */
    bool mounted;               /* 是否已挂载 */
} GraphVfsHandle_t;

/*******************************************************************************
 * API函数
 ******************************************************************************/

/**
 * @brief  初始化效果图VFS系统（创建/audio目录）
 * @return GRAPH_VFS_OK成功，其他失败
 * @note   必须先调用Vfs_Init()和ShellFs_Init()
 */
GraphVfsError_t EffectGraphVfs_Init(void);

/**
 * @brief  挂载效果图到VFS（创建graph目录结构）
 * @param  graph_name: 图名称，如"graph0"
 * @param  graph: 效果图实例指针
 * @return 图VFS句柄，NULL表示失败
 * @note   会在/audio下创建对应的目录结构
 */
GraphVfsHandle_t* EffectGraphVfs_Mount(const char *graph_name, EffectGraph_t *graph);

/**
 * @brief  卸载效果图
 * @param  handle: 图VFS句柄
 * @return GRAPH_VFS_OK成功，其他失败
 */
GraphVfsError_t EffectGraphVfs_Unmount(GraphVfsHandle_t *handle);

/**
 * @brief  刷新效果图VFS（更新节点参数）
 * @param  handle: 图VFS句柄
 * @return GRAPH_VFS_OK成功，其他失败
 * @note   当效果图结构变化（如添加/删除节点）时调用
 */
GraphVfsError_t EffectGraphVfs_Refresh(GraphVfsHandle_t *handle);

/**
 * @brief  通过VFS路径查找节点
 * @param  path: VFS路径，如"/audio/graph0/nodes/3_drc"
 * @return 节点指针，NULL表示未找到
 */
EffectNode_t* EffectGraphVfs_FindNodeByPath(const char *path);

/**
 * @brief  获取/audio目录节点
 * @return /audio目录节点指针
 */
VfsNode_t* EffectGraphVfs_GetAudioDir(void);

/**
 * @brief  创建新的效果图实例（从命令行）
 * @param  graph_name: 图名称
 * @param  preset_id: 预设ID（用于初始化）
 * @return 新创建的图句柄，NULL表示失败
 * @note   支持动态创建多个效果图实例
 */
GraphVfsHandle_t* EffectGraphVfs_CreateGraph(const char *graph_name, GraphPreset_t preset_id);

/**
 * @brief  删除效果图实例
 * @param  graph_name: 图名称
 * @return GRAPH_VFS_OK成功，其他失败
 */
GraphVfsError_t EffectGraphVfs_DeleteGraph(const char *graph_name);

/**
 * @brief  列出所有已挂载的效果图
 * @param  callback: 回调函数，参数为图名称和句柄
 * @return 图数量
 */
int EffectGraphVfs_ListGraphs(void (*callback)(const char *name, GraphVfsHandle_t *handle));

/**
 * @brief  挂载默认效果图（系统启动时调用）
 * @return GRAPH_VFS_OK成功，其他失败
 * @note   将默认效果图实例挂载为 /audio/graph0
 *         如果效果图未初始化，会静默返回OK，稍后可重试
 */
GraphVfsError_t EffectGraphVfs_MountDefault(void);

/**
 * @brief  尝试自动挂载（音频系统初始化后调用）
 * @return GRAPH_VFS_OK成功，其他失败
 * @note   应在音频效果图初始化后调用此函数
 */
GraphVfsError_t EffectGraphVfs_TryAutoMount(void);

#ifdef __cplusplus
}
#endif

#endif /* __EFFECT_GRAPH_VFS_H__ */
