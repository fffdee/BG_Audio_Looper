
#ifndef __VFS_H__
#define __VFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"
#define VFS_EN
/*******************************************************************************
 * Configuration definitions - optimized for embedded environment, reduce memory usage
 ******************************************************************************/
#define VFS_MAX_PATH_LEN     64      /* Maximum path length */
#define VFS_MAX_NAME_LEN     16      /* Maximum node name length */
#define VFS_MAX_CHILDREN     32      /* Maximum number of child nodes per directory (increased to support complex node topology) */
#define VFS_MAX_PARAM_LEN    32      /* Maximum parameter value length */
#define VFS_MAX_NODES        256     /* Maximum number of nodes in the system (increased to support complex VFS topology) */

/*******************************************************************************
 * Node type definitions
 ******************************************************************************/
typedef enum {
    VFS_NODE_DIR = 0,        /* Directory node */
    VFS_NODE_PARAM,          /* Parameter node (readable/writable) */
    VFS_NODE_DEV,            /* Device node (related to device) */
    VFS_NODE_CMD,            /* Command node (bin/command) */
} VfsNodeType_t;

/*******************************************************************************
 * Parameter read/write callback function types
 ******************************************************************************/
/**
 * @brief  Parameter read callback
 * @param  buf: Output buffer
 * @param  maxLen: Maximum buffer length
 * @param  userData: User data (device-specific data)
 * @return Actual read length, -1 indicates error
 */
typedef int (*VfsParamGet_t)(char *buf, uint16_t maxLen, void *userData);

/**
 * @brief  Parameter write callback
 * @param  value: Input string value
 * @param  userData: User data (device-specific data)
 * @return 0 success, 1 failure
 */
typedef int (*VfsParamSet_t)(const char *value, void *userData);

/*******************************************************************************
 * File system node structure (tree structure)
 ******************************************************************************/
typedef struct VfsNode {
    char                name[VFS_MAX_NAME_LEN];      /* Node name */
    VfsNodeType_t       type;                        /* Node type */
    struct VfsNode     *parent;                      /* Parent node */
    struct VfsNode     *children[VFS_MAX_CHILDREN];  /* Child nodes array */
    uint8_t             childCount;                  /* Number of child nodes */
    
    /* For parameter nodes */
    VfsParamGet_t       paramGet;                    /* Parameter read function */
    VfsParamSet_t       paramSet;                    /* Parameter write function */
    const char         *paramDesc;                   /* Parameter description */
    
    /* For device/parameter nodes */
    void               *userData;                    /* User private data */
    void               *driver;                      /* Associated driver pointer */
} VfsNode_t;

/*******************************************************************************
 * Error code definitions
 ******************************************************************************/
typedef enum {
    VFS_OK = 0,              /* Success */
    VFS_ERR_NOT_FOUND,       /* Path does not exist */
    VFS_ERR_NOT_DIR,         /* Not a directory */
    VFS_ERR_NOT_PARAM,       /* Not a parameter node */
    VFS_ERR_READ_ONLY,       /* Parameter read-only */
    VFS_ERR_NO_MEMORY,       /* Out of memory */
    VFS_ERR_NAME_TOO_LONG,   /* Name too long */
    VFS_ERR_DIR_FULL,        /* Directory full */
    VFS_ERR_ALREADY_EXISTS,  /* Node already exists */
    VFS_ERR_INVALID_PATH,    /* Invalid path */
} VfsError_t;

/*******************************************************************************
 * Directory listing callback function type
 ******************************************************************************/
typedef void (*VfsListCallback_t)(VfsNode_t *node, void *userData);

/*******************************************************************************
 * Public API functions
 ******************************************************************************/

/**
 * @brief  Initialize virtual file system (create root node)
 * @return VFS_OK on success, other error codes
 */
VfsError_t Vfs_Init(void);

/**
 * @brief  Get root node
 * @return Root node pointer
 */
VfsNode_t* Vfs_GetRoot(void);

/**
 * @brief  Get current working directory node
 * @return Current directory node pointer
 */
VfsNode_t* Vfs_GetCwd(void);

/**
 * @brief  Get current working directory path string
 * @param  buf: Output buffer
 * @param  maxLen: Buffer size
 * @return VFS_OK on success
 */
VfsError_t Vfs_GetCwdPath(char *buf, uint16_t maxLen);

/**
 * @brief  Change current directory
 * @param  path: Target path (supports relative and absolute paths)
 * @return VFS_OK on success, other error codes
 */
VfsError_t Vfs_Cd(const char *path);

/**
 * @brief  Find node by path
 * @param  path: Path (absolute or relative)
 * @return Node pointer, NULL if not found
 */
VfsNode_t* Vfs_FindNode(const char *path);

/**
 * @brief  Create subdirectory under specified directory
 * @param  parent: Parent directory node
 * @param  name: Directory name
 * @return Newly created directory node, NULL if failed
 */
VfsNode_t* Vfs_CreateDir(VfsNode_t *parent, const char *name);

/**
 * @brief  Create parameter node under specified directory
 * @param  parent: Parent directory node
 * @param  name: Parameter name
 * @param  desc: Parameter description
 * @param  get: Read callback
 * @param  set: Write callback (NULL means read-only)
 * @param  userData: User data
 * @return Newly created parameter node, NULL if failed
 */
VfsNode_t* Vfs_CreateParam(VfsNode_t *parent, const char *name, 
                            const char *desc,
                            VfsParamGet_t get, VfsParamSet_t set,
                            void *userData);

/**
 * @brief  Create device node under specified directory
 * @param  parent: Parent directory node
 * @param  name: Device name
 * @param  userData: Device private data
 * @return Newly created device node, NULL if failed
 */
VfsNode_t* Vfs_CreateDevice(VfsNode_t *parent, const char *name, void *userData);

/**
 * @brief  Create generic node under specified directory
 * @param  parent: Parent directory node
 * @param  name: Node name
 * @param  type: Node type
 * @param  userData: User data
 * @return Newly created node, NULL if failed
 */
VfsNode_t* Vfs_CreateNode(VfsNode_t *parent, const char *name, VfsNodeType_t type, void *userData);

/**
 * @brief  Read parameter value
 * @param  node: Parameter node
 * @param  buf: Output buffer
 * @param  maxLen: Buffer size
 * @return Number of bytes read, -1 error, -2 read-only parameter
 */
int Vfs_ReadParam(VfsNode_t *node, char *buf, uint16_t maxLen);

/**
 * @brief  Write parameter value
 * @param  node: Parameter node
 * @param  value: Value to write
 * @return VFS_OK on success
 */
VfsError_t Vfs_WriteParam(VfsNode_t *node, const char *value);

/**
 * @brief  List directory contents
 * @param  node: Directory node
 * @param  callback: Callback function
 * @param  userData: User data
 * @return VFS_OK on success
 */
VfsError_t Vfs_ListDir(VfsNode_t *node, VfsListCallback_t callback, void *userData);

/**
 * @brief  Remove node (recursively delete child nodes)
 * @param  node: Node to remove
 * @return VFS_OK on success
 */
VfsError_t Vfs_RemoveNode(VfsNode_t *node);

/**
 * @brief  Get node type name string
 * @param  type: Node type
 * @return Type name string
 */
const char* Vfs_GetTypeName(VfsNodeType_t type);

/**
 * @brief  Create directory by path (create parent if needed)
 * @param  path: Absolute path, e.g. "/bin/sys"
 * @return Created directory node, NULL if failed
 */
VfsNode_t* Vfs_Mkdir(const char *path);

/**
 * @brief VFS parameter node definition structure
 */
typedef struct {
    const char *name;    // Parameter name
    const char *desc;    // Parameter description
    int (*get)(char *buf, uint16_t maxLen, void *userData); // Read callback
    int (*set)(const char *buf, void *userData);            // Write callback
    void *userData;     // User data
} FsParamDef_t;

#define FS_PARAM_END {NULL, NULL, NULL, NULL, NULL}

#ifdef __cplusplus
}
#endif

#endif /* __VFS_H__ */
