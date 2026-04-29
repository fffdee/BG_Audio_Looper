/**
 *****************************************************************************
 * @file     shell_cmd_audio_vfs.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     06-January-2026
 * @brief    /audio 目录Shell命令 - 管理效果图VFS
 *****************************************************************************
 * @attention
 *
 * 提供在命令行中管理效果图的功能：
 * - 创建新效果图: audio create <name> [preset]
 * - 删除效果图:   audio delete <name>
 * - 列出效果图:   audio list
 * - 重载效果图:   audio reload <name>
 *
 * 结合VFS命令可实现：
 *   $ cd /audio/graph0/nodes/3_drc
 *   $ cat threshold     # 读取参数
 *   $ echo -20 > threshold  # 设置参数（如果系统支持）
 *   $ ls               # 列出所有参数
 *
 *****************************************************************************
 */

#include "effect_graph_vfs.h"
#include "effect_graph.h"
#include "effect_graph_config.h"
#include "bg_shell.h"
#include "vfs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"

#if USE_EFFECT_GRAPH_VFS && EFFECT_GRAPHICS_EN

/*******************************************************************************
 * 内部函数
 ******************************************************************************/

/**
 * @brief 打印帮助信息
 */
static void PrintHelp(void)
{
    Shell_Printf("\n===== Audio VFS Commands =====\n");
    Shell_Printf("audio list               - List all effect graphs\n");
    Shell_Printf("audio mount              - Mount default graph (graph0)\n");
    Shell_Printf("audio create <name> [preset] - Create new graph\n");
    Shell_Printf("audio delete <name>      - Delete graph\n");
    Shell_Printf("audio reload <name>      - Reload graph VFS\n");
    Shell_Printf("audio info <name>        - Show graph info\n");
    Shell_Printf("\n");
    Shell_Printf("Use with VFS commands:\n");
    Shell_Printf("  cd /audio              - Enter audio directory\n");
    Shell_Printf("  ls                     - List graphs\n");
    Shell_Printf("  cd graph0/nodes/3_drc  - Enter node directory\n");
    Shell_Printf("  cat threshold          - Read parameter\n");
    Shell_Printf("================================\n\n");
}

/**
 * @brief 列出效果图回调
 */
static void ListGraphCallback(const char *name, GraphVfsHandle_t *handle)
{
    Shell_Printf("  /audio/%-12s  nodes=%d\n", 
                 name, handle->graph ? handle->graph->node_count : 0);
}

/**
 * @brief 列出所有效果图
 */
static int CmdList(int argc, char *argv[])
{
    int count;
    (void)argc;
    (void)argv;
    
    Shell_Printf("\n===== Effect Graphs =====\n");
    count = EffectGraphVfs_ListGraphs(ListGraphCallback);
    
    if (count == 0) {
        Shell_Printf("  (no graphs mounted)\n");
        Shell_Printf("  Use 'audio mount' to mount default graph\n");
    }
    
    Shell_Printf("=========================\n");
    Shell_Printf("Total: %d graph(s)\n\n", count);
    return 0;
}

/**
 * @brief 挂载默认效果图
 */
static int CmdMount(int argc, char *argv[])
{
    EffectGraphRuntime_t *graph;
    GraphVfsHandle_t *handle;
    (void)argc;
    (void)argv;
    
    /* 检查效果图是否已初始化 */
    graph = EffectGraph_GetInstance();
    if (!graph) {
        Shell_Printf("ERROR: Effect graph not initialized yet\n");
        Shell_Printf("Please wait for audio system to start\n");
        return -1;
    }
    
    /* 检查是否已挂载 */
    if (EffectGraphVfs_ListGraphs(NULL) > 0) {
        Shell_Printf("Graph already mounted. Use 'audio list' to see\n");
        return 0;
    }
    
    /* 挂载为 graph0 */
    handle = EffectGraphVfs_Mount("graph0", graph);
    if (!handle) {
        Shell_Printf("ERROR: Failed to mount graph\n");
        return -1;
    }
    
    Shell_Printf("Default graph mounted at /audio/graph0\n");
    Shell_Printf("Use 'cd /audio/graph0' to access it\n");
    return 0;
}

/**
 * @brief 创建新效果图
 */
static int CmdCreate(int argc, char *argv[])
{
    const char *name;
    int preset = 0;
    GraphVfsHandle_t *handle;
    
    if (argc < 3) {
        Shell_Printf("Usage: audio create <name> [preset]\n");
        Shell_Printf("  preset: 0-%d (default=0)\n", GRAPH_PRESET_MAX - 1);
        return -1;
    }
    
    name = argv[2];
    
    if (argc >= 4) {
        preset = atoi(argv[3]);
        if (preset < 0 || preset >= GRAPH_PRESET_MAX) {
            Shell_Printf("ERROR: Invalid preset [0-%d]\n", GRAPH_PRESET_MAX - 1);
            return -1;
        }
    }
    
    handle = EffectGraphVfs_CreateGraph(name, (GraphPreset_t)preset);
    if (!handle) {
        Shell_Printf("ERROR: Failed to create graph '%s'\n", name);
        return -1;
    }
    
    Shell_Printf("Graph '%s' created at /audio/%s\n", name, name);
    Shell_Printf("Use 'cd /audio/%s' to access it\n", name);
    return 0;
}

/**
 * @brief 删除效果图
 */
static int CmdDelete(int argc, char *argv[])
{
    const char *name;
    GraphVfsError_t err;
    
    if (argc < 3) {
        Shell_Printf("Usage: audio delete <name>\n");
        return -1;
    }
    
    name = argv[2];
    
    err = EffectGraphVfs_DeleteGraph(name);
    if (err != GRAPH_VFS_OK) {
        Shell_Printf("ERROR: Graph '%s' not found\n", name);
        return -1;
    }
    
    Shell_Printf("Graph '%s' deleted\n", name);
    return 0;
}

/**
 * @brief 重载效果图VFS
 */
static int CmdReload(int argc, char *argv[])
{
    const char *name;
    GraphVfsHandle_t *handle;
    GraphVfsError_t err;
    
    if (argc < 3) {
        Shell_Printf("Usage: audio reload <name>\n");
        return -1;
    }
    
    name = argv[2];
    
    /* 查找已挂载的图 */
    /* 注意：需要内部函数访问，这里简化处理 */
    Shell_Printf("Reloading graph '%s'...\n", name);
    
    /* 删除后重新创建 */
    EffectGraphVfs_DeleteGraph(name);
    handle = EffectGraphVfs_CreateGraph(name, GRAPH_PRESET_DEFAULT);
    
    if (!handle) {
        Shell_Printf("ERROR: Failed to reload graph '%s'\n", name);
        return -1;
    }
    
    Shell_Printf("Graph '%s' reloaded\n", name);
    return 0;
}

/**
 * @brief 显示效果图信息
 */
static int CmdInfo(int argc, char *argv[])
{
    const char *name;
    char path[64];
    VfsNode_t *node;
    char buf[64];
    int len;
    
    if (argc < 3) {
        Shell_Printf("Usage: audio info <name>\n");
        return -1;
    }
    
    name = argv[2];
    
    /* 构建路径并读取info节点 */
    snprintf(path, sizeof(path), "/audio/%s/info", name);
    node = Vfs_FindNode(path);
    
    if (!node) {
        Shell_Printf("ERROR: Graph '%s' not found\n", name);
        return -1;
    }
    
    len = Vfs_ReadParam(node, buf, sizeof(buf));
    if (len > 0) {
        Shell_Printf("\n=== Graph: %s ===\n", name);
        Shell_Printf("%s\n", buf);
        
        /* 读取preset */
        snprintf(path, sizeof(path), "/audio/%s/preset", name);
        node = Vfs_FindNode(path);
        if (node && Vfs_ReadParam(node, buf, sizeof(buf)) > 0) {
            Shell_Printf("Preset: %s\n", buf);
        }
        
        /* 读取node_count */
        snprintf(path, sizeof(path), "/audio/%s/node_count", name);
        node = Vfs_FindNode(path);
        if (node && Vfs_ReadParam(node, buf, sizeof(buf)) > 0) {
            Shell_Printf("Node count: %s\n", buf);
        }
        
        Shell_Printf("==================\n\n");
    }
    
    return 0;
}

/*******************************************************************************
 * Shell模块注册
 ******************************************************************************/

/**
 * @brief audio命令处理入口
 */
static int AudioModuleHandler(int argc, char *argv[])
{
    /* 
     * Shell系统调用此函数时：
     * - 如果用户输入 "audio"，则 argc=0, argv=NULL
     * - 如果用户输入 "audio mount"，则 argc=1, argv[0]="mount"
     * - 如果用户输入 "audio mount arg1"，则 argc=2, argv[0]="mount", argv[1]="arg1"
     */
    
    /* 重新构建参数列表，添加 "audio" 作为 argv[0] */
    char *fullArgv[32];
    int fullArgc = argc + 1;
    int i;
    
    fullArgv[0] = "audio";
    for (i = 0; i < argc && i < 31; i++) {
        fullArgv[i + 1] = argv[i];
    }
    
    /* 调用执行函数 */
    return ShellCmdAudioVfs_Execute(fullArgc, fullArgv);
}

/**
 * @brief audio命令选项定义
 */
static const ShellOpt_t g_AudioVfsOpts[] = {
    { "", NULL, "[subcmd] [args]", "Audio VFS Management", AudioModuleHandler },
    OPT_END()
};

/**
 * @brief audio命令模块定义
 */
static const ShellModule_t g_AudioVfsModule = {
    "audio",
    "Audio Effect Graph VFS Management",
    MOD_CAT_AUDIO,
    g_AudioVfsOpts,
    1
};

/*******************************************************************************
 * 公共API
 ******************************************************************************/

/**
 * @brief 注册audio VFS Shell命令
 */
void ShellCmdAudioVfs_Register(void)
{
    Shell_RegisterModule(&g_AudioVfsModule);
    DBG("[ShellCmdAudioVfs] Registered\n");
}

/**
 * @brief audio命令执行入口
 */
int ShellCmdAudioVfs_Execute(int argc, char *argv[])
{
    if (argc < 2) {
        PrintHelp();
        return 0;
    }
    
    const char *subcmd = argv[1];
    
    if (strcmp(subcmd, "help") == 0) {
        PrintHelp();
        return 0;
    }
    else if (strcmp(subcmd, "list") == 0) {
        return CmdList(argc, argv);
    }
    else if (strcmp(subcmd, "mount") == 0) {
        return CmdMount(argc, argv);
    }
    else if (strcmp(subcmd, "create") == 0) {
        return CmdCreate(argc, argv);
    }
    else if (strcmp(subcmd, "delete") == 0) {
        return CmdDelete(argc, argv);
    }
    else if (strcmp(subcmd, "reload") == 0) {
        return CmdReload(argc, argv);
    }
    else if (strcmp(subcmd, "info") == 0) {
        return CmdInfo(argc, argv);
    }
    else {
        Shell_Printf("ERROR: Unknown subcommand '%s'\n", subcmd);
        PrintHelp();
        return -1;
    }
}

/**
 * @brief 检查并自动挂载默认效果图
 * @note  此函数可在音频系统初始化后被调用，会自动挂载graph0
 */
void ShellCmdAudioVfs_CheckAutoMount(void)
{
    /* 如果还没有挂载任何图，尝试挂载默认图 */
    if (EffectGraphVfs_ListGraphs(NULL) == 0) {
        GraphVfsError_t err = EffectGraphVfs_TryAutoMount();
        if (err == GRAPH_VFS_OK) {
            DBG("[AudioVfs] Auto-mounted graph0\n");
        }
    }
}

#endif /* USE_EFFECT_GRAPH_VFS */
