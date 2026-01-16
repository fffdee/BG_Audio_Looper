/**
 *****************************************************************************
 * @file     shell_cmd_graph.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     04-January-2026
 * @brief    音频效果图Shell命令模块 - 通过CDC/BLE命令行配置音频图
 * 
 * 命令格式:
 *   graph list                    - 列出所有节点
 *   graph info                    - 显示图详细信息
 *   graph preset [id]             - 切换/显示预设
 *   graph node <name> [on|off]    - 启用/禁用节点
 *   graph bypass <name> [on|off]  - 设置节点旁路
 *   graph param <name> <key> [val]- 读取/设置节点参数
 *   graph connect <src> <dst>     - 连接两个节点
 *   graph disconnect <src> <dst>  - 断开两个节点
 *   graph rebuild                 - 重建图
 *   graph save                    - 保存配置到Flash
 *   graph load                    - 从Flash加载配置
 *****************************************************************************
 */

#ifndef __SHELL_CMD_GRAPH_H__
#define __SHELL_CMD_GRAPH_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * 初始化函数
 ******************************************************************************/

/**
 * @brief 注册效果图Shell命令
 */
void ShellCmdGraph_Register(void);

/**
 * @brief 效果图命令处理入口
 * @param argc 参数个数
 * @param argv 参数列表
 * @return 0成功，其他失败
 */
int ShellCmdGraph_Execute(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_GRAPH_H__ */
