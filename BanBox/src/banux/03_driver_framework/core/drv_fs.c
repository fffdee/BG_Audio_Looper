/**
 *****************************************************************************
 * @file     drv_fs.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    设备文件系统实现
 *****************************************************************************
 */

#include <string.h>
#include <stdio.h>
#include "drv_fs.h"
#include "debug.h"  /* For DBG macro */

/*******************************************************************************
 * 静态变�?
 ******************************************************************************/
/* 节点内存�?*/
static FsNode_t g_NodePool[DRV_FS_MAX_NODES];
static uint8_t  g_NodeUsed[DRV_FS_MAX_NODES];
static uint8_t  g_NodeCount = 0;

/* 根节点和当前目录 */
static FsNode_t *g_RootNode = NULL;
static FsNode_t *g_CwdNode = NULL;
static bool      g_Initialized = FALSE;

/* 标准目录节点（方便快速访问） */
static FsNode_t *g_DriverDir = NULL;
static FsNode_t *g_SpiDir = NULL;
static FsNode_t *g_I2cDir = NULL;
static FsNode_t *g_I2sDir = NULL;
static FsNode_t *g_SdioDir = NULL;
static FsNode_t *g_PowerDir = NULL;
static FsNode_t *g_UsbDir = NULL;

/*******************************************************************************
 * 内部函数声明
 ******************************************************************************/
static FsNode_t* AllocNode(void);
static void FreeNode(FsNode_t *node);
static void InitNode(FsNode_t *node, const char *name, FsNodeType_t type);
static FsNode_t* FindChildByName(FsNode_t *parent, const char *name);
static int ParsePath(const char *path, char segments[][DRV_FS_MAX_NAME_LEN], int maxSegments);
static void BuildPath(FsNode_t *node, char *buf, uint16_t maxLen);

/*******************************************************************************
 * 内存管理
 ******************************************************************************/

static FsNode_t* AllocNode(void)
{
    uint8_t i;
    for (i = 0; i < DRV_FS_MAX_NODES; i++) {
        if (!g_NodeUsed[i]) {
            g_NodeUsed[i] = 1;
            g_NodeCount++;
            memset(&g_NodePool[i], 0, sizeof(FsNode_t));
            return &g_NodePool[i];
        }
    }
    return NULL;
}

static void FreeNode(FsNode_t *node)
{
    uint8_t i;
    if (!node) return;
    
    for (i = 0; i < DRV_FS_MAX_NODES; i++) {
        if (&g_NodePool[i] == node && g_NodeUsed[i]) {
            g_NodeUsed[i] = 0;
            g_NodeCount--;
            return;
        }
    }
}

static void InitNode(FsNode_t *node, const char *name, FsNodeType_t type)
{
    if (!node) return;
    
    strncpy(node->name, name, DRV_FS_MAX_NAME_LEN - 1);
    node->name[DRV_FS_MAX_NAME_LEN - 1] = '\0';
    node->type = type;
    node->parent = NULL;
    node->childCount = 0;
    node->paramGet = NULL;
    node->paramSet = NULL;
    node->paramDesc = NULL;
    node->deviceData = NULL;
    node->driver = NULL;
    
    memset(node->children, 0, sizeof(node->children));
}

/*******************************************************************************
 * 路径解析
 ******************************************************************************/

/**
 * @brief  解析路径为段数组
 * @param  path: 输入路径
 * @param  segments: 输出段数�?
 * @param  maxSegments: 最大段�?
 * @return 段数量，-1表示错误
 */
static int ParsePath(const char *path, char segments[][DRV_FS_MAX_NAME_LEN], int maxSegments)
{
    int count = 0;
    const char *start = path;
    const char *p = path;
    int len;
    
    if (!path || !segments) return -1;
    
    /* 跳过开头的斜杠 */
    while (*p == '/') p++;
    start = p;
    
    while (*p && count < maxSegments) {
        if (*p == '/') {
            len = p - start;
            if (len > 0 && len < DRV_FS_MAX_NAME_LEN) {
                strncpy(segments[count], start, len);
                segments[count][len] = '\0';
                count++;
            }
            while (*p == '/') p++;
            start = p;
        } else {
            p++;
        }
    }
    
    /* 处理最后一�?*/
    if (start < p) {
        len = p - start;
        if (len > 0 && len < DRV_FS_MAX_NAME_LEN) {
            strncpy(segments[count], start, len);
            segments[count][len] = '\0';
            count++;
        }
    }
    
    return count;
}

/**
 * @brief  根据名称查找子节�?
 */
static FsNode_t* FindChildByName(FsNode_t *parent, const char *name)
{
    uint8_t i;
    if (!parent || !name) return NULL;
    
    for (i = 0; i < parent->childCount; i++) {
        if (parent->children[i] && 
            strcmp(parent->children[i]->name, name) == 0) {
            return parent->children[i];
        }
    }
    return NULL;
}

/**
 * @brief  构建节点的完整路�?
 */
static void BuildPath(FsNode_t *node, char *buf, uint16_t maxLen)
{
    char tempPath[DRV_FS_MAX_PATH_LEN];
    FsNode_t *current;
    int pos;
    int len;
    
    if (!node || !buf || maxLen == 0) return;
    
    /* 根节点特殊处�?*/
    if (node == g_RootNode) {
        strncpy(buf, "/", maxLen);
        return;
    }
    
    /* 从当前节点向上遍历构建路�?*/
    tempPath[DRV_FS_MAX_PATH_LEN - 1] = '\0';
    pos = DRV_FS_MAX_PATH_LEN - 1;
    
    current = node;
    while (current && current != g_RootNode) {
        len = strlen(current->name);
        pos -= len;
        if (pos < 1) break;
        memcpy(&tempPath[pos], current->name, len);
        pos--;
        tempPath[pos] = '/';
        current = current->parent;
    }
    
    /* 如果路径为空，则为根目录 */
    if (pos >= DRV_FS_MAX_PATH_LEN - 1) {
        strncpy(buf, "/", maxLen);
    } else {
        strncpy(buf, &tempPath[pos], maxLen - 1);
        buf[maxLen - 1] = '\0';
    }
}

/*******************************************************************************
 * 公共API实现
 ******************************************************************************/

FsError_t DrvFs_Init(void)
{
    if (g_Initialized) return FS_OK;
    
    DBG("[DrvFs] Step 1: Clearing node pool (size=%d)...\n", sizeof(g_NodePool));
    /* 初始化内存池 */
    memset(g_NodePool, 0, sizeof(g_NodePool));
    memset(g_NodeUsed, 0, sizeof(g_NodeUsed));
    g_NodeCount = 0;
    DBG("[DrvFs] Step 1: OK\n");
    
    DBG("[DrvFs] Step 2: Creating root node...\n");
    /* 创建根节�?"/" */
    g_RootNode = AllocNode();
    if (!g_RootNode) {
        DBG("[DrvFs] ERROR: Failed to alloc root node!\n");
        return FS_ERR_NO_MEMORY;
    }
    DBG("[DrvFs] Step 2a: Root allocated at %p\n", g_RootNode);
    
    InitNode(g_RootNode, "/", FS_NODE_DIR);
    g_CwdNode = g_RootNode;
    DBG("[DrvFs] Step 2: OK\n");
    
    /* 创建标准目录结构 */
    DBG("[DrvFs] Step 3: Creating /driver...\n");
    g_DriverDir = DrvFs_CreateDir(g_RootNode, "driver");
    if (!g_DriverDir) {
        DBG("[DrvFs] ERROR: Failed to create /driver\n");
        return FS_ERR_NO_MEMORY;
    }
    DBG("[DrvFs] Step 3: OK\n");
    
    DBG("[DrvFs] Step 4: Creating /driver/spi...\n");
    g_SpiDir = DrvFs_CreateDir(g_DriverDir, "spi");
    if (!g_SpiDir) return FS_ERR_NO_MEMORY;
    
    DBG("[DrvFs] Step 5: Creating /driver/i2c...\n");
    g_I2cDir = DrvFs_CreateDir(g_DriverDir, "i2c");
    if (!g_I2cDir) return FS_ERR_NO_MEMORY;
    
    DBG("[DrvFs] Step 6: Creating /driver/i2s...\n");
    g_I2sDir = DrvFs_CreateDir(g_DriverDir, "i2s");
    if (!g_I2sDir) return FS_ERR_NO_MEMORY;
    
    DBG("[DrvFs] Step 7: Creating /driver/sdio...\n");
    g_SdioDir = DrvFs_CreateDir(g_DriverDir, "sdio");
    if (!g_SdioDir) return FS_ERR_NO_MEMORY;
    
    DBG("[DrvFs] Step 8: Creating /driver/power...\n");
    g_PowerDir = DrvFs_CreateDir(g_DriverDir, "power");
    if (!g_PowerDir) return FS_ERR_NO_MEMORY;
    
    DBG("[DrvFs] Step 9: Creating /driver/usb...\n");
    g_UsbDir = DrvFs_CreateDir(g_DriverDir, "usb");
    if (!g_UsbDir) return FS_ERR_NO_MEMORY;
    
    DBG("[DrvFs] All directories created successfully\n");
    g_Initialized = TRUE;
    return FS_OK;
}

FsNode_t* DrvFs_GetRoot(void)
{
    return g_RootNode;
}

FsNode_t* DrvFs_GetCwd(void)
{
    return g_CwdNode;
}

FsError_t DrvFs_GetCwdPath(char *buf, uint16_t maxLen)
{
    if (!buf || maxLen == 0) return FS_ERR_INVALID_PATH;
    if (!g_CwdNode) return FS_ERR_NOT_FOUND;
    
    BuildPath(g_CwdNode, buf, maxLen);
    return FS_OK;
}

FsError_t DrvFs_Cd(const char *path)
{
    FsNode_t *target;
    char segments[8][DRV_FS_MAX_NAME_LEN];
    int segCount;
    int i;
    FsNode_t *current;
    
    if (!path) return FS_ERR_INVALID_PATH;
    
    /* 空路径或单独�?"/" 切换到根目录 */
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        g_CwdNode = g_RootNode;
        return FS_OK;
    }
    
    /* 解析路径 */
    segCount = ParsePath(path, segments, 8);
    if (segCount < 0) return FS_ERR_INVALID_PATH;
    
    /* 确定起始目录 */
    if (path[0] == '/') {
        current = g_RootNode;
    } else {
        current = g_CwdNode;
    }
    
    /* 逐段解析 */
    for (i = 0; i < segCount; i++) {
        if (strcmp(segments[i], "..") == 0) {
            /* 回到上级目录 */
            if (current->parent) {
                current = current->parent;
            }
        } else if (strcmp(segments[i], ".") == 0) {
            /* 当前目录，不�?*/
        } else {
            /* 查找子节�?*/
            target = FindChildByName(current, segments[i]);
            if (!target) {
                return FS_ERR_NOT_FOUND;
            }
            /* 只能cd到目录或设备节点 */
            if (target->type != FS_NODE_DIR && target->type != FS_NODE_DEV) {
                return FS_ERR_NOT_DIR;
            }
            current = target;
        }
    }
    
    g_CwdNode = current;
    return FS_OK;
}

FsNode_t* DrvFs_FindNode(const char *path)
{
    char segments[8][DRV_FS_MAX_NAME_LEN];
    int segCount;
    int i;
    FsNode_t *current;
    FsNode_t *target;
    
    if (!path) return NULL;
    
    /* 根路�?*/
    if (path[0] == '/' && path[1] == '\0') {
        return g_RootNode;
    }
    
    /* 解析路径 */
    segCount = ParsePath(path, segments, 8);
    if (segCount < 0) return NULL;
    
    /* 确定起始目录 */
    if (path[0] == '/') {
        current = g_RootNode;
    } else {
        current = g_CwdNode;
    }
    
    /* 逐段解析 */
    for (i = 0; i < segCount; i++) {
        if (strcmp(segments[i], "..") == 0) {
            if (current->parent) {
                current = current->parent;
            }
        } else if (strcmp(segments[i], ".") == 0) {
            /* 当前目录 */
        } else {
            target = FindChildByName(current, segments[i]);
            if (!target) {
                return NULL;
            }
            current = target;
        }
    }
    
    return current;
}

FsNode_t* DrvFs_CreateDir(FsNode_t *parent, const char *name)
{
    FsNode_t *node;
    
    if (!parent || !name) return NULL;
    if (parent->type != FS_NODE_DIR && parent->type != FS_NODE_DEV) return NULL;
    if (strlen(name) >= DRV_FS_MAX_NAME_LEN) return NULL;
    if (parent->childCount >= DRV_FS_MAX_CHILDREN) return NULL;
    
    /* 检查是否已存在 */
    if (FindChildByName(parent, name)) return NULL;
    
    /* 分配节点 */
    node = AllocNode();
    if (!node) return NULL;
    
    InitNode(node, name, FS_NODE_DIR);
    node->parent = parent;
    parent->children[parent->childCount++] = node;
    
    return node;
}

FsNode_t* DrvFs_CreateParam(FsNode_t *parent, const char *name,
                             const char *desc,
                             FsParamGet_t get, FsParamSet_t set,
                             void *userData)
{
    FsNode_t *node;
    
    DBG("[CreateParam] name='%s', parent=%p, childCount=%d\n", 
        name ? name : "NULL", parent, parent ? parent->childCount : -1);
    
    /* 参数至少需要get或set其中一个 */
    if (!parent || !name || (!get && !set)) {
        DBG("[CreateParam] ERROR: Invalid args (parent=%p, name=%p, get=%p, set=%p)\n", 
            parent, name, get, set);
        return NULL;
    }
    if (parent->type != FS_NODE_DIR && parent->type != FS_NODE_DEV) {
        DBG("[CreateParam] ERROR: Parent not DIR/DEV (type=%d)\n", parent->type);
        return NULL;
    }
    if (strlen(name) >= DRV_FS_MAX_NAME_LEN) {
        DBG("[CreateParam] ERROR: Name too long (%d >= %d)\n", strlen(name), DRV_FS_MAX_NAME_LEN);
        return NULL;
    }
    if (parent->childCount >= DRV_FS_MAX_CHILDREN) {
        DBG("[CreateParam] ERROR: Too many children (%d >= %d)\n", parent->childCount, DRV_FS_MAX_CHILDREN);
        return NULL;
    }
    
    /* 检查是否已存在 */
    if (FindChildByName(parent, name)) {
        DBG("[CreateParam] ERROR: Child '%s' already exists\n", name);
        return NULL;
    }
    
    /* 分配节点 */
    node = AllocNode();
    if (!node) {
        DBG("[CreateParam] ERROR: AllocNode failed (nodeCount=%d)\n", g_NodeCount);
        return NULL;
    }
    
    InitNode(node, name, FS_NODE_PARAM);
    node->parent = parent;
    node->paramGet = get;
    node->paramSet = set;
    node->paramDesc = desc;
    node->deviceData = userData;
    parent->children[parent->childCount++] = node;
    
    DBG("[CreateParam] Created '%s' at %p (parent children=%d)\n", name, node, parent->childCount);
    return node;
}

FsNode_t* DrvFs_CreateDevice(FsNode_t *parent, const char *name, void *deviceData)
{
    FsNode_t *node;
    
    if (!parent || !name) return NULL;
    if (parent->type != FS_NODE_DIR) return NULL;
    if (strlen(name) >= DRV_FS_MAX_NAME_LEN) return NULL;
    if (parent->childCount >= DRV_FS_MAX_CHILDREN) return NULL;
    
    /* 检查是否已存在 */
    if (FindChildByName(parent, name)) return NULL;
    
    /* 分配节点 */
    node = AllocNode();
    if (!node) return NULL;
    
    InitNode(node, name, FS_NODE_DEV);
    node->parent = parent;
    node->deviceData = deviceData;
    parent->children[parent->childCount++] = node;
    
    return node;
}

int DrvFs_ReadParam(FsNode_t *node, char *buf, uint16_t maxLen)
{
    if (!node || !buf || maxLen == 0) return -1;
    if (node->type != FS_NODE_PARAM) return -1;
    if (!node->paramGet) return -2;  // -2 表示只写参数
    
    return node->paramGet(buf, maxLen, node->deviceData);
}

FsError_t DrvFs_WriteParam(FsNode_t *node, const char *value)
{
    if (!node || !value) return FS_ERR_INVALID_PATH;
    if (node->type != FS_NODE_PARAM) return FS_ERR_NOT_PARAM;
    if (!node->paramSet) return FS_ERR_READ_ONLY;
    
    if (node->paramSet(value, node->deviceData) == 0) {
        return FS_OK;
    }
    return FS_ERR_INVALID_PATH;
}

FsError_t DrvFs_ListDir(FsNode_t *node, FsListCallback_t callback, void *userData)
{
    uint8_t i;
    
    if (!node || !callback) return FS_ERR_INVALID_PATH;
    if (node->type != FS_NODE_DIR && node->type != FS_NODE_DEV) {
        return FS_ERR_NOT_DIR;
    }
    
    for (i = 0; i < node->childCount; i++) {
        if (node->children[i]) {
            callback(node->children[i], userData);
        }
    }
    
    return FS_OK;
}

const char* DrvFs_GetTypeName(FsNodeType_t type)
{
    switch (type) {
        case FS_NODE_DIR:   return "DIR";
        case FS_NODE_PARAM: return "PARAM";
        case FS_NODE_DEV:   return "DEV";
        default:            return "???";
    }
}

FsError_t DrvFs_RemoveNode(FsNode_t *node)
{
    uint8_t i, j;
    FsNode_t *parent;
    
    if (!node) return FS_ERR_NOT_FOUND;
    if (node == g_RootNode) return FS_ERR_INVALID_PATH;  /* 不能删除根节�?*/
    
    /* 递归删除所有子节点 */
    for (i = 0; i < node->childCount; i++) {
        if (node->children[i]) {
            DrvFs_RemoveNode(node->children[i]);
        }
    }
    
    /* 从父节点中移�?*/
    parent = node->parent;
    if (parent) {
        for (i = 0; i < parent->childCount; i++) {
            if (parent->children[i] == node) {
                /* 移动后面的元�?*/
                for (j = i; j < parent->childCount - 1; j++) {
                    parent->children[j] = parent->children[j + 1];
                }
                parent->children[parent->childCount - 1] = NULL;
                parent->childCount--;
                break;
            }
        }
    }
    
    /* 如果删除的是当前目录，切换到父目�?*/
    if (g_CwdNode == node) {
        g_CwdNode = parent ? parent : g_RootNode;
    }
    
    /* 释放节点 */
    FreeNode(node);
    
    return FS_OK;
}

/*******************************************************************************
 * 获取标准目录节点（供驱动注册使用�?
 ******************************************************************************/

FsNode_t* DrvFs_GetDriverDir(void)
{
    return g_DriverDir;
}

FsNode_t* DrvFs_GetSpiDir(void)
{
    return g_SpiDir;
}

FsNode_t* DrvFs_GetI2cDir(void)
{
    return g_I2cDir;
}

FsNode_t* DrvFs_GetI2sDir(void)
{
    return g_I2sDir;
}

FsNode_t* DrvFs_GetSdioDir(void)
{
    return g_SdioDir;
}


FsNode_t* DrvFs_GetUsbDir(void)
{
    return g_UsbDir;
}
FsNode_t* DrvFs_GetPowerDir(void)
{
    return g_PowerDir;
}
