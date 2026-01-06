/**
 *****************************************************************************
 * @file     shell_cmd_graph.h
 * @author   BG Card Team
 * @version  V1.2.0
 * @date     06-January-2026
 * @brief    音频效果图Shell命令模块 - 通过CDC/BLE命令行配置音频图
 * 
 * graph 命令格式:
 *   graph list                    - 列出所有节点
 *   graph info                    - 显示图详细信息
 *   graph preset [id]             - 切换/显示预设
 *   graph node <id|name> [on|off] - 启用/禁用节点 (支持ID或名称)
 *   graph bypass <id|name> [on|off]- 设置节点旁路
 *   graph get <id|name> [param]   - 获取节点参数 (无param则显示全部)
 *   graph set <id|name> <param> <val> - 设置节点参数
 *   graph params <id|name>        - 显示节点可用参数及范围
 *   graph rebuild                 - 重建图
 * 
 * 批量操作:
 *   graph allfx <on|off>          - 批量启用/禁用所有效果
 *   graph allbypass <on|off>      - 批量旁路所有效果
 * 
 * 快照管理:
 *   graph snapshot save <slot> [name] - 保存状态到槽位
 *   graph snapshot load <slot>    - 从槽位加载状态
 *   graph snapshot list           - 列出所有快照
 * 
 * fx 快捷命令 (通过ID快速访问):
 *   fx <id>                       - 显示节点所有参数
 *   fx <id> <param>               - 获取指定参数
 *   fx <id> <param> <value>       - 设置参数值
 * 
 * 示例:
 *   graph list                    - 列出所有节点及ID
 *   graph get 3                   - 查看节点3的所有参数
 *   graph set 3 threshold -20     - 设置节点3的threshold为-20
 *   graph params 3                - 查看节点3支持的参数及范围
 *   graph snapshot save 0 "clean" - 保存当前状态到槽位0
 *   graph allfx off               - 关闭所有效果
 *   fx 3 threshold -20            - 快捷设置参数
 *****************************************************************************
 */

#ifndef __SHELL_CMD_GRAPH_H__
#define __SHELL_CMD_GRAPH_H__

#include <stdint.h>
#include <stdbool.h>
#include "bg_shell.h"  /* 需要 ShellModule_t, OPT 宏等定义 */

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
 * @brief graph命令处理入口
 * @param argc 参数个数
 * @param argv 参数列表
 * @return 0成功，其他失败
 */
int ShellCmdGraph_Execute(int argc, char *argv[]);

/**
 * @brief fx快捷命令入口 - 通过ID快速访问节点参数
 * @param argc 参数个数
 * @param argv 参数列表
 * @return 0成功，其他失败
 * 
 * 用法:
 *   fx <id>              - 显示节点信息
 *   fx <id> <param>      - 获取参数值
 *   fx <id> <param> <val>- 设置参数值
 */
int ShellCmdFx_Execute(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_CMD_GRAPH_H__ */
