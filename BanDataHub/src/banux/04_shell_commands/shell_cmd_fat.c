/**
 * @file shell_cmd_fat.c
 * @brief FAT32 文件系统 Shell 命令模块实现
 *
 * 提供以下命令:
 *   sd ls                 列出当前目录内容
 *   sd cd <dir>           切换到指定目录
 *   sd pwd                显示当前目录路径
 *   sd cat <file>         读取并显示文件内容
 *   sd write <file> <data> 写入数据到文件
 *   sd rm <file>          删除文件
 *   sd mkdir <dir>        创建目录
 *   sd rmdir <dir>        删除目录
 *   sd rename <old> <new> 重命名文件或目录
 *   sd info               显示文件系统信息
 *   sd diag               诊断 FAT32 初始化
 *
 * 编译条件: FAT32_EN
 */

#include "product_def.h"

#if FAT32_EN && HW_CMD_FAT_EN

#include "shell_cmd_fat.h"
#include "bg_shell.h"
#include "fat32_reader.h"
#include "fat32_diskio.h"
#include "hal_sdio.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ============================================
 * 内部常量
 * ============================================ */
#define FAT_MAX_READ_SIZE    4096   /* 单次读取最大字节数 */
#define FAT_MAX_DATA_SIZE    1024   /* 写入数据最大长度 */

/* ============================================
 * 全局状态
 * ============================================ */
static uint32_t g_cwd_cluster = 0;  /* 当前工作目录簇号 */
static char g_cwd_path[256] = "/";  /* 当前工作目录路径字符串 */

/* ============================================
 * 内部函数声明
 * ============================================ */

static int fat_ls_cmd(int argc, char *argv[]);
static int fat_cd_cmd(int argc, char *argv[]);
static int fat_pwd_cmd(int argc, char *argv[]);
static int fat_cat_cmd(int argc, char *argv[]);
static int fat_write_cmd(int argc, char *argv[]);
static int fat_rm_cmd(int argc, char *argv[]);
static int fat_mkdir_cmd(int argc, char *argv[]);
static int fat_rmdir_cmd(int argc, char *argv[]);
static int fat_rename_cmd(int argc, char *argv[]);
static int fat_info_cmd(int argc, char *argv[]);
static int fat_diag_cmd(int argc, char *argv[]);

/* 列目录回调函数 */
static int fat_list_callback(const FAT32_FileInfo_t *info, void *user);

/* ============================================
 * 命令选项定义
 * ============================================ */

static const ShellOpt_t fat_options[] = {
    OPT("ls",     "list",    "",             "List current directory contents", fat_ls_cmd),
    OPT("cd",     "cd",      "<dir>",        "Change directory",                fat_cd_cmd),
    OPT("pwd",    "pwd",     "",             "Print working directory",         fat_pwd_cmd),
    OPT("cat",    "cat",     "<file>",       "Display file contents",           fat_cat_cmd),
    OPT("write",  "write",   "<file> <data>", "Write data to file",            fat_write_cmd),
    OPT("rm",     "remove",  "<file>",       "Delete file",                     fat_rm_cmd),
    OPT("mkdir",  "mkdir",   "<dir>",        "Create directory",                fat_mkdir_cmd),
    OPT("rmdir",  "rmdir",   "<dir>",        "Remove directory",                fat_rmdir_cmd),
    OPT("rename", "rename",  "<old> <new>",  "Rename file or directory",        fat_rename_cmd),
    OPT("info",   "info",    "",             "Show filesystem info",            fat_info_cmd),
    OPT("diag",   "diag",    "",             "Dump sector 0 / diagnose FAT32 init", fat_diag_cmd),
    OPT_END()
};

/* ============================================
 * 模块定义
 * ============================================ */

DEFINE_MODULE(sd, "SD card FAT32 filesystem operations", MOD_CAT_DEBUG, fat_options);

/* ============================================
 * 公共接口实现
 * ============================================ */

int ShellCmdFat_Register(void)
{
    return Shell_RegisterModule(&_mod_sd) ? 0 : -1;
}

/* ============================================
 * 路径解析辅助函数
 * ============================================ */

/**
 * 将路径解析为「父目录簇号 + 最后一个分量名」
 * 支持绝对路径（以'/')和相对路径
 * *out_name 指向内部静态缓冲区，调用后立即使用
 */
static BG_ERR fat_path_to_dir(const char *path,
                               uint32_t  *out_dir,
                               const char **out_name)
{
    static char    s_buf[256];
    char          *p;
    char          *slash;
    uint32_t       cluster;
    FAT32_FileInfo_t info;
    BG_ERR         ret;

    if (!path || !path[0]) {
        return ENABLE_INVALID_INPUT;
    }

    if (path[0] == '/') {
        cluster = FAT32_GetRootCluster();
        strncpy(s_buf, path + 1, sizeof(s_buf) - 1);
    } else {
        cluster = g_cwd_cluster;
        strncpy(s_buf, path, sizeof(s_buf) - 1);
    }
    s_buf[sizeof(s_buf) - 1] = '\0';

    p = s_buf;
    while (1) {
        slash = strchr(p, '/');
        if (!slash) {
            *out_dir  = cluster;
            *out_name = p;
            return SUCCESS;
        }
        *slash = '\0';
        if (p[0] != '\0') {
            ret = FAT32_FindEntryInDir(cluster, p, &info);
            if (ret != SUCCESS) {
                return ret;
            }
            if (!(info.attr & DIR_ATTR_DIRECTORY)) {
                return ENABLE_INVALID_INPUT;
            }
            cluster = info.start_cluster;
        }
        p = slash + 1;
    }
}

/* ============================================
 * 命令处理器实现
 * ============================================ */

static int fat_ls_cmd(int argc, char *argv[])
{
    BG_ERR ret;

    (void)argc; (void)argv;

    /* 延迟初始化：首次使用时初始化 FAT32 */
    ret = FAT32_Init();
    if (ret != SUCCESS) {
        Shell_Printf("FAT32_Init failed: %d\r\n", ret);
        return -1;
    }

    /* 初始化 cwd 到根目录（如果还没初始化） */
    if (g_cwd_cluster == 0) {
        g_cwd_cluster = FAT32_GetRootCluster();
        strcpy(g_cwd_path, "/");
    }

    Shell_Print("Listing directory: ");
    Shell_Printf("%s\r\n", g_cwd_path);
    Shell_Print("Name                    Size        Attr\r\n");
    Shell_Print("----------------------------------------\r\n");

    ret = FAT32_ListDirByCluster(g_cwd_cluster, fat_list_callback, NULL);
    if (ret != SUCCESS) {
        Shell_Printf("Error: %d\r\n", ret);
        return -1;
    }

    return 0;
}

static int fat_cd_cmd(int argc, char *argv[])
{
    const char       *arg;
    FAT32_FileInfo_t  info;
    BG_ERR            ret;
    uint32_t          dir_cluster;
    const char       *name;
    char             *p;

    /* 延迟初始化 */
    ret = FAT32_Init();
    if (ret != SUCCESS) {
        Shell_Printf("FAT32_Init failed: %d\r\n", ret);
        return -1;
    }

    /* 初始化 cwd */
    if (g_cwd_cluster == 0) {
        g_cwd_cluster = FAT32_GetRootCluster();
        strcpy(g_cwd_path, "/");
    }

    if (argc < 1 || !argv[0] || !argv[0][0]) {
        Shell_Print("Usage: sd -cd <dirname>\r\n");
        return -1;
    }

    arg = argv[0];

    /* cd / 回根目录 */
    if (strcmp(arg, "/") == 0) {
        g_cwd_cluster = FAT32_GetRootCluster();
        strcpy(g_cwd_path, "/");
        return 0;
    }

    /* cd .. 返回上级 */
    if (strcmp(arg, "..") == 0) {
        if (strcmp(g_cwd_path, "/") == 0) {
            return 0;  /* 已在根目录，静默返回 */
        }
        ret = FAT32_FindEntryInDir(g_cwd_cluster, "..", &info);
        if (ret != SUCCESS) {
            Shell_Printf("Cannot access parent: %d\r\n", ret);
            return -1;
        }
        /* .. 在根目录子项中 start_cluster==0 表示回到根 */
        g_cwd_cluster = (info.start_cluster == 0) ? FAT32_GetRootCluster()
                                                   : info.start_cluster;
        p = strrchr(g_cwd_path, '/');
        if (p && p != g_cwd_path) {
            *p = '\0';
        } else {
            strcpy(g_cwd_path, "/");
        }
        return 0;
    }

    /* 解析路径（支持绝对路径 /SF2 和相对路径 SF2） */
    ret = fat_path_to_dir(arg, &dir_cluster, &name);
    if (ret != SUCCESS) {
        Shell_Printf("Directory '%s' not found\r\n", arg);
        return -1;
    }

    /* 查找目标目录条目 */
    ret = FAT32_FindEntryInDir(dir_cluster, name, &info);
    if (ret != SUCCESS) {
        Shell_Printf("Directory '%s' not found\r\n", arg);
        return -1;
    }
    if (!(info.attr & DIR_ATTR_DIRECTORY)) {
        Shell_Printf("'%s' is not a directory\r\n", arg);
        return -1;
    }

    g_cwd_cluster = info.start_cluster;

    /* 更新显示路径 */
    if (arg[0] == '/') {
        /* 绝对路径：直接使用 arg */
        strncpy(g_cwd_path, arg, sizeof(g_cwd_path) - 1);
        g_cwd_path[sizeof(g_cwd_path) - 1] = '\0';
    } else {
        /* 相对路径：追加到当前路径 */
        if (strcmp(g_cwd_path, "/") != 0) {
            strncat(g_cwd_path, "/", sizeof(g_cwd_path) - strlen(g_cwd_path) - 1);
        }
        strncat(g_cwd_path, arg, sizeof(g_cwd_path) - strlen(g_cwd_path) - 1);
    }

    return 0;
}

static int fat_pwd_cmd(int argc, char *argv[])
{
    (void)argc; (void)argv;

    Shell_Printf("%s\r\n", g_cwd_path);
    return 0;
}

static int fat_cat_cmd(int argc, char *argv[])
{
    const char        *name;
    uint32_t           dir_cluster;
    FAT32_FileHandle_t handle;
    BG_ERR ret;
    int32_t read_bytes;
    uint8_t buffer[FAT_MAX_READ_SIZE];
    uint32_t total_read = 0;

    /* 延迟初始化 */
    ret = FAT32_Init();
    if (ret != SUCCESS) {
        Shell_Printf("FAT32_Init failed: %d\r\n", ret);
        return -1;
    }

    if (argc < 1) {
        Shell_Print("Usage: sd -cat <filename>\r\n");
        return -1;
    }

    ret = fat_path_to_dir(argv[0], &dir_cluster, &name);
    if (ret != SUCCESS) {
        Shell_Printf("Path error: %d\r\n", ret);
        return -1;
    }

    ret = FAT32_OpenFileInDir(dir_cluster, name, &handle);
    if (ret != SUCCESS) {
        Shell_Printf("Failed to open file '%s': %d\r\n", argv[0], ret);
        return -1;
    }

    while ((read_bytes = FAT32_ReadFile(&handle, buffer, sizeof(buffer))) > 0) {
        uint32_t i;
        for (i = 0; i < (uint32_t)read_bytes; i++) {
            if ((total_read + i) % 16 == 0) {
                Shell_Printf("\r\n%08X: ", total_read + i);
            }
            Shell_Printf("%02X ", buffer[i]);
        }
        total_read += read_bytes;
        if (total_read >= FAT_MAX_READ_SIZE) {
            Shell_Print("\r\n... (truncated)\r\n");
            break;
        }
    }

    if (read_bytes < 0) {
        Shell_Printf("\r\nRead error: %d\r\n", read_bytes);
    } else {
        Shell_Printf("\r\nTotal: %u bytes\r\n", total_read);
    }

    FAT32_CloseFile(&handle);
    return 0;
}

static int fat_write_cmd(int argc, char *argv[])
{
    const char *name;
    const char *data_str;
    uint32_t    dir_cluster;
    int32_t     written;
    BG_ERR      ret;

    /* 延迟初始化 */
    ret = FAT32_Init();
    if (ret != SUCCESS) {
        Shell_Printf("FAT32_Init failed: %d\r\n", ret);
        return -1;
    }

    if (argc < 2) {
        Shell_Print("Usage: sd -write <filename> <data>\r\n");
        return -1;
    }

    ret = fat_path_to_dir(argv[0], &dir_cluster, &name);
    if (ret != SUCCESS) {
        Shell_Printf("Path error: %d\r\n", ret);
        return -1;
    }

    data_str = argv[1];
    if (strlen(data_str) > FAT_MAX_DATA_SIZE) {
        Shell_Printf("Data too long (max %d bytes)\r\n", FAT_MAX_DATA_SIZE);
        return -1;
    }

    written = FAT32_WriteFile(dir_cluster, name, data_str, strlen(data_str));
    if (written < 0) {
        Shell_Printf("Write failed: %d\r\n", written);
        return -1;
    }

    Shell_Printf("Written %d bytes to '%s'\r\n", written, argv[0]);
    return 0;
}

static int fat_rm_cmd(int argc, char *argv[])
{
    const char *name;
    uint32_t    dir_cluster;
    BG_ERR ret;

    /* 延迟初始化 */
    ret = FAT32_Init();
    if (ret != SUCCESS) {
        Shell_Printf("FAT32_Init failed: %d\r\n", ret);
        return -1;
    }

    if (argc < 1) {
        Shell_Print("Usage: sd -rm <filename>\r\n");
        return -1;
    }

    ret = fat_path_to_dir(argv[0], &dir_cluster, &name);
    if (ret != SUCCESS) {
        Shell_Printf("Path error: %d\r\n", ret);
        return -1;
    }

    ret = FAT32_DeleteFile(dir_cluster, name);
    if (ret != SUCCESS) {
        Shell_Printf("Delete failed: %d\r\n", ret);
        return -1;
    }

    Shell_Printf("Deleted '%s'\r\n", argv[0]);
    return 0;
}

static int fat_mkdir_cmd(int argc, char *argv[])
{
    const char *name;
    uint32_t    dir_cluster;
    BG_ERR ret;

    ret = FAT32_Init();
    if (ret != SUCCESS) {
        Shell_Printf("FAT32_Init failed: %d\r\n", ret);
        return -1;
    }

    if (argc < 1 || !argv[0] || !argv[0][0]) {
        Shell_Print("Usage: sd -mkdir <dirname>\r\n");
        return -1;
    }

    ret = fat_path_to_dir(argv[0], &dir_cluster, &name);
    if (ret != SUCCESS) {
        Shell_Printf("Path error: %d\r\n", ret);
        return -1;
    }

    ret = FAT32_MkDir(dir_cluster, name);
    if (ret != SUCCESS) {
        Shell_Printf("mkdir '%s' failed: %d\r\n", argv[0], ret);
        return -1;
    }

    Shell_Printf("Created directory '%s'\r\n", argv[0]);
    return 0;
}

static int fat_rmdir_cmd(int argc, char *argv[])
{
    const char *name;
    uint32_t    dir_cluster;
    BG_ERR ret;

    ret = FAT32_Init();
    if (ret != SUCCESS) {
        Shell_Printf("FAT32_Init failed: %d\r\n", ret);
        return -1;
    }

    if (argc < 1 || !argv[0] || !argv[0][0]) {
        Shell_Print("Usage: sd -rmdir <dirname>\r\n");
        return -1;
    }

    ret = fat_path_to_dir(argv[0], &dir_cluster, &name);
    if (ret != SUCCESS) {
        Shell_Printf("Path error: %d\r\n", ret);
        return -1;
    }

    ret = FAT32_RmDir(dir_cluster, name);
    if (ret != SUCCESS) {
        Shell_Printf("rmdir '%s' failed: %d\r\n", argv[0], ret);
        return -1;
    }

    Shell_Printf("Removed directory '%s'\r\n", argv[0]);
    return 0;
}

static int fat_rename_cmd(int argc, char *argv[])
{
    const char *oldname;
    const char *newname;
    uint32_t    dir_cluster;
    BG_ERR ret;

    ret = FAT32_Init();
    if (ret != SUCCESS) {
        Shell_Printf("FAT32_Init failed: %d\r\n", ret);
        return -1;
    }

    if (argc < 2 || !argv[0] || !argv[0][0] || !argv[1] || !argv[1][0]) {
        Shell_Print("Usage: sd -rename <oldname> <newname>\r\n");
        return -1;
    }

    ret = fat_path_to_dir(argv[0], &dir_cluster, &oldname);
    if (ret != SUCCESS) {
        Shell_Printf("Path error: %d\r\n", ret);
        return -1;
    }

    newname = argv[1];

    ret = FAT32_Rename(dir_cluster, oldname, newname);
    if (ret != SUCCESS) {
        Shell_Printf("rename '%s' -> '%s' failed: %d\r\n", argv[0], newname, ret);
        return -1;
    }

    Shell_Printf("Renamed '%s' -> '%s'\r\n", argv[0], newname);
    return 0;
}

static int fat_info_cmd(int argc, char *argv[])
{
    FAT32_FSInfo_t info;
    BG_ERR ret;

    /* 延迟初始化 */
    ret = FAT32_Init();
    if (ret != SUCCESS) {
        Shell_Printf("FAT32_Init failed: %d\r\n", ret);
        return -1;
    }

    ret = FAT32_GetFSInfo(&info);
    if (ret != SUCCESS) {
        Shell_Printf("Get info failed: %d\r\n", ret);
        return -1;
    }

    Shell_Print("FAT32 Filesystem Info:\r\n");
    Shell_Printf("  Bytes per sector: %u\r\n", info.bpb.bytes_per_sector);
    Shell_Printf("  Sectors per cluster: %u\r\n", info.bpb.sectors_per_cluster);
    Shell_Printf("  Reserved sectors: %u\r\n", info.bpb.reserved_sectors);
    Shell_Printf("  Number of FATs: %u\r\n", info.bpb.num_fats);
    Shell_Printf("  Root cluster: %u\r\n", info.bpb.root_cluster);
    Shell_Printf("  Total clusters: %u\r\n", info.total_clusters);
    Shell_Printf("  FAT start sector: %u\r\n", info.fat_start_sector);
    Shell_Printf("  Data start sector: %u\r\n", info.data_start_sector);

    return 0;
}

static int fat_diag_cmd(int argc, char *argv[])
{
    static uint8_t buf[512]; /* static: 避免堆栈溢出 */
    uint32_t i;

    (void)argc; (void)argv;

    Shell_Print("=== FAT32 Sector 0 Diagnostic ===\r\n");
    if (HAL_SD_ReadBlocks(0, buf, 1) != HAL_SD_OK) {
        Shell_Print("  ERROR: HAL_SD_ReadBlocks(0) FAILED\r\n");
        return -1;
    }

    Shell_Printf("  byte[0]  = 0x%02X\r\n", (unsigned)buf[0]);
    Shell_Printf("  sig[510] = 0x%02X  sig[511] = 0x%02X\r\n",
                 (unsigned)buf[510], (unsigned)buf[511]);

    /* 判断顺序：先检查 JMP 字节（与 fat32_parse_mbr 保持一致） */
    if (buf[0] == 0xEB || buf[0] == 0xE9) {
        /* 超级软盘格式：打印 BPB 关键字段 */
        uint16_t bps     = (uint16_t)buf[0x0B] | ((uint16_t)buf[0x0C] << 8);
        uint8_t  spc     = buf[0x0D];
        uint16_t rsvd    = (uint16_t)buf[0x0E] | ((uint16_t)buf[0x0F] << 8);
        uint16_t fsver   = (uint16_t)buf[0x2A] | ((uint16_t)buf[0x2B] << 8);
        uint32_t root_cl = (uint32_t)buf[0x2C] | ((uint32_t)buf[0x2D] << 8) |
                           ((uint32_t)buf[0x2E] << 16) | ((uint32_t)buf[0x2F] << 24);
        uint32_t j;
        char fstype[9];
        for (j = 0; j < 8u; j++) {
            fstype[j] = (buf[0x52+j] >= 0x20u && buf[0x52+j] < 0x7Fu)
                        ? (char)buf[0x52+j] : '.';
        }
        fstype[8] = '\0';
        Shell_Print("  -> Super-floppy BPB detected\r\n");
        Shell_Printf("  bytes_per_sector  = %u  (%s)\r\n", (unsigned)bps,
                     (bps == 512u) ? "OK" : "NOT 512 -- WILL FAIL");
        Shell_Printf("  sectors_per_clust = %u\r\n",  (unsigned)spc);
        Shell_Printf("  reserved_sectors  = %u\r\n",  (unsigned)rsvd);
        Shell_Printf("  fs_version(0x2A)  = 0x%04X  (%s)\r\n", (unsigned)fsver,
                     (fsver == 0u) ? "OK (0)" : "non-zero -- was FAILING before fix");
        Shell_Printf("  root_cluster      = %u\r\n",  (unsigned)root_cl);
        Shell_Printf("  fs_type string    = '%s'\r\n", fstype);
    } else if (buf[510] == 0x55 && buf[511] == 0xAA) {
        /* MBR 格式 */
        Shell_Print("  -> MBR detected\r\n");
        Shell_Print("  Partition table:\r\n");
        for (i = 0; i < 4; i++) {
            uint8_t *p = &buf[0x1BE + i * 16u];
            uint32_t lba  = (uint32_t)p[8]  | ((uint32_t)p[9]  << 8) |
                            ((uint32_t)p[10] << 16) | ((uint32_t)p[11] << 24);
            uint32_t nsec = (uint32_t)p[12] | ((uint32_t)p[13] << 8) |
                            ((uint32_t)p[14] << 16) | ((uint32_t)p[15] << 24);
            Shell_Printf("    [%u] type=0x%02X lba=%u secs=%u\r\n",
                         (unsigned)i, (unsigned)p[4],
                         (unsigned)lba, (unsigned)nsec);
        }
    } else {
        Shell_Print("  -> Unknown format\r\n");
        Shell_Print("  First 16 bytes: ");
        for (i = 0; i < 16u; i++) {
            Shell_Printf("%02X ", (unsigned)buf[i]);
        }
        Shell_Print("\r\n");
    }

    /* 运行 FAT32_Init 并显示结果 */
    {
        BG_ERR ret = FAT32_Init();
        Shell_Printf("  FAT32_Init() = %d (%s)\r\n",
                     (int)ret, (ret == SUCCESS) ? "OK" : "FAILED");
    }
    return 0;
}

/* ============================================
 * 内部辅助函数
 * ============================================ */

static int fat_list_callback(const FAT32_FileInfo_t *info, void *user)
{
    char attr_str[6] = {0};
    uint32_t size_kb = info->size / 1024;
    uint32_t size_rem = info->size % 1024;

    /* 属性字符串 */
    attr_str[0] = (info->attr & DIR_ATTR_DIRECTORY) ? 'D' : '-';
    attr_str[1] = (info->attr & DIR_ATTR_READ_ONLY) ? 'R' : '-';
    attr_str[2] = (info->attr & DIR_ATTR_HIDDEN) ? 'H' : '-';
    attr_str[3] = (info->attr & DIR_ATTR_SYSTEM) ? 'S' : '-';
    attr_str[4] = (info->attr & DIR_ATTR_ARCHIVE) ? 'A' : '-';

    /* 显示文件名 (截断到20字符) */
    Shell_Printf("%-20.20s ", info->name);

    /* 显示大小 */
    if (size_kb > 0) {
        Shell_Printf("%4u.%03u KB ", size_kb, (size_rem * 1000) / 1024);
    } else {
        Shell_Printf("%8u B  ", info->size);
    }

    /* 显示属性 */
    Shell_Printf("%s\r\n", attr_str);

    return 0; /* 继续遍历 */
}

#endif /* FAT32_EN */
