/**
 *****************************************************************************
 * @file     vfs.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    铏氭嫙鏂囦欢绯荤粺 - 绫籐inux鏍戝舰鐩綍缁撴瀯
 *****************************************************************************
 * @attention
 *
 * 鏈ā鍧楀疄鐜扮被Linux铏氭嫙鏂囦欢绯荤粺锛屾彁渚涳細
 * 1. 鏍戝舰鐩綍缁撴瀯锛�driver/spi/st7735/param1, /bin/sys/info锛� * 2. 鑺傜偣绫诲瀷锛氱洰褰曡妭鐐�DIR) / 鍙傛暟鑺傜偣(PARAM) / 璁惧鑺傜偣(DEV)
 * 3. 璺緞瑙ｆ瀽涓庡鑸� * 4. 涓嶴hell鍛戒护绯荤粺缁戝畾锛坈d/pwd/ls/cat/echo锛� *
 * 鐩綍缁撴瀯绀轰緥锛� *   /
 *   鈹溾攢鈹�bin                    # 绯荤粺鍛戒护
 *   鈹�  鈹斺攢鈹�sys
 *   鈹�      鈹溾攢鈹�info
 *   鈹�      鈹溾攢鈹�mem
 *   鈹�      鈹斺攢鈹�tasks
 *   鈹斺攢鈹�driver                 # 纭欢椹卞姩
 *       鈹溾攢鈹�spi
 *       鈹�  鈹溾攢鈹�st7735
 *       鈹�  鈹�  鈹溾攢鈹�name
 *       鈹�  鈹�  鈹溾攢鈹�width
 *       鈹�  鈹�  鈹斺攢鈹�height
 *       鈹�  鈹斺攢鈹�w25q64
 *       鈹溾攢鈹�i2c
 *       鈹溾攢鈹�i2s
 *       鈹斺攢鈹�usb
 *
 *****************************************************************************
 */

#ifndef __VFS_H__
#define __VFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

/*******************************************************************************
 * 閰嶇疆瀹氫箟 - 閽堝宓屽叆寮忕郴缁熶紭鍖栵紝鍑忓皯鍐呭瓨鍗犵敤
 ******************************************************************************/
#define VFS_MAX_PATH_LEN     64      /* 鏈�ぇ璺緞闀垮害 */
#define VFS_MAX_NAME_LEN     16      /* 鑺傜偣鍚嶇О鏈�ぇ闀垮害 */
#define VFS_MAX_CHILDREN     32      /* 姣忎釜鐩綍鏈�ぇ瀛愯妭鐐规暟锛堝鍔犱互鏀寔澶氳妭鐐规晥鏋滃浘锛�*/
#define VFS_MAX_PARAM_LEN    32      /* 鍙傛暟鍊兼渶澶ч暱搴�*/
#define VFS_MAX_NODES        256     /* 绯荤粺鏈�ぇ鑺傜偣鏁帮紙澧炲姞浠ユ敮鎸佹晥鏋滃浘VFS锛�*/

/*******************************************************************************
 * 鑺傜偣绫诲瀷瀹氫箟
 ******************************************************************************/
typedef enum {
    VFS_NODE_DIR = 0,        /* 鐩綍鑺傜偣 */
    VFS_NODE_PARAM,          /* 鍙傛暟鑺傜偣锛堝彲璇诲啓锛�*/
    VFS_NODE_DEV,            /* 璁惧鑺傜偣锛堝叧鑱旈┍鍔級 */
    VFS_NODE_CMD,            /* 鍛戒护鑺傜偣锛�bin/鍛戒护锛�*/
} VfsNodeType_t;

/*******************************************************************************
 * 鍙傛暟璇诲啓鍥炶皟鍑芥暟绫诲瀷
 ******************************************************************************/
/**
 * @brief  鍙傛暟璇诲彇鍥炶皟
 * @param  buf: 杈撳嚭缂撳啿鍖� * @param  maxLen: 缂撳啿鍖烘渶澶ч暱搴� * @param  userData: 鐢ㄦ埛鏁版嵁锛堣澶囩鏈夋暟鎹級
 * @return 瀹為檯璇诲彇鐨勯暱搴︼紝-1琛ㄧず閿欒
 */
typedef int (*VfsParamGet_t)(char *buf, uint16_t maxLen, void *userData);

/**
 * @brief  鍙傛暟鍐欏叆鍥炶皟
 * @param  value: 鍐欏叆鐨勫�瀛楃涓� * @param  userData: 鐢ㄦ埛鏁版嵁锛堣澶囩鏈夋暟鎹級
 * @return 0鎴愬姛锛�1澶辫触
 */
typedef int (*VfsParamSet_t)(const char *value, void *userData);

/*******************************************************************************
 * 鏂囦欢绯荤粺鑺傜偣缁撴瀯锛堟爲褰㈢粨鏋勶級
 ******************************************************************************/
typedef struct VfsNode {
    char                name[VFS_MAX_NAME_LEN];      /* 鑺傜偣鍚嶇О */
    VfsNodeType_t       type;                        /* 鑺傜偣绫诲瀷 */
    struct VfsNode     *parent;                      /* 鐖惰妭鐐�*/
    struct VfsNode     *children[VFS_MAX_CHILDREN];  /* 瀛愯妭鐐规暟缁�*/
    uint8_t             childCount;                  /* 瀛愯妭鐐规暟閲�*/
    
    /* 鍙傛暟鑺傜偣涓撶敤 */
    VfsParamGet_t       paramGet;                    /* 鍙傛暟璇诲彇鍑芥暟 */
    VfsParamSet_t       paramSet;                    /* 鍙傛暟鍐欏叆鍑芥暟 */
    const char         *paramDesc;                   /* 鍙傛暟鎻忚堪 */
    
    /* 璁惧/鍙傛暟鑺傜偣涓撶敤 */
    void               *userData;                    /* 鐢ㄦ埛绉佹湁鏁版嵁 */
    void               *driver;                      /* 鍏宠仈鐨勯┍鍔ㄦ寚閽�*/
} VfsNode_t;

/*******************************************************************************
 * 閿欒鐮佸畾涔� ******************************************************************************/
typedef enum {
    VFS_OK = 0,              /* 鎴愬姛 */
    VFS_ERR_NOT_FOUND,       /* 璺緞涓嶅瓨鍦�*/
    VFS_ERR_NOT_DIR,         /* 涓嶆槸鐩綍 */
    VFS_ERR_NOT_PARAM,       /* 涓嶆槸鍙傛暟鑺傜偣 */
    VFS_ERR_READ_ONLY,       /* 鍙傛暟鍙 */
    VFS_ERR_NO_MEMORY,       /* 鍐呭瓨涓嶈冻 */
    VFS_ERR_NAME_TOO_LONG,   /* 鍚嶇О杩囬暱 */
    VFS_ERR_DIR_FULL,        /* 鐩綍宸叉弧 */
    VFS_ERR_ALREADY_EXISTS,  /* 鑺傜偣宸插瓨鍦�*/
    VFS_ERR_INVALID_PATH,    /* 鏃犳晥璺緞 */
} VfsError_t;

/*******************************************************************************
 * 鐩綍鍒椾妇鍥炶皟鍑芥暟绫诲瀷
 ******************************************************************************/
typedef void (*VfsListCallback_t)(VfsNode_t *node, void *userData);

/*******************************************************************************
 * 鏍稿績API鍑芥暟
 ******************************************************************************/

/**
 * @brief  鍒濆鍖栬櫄鎷熸枃浠剁郴缁燂紙浠呭垱寤烘牴鑺傜偣锛� * @return VFS_OK鎴愬姛锛屽叾浠栧け璐� */
VfsError_t Vfs_Init(void);

/**
 * @brief  鑾峰彇鏍硅妭鐐� * @return 鏍硅妭鐐规寚閽� */
VfsNode_t* Vfs_GetRoot(void);

/**
 * @brief  鑾峰彇褰撳墠宸ヤ綔鐩綍鑺傜偣
 * @return 褰撳墠鐩綍鑺傜偣鎸囬拡
 */
VfsNode_t* Vfs_GetCwd(void);

/**
 * @brief  鑾峰彇褰撳墠宸ヤ綔鐩綍璺緞瀛楃涓� * @param  buf: 杈撳嚭缂撳啿鍖� * @param  maxLen: 缂撳啿鍖哄ぇ灏� * @return VFS_OK鎴愬姛
 */
VfsError_t Vfs_GetCwdPath(char *buf, uint16_t maxLen);

/**
 * @brief  鍒囨崲褰撳墠鐩綍
 * @param  path: 鐩爣璺緞锛堟敮鎸佺浉瀵硅矾寰勫拰缁濆璺緞锛� * @return VFS_OK鎴愬姛锛屽叾浠栧け璐� */
VfsError_t Vfs_Cd(const char *path);

/**
 * @brief  鏍规嵁璺緞鏌ユ壘鑺傜偣
 * @param  path: 璺緞锛堢粷瀵规垨鐩稿锛� * @return 鑺傜偣鎸囬拡锛孨ULL琛ㄧず鏈壘鍒� */
VfsNode_t* Vfs_FindNode(const char *path);

/**
 * @brief  鍦ㄦ寚瀹氱洰褰曚笅鍒涘缓瀛愮洰褰� * @param  parent: 鐖剁洰褰曡妭鐐� * @param  name: 鐩綍鍚� * @return 鏂板垱寤虹殑鐩綍鑺傜偣锛孨ULL琛ㄧず澶辫触
 */
VfsNode_t* Vfs_CreateDir(VfsNode_t *parent, const char *name);

/**
 * @brief  鍦ㄦ寚瀹氱洰褰曚笅鍒涘缓鍙傛暟鑺傜偣
 * @param  parent: 鐖剁洰褰曡妭鐐� * @param  name: 鍙傛暟鍚� * @param  desc: 鍙傛暟鎻忚堪
 * @param  get: 璇诲彇鍥炶皟
 * @param  set: 鍐欏叆鍥炶皟锛圢ULL琛ㄧず鍙锛� * @param  userData: 鐢ㄦ埛鏁版嵁
 * @return 鏂板垱寤虹殑鍙傛暟鑺傜偣锛孨ULL琛ㄧず澶辫触
 */
VfsNode_t* Vfs_CreateParam(VfsNode_t *parent, const char *name, 
                            const char *desc,
                            VfsParamGet_t get, VfsParamSet_t set,
                            void *userData);

/**
 * @brief  鍦ㄦ寚瀹氱洰褰曚笅鍒涘缓璁惧鑺傜偣
 * @param  parent: 鐖剁洰褰曡妭鐐� * @param  name: 璁惧鍚� * @param  userData: 璁惧绉佹湁鏁版嵁
 * @return 鏂板垱寤虹殑璁惧鑺傜偣锛孨ULL琛ㄧず澶辫触
 */
VfsNode_t* Vfs_CreateDevice(VfsNode_t *parent, const char *name, void *userData);

/**
 * @brief  鍦ㄦ寚瀹氱洰褰曚笅鍒涘缓閫氱敤鑺傜偣
 * @param  parent: 鐖剁洰褰曡妭鐐� * @param  name: 鑺傜偣鍚� * @param  type: 鑺傜偣绫诲瀷
 * @param  userData: 鐢ㄦ埛鏁版嵁
 * @return 鏂板垱寤虹殑鑺傜偣锛孨ULL琛ㄧず澶辫触
 */
VfsNode_t* Vfs_CreateNode(VfsNode_t *parent, const char *name, VfsNodeType_t type, void *userData);

/**
 * @brief  璇诲彇鍙傛暟鍊� * @param  node: 鍙傛暟鑺傜偣
 * @param  buf: 杈撳嚭缂撳啿鍖� * @param  maxLen: 缂撳啿鍖哄ぇ灏� * @return 璇诲彇鐨勫瓧鑺傛暟锛�1閿欒锛�2鍙啓鍙傛暟
 */
int Vfs_ReadParam(VfsNode_t *node, char *buf, uint16_t maxLen);

/**
 * @brief  鍐欏叆鍙傛暟鍊� * @param  node: 鍙傛暟鑺傜偣
 * @param  value: 鍐欏叆鐨勫�
 * @return VFS_OK鎴愬姛
 */
VfsError_t Vfs_WriteParam(VfsNode_t *node, const char *value);

/**
 * @brief  鍒椾妇鐩綍鍐呭
 * @param  node: 鐩綍鑺傜偣
 * @param  callback: 鍥炶皟鍑芥暟
 * @param  userData: 鐢ㄦ埛鏁版嵁
 * @return VFS_OK鎴愬姛
 */
VfsError_t Vfs_ListDir(VfsNode_t *node, VfsListCallback_t callback, void *userData);

/**
 * @brief  鍒犻櫎鑺傜偣锛堥�褰掑垹闄ゅ瓙鑺傜偣锛� * @param  node: 瑕佸垹闄ょ殑鑺傜偣
 * @return VFS_OK鎴愬姛
 */
VfsError_t Vfs_RemoveNode(VfsNode_t *node);

/**
 * @brief  鑾峰彇鑺傜偣绫诲瀷鍚嶇О瀛楃涓� * @param  type: 鑺傜偣绫诲瀷
 * @return 绫诲瀷鍚嶇О瀛楃涓� */
const char* Vfs_GetTypeName(VfsNodeType_t type);

/**
 * @brief  鏍规嵁璺緞鍒涘缓鐩綍锛堥�褰掑垱寤猴級
 * @param  path: 缁濆璺緞锛堝 "/bin/sys"锛� * @return 鍒涘缓鐨勭洰褰曡妭鐐癸紝NULL琛ㄧず澶辫触
 */
VfsNode_t* Vfs_Mkdir(const char *path);

/**
 * @brief VFS鍙傛暟鑺傜偣瀹氫箟缁撴瀯浣� */
typedef struct {
    const char *name;    // 鍙傛暟鍚�
    const char *desc;    // 鍙傛暟鎻忚堪
    int (*get)(char *buf, uint16_t maxLen, void *userData); // 璇诲洖璋�
    int (*set)(const char *buf, void *userData);            // 鍐欏洖璋�
    void *userData;     // 鐢ㄦ埛鏁版嵁
} FsParamDef_t;

#define FS_PARAM_END {NULL, NULL, NULL, NULL, NULL}

#ifdef __cplusplus
}
#endif

#endif /* __VFS_H__ */
