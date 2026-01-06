/**
 *****************************************************************************
 * @file     shell_cmd_effect.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     06-January-2026
 * @brief    音频效果器Shell命令模块实现 - 通过ID查询和调节效果器参数
 *****************************************************************************
 */

#include "shell_cmd_effect.h"
#include "ctrlvars.h"
#include "bg_shell.h"
#include "shell_fs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"

/*******************************************************************************
 * 效果器信息表
 ******************************************************************************/
typedef struct {
    EffectId_t id;
    const char *name;
    const char *description;
} EffectInfo_t;

static const EffectInfo_t g_EffectInfoTable[] = {
    { EFFECT_ID_REVERB,         "reverb",       "混响效果" },
    { EFFECT_ID_DRC,            "drc",          "动态范围压缩 (麦克风)" },
    { EFFECT_ID_EQ,             "eq",           "均衡器 (麦克风输出)" },
    { EFFECT_ID_EXPANDER,       "expander",     "扩展器" },
    { EFFECT_ID_ECHO,           "echo",         "回声效果" },
    { EFFECT_ID_HOWLING,        "howling",      "啸叫抑制" },
    { EFFECT_ID_3D,             "3d",           "3D音效" },
    { EFFECT_ID_VIRTUAL_BASS,   "vbass",        "虚拟低音" },
    { EFFECT_ID_PLATE_REVERB,   "plate_reverb", "板式混响" },
    { EFFECT_ID_MUSIC_DRC,      "music_drc",    "动态范围压缩 (音乐)" },
    { EFFECT_ID_MUSIC_EQ,       "music_eq",     "均衡器 (音乐输出)" },
};

#define EFFECT_TABLE_SIZE (sizeof(g_EffectInfoTable) / sizeof(g_EffectInfoTable[0]))

/*******************************************************************************
 * 内部函数前向声明
 ******************************************************************************/
static int CmdList(int argc, char *argv[]);
static int CmdInfo(int argc, char *argv[]);
static int CmdGet(int argc, char *argv[]);
static int CmdSet(int argc, char *argv[]);
static int CmdEnable(int argc, char *argv[]);

/*******************************************************************************
 * 内部辅助函数
 ******************************************************************************/

/**
 * @brief Effect命令默认处理 - 用于模块系统
 */
static int EffectModuleHandler(int argc, char *argv[])
{
    /* argc 不包含模块名本身，argv[0] 是第一个参数 */
    /* 重新构建完整的 argc/argv 供 ShellCmdEffect_Execute 使用 */
    char *fullArgv[32];  /* 假设最多32个参数 */
    int fullArgc = argc + 1;
    int i;
    
    fullArgv[0] = "effect";  /* 模块名 */
    for (i = 0; i < argc && i < 31; i++) {
        fullArgv[i + 1] = argv[i];
    }
    
    return ShellCmdEffect_Execute(fullArgc, fullArgv);
}

/**
 * @brief Effect命令选项定义（使用默认选项模式）
 */
static const ShellOpt_t g_EffectOpts[] = {
    { "", NULL, "[subcmd] [args]", "Audio Effect Parameter Control", EffectModuleHandler },
    OPT_END()
};

/**
 * @brief Effect命令模块定义
 */
static const ShellModule_t g_EffectModule = {
    "effect",
    "Audio Effect Parameter Control",
    MOD_CAT_AUDIO,
    g_EffectOpts,
    1
};

/**
 * @brief 打印帮助信息
 */
static void PrintHelp(void)
{
    Shell_Printf("\n===== Audio Effect Commands =====\n");
    Shell_Printf("effect list                  - List all effects\n");
    Shell_Printf("effect info <id>             - Show effect details\n");
    Shell_Printf("effect set <id> <param> <val>- Set effect parameter\n");
    Shell_Printf("effect get <id> <param>      - Get effect parameter\n");
    Shell_Printf("effect enable <id> [on|off]  - Enable/disable effect\n");
    Shell_Printf("effect help                  - Show this help\n");
    Shell_Printf("==================================\n\n");
}

/**
 * @brief 列出所有效果器
 */
static int CmdList(int argc, char *argv[])
{
    uint8_t i;
    
    Shell_Printf("\n===== Audio Effects [%d] =====\n", EFFECT_TABLE_SIZE);
    
    for (i = 0; i < EFFECT_TABLE_SIZE; i++) {
        const EffectInfo_t *info = &g_EffectInfoTable[i];
        bool enabled = Effect_GetEnabled(info->id);
        
        Shell_Printf("[%2d] %-15s - %s %s\n", 
                     info->id,
                     info->name,
                     info->description,
                     enabled ? "[ON]" : "[OFF]");
    }
    
    Shell_Printf("=============================\n\n");
    return 0;
}

/**
 * @brief 显示指定效果器的详细信息
 */
static int CmdInfo(int argc, char *argv[])
{
    if (argc < 3) {
        Shell_Printf("Usage: effect info <id>\n");
        return -1;
    }
    
    EffectId_t id = (EffectId_t)atoi(argv[2]);
    
    if (id >= EFFECT_ID_MAX) {
        Shell_Printf("ERROR: Invalid effect ID [0-%d]\n", EFFECT_ID_MAX - 1);
        return -1;
    }
    
    const char *name = Effect_GetName(id);
    bool enabled = Effect_GetEnabled(id);
    
    Shell_Printf("\n===== Effect %d: %s =====\n", id, name);
    Shell_Printf("Status:     %s\n", enabled ? "Enabled" : "Disabled");
    
    /* 根据效果器类型显示不同的参数 */
    switch (id) {
        case EFFECT_ID_REVERB:
            Shell_Printf("Available params:\n");
            Shell_Printf("  room   - Room size (0-100)\n");
            Shell_Printf("  damp   - Damping (0-100)\n");
            Shell_Printf("  wet    - Wet/Dry mix (0-100)\n");
            break;
            
        case EFFECT_ID_DRC:
        case EFFECT_ID_MUSIC_DRC:
            Shell_Printf("Available params:\n");
            Shell_Printf("  threshold - Threshold (dB)\n");
            Shell_Printf("  ratio     - Compression ratio\n");
            Shell_Printf("  attack    - Attack time (ms)\n");
            Shell_Printf("  release   - Release time (ms)\n");
            break;
            
        case EFFECT_ID_EQ:
        case EFFECT_ID_MUSIC_EQ:
            Shell_Printf("Available params:\n");
            Shell_Printf("  band<n> - Band n gain (dB), n=0-9\n");
            Shell_Printf("  gain    - Pre-gain\n");
            break;
            
        case EFFECT_ID_EXPANDER:
            Shell_Printf("Available params:\n");
            Shell_Printf("  threshold - Threshold (dB)\n");
            Shell_Printf("  ratio     - Expansion ratio\n");
            break;
            
        case EFFECT_ID_ECHO:
            Shell_Printf("Available params:\n");
            Shell_Printf("  delay     - Delay time (ms)\n");
            Shell_Printf("  feedback  - Feedback amount (0-100)\n");
            Shell_Printf("  wet       - Wet/Dry mix (0-100)\n");
            break;
            
        default:
            Shell_Printf("No parameters available for this effect\n");
            break;
    }
    
    Shell_Printf("==========================\n\n");
    return 0;
}

/**
 * @brief 获取效果器参数
 */
static int CmdGet(int argc, char *argv[])
{
    if (argc < 4) {
        Shell_Printf("Usage: effect get <id> <param>\n");
        return -1;
    }
    
    EffectId_t id = (EffectId_t)atoi(argv[2]);
    const char *param = argv[3];
    
    if (id >= EFFECT_ID_MAX) {
        Shell_Printf("ERROR: Invalid effect ID [0-%d]\n", EFFECT_ID_MAX - 1);
        return -1;
    }
    
    Shell_Printf("[Effect %d] Getting parameter '%s'...\n", id, param);
    
    /* 根据效果器ID和参数名读取参数值 */
    switch (id) {
        case EFFECT_ID_REVERB:
            if (strcmp(param, "room") == 0) {
                Shell_Printf("Reverb room_size: %d\n", gCtrlVars.reverb_unit.enable);
            } else if (strcmp(param, "damp") == 0) {
                Shell_Printf("Reverb damping: (not accessible)\n");
            } else if (strcmp(param, "wet") == 0) {
                Shell_Printf("Reverb wet_dry: (not accessible)\n");
            } else {
                Shell_Printf("ERROR: Unknown parameter '%s'\n", param);
                return -1;
            }
            break;
            
        case EFFECT_ID_DRC:
            if (strcmp(param, "threshold") == 0) {
                Shell_Printf("DRC threshold: %ld dB\n", (long)gCtrlVars.mic_drc_unit.threshold[0]);
            } else if (strcmp(param, "ratio") == 0) {
                Shell_Printf("DRC ratio: %ld\n", (long)gCtrlVars.mic_drc_unit.ratio[0]);
            } else {
                Shell_Printf("ERROR: Unknown parameter '%s'\n", param);
                return -1;
            }
            break;
            
        case EFFECT_ID_EXPANDER:
            if (strcmp(param, "threshold") == 0) {
                Shell_Printf("Expander threshold: %ld dB\n", (long)gCtrlVars.mic_expander_unit.threshold);
            } else if (strcmp(param, "ratio") == 0) {
                Shell_Printf("Expander ratio: %ld\n", (long)gCtrlVars.mic_expander_unit.ratio);
            } else {
                Shell_Printf("ERROR: Unknown parameter '%s'\n", param);
                return -1;
            }
            break;
            
        default:
            Shell_Printf("Effect %d does not support parameter reading\n", id);
            return -1;
    }
    
    return 0;
}

/**
 * @brief 设置效果器参数
 */
static int CmdSet(int argc, char *argv[])
{
    if (argc < 5) {
        Shell_Printf("Usage: effect set <id> <param> <value>\n");
        return -1;
    }
    
    EffectId_t id = (EffectId_t)atoi(argv[2]);
    const char *param = argv[3];
    int32_t value = atoi(argv[4]);
    
    if (id >= EFFECT_ID_MAX) {
        Shell_Printf("ERROR: Invalid effect ID [0-%d]\n", EFFECT_ID_MAX - 1);
        return -1;
    }
    
    Shell_Printf("[Effect %d] Setting parameter '%s' to %ld...\n", id, param, (long)value);
    
    /* 根据效果器ID和参数名设置参数值 */
    switch (id) {
        case EFFECT_ID_REVERB:
            if (strcmp(param, "room") == 0) {
                Shell_Printf("Setting Reverb room_size to %ld\n", (long)value);
            } else if (strcmp(param, "damp") == 0) {
                Shell_Printf("Setting Reverb damping to %ld\n", (long)value);
            } else if (strcmp(param, "wet") == 0) {
                Shell_Printf("Setting Reverb wet_dry to %ld\n", (long)value);
            } else {
                Shell_Printf("ERROR: Unknown parameter '%s'\n", param);
                return -1;
            }
            break;
            
        case EFFECT_ID_DRC:
            if (strcmp(param, "threshold") == 0) {
                gCtrlVars.mic_drc_unit.threshold[0] = value;
                Shell_Printf("DRC threshold set to %ld dB\n", (long)value);
            } else if (strcmp(param, "ratio") == 0) {
                gCtrlVars.mic_drc_unit.ratio[0] = value;
                Shell_Printf("DRC ratio set to %ld\n", (long)value);
            } else if (strcmp(param, "attack") == 0) {
                gCtrlVars.mic_drc_unit.attack_tc[0] = value;
                Shell_Printf("DRC attack set to %ld ms\n", (long)value);
            } else if (strcmp(param, "release") == 0) {
                gCtrlVars.mic_drc_unit.release_tc[0] = value;
                Shell_Printf("DRC release set to %ld ms\n", (long)value);
            } else {
                Shell_Printf("ERROR: Unknown parameter '%s'\n", param);
                return -1;
            }
            break;
            
        case EFFECT_ID_EXPANDER:
            if (strcmp(param, "threshold") == 0) {
                gCtrlVars.mic_expander_unit.threshold = value;
                Shell_Printf("Expander threshold set to %ld dB\n", (long)value);
            } else if (strcmp(param, "ratio") == 0) {
                gCtrlVars.mic_expander_unit.ratio = value;
                Shell_Printf("Expander ratio set to %ld\n", (long)value);
            } else {
                Shell_Printf("ERROR: Unknown parameter '%s'\n", param);
                return -1;
            }
            break;
            
        case EFFECT_ID_ECHO:
            if (strcmp(param, "delay") == 0) {
                gCtrlVars.echo_unit.delay = value;
                Shell_Printf("Echo delay set to %ld ms\n", (long)value);
            } else if (strcmp(param, "feedback") == 0) {
                gCtrlVars.echo_unit.attenuation = value;
                Shell_Printf("Echo feedback set to %ld\n", (long)value);
            } else {
                Shell_Printf("ERROR: Unknown parameter '%s'\n", param);
                return -1;
            }
            break;
            
        default:
            Shell_Printf("Effect %d does not support parameter setting\n", id);
            return -1;
    }
    
    return 0;
}

/**
 * @brief 启用/禁用效果器
 */
static int CmdEnable(int argc, char *argv[])
{
    if (argc < 3) {
        Shell_Printf("Usage: effect enable <id> [on|off]\n");
        return -1;
    }
    
    EffectId_t id = (EffectId_t)atoi(argv[2]);
    
    if (id >= EFFECT_ID_MAX) {
        Shell_Printf("ERROR: Invalid effect ID [0-%d]\n", EFFECT_ID_MAX - 1);
        return -1;
    }
    
    if (argc >= 4) {
        bool enabled = (strcmp(argv[3], "on") == 0);
        Effect_SetEnabled(id, enabled);
        Shell_Printf("Effect '%s' %s\n", Effect_GetName(id), enabled ? "enabled" : "disabled");
    } else {
        bool enabled = Effect_GetEnabled(id);
        Shell_Printf("Effect '%s' is currently %s\n", Effect_GetName(id), enabled ? "enabled" : "disabled");
    }
    
    return 0;
}

/*******************************************************************************
 * 公共API实现
 ******************************************************************************/

/**
 * @brief 注册效果器Shell命令
 */
void ShellCmdEffect_Register(void)
{
    Shell_RegisterModule(&g_EffectModule);
    ShellFs_RegisterCommand("effect");
    DBG("[ShellCmdEffect] Registered\n");
}

/**
 * @brief 通过ID获取效果器使能状态
 */
bool Effect_GetEnabled(EffectId_t id)
{
    switch (id) {
        case EFFECT_ID_REVERB:
            return gCtrlVars.reverb_unit.enable != 0;
        case EFFECT_ID_DRC:
            return gCtrlVars.mic_drc_unit.enable != 0;
        case EFFECT_ID_EQ:
            return gCtrlVars.mic_out_eq_unit.enable != 0;
        case EFFECT_ID_EXPANDER:
            return gCtrlVars.mic_expander_unit.enable != 0;
        case EFFECT_ID_ECHO:
            return gCtrlVars.echo_unit.enable != 0;
        case EFFECT_ID_HOWLING:
            return gCtrlVars.howling_dector_unit.enable != 0;
        case EFFECT_ID_PLATE_REVERB:
            return gCtrlVars.plate_reverb_unit.enable != 0;
        case EFFECT_ID_MUSIC_DRC:
            return gCtrlVars.music_drc_unit.enable != 0;
        default:
            return false;
    }
}

/**
 * @brief 通过ID设置效果器使能状态
 */
int Effect_SetEnabled(EffectId_t id, bool enabled)
{
    switch (id) {
        case EFFECT_ID_REVERB:
            gCtrlVars.reverb_unit.enable = enabled ? 1 : 0;
            break;
        case EFFECT_ID_DRC:
            gCtrlVars.mic_drc_unit.enable = enabled ? 1 : 0;
            break;
        case EFFECT_ID_EQ:
            gCtrlVars.mic_out_eq_unit.enable = enabled ? 1 : 0;
            break;
        case EFFECT_ID_EXPANDER:
            gCtrlVars.mic_expander_unit.enable = enabled ? 1 : 0;
            break;
        case EFFECT_ID_ECHO:
            gCtrlVars.echo_unit.enable = enabled ? 1 : 0;
            break;
        case EFFECT_ID_HOWLING:
            gCtrlVars.howling_dector_unit.enable = enabled ? 1 : 0;
            break;
        case EFFECT_ID_PLATE_REVERB:
            gCtrlVars.plate_reverb_unit.enable = enabled ? 1 : 0;
            break;
        case EFFECT_ID_MUSIC_DRC:
            gCtrlVars.music_drc_unit.enable = enabled ? 1 : 0;
            break;
        default:
            return -1;
    }
    return 0;
}

/**
 * @brief 获取效果器名称
 */
const char* Effect_GetName(EffectId_t id)
{
    if (id >= EFFECT_TABLE_SIZE) {
        return "unknown";
    }
    return g_EffectInfoTable[id].name;
}

/**
 * @brief Shell命令处理入口
 * @param argc 参数个数
 * @param argv 参数列表
 * @return 0成功，其他失败
 */
int ShellCmdEffect_Execute(int argc, char *argv[])
{
    if (argc < 2) {
        PrintHelp();
        return 0;
    }
    
    const char *subcmd = argv[1];
    
    if (strcmp(subcmd, "help") == 0) {
        PrintHelp();
        return 0;
    }
    else if (strcmp(subcmd, "list") == 0) {
        return CmdList(argc, argv);
    }
    else if (strcmp(subcmd, "info") == 0) {
        return CmdInfo(argc, argv);
    }
    else if (strcmp(subcmd, "get") == 0) {
        return CmdGet(argc, argv);
    }
    else if (strcmp(subcmd, "set") == 0) {
        return CmdSet(argc, argv);
    }
    else if (strcmp(subcmd, "enable") == 0) {
        return CmdEnable(argc, argv);
    }
    else {
        Shell_Printf("ERROR: Unknown command '%s'\n", subcmd);
        PrintHelp();
        return -1;
    }
}

/**
 * @brief 获取DRC参数 (动态范围压缩)
 */
int Effect_GetDRCParam(EffectId_t id, const char *param_name, int32_t *value)
{
    if (!param_name || !value) {
        return -1;
    }
    
    if (id != EFFECT_ID_DRC && id != EFFECT_ID_MUSIC_DRC) {
        return -1;
    }
    
    if (strcmp(param_name, "threshold") == 0) {
        *value = (id == EFFECT_ID_DRC) ? 
                 gCtrlVars.mic_drc_unit.threshold[0] : 
                 gCtrlVars.music_drc_unit.threshold[0];
    } else if (strcmp(param_name, "ratio") == 0) {
        *value = (id == EFFECT_ID_DRC) ? 
                 gCtrlVars.mic_drc_unit.ratio[0] : 
                 gCtrlVars.music_drc_unit.ratio[0];
    } else if (strcmp(param_name, "attack") == 0) {
        *value = (id == EFFECT_ID_DRC) ? 
                 gCtrlVars.mic_drc_unit.attack_tc[0] : 
                 gCtrlVars.music_drc_unit.attack_tc[0];
    } else if (strcmp(param_name, "release") == 0) {
        *value = (id == EFFECT_ID_DRC) ? 
                 gCtrlVars.mic_drc_unit.release_tc[0] : 
                 gCtrlVars.music_drc_unit.release_tc[0];
    } else {
        return -1;
    }
    
    return 0;
}

/**
 * @brief 设置DRC参数 (动态范围压缩)
 */
int Effect_SetDRCParam(EffectId_t id, const char *param_name, int32_t value)
{
    if (!param_name) {
        return -1;
    }
    
    if (id != EFFECT_ID_DRC && id != EFFECT_ID_MUSIC_DRC) {
        return -1;
    }
    
    if (strcmp(param_name, "threshold") == 0) {
        if (id == EFFECT_ID_DRC) {
            gCtrlVars.mic_drc_unit.threshold[0] = value;
        } else {
            gCtrlVars.music_drc_unit.threshold[0] = value;
        }
    } else if (strcmp(param_name, "ratio") == 0) {
        if (id == EFFECT_ID_DRC) {
            gCtrlVars.mic_drc_unit.ratio[0] = value;
        } else {
            gCtrlVars.music_drc_unit.ratio[0] = value;
        }
    } else if (strcmp(param_name, "attack") == 0) {
        if (id == EFFECT_ID_DRC) {
            gCtrlVars.mic_drc_unit.attack_tc[0] = value;
        } else {
            gCtrlVars.music_drc_unit.attack_tc[0] = value;
        }
    } else if (strcmp(param_name, "release") == 0) {
        if (id == EFFECT_ID_DRC) {
            gCtrlVars.mic_drc_unit.release_tc[0] = value;
        } else {
            gCtrlVars.music_drc_unit.release_tc[0] = value;
        }
    } else {
        return -1;
    }
    
    return 0;
}

/**
 * @brief 获取混响参数
 */
int Effect_GetReverbParam(const char *param_name, int32_t *value)
{
    if (!param_name || !value) {
        return -1;
    }
    
    Shell_Printf("Effect_GetReverbParam: Not fully implemented\n");
    return 0;
}

/**
 * @brief 设置混响参数
 */
int Effect_SetReverbParam(const char *param_name, int32_t value)
{
    if (!param_name) {
        return -1;
    }
    
    Shell_Printf("Effect_SetReverbParam: %s = %ld\n", param_name, (long)value);
    return 0;
}

/**
 * @brief 获取EQ参数
 */
int Effect_GetEQBandGain(EffectId_t id, uint8_t band_index, int8_t *gain)
{
    if (!gain || band_index >= 10) {
        return -1;
    }
    
    if (id == EFFECT_ID_EQ) {
        if (gCtrlVars.mic_out_eq_unit.filter_count > band_index) {
            *gain = gCtrlVars.mic_out_eq_unit.eq_params[band_index].gain >> 8; /* 转换Q8.8格式 */
            return 0;
        }
    } else if (id == EFFECT_ID_MUSIC_EQ) {
        if (gCtrlVars.music_drc_unit.enable) { /* 音乐EQ的检查逻辑 */
            *gain = 0;
            return 0;
        }
    }
    
    return -1;
}

/**
 * @brief 设置EQ参数
 */
int Effect_SetEQBandGain(EffectId_t id, uint8_t band_index, int8_t gain)
{
    if (band_index >= 10) {
        return -1;
    }
    
    if (id == EFFECT_ID_EQ) {
        if (gCtrlVars.mic_out_eq_unit.filter_count > band_index) {
            gCtrlVars.mic_out_eq_unit.eq_params[band_index].gain = (gain << 8); /* 转换为Q8.8格式 */
            return 0;
        }
    } else if (id == EFFECT_ID_MUSIC_EQ) {
        Shell_Printf("Effect_SetEQBandGain: Music EQ band %d = %d dB\n", band_index, gain);
        return 0;
    }
    
    return -1;
}

/**
 * @brief 重置效果器为默认参数
 */
int Effect_Reset(EffectId_t id)
{
    Shell_Printf("Resetting effect %d to default parameters...\n", id);
    
    switch (id) {
        case EFFECT_ID_REVERB:
            gCtrlVars.reverb_unit.enable = 1;
            break;
        case EFFECT_ID_DRC:
            gCtrlVars.mic_drc_unit.threshold[0] = -20;
            gCtrlVars.mic_drc_unit.ratio[0] = 4;
            break;
        case EFFECT_ID_EXPANDER:
            gCtrlVars.mic_expander_unit.threshold = -60;
            gCtrlVars.mic_expander_unit.ratio = 2;
            break;
        default:
            Shell_Printf("No reset available for effect %d\n", id);
            return -1;
    }
    
    return 0;
}

/**
 * @brief 保存所有效果器参数到非易失性存储
 */
int Effect_SaveConfig(void)
{
    Shell_Printf("Saving all effect parameters...\n");
    /* TODO: 实现Flash保存逻辑 */
    return 0;
}

/**
 * @brief 从非易失性存储加载所有效果器参数
 */
int Effect_LoadConfig(void)
{
    Shell_Printf("Loading effect parameters from storage...\n");
    /* TODO: 实现Flash加载逻辑 */
    return 0;
}

/**
 * @brief 打印指定效果器的所有参数
 */
int Effect_PrintAllParams(EffectId_t id)
{
    Shell_Printf("\n===== Effect %d Parameters =====\n", id);
    
    switch (id) {
        case EFFECT_ID_REVERB:
            Shell_Printf("Reverb:\n");
            Shell_Printf("  Enable: %d\n", gCtrlVars.reverb_unit.enable);
            break;
            
        case EFFECT_ID_DRC:
            Shell_Printf("DRC (Mic):\n");
            Shell_Printf("  Enable:    %d\n", gCtrlVars.mic_drc_unit.enable);
            Shell_Printf("  Threshold: %ld dB\n", (long)gCtrlVars.mic_drc_unit.threshold[0]);
            Shell_Printf("  Ratio:     %ld\n", (long)gCtrlVars.mic_drc_unit.ratio[0]);
            Shell_Printf("  Attack:    %ld ms\n", (long)gCtrlVars.mic_drc_unit.attack_tc[0]);
            Shell_Printf("  Release:   %ld ms\n", (long)gCtrlVars.mic_drc_unit.release_tc[0]);
            break;
            
        case EFFECT_ID_EXPANDER:
            Shell_Printf("Expander:\n");
            Shell_Printf("  Enable:    %d\n", gCtrlVars.mic_expander_unit.enable);
            Shell_Printf("  Threshold: %ld dB\n", (long)gCtrlVars.mic_expander_unit.threshold);
            Shell_Printf("  Ratio:     %ld\n", (long)gCtrlVars.mic_expander_unit.ratio);
            break;
            
        case EFFECT_ID_ECHO:
            Shell_Printf("Echo:\n");
            Shell_Printf("  Enable:   %d\n", gCtrlVars.echo_unit.enable);
            Shell_Printf("  Delay:    %ld ms\n", (long)gCtrlVars.echo_unit.delay);
            Shell_Printf("  Feedback: %ld\n", (long)gCtrlVars.echo_unit.attenuation);
            break;
            
        default:
            Shell_Printf("No detailed parameters available for effect %d\n", id);
            return -1;
    }
    
    Shell_Printf("================================\n\n");
    return 0;
}
