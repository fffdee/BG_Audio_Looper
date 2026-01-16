# Echo命令测试指南

## 概述
echo命令已成功实现并集成到Shell和VFS系统中。本文档提供详细的测试步骤和使用示例。

## 命令位置
- **Shell命令**: 在Shell命令行中直接使用`echo`
- **VFS路径**: `/bin/echo` (已注册到VFS)

## 命令语法

### 基本语法
```bash
# 格式1: 简化写法
echo <parameter_path> <value>

# 格式2: 重定向写法
echo <value> > <parameter_path>
```

## 使用场景

### 1. 写入音频参数

#### 场景1.1: 调整EQ增益
```bash
# 查看当前值
cat /audio/MainGraph/EQ/band0_gain

# 写入新值 (简化写法)
echo /audio/MainGraph/EQ/band0_gain 6

# 写入新值 (重定向写法)
echo 6 > /audio/MainGraph/EQ/band0_gain

# 验证修改
cat /audio/MainGraph/EQ/band0_gain
```

#### 场景1.2: 调整压缩器阈值
```bash
# 写入压缩器阈值
echo /audio/MainGraph/Compressor/threshold -20

# 或使用重定向
echo -20 > /audio/MainGraph/Compressor/threshold
```

#### 场景1.3: 调整混响参数
```bash
# 调整混响时间
echo /audio/MainGraph/Reverb/reverb_time 800

# 调整混响深度
echo /audio/MainGraph/Reverb/wet_dry 60
```

### 2. 批量参数修改

#### 场景2.1: 配置多段EQ
```bash
# 调整所有频段增益
echo /audio/MainGraph/EQ/band0_gain 3
echo /audio/MainGraph/EQ/band1_gain 2
echo /audio/MainGraph/EQ/band2_gain 0
echo /audio/MainGraph/EQ/band3_gain -2
echo /audio/MainGraph/EQ/band4_gain -3
```

#### 场景2.2: 配置压缩器链
```bash
# 压缩器参数配置
echo /audio/MainGraph/Compressor/threshold -15
echo /audio/MainGraph/Compressor/ratio 3
echo /audio/MainGraph/Compressor/attack 5
echo /audio/MainGraph/Compressor/release 50
```

### 3. 使用相对路径

```bash
# 先切换到参数目录
cd /audio/MainGraph/EQ

# 使用相对路径写入
echo band0_gain 6
echo band1_gain 4
echo band2_gain 0

# 切换到另一个节点
cd ../Compressor
echo threshold -18
echo ratio 4
```

### 4. 配合其他命令使用

#### 场景4.1: 修改前后对比
```bash
# 保存当前配置
cat /audio/MainGraph/EQ/band0_gain > old_value.txt

# 修改参数
echo /audio/MainGraph/EQ/band0_gain 8

# 对比新旧值
cat /audio/MainGraph/EQ/band0_gain
cat old_value.txt
```

#### 场景4.2: 快速重置参数
```bash
# 将所有EQ频段重置为0
echo /audio/MainGraph/EQ/band0_gain 0
echo /audio/MainGraph/EQ/band1_gain 0
echo /audio/MainGraph/EQ/band2_gain 0
echo /audio/MainGraph/EQ/band3_gain 0
echo /audio/MainGraph/EQ/band4_gain 0
```

## 错误处理

### 常见错误及解决方案

#### 错误1: No such file or directory
```bash
$ echo /audio/MainGraph/XXX/param 100
echo: /audio/MainGraph/XXX/param: No such file or directory
```
**原因**: 参数路径不存在  
**解决**: 使用`ls`或`tree`命令查看可用的参数路径

#### 错误2: Not a parameter file
```bash
$ echo /audio/MainGraph 100
echo: /audio/MainGraph: Not a parameter file
```
**原因**: 目标是目录而非参数文件  
**解决**: 确保路径指向具体的参数节点

#### 错误3: Read-only parameter
```bash
$ echo /audio/MainGraph/status 1
echo: /audio/MainGraph/status: Read-only parameter
```
**原因**: 参数只读，不允许写入  
**解决**: 查看参数的访问权限，只写入可写参数

#### 错误4: Missing operand
```bash
$ echo /audio/MainGraph/EQ/band0_gain
echo: missing operand
Usage: echo <parameter> <value>
```
**原因**: 缺少参数值  
**解决**: 提供要写入的值

## 完整测试流程

### 测试1: 基本写入功能
```bash
# 1. 列出可用参数
ls /audio/MainGraph/EQ

# 2. 查看当前值
cat /audio/MainGraph/EQ/band0_gain

# 3. 写入新值
echo /audio/MainGraph/EQ/band0_gain 5

# 4. 验证写入成功
cat /audio/MainGraph/EQ/band0_gain
# 预期输出: 5
```

### 测试2: 重定向语法
```bash
# 使用重定向语法写入
echo 8 > /audio/MainGraph/EQ/band1_gain

# 验证
cat /audio/MainGraph/EQ/band1_gain
# 预期输出: 8
```

### 测试3: 负数参数
```bash
# 写入负数
echo /audio/MainGraph/Compressor/threshold -25

# 验证
cat /audio/MainGraph/Compressor/threshold
# 预期输出: -25
```

### 测试4: 小数参数
```bash
# 写入小数（如果支持）
echo /audio/MainGraph/Reverb/wet_dry 0.65

# 验证
cat /audio/MainGraph/Reverb/wet_dry
# 预期输出: 0.65 或 65（取决于参数格式）
```

### 测试5: 相对路径
```bash
# 切换目录
cd /audio/MainGraph/EQ

# 使用相对路径
echo band2_gain 3

# 验证
cat band2_gain
# 预期输出: 3
```

### 测试6: 错误处理
```bash
# 测试不存在的路径
echo /audio/NonExist/param 100
# 预期: 错误提示

# 测试写入目录
echo /audio/MainGraph 100
# 预期: 错误提示

# 测试缺少参数
echo /audio/MainGraph/EQ/band0_gain
# 预期: 使用说明
```

## 集成测试脚本

### 脚本1: EQ完整配置
```bash
#!/bin/sh
# EQ配置脚本

echo "Configuring EQ..."

# 进入EQ目录
cd /audio/MainGraph/EQ

# 配置5段EQ
echo band0_gain 3
echo band1_gain 2
echo band2_gain 0
echo band3_gain -1
echo band4_gain -2

echo "EQ configured!"

# 显示配置
echo "Current EQ settings:"
cat band0_gain
cat band1_gain
cat band2_gain
cat band3_gain
cat band4_gain
```

### 脚本2: 动态效果调制
```bash
#!/bin/sh
# 动态效果调制脚本

echo "Starting dynamic effect modulation..."

# 循环调制混响深度
for i in 0 20 40 60 80 100; do
    echo /audio/MainGraph/Reverb/wet_dry $i
    sleep 1
done

echo "Modulation complete!"
```

### 脚本3: 参数快照
```bash
#!/bin/sh
# 保存/恢复参数快照

# 保存快照
echo "Saving snapshot..."
mkdir /tmp/snapshot
cat /audio/MainGraph/EQ/band0_gain > /tmp/snapshot/eq_b0
cat /audio/MainGraph/EQ/band1_gain > /tmp/snapshot/eq_b1
cat /audio/MainGraph/Compressor/threshold > /tmp/snapshot/comp_th

# 修改参数
echo "Modifying parameters..."
echo /audio/MainGraph/EQ/band0_gain 10
echo /audio/MainGraph/EQ/band1_gain 8
echo /audio/MainGraph/Compressor/threshold -10

# 恢复快照
echo "Restoring snapshot..."
cat /tmp/snapshot/eq_b0 | echo /audio/MainGraph/EQ/band0_gain
cat /tmp/snapshot/eq_b1 | echo /audio/MainGraph/EQ/band1_gain
cat /tmp/snapshot/comp_th | echo /audio/MainGraph/Compressor/threshold

echo "Snapshot restored!"
```

## 性能测试

### 测试写入速度
```bash
# 测试100次写入的性能
time (
    for i in $(seq 1 100); do
        echo /audio/MainGraph/EQ/band0_gain 5
    done
)
```

### 测试批量写入
```bash
# 测试批量参数写入
time (
    echo /audio/MainGraph/EQ/band0_gain 3
    echo /audio/MainGraph/EQ/band1_gain 2
    echo /audio/MainGraph/EQ/band2_gain 0
    echo /audio/MainGraph/EQ/band3_gain -2
    echo /audio/MainGraph/EQ/band4_gain -3
    echo /audio/MainGraph/Compressor/threshold -15
    echo /audio/MainGraph/Compressor/ratio 4
    echo /audio/MainGraph/Compressor/attack 10
    echo /audio/MainGraph/Compressor/release 100
    echo /audio/MainGraph/Reverb/reverb_time 500
)
```

## 调试技巧

### 1. 查看命令是否注册
```bash
# 检查echo命令是否在/bin中
ls /bin | grep echo

# 或直接列出
ls /bin/echo
```

### 2. 查看参数树结构
```bash
# 显示完整参数树
tree /audio

# 显示特定节点
tree /audio/MainGraph
```

### 3. 使用help查看命令帮助
```bash
# 查看echo命令帮助
help echo

# 或不带参数执行
echo
```

### 4. 启用调试输出
```bash
# 如果支持调试模式
sys -d on
echo /audio/MainGraph/EQ/band0_gain 5
sys -d off
```

## 常见使用场景

### 场景1: 现场演出EQ快速调整
```bash
# 根据场地调整EQ
cd /audio/MainGraph/EQ
echo band0_gain 2    # 低音稍提
echo band1_gain 0    # 中低平
echo band2_gain -1   # 中频削
echo band3_gain 1    # 中高提
echo band4_gain 3    # 高音提升
```

### 场景2: 录音棚精确参数设置
```bash
# 人声压缩配置
cd /audio/MainGraph/Compressor
echo threshold -18
echo ratio 3.5
echo attack 5
echo release 50
echo knee 2

# EQ美化
cd ../EQ
echo band0_gain -2   # 削减低频
echo band1_gain 0
echo band2_gain 2    # 提升中频
echo band3_gain 3    # 提升高频
echo band4_gain 1
```

### 场景3: 实时效果切换
```bash
# 切换到空间感效果
echo /audio/MainGraph/Reverb/wet_dry 80
echo /audio/MainGraph/Reverb/reverb_time 1200

# 切换回干声
echo /audio/MainGraph/Reverb/wet_dry 0
```

## 注意事项

1. **参数范围**: 确保写入的值在参数允许的范围内
2. **参数类型**: 注意参数的类型（整数/浮点数）
3. **实时性**: 参数修改会立即生效，请谨慎操作
4. **权限检查**: 某些参数可能只读，无法通过echo修改
5. **路径正确性**: 确保参数路径完全正确，包括大小写

## 下一步扩展

### 计划功能
1. 支持参数范围检查和警告
2. 支持参数值的预览（修改前显示当前值）
3. 支持批量参数文件导入
4. 支持参数变更日志记录
5. 支持参数变更的撤销/重做功能

## 故障排查

### 问题1: echo命令不可用
**检查步骤:**
1. 确认Shell系统已初始化
2. 确认`Shell_RegisterAllModules()`已调用
3. 确认echo模块已注册：`REGISTER_MODULE(echo)`
4. 检查/bin目录：`ls /bin | grep echo`

### 问题2: 参数写入无效
**检查步骤:**
1. 确认参数路径正确：`ls <path_to_param>`
2. 确认参数可写：`cat <path_to_param>`
3. 确认参数范围：查看参数定义
4. 检查VFS节点数：确保未超出`VFS_MAX_NODES`

### 问题3: 命令执行报错
**检查步骤:**
1. 查看错误提示信息
2. 确认命令语法正确
3. 检查参数值格式
4. 查看系统日志

## 总结

echo命令为音频参数调试提供了强大而灵活的工具，支持：
- ✅ 简化和重定向两种语法
- ✅ 绝对和相对路径
- ✅ 负数和小数参数
- ✅ 完善的错误处理
- ✅ 与其他命令的集成
- ✅ 批量参数操作

通过echo命令，可以快速、准确地调整音频效果参数，实现实时音频调试和优化。
