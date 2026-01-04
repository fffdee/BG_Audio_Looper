/**
 *****************************************************************************
 * @file     drv_fs.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    设备文件系统 - 类Linux树形目录结构
 *****************************************************************************
 * @attention
 *
 * 本模块实现类Linux设备文件系统，提供：
 * 1. 树形目录结构（/driver/spi/st7735/param1）
 * 2. 节点类型：目录节点(DIR) / 参数节点(PARAM) / 设备节点(DEV)
 * 3. 路径解析与导航
 * 4. 与Shell命令系统绑定（cd/pwd/ls/cat/echo）
 *
 * 目录结构示例：
 *   /
 *   └── driver
 *       ├── spi
 *       │   ├── st7735
 *       │   │   ├── name      (参数：驱动名称)
 *       │   │   ├── width     (参数：LCD宽度)
 *       │   │   └── height    (参数：LCD高度)
 *       │   └── w25q64
 *       │       ├── name
 *       │       └── capacity
 *       ├── i2c
 *       ├── i2s
 *       └── sdio
 *
 *****************************************************************************
 */

#ifndef __DRV_FS_H__
#define __DRV_FS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

/*******************************************************************************
 * 配置定义 - 针对嵌入式系统优化，减少内存占用
 ******************************************************************************/
#define DRV_FS_MAX_PATH_LEN     64      /* 最大路径长度 (减小) */
#define DRV_FS_MAX_NAME_LEN     16      /* 节点名称最大长度 (减小) */
#define DRV_FS_MAX_CHILDREN     8       /* 每个目录最大子节点数 (减小) */
#define DRV_FS_MAX_PARAM_LEN    32      /* 参数值最大长度 (减小) */
#define DRV_FS_MAX_NODES        48      /* 系统最大节点数：基础8 + 4驱动约35 = 需要48 */

/*******************************************************************************
 * 节点类型定义
 ******************************************************************************/
typedef enum {
    FS_NODE_DIR = 0,        /* 目录节点 */
    FS_NODE_PARAM,          /* 参数节点（可读写） */
    FS_NODE_DEV,            /* 设备节点（关联驱动） */
} FsNodeType_t;

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
typedef int (*FsParamGet_t)(char *buf, uint16_t maxLen, void *userData);

/**
 * @brief  参数写入回调
 * @param  value: 写入的值字符串
 * @param  userData: 用户数据（设备私有数据）
 * @return 0成功，-1失败
 */
typedef int (*FsParamSet_t)(const char *value, void *userData);

/*******************************************************************************
 * 参数节点定义（用于驱动注册时声明参数列表）
 ******************************************************************************/
typedef struct {
    const char     *name;       /* 参数名称 */
    const char     *desc;       /* 参数描述 */
    FsParamGet_t    get;        /* 读取回调 */
    FsParamSet_t    set;        /* 写入回调（NULL表示只读） */
} FsParamDef_t;

/*******************************************************************************
 * 文件系统节点结构（树形结构）
 ******************************************************************************/
typedef struct FsNode {
    char                name[DRV_FS_MAX_NAME_LEN];  /* 节点名称 */
    FsNodeType_t        type;                        /* 节点类型 */
    struct FsNode      *parent;                      /* 父节点 */
    struct FsNode      *children[DRV_FS_MAX_CHILDREN]; /* 子节点数组 */
    uint8_t             childCount;                  /* 子节点数量 */
    
    /* 参数节点专用 */
    FsParamGet_t        paramGet;                    /* 参数读取函数 */
    FsParamSet_t        paramSet;                    /* 参数写入函数 */
    const char         *paramDesc;                   /* 参数描述 */
    
    /* 设备节点专用 */
    void               *deviceData;                  /* 设备私有数据 */
    void               *driver;                      /* 关联的驱动指针 */
} FsNode_t;

/*******************************************************************************
 * 错误码定义
 ******************************************************************************/
typedef enum {
    FS_OK = 0,              /* 成功 */
    FS_ERR_NOT_FOUND,       /* 路径不存在 */
    FS_ERR_NOT_DIR,         /* 不是目录 */
    FS_ERR_NOT_PARAM,       /* 不是参数节点 */
    FS_ERR_READ_ONLY,       /* 参数只读 */
    FS_ERR_NO_MEMORY,       /* 内存不足 */
    FS_ERR_NAME_TOO_LONG,   /* 名称过长 */
    FS_ERR_DIR_FULL,        /* 目录已满 */
    FS_ERR_ALREADY_EXISTS,  /* 节点已存在 */
    FS_ERR_INVALID_PATH,    /* 无效路径 */
} FsError_t;

/*******************************************************************************
 * 公共API函数
 ******************************************************************************/

/**
 * @brief  初始化设备文件系统
 * @return FS_OK成功，其他失败
 * @note   创建根目录 "/" 和标准目录结构
 */
FsError_t DrvFs_Init(void);

/**
 * @brief  获取根节点
 * @return 根节点指针
 */
FsNode_t* DrvFs_GetRoot(void);

/**
 * @brief  获取当前工作目录节点
 * @return 当前目录节点指针
 */
FsNode_t* DrvFs_GetCwd(void);

/**
 * @brief  获取当前工作目录路径字符串
 * @param  buf: 输出缓冲区
 * @param  maxLen: 缓冲区大小
 * @return FS_OK成功
 */
FsError_t DrvFs_GetCwdPath(char *buf, uint16_t maxLen);

/**
 * @brief  切换当前目录
 * @param  path: 目标路径（支持相对路径和绝对路径）
 * @return FS_OK成功，其他失败
 * @note   支持 ".." 回退上级目录
 */
FsError_t DrvFs_Cd(const char *path);

/**
 * @brief  根据路径查找节点
 * @param  path: 路径（绝对或相对）
 * @return 节点指针，NULL表示不存在
 */
FsNode_t* DrvFs_FindNode(const char *path);

/**
 * @brief  在指定目录下创建子目录
 * @param  parent: 父目录节点
 * @param  name: 目录名
 * @return 新创建的目录节点，NULL表示失败
 */
FsNode_t* DrvFs_CreateDir(FsNode_t *parent, const char *name);

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
FsNode_t* DrvFs_CreateParam(FsNode_t *parent, const char *name, 
                             const char *desc,
                             FsParamGet_t get, FsParamSet_t set,
                             void *userData);

/**
 * @brief  在指定目录下创建设备节点
 * @param  parent: 父目录节点
 * @param  name: 设备名
 * @param  deviceData: 设备私有数据
 * @return 新创建的设备节点，NULL表示失败
 */
FsNode_t* DrvFs_CreateDevice(FsNode_t *parent, const char *name, void *deviceData);

/**
 * @brief  读取参数值
 * @param  node: 参数节点
 * @param  buf: 输出缓冲区
 * @param  maxLen: 缓冲区大小
 * @return 读取的字节数，-1表示错误
 */
int DrvFs_ReadParam(FsNode_t *node, char *buf, uint16_t maxLen);

/**
 * @brief  写入参数值
 * @param  node: 参数节点
 * @param  value: 写入的值
 * @return FS_OK成功，其他失败
 */
FsError_t DrvFs_WriteParam(FsNode_t *node, const char *value);

/**
 * @brief  列出目录内容
 * @param  node: 目录节点
 * @param  callback: 回调函数，对每个子节点调用
 * @param  userData: 用户数据传递给回调
 * @return FS_OK成功
 */
typedef void (*FsListCallback_t)(FsNode_t *node, void *userData);
FsError_t DrvFs_ListDir(FsNode_t *node, FsListCallback_t callback, void *userData);

/**
 * @brief  获取节点类型字符串
 * @param  type: 节点类型
 * @return 类型字符串 "DIR"/"PARAM"/"DEV"
 */
const char* DrvFs_GetTypeName(FsNodeType_t type);

/**
 * @brief  删除节点（及其所有子节点）
 * @param  node: 要删除的节点
 * @return FS_OK成功
 */
FsError_t DrvFs_RemoveNode(FsNode_t *node);

/*******************************************************************************
 * 获取标准目录节点（供驱动注册使用）
 ******************************************************************************/

/**
 * @brief  获取driver目录节点
 */
FsNode_t* DrvFs_GetDriverDir(void);

/**
 * @brief  获取SPI目录节点
 */
FsNode_t* DrvFs_GetSpiDir(void);

/**
 * @brief  获取I2C目录节点
 */
FsNode_t* DrvFs_GetI2cDir(void);

/**
 * @brief  获取I2S目录节点
 */
FsNode_t* DrvFs_GetI2sDir(void);

/**
 * @brief  获取SDIO目录节点
 */
FsNode_t* DrvFs_GetSdioDir(void);

/**
 * @brief  获取Power目录节点
 */
FsNode_t* DrvFs_GetPowerDir(void);

/**
 * @brief  获取USB目录节点
 */
FsNode_t* DrvFs_GetUsbDir(void);

/*******************************************************************************
 * 便捷宏定义
 ******************************************************************************/

/* 定义参数项（用于驱动注册） */
#define FS_PARAM_DEF(n, d, g, s)    { .name = n, .desc = d, .get = g, .set = s }

/* 参数列表结束标记 */
#define FS_PARAM_END()              { .name = NULL }

/* 计算参数数量 */
#define FS_PARAM_COUNT(params)      ((sizeof(params) / sizeof(params[0])) - 1)

#ifdef __cplusplus
}
#endif

#endif /* __DRV_FS_H__ */
