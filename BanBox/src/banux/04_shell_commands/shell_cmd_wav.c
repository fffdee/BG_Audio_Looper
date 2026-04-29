/**
 * @file shell_cmd_wav.c
 * @brief Shell 命令 - WAV 文件导出和管理
 *
 * 支持的子命令：
 *   wav export <seg>    - 导出指定段为 WAV 文件 (seg: 0-3 或 'mix')
 *   wav list            - 列出所有 WAV 文件
 *   wav delete <file>   - 删除指定 WAV 文件
 *   wav info            - 显示 NAND FAT32 信息
 */

#include "product_def.h"

#if FAT32_EN && HW_DRV_FLASH_NAND_EN

#include "bg_shell.h"
#include "looper_wav_export.h"
#include "fat32_nand.h"
#include "audio_looper.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ============================================
 * 主命令处理函数
 * ============================================ */

static int cmd_wav_handler(int argc, char *argv[])
{
    if (argc < 2) {
        Shell_Print("WAV file export and management\r\n");
        Shell_Print("Usage:\r\n");
        Shell_Print("  wav export <seg>    Export segment to WAV (seg: 0-3 or 'mix')\r\n");
        Shell_Print("  wav list            List all WAV files\r\n");
        Shell_Print("  wav delete <file>   Delete WAV file\r\n");
        Shell_Print("  wav info            Show NAND FAT32 info\r\n");
        Shell_Print("  wav help            Show this help\r\n");
        return 0;
    }
    
    /* export 子命令 */
    if (strcmp(argv[1], "export") == 0) {
        BG_ERR ret;
        
        if (argc < 3) {
            Shell_Printf("Usage: wav export <seg>  (seg: 0-3 or 'mix')\r\n");
            return -1;
        }
        
        if (strcmp(argv[2], "mix") == 0) {
            Shell_Printf("Exporting mix to WAV...\r\n");
            ret = LooperWAV_ExportMix(NULL);
        } else {
            int seg = atoi(argv[2]);
            if (seg < 0 || seg >= MAX_SEGMENTS) {
                Shell_Printf("Error: Invalid segment index (must be 0-3)\r\n");
                return -1;
            }
            
            Shell_Printf("Exporting segment %d to WAV...\r\n", seg);
            ret = LooperWAV_ExportSegment((uint8_t)seg, NULL);
        }
        
        if (ret == SUCCESS) {
            Shell_Printf("Export completed successfully\r\n");
            Shell_Printf("Free space: %u KB\r\n", LooperWAV_GetFreeSpace() / 1024);
            return 0;
        } else {
            Shell_Printf("Export failed: %d\r\n", ret);
            return -1;
        }
    }
    
    /* list 子命令 */
    if (strcmp(argv[1], "list") == 0) {
        Shell_Printf("Listing WAV files in /recordings\r\n");
        Shell_Printf("(file listing not implemented yet)\r\n");
        return 0;
    }
    
    /* delete 子命令 */
    if (strcmp(argv[1], "delete") == 0) {
        BG_ERR ret;
        
        if (argc < 3) {
            Shell_Printf("Usage: wav delete <filename>\r\n");
            return -1;
        }
        
        Shell_Printf("Deleting %s...\r\n", argv[2]);
        ret = LooperWAV_DeleteFile(argv[2]);
        
        if (ret == SUCCESS) {
            Shell_Printf("File deleted successfully\r\n");
            return 0;
        } else {
            Shell_Printf("Delete failed: %d\r\n", ret);
            return -1;
        }
    }
    
    /* info 子命令 */
    if (strcmp(argv[1], "info") == 0) {
        uint32_t free_space = LooperWAV_GetFreeSpace();
        uint32_t total = FAT32_NAND_PARTITION_SIZE;
        uint32_t used = total - free_space;
        
        Shell_Print("NAND FAT32 Information:\r\n");
        Shell_Printf("  Partition: %u MB\r\n", total / (1024 * 1024));
        Shell_Printf("  Used:      %u KB\r\n", used / 1024);
        Shell_Printf("  Free:      %u KB\r\n", free_space / 1024);
        Shell_Printf("  Dir:       /recordings\r\n");
        return 0;
    }
    
    /* help 子命令 */
    if (strcmp(argv[1], "help") == 0) {
        Shell_Print("WAV file export and management\r\n");
        Shell_Print("Usage:\r\n");
        Shell_Print("  wav export <seg>    Export segment to WAV (seg: 0-3 or 'mix')\r\n");
        Shell_Print("  wav list            List all WAV files\r\n");
        Shell_Print("  wav delete <file>   Delete WAV file\r\n");
        Shell_Print("  wav info            Show NAND FAT32 info\r\n");
        Shell_Print("\r\nExample:\r\n");
        Shell_Print("  wav export 0        Export segment 0 to WAV\r\n");
        Shell_Print("  wav export mix      Export all segments mixed\r\n");
        Shell_Print("  wav info            Show available space\r\n");
        return 0;
    }
    
    Shell_Printf("Unknown subcommand: %s\r\n", argv[1]);
    Shell_Print("Type 'wav help' for usage\r\n");
    return -1;
}

/* ============================================
 * Shell Module 定义
 * ============================================ */

static const ShellOpt_t wav_options[] = {
    OPT("", "<subcommand>", "WAV export", 
        "Export, list, delete, or manage WAV files\n"
        "    wav export <seg>    - Export segment to WAV\n"
        "    wav list            - List WAV files\n"
        "    wav delete <file>   - Delete WAV file\n"
        "    wav info            - Show NAND FAT32 info",
        cmd_wav_handler),
    OPT_END()
};

DEFINE_MODULE(wav, "Audio Looper WAV export", MOD_CAT_DEBUG, wav_options);

/* ============================================
 * 公共接口
 * ============================================ */

void ShellCmdWav_Register(void)
{
    Shell_RegisterModule(&_mod_wav);
}

#endif /* FAT32_EN && HW_DRV_FLASH_NAND_EN */
