/**
 *****************************************************************************
 * @file     shell_cmd_effect.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     06-January-2026
 * @brief    音频效果器Shell命令模块 - 通过ID查询和调节效果器参数
 * 
 * 命令格式:
 *   effect list                   - 列出所有效果器及其ID
 *   effect info <id>              - 显示指定效果器的详细参数
 *   effect set <id> <param> <val> - 设置效果器参数
 *   effect get <id> <param>       - 获取效果器参数
 *   effect enable <id> [on|off]   - 启用/禁用效果器
 *   effect help                   - 显示帮助信息
 * 
 * 支持的效果器:
 *   0: Reverb      - 混响
 *   1: DRC         - 动态范围压缩
 *   2: EQ          - 均衡器
 *   3: Expander    - 扩展器
 *   4: Echo        - 回声
 *   5: Howling     - 啸叫抑制
 *   6: 3D          - 3D音效
 *   7: VirtualBass - 虚拟低音
 *****************************************************************************
 */

#ifndef __SHELL_CMD_EFFECT_H__
#define __SHELL_CMD_EFFECT_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * 效果器ID定义
 * 
 * 2026-02-04 更新: 新增4个独立EQ效果器ID
 *   - EQ_GUITAR_L (4): 乐器左声道EQ
 *   - EQ_GUITAR_R (5): 乐器右声道EQ
 *   - EQ_MIC_L (6): 麦克风左声道EQ
 *   - EQ_MIC_R (7): 麦克风右声道EQ
 ******************************************************************************/
typedef enum {
    EFFECT_ID_REVERB = 0,       /* 混响 */
    EFFECT_ID_DRC,              /* 动态范围压缩 (麦克风通道) */
    EFFECT_ID_EQ,               /* 均衡器 (兼容旧版，映射到EQ_GUITAR_L) */
    EFFECT_ID_EXPANDER,         /* 扩展器 */
    EFFECT_ID_EQ_GUITAR_L,      /* 乐器左声道EQ */
    EFFECT_ID_EQ_GUITAR_R,      /* 乐器右声道EQ */
    EFFECT_ID_EQ_MIC_L,         /* 麦克风左声道EQ */
    EFFECT_ID_EQ_MIC_R,         /* 麦克风右声道EQ */
    EFFECT_ID_ECHO,             /* 回声 */
    EFFECT_ID_HOWLING,          /* 啸叫抑制 */
    EFFECT_ID_3D,               /* 3D音效 */
    EFFECT_ID_VIRTUAL_BASS,     /* 虚拟低音 */
    EFFECT_ID_PLATE_REVERB,     /* 板式混响 */
    EFFECT_ID_MUSIC_DRC,        /* 动态范围压缩 (音乐通道) */
    EFFECT_ID_MUSIC_EQ,         /* 均衡器 (音乐输出EQ/USB_BT_EQ) */
    EFFECT_ID_MAX
} EffectId_t;

/*******************************************************************************
 * 公共API
 ******************************************************************************/

/**
 * @brief 注册效果器Shell命令
 */
void ShellCmdEffect_Register(void);

/**
 * @brief 通过ID获取效果器使能状态
 * @param id 效果器ID
 * @return true=启用, false=禁用
 */
bool Effect_GetEnabled(EffectId_t id);

/**
 * @brief 通过ID设置效果器使能状态
 * @param id 效果器ID
 * @param enabled true=启用, false=禁用
 * @return 0=成功, -1=失败
 */
int Effect_SetEnabled(EffectId_t id, bool enabled);

/**
 * @brief 获取效果器名称
 * @param id 效果器ID
 * @return 效果器名称字符串
 */
const char* Effect_GetName(EffectId_t id);

/**
 * @brief 效果器命令处理入口
 * @param argc 参数个数
 * @param argv 参数列表
 * @return 0成功，其他失败
 */
int ShellCmdEffect_Execute(int argc, char *argv[]);

/**
 * @brief 获取DRC参数 (动态范围压缩)
 * @param id 效果器ID
 * @param param_name 参数名 ("threshold", "ratio", "attack", "release")
 * @param value 输出参数值指针
 * @return 0成功，-1失败
 */
int Effect_GetDRCParam(EffectId_t id, const char *param_name, int32_t *value);

/**
 * @brief 设置DRC参数 (动态范围压缩)
 * @param id 效果器ID
 * @param param_name 参数名 ("threshold", "ratio", "attack", "release")
 * @param value 参数值
 * @return 0成功，-1失败
 */
int Effect_SetDRCParam(EffectId_t id, const char *param_name, int32_t value);

/**
 * @brief 获取混响参数
 * @param param_name 参数名 ("room", "damp", "wet")
 * @param value 输出参数值指针
 * @return 0成功，-1失败
 */
int Effect_GetReverbParam(const char *param_name, int32_t *value);

/**
 * @brief 设置混响参数
 * @param param_name 参数名 ("room", "damp", "wet")
 * @param value 参数值
 * @return 0成功，-1失败
 */
int Effect_SetReverbParam(const char *param_name, int32_t value);

/**
 * @brief 获取EQ参数
 * @param id 效果器ID
 * @param band_index 频段索引 (0-9)
 * @param gain 输出增益值指针 (dB)
 * @return 0成功，-1失败
 */
int Effect_GetEQBandGain(EffectId_t id, uint8_t band_index, int8_t *gain);

/**
 * @brief 设置EQ参数
 * @param id 效果器ID
 * @param band_index 频段索引 (0-9)
 * @param gain 增益值 (dB)
 * @return 0成功，-1失败
 */
int Effect_SetEQBandGain(EffectId_t id, uint8_t band_index, int8_t gain);

/**
 * @brief 重置效果器为默认参数
 * @param id 效果器ID
 * @return 0成功，-1失败
 */
int Effect_Reset(EffectId_t id);

/**
 * @brief 保存所有效果器参数到非易失性存储
 * @return 0成功，-1失败
 */
int Effect_SaveConfig(void);

/**
 * @brief 从非易失性存储加载所有效果器参数
 * @return 0成功，-1失败
 */
int Effect_LoadConfig(void);

/**
 * @brief 打印指定效果器的所有参数
 * @param id 效果器ID
 * @return 0成功，-1失败
 */
int Effect_PrintAllParams(EffectId_t id);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_EFFECT_H__ */
