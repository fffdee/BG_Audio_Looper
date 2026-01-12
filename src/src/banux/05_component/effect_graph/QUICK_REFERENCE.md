# 快速参考卡

## 🚀 立即开始

### 1. 查看效果图

```bash
$ ls /audio
graph0/

$ cd /audio/graph0/nodes
$ ls
0_guitar_in/  1_mic_in/  2_usb_in/  3_bt_in/  4_adc_mixer/
5_expander/   6_drc/     7_eq/      8_reverb/ 9_usb_bt_mixer/
10_usb_bt_eq/ 11_final_mixer/ 12_dac_out/ 13_usb_out/
```

### 2. 调整参数（最常用）

```bash
# 进入DRC节点
$ cd 6_drc

# 查看当前阈值
$ cat threshold
-20

# 修改阈值
$ echo threshold -30
OK

# 确认修改
$ cat threshold
-30
```

## 📝 常用命令速查

### 导航命令

```bash
ls              # 列出当前目录
ls /audio       # 列出/audio目录
cd <dir>        # 切换目录
cd ..           # 返回上级
cd /            # 回到根目录
pwd             # 显示当前路径
tree            # 显示目录树
```

### 参数操作

```bash
cat <param>     # 读取参数值
echo <param> <value>    # 写入参数值
echo <value> > <param>  # 重定向写入
```

### 效果图命令

```bash
audio list      # 列出所有效果图
audio mount     # 挂载默认效果图
audio info graph0   # 显示图信息
graph info      # 显示图详情
fx list         # 列出所有节点
```

## 🎛️ 常用参数快速调整

### DRC（动态范围压缩）

```bash
cd /audio/graph0/nodes/6_drc
echo threshold -30    # 阈值 (-60~0 dB)
echo ratio 6          # 压缩比 (1~20)
echo attack 5         # 启动 (1~500 ms)
echo release 150      # 释放 (10~2000 ms)
```

### Reverb（混响）

```bash
cd /audio/graph0/nodes/8_reverb
echo room 70      # 房间大小 (0~100)
echo damp 60      # 阻尼 (0~100)
echo wet 40       # 干湿比 (0~100)
```

### EQ（均衡器）

```bash
cd /audio/graph0/nodes/7_eq
echo band0 3      # 低频 (-12~+12 dB)
echo band1 6      # 中低频
echo band2 9      # 中频
echo band3 6      # 中高频
echo band4 3      # 高频
```

### Expander（扩展器）

```bash
cd /audio/graph0/nodes/5_expander
echo threshold -70    # 阈值 (-80~0 dB)
echo ratio 3          # 比率 (1~10)
```

### 节点开关

```bash
cd /audio/graph0/nodes/<node>
echo enabled 1    # 启用节点
echo enabled 0    # 禁用节点
echo bypass 1     # 旁路节点
echo bypass 0     # 取消旁路
```

## 🎸 效果预设

### 吉他效果

```bash
# 清晰明亮（流行）
cd /audio/graph0/nodes
echo 6_drc/threshold -25
echo 6_drc/ratio 4
echo 8_reverb/room 40
echo 8_reverb/wet 20
echo 7_eq/band0 3
echo 7_eq/band1 6
echo 7_eq/band4 3

# 温暖厚重（布鲁斯）
cd /audio/graph0/nodes
echo 6_drc/threshold -30
echo 6_drc/ratio 6
echo 8_reverb/room 60
echo 8_reverb/wet 35
echo 7_eq/band0 6
echo 7_eq/band1 3
echo 7_eq/band4 -3

# 空灵飘渺（氛围）
cd /audio/graph0/nodes
echo 8_reverb/room 80
echo 8_reverb/damp 40
echo 8_reverb/wet 60
echo 7_eq/band0 -6
echo 7_eq/band4 6
```

### 麦克风效果

```bash
# 人声优化
cd /audio/graph0/nodes
echo 6_drc/threshold -20
echo 6_drc/ratio 3
echo 5_expander/threshold -60
echo 8_reverb/room 30
echo 8_reverb/wet 15

# 广播效果
cd /audio/graph0/nodes
echo 6_drc/threshold -15
echo 6_drc/ratio 4
echo 5_expander/threshold -50
echo 8_reverb/room 20
echo 8_reverb/wet 10
```

## 🔧 故障排除速查

### /audio为空

```bash
$ audio mount
Default graph mounted at /audio/graph0
```

### echo命令不存在

```bash
# 检查命令是否注册
$ ls /bin
# 应该看到 echo

# 如果没有，需要重新编译
```

### 参数修改不生效

```bash
# 检查节点是否启用
$ cd /audio/graph0/nodes/<node>
$ cat enabled
0
$ echo enabled 1    # 启用节点

# 检查是否旁路
$ cat bypass
1
$ echo bypass 0     # 取消旁路
```

## 💡 小技巧

### 1. 快速切换目录

```bash
# 使用绝对路径
$ cd /audio/graph0/nodes/6_drc
$ cat threshold

# 或使用相对路径
$ cd /audio/graph0/nodes
$ cd 6_drc
$ cat threshold
```

### 2. 批量查看参数

```bash
$ cd /audio/graph0/nodes/6_drc
$ ls
enabled  bypass  type  threshold  ratio  attack  release

$ cat threshold && cat ratio && cat attack && cat release
-20
4
10
100
```

### 3. 使用tab补全（如果支持）

```bash
$ cd /audio/gra<TAB>     # 自动补全为 graph0
$ cd nodes/6_dr<TAB>     # 自动补全为 6_drc
```

### 4. 绝对路径快速操作

```bash
# 不用cd，直接读写
$ cat /audio/graph0/nodes/6_drc/threshold
-20

$ echo /audio/graph0/nodes/6_drc/threshold -30
OK
```

## 📊 系统监控

### 查看系统状态

```bash
$ sysmon -c     # CPU使用率
$ sysmon -m     # 内存使用
$ sysmon -t     # 任务列表
$ sysmon -a     # 全部信息
```

### 查看驱动状态

```bash
$ drivers       # 列出所有驱动
$ tree          # 显示完整树
```

## 🎯 一键配置脚本

### 保存当前配置

```bash
# 手动记录当前参数
cd /audio/graph0/nodes
cat 6_drc/threshold
cat 6_drc/ratio
cat 8_reverb/room
# ... 记录所有需要的参数
```

### 恢复配置

```bash
# 按保存的值恢复
cd /audio/graph0/nodes
echo 6_drc/threshold -25
echo 6_drc/ratio 6
echo 8_reverb/room 60
# ... 恢复所有参数
```

## 📱 通过CDC/BLE控制

### 连接方式

1. **USB CDC**：连接USB线，使用串口工具（115200波特率）
2. **BLE SPP**：通过蓝牙连接，使用串口工具

### 实时调整

```bash
# 连接后直接输入命令
> cd /audio/graph0/nodes/8_reverb
> echo room 70
OK
> echo wet 40
OK
# 立即听到效果变化！
```

## 🎉 开始使用

现在你已经掌握了所有必要的命令，开始探索和调整你的音频效果吧！

**记住**：
- ✅ 所有修改立即生效
- ✅ 使用 `cat` 查看，`echo` 修改
- ✅ 不满意随时可以改回来
- ✅ 多尝试，找到最适合的音色！

Have fun! 🎸🎤🎶
