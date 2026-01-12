/**
 *****************************************************************************
 * @file     vfs.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    虚拟文件系统核心实现
 *****************************************************************************
 */

#include <string.h>
#include <stdio.h>
#include "vfs.h"
#include "debug.h"

/*******************************************************************************
 * 静态变量
 ******************************************************************************/
static VfsNode_t g_NodePool[VFS_MAX_NODES];
static uint8_t   g_NodeUsed[VFS_MAX_NODES];
static uint8_t   g_NodeCount = 0;

static VfsNode_t *g_RootNode = NULL;
static VfsNode_t *g_CwdNode = NULL;
static bool       g_Initialized = FALSE;

/*******************************************************************************
 * 内部函数声明
 ******************************************************************************/
static VfsNode_t* AllocNode(void);
static void FreeNode(VfsNode_t *node);
static void InitNode(VfsNode_t *node, const char *name, VfsNodeType_t type);
static VfsNode_t* FindChildByName(VfsNode_t *parent, const char *name);
static int ParsePath(const char *path, char segments[][VFS_MAX_NAME_LEN], int maxSegments);
static void BuildPath(VfsNode_t *node, char *buf, uint16_t maxLen);

/*******************************************************************************
 * 内存管理
 ******************************************************************************/

static VfsNode_t* AllocNode(void)
{
    uint8_t i;
    for (i = 0; i < VFS_MAX_NODES; i++) {
        if (!g_NodeUsed[i]) {
            g_NodeUsed[i] = 1;
            g_NodeCount++;
            memset(&g_NodePool[i], 0, sizeof(VfsNode_t));
            return &g_NodePool[i];
        }
    }
    return NULL;
}

static void FreeNode(VfsNode_t *node)
{
    uint8_t i;
    if (!node) return;
    
    for (i = 0; i < VFS_MAX_NODES; i++) {
        if (&g_NodePool[i] == node && g_NodeUsed[i]) {
            g_NodeUsed[i] = 0;
            g_NodeCount--;
            return;
        }
    }
}

static void InitNode(VfsNode_t *node, const char *name, VfsNodeType_t type)
{
    if (!node) return;
    
    strncpy(node->name, name, VFS_MAX_NAME_LEN - 1);
    node->name[VFS_MAX_NAME_LEN - 1] = '\0';
    node->type = type;
    node->parent = NULL;
    node->childCount = 0;
    node->paramGet = NULL;
    node->paramSet = NULL;
    node->paramDesc = NULL;
    node->userData = NULL;
    node->driver = NULL;
    
    memset(node->children, 0, sizeof(node->children));
}

/*******************************************************************************
 * 路径解析
 ******************************************************************************/

static int ParsePath(const char *path, char segments[][VFS_MAX_NAME_LEN], int maxSegments)
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
            if (len > 0 && len < VFS_MAX_NAME_LEN) {
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
    
    /* 处理最后一段 */
    if (start < p) {
        len = p - start;
        if (len > 0 && len < VFS_MAX_NAME_LEN) {
            strncpy(segments[count], start, len);
            segments[count][len] = '\0';
            count++;
        }
    }
    
    return count;
}

static VfsNode_t* FindChildByName(VfsNode_t *parent, const char *name)
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

static void BuildPath(VfsNode_t *node, char *buf, uint16_t maxLen)
{
    char tempPath[VFS_MAX_PATH_LEN];
    VfsNode_t *current;
    int pos;
    int len;
    
    if (!node || !buf || maxLen == 0) return;
    
    if (node == g_RootNode) {
        strncpy(buf, "/", maxLen);
        return;
    }
    
    tempPath[VFS_MAX_PATH_LEN - 1] = '\0';
    pos = VFS_MAX_PATH_LEN - 1;
    
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
    
    if (pos >= VFS_MAX_PATH_LEN - 1) {
        strncpy(buf, "/", maxLen);
    } else {
        strncpy(buf, &tempPath[pos], maxLen - 1);
        buf[maxLen - 1] = '\0';
    }
}

/*******************************************************************************
 * 公共API实现
 ******************************************************************************/

VfsError_t Vfs_Init(void)
{
    if (g_Initialized) return VFS_OK;
    
    DBG("[VFS] Initializing...\n");
    
    /* 初始化内存池 */
    memset(g_NodePool, 0, sizeof(g_NodePool));
    memset(g_NodeUsed, 0, sizeof(g_NodeUsed));
    g_NodeCount = 0;
    
    /* 创建根节点 "/" */
    g_RootNode = AllocNode();
    if (!g_RootNode) {
        DBG("[VFS] ERROR: Failed to alloc root node!\n");
        return VFS_ERR_NO_MEMORY;
    }
    
    InitNode(g_RootNode, "/", VFS_NODE_DIR);
    g_CwdNode = g_RootNode;
    
    DBG("[VFS] Root created, VFS ready\n");
    g_Initialized = TRUE;
    return VFS_OK;
}

VfsNode_t* Vfs_GetRoot(void)
{
    return g_RootNode;
}

VfsNode_t* Vfs_GetCwd(void)
{
    return g_CwdNode;
}

VfsError_t Vfs_GetCwdPath(char *buf, uint16_t maxLen)
{
    if (!buf || maxLen == 0) return VFS_ERR_INVALID_PATH;
    if (!g_CwdNode) return VFS_ERR_NOT_FOUND;
    
    BuildPath(g_CwdNode, buf, maxLen);
    return VFS_OK;
}

VfsError_t Vfs_Cd(const char *path)
{
    VfsNode_t *target;
    char segments[8][VFS_MAX_NAME_LEN];
    int segCount;
    int i;
    VfsNode_t *current;
    
    if (!path) return VFS_ERR_INVALID_PATH;
    
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        g_CwdNode = g_RootNode;
        return VFS_OK;
    }
    
    segCount = ParsePath(path, segments, 8);
    if (segCount < 0) return VFS_ERR_INVALID_PATH;
    
    if (path[0] == '/') {
        current = g_RootNode;
    } else {
        current = g_CwdNode;
    }
    
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
                return VFS_ERR_NOT_FOUND;
            }
            if (target->type != VFS_NODE_DIR && target->type != VFS_NODE_DEV) {
                return VFS_ERR_NOT_DIR;
            }
            current = target;
        }
    }
    
    g_CwdNode = current;
    return VFS_OK;
}

VfsNode_t* Vfs_FindNode(const char *path)
{
    char segments[8][VFS_MAX_NAME_LEN];
    int segCount;
    int i;
    VfsNode_t *current;
    VfsNode_t *target;
    
    if (!path) return NULL;
    
    if (path[0] == '/' && path[1] == '\0') {
        return g_RootNode;
    }
    
    segCount = ParsePath(path, segments, 8);
    if (segCount < 0) return NULL;
    
    if (path[0] == '/') {
        current = g_RootNode;
    } else {
        current = g_CwdNode;
    }
    
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

VfsNode_t* Vfs_CreateDir(VfsNode_t *parent, const char *name)
{
    VfsNode_t *node;
    
    if (!parent || !name) return NULL;
    if (parent->type != VFS_NODE_DIR && parent->type != VFS_NODE_DEV) return NULL;
    if (strlen(name) >= VFS_MAX_NAME_LEN) return NULL;
    if (parent->childCount >= VFS_MAX_CHILDREN) return NULL;
    
    /* 检查是否已存在 */
    if (FindChildByName(parent, name)) return NULL;
    
    node = AllocNode();
    if (!node) return NULL;
    
    InitNode(node, name, VFS_NODE_DIR);
    node->parent = parent;
    parent->children[parent->childCount++] = node;
    
    return node;
}

VfsNode_t* Vfs_Mkdir(const char *path)
{
    char segments[8][VFS_MAX_NAME_LEN];
    int segCount;
    int i;
    VfsNode_t *current;
    VfsNode_t *child;
    
    if (!path || path[0] != '/') return NULL;
    
    segCount = ParsePath(path, segments, 8);
    if (segCount < 0) return NULL;
    
    current = g_RootNode;
    
    for (i = 0; i < segCount; i++) {
        child = FindChildByName(current, segments[i]);
        if (child) {
            if (child->type != VFS_NODE_DIR) {
                return NULL;  /* 路径中存在非目录节点 */
            }
            current = child;
        } else {
            /* 创建目录 */
            current = Vfs_CreateDir(current, segments[i]);
            if (!current) return NULL;
        }
    }
    
    return current;
}

VfsNode_t* Vfs_CreateParam(VfsNode_t *parent, const char *name,
                            const char *desc,
                            VfsParamGet_t get, VfsParamSet_t set,
                            void *userData)
{
    VfsNode_t *node;
    
    /* 参数至少需要get或set其中一个 */
    if (!parent || !name || (!get && !set)) {
        return NULL;
    }
    if (parent->type != VFS_NODE_DIR && parent->type != VFS_NODE_DEV) {
        return NULL;
    }
    if (strlen(name) >= VFS_MAX_NAME_LEN) {
        return NULL;
    }
    if (parent->childCount >= VFS_MAX_CHILDREN) {
        return NULL;
    }
    
    if (FindChildByName(parent, name)) {
        return NULL;
    }
    
    node = AllocNode();
    if (!node) {
        return NULL;
    }
    
    InitNode(node, name, VFS_NODE_PARAM);
    node->parent = parent;
    node->paramGet = get;
    node->paramSet = set;
    node->paramDesc = desc;
    node->userData = userData;
    parent->children[parent->childCount++] = node;
    
    return node;
}

VfsNode_t* Vfs_CreateDevice(VfsNode_t *parent, const char *name, void *userData)
{
    VfsNode_t *node;
    
    if (!parent || !name) return NULL;
    if (parent->type != VFS_NODE_DIR) return NULL;
    if (strlen(name) >= VFS_MAX_NAME_LEN) return NULL;
    if (parent->childCount >= VFS_MAX_CHILDREN) return NULL;
    
    if (FindChildByName(parent, name)) return NULL;
    
    node = AllocNode();
    if (!node) return NULL;
    
    InitNode(node, name, VFS_NODE_DEV);
    node->parent = parent;
    node->userData = userData;
    parent->children[parent->childCount++] = node;
    
    return node;
}

VfsNode_t* Vfs_CreateNode(VfsNode_t *parent, const char *name, VfsNodeType_t type, void *userData)
{
    if (!parent || !name) return NULL;
    if (parent->childCount >= VFS_MAX_CHILDREN) return NULL;
    VfsNode_t *node = AllocNode();
    if (!node) return NULL;
    InitNode(node, name, type);
    node->parent = parent;
    node->userData = userData;
    parent->children[parent->childCount++] = node;
    return node;
}

int Vfs_ReadParam(VfsNode_t *node, char *buf, uint16_t maxLen)
{
    if (!node || !buf || maxLen == 0) return -1;
    if (node->type != VFS_NODE_PARAM) return -1;
    if (!node->paramGet) return -2;  /* -2 表示只写参数 */
    
    return node->paramGet(buf, maxLen, node->userData);
}

VfsError_t Vfs_WriteParam(VfsNode_t *node, const char *value)
{
    if (!node || !value) return VFS_ERR_INVALID_PATH;
    if (node->type != VFS_NODE_PARAM) return VFS_ERR_NOT_PARAM;
    if (!node->paramSet) return VFS_ERR_READ_ONLY;
    
    if (node->paramSet(value, node->userData) == 0) {
        return VFS_OK;
    }
    return VFS_ERR_INVALID_PATH;
}

VfsError_t Vfs_ListDir(VfsNode_t *node, VfsListCallback_t callback, void *userData)
{
    uint8_t i;
    
    if (!node || !callback) return VFS_ERR_INVALID_PATH;
    if (node->type != VFS_NODE_DIR && node->type != VFS_NODE_DEV) {
        return VFS_ERR_NOT_DIR;
    }
    
    for (i = 0; i < node->childCount; i++) {
        if (node->children[i]) {
            callback(node->children[i], userData);
        }
    }
    
    return VFS_OK;
}

const char* Vfs_GetTypeName(VfsNodeType_t type)
{
    switch (type) {
        case VFS_NODE_DIR:   return "DIR";
        case VFS_NODE_PARAM: return "PARAM";
        case VFS_NODE_DEV:   return "DEV";
        default:             return "???";
    }
}

VfsError_t Vfs_RemoveNode(VfsNode_t *node)
{
    uint8_t i, j;
    VfsNode_t *parent;
    
    if (!node) return VFS_ERR_NOT_FOUND;
    if (node == g_RootNode) return VFS_ERR_INVALID_PATH;
    
    /* 递归删除所有子节点 */
    for (i = 0; i < node->childCount; i++) {
        if (node->children[i]) {
            Vfs_RemoveNode(node->children[i]);
        }
    }
    
    parent = node->parent;
    if (parent) {
        for (i = 0; i < parent->childCount; i++) {
            if (parent->children[i] == node) {
                for (j = i; j < parent->childCount - 1; j++) {
                    parent->children[j] = parent->children[j + 1];
                }
                parent->children[parent->childCount - 1] = NULL;
                parent->childCount--;
                break;
            }
        }
    }
    
    if (g_CwdNode == node) {
        g_CwdNode = parent ? parent : g_RootNode;
    }
    
    FreeNode(node);
    
    return VFS_OK;
}
