/**
 *****************************************************************************
 * @file     effect_graph_vfs.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     06-January-2026
 * @brief    鏁堟灉鍥捐櫄鎷熸枃浠剁郴缁�- 灏嗘晥鏋滃浘鍙傛暟鏄犲皠鍒版枃浠剁郴缁熺洰褰� *****************************************************************************
 * @attention
 *
 * 鏈ā鍧楀湪Shell VFS涓垱寤�audio鐩綍锛屾敮鎸佸涓晥鏋滃浘瀹炰緥锛� * 姣忎釜鏁堟灉鍥剧殑鑺傜偣鍜屽弬鏁伴兘鍙互閫氳繃鏂囦欢绯荤粺璺緞璁块棶鍜屼慨鏀广�
 *
 * 鐩綍缁撴瀯绀轰緥锛� *   /audio
 *       鈹溾攢鈹�graph0/              (榛樿鏁堟灉鍥�
 *       鈹�  鈹溾攢鈹�info             (鍙锛氬浘淇℃伅)
 *       鈹�  鈹溾攢鈹�preset           (璇诲啓锛氬綋鍓嶉璁綢D)
 *       鈹�  鈹溾攢鈹�nodes/
 *       鈹�  鈹�  鈹溾攢鈹�0_adc0/
 *       鈹�  鈹�  鈹�  鈹溾攢鈹�enabled  (璇诲啓锛�/1)
 *       鈹�  鈹�  鈹�  鈹斺攢鈹�bypass   (璇诲啓锛�/1)
 *       鈹�  鈹�  鈹溾攢鈹�3_drc/
 *       鈹�  鈹�  鈹�  鈹溾攢鈹�enabled
 *       鈹�  鈹�  鈹�  鈹溾攢鈹�bypass
 *       鈹�  鈹�  鈹�  鈹溾攢鈹�threshold (璇诲啓锛氬弬鏁板�)
 *       鈹�  鈹�  鈹�  鈹溾攢鈹�ratio
 *       鈹�  鈹�  鈹�  鈹溾攢鈹�attack
 *       鈹�  鈹�  鈹�  鈹斺攢鈹�release
 *       鈹�  鈹�  鈹斺攢鈹�...
 *       鈹�  鈹斺攢鈹�snapshots/       (蹇収鐩綍)
 *       鈹�      鈹溾攢鈹�0_name
 *       鈹�      鈹溾攢鈹�1_name
 *       鈹�      鈹斺攢鈹�...
 *       鈹斺攢鈹�graph1/              (鍙姩鎬佸垱寤烘洿澶氬浘)
 *
 * 浣跨敤绀轰緥锛� *   $ cd /audio/graph0/nodes/3_drc
 *   $ cat threshold              # 璇诲彇鍙傛暟
 *   $ echo -20 > threshold       # 璁剧疆鍙傛暟
 *   $ cat enabled                # 鏌ョ湅鍚敤鐘舵�
 *   $ echo 1 > enabled           # 鍚敤鑺傜偣
 *   $ cd /audio
 *   $ ls                         # 鍒楀嚭鎵�湁鏁堟灉鍥� *
 *****************************************************************************
 */

#ifndef __EFFECT_GRAPH_VFS_H__
#define __EFFECT_GRAPH_VFS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * VFS 功能开关 (2026-02-05 新增)
 * 
 * USE_EFFECT_GRAPH_VFS=1: 启用 VFS 功能
 *   - 支持通过 Shell 文件系统访问效果图参数
 *   - 消耗约 700 bytes RAM + 4KB Flash
 *   - 适用于调试开发阶段
 * 
 * USE_EFFECT_GRAPH_VFS=0: 关闭 VFS 功能（默认）
 *   - 所有 VFS API 变为空操作宏
 *   - 节省 ~700 bytes RAM + ~4KB Flash
 *   - 适用于生产环境
 ******************************************************************************/
#ifndef USE_EFFECT_GRAPH_VFS
#define USE_EFFECT_GRAPH_VFS  1  /* 默认开启VFS功能，方便调试 */
#endif

#if USE_EFFECT_GRAPH_VFS
/* ==================== VFS 启用模式 ==================== */

#include "vfs.h"
#include "effect_graph.h"
#include "effect_graph_config.h"  /* For GraphPreset_t */
#include <stdbool.h>
/*******************************************************************************
 * 绫诲瀷瀹氫箟
 ******************************************************************************/

/**
 * @brief 鏁堟灉鍥綱FS閿欒鐮� */
typedef enum {
    GRAPH_VFS_OK = 0,
    GRAPH_VFS_ERR_NO_MEMORY,
    GRAPH_VFS_ERR_NOT_FOUND,
    GRAPH_VFS_ERR_INVALID_PARAM,
    GRAPH_VFS_ERR_ALREADY_EXISTS,
    GRAPH_VFS_ERR_NOT_INITIALIZED,
} GraphVfsError_t;

/**
 * @brief 鏁堟灉鍥惧疄渚媀FS鍙ユ焺
 */
typedef struct {
    char name[16];              /* 鍥惧悕绉帮紝濡�graph0" */
    EffectGraphRuntime_t *graph;       /* 鍏宠仈鐨勬晥鏋滃浘瀹炰緥 */
    VfsNode_t *root_dir;        /* 鍥炬牴鐩綍鑺傜偣 */
    VfsNode_t *info_node;       /* info鑺傜偣 */
    VfsNode_t *preset_node;     /* preset鑺傜偣 */
    VfsNode_t *nodes_dir;       /* nodes/鐩綍 */
    VfsNode_t *snapshots_dir;   /* snapshots/鐩綍 */
    bool mounted;               /* 鏄惁宸叉寕杞�*/
} GraphVfsHandle_t;

/*******************************************************************************
 * API鍑芥暟
 ******************************************************************************/

/**
 * @brief  鍒濆鍖栨晥鏋滃浘VFS绯荤粺锛堝垱寤�audio鐩綍锛� * @return GRAPH_VFS_OK鎴愬姛锛屽叾浠栧け璐� * @note   蹇呴』鍏堣皟鐢╒fs_Init()鍜孲hellFs_Init()
 */
GraphVfsError_t EffectGraphVfs_Init(void);

/**
 * @brief  鎸傝浇鏁堟灉鍥惧埌VFS锛堝垱寤篻raph鐩綍缁撴瀯锛� * @param  graph_name: 鍥惧悕绉帮紝濡�graph0"
 * @param  graph: 鏁堟灉鍥惧疄渚嬫寚閽� * @return 鍥綱FS鍙ユ焺锛孨ULL琛ㄧず澶辫触
 * @note   浼氬湪/audio涓嬪垱寤哄搴旂殑鐩綍缁撴瀯
 */
GraphVfsHandle_t* EffectGraphVfs_Mount(const char *graph_name, EffectGraphRuntime_t *graph);

/**
 * @brief  鍗歌浇鏁堟灉鍥� * @param  handle: 鍥綱FS鍙ユ焺
 * @return GRAPH_VFS_OK鎴愬姛锛屽叾浠栧け璐� */
GraphVfsError_t EffectGraphVfs_Unmount(GraphVfsHandle_t *handle);

/**
 * @brief  鍒锋柊鏁堟灉鍥綱FS锛堟洿鏂拌妭鐐瑰弬鏁帮級
 * @param  handle: 鍥綱FS鍙ユ焺
 * @return GRAPH_VFS_OK鎴愬姛锛屽叾浠栧け璐� * @note   褰撴晥鏋滃浘缁撴瀯鍙樺寲锛堝娣诲姞/鍒犻櫎鑺傜偣锛夋椂璋冪敤
 */
GraphVfsError_t EffectGraphVfs_Refresh(GraphVfsHandle_t *handle);

/**
 * @brief  閫氳繃VFS璺緞鏌ユ壘鑺傜偣
 * @param  path: VFS璺緞锛屽"/audio/graph0/nodes/3_drc"
 * @return 鑺傜偣鎸囬拡锛孨ULL琛ㄧず鏈壘鍒� */
EffectNode_t* EffectGraphVfs_FindNodeByPath(const char *path);

/**
 * @brief  鑾峰彇/audio鐩綍鑺傜偣
 * @return /audio鐩綍鑺傜偣鎸囬拡
 */
VfsNode_t* EffectGraphVfs_GetAudioDir(void);

/**
 * @brief  鍒涘缓鏂扮殑鏁堟灉鍥惧疄渚嬶紙浠庡懡浠よ锛� * @param  graph_name: 鍥惧悕绉� * @param  preset_id: 棰勮ID锛堢敤浜庡垵濮嬪寲锛� * @return 鏂板垱寤虹殑鍥惧彞鏌勶紝NULL琛ㄧず澶辫触
 * @note   鏀寔鍔ㄦ�鍒涘缓澶氫釜鏁堟灉鍥惧疄渚� */
GraphVfsHandle_t* EffectGraphVfs_CreateGraph(const char *graph_name, GraphPreset_t preset_id);

/**
 * @brief  鍒犻櫎鏁堟灉鍥惧疄渚� * @param  graph_name: 鍥惧悕绉� * @return GRAPH_VFS_OK鎴愬姛锛屽叾浠栧け璐� */
GraphVfsError_t EffectGraphVfs_DeleteGraph(const char *graph_name);

/**
 * @brief  鍒楀嚭鎵�湁宸叉寕杞界殑鏁堟灉鍥� * @param  callback: 鍥炶皟鍑芥暟锛屽弬鏁颁负鍥惧悕绉板拰鍙ユ焺
 * @return 鍥炬暟閲� */
int EffectGraphVfs_ListGraphs(void (*callback)(const char *name, GraphVfsHandle_t *handle));

/**
 * @brief  鎸傝浇榛樿鏁堟灉鍥撅紙绯荤粺鍚姩鏃惰皟鐢級
 * @return GRAPH_VFS_OK鎴愬姛锛屽叾浠栧け璐� * @note   灏嗛粯璁ゆ晥鏋滃浘瀹炰緥鎸傝浇涓�/audio/graph0
 *         濡傛灉鏁堟灉鍥炬湭鍒濆鍖栵紝浼氶潤榛樿繑鍥濷K锛岀◢鍚庡彲閲嶈瘯
 */
GraphVfsError_t EffectGraphVfs_MountDefault(void);

/**
 * @brief  灏濊瘯鑷姩鎸傝浇锛堥煶棰戠郴缁熷垵濮嬪寲鍚庤皟鐢級
 * @return GRAPH_VFS_OK鎴愬姛锛屽叾浠栧け璐� * @note   搴斿湪闊抽鏁堟灉鍥惧垵濮嬪寲鍚庤皟鐢ㄦ鍑芥暟
 */
GraphVfsError_t EffectGraphVfs_TryAutoMount(void);
#else
/* ==================== VFS 关闭模式 - 空操作宏 ==================== */

/* 类型定义（空壳，避免编译错误） */
typedef int GraphVfsError_t;
typedef void* GraphVfsHandle_t;
#define GRAPH_VFS_OK 0

/* 所有 API 变为空操作宏，不占用任何内存 */
#define EffectGraphVfs_Init()                           (GRAPH_VFS_OK)
#define EffectGraphVfs_Mount(graph_name, graph)         ((GraphVfsHandle_t*)0)
#define EffectGraphVfs_Unmount(handle)                  (GRAPH_VFS_OK)
#define EffectGraphVfs_Refresh(handle)                  (GRAPH_VFS_OK)
#define EffectGraphVfs_FindNodeByPath(path)             ((void*)0)
#define EffectGraphVfs_GetAudioDir()                    ((void*)0)
#define EffectGraphVfs_CreateGraph(graph_name, preset_id) ((GraphVfsHandle_t*)0)
#define EffectGraphVfs_DeleteGraph(graph_name)          (GRAPH_VFS_OK)
#define EffectGraphVfs_ListGraphs(callback)             (0)
#define EffectGraphVfs_MountDefault()                   (GRAPH_VFS_OK)
#define EffectGraphVfs_TryAutoMount()                   (GRAPH_VFS_OK)

#endif /* USE_EFFECT_GRAPH_VFS */
#ifdef __cplusplus
}
#endif

#endif /* __EFFECT_GRAPH_VFS_H__ */
