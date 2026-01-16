# 快速开始指南

## 🚀 5分钟快速开始

### 第一步：编译
```bash
cd BanBox/Debug
make clean
make -j4
```

如果编译成功，你会看到：
```
Finished building: BanBox.elf
Build complete: 0 errors
```

### 第二步：烧录固件
使用J-Link或IDE烧录工具烧录 `output/BanBox.elf` 到设备

### 第三步：连接串口
用USB数据线连接设备，打开串口工具（115200 8N1）

### 第四步：测试命令
```bash
$ help -a
# 应该看到 graph, fx, effect, audio 命令

$ audio list
# 应该看到 graph0

$ ls /audio
# 应该看到 graph0/

$ cd /audio/graph0/nodes
$ ls
# 应该看到所有节点
```

## 📝 常用命令速查

### 查看参数
```bash
fx 3                    # 查看节点3的所有参数
fx 3 threshold          # 查看threshold值
cat /audio/graph0/nodes/3_drc/threshold
```

### 修改参数
```bash
fx 3 threshold -20      # 设置threshold为-20
echo -20 > /audio/graph0/nodes/3_drc/threshold
```

### 快速导航
```bash
cd /audio/graph0/nodes/3_drc
ls                      # 列出所有参数
cat enabled             # 查看状态
echo 0 > enabled        # 禁用节点
```

### 快照管理
```bash
graph snapshot save 0 "my_preset"    # 保存快照
graph snapshot list                  # 查看快照
graph snapshot load 0                # 加载快照
```

### 批量操作
```bash
graph allfx off         # 禁用所有效果
graph allbypass on      # 旁路所有效果
```

## 🎯 典型工作流

### 调试混响效果
```bash
# 1. 进入节点目录
$ cd /audio/graph0/nodes/5_reverb

# 2. 查看当前值
$ cat room
50
$ cat damp
30

# 3. 调整参数
$ echo 70 > room
$ echo 50 > damp

# 4. 保存配置
$ cd /audio/graph0
$ graph snapshot save 0 "reverb_preset"
```

### 调试DRC压缩
```bash
$ fx 3 threshold -20    # 查看阈值
$ fx 3 threshold -25    # 调低阈值
$ fx 3 ratio 6          # 设置比率为6
$ fx 3 attack 10        # 设置起音10ms
$ fx 3 release 100      # 设置释放100ms
```

## 🔧 故障排查

### 问题1：命令找不到
```bash
$ graph list
Unknown command
```

**解决**:
- 检查是否烧录了新固件
- 检查串口连接
- 尝试 `help -a` 查看可用命令

### 问题2：参数设置无效
```bash
$ fx 3 threshold -100
WARN: threshold out of range [-60~0]dB
```

**解决**:
- 参数值超出范围
- 查看提示信息确认范围
- 使用有效的参数值

### 问题3：VFS目录不存在
```bash
$ ls /audio
ERROR: Path not found
```

**解决**:
- 效果图未初始化
- 检查启动日志
- 尝试 `audio list` 看是否有效果图

### 问题4：无法创建新效果图
```bash
$ audio create graph1 0
ERROR: Failed to create graph
```

**解决**:
- 检查是否已达到最大图数（4个）
- 尝试删除旧图 `audio delete graph1`
- 检查内存是否充足

## 💡 实用技巧

### 技巧1：快速比较效果
```bash
# 保存当前设置
$ graph snapshot save 0 "current"

# 修改参数
$ fx 3 threshold -30

# 恢复对比
$ graph snapshot load 0
```

### 技巧2：批量测试
```bash
# 快速连续调整
$ fx 3 threshold -20
$ fx 3 threshold -22
$ fx 3 threshold -24
$ fx 3 threshold -26
$ fx 3 threshold -28
```

### 技巧3：查看所有可用参数
```bash
$ graph params 3
Available parameters:
  threshold    [-60~0] dB
  ratio        [1~20]
  attack       [1~500] ms
  release      [10~2000] ms
```

### 技巧4：导航快捷方式
```bash
# 直接访问任意文件
$ cat /audio/graph0/nodes/3_drc/threshold

# 从任何位置修改参数
$ echo -25 > /audio/graph0/nodes/3_drc/threshold

# 查看图信息
$ cat /audio/graph0/info
```

## 📊 命令对照表

| 需求 | graph命令 | fx命令 | VFS命令 |
|------|----------|---------|---------|
| 查看参数 | `graph get 3 threshold` | `fx 3 threshold` | `cat /audio/graph0/nodes/3_drc/threshold` |
| 修改参数 | `graph set 3 threshold -20` | `fx 3 threshold -20` | `echo -20 > /audio/graph0/nodes/3_drc/threshold` |
| 启用节点 | `graph node 3 on` | - | `echo 1 > /audio/graph0/nodes/3_drc/enabled` |
| 禁用节点 | `graph node 3 off` | - | `echo 0 > /audio/graph0/nodes/3_drc/enabled` |
| 快照保存 | `graph snapshot save 0` | - | - |
| 快照加载 | `graph snapshot load 0` | - | - |

## 🌟 高级用法

### 创建多个配置方案
```bash
# 方案A：高压缩
$ fx 3 threshold -25 & fx 3 ratio 8
$ graph snapshot save 0 "compression_high"

# 方案B：低压缩
$ fx 3 threshold -15 & fx 3 ratio 4
$ graph snapshot save 1 "compression_low"

# 方案C：中等压缩
$ fx 3 threshold -20 & fx 3 ratio 6
$ graph snapshot save 2 "compression_mid"

# 快速切换
$ graph snapshot load 0  # 切换到高压缩
$ graph snapshot load 1  # 切换到低压缩
```

### 参数A/B对比
```bash
# 保存A方案
$ graph snapshot save 0

# 调整参数为B方案
$ fx 5 room 80

# 对比效果后恢复A方案
$ graph snapshot load 0
```

## 📚 完整文档

- **AUDIO_VFS_GUIDE.md** - VFS详细说明
- **GRAPH_PARAMS_GUIDE.md** - 所有参数范围
- **BUILD_DEPLOY_GUIDE.md** - 编译部署步骤
- **SHELL_TEST_SCRIPT.md** - 完整测试脚本
- **PROJECT_COMPLETION_SUMMARY.md** - 项目总结

---

## ✅ 验证清单

烧录后检查以下功能是否正常：

- [ ] `help -a` 显示所有命令
- [ ] `graph list` 列出节点
- [ ] `fx 3 threshold` 能读取参数
- [ ] `fx 3 threshold -20` 能设置参数
- [ ] `audio list` 显示 graph0
- [ ] `ls /audio` 显示 graph0/
- [ ] `cd /audio/graph0` 能进入目录
- [ ] `ls nodes` 显示所有节点
- [ ] `cat threshold` 能读取参数
- [ ] 参数修改后有音频变化

---

**恭喜！** 如果以上测试都通过，说明系统正常工作！🎉
