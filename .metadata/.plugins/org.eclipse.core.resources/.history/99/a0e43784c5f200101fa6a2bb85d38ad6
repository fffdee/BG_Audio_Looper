# Echo命令速查表

## 快速参考

### 基本语法
```bash
echo <parameter> <value>        # 简化写法
echo <value> > <parameter>      # 重定向写法
```

## 常用操作

### 1. 调整EQ
```bash
# 单个频段
echo /audio/MainGraph/EQ/band0_gain 6

# 批量调整
cd /audio/MainGraph/EQ
echo band0_gain 3
echo band1_gain 2
echo band2_gain 0
echo band3_gain -2
echo band4_gain -3
```

### 2. 调整压缩器
```bash
echo /audio/MainGraph/Compressor/threshold -18
echo /audio/MainGraph/Compressor/ratio 4
echo /audio/MainGraph/Compressor/attack 10
echo /audio/MainGraph/Compressor/release 100
```

### 3. 调整混响
```bash
echo /audio/MainGraph/Reverb/reverb_time 800
echo /audio/MainGraph/Reverb/wet_dry 60
```

### 4. 使用相对路径
```bash
cd /audio/MainGraph/EQ
echo band0_gain 5    # 相对于当前目录
```

### 5. 使用重定向
```bash
echo 6 > /audio/MainGraph/EQ/band0_gain
echo -20 > /audio/MainGraph/Compressor/threshold
```

## 配合其他命令

### 查看和修改
```bash
cat /audio/MainGraph/EQ/band0_gain    # 查看当前值
echo /audio/MainGraph/EQ/band0_gain 5 # 修改
cat /audio/MainGraph/EQ/band0_gain    # 确认新值
```

### 批量操作
```bash
# EQ完整配置
cd /audio/MainGraph/EQ
for band in band0 band1 band2 band3 band4; do
    echo ${band}_gain 0
done
```

### 配置脚本
```bash
# 保存为preset.sh
cd /audio/MainGraph/EQ
echo band0_gain 3
echo band1_gain 2
echo band2_gain 0
cd ../Compressor
echo threshold -15
echo ratio 4
```

## 错误处理

| 错误 | 原因 | 解决 |
|------|------|------|
| No such file | 路径不存在 | 用`ls`检查路径 |
| Not a parameter | 目标是目录 | 确保是参数文件 |
| Read-only | 参数只读 | 查看参数属性 |
| Missing operand | 缺少参数值 | 提供value参数 |

## 提示

✅ 参数立即生效  
✅ 支持负数和小数  
✅ 支持绝对和相对路径  
✅ 可批量操作  
✅ 与cat命令配合使用  

---
更多详情见: ECHO_COMMAND_GUIDE.md
