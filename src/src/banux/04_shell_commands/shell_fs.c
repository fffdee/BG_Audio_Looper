/**
 *****************************************************************************
 * @file     shell_fs.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    Shell文件系统实现 - /bin目录管理
 *****************************************************************************
 */

#include <string.h>
#include <stdio.h>
#include "shell_fs.h"
#include "bg_shell.h"
#include "shell_io_manager.h"
#include "chip_info.h"
#include "debug.h"

/*******************************************************************************
 * 静态变量
 ******************************************************************************/
static VfsNode_t *g_BinDir = NULL;
static bool       g_ShellFsInitialized = FALSE;

/*******************************************************************************
 * 公共API实现
 ******************************************************************************/

VfsError_t ShellFs_Init(void)
{
    VfsNode_t *root;
    if (g_ShellFsInitialized) return VFS_OK;
    root = Vfs_GetRoot();
    if (!root) {
        DBG("[ShellFs] ERROR: VFS not initialized!\n");
        return VFS_ERR_NOT_FOUND;
    }
    DBG("[ShellFs] Creating /bin directory...\n");
    /* 创建 /bin */
    g_BinDir = Vfs_CreateDir(root, "bin");
    if (!g_BinDir) {
        DBG("[ShellFs] ERROR: Failed to create /bin\n");
        return VFS_ERR_NO_MEMORY;
    }
    DBG("[ShellFs] /bin created successfully\n");
    g_ShellFsInitialized = TRUE;
    return VFS_OK;
}

VfsNode_t* ShellFs_GetBinDir(void)
{
    return g_BinDir;
}

VfsNode_t* ShellFs_RegisterCommand(const char *name)
{
    VfsNode_t *node;
    if (!g_BinDir || !name) return NULL;
    // 只在/bin下创建一个普通节点，类型为命令
    node = Vfs_CreateNode(g_BinDir, name, VFS_NODE_CMD, NULL);
    if (!node) {
        DBG("[ShellFs] ERROR: Failed to register command: %s\n", name);
    }
    return node;
}

void ShellFs_RegisterAllCommands(void)
{
    DBG("[ShellFs] Registering all /bin commands...\n");
    // 只注册命令名节点，不再注册参数节点
    ShellFs_RegisterCommand("sys");
    ShellFs_RegisterCommand("audio");
    ShellFs_RegisterCommand("gpio");
    ShellFs_RegisterCommand("lcd");
    ShellFs_RegisterCommand("led");
    ShellFs_RegisterCommand("dbg");
    ShellFs_RegisterCommand("looper");
    ShellFs_RegisterCommand("flash");
    ShellFs_RegisterCommand("battery");
    ShellFs_RegisterCommand("bt");
    ShellFs_RegisterCommand("ls");
    ShellFs_RegisterCommand("pwd");
    ShellFs_RegisterCommand("cd");
    ShellFs_RegisterCommand("cat");
    ShellFs_RegisterCommand("echo");    // 写入参数值
    ShellFs_RegisterCommand("tree");
    ShellFs_RegisterCommand("drivers");
    ShellFs_RegisterCommand("effect");
    ShellFs_RegisterCommand("sysmon");
    // TODO: 其他命令名同理注册
    DBG("[ShellFs] All /bin commands registered\n");
}
