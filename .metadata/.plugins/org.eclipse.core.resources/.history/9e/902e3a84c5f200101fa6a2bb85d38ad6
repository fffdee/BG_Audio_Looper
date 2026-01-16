# Shell命令测试脚本
# 通过CDC/BLE串口执行这些命令来测试效果图Shell系统

## 基本命令可用性测试
help -a                    # 应该列出 graph, fx, effect 命令

## graph 命令测试
graph list                 # 列出所有节点
graph info                 # 显示图详细信息
graph params 3             # 显示节点3的参数（假设是DRC）

## fx 快捷命令测试
fx 3                       # 显示节点3的所有参数
fx 3 threshold             # 获取threshold参数
fx 3 threshold -25         # 设置threshold参数

## effect 命令测试
effect list                # 列出所有效果器
effect info 1              # 显示效果器1的详情（假设是DRC）
effect get 1 threshold     # 获取参数
effect set 1 threshold -25 # 设置参数
effect enable 1 on         # 启用效果器

## 参数范围校验测试
fx 3 threshold -100        # 测试超范围参数（应警告但允许）
fx 3 invalid_param 10      # 测试无效参数（应显示可用参数列表）

## 快照功能测试
graph snapshot save 0 "test_preset"  # 保存快照
graph snapshot list                  # 列出所有快照
graph snapshot load 0                # 加载快照

## 批量操作测试
graph allfx off            # 禁用所有效果
graph list                 # 验证所有效果已禁用
graph allfx on             # 重新启用所有效果

## ==================================================
## ★新增★ VFS命令测试 - /audio 目录
## ==================================================

## VFS基础导航测试
$ ls /                     # 列出根目录（应包含 bin driver audio）
$ ls /audio                # 列出音频目录（应包含 graph0）
$ cd /audio                # 进入音频目录
$ pwd                      # 显示当前路径
$ ls                       # 列出效果图

## audio命令测试
$ audio list               # 列出所有效果图
$ audio info graph0        # 显示graph0详细信息

## 节点参数读取测试（通过VFS）
$ cd /audio/graph0
$ ls                       # 应显示 info preset node_count nodes/
$ cat info                 # 读取图信息
$ cat preset               # 读取当前预设
$ cat node_count           # 读取节点数量

## 进入节点目录
$ cd nodes
$ ls                       # 列出所有节点 (0_adc0, 3_drc, etc)
$ cd 3_drc                 # 进入DRC节点
$ ls                       # 列出DRC参数
$ cat enabled              # 读取启用状态
$ cat threshold            # 读取threshold参数
$ cat ratio                # 读取ratio参数

## 参数修改测试（如果支持echo重定向）
$ echo 0 > enabled         # 禁用DRC
$ cat enabled              # 验证已禁用
$ echo 1 > enabled         # 重新启用
$ echo -25 > threshold     # 修改threshold
$ cat threshold            # 验证修改

## 多节点访问测试
$ cd /audio/graph0/nodes/5_reverb
$ ls
$ cat room
$ cat damp
$ cat wet

## 快捷路径访问
$ cat /audio/graph0/nodes/3_drc/threshold
$ cat /audio/graph0/nodes/5_reverb/room

## 创建新效果图测试
$ audio create graph1 1    # 创建第二个图，使用预设1
$ audio list               # 应显示2个图
$ ls /audio                # 应显示 graph0 graph1
$ cd /audio/graph1
$ ls

## 删除效果图测试
$ audio delete graph1
$ audio list               # 应只剩graph0

## 重载效果图测试
$ audio reload graph0
$ audio info graph0

## 性能测试
# 快速连续调节参数，测试响应时间
fx 3 threshold -20
fx 3 threshold -22
fx 3 threshold -24
fx 3 threshold -26
fx 3 threshold -28
fx 3 threshold -30

## 错误处理测试
graph get 999 param        # 无效节点ID
fx abc                     # 无效节点ID格式
effect set 999 param val   # 无效效果器ID

## 综合场景测试：调试混响效果
effect list                # 找到混响效果器ID
effect info 0              # 查看混响参数
effect set 0 room 75       # 调节房间大小
effect set 0 damp 50       # 调节阻尼
effect set 0 wet 40        # 调节干湿比
effect enable 0 on         # 启用混响
graph snapshot save 1 "reverb_config"  # 保存配置

## 预期结果
# ✅ 所有命令应该被识别（不出现 "Unknown command"）
# ✅ 参数校验应正常工作（超范围警告，无效参数提示）
# ✅ 快照保存/加载应正常工作
# ✅ 批量操作应正常工作
# ✅ 节点控制应正常工作
# ❌ 如果出现 "Unknown command"，说明模块未正确注册

## 调试提示
# 如果命令无法识别：
# 1. 检查 Shell_RegisterAllModules() 是否调用了相应的注册函数
# 2. 检查模块定义中的选项数量是否正确
# 3. 检查是否实现了模块处理器适配器函数
# 4. 使用 help -a 查看所有已注册的命令

## 常见问题排查
# Q: effect命令无法识别？
# A: 检查是否实现了EffectModuleHandler适配器函数

# Q: graph命令工作但fx不工作？
# A: 检查FxModuleHandler是否正确注册

# Q: 参数设置无效？
# A: 检查是否调用了底层更新函数（如AudioEffectApply等）

# Q: 快照加载后效果没变化？
# A: 检查快照加载后是否触发了DSP参数刷新
