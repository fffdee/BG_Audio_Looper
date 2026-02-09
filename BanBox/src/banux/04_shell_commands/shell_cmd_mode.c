/**
 *****************************************************************************
 * @file     shell_cmd_mode.c
 * @author   BG Audio Team
 * @version  V1.0.0
 * @date     06-February-2026
 * @brief    设备模式Shell命令实现 - 控制主副音箱模式切换
 * 
 * 功能说明:
 *   - 副音箱模式：仅混音所有音频输入到输出，无效果器和Looper
 *   - 主音箱模式：完整功能模式（默认）
 *   - 模式切换不保存到Flash，掉电后恢复为主音箱模式
 * 
 * 命令用法:
 *   mode           - 显示当前模式
 *   mode main      - 切换到主音箱模式
 *   mode secondary - 切换到副音箱模式
 *   mode help      - 显示帮助信息
 *****************************************************************************
 */

#include "shell_cmd_mode.h"
#include "bg_shell.h"
#include "effect_graph_config.h"
#include "effect_graph.h"
#include "audio_looper.h"
#include "bg_audio_io_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "audio_setting.h"
/*******************************************************************************
 * 静态变量
 ******************************************************************************/
static DeviceMode_t g_CurrentMode = DEVICE_MODE_MAIN;  /* 默认主音箱模式 */

/*******************************************************************************
 * 模式名称表
 ******************************************************************************/
static const char* g_ModeNames[] = {
    "Main Speaker",       /* 主音箱模式 */
    "Secondary Speaker"   /* 副音箱模式 */
};

/*******************************************************************************
 * 内部辅助函数
 ******************************************************************************/

/**
 * @brief 显示当前模式状态
 */
static void show_mode_status(void)
{
    Shell_Printf("\n========== Device Mode Status ==========\n");
    Shell_Printf("Current Mode: %s\n", g_ModeNames[g_CurrentMode]);
    
    if (g_CurrentMode == DEVICE_MODE_MAIN) {
        Shell_Printf("Description:  Full function mode with effects and looper\n");
        Shell_Printf("Graph Preset: Default (Full)\n");
    } else {
        Shell_Printf("Description:  Simple mixer mode without effects and looper\n");
        Shell_Printf("Graph Preset: Secondary Speaker\n");
    }
    
    Shell_Printf("Note:         Mode setting is NOT saved to Flash\n");
    Shell_Printf("              (resets to Main mode after power-off)\n");
    Shell_Printf("========================================\n\n");
}

/**
 * @brief 显示帮助信息
 */
static void show_help(void)
{
    Shell_Printf("\n========== Mode Commands ==========\n");
    Shell_Printf("mode              - Show current mode\n");
    Shell_Printf("mode main         - Switch to main speaker mode (full function)\n");
    Shell_Printf("mode secondary    - Switch to secondary speaker mode (mixer only)\n");
    Shell_Printf("mode help         - Show this help\n");
    Shell_Printf("\n");
    Shell_Printf("Main Mode Features:\n");
    Shell_Printf("  - Full effect chain (EQ, Reverb, DRC, etc.)\n");
    Shell_Printf("  - Looper recording and playback\n");
    Shell_Printf("  - Metronome support\n");
    Shell_Printf("\n");
    Shell_Printf("Secondary Mode Features:\n");
    Shell_Printf("  - Simple mixer (all inputs to outputs)\n");
    Shell_Printf("  - No effects processing\n");
    Shell_Printf("  - No looper function\n");
    Shell_Printf("  - Lower CPU usage\n");
    Shell_Printf("===================================\n\n");
}

/*******************************************************************************
 * 公共API实现
 ******************************************************************************/

/**
 * @brief 获取当前设备模式
 */
DeviceMode_t DeviceMode_GetCurrent(void)
{
    return g_CurrentMode;
}

/**
 * @brief 设置设备模式
 */
int DeviceMode_Set(DeviceMode_t mode)
{
    GraphError_t err;
    
    if (mode >= DEVICE_MODE_MAX) {
        DBG("[Mode] Invalid mode: %d\n", mode);
        return -1;
    }
    
    if (mode == g_CurrentMode) {
        DBG("[Mode] Already in %s mode\n", g_ModeNames[mode]);
        return 0;
    }
    
    DBG("[Mode] Switching from %s to %s mode...\n", 
        g_ModeNames[g_CurrentMode], g_ModeNames[mode]);
    
    /* 如果要切换到副音箱模式，先停止looper */
    if (mode == DEVICE_MODE_SECONDARY) {
        /* 停止所有looper段的录制和播放 */
        int i;
        for (i = 0; i < MAX_SEGMENTS; i++) {
            loop_set_segment_stopped(i);
        }
        DBG("[Mode] Looper stopped\n");
    }
    
    /* 根据模式加载相应的effect graph预设 */
    if (mode == DEVICE_MODE_MAIN) {
        /* 主音箱模式：加载默认完整配置 */
        err = EffectGraphConfig_LoadPreset(GRAPH_PRESET_DEFAULT);
    } else {
        /* 副音箱模式：加载简单混音配置 */
        err = EffectGraphConfig_LoadPreset(GRAPH_PRESET_SECONDARY_SPEAKER);
        AudioSetting_SetMic1Volume(10);
        AudioSetting_SetMic2Volume(10);
        AudioSetting_SetGuitar1Volume(10);
        AudioSetting_SetGuitar2Volume(10);
    }
    
    if (err != GRAPH_OK) {
        DBG("[Mode] Failed to load preset: %d\n", err);
        return -1;
    }
    
    /* 重新注册所有节点的回调函数 - 关键！*/
    DBG("[Mode] Re-registering Effect Graph callbacks...\n");
    BG_AudioIO_SetupEffectGraphCallbacks();
    
    /* 更新当前模式 */
    g_CurrentMode = mode;
    
    DBG("[Mode] Successfully switched to %s mode\n", g_ModeNames[mode]);
    
    return 0;
}

/*******************************************************************************
 * Mode命令处理函数
 ******************************************************************************/

/**
 * @brief mode命令主处理函数
 */
static int cmd_mode_main(int argc, char *argv[])
{
    /* 如果没有子命令，显示当前状态 */
    if (argc < 1) {
        show_mode_status();
        return 0;
    }

    const char *subcmd = argv[0];

    /* 根据子命令路由 */
    if (strcmp(subcmd, "help") == 0) {
        show_help();
        return 0;
    }
    else if (strcmp(subcmd, "main") == 0) {
        if (DeviceMode_Set(DEVICE_MODE_MAIN) != 0) {
            Shell_Printf("ERROR: Failed to switch to main mode\n");
            return -1;
        }
        Shell_Printf("Switched to Main Speaker mode\n");
        return 0;
    }
    else if (strcmp(subcmd, "secondary") == 0) {
        if (DeviceMode_Set(DEVICE_MODE_SECONDARY) != 0) {
            Shell_Printf("ERROR: Failed to switch to secondary mode\n");
            return -1;
        }
        Shell_Printf("Switched to Secondary Speaker mode (mixer only)\n");
        Shell_Printf("Note: Effects and Looper are disabled in this mode\n");
        return 0;
    }
    else {
        Shell_Printf("ERROR: Unknown subcommand: %s\n", subcmd);
        Shell_Printf("Try 'mode help' for usage information\n");
        return -1;
    }
}

/**
 * @brief mode命令选项定义
 */
static const ShellOpt_t mode_options[] = {
    { "", NULL, "[subcmd]", "Device mode control", cmd_mode_main },
    OPT_END()
};

/**
 * @brief 执行mode命令
 */
int ShellCmdMode_Execute(int argc, char *argv[])
{
    return cmd_mode_main(argc - 1, argv + 1);
}

/**
 * @brief 注册mode命令到Shell系统
 */
int ShellCmdMode_Register(void)
{
    static const ShellModule_t mode_module = {
        "mode",
        "Device mode control (main/secondary speaker)",
        MOD_CAT_AUDIO,
        mode_options,
        1  /* optCount */
    };

    return Shell_RegisterModule(&mode_module);
}
    
