/**
 *****************************************************************************
 * @file     vfs.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    虚拟文件系统 - 类Linux树形目录结构
 *****************************************************************************
 * @attention
 *
 * 本模块实现类Linux虚拟文件系统，提供：
 * 1. 树形目录结构（/driver/spi/st7735/param1, /bin/sys/info）
 * 2. 节点类型：目录节点(DIR) / 参数节点(PARAM) / 设备节点(DEV)
 * 3. 路径解析与导航
 * 4. 与Shell命令系统绑定（cd/pwd/ls/cat/echo）
 *
 * 目录结构示例：
 *   /
 *   ├── bin                    # 系统命令
 *   │   └── sys
 *   │       ├── info
 *   │       ├── mem
 *   │       └── tasks
 *   └── driver                 # 硬件驱动
 *       ├── spi
 *       │   ├── st7735
 *       │   │   ├── name
 *       │   │   ├── width
 *       │   │   └── height
 *       │   └── w25q64
 *       ├── i2c
 *       ├── i2s
 *       └── usb
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
 * 配置定义 - 针对嵌入式系统优化，减少内存占用
 ******************************************************************************/
#define VFS_MAX_PATH_LEN     64      /* 最大路径长度 */
#define VFS_MAX_NAME_LEN     16      /* 节点名称最大长度 */
#define VFS_MAX_CHILDREN     20      /* 每个目录最大子节点数 */
#define VFS_MAX_PARAM_LEN    32      /* 参数值最大长度 */
#define VFS_MAX_NODES        64      /* 系统最大节点数 */

/*******************************************************************************
 * 节点类型定义
 ******************************************************************************/
typedef enum {
    VFS_NODE_DIR = 0,        /* 目录节点 */
    VFS_NODE_PARAM,          /* 参数节点（可读写） */
    VFS_NODE_DEV,            /* 设备节点（关联驱动） */
    VFS_NODE_CMD,            /* 命令节点（/bin/命令） */
} VfsNodeType_t;

/*******************************************************************************
 * 参数读写回调函数类型
 ******************************************************************************/
/**
 * @brief  参数读取回调
 * @param  buf: 输出缓冲区
 * @param  maxLen: 缓冲区最大长度
 * @param  userData: 用户数据（设备私有数据）
 * @return 实际读取的长度，-1表示错误
 */
typedef int (*VfsParamGet_t)(char *buf, uint16_t maxLen, void *userData);

/**
 * @brief  参数写入回调
 * @param  value: 写入的值字符串
 * @param  userData: 用户数据（设备私有数据）
 * @return 0成功，-1失败
 */
typedef int (*VfsParamSet_t)(const char *value, void *userData);

/*******************************************************************************
 * 文件系统节点结构（树形结构）
 ******************************************************************************/
typedef struct VfsNode {
    char                name[VFS_MAX_NAME_LEN];      /* 节点名称 */
    VfsNodeType_t       type;                        /* 节点类型 */
    struct VfsNode     *parent;                      /* 父节点 */
    struct VfsNode     *children[VFS_MAX_CHILDREN];  /* 子节点数组 */
    uint8_t             childCount;                  /* 子节点数量 */
    
    /* 参数节点专用 */
    VfsParamGet_t       paramGet;                    /* 参数读取函数 */
    VfsParamSet_t       paramSet;                    /* 参数写入函数 */
    const char         *paramDesc;                   /* 参数描述 */
    
    /* 设备/参数节点专用 */
    void               *userData;                    /* 用户私有数据 */
    void               *driver;                      /* 关联的驱动指针 */
} VfsNode_t;

/*******************************************************************************
 * 错误码定义
 ******************************************************************************/
typedef enum {
    VFS_OK = 0,              /* 成功 */
    VFS_ERR_NOT_FOUND,       /* 路径不存在 */
    VFS_ERR_NOT_DIR,         /* 不是目录 */
    VFS_ERR_NOT_PARAM,       /* 不是参数节点 */
    VFS_ERR_READ_ONLY,       /* 参数只读 */
    VFS_ERR_NO_MEMORY,       /* 内存不足 */
    VFS_ERR_NAME_TOO_LONG,   /* 名称过长 */
    VFS_ERR_DIR_FULL,        /* 目录已满 */
    VFS_ERR_ALREADY_EXISTS,  /* 节点已存在 */
    VFS_ERR_INVALID_PATH,    /* 无效路径 */
} VfsError_t;

/*******************************************************************************
 * 目录列举回调函数类型
 ******************************************************************************/
typedef void (*VfsListCallback_t)(VfsNode_t *node, void *userData);

/*******************************************************************************
 * 核心API函数
 ******************************************************************************/

/**
 * @brief  初始化虚拟文件系统（仅创建根节点）
 * @return VFS_OK成功，其他失败
 */
VfsError_t Vfs_Init(void);

/**
 * @brief  获取根节点
 * @return 根节点指针
 */
VfsNode_t* Vfs_GetRoot(void);

/**
 * @brief  获取当前工作目录节点
 * @return 当前目录节点指针
 */
VfsNode_t* Vfs_GetCwd(void);

/**
 * @brief  获取当前工作目录路径字符串
 * @param  buf: 输出缓冲区
 * @param  maxLen: 缓冲区大小
 * @return VFS_OK成功
 */
VfsError_t Vfs_GetCwdPath(char *buf, uint16_t maxLen);

/**
 * @brief  切换当前目录
 * @param  path: 目标路径（支持相对路径和绝对路径）
 * @return VFS_OK成功，其他失败
 */
VfsError_t Vfs_Cd(const char *path);

/**
 * @brief  根据路径查找节点
 * @param  path: 路径（绝对或相对）
 * @return 节点指针，NULL表示未找到
 */
VfsNode_t* Vfs_FindNode(const char *path);

/**
 * @brief  在指定目录下创建子目录
 * @param  parent: 父目录节点
 * @param  name: 目录名
 * @return 新创建的目录节点，NULL表示失败
 */
VfsNode_t* Vfs_CreateDir(VfsNode_t *parent, const char *name);

/**
 * @brief  在指定目录下创建参数节点
 * @param  parent: 父目录节点
 * @param  name: 参数名
 * @param  desc: 参数描述
 * @param  get: 读取回调
 * @param  set: 写入回调（NULL表示只读）
 * @param  userData: 用户数据
 * @return 新创建的参数节点，NULL表示失败
 */
VfsNode_t* Vfs_CreateParam(VfsNode_t *parent, const char *name, 
                            const char *desc,
                            VfsParamGet_t get, VfsParamSet_t set,
                            void *userData);

/**
 * @brief  在指定目录下创建设备节点
 * @param  parent: 父目录节点
 * @param  name: 设备名
 * @param  userData: 设备私有数据
 * @return 新创建的设备节点，NULL表示失败
 */
VfsNode_t* Vfs_CreateDevice(VfsNode_t *parent, const char *name, void *userData);

/**
 * @brief  在指定目录下创建通用节点
 * @param  parent: 父目录节点
 * @param  name: 节点名
 * @param  type: 节点类型
 * @param  userData: 用户数据
 * @return 新创建的节点，NULL表示失败
 */
VfsNode_t* Vfs_CreateNode(VfsNode_t *parent, const char *name, VfsNodeType_t type, void *userData);

/**
 * @brief  读取参数值
 * @param  node: 参数节点
 * @param  buf: 输出缓冲区
 * @param  maxLen: 缓冲区大小
 * @return 读取的字节数，-1错误，-2只写参数
 */
int Vfs_ReadParam(VfsNode_t *node, char *buf, uint16_t maxLen);

/**
 * @brief  写入参数值
 * @param  node: 参数节点
 * @param  value: 写入的值
 * @return VFS_OK成功
 */
VfsError_t Vfs_WriteParam(VfsNode_t *node, const char *value);

/**
 * @brief  列举目录内容
 * @param  node: 目录节点
 * @param  callback: 回调函数
 * @param  userData: 用户数据
 * @return VFS_OK成功
 */
VfsError_t Vfs_ListDir(VfsNode_t *node, VfsListCallback_t callback, void *userData);

/**
 * @brief  删除节点（递归删除子节点）
 * @param  node: 要删除的节点
 * @return VFS_OK成功
 */
VfsError_t Vfs_RemoveNode(VfsNode_t *node);

/**
 * @brief  获取节点类型名称字符串
 * @param  type: 节点类型
 * @return 类型名称字符串
 */
const char* Vfs_GetTypeName(VfsNodeType_t type);

/**
 * @brief  根据路径创建目录（递归创建）
 * @param  path: 绝对路径（如 "/bin/sys"）
 * @return 创建的目录节点，NULL表示失败
 */
VfsNode_t* Vfs_Mkdir(const char *path);

/**
 * @brief VFS参数节点定义结构体
 */
typedef struct {
    const char *name;    // 参数名
    const char *desc;    // 参数描述
    int (*get)(char *buf, uint16_t maxLen, void *userData); // 读回调
    int (*set)(const char *buf, void *userData);            // 写回调
    void *userData;     // 用户数据
} FsParamDef_t;

#define FS_PARAM_END {NULL, NULL, NULL, NULL, NULL}

#ifdef __cplusplus
}
#endif

#endif /* __VFS_H__ */
